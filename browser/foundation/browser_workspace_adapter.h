#ifndef MYARC_BROWSER_WORKSPACE_ADAPTER_H_
#define MYARC_BROWSER_WORKSPACE_ADAPTER_H_

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "myarc/workspace_service.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace favicon {
class FaviconService;
}
namespace favicon_base {
struct FaviconImageResult;
}

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
  // The window's Profile window group, and "1" while the window is ordered
  // out in it. BrowserView owns both.
  static constexpr char kGroupKey[] = "myarc.group.v1";
  static constexpr char kGroupHiddenKey[] = "myarc.group_hidden.v1";

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

  // The Profile window group restored with this window, or 0, and whether
  // the window was ordered out in it.
  uint64_t restored_group() const { return restored_group_; }
  bool restored_group_hidden() const { return restored_group_hidden_; }
  // Saves the window's Profile window group with its session binding.
  void SetWindowGroup(uint64_t group, bool hidden);

  bool ready() const;
  std::string error() const;
  std::string binding_warning() const { return binding_warning_; }
  uint64_t window_id() const { return window_id_; }
  WorkspaceService& service() const { return *service_; }
  foundation::SidebarResult Sidebar() const;
  base::CallbackListSubscription Subscribe(base::RepeatingClosure callback);
  // Browser control's active client changed; subscribers redraw.
  void NotifyControlChanged() { changed_.Notify(); }

  struct LiveMetadata {
    std::u16string title;
    GURL current_url;  // Never written back to saved canonical_url.
    gfx::Image icon;
    bool loading = false;
    size_t projections = 0;  // Exact Profile, including detached live pages.
  };
  std::optional<LiveMetadata> Metadata(uint64_t item) const;
  // Favicon the Profile's history cached for a saved row's URL, for rows with
  // no live page. Empty until the lookup Refresh starts has answered, or when
  // nothing is cached. Never fetches from the network.
  gfx::Image SavedIcon(const foundation::ItemRecord& item) const;
  // The same cache by canonical URL spec, which also covers Archived Tabs.
  gfx::Image CachedIcon(std::string_view url) const;
  // Starts favicon lookups for Archived Tabs not looked up yet. Call only
  // while the Library shows them; Refresh then reports found icons.
  void RequestArchivedIcons(
      const rust::Vec<foundation::ArchivedTabRecord>& tabs);
  // Reopens an Archived Tab as a temporary item in its Space (or the current
  // one if that Space is gone), selects it, then drops the archive entry.
  foundation::MutationResult RestoreArchivedTab(uint64_t id);
  content::WebContents* DisplayedContents() const;
  bool HasDisplayedContents() const { return DisplayedContents() != nullptr; }
  // Use for an explicit URL commit when no page is displayed. Native navigation
  // keeps upstream policy; hidden active pages are never navigated.
  foundation::MutationResult OpenInCurrentSpace(const GURL& url);
  // Opens `url` as a temporary item of `space`, switching this window to that
  // Space once the page is open. Sets `opened` (if given) once the page is
  // open and bound, even when selecting it fails.
  foundation::MutationResult OpenInSpace(const GURL& url,
                                         uint64_t space,
                                         bool* opened);
  // Like OpenInSpace, but a window showing another Space stays there: the
  // page opens behind and becomes the item the window shows when it next
  // selects `space`.
  foundation::MutationResult OpenWithoutSwitching(const GURL& url,
                                                  uint64_t space);
  // The agent Space whose tab `page` is, or 0. Navigate opens the tabs and
  // popups such a page opens as background tabs there.
  uint64_t AgentSpaceOf(content::WebContents& page) const;
  // Binds `contents`, a tab an agent Space page opened, to that Space before
  // it is inserted, so binding gives it an item there rather than in the
  // Space the window shows.
  void AdoptAgentTab(content::WebContents& contents, uint64_t space);
  foundation::MutationResult ActivateItem(uint64_t item);
  // ActivateItem when this window shows the item's Space; otherwise makes
  // it the item the window shows when it next selects that Space.
  foundation::MutationResult ActivateInItsSpace(uint64_t item);
  // The live page of `item` in this window's Profile, or null. Sets `owner`
  // to the window whose tab strip holds it, or null while it is detached.
  content::WebContents* FindPage(uint64_t item,
                                 BrowserWorkspaceAdapter** owner) const;
  // Close this window's projection only. Dormant saved rows remain unchanged.
  foundation::MutationResult CloseItem(uint64_t item);
  foundation::MutationResult CreateSpace(std::string_view name);
  foundation::MutationResult CreateSpace(
      std::string_view name,
      foundation::SpaceIconKind icon_kind,
      std::string_view icon,
      const foundation::SpaceThemeRecord& theme);
  // The agent Space's ID, created when missing. The window stays in its
  // Space.
  foundation::MutationResult EnsureAgentSpace();
  foundation::MutationResult RenameSpace(uint64_t space, std::string_view name);
  foundation::MutationResult SetSpaceIcon(uint64_t space,
                                          foundation::SpaceIconKind icon_kind,
                                          std::string_view icon);
  foundation::MutationResult SetSpaceTheme(
      uint64_t space,
      const foundation::SpaceThemeRecord& theme);
  foundation::MutationResult SelectSpace(uint64_t space);
  // Runs a browser control request against this window's workspace, and
  // shows the newly selected Space when the request changed it. Tab commands
  // run in this window and may answer later.
  void AnswerControlRequest(rust::Box<foundation::ControlReply> reply);
  foundation::MutationResult Pin(uint64_t item);
  foundation::MutationResult Favorite(uint64_t item);
  foundation::MutationResult Unsave(uint64_t item);
  foundation::MutationResult MoveItemBefore(uint64_t item, uint64_t before);
  foundation::MutationResult MoveSpaceBefore(uint64_t space, uint64_t before);
  // Folders of this window's current Space. `id` is the new folder.
  foundation::MutationResult CreateFolder(std::string_view name);
  foundation::MutationResult RenameFolder(uint64_t folder,
                                          std::string_view name);
  foundation::MutationResult SetFolderOpen(uint64_t folder, bool open);
  // Pins a temporary item here first. Zero folder takes a Pin out of its
  // folder.
  foundation::MutationResult MovePinToFolder(uint64_t item, uint64_t folder);
  // Deletes the folder and archives its dormant Pins. A Pin with a live page
  // keeps it open as a temporary tab of its Space instead, so no page loses
  // its row. `id` is how many Pins were archived.
  foundation::MutationResult DeleteFolder(uint64_t folder);

  // Keyboard commands over Rust's sidebar order: Favorites, Pins, then
  // temporary items, and Spaces in footer order. Index -1 means the last row;
  // an index past the end does nothing. Steps wrap around.
  static BrowserWorkspaceAdapter* ForBrowser(BrowserWindowInterface& browser);
  // Pins the selected temporary item or unpins the selected Space Pin.
  foundation::MutationResult TogglePin();
  foundation::MutationResult SelectSpaceAt(int index);
  foundation::MutationResult StepSpace(int delta);
  foundation::MutationResult ActivateSidebarItemAt(int index);
  foundation::MutationResult StepSidebarItem(int delta);

 private:
  friend class WorkspaceLifecycleBridge;
  static foundation::MutationResult ReconcileBindings(Profile& profile);
  // Auto Archive (BC-006): closes temporary pages left unviewed for 12 hours
  // through the ordinary close path, which archives them. Pages the archive
  // could not keep stay open.
  static void AutoArchive(Profile& profile);
  foundation::MutationResult BindWindow();
  foundation::MutationResult BindTabs();
  foundation::MutationResult Save(uint64_t item,
                                  foundation::PlacementKind kind);
  foundation::MutationResult Open(const GURL& url,
                                  uint64_t space,
                                  bool* opened,
                                  bool switch_space);
  static std::map<std::string, std::string> TabBindingExtraData(
      content::WebContents& contents,
      const foundation::ItemRecord* row,
      uint64_t window_space);
  void PersistBindings(bool force = false);
  void SyncNativePins();
  void ShowSelected();
  void OnServiceChanged();
  void Refresh();
  // Null for off-the-record Profiles.
  favicon::FaviconService* Favicons() const;
  // Looks `url` up once; later calls answer from the cache. With `fetch`, a
  // miss asks SiteIconFetcher for the site's own icon.
  void RequestIcon(favicon::FaviconService& favicons,
                   std::string_view url,
                   bool fetch);
  // Starts favicon lookups for dormant saved rows not looked up yet.
  void RequestSavedIcons(const foundation::SidebarResult& sidebar);
  void OnSavedIcon(const std::string& url,
                   bool fetch,
                   const favicon_base::FaviconImageResult& result);
  // Looks a fetched page up again if its row still shows no icon.
  void OnSiteIconStored(const std::string& url);
  foundation::MutationResult Remember(foundation::MutationResult result);
  foundation::MutationResult StepOrSelectItem(int index, int delta);
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
  uint64_t restored_group_ = 0;
  bool restored_group_hidden_ = false;
  uint64_t group_ = 0;
  bool group_hidden_ = false;
  bool changing_ = false;
  bool bound_ = false;
  bool refresh_pending_ = false;
  bool refreshing_ = false;
  bool bindings_dirty_ = true;
  bool legacy_pending_ = false;
  bool reveal_active_ = false;
  std::map<std::string, std::string> restored_extra_data_;
  std::map<std::string, std::string> persisted_window_;
  std::map<int, std::map<std::string, std::string>> persisted_tabs_;
  std::string binding_error_;
  std::string action_error_;
  std::string binding_warning_;
  base::RepeatingClosureList changed_;
  base::CallbackListSubscription service_subscription_;
  base::CallbackListSubscription site_icon_subscription_;
  // By saved or archived canonical URL spec. An empty image marks a pending
  // lookup or a cache miss.
  std::map<std::string, gfx::Image, std::less<>> saved_icons_;
  // Lookups not answered yet, and whether this batch found any icon.
  size_t pending_icons_ = 0;
  bool icons_found_ = false;
  base::CancelableTaskTracker icon_tracker_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BrowserWorkspaceAdapter> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_BROWSER_WORKSPACE_ADAPTER_H_
