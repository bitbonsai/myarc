#ifndef MYARC_SIDEBAR_VIEW_H_
#define MYARC_SIDEBAR_VIEW_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "myarc/create_space_panel.h"
#include "myarc/space_icon_catalog.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace ui {
class Event;
class EventHandler;
}  // namespace ui

namespace views {
class MdTextButton;
class MenuRunner;
class ScrollView;
}  // namespace views

namespace myarc::foundation {
enum class DropTargetKind : std::uint8_t;
struct FolderRecord;
struct ItemRecord;
struct SidebarResult;
}  // namespace myarc::foundation

namespace myarc {

// The New Tab row's label, which an empty page's row shows too.
inline constexpr char16_t kNewTabLabel[] = u"New Tab";

class CreateButton;
class CreateMenu;
class DownloadsStack;
class FooterTooltip;
class LibraryView;
class SpaceRows;

// Presentation only, on the UI sequence. Delegate outlives this view. The
// parent owns Profile/window scope, Rust mutations, browser tabs, dialogs and
// failures.
class SidebarView final : public views::View,
                          public views::AnimationDelegateViews,
                          public views::ContextMenuController,
                          public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(SidebarView, views::View)

 public:
  enum class StorageRetry { kNone, kLoad, kSave };

  struct ItemPresentation {
    std::u16string title;
    ui::ImageModel icon;  // Parent supplies a favicon, preferably 16 or 20 DIP.
    bool is_live = false;
  };

  // Arc's Library rail, top to bottom.
  enum class LibrarySection { kMedia, kDownloads, kSpaces, kBoosts, kArchive };

  struct DownloadPresentation {
    uint32_t id = 0;
    std::u16string name;
    std::u16string detail;  // Source host, or why it can't be opened.
    bool complete = false;  // Finished and still on disk.
    base::Time time;        // When it finished, or started if it hasn't.
    ui::ImageModel icon;    // The file type's icon once loaded, or empty.
    bool operator==(const DownloadPresentation&) const = default;
  };

  struct ArchivedTabPresentation {
    uint64_t id = 0;  // Rust archive ID.
    std::u16string title;
    std::u16string url;    // Display form, without the scheme.
    std::u16string space;  // Owning Space's name; empty once it is deleted.
    ui::ImageModel icon;   // Cached favicon, or empty.
    base::Time archived;
    bool operator==(const ArchivedTabPresentation&) const = default;
  };

  // A Space's card in the Library, with its Pins in sidebar order.
  struct LibraryPin {
    uint64_t id = 0;  // Rust item ID.
    std::u16string title;
    ui::ImageModel icon;  // Favicon, or empty.
    bool operator==(const LibraryPin&) const = default;
  };
  struct LibrarySpace {
    uint64_t id = 0;
    std::u16string name;
    std::vector<LibraryPin> pins;
    bool operator==(const LibrarySpace&) const = default;
  };

  // A Space as the footer and title show it.
  struct SpaceEntry {
    uint64_t id = 0;
    std::u16string name;
    SpaceIconChoice icon;
    // The Profile's agent Space, shown after the others in the footer.
    bool agent = false;
    bool operator==(const SpaceEntry&) const = default;
  };

