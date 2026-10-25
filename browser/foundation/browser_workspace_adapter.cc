#include "myarc/browser_workspace_adapter.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "myarc/workspace_lifecycle.h"

namespace myarc {

class WorkspaceLifecycleBridge final : public WorkspaceLifecycle {
 public:
  explicit WorkspaceLifecycleBridge(Profile& profile)
      : WorkspaceLifecycle(&profile), profile_(&profile) {}
  void OnLifecycleEvent(Event event) override;
  void RestoreTab(BrowserWindowInterface& browser,
                  content::WebContents& contents,
                  const ExtraData& data) override;
  void RestoreWindow(BrowserWindowInterface& browser,
                     const ExtraData& data) override;
  ExtraData TabData(BrowserWindowInterface& browser,
                    content::WebContents& contents) override;
  ExtraData WindowData(BrowserWindowInterface& browser) override;
  void Retry();

 private:
  friend class BrowserWorkspaceAdapter;
  BrowserWorkspaceAdapter* Find(BrowserWindowInterface& browser);
  void Resume();
  void NativeReady();
  const raw_ptr<Profile> profile_;
  bool native_ready_ = false;
  bool finishing_ = false;
  base::WeakPtrFactory<WorkspaceLifecycleBridge> weak_factory_{this};
};

namespace {

const char kRuntimeKey = 0;
const char kBindingKey = 0;
const char kHistoricalDataKey = 0;

// Keep foreign keys across restore/UndoClose. Current upstream metadata wins
// when BrowserLiveTabContext merges this historical data into its result.
struct HistoricalData : base::SupportsUserData::Data {
  explicit HistoricalData(const WorkspaceLifecycle::ExtraData& data)
      : extra(WorkspaceLifecycle::PreserveUnknownData(data)) {}
  WorkspaceLifecycle::ExtraData extra;
};

foundation::MutationResult Ok(uint64_t id = 0) {
  return {true, id, {}};
}
foundation::MutationResult Fail(std::string_view error) {
  return {false, 0, rust::String(error.data(), error.size())};
}

// Runtime pointers locate live projections only, never persisted identities.
struct Runtime : base::SupportsUserData::Data {
  std::unique_ptr<WorkspaceLifecycleBridge> lifecycle;
  std::vector<raw_ptr<BrowserWorkspaceAdapter>> adapters;
  std::map<content::WebContents*, uint64_t> pages;
  bool complete = false;
  bool shutting_down = false;
};

Runtime& ForProfile(Profile& profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* runtime = static_cast<Runtime*>(profile.GetUserData(&kRuntimeKey));
  if (!runtime) {
    auto owned = std::make_unique<Runtime>();
    runtime = owned.get();
    profile.SetUserData(&kRuntimeKey, std::move(owned));
  }
  return *runtime;
}

size_t Count(Profile& profile, uint64_t id) {
  return std::ranges::count_if(
      ForProfile(profile).pages,
      [id](const auto& page) { return page.second == id; });
}

// Survives detach/reinsert. Replacements explicitly transfer the binding before
// old contents dies. Actual destruction, not temporary detach, is last-close.
class Binding final : public base::SupportsUserData::Data,
                      public content::WebContentsObserver {
 public:
  Binding(content::WebContents& contents,
          uint64_t item,
          uint64_t space,
          bool legacy)
      : content::WebContentsObserver(&contents),
        profile_(Profile::FromBrowserContext(contents.GetBrowserContext())),
        item_(item),
        space_(space),
        legacy_(legacy) {
    ForProfile(*profile_).pages[&contents] = item;
  }
  static Binding* Get(content::WebContents& contents) {
    return static_cast<Binding*>(contents.GetUserData(&kBindingKey));
  }
  static foundation::MutationResult Set(content::WebContents& contents,
                                        uint64_t item,
                                        uint64_t space = 0,
                                        bool legacy = false) {
    if (auto* old = Get(contents)) {
      if (old->item_ != item)
        return Fail("Conflicting native item binding.");
      old->legacy_ = legacy;
      if (space)
        old->space_ = space;
      return Ok(item);
    }
    contents.SetUserData(
        &kBindingKey, std::make_unique<Binding>(contents, item, space, legacy));
    return Ok(item);
  }
  uint64_t item() const { return item_; }
  uint64_t space() const { return space_; }
  bool legacy() const { return legacy_; }
  void SetSpace(uint64_t space) { space_ = space; }
  void Rebind(uint64_t item, uint64_t space) {
    DCHECK_NE(item, 0u);
    DCHECK_NE(space, 0u);
    item_ = item;
    space_ = space;
    ForProfile(*profile_).pages[web_contents()] = item;
  }
  void Adopted() { legacy_ = false; }
  void WebContentsDestroyed() override {
    auto& runtime = ForProfile(*profile_);
    runtime.pages.erase(web_contents());
    if (item_ && runtime.complete && !runtime.shutting_down &&
        !browser_shutdown::IsTryingToQuit() &&
        !SessionRestore::IsRestoring(profile_) &&
        Count(*profile_, item_) == 0) {
      // Unknown IDs can occur only before rejected restore binding finishes.
      WorkspaceService::ForProfile(*profile_).LiveTabClosed(item_);
    }
  }

 private:
  const raw_ptr<Profile> profile_;
  uint64_t item_;
  uint64_t space_;
  bool legacy_;
};

std::optional<uint64_t> ReadBinding(
    const std::map<std::string, std::string>& extra_data,
    const char* key) {
  auto it = extra_data.find(key);
  if (it == extra_data.end())
    return uint64_t{0};
  uint64_t id = 0;
  if (it->second.size() > 20 || !base::StringToUint64(it->second, &id) ||
      id == 0 || base::NumberToString(id) != it->second)
    return std::nullopt;
  return id;
}

content::WebContents* LocalPage(TabStripModel& tabs, uint64_t item) {
  if (!item)
    return nullptr;
  for (int i = 0; i < tabs.count(); ++i) {
    auto* contents = tabs.GetWebContentsAt(i);
    auto* binding = Binding::Get(*contents);
    if (binding && binding->item() == item)
      return contents;
  }
  return nullptr;
}

const foundation::ItemRecord* FindItem(const foundation::StateSnapshot& state,
                                       uint64_t item) {
  for (const auto& row : state.items) {
    if (row.id == item)
      return &row;
  }
  return nullptr;
}

bool IsReady(WorkspaceService& service) {
  using State = WorkspaceService::StorageState;
  return service.storage_state() != State::kLoading &&
         service.storage_state() != State::kLoadError;
}

}  // namespace

BrowserWorkspaceAdapter* WorkspaceLifecycleBridge::Find(
    BrowserWindowInterface& browser) {
  for (const auto& adapter : ForProfile(*profile_).adapters) {
    if (adapter->browser_ == &browser) {
      return adapter;
    }
  }
  return nullptr;
}

void WorkspaceLifecycleBridge::RestoreTab(BrowserWindowInterface& browser,
                                          content::WebContents& contents,
                                          const ExtraData& data) {
  if (auto* adapter = Find(browser)) {
    auto result = BrowserWorkspaceAdapter::RestoreTabBinding(contents, data);
    if (!result.ok) {
      adapter->binding_warning_ = std::string(result.error);
      adapter->OnServiceChanged();
    }
  }
}

void WorkspaceLifecycleBridge::RestoreWindow(BrowserWindowInterface& browser,
                                             const ExtraData& data) {
  if (auto* adapter = Find(browser)) {
    auto result = adapter->RestoreWindowBinding(data);
    if (!result.ok) {
      adapter->binding_warning_ = std::string(result.error);
      adapter->OnServiceChanged();
    }
  }
}

WorkspaceLifecycle::ExtraData WorkspaceLifecycleBridge::TabData(
    BrowserWindowInterface& browser,
    content::WebContents& contents) {
  if (!Find(browser)) {
    return {};
  }
  ExtraData data;
  if (auto* historical = static_cast<HistoricalData*>(
          contents.GetUserData(&kHistoricalDataKey))) {
    data = historical->extra;
  }
  data.merge(BrowserWorkspaceAdapter::TabExtraData(contents));
  return data;
}

WorkspaceLifecycle::ExtraData WorkspaceLifecycleBridge::WindowData(
    BrowserWindowInterface& browser) {
  auto* adapter = Find(browser);
  if (!adapter) {
    return {};
  }
  auto data = adapter->restored_extra_data_;
  data.merge(adapter->WindowExtraData());
  return data;
}

void WorkspaceLifecycleBridge::Retry() {
  auto& runtime = ForProfile(*profile_);
  if (!native_ready_ || finishing_ || runtime.complete ||
      runtime.shutting_down || browser_shutdown::IsTryingToQuit() ||
      HasPendingSessionRestore(profile_) ||
      SessionRestore::IsRestoring(profile_) ||
      !IsReady(WorkspaceService::ForProfile(*profile_))) {
    return;
  }
  base::AutoReset<bool> finishing(&finishing_, true);
  auto result = BrowserWorkspaceAdapter::FinishSessionRestore(*profile_);
  // Global reconciliation errors keep every window on native fallback. Later
  // per-window allocation errors are already reported by FinishSessionRestore.
  if (!result.ok && !runtime.complete) {
    for (const auto& adapter : runtime.adapters) {
      const std::string error(result.error);
      if (adapter->binding_error_ != error) {
        adapter->binding_error_ = error;
        adapter->OnServiceChanged();
      }
    }
  }
}

void WorkspaceLifecycleBridge::NativeReady() {
  if (HasPendingSessionRestore(profile_) ||
      SessionRestore::IsRestoring(profile_)) {
    return;
  }
  native_ready_ = true;
  Retry();
}

void WorkspaceLifecycleBridge::Resume() {
  if (browser_shutdown::IsTryingToQuit()) {
    return;
  }
  // Clears the shutdown latch even if metadata/another restorer is not ready.
  // Retry still requires an actual native-completion notification.
  BrowserWorkspaceAdapter::ResumeAfterCancelledShutdown(*profile_);
}

void WorkspaceLifecycleBridge::OnLifecycleEvent(Event event) {
  switch (event) {
    case Event::kStartupStarted:
    case Event::kRestoreStarted:
    case Event::kSessionRestoreStarted:
      native_ready_ = false;
      BrowserWorkspaceAdapter::BeginSessionRestore(*profile_);
      break;
    case Event::kStartupFinished:
    case Event::kNativeReady:
    case Event::kSessionRestoreFinished:
      // Reconcile after native constructors/destructors and nested UI loops.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
          FROM_HERE, base::BindOnce(&WorkspaceLifecycleBridge::NativeReady,
                                    weak_factory_.GetWeakPtr()));
      break;
    case Event::kSessionReset:
      BrowserWorkspaceAdapter::PersistSessionBindings(*profile_);
      break;
    case Event::kShutdownStarted:
      BrowserWorkspaceAdapter::BeginShutdown(*profile_);
      break;
    case Event::kShutdownCancelled:
      // Run after caller unload/confirmation cleanup, even in nested UI loops.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
          FROM_HERE, base::BindOnce(&WorkspaceLifecycleBridge::Resume,
                                    weak_factory_.GetWeakPtr()));
      break;
  }
}

