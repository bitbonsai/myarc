#include "myarc/browser_workspace_adapter.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
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

namespace myarc {
namespace {

const char kRuntimeKey = 0;
const char kBindingKey = 0;

foundation::MutationResult Ok(uint64_t id = 0) {
  return {true, id, {}};
}
foundation::MutationResult Fail(std::string_view error) {
  return {false, 0, rust::String(error.data(), error.size())};
}

// Runtime pointers locate live projections only, never persisted identities.
struct Runtime : base::SupportsUserData::Data {
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
  Binding(content::WebContents& contents, uint64_t item)
      : content::WebContentsObserver(&contents),
        profile_(Profile::FromBrowserContext(contents.GetBrowserContext())),
        item_(item) {
    ForProfile(*profile_).pages[&contents] = item;
  }
  static Binding* Get(content::WebContents& contents) {
    return static_cast<Binding*>(contents.GetUserData(&kBindingKey));
  }
  static foundation::MutationResult Set(content::WebContents& contents,
                                        uint64_t item) {
    if (auto* old = Get(contents)) {
      if (old->item_ != item)
        return Fail("Conflicting native item binding.");
      return Ok(item);
    }
    auto& runtime =
        ForProfile(*Profile::FromBrowserContext(contents.GetBrowserContext()));
    if (runtime.pages.size() >= 4096)
      return Fail("Too many live workspace projections.");
    contents.SetUserData(&kBindingKey,
                         std::make_unique<Binding>(contents, item));
    return Ok(item);
  }
  uint64_t item() const { return item_; }
  void PreserveOnDestruction() { preserve_ = true; }
  void WebContentsDestroyed() override {
    auto& runtime = ForProfile(*profile_);
    runtime.pages.erase(web_contents());
    if (!preserve_ && runtime.complete && !runtime.shutting_down &&
        !browser_shutdown::IsTryingToQuit() &&
        !SessionRestore::IsRestoring(profile_) &&
        Count(*profile_, item_) == 0) {
      // Unknown IDs can occur only before rejected restore binding finishes.
      WorkspaceService::ForProfile(*profile_).LiveTabClosed(item_);
    }
  }