  // A Space in another Profile, from the Spaces index, as the footer shows it
  // after a divider.
  struct OtherSpaceEntry {
    std::string profile_dir;  // The Profile's directory name.
    uint64_t space_id = 0;    // That Profile's Rust ID.
    std::u16string name;
    std::u16string profile_name;
    SpaceIconChoice icon;
    size_t order = 0;  // Its place among its Profile's Spaces.
    bool operator==(const OtherSpaceEntry&) const = default;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Lookup only: never open a page or fetch a favicon while rendering.
    virtual ItemPresentation GetItemPresentation(
        const foundation::ItemRecord& item) const = 0;
    // IDs are Rust IDs in this view's parent-owned window, not native tab IDs.
    // Resolve the saved canonical URL in the controller for dormant items.
    virtual void ActivateItem(uint64_t item_id) = 0;
    virtual void CloseLiveItem(uint64_t item_id) = 0;
    // Parent resolves the canonical URL and reports errors. Unsave is explicit
    // removal of saved data, distinct from closing a live page.
    virtual void PinItem(uint64_t item_id) = 0;
    virtual void FavoriteItem(uint64_t item_id) = 0;
    // Promotes if needed, then places `item_id` before `before_id` (0 = end).
    // Returns false when the workspace rejected the change.
    virtual bool DropFavorite(uint64_t item_id, uint64_t before_id) = 0;
    virtual void UnsaveItem(uint64_t item_id) = 0;
    // Folders of the current Space's Pins. The parent names a new folder
    // and lets the user rename it, confirms a delete, and reports errors.
    virtual void CreateFolder() = 0;
    virtual void RenameFolder(uint64_t folder_id) = 0;
    virtual void DeleteFolder(uint64_t folder_id) = 0;
    virtual void SetFolderOpen(uint64_t folder_id, bool open) = 0;
    // Pins `item_id` first if needed. Zero `folder_id` takes it out of its
    // folder.
    virtual void MovePinToFolder(uint64_t item_id, uint64_t folder_id) = 0;
    // A dragged Pin or Today tab landed on `target`: a folder (`folder_id`),
    // the ungrouped Pins or Today, above `before_id` or at the end with 0.
    // Returns false when the workspace rejected it; the parent reports why.
    virtual bool DropItem(uint64_t item_id,
                          foundation::DropTargetKind target,
                          uint64_t folder_id,
                          uint64_t before_id) = 0;
    // Rename… on a Pin: the parent asks for its name, and an empty one goes
    // back to the page title.
    virtual void RenameItem(uint64_t item_id) = 0;
    // Split View. Open in Split View splits the item with the current tab,
    // or, for the current tab itself, opens a new pane beside it as New
    // Split does. Separate All Tabs ends the item's split and keeps both
    // tabs. The parent validates through Rust and reports errors.
    virtual void OpenInSplitView(uint64_t item_id) = 0;
    virtual void NewSplit() = 0;
    virtual void SeparateSplit(uint64_t item_id) = 0;
    virtual void SelectSpace(uint64_t space_id) = 0;
    // A footer glyph of another Profile's Space was pressed.
    virtual void SwitchToSpaceInProfile(const std::string& profile_dir,
                                        uint64_t space_id) = 0;
    // Asks the parent to open the Create a Space panel with OpenCreateSpace.
    virtual void CreateSpace() = 0;
    // The panel's Create: adds and selects the Space with `icon` (Kind::kNone
    // for the default glyph) and `theme` (none for the default tint) in the
    // Profile `profile_id`, or returns why it can't.
    virtual std::u16string CreateNamedSpace(
        const std::u16string& name,
        const SpaceIconChoice& icon,
        const std::optional<SpaceTheme>& theme,
        const std::string& profile_id) = 0;
    // The panel's New Profile…: see CreateSpacePanel::CreateProfileCallback.
    virtual void CreateProfile(const std::u16string& name,
                               CreateSpacePanel::ProfileDoneCallback done) = 0;
    // The panel closed; the parent supplies the rows' model with SetModel.
    virtual void CreateSpacePanelClosed() = 0;
    virtual void RenameSpace(uint64_t space_id) = 0;
    // Places `space_id` before `before_id` (0 = last) in the Space order
    // the footer, Ctrl+1..9 and the Library share. The parent reports errors.
    virtual void MoveSpace(uint64_t space_id, uint64_t before_id) = 0;
    virtual void NewTab() = 0;  // Parent opens its real New Tab/palette flow.
    virtual void OpenDownloads() = 0;  // Exact owning browser's Chrome command.
    // Newest first, bounded by the parent. Lookup only, except that the
    // parent may start loading missing file icons and then calls
    // RefreshDownloadsStack.
    virtual std::vector<DownloadPresentation> GetRecentDownloads() const = 0;
    virtual void OpenDownload(uint32_t download_id) = 0;
    // Starts a native file drag of a complete download, from a mouse drag.
    // Can delete the caller before it returns.
    virtual void DragDownload(uint32_t download_id) = 0;
    // The downloads stack started or stopped showing. While it shows, the
    // parent checks for files removed outside the browser and calls
    // RefreshDownloadsStack when downloads change.
    virtual void SetDownloadsStackShown(bool shown) = 0;
    // Newest first. Lookup only.
    virtual std::vector<ArchivedTabPresentation> GetArchivedTabs() const = 0;
    // Reopens the tab in its Space and drops it from the archive. The parent
    // reports errors.
    virtual void RestoreArchivedTab(uint64_t archive_id) = 0;
    // Clear Archive: the parent asks for confirmation, then drops every
    // archived tab and reports errors.
    virtual void ClearArchive() = 0;
    // The parent resizes the sidebar region for the Library, and calls
    // RefreshLibraryDownloads while it is open. After closing it supplies a
    // fresh model with SetModel.
    virtual void SetLibraryOpen(bool open) = 0;
    virtual void RetryWorkspaceStorage(StorageRetry retry) = 0;
  };

  explicit SidebarView(Delegate& delegate);
  ~SidebarView() override;