BrowserWorkspaceAdapter::BrowserWorkspaceAdapter(
    BrowserWindowInterface& browser)
    : browser_(&browser),
      profile_(browser.GetProfile()),
      service_(&WorkspaceService::ForProfile(*profile_)),
      tabs_(browser.GetTabStripModel()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(browser.GetType(), BrowserWindowInterface::TYPE_NORMAL);
  auto& runtime = ForProfile(*profile_);
  runtime.adapters.push_back(this);
  if (!runtime.lifecycle) {
    runtime.lifecycle = std::make_unique<WorkspaceLifecycleBridge>(*profile_);
  }
  if (browser_shutdown::IsTryingToQuit()) {
    BeginShutdown(*profile_);
  }
  tabs_->AddObserver(this);
  service_subscription_ = service_->Subscribe(base::BindRepeating(
      [](BrowserWorkspaceAdapter* adapter) {
        adapter->bindings_dirty_ = true;
        adapter->OnServiceChanged();
      },
      base::Unretained(this)));
  OnServiceChanged();
}

BrowserWorkspaceAdapter::~BrowserWorkspaceAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tabs_)
    tabs_->RemoveObserver(this);
  std::erase(ForProfile(*profile_).adapters, this);
  // Preserve windows/temporary organization for session restore. Ordinary page
  // close is handled by Binding; startup reconciliation removes stale rows.
}