 private:
  const raw_ptr<Profile> profile_;
  const uint64_t item_;
  bool preserve_ = false;
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

BrowserWorkspaceAdapter::BrowserWorkspaceAdapter(
    BrowserWindowInterface& browser)
    : browser_(&browser),
      profile_(browser.GetProfile()),
      service_(&WorkspaceService::ForProfile(*profile_)),
      tabs_(browser.GetTabStripModel()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(browser.GetType(), BrowserWindowInterface::TYPE_NORMAL);
  ForProfile(*profile_).adapters.push_back(this);
  tabs_->AddObserver(this);
  service_subscription_ = service_->Subscribe(base::BindRepeating(
      &BrowserWorkspaceAdapter::OnServiceChanged, base::Unretained(this)));
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
  auto id = ReadBinding(extra_data, kItemKey);
  if (!id)
    return Fail("Invalid restored workspace item ID.");
  return *id ? Binding::Set(contents, *id) : Ok();
}

foundation::MutationResult BrowserWorkspaceAdapter::RestoreWindowBinding(
    const std::map<std::string, std::string>& extra_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bound_)
    return Fail("Window binding already finalized.");
  auto id = ReadBinding(extra_data, kWindowKey);
  if (!id)
    return Fail("Invalid restored workspace window ID.");
  restored_window_id_ = *id;
  return Ok();
}

foundation::MutationResult BrowserWorkspaceAdapter::BindWindow() {
  if (window_id_)
    return Ok(window_id_);
  auto state = service_->ExportSnapshot();
  if (!state)
    return Fail("Workspace metadata is still loading or failed to load.");
  if (restored_window_id_) {
    if (std::ranges::none_of(state->windows, [this](const auto& window) {
          return window.id == restored_window_id_;
        }))
      return Fail(
          "Restored native window references missing workspace window.");
    for (const auto& adapter : ForProfile(*profile_).adapters) {
      if (adapter != this && adapter->window_id_ == restored_window_id_)
        return Fail("Duplicate restored workspace window binding.");
    }
    window_id_ = restored_window_id_;
    return Ok(window_id_);
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
  auto result = service_->CreateWindow(state->spaces[0].id);
  if (result.ok)
    window_id_ = result.id;
  return result;
}

foundation::MutationResult BrowserWorkspaceAdapter::BindTabs() {
  auto sidebar = service_->Sidebar(window_id_);
  auto state = service_->ExportSnapshot();
  if (!sidebar.ok || !state)
    return Fail("Workspace window is not ready.");
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* contents = tabs_->GetWebContentsAt(i);
    if (contents->GetBrowserContext() != profile_)
      return Fail("Native page belongs to a different Profile.");
    if (auto* binding = Binding::Get(*contents)) {
      if (!FindItem(*state, binding->item()))
        return Fail("Restored native page references missing workspace item.");
    } else {
      auto result = opening_item_
                        ? Ok(opening_item_)
                        : service_->AddTemporaryItem(sidebar.current_space.id);
      if (!result.ok)
        return result;
      result = Binding::Set(*contents, result.id);
      if (!result.ok)
        return result;
    }
  }
  return Ok();
}

foundation::MutationResult BrowserWorkspaceAdapter::FinishSessionRestore(
    Profile& profile) {
  auto& runtime = ForProfile(profile);
  auto& service = WorkspaceService::ForProfile(profile);
  if (runtime.shutting_down || SessionRestore::IsRestoring(&profile))
    return Fail(
        "Native session restore is in flight or browser is shutting down.");
  if (!IsReady(service))
    return Fail("Workspace metadata is still loading or failed to load.");
  runtime.complete = false;
  for (const auto& adapter : runtime.adapters) {
    if (!adapter->tabs_)
      continue;
    base::AutoReset<bool> changing(&adapter->changing_, true);
    auto result = adapter->BindWindow();
    if (result.ok)
      result = adapter->BindTabs();
    adapter->error_ = std::string(result.error);
    if (!result.ok)
      return result;
  }
  std::set<uint64_t> unique;
  for (const auto& [page, item] : runtime.pages)
    unique.insert(item);
  std::vector<uint64_t> live(unique.begin(), unique.end());
  auto result = service.ReconcileLiveItems(live);
  if (!result.ok)
    return result;
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
  runtime.complete = true;
  for (const auto& adapter : runtime.adapters) {
    adapter->bound_ = true;
    adapter->PersistBindings();
    adapter->OnServiceChanged();
  }
  return Ok();
}

void BrowserWorkspaceAdapter::BeginShutdown(Profile& profile) {
  ForProfile(profile).shutting_down = true;
  WorkspaceService::ForProfile(profile).FlushForShutdown();
}

void BrowserWorkspaceAdapter::PersistSessionBindings(Profile& profile) {
  for (const auto& adapter : ForProfile(profile).adapters) {
    if (adapter->bound_)
      adapter->PersistBindings();
  }
}

bool BrowserWorkspaceAdapter::ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& runtime = ForProfile(*profile_);
  return tabs_ && !tabs_->closing_all() && bound_ && runtime.complete &&
         !runtime.shutting_down && !browser_->IsDeleteScheduled() &&
         error_.empty() && IsReady(*service_) &&
         !SessionRestore::IsRestoring(profile_);
}

std::string BrowserWorkspaceAdapter::error() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error_.empty())
    return error_;
  if (!IsReady(*service_))
    return service_->storage_error().empty() ? "Loading workspace metadata."
                                             : service_->storage_error();
  if (ForProfile(*profile_).shutting_down || browser_->IsDeleteScheduled() ||
      !tabs_ || tabs_->closing_all())
    return "Browser window is closing.";
  if (SessionRestore::IsRestoring(profile_))
    return "Native session restoration is still in flight.";
  return bound_ && ForProfile(*profile_).complete
             ? std::string()
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