  // Copies display values/IDs, retains no Rust borrows. Sections and order come
  // directly from Rust. Call again when model or native title/favicon changes.
  // Optional status explains storage failures without claiming durable success.
  void SetModel(const foundation::SidebarResult& model,
                const std::u16string& status = {},
                StorageRetry retry = StorageRetry::kNone);
  // Other Profiles' Spaces, grouped by Profile in the order to show. Takes
  // effect with the next SetModel.
  void SetOtherSpaces(std::vector<OtherSpaceEntry> spaces);
  // The browser control client using this Profile's agent Space, or empty.
  // While set, the agent Space's footer glyph carries a dot, and its context
  // menu and a Stop button in its title run `stop`. Takes effect with the
  // next SetModel.
  void SetControlClient(std::u16string client, base::RepeatingClosure stop);
  // Drops mutation controls while loading or after a read/snapshot failure.
  // Parent supplies only a retry supported by its current storage state.
  void SetUnavailable(const std::u16string& reason,
                      StorageRetry retry = StorageRetry::kNone);
  // Fills the New Tab row while the parent's New Tab palette is open.
  void SetNewTabIntent(bool intent);
  // The current Space's rows in widget coordinates, cut to the part that
  // shows. Empty while the Library or the Create a Space panel shows.
  gfx::Rect GetSpaceRowsBoundsInWidget() const;
  // Runs the Space switch slide with `outgoing`, a picture of another
  // window's rows that showed at `origin` in widget coordinates, leaving as
  // the current rows come in. A Profile swap uses it. +1 slides them right.
  void SlideInSpace(const gfx::ImageSkia& outgoing,
                    const gfx::Point& origin,
                    int shift);
  // Replaces the sidebar contents with the Library until back or Esc. Needs a
  // usable model.
  void OpenLibrary(LibrarySection section = LibrarySection::kSpaces);
  // Returns to the sidebar rows. Never call from the Library's own handlers.
  void CloseLibrary();
  // Reloads an open Library's Downloads list if it changed.
  void RefreshLibraryDownloads();
  bool library_open() const { return !!library_; }
  // Replaces the rows, or the Library, with Arc's Create a Space panel until
  // Create or Cancel. Needs a usable model. `own_profile` is the window's
  // Profile, selected first. Without `profile_menu` the Profile pill only
  // names the selected Profile.
  void OpenCreateSpace(std::vector<ProfileChoice> profiles,
                       ProfileChoice own_profile,
                       bool profile_menu);
  // Refreshes the open panel's Profiles; see CreateSpacePanel::SetProfiles.
  void SetCreateSpaceProfiles(std::vector<ProfileChoice> profiles,
                              const std::string& selected_id);
  bool create_space_open() const { return !!create_panel_; }
  // Shows the footer tooltip of "library", "space" (the current one) or
  // "create" without hover and keeps it up, for background captures.
  void ShowFooterTooltipForCapture(std::string_view target);
  // Reloads the downloads stack above the Library button while it shows.
  void RefreshDownloadsStack();
  // Shows the downloads stack without hover and keeps it up, for captures.
  void ShowDownloadsStackForCapture();

 private:
  friend class SidebarViewTestApi;

  enum class FocusAction {
    kActivate,
    kClose,
    kRename,
    // A folder row; the id is the folder.
    kFolder,
    kNewTab,
    kClearToday,
    kSpaces,
    // Another Profile's Space; the id indexes footer_other_spaces_.
    kOtherSpace,
    kCreate,
    kLibrary,
    kRetryLoad,
    kRetrySave,
    kStatus
  };
  struct ItemMenuState {
    bool is_pin;
    bool is_favorite;
    bool is_live;
    // The saved or current URL Copy Link writes; empty without one.
    std::u16string url;
    // The folder holding the Pin, or 0.
    uint64_t folder = 0;
    // A live Today tab can open in Split View; a split's tab can leave it.
    bool can_split = false;
    bool in_split = false;
    bool operator==(const ItemMenuState&) const = default;
  };
  struct FocusTarget {
    FocusAction action;
    uint64_t id;
    raw_ptr<views::View> view;
    std::optional<ItemMenuState> item_menu;
  };
  // A part of the rows a dragged row can land on, in row order.
  struct DropZone {
    raw_ptr<views::View> view;
    foundation::DropTargetKind kind;
    uint64_t folder = 0;
    // The row's item, or 0 for the Space title, a folder row or Today's
    // divider and New Tab row, which drop at `next`.
    uint64_t item = 0;
    // The row after it in its section, or 0 at the section's end.
    uint64_t next = 0;
    // A favicon tile, laid out left to right.
    bool tile = false;
    // A split's row, which stands for its left tab and lands in Today only.
    bool split = false;
  };
  // Where a drag would land now, and the line or outline that shows it, in
  // this view's coordinates.
  struct Drop {
    foundation::DropTargetKind kind;
    uint64_t folder = 0;
    uint64_t before = 0;
    gfx::Rect mark;
    // `mark` outlines a folder row rather than drawing a line.
    bool outline = false;
    bool operator==(const Drop&) const = default;
  };
  struct RowDrag {
    uint64_t item = 0;
    bool split = false;
    raw_ptr<views::View> source;
    gfx::Point press;
    bool lifted = false;
    std::optional<Drop> drop;
  };