foundation::MutationResult BrowserWorkspaceAdapter::RestoreTabBinding(
    content::WebContents& contents,
    const std::map<std::string, std::string>& extra_data) {
  contents.SetUserData(&kHistoricalDataKey,
                       std::make_unique<HistoricalData>(extra_data));
  const auto id = ReadBinding(extra_data, kItemKey);
  const auto space = ReadBinding(extra_data, kSpaceKey);
  // Even malformed/stale metadata marks this as a restore, not legacy pin
  // migration. BindTabs allocates a fresh Temporary after the model loads.
  return Binding::Set(
      contents, id.value_or(0), space.value_or(0),
      !extra_data.contains(kItemKey) && !extra_data.contains(kSpaceKey));
}

foundation::MutationResult BrowserWorkspaceAdapter::RestoreWindowBinding(
    const std::map<std::string, std::string>& extra_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bound_)
    return Fail("Window binding already finalized.");
  restored_extra_data_ = HistoricalData(extra_data).extra;
  const auto id = ReadBinding(extra_data, kWindowKey);
  restored_window_id_ = id.value_or(0);
  restored_space_id_ = ReadBinding(extra_data, kSpaceKey).value_or(0);
  if (!id)
    binding_warning_ = "Invalid native window identity. Rebinding this window.";
  return Ok();
}

foundation::MutationResult BrowserWorkspaceAdapter::BindWindow() {
  if (window_id_)
    return Ok(window_id_);
  auto state = service_->ExportSnapshot();
  if (!state)
    return Fail("Workspace metadata is still loading or failed to load.");
  // A rolled-back metadata file can generate an ID still present in native
  // restore data. Consume those IDs through Rust, never alias another window.
  auto allocate = [this](uint64_t space) {
    const auto& adapters = ForProfile(*profile_).adapters;
    for (size_t attempt = 0; attempt <= adapters.size(); ++attempt) {
      auto result = service_->CreateWindow(space);
      if (!result.ok ||
          std::ranges::none_of(adapters, [&result](const auto& a) {
            return a->restored_window_id_ == result.id;
          }))
        return result;
      auto closed = service_->CloseWindow(result.id);
      if (!closed.ok)
        return closed;
    }
    return Fail(
        "Native window identities could not be rebound. Retry binding.");
  };
  if (restored_window_id_) {
    const bool exists = std::ranges::any_of(
        state->windows,
        [this](const auto& w) { return w.id == restored_window_id_; });
    const bool claimed = std::ranges::any_of(
        ForProfile(*profile_).adapters, [this](const auto& adapter) {
          return adapter != this && adapter->window_id_ == restored_window_id_;
        });
    if (exists && !claimed) {
      window_id_ = restored_window_id_;
      return Ok(window_id_);
    }
    binding_warning_ =
        "Missing or duplicate native window identity. Rebinding this window.";
    if (exists) {
      for (const auto& window : state->windows) {
        if (window.id == restored_window_id_) {
          auto result = allocate(window.space_id);
          if (result.ok)
            window_id_ = result.id;
          return result;
        }
      }
    }
  }
  if (restored_space_id_ &&
      std::ranges::any_of(state->spaces, [this](const auto& space) {
        return space.id == restored_space_id_;
      })) {
    auto result = allocate(restored_space_id_);
    if (result.ok)
      window_id_ = result.id;
    return result;
  }
  // Rebind an unclaimed model window, reserving IDs named by restore hooks
  // even if those adapters have not been processed yet. Never assume Window 1.
  for (const auto& window : state->windows) {
    if (std::ranges::none_of(ForProfile(*profile_).adapters,
                             [&window](const auto& adapter) {
                               return adapter->window_id_ == window.id ||
                                      adapter->restored_window_id_ == window.id;
                             })) {
      window_id_ = window.id;
      return Ok(window_id_);
    }
  }
  // Zero model windows is valid; allocation uses the model's existing Space.
  if (state->spaces.empty())
    return Fail("Workspace has no Space. Recover metadata and retry loading.");
  auto result = allocate(state->spaces[0].id);
  if (result.ok)
    window_id_ = result.id;
  return result;
}

foundation::MutationResult BrowserWorkspaceAdapter::ReconcileBindings(
    Profile& profile) {
  auto& runtime = ForProfile(profile);
  auto& service = WorkspaceService::ForProfile(profile);
  // Phase one records hints only. Allocating here can deadlock at MAX_ITEMS
  // while stale Temporary rows occupy the slots reconciliation would release.
  for (const auto& adapter : runtime.adapters) {
    if (!adapter->tabs_)
      continue;
    for (int i = 0; i < adapter->tabs_->count(); ++i) {
      auto* page = adapter->tabs_->GetWebContentsAt(i);
      if (page->GetBrowserContext() != &profile)
        return Fail("Native page belongs to a different Profile.");
      if (!Binding::Get(*page)) {
        auto result = Binding::Set(*page, 0, 0, true);
        if (!result.ok)
          return result;
      }
    }
  }
  auto state = service.ExportSnapshot();
  if (!state)
    return Fail("Workspace metadata is still loading or failed to load.");
  std::set<uint64_t> existing;
  for (const auto& row : state->items)
    existing.insert(row.id);
  // Include detached pages and shared saved projections, once per valid ID.
  std::set<uint64_t> live;
  for (const auto& [page, item] : runtime.pages) {
    if (item && existing.contains(item))
      live.insert(item);
  }
  const bool stale =
      std::ranges::any_of(state->items, [&live](const auto& row) {
        return row.placement == foundation::PlacementKind::Temporary &&
               !live.contains(row.id);
      });
  if (!stale)
    return Ok();
  return service.ReconcileLiveItems(
      std::vector<uint64_t>(live.begin(), live.end()));
}

