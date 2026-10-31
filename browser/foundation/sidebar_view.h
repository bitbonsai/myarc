#ifndef MYARC_SIDEBAR_VIEW_H_
#define MYARC_SIDEBAR_VIEW_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/view.h"

namespace views {
class MdTextButton;
class MenuRunner;
class ScrollView;
}  // namespace views

namespace myarc::foundation {
struct ItemRecord;
struct SidebarResult;
}  // namespace myarc::foundation

namespace myarc {

class LibraryView;

// Presentation only, on the UI sequence. Delegate outlives this view. The
// parent owns Profile/window scope, Rust mutations, browser tabs, dialogs and
// failures.
class SidebarView final : public views::View,
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
  enum class LibrarySection {
    kMedia,
    kDownloads,
    kEasels,
    kSpaces,
    kBoosts,
    kArchive
  };

  struct DownloadPresentation {
    uint32_t id = 0;
    std::u16string name;
    std::u16string detail;  // Source host, or why it can't be opened.
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
    virtual void SelectSpace(uint64_t space_id) = 0;
    virtual void CreateSpace() = 0;
    virtual void RenameSpace(uint64_t space_id) = 0;
    virtual void NewTab() = 0;  // Parent opens its real New Tab/palette flow.
    virtual void OpenDownloads() = 0;  // Exact owning browser's Chrome command.
    // Newest first, bounded by the parent. Lookup only.
    virtual std::vector<DownloadPresentation> GetRecentDownloads() const = 0;
    virtual void OpenDownload(uint32_t download_id) = 0;
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
  // Drops mutation controls while loading or after a read/snapshot failure.
  // Parent supplies only a retry supported by its current storage state.
  void SetUnavailable(const std::u16string& reason,
                      StorageRetry retry = StorageRetry::kNone);
  // Fills the New Tab row while the parent's New Tab palette is open.
  void SetNewTabIntent(bool intent);
  // Replaces the sidebar contents with the Library until back or Esc. Needs a
  // usable model.
  void OpenLibrary(LibrarySection section = LibrarySection::kSpaces);
  // Returns to the sidebar rows. Never call from the Library's own handlers.
  void CloseLibrary();
  // Reloads an open Library's Downloads list if it changed.
  void RefreshLibraryDownloads();
  bool library_open() const { return !!library_; }

 private:
  friend class SidebarViewTestApi;

  enum class FocusAction {
    kActivate,
    kClose,
    kRename,
    kNewTab,
    kSpaces,
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
    bool operator==(const ItemMenuState&) const = default;
  };
  struct FocusTarget {
    FocusAction action;
    uint64_t id;
    raw_ptr<views::View> view;
    std::optional<ItemMenuState> item_menu;
  };

  views::View* AddStorageRetry(views::View& parent, StorageRetry retry);
  void TrackFocus(views::View* view,
                  FocusAction action,
                  uint64_t id = 0,
                  std::optional<ItemMenuState> item_menu = std::nullopt);
  // views::View:
  void OnThemeChanged() override;
  // Applies the scheme's tile and New Tab fills to the current rows.
  void UpdateFills();
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;
  // Drops the Library view without notifying the parent.
  void ResetLibrary();
  void BuildCreateMenu();
  void ShowCreateMenu(const ui::Event& event);
  void SetCreateMenuOpen(bool open);
  void ExecuteCommand(int command_id, int event_flags) override;
  std::unique_ptr<views::View> MakeItem(const foundation::ItemRecord& item,
                                        bool tile,
                                        bool selected);

  const raw_ref<Delegate> delegate_;
  std::vector<FocusTarget> focus_targets_;
  raw_ptr<views::ScrollView> spaces_scroll_ = nullptr;
  raw_ptr<views::MdTextButton> new_tab_ = nullptr;
  // Favorite tiles and whether each is selected.
  std::vector<std::pair<raw_ptr<views::MdTextButton>, bool>> tiles_;
  bool new_tab_intent_ = false;
  uint64_t current_space_id_ = 0;
  uint64_t menu_space_id_ = 0;
  uint64_t menu_item_id_ = 0;
  std::vector<std::pair<uint64_t, std::u16string>> spaces_;
  raw_ptr<LibraryView> library_ = nullptr;
  ItemMenuState menu_state_ = {};
  // Stable sidebar owns both; no menu callback or anchor retains a child row.
  // Runner must be destroyed before its model and before children.
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  bool create_menu_open_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SidebarView> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_SIDEBAR_VIEW_H_
