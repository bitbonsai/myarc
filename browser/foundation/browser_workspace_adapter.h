#ifndef MYARC_BROWSER_WORKSPACE_ADAPTER_H_
#define MYARC_BROWSER_WORKSPACE_ADAPTER_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "myarc/workspace_service.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace myarc {

// One owner per normal browser window, destroyed before its TabStripModel and
// Profile. UI-sequence only. Browser owns every WebContents; Rust owns all IDs,
// sections and visibility. Parent retains native UI/body when !ready(). Only
// ready() && !DisplayedContents() means an intentional empty workspace surface.
class BrowserWorkspaceAdapter final : public TabStripModelObserver {
 public:
  static constexpr char kItemKey[] = "myarc.item.v1";
  static constexpr char kWindowKey[] = "myarc.window.v1";
  static constexpr char kSpaceKey[] = "myarc.space.v1";

  explicit BrowserWorkspaceAdapter(BrowserWindowInterface& browser);
  ~BrowserWorkspaceAdapter() override;

  // Parent restore hooks, before FinishSessionRestore and before tab insertion
  // where possible. Pass SessionTab/SessionWindow.extra_data unchanged.
  // Upstream does not generically retain arbitrary window/tab extra_data on
  // live objects.
  static foundation::MutationResult RestoreTabBinding(
      content::WebContents& contents,
      const std::map<std::string, std::string>& extra_data);
  foundation::MutationResult RestoreWindowBinding(
      const std::map<std::string, std::string>& extra_data);
  // Call once per exact Profile only after all native windows/tabs have been
  // restored, their hooks applied and adapters constructed. Also call on fresh
  // startup with zero restored windows. Returns loading/error until service is
  // ready, and rejects in-flight async session restore. Retry via Subscribe().
  // After successful reconciliation, allocation failures gate only affected
  // windows: retain native UI when !ready(), show error() and allow retry.
  static foundation::MutationResult FinishSessionRestore(Profile& profile);
  // Parent calls before every later async restore, before applying any hints.
  static void BeginSessionRestore(Profile& profile);
  // Call before application shutdown starts closing tabs; independent of order
  // of upstream IsTryingToQuit and close notifications. No persistence for OTR.
  static void BeginShutdown(Profile& profile);
  // Parent calls after SetTryingToQuit(false), including cancelled
  // quit/restart. Reconciles pages actually destroyed while shutdown was
  // pending.
  static foundation::MutationResult ResumeAfterCancelledShutdown(
      Profile& profile);
  // Merge into BrowserLiveTabContext's historical-tab/window extra_data.
  static std::map<std::string, std::string> TabExtraData(
      content::WebContents& contents);
  // Native menu commands use this ID with explicit Unsave, never SetTabPinned.
  static uint64_t ItemIdForContents(content::WebContents& contents);
  std::map<std::string, std::string> WindowExtraData() const;
  // Parent calls after SessionService::ScheduleResetCommands rebuilds its log,
  // since upstream only preserves its own known extra-data keys during rebuild.
  // Forces emission even when bindings equal the adapter's last-write cache.
  static void PersistSessionBindings(Profile& profile);

  bool ready() const;
  std::string error() const;
  std::string binding_warning() const { return binding_warning_; }
  uint64_t window_id() const { return window_id_; }
  WorkspaceService& service() const { return *service_; }
  foundation::SidebarResult Sidebar() const;
  base::CallbackListSubscription Subscribe(base::RepeatingClosure callback);

  struct LiveMetadata {
    std::u16string title;
    GURL current_url;  // Never written back to saved canonical_url.
    gfx::Image icon;
    bool loading = false;
    size_t projections = 0;  // Exact Profile, including detached live pages.
  };
  std::optional<LiveMetadata> Metadata(uint64_t item) const;
  content::WebContents* DisplayedContents() const;
  bool HasDisplayedContents() const { return DisplayedContents() != nullptr; }
  // Use for an explicit URL commit when no page is displayed. Native navigation
  // keeps upstream policy; hidden active pages are never navigated.
  foundation::MutationResult OpenInCurrentSpace(const GURL& url);
  foundation::MutationResult ActivateItem(uint64_t item);
  // Close this window's projection only. Dormant saved rows remain unchanged.
  foundation::MutationResult CloseItem(uint64_t item);
  foundation::MutationResult CreateSpace(std::string_view name);
  foundation::MutationResult RenameSpace(uint64_t space, std::string_view name);
  foundation::MutationResult SelectSpace(uint64_t space);
  foundation::MutationResult Pin(uint64_t item);
  foundation::MutationResult Favorite(uint64_t item);
  foundation::MutationResult Unsave(uint64_t item);
  foundation::MutationResult MoveItemBefore(uint64_t item, uint64_t before);

 private:
  static foundation::MutationResult ReconcileBindings(Profile& profile);
  foundation::MutationResult BindWindow();
  foundation::MutationResult BindTabs();
  foundation::MutationResult Save(uint64_t item,
                                  foundation::PlacementKind kind);
  static std::map<std::string, std::string> TabBindingExtraData(
      content::WebContents& contents,
      const foundation::ItemRecord* row,
      uint64_t window_space);
  void PersistBindings(bool force = false);
  void SyncNativePins();
  void ShowSelected();
  void OnServiceChanged();
  void Refresh();
  foundation::MutationResult Remember(foundation::MutationResult result);
  void OnTabStripModelChanged(
      TabStripModel* model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab, TabChangeType type) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override;
  void OnTabStripModelDestroyed(TabStripModel* model) override;

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<WorkspaceService> service_;
  raw_ptr<TabStripModel> tabs_;
  uint64_t window_id_ = 0;
  uint64_t restored_window_id_ = 0;
  uint64_t restored_space_id_ = 0;
  bool changing_ = false;
  bool bound_ = false;
  bool refresh_pending_ = false;
  bool refreshing_ = false;
  bool bindings_dirty_ = true;
  bool legacy_pending_ = false;
  bool reveal_active_ = false;
  std::map<std::string, std::string> persisted_window_;
  std::map<int, std::map<std::string, std::string>> persisted_tabs_;
  std::string binding_error_;
  std::string action_error_;
  std::string binding_warning_;
  base::RepeatingClosureList changed_;
  base::CallbackListSubscription service_subscription_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BrowserWorkspaceAdapter> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_BROWSER_WORKSPACE_ADAPTER_H_