  views::View* AddStorageRetry(views::View& parent, StorageRetry retry);
  void TrackFocus(views::View* view,
                  FocusAction action,
                  uint64_t id = 0,
                  std::optional<ItemMenuState> item_menu = std::nullopt);
  // views::View:
  void Layout(PassKey) override;
  void AddedToWidget() override;
  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void OnThemeChanged() override;
  // Applies the scheme's tile and New Tab fills to the current rows.
  void UpdateFills();
  // Places the sliding Space rows for the slide's progress, or settles them
  // once it is `done`.
  void UpdateSpaceSlide(bool done);
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;
  // Removes every child and forgets the rows', footer's and panel's views.
  void DropRows();
  // Row drags, fed every mouse event under this view before its target.
  void OnRowDragEvent(ui::Event& event);
  // Where a row dragged to `point` would land, or nothing outside the rows.
  std::optional<Drop> DropAt(const gfx::Point& point) const;
  void ShowDrop(const std::optional<Drop>& drop);
  // Ends the drag and, with `commit`, drops the row where it shows. Can
  // delete this view.
  void EndRowDrag(bool commit);
  // --myarc-capture-drag=<item ID>:<folder ID>[:hold]: drags the row onto
  // the folder row with synthetic mouse events, holding it there with hold.
  // r<item ID> in place of the folder drops it above that row (`row`).
  void RunCaptureDrag(uint64_t item, uint64_t folder, uint64_t row, bool hold);
  // Drops the Library view without notifying the parent.
  void ResetLibrary();
  // The Create a Space panel's callbacks.
  std::u16string SubmitCreateSpace(const std::u16string& name,
                                   const SpaceIconChoice& icon,
                                   const std::optional<SpaceTheme>& theme,
                                   const std::string& profile_id);
  void CloseCreateSpace();
  // Checked when the rows replace the Library, not when closing starts.
  bool LibraryHasFocus();
  // Drops the Library and has the parent rebuild the rows.
  void FinishCloseLibrary(bool had_focus);
  void ShowCreateMenu(const ui::Event& event);
  void SetCreateMenuOpen(bool open);
  // Runs the creation menu's choice once its handlers have unwound.
  void ChooseCreateItem(int item);
  void ExecuteCommand(int command_id, int event_flags) override;
  // Runs the Stop of the active control client after the caller unwinds.
  void StopControl();
  // --myarc-capture-item-menu: opens the first tile's context menu, then
  // reopens it whenever it closed, `tries` more times.
  void OpenCaptureItemMenu(int tries);
  // Builds the footer for spaces_ and current_space_id_, tracking its
  // controls' focus.
  void BuildFooter(const gfx::PointF& spaces_offset, bool space_changed);
  // The Create button's footer tooltip label.
  std::u16string GetCreateTooltipForTesting() const;
  // The row's title and favicon, with the saved title or URL for dormant rows.
  ItemPresentation Present(const foundation::ItemRecord& item) const;
  std::unique_ptr<views::View> MakeItem(const foundation::ItemRecord& item,
                                        bool tile,
                                        bool selected,
                                        uint64_t folder = 0,
                                        bool in_split = false);
  // A split's one row: both panes' favicons and titles side by side, each
  // activating and closing its own tab.
  std::unique_ptr<views::View> MakeSplit(const foundation::ItemRecord& left,
                                         const foundation::ItemRecord& right,
                                         uint64_t selected);
  // A folder's row, which opens or closes it.
  std::unique_ptr<views::View> MakeFolder(const foundation::FolderRecord& folder);
  // Runs the menu built in `menu_model_` at `point`.
  void RunMenu(const gfx::Point& point, ui::mojom::MenuSourceType source_type);
  // The divider above Today, with Clear closing `today`.
  std::unique_ptr<views::View> MakeTodayDivider(std::vector<uint64_t> today);

