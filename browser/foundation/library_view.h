#ifndef MYARC_LIBRARY_VIEW_H_
#define MYARC_LIBRARY_VIEW_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "myarc/sidebar_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Button;
class MenuRunner;
class ScrollView;
class Textfield;
}  // namespace views

namespace myarc {

class FooterButton;
class FooterTooltip;

// Arc's Library: a section rail and one section pane. Presentation only; the
// Space and download actions go through the sidebar's Delegate. `close`
// destroys this view, so the view runs it only from a posted task.
class LibraryView final : public views::View,
                          public views::TextfieldController,
                          public views::ContextMenuController,
                          public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(LibraryView, views::View)

 public:
  using Section = SidebarView::LibrarySection;
  using Spaces = std::vector<SidebarView::LibrarySpace>;

  LibraryView(SidebarView::Delegate& delegate,
              base::RepeatingClosure close,
              Section section,
              base::WeakPtr<FooterTooltip> tooltip);
  ~LibraryView() override;

  // Rebuilds the Spaces section only when its Spaces, their Pins or the
  // current one changed.
  void SetSpaces(Spaces spaces, uint64_t current);
  void SelectSection(Section section);
  void FocusSection();
  // A click leaves no focus ring, so a click-opened Library holds focus itself
  // and Esc reaches it rather than the page. It stays focusable only until
  // focus moves on, so Tab never stops on it.
  void HoldFocus();
  // Reloads the Downloads list when it is showing and its entries changed.
  void RefreshDownloads();
  // Reloads the Archived Tabs list when it is showing and its entries changed.
  void RefreshArchive();
  // Shows the Exit Library tooltip at once and keeps it up, for captures.
  void ShowExitTooltipNow();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnBlur() override;
  void OnThemeChanged() override;

 private:
  void Rebuild();
  // `offsets` holds the scroll offsets to put back: key 0 for the row of
  // cards, a Space's ID for its card's Pins.
  void BuildSpaces(const std::map<uint64_t, gfx::PointF>& offsets);
  void BuildList();
  // Fills the Downloads or Archived Tabs list from the cached entries.
  // `keep_scroll` keeps the list's scroll offset across the rebuild.
  void UpdateList(bool keep_scroll);
  void AddArchiveRows(views::View& rows);
  // The keyed control that has focus, and putting focus back on it after a
  // rebuild. Returns false when no control has `key` any more.
  std::optional<uint64_t> FocusedKey() const;
  bool RestoreFocus(std::optional<uint64_t> key);
  // Runs `action` after the pressed button's handler unwinds, since the
  // action can rebuild or close this view.
  void PostAction(base::OnceClosure action);

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // A Space card's "..." menu, under `anchor`.
  void ShowSpaceMenu(uint64_t space,
                     views::View* anchor,
                     ui::mojom::MenuSourceType source_type);

  // views::ContextMenuController, for Arc's right-click on the Archived Tabs
  // rail item:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  const raw_ref<SidebarView::Delegate> delegate_;
  const base::RepeatingClosure close_;
  Section section_;
  Spaces spaces_;
  uint64_t current_space_ = 0;
  std::vector<raw_ptr<views::Button>> rail_items_;
  raw_ptr<FooterButton> exit_ = nullptr;
  raw_ptr<views::View> pane_ = nullptr;
  raw_ptr<views::Textfield> search_ = nullptr;
  raw_ptr<views::View> list_ = nullptr;
  // The Spaces section's scrolling views, keyed like BuildSpaces' `offsets`,
  // so a rebuild (a favicon, a rename) keeps every card where it was.
  std::vector<std::pair<uint64_t, raw_ptr<views::ScrollView>>> scrolls_;
  // Pane controls keyed by the Space, Pin, download or archived tab they act
  // on (0 for Create and Show All Downloads), so a rebuild can put focus back.
  // Pin keys carry library::kPinKey to keep them apart from Space IDs.
  std::vector<std::pair<uint64_t, raw_ptr<views::View>>> keyed_views_;
  // Recent downloads, unfiltered, while the Downloads section shows.
  std::vector<SidebarView::DownloadPresentation> downloads_;
  // Archived Tabs, newest first and unfiltered, while that section shows.
  std::vector<SidebarView::ArchivedTabPresentation> archived_;
  // Archived Tabs whose reopen is queued, so a second click does nothing.
  std::set<uint64_t> reopening_;
  std::u16string query_;
  // Surface and text colors the pane was last built with.
  std::optional<std::pair<SkColor, SkColor>> theme_colors_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  // The Space a card's "..." menu acts on.
  uint64_t menu_space_ = 0;
  base::WeakPtrFactory<LibraryView> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_LIBRARY_VIEW_H_