foundation::MutationResult BrowserWorkspaceAdapter::BindTabs() {
  if (tabs_->count() > 4096)
    return Fail(
        "This window exceeds 4096 workspace projections. Use native "
        "tabs to close or move pages, then retry binding.");
  auto sidebar = service_->Sidebar(window_id_);
  auto state = service_->ExportSnapshot();
  if (!sidebar.ok || !state)
    return Fail("Workspace window is not ready.");
  legacy_pending_ = false;
  binding_warning_.clear();
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* contents = tabs_->GetWebContentsAt(i);
    if (contents->GetBrowserContext() != profile_)
      return Fail("Native page belongs to a different Profile.");
    auto* binding = Binding::Get(*contents);
    if (!binding) {
      auto result = Binding::Set(*contents, 0, 0, true);
      if (!result.ok)
        return result;
      binding = Binding::Get(*contents);
    }
    if (!FindItem(*state, binding->item())) {
      uint64_t space = sidebar.current_space.id;
      if (std::ranges::any_of(state->spaces, [binding](const auto& row) {
            return row.id == binding->space();
          }))
        space = binding->space();
      // Native and workspace files can disagree after an interrupted write.
      // Skip generated IDs still claimed by restored pages, including this
      // stale binding, so unrelated histories cannot collapse into one item.
      auto result = service_->AddTemporaryItem(space);
      const size_t reservations = ForProfile(*profile_).pages.size();
      for (size_t attempt = 0;
           result.ok && Count(*profile_, result.id) && attempt < reservations;
           ++attempt) {
        auto closed = service_->LiveTabClosed(result.id);
        if (!closed.ok)
          return closed;
        result = service_->AddTemporaryItem(space);
      }
      if (!result.ok)
        return Fail("Could not bind this window's native pages: " +
                    std::string(result.error) +
                    " Close unused native tabs or unsave unused items, then "
                    "retry. Native pages remain intact.");
      if (Count(*profile_, result.id)) {
        auto closed = service_->LiveTabClosed(result.id);
        if (!closed.ok)
          return closed;
        return Fail("Native item reservations exhausted. Retry binding.");
      }
      DCHECK_NE(result.id, 0u);
      DCHECK_EQ(Count(*profile_, result.id), 0u);
      binding->Rebind(result.id, space);
      state = service_->ExportSnapshot();
      if (!state)
        return Fail("Workspace metadata became unavailable during binding.");
    }
    if (binding->legacy() && tabs_->IsTabPinned(i)) {
      // Failed/lazy legacy saves retain their native pin and retry on page
      // changes. Stale saved IDs never enter this migration path.
      auto result = service_->SetPlacement(
          binding->item(), foundation::PlacementKind::Favorite,
          sidebar.current_space.profile_id, true,
          contents->GetLastCommittedURL().spec());
      if (!result.ok) {
        DCHECK(binding->legacy());
        DCHECK(tabs_->IsTabPinned(i));
        legacy_pending_ = true;
        binding_warning_ =
            "Native pin retained. Open a supported page to finish migration: " +
            std::string(result.error);
        continue;
      }
      Remember(Ok(binding->item()));
    }
    binding->Adopted();
  }
  return Ok();
}

void BrowserWorkspaceAdapter::BeginSessionRestore(Profile& profile) {
  ForProfile(profile).complete = false;
  for (const auto& adapter : ForProfile(profile).adapters) {
    adapter->reveal_active_ = false;
    adapter->OnServiceChanged();
  }
}

foundation::MutationResult BrowserWorkspaceAdapter::FinishSessionRestore(
    Profile& profile) {
  auto& runtime = ForProfile(profile);
  auto& service = WorkspaceService::ForProfile(profile);
  if (runtime.shutting_down || browser_shutdown::IsTryingToQuit() ||
      WorkspaceLifecycle::HasPendingSessionRestore(&profile) ||
      SessionRestore::IsRestoring(&profile)) {
    return Fail(
        "Native session restore is in flight or browser is shutting down.");
  }
  if (!IsReady(service))
    return Fail("Workspace metadata is still loading or failed to load.");
  // Only the parent may release the native-restore/hints barrier. After
  // reconciliation, allocation failures are per-window native fallback, not
  // a Profile-wide failure and never grounds for discarding native pages.
  auto reconciled = ReconcileBindings(profile);
  if (!reconciled.ok)
    return reconciled;
  const bool released = !runtime.complete;
  runtime.complete = true;
  auto failure = Ok();
  for (const auto& adapter : runtime.adapters) {
    if (!adapter->tabs_)
      continue;
    base::AutoReset<bool> changing(&adapter->changing_, true);
    auto result = adapter->BindWindow();
    if (result.ok)
      result = adapter->BindTabs();
    const std::string error(result.error);
    const bool changed = released || adapter->binding_error_ != error ||
                         adapter->bound_ != result.ok;
    adapter->binding_error_ = error;
    adapter->bound_ = result.ok;
    adapter->bindings_dirty_ = false;
    if (changed)
      adapter->OnServiceChanged();
    if (!result.ok && failure.ok)
      failure = std::move(result);
  }
  // Preserve completed bindings even if retiring an obsolete model window fails.
  for (const auto& adapter : runtime.adapters) {
    if (adapter->bound_)
      adapter->PersistBindings();
  }
  auto result = Ok();
  // Retire windows not rebound by this complete native restoration. In
  // particular, the default Window 1 is not assumed to have a native peer.
  auto state = service.ExportSnapshot();
  if (!state)
    return Fail("Workspace metadata became unavailable during binding.");
  for (const auto& window : state->windows) {
    if (std::ranges::none_of(runtime.adapters, [&window](const auto& adapter) {
          return adapter->tabs_ && adapter->window_id_ == window.id;
        })) {
      result = service.CloseWindow(window.id);
      if (!result.ok)
        return result;
    }
  }
  return failure;
}

void BrowserWorkspaceAdapter::BeginShutdown(Profile& profile) {
  ForProfile(profile).shutting_down = true;
  WorkspaceService::ForProfile(profile).FlushForShutdown();
  for (const auto& adapter : ForProfile(profile).adapters)
    adapter->OnServiceChanged();
}