void BrowserWorkspaceAdapter::PersistBindings() {
  if (profile_->IsOffTheRecord() || !tabs_ || !window_id_)
    return;
  auto* session = SessionServiceFactory::GetForProfile(profile_);
  if (!session)
    return;
  session->AddWindowExtraData(browser_->GetSessionID(), kWindowKey,
                              base::NumberToString(window_id_));
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* contents = tabs_->GetWebContentsAt(i);
    if (auto* binding = Binding::Get(*contents)) {
      session->AddTabExtraData(browser_->GetSessionID(),
                               sessions::SessionTabHelper::IdForTab(contents),
                               kItemKey, base::NumberToString(binding->item()));
    }
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
         {&sidebar.favorites, &sidebar.pins, &sidebar.temporary}) {
      for (const auto& row : *section) {
        page = LocalPage(*tabs_, row.id);
        if (page) {
          Remember(service_->SelectItem(window_id_, row.id));
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

void BrowserWorkspaceAdapter::Remember(foundation::MutationResult result) {
  if (!result.ok)
    error_ = std::string(result.error);
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
  auto& runtime = ForProfile(*profile_);
  if (!bound_ && runtime.complete && tabs_ && IsReady(*service_) &&
      !SessionRestore::IsRestoring(profile_)) {
    base::AutoReset<bool> changing(&changing_, true);
    auto result = BindWindow();
    if (result.ok)
      result = BindTabs();
    error_ = std::string(result.error);
    bound_ = result.ok;
    if (bound_)
      PersistBindings();
  }
  ShowSelected();
  changed_.Notify();
}

foundation::MutationResult BrowserWorkspaceAdapter::ActivateItem(
    uint64_t item) {
  if (!ready())
    return Fail(error());
  base::AutoReset<bool> changing(&changing_, true);
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
    base::AutoReset<uint64_t> opening(&opening_item_, item);
    NavigateParams params(browser_, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = WindowOpenDisposition::kNewBackgroundTab;
    params.tabstrip_add_types = ADD_NONE;
    Navigate(&params);
    page = params.navigated_or_inserted_contents;
    if (!page || params.browser != browser_ ||
        page->GetBrowserContext() != profile_)
      return Fail(
          "Native navigation did not open in the owning window/Profile.");
    auto result = Binding::Set(*page, item);
    if (!result.ok)
      return result;
  }
  auto result = service_->RevealItem(window_id_, item);
  if (!result.ok)
    return result;
  tabs_->ActivateTabAt(tabs_->GetIndexOfWebContents(page));
  page->Focus();
  PersistBindings();
  OnServiceChanged();
  return Ok(item);
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
  return Ok(item);
}

foundation::MutationResult BrowserWorkspaceAdapter::CreateSpace(
    std::string_view name) {
  return ready() ? service_->CreateSpace(name) : Fail(error());
}
foundation::MutationResult BrowserWorkspaceAdapter::RenameSpace(
    uint64_t space,
    std::string_view name) {
  return ready() ? service_->RenameSpace(space, name) : Fail(error());
}
foundation::MutationResult BrowserWorkspaceAdapter::SelectSpace(
    uint64_t space) {
  if (!ready())
    return Fail(error());
  auto result = service_->SelectSpace(window_id_, space);
  if (result.ok)
    ShowSelected();
  return result;
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
    return service_->SetPlacement(item, kind, owner, false, "");
  auto* page = LocalPage(*tabs_, item);
  if (!page)
    return Fail("Save requires an actual live page in this window.");
  return service_->SetPlacement(item, kind, owner, true,
                                page->GetLastCommittedURL().spec());
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
  auto sidebar = Sidebar();
  // Include detached and other-window live projections. Zero is factual
  // absence.
  return service_->UnsaveItem(
      item, Count(*profile_, item) ? sidebar.current_space.id : 0);
}
foundation::MutationResult BrowserWorkspaceAdapter::MoveItemBefore(
    uint64_t item,
    uint64_t before) {
  return ready() ? service_->MoveItemBefore(item, before) : Fail(error());
}

void BrowserWorkspaceAdapter::OnTabStripModelChanged(
    TabStripModel* model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (const auto* remove = change.GetRemove()) {
    if (model->closing_all() || browser_shutdown::IsTryingToQuit() ||
        ForProfile(*profile_).shutting_down) {
      for (const auto& removed : remove->contents) {
        if (auto* binding = Binding::Get(*removed.contents))
          binding->PreserveOnDestruction();
      }
    }
  }
  if (const auto* replace = change.GetReplace()) {
    if (auto* binding = Binding::Get(*replace->old_contents)) {
      Remember(Binding::Set(*replace->new_contents, binding->item()));
      binding->PreserveOnDestruction();
      if (bound_)
        PersistBindings();
    }
  }
  if (bound_ && ForProfile(*profile_).complete && IsReady(*service_) &&
      !SessionRestore::IsRestoring(profile_) && change.GetInsert()) {
    base::AutoReset<bool> changing(&changing_, true);
    Remember(BindTabs());
    PersistBindings();
  }
  if (ready() && !changing_ && selection.active_tab_changed() &&
      selection.new_contents &&
      (change.GetInsert() ||
       change.type() == TabStripModelChange::kSelectionOnly)) {
    if (auto* binding = Binding::Get(*selection.new_contents))
      Remember(service_->RevealItem(window_id_, binding->item()));
  }
  for (const auto& adapter : ForProfile(*profile_).adapters)
    adapter->OnServiceChanged();
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