  const raw_ref<Delegate> delegate_;
  // Every control's target, the footer's included.
  std::vector<FocusTarget> focus_targets_;
  std::vector<DropZone> drop_zones_;
  // Sees every mouse event under this view before its target, for row drags.
  std::unique_ptr<ui::EventHandler> row_drag_handler_;
  std::optional<RowDrag> row_drag_;
  // The drop's line or outline, above the rows while a drag shows one.
  raw_ptr<views::View> drop_indicator_ = nullptr;
  // A model that arrived mid-drag, applied when the drag ends without a drop.
  std::unique_ptr<foundation::SidebarResult> deferred_model_;
  std::u16string deferred_status_;
  StorageRetry deferred_retry_ = StorageRetry::kNone;
  bool capture_drag_scheduled_ = false;
  // The footer kept across row rebuilds, and the Spaces it shows.
  raw_ptr<views::View> footer_ = nullptr;
  // Float above the footer, outside the layout, and go with it.
  raw_ptr<DownloadsStack> downloads_stack_ = nullptr;
  raw_ptr<CreateMenu> create_menu_ = nullptr;
  raw_ptr<CreateButton> create_button_ = nullptr;
  std::vector<FocusTarget> footer_targets_;
  std::vector<SpaceEntry> footer_spaces_;
  std::vector<OtherSpaceEntry> footer_other_spaces_;
  uint64_t footer_space_id_ = 0;
  std::u16string footer_control_client_;
  std::u16string control_client_;
  base::RepeatingClosure stop_control_;
  raw_ptr<views::ScrollView> spaces_scroll_ = nullptr;
  raw_ptr<views::MdTextButton> new_tab_ = nullptr;
  // The current Space's title and rows, which slide on a Space switch.
  raw_ptr<SpaceRows> space_rows_ = nullptr;
  // Favorite and Pin tiles and whether each is selected.
  std::vector<std::pair<raw_ptr<views::MdTextButton>, bool>> tiles_;
  bool new_tab_intent_ = false;
  uint64_t current_space_id_ = 0;
  // The current Space's theme, which tints the window's sidebar unless the
  // Create a Space panel previews another.
  std::optional<SpaceTheme> space_theme_;
  uint64_t menu_space_id_ = 0;
  uint64_t menu_item_id_ = 0;
  // The folder whose row menu is open, or 0.
  uint64_t menu_folder_id_ = 0;
  // The current Space's folders, for the Move to Folder menu.
  struct FolderEntry {
    uint64_t id = 0;
    std::u16string name;
  };
  std::vector<FolderEntry> folders_;
  std::vector<SpaceEntry> spaces_;
  std::vector<OtherSpaceEntry> other_spaces_;
  // The same Spaces with their Pins, for the Library's cards.
  std::vector<LibrarySpace> library_spaces_;
  raw_ptr<LibraryView> library_ = nullptr;
  raw_ptr<CreateSpacePanel> create_panel_ = nullptr;
  ItemMenuState menu_state_ = {};
  // Stable sidebar owns both; no menu callback or anchor retains a child row.
  // Runner must be destroyed before its model and before children.
  // Move to Folder's submenu of `menu_model_`, which outlives it, and the
  // folder each of its entries names.
  std::unique_ptr<ui::SimpleMenuModel> folder_menu_model_;
  std::vector<uint64_t> menu_folder_ids_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  bool create_menu_open_ = false;
  // --myarc-capture-create-menu opens the menu once, for captures.
  bool capture_menu_scheduled_ = false;
  // --myarc-capture-activate=<item ID> opens a seeded row once, for captures.
  bool capture_activate_scheduled_ = false;
  // --myarc-capture-item-menu opens the first tile's context menu once.
  bool capture_item_menu_scheduled_ = false;
  // Shared with the Library, so a press there keeps the tooltip quiet here.
  std::unique_ptr<FooterTooltip> footer_tooltip_;
  // The next footer's Library button starts glowing and settles.
  bool library_closed_ = false;
  // Arc's Library exit: it slides out ahead of the narrowing sidebar, then the
  // rows replace it and fade in.
  gfx::LinearAnimation library_exit_{this};
  // Where a click-opened Library hands focus back when it closes, usually the
  // page.
  views::ViewTracker library_return_focus_;
  // The Library opened from the keyboard, so closing it lands on its button.
  bool library_keyboard_ = false;
  // Arc's Space switch: the old Space's title and rows leave sideways as the
  // new ones enter beside them, with no fade. +1 slides them right.
  gfx::LinearAnimation space_slide_{this};
  int space_slide_shift_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SidebarView> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_SIDEBAR_VIEW_H_