foundation::MutationResult
BrowserWorkspaceAdapter::ResumeAfterCancelledShutdown(Profile& profile) {
  if (browser_shutdown::IsTryingToQuit())
    return Fail(
        "Quit is still pending. Resume only after SetTryingToQuit(false).");
  auto& runtime = ForProfile(profile);
  if (!runtime.shutting_down)
    return Ok();
  runtime.shutting_down = false;
  // Pages may have closed while quit was pending. Gate commands until the
  // surviving projections reconcile, including after a metadata-load retry.
  runtime.complete = false;
  for (const auto& adapter : runtime.adapters) {
    adapter->OnServiceChanged();
  }
  if (runtime.lifecycle && !runtime.lifecycle->native_ready_) {
    return Fail("Native session insertion has not completed.");
  }
  return FinishSessionRestore(profile);
}

void BrowserWorkspaceAdapter::PersistSessionBindings(Profile& profile) {
  auto& runtime = ForProfile(profile);
  // Invalidate now; the completed binding pass must re-emit after a reset
  // that arrived before its restore barrier opened.
  for (const auto& adapter : runtime.adapters) {
    adapter->persisted_window_.clear();
    adapter->persisted_tabs_.clear();
  }
  if (!runtime.complete || runtime.shutting_down ||
      browser_shutdown::IsTryingToQuit() ||
      WorkspaceLifecycle::HasPendingSessionRestore(&profile) ||
      SessionRestore::IsRestoring(&profile)) {
    return;
  }
  for (const auto& adapter : runtime.adapters) {
    // A partial allocation failure still has valid existing native bindings.
    if (adapter->window_id_)
      adapter->PersistBindings(true);
  }
}

bool BrowserWorkspaceAdapter::ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& runtime = ForProfile(*profile_);
  return tabs_ && !tabs_->closing_all() && bound_ && runtime.complete &&
         !runtime.shutting_down && !browser_shutdown::IsTryingToQuit() &&
         !browser_->IsDeleteScheduled() && binding_error_.empty() &&
         IsReady(*service_) && !SessionRestore::IsRestoring(profile_);
}

std::string BrowserWorkspaceAdapter::error() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!binding_error_.empty())
    return binding_error_;
  if (!IsReady(*service_))
    return service_->storage_error().empty() ? "Loading workspace metadata."
                                             : service_->storage_error();
  if (ForProfile(*profile_).shutting_down ||
      browser_shutdown::IsTryingToQuit() || browser_->IsDeleteScheduled() ||
      !tabs_ || tabs_->closing_all())
    return "Browser window is closing.";
  if (SessionRestore::IsRestoring(profile_))
    return "Native session restoration is still in flight.";
  return bound_ && ForProfile(*profile_).complete
             ? action_error_
             : "Waiting for native session binding.";
}

foundation::SidebarResult BrowserWorkspaceAdapter::Sidebar() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ready())
    return service_->Sidebar(window_id_);
  foundation::SidebarResult result;
  result.ok = false;
  result.error = error();
  return result;
}

base::CallbackListSubscription BrowserWorkspaceAdapter::Subscribe(
    base::RepeatingClosure callback) {
  return changed_.Add(std::move(callback));
}

std::optional<BrowserWorkspaceAdapter::LiveMetadata>
BrowserWorkspaceAdapter::Metadata(uint64_t item) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!item)
    return std::nullopt;
  const auto& pages = ForProfile(*profile_).pages;
  content::WebContents* page = tabs_ ? LocalPage(*tabs_, item) : nullptr;
  if (!page) {
    for (const auto& [contents, id] : pages) {
      if (id == item) {
        page = contents;
        break;
      }
    }
  }
  if (!page)
    return std::nullopt;
  LiveMetadata metadata;
  metadata.title = page->GetTitle();
  metadata.current_url = page->GetLastCommittedURL();
  metadata.loading = page->IsLoading();
  metadata.projections = Count(*profile_, item);
  if (auto* entry = page->GetController().GetVisibleEntry())
    metadata.icon = entry->GetFavicon().image;
  return metadata;
}

content::WebContents* BrowserWorkspaceAdapter::DisplayedContents() const {
  if (!ready())
    return nullptr;
  auto sidebar = Sidebar();
  return sidebar.ok && sidebar.selected_item_id
             ? LocalPage(*tabs_, sidebar.selected_item_id)
             : nullptr;
}

std::map<std::string, std::string> BrowserWorkspaceAdapter::TabExtraData(
    content::WebContents& contents) {
  auto* profile = Profile::FromBrowserContext(contents.GetBrowserContext());
  auto state = WorkspaceService::ForProfile(*profile).ExportSnapshot();
  if (!state) {
    // UndoClose may capture a restored tab before metadata loads. Keep only
    // already bounded hints; reconciliation will validate them when ready.
    const auto* binding = Binding::Get(contents);
    if (!binding || !binding->item() || binding->legacy()) {
      return {};
    }
    std::map<std::string, std::string> data = {
        {kItemKey, base::NumberToString(binding->item())}};
    if (binding->space()) {
      data[kSpaceKey] = base::NumberToString(binding->space());
    }
    return data;
  }
  uint64_t space = 0;
  for (const auto& adapter : ForProfile(*profile).adapters) {
    if (adapter->tabs_ &&
        adapter->tabs_->GetIndexOfWebContents(&contents) >= 0) {
      auto sidebar = adapter->service_->Sidebar(adapter->window_id_);
      if (sidebar.ok)
        space = sidebar.current_space.id;
      break;
    }
  }
  const auto* binding = Binding::Get(contents);
  return TabBindingExtraData(
      contents, binding ? FindItem(*state, binding->item()) : nullptr, space);
}

uint64_t BrowserWorkspaceAdapter::ItemIdForContents(
    content::WebContents& contents) {
  auto* binding = Binding::Get(contents);
  return binding ? binding->item() : 0;
}

std::map<std::string, std::string> BrowserWorkspaceAdapter::TabBindingExtraData(
    content::WebContents& contents,
    const foundation::ItemRecord* row,
    uint64_t window_space) {
  auto* binding = Binding::Get(contents);
  // Unresolved legacy pins need migration again after restart. Writing their
  // provisional Temporary identity would classify them as deliberately unsaved.
  if (!binding || !binding->item() || binding->legacy())
    return {};
  if (!row)
    return {};
  DCHECK_EQ(row->id, binding->item());
  DCHECK_NE(binding->item(), 0u);
  if (row->placement != foundation::PlacementKind::Favorite)
    binding->SetSpace(row->owner_id);
  else if (window_space)
    binding->SetSpace(window_space);
  std::map<std::string, std::string> data = {
      {kItemKey, base::NumberToString(binding->item())}};
  if (binding->space())
    data[kSpaceKey] = base::NumberToString(binding->space());
  return data;
}

std::map<std::string, std::string> BrowserWorkspaceAdapter::WindowExtraData()
    const {
  const uint64_t id = window_id_ ? window_id_ : restored_window_id_;
  if (!id)
    return {};
  std::map<std::string, std::string> data = {
      {kWindowKey, base::NumberToString(id)}};
  auto sidebar = service_->Sidebar(window_id_);
  const uint64_t space =
      sidebar.ok ? sidebar.current_space.id : restored_space_id_;
  if (space)
    data[kSpaceKey] = base::NumberToString(space);
  return data;
}

void BrowserWorkspaceAdapter::PersistBindings(bool force) {
  if (profile_->IsOffTheRecord() || !tabs_ || !window_id_)
    return;
  auto* session = SessionServiceFactory::GetForProfile(profile_);
  if (!session)
    return;
  auto state = service_->ExportSnapshot();
  if (!state)
    return;
  auto window = WindowExtraData();
  window.insert(restored_extra_data_.begin(), restored_extra_data_.end());
  for (const auto& [key, value] : window) {
    if (force || !persisted_window_.contains(key) ||
        persisted_window_.at(key) != value)
      session->AddWindowExtraData(browser_->GetSessionID(), key.c_str(), value);
  }
  persisted_window_ = window;
  uint64_t space = 0;
  for (const auto& row : state->windows) {
    if (row.id == window_id_) {
      space = row.space_id;
      break;
    }
  }
  std::map<uint64_t, const foundation::ItemRecord*> items;
  for (const auto& row : state->items)
    items.emplace(row.id, &row);
  std::set<int> live;
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* contents = tabs_->GetWebContentsAt(i);
    const auto session_id = sessions::SessionTabHelper::IdForTab(contents);
    live.insert(session_id.id());
    const uint64_t item = ItemIdForContents(*contents);
    const auto row = items.find(item);
    auto data = TabBindingExtraData(
        *contents, row == items.end() ? nullptr : row->second, space);
    if (auto* historical = static_cast<HistoricalData*>(
            contents->GetUserData(&kHistoricalDataKey))) {
      data.insert(historical->extra.begin(), historical->extra.end());
    }
    auto& previous = persisted_tabs_[session_id.id()];
    for (const auto& [key, value] : data) {
      if (force || !previous.contains(key) || previous.at(key) != value)
        session->AddTabExtraData(browser_->GetSessionID(), session_id,
                                 key.c_str(), value);
    }
    previous = std::move(data);
  }
  std::erase_if(persisted_tabs_, [&live](const auto& entry) {
    return !live.contains(entry.first);
  });
}

void BrowserWorkspaceAdapter::SyncNativePins() {
  if (!ready() || changing_)
    return;
  base::AutoReset<bool> changing(&changing_, true);
  auto state = service_->ExportSnapshot();
  if (!state)
    return;
  // Pinning reorders the native strip. Keep contents pointers, not indices.
  std::vector<content::WebContents*> pages;
  for (int i = 0; i < tabs_->count(); ++i)
    pages.push_back(tabs_->GetWebContentsAt(i));
  for (auto* page : pages) {
    const auto* binding = Binding::Get(*page);
    const auto* row = binding ? FindItem(*state, binding->item()) : nullptr;
    if (!row || binding->legacy())
      continue;
    const bool favorite = row->placement == foundation::PlacementKind::Favorite;
    const int index = tabs_->GetIndexOfWebContents(page);
    if (tabs_->IsTabPinned(index) != favorite)
      tabs_->SetTabPinned(index, favorite);
  }
}

void BrowserWorkspaceAdapter::ShowSelected() {
  if (!ready() || changing_)
    return;
  base::AutoReset<bool> changing(&changing_, true);
  auto sidebar = Sidebar();
  if (!sidebar.ok)
    return;
  content::WebContents* page = LocalPage(*tabs_, sidebar.selected_item_id);
  // Selection is UI policy over Rust's supplied rows, never native tab order
  // or independently computed placement/Profile visibility.
  if (!page) {
    for (const auto* section :
         {&sidebar.pins, &sidebar.temporary, &sidebar.favorites}) {
      for (const auto& row : *section) {
        page = LocalPage(*tabs_, row.id);
        if (page) {
          auto result = service_->SelectItem(window_id_, row.id);
          Remember(result);
          if (!result.ok)
            return;
          break;
        }
      }
      if (page)
        break;
    }
  }
  if (page) {
    const int index = tabs_->GetIndexOfWebContents(page);
    if (index != tabs_->active_index())
      tabs_->ActivateTabAt(index);
  }
}

foundation::MutationResult BrowserWorkspaceAdapter::Remember(
    foundation::MutationResult result) {
  const std::string error =
      result.ok ? std::string() : std::string(result.error);
  if (error == action_error_)
    return result;
  action_error_ = error;
  // Refresh already publishes status. Reposting from ShowSelected would spin
  // indefinitely on a persistent selection error.
  if (!refreshing_)
    OnServiceChanged();
  return result;
}

void BrowserWorkspaceAdapter::OnServiceChanged() {
  if (refresh_pending_)
    return;
  refresh_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserWorkspaceAdapter::Refresh,
                                weak_factory_.GetWeakPtr()));
}

void BrowserWorkspaceAdapter::Refresh() {
  refresh_pending_ = false;
  // Navigate can run a nested UI loop before returning its exact contents.
  if (changing_)
    return;
  // Metadata readiness retries a pending native completion; it never releases
  // the restore barrier by itself.
  ForProfile(*profile_).lifecycle->Retry();
  base::AutoReset<bool> refreshing(&refreshing_, true);
  auto& runtime = ForProfile(*profile_);
  const bool dirty = bindings_dirty_;
  bindings_dirty_ = false;
  if ((dirty || !bound_ || legacy_pending_) && runtime.complete && tabs_ &&
      !runtime.shutting_down && !browser_shutdown::IsTryingToQuit() &&
      !browser_->IsDeleteScheduled() && !tabs_->closing_all() &&
      IsReady(*service_) && !SessionRestore::IsRestoring(profile_)) {
    base::AutoReset<bool> changing(&changing_, true);
    // Late inserts and recovery use the same known-ID-first reconciliation.
    auto result = ReconcileBindings(*profile_);
    if (result.ok)
      result = BindWindow();
    if (result.ok)
      result = BindTabs();
    binding_error_ = std::string(result.error);
    bound_ = result.ok;
    if (bound_)
      PersistBindings();
  }
  if (ready() && reveal_active_) {
    reveal_active_ = false;
    if (auto* page = tabs_->GetActiveWebContents()) {
      const uint64_t item = ItemIdForContents(*page);
      if (item)
        Remember(service_->RevealItem(window_id_, item));
    }
  }
  SyncNativePins();
  ShowSelected();
  changed_.Notify();
}

foundation::MutationResult BrowserWorkspaceAdapter::ActivateItem(
    uint64_t item) {
  if (!ready())
    return Fail(error());
  base::AutoReset<bool> changing(&changing_, true);
  base::ScopedClosureRunner refresh_on_exit(base::BindOnce(
      &BrowserWorkspaceAdapter::OnServiceChanged, base::Unretained(this)));
  auto state = service_->ExportSnapshot();
  const auto* row = state ? FindItem(*state, item) : nullptr;
  if (!row)
    return Fail("Workspace item does not exist.");
  auto* page = LocalPage(*tabs_, item);
  if (!page) {
    if (!row->has_url)
      return Fail("Temporary page is detached or unavailable in this window.");
    const GURL url(std::string(row->canonical_url));
    if (!url.is_valid() || url.has_username() || url.has_password() ||
        !(url.SchemeIsHTTPOrHTTPS() || url.spec() == "about:blank"))
      return Fail("Saved URL is invalid.");
    // Native navigation owns security policy and page lifetime. Background
    // insertion avoids revealing a new item before Rust validates selection.
    NavigateParams params(browser_, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = WindowOpenDisposition::kNewBackgroundTab;
    params.tabstrip_add_types = ADD_NONE;
    Navigate(&params);
    page = params.navigated_or_inserted_contents;
    if (!page || params.browser != browser_ ||
        page->GetBrowserContext() != profile_ ||
        tabs_->GetIndexOfWebContents(page) < 0)
      return Fail(
          "Native navigation did not open in the owning window/Profile.");
    auto result = Binding::Set(*page, item);
    if (!result.ok)
      return result;
    DCHECK_EQ(ItemIdForContents(*page), item);
  }
  auto result = service_->RevealItem(window_id_, item);
  if (!result.ok)
    return result;
  tabs_->ActivateTabAt(tabs_->GetIndexOfWebContents(page));
  page->Focus();
  PersistBindings();
  OnServiceChanged();
  return Remember(Ok(item));
}

foundation::MutationResult BrowserWorkspaceAdapter::OpenInCurrentSpace(
    const GURL& url) {
  if (!ready())
    return Fail(error());
  if (!url.is_valid())
    return Fail("Enter a valid URL.");
  auto sidebar = Sidebar();
  if (!sidebar.ok)
    return Fail("Workspace window is not ready.");
  base::AutoReset<bool> changing(&changing_, true);
  base::ScopedClosureRunner refresh_on_exit(base::BindOnce(
      &BrowserWorkspaceAdapter::OnServiceChanged, base::Unretained(this)));
  auto result = service_->AddTemporaryItem(sidebar.current_space.id);
  if (!result.ok)
    return result;
  const uint64_t item = result.id;
  NavigateParams params(browser_, url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::kNewBackgroundTab;
  params.tabstrip_add_types = ADD_NONE;
  Navigate(&params);
  auto* page = params.navigated_or_inserted_contents.get();
  if (!page || params.browser != browser_ ||
      page->GetBrowserContext() != profile_ ||
      tabs_->GetIndexOfWebContents(page) < 0) {
    if (!Count(*profile_, item))
      Remember(service_->LiveTabClosed(item));
    return Fail("Native navigation did not open in the owning window/Profile.");
  }
  result = Binding::Set(*page, item, sidebar.current_space.id);
  if (result.ok) {
    DCHECK_EQ(ItemIdForContents(*page), item);
    result = service_->SelectItem(window_id_, item);
  }
  if (!result.ok)
    return result;
  tabs_->ActivateTabAt(tabs_->GetIndexOfWebContents(page));
  page->Focus();
  PersistBindings();
  OnServiceChanged();
  return Remember(Ok(item));
}

foundation::MutationResult BrowserWorkspaceAdapter::CloseItem(uint64_t item) {
  if (!ready())
    return Fail(error());
  auto* page = LocalPage(*tabs_, item);
  if (!page)
    return Fail("No live projection of this item in this window.");
  tabs_->CloseWebContents(page,
                          CLOSE_USER_GESTURE | CLOSE_CREATE_HISTORICAL_TAB);
  // Native beforeunload may cancel. Binding destruction acknowledges actual
  // close.
  return Remember(Ok(item));
}

foundation::MutationResult BrowserWorkspaceAdapter::CreateSpace(
    std::string_view name) {
  return Remember(ready() ? service_->CreateSpace(name) : Fail(error()));
}
foundation::MutationResult BrowserWorkspaceAdapter::RenameSpace(
    uint64_t space,
    std::string_view name) {
  return Remember(ready() ? service_->RenameSpace(space, name) : Fail(error()));
}
foundation::MutationResult BrowserWorkspaceAdapter::SelectSpace(
    uint64_t space) {
  if (!ready())
    return Fail(error());
  auto result = service_->SelectSpace(window_id_, space);
  if (result.ok)
    ShowSelected();
  return Remember(std::move(result));
}
foundation::MutationResult BrowserWorkspaceAdapter::Save(
    uint64_t item,
    foundation::PlacementKind kind) {
  if (!ready())
    return Fail(error());
  auto state = service_->ExportSnapshot();
  const auto* row = state ? FindItem(*state, item) : nullptr;
  if (!row)
    return Fail("Workspace item does not exist.");
  auto sidebar = Sidebar();
  if (!sidebar.ok)
    return Fail("Workspace window is not ready.");
  const uint64_t owner = kind == foundation::PlacementKind::Favorite
                             ? sidebar.current_space.profile_id
                             : sidebar.current_space.id;
  if (row->has_url)
    return Remember(service_->SetPlacement(item, kind, owner, false, ""));
  auto* page = LocalPage(*tabs_, item);
  if (!page)
    return Fail("Save requires an actual live page in this window.");
  return Remember(service_->SetPlacement(item, kind, owner, true,
                                         page->GetLastCommittedURL().spec()));
}
foundation::MutationResult BrowserWorkspaceAdapter::Pin(uint64_t item) {
  return Save(item, foundation::PlacementKind::Pin);
}
foundation::MutationResult BrowserWorkspaceAdapter::Favorite(uint64_t item) {
  return Save(item, foundation::PlacementKind::Favorite);
}
foundation::MutationResult BrowserWorkspaceAdapter::Unsave(uint64_t item) {
  if (!ready())
    return Fail(error());
  if (!Count(*profile_, item))
    return Remember(service_->UnsaveItem(item, 0));
  // Prefer this window's projection. A detached page has no owning window
  // Space yet, so wait for insertion rather than invent a destination.
  BrowserWorkspaceAdapter* owner = nullptr;
  if (LocalPage(*tabs_, item)) {
    owner = this;
  } else {
    for (const auto& adapter : ForProfile(*profile_).adapters) {
      if (adapter->tabs_ && LocalPage(*adapter->tabs_, item)) {
        owner = adapter;
        break;
      }
    }
  }
  if (!owner)
    return Fail(
        "Page is detached. Finish moving it to a window and retry unsave.");
  auto sidebar = owner->service_->Sidebar(owner->window_id_);
  if (!sidebar.ok)
    return Fail(
        "Owning window is not bound. Retry after native session binding.");
  auto state = service_->ExportSnapshot();
  const auto* row = state ? FindItem(*state, item) : nullptr;
  if (!row)
    return Fail("Workspace item does not exist. Retry native session binding.");
  // A hidden Space Pin still belongs to Rust's reported Space. Only a
  // Profile-wide Favorite takes its destination from the projection's window.
  const uint64_t space = row->placement == foundation::PlacementKind::Favorite
                             ? sidebar.current_space.id
                             : row->owner_id;
  return Remember(service_->UnsaveItem(item, space));
}
foundation::MutationResult BrowserWorkspaceAdapter::MoveItemBefore(
    uint64_t item,
    uint64_t before) {
  return Remember(ready() ? service_->MoveItemBefore(item, before)
                          : Fail(error()));
}

void BrowserWorkspaceAdapter::OnTabStripModelChanged(
    TabStripModel*,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (change.type() == TabStripModelChange::kReplaced) {
    const auto* replace = change.GetReplace();
    if (auto* binding = Binding::Get(*replace->old_contents)) {
      Remember(Binding::Set(*replace->new_contents, binding->item(),
                            binding->space(), binding->legacy()));
      if (bound_)
        PersistBindings();
    }
  }
  // Navigate binds only navigated_or_inserted_contents after it returns.
  // Never assign its pending identity by scanning unrelated unbound tabs.
  bindings_dirty_ = true;
  if (change.type() == TabStripModelChange::kInserted && !changing_)
    bound_ = false;
  if (!changing_ && selection.active_tab_changed() && selection.new_contents &&
      (change.type() == TabStripModelChange::kInserted ||
       change.type() == TabStripModelChange::kSelectionOnly)) {
    if (ready()) {
      if (auto* binding = Binding::Get(*selection.new_contents);
          binding && binding->item())
        Remember(service_->RevealItem(window_id_, binding->item()));
    } else if (ForProfile(*profile_).complete &&
               !SessionRestore::IsRestoring(profile_)) {
      reveal_active_ = true;
    }
  }
  for (const auto& adapter : ForProfile(*profile_).adapters)
    adapter->OnServiceChanged();
}

void BrowserWorkspaceAdapter::OnTabPinnedStateChanged(tabs::TabInterface*,
                                                      int index) {
  if (changing_ || !ready())
    return;
  auto* binding = Binding::Get(*tabs_->GetWebContentsAt(index));
  if (!binding)
    return;
  if (!binding->item())
    return;
  auto state = service_->ExportSnapshot();
  const auto* row = state ? FindItem(*state, binding->item()) : nullptr;
  if (!row)
    return;
  if (tabs_->IsTabPinned(index)) {
    // Native true is explicit Favorite adoption. Unsupported new pin requests
    // report failure and mirror Rust; existing unresolved legacy pins are kept.
    Remember(Favorite(binding->item()));
  } else if (row->placement == foundation::PlacementKind::Favorite) {
    // Extensions and drag operations call SetTabPinned too. Saved data may
    // only be removed through the explicit Unsave command.
    Remember(
        Fail("Favorite retained. Use explicit sidebar Unsave to remove "
             "saved data."));
  } else {
    binding->Adopted();
    Remember(Ok(binding->item()));
  }
  bindings_dirty_ = true;
  OnServiceChanged();
}

void BrowserWorkspaceAdapter::OnTabChangedAt(tabs::TabInterface*,
                                             TabChangeType) {
  for (const auto& adapter : ForProfile(*profile_).adapters)
    adapter->OnServiceChanged();
}

void BrowserWorkspaceAdapter::OnTabStripModelDestroyed(TabStripModel*) {
  tabs_ = nullptr;
  bound_ = false;
  OnServiceChanged();
}

}  // namespace myarc
