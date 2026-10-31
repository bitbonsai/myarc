#ifndef MYARC_LIBRARY_VIEW_H_
#define MYARC_LIBRARY_VIEW_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "myarc/sidebar_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Button;
class Textfield;
}  // namespace views

namespace myarc {

// Arc's Library: a section rail and one section pane. Presentation only; the
// Space and download actions go through the sidebar's Delegate. `close` must
// post, because it destroys this view.
class LibraryView final : public views::View,
                          public views::TextfieldController {
  METADATA_HEADER(LibraryView, views::View)

 public:
  using Section = SidebarView::LibrarySection;
  using Spaces = std::vector<std::pair<uint64_t, std::u16string>>;

  LibraryView(SidebarView::Delegate& delegate,
              base::RepeatingClosure close,
              Section section);
  ~LibraryView() override;

  // Rebuilds the Spaces section only when its Spaces or the current one
  // changed.
  void SetSpaces(Spaces spaces, uint64_t current);
  void SelectSection(Section section);
  void FocusSection();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnThemeChanged() override;

 private:
  void Rebuild();
  void BuildSpaces();
  void BuildList();
  void UpdateDownloads();
  // Runs `action` after the pressed button's handler unwinds, since the
  // action can rebuild or close this view.
  void PostAction(base::OnceClosure action);

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  const raw_ref<SidebarView::Delegate> delegate_;
  const base::RepeatingClosure close_;
  Section section_;
  Spaces spaces_;
  uint64_t current_space_ = 0;
  std::vector<raw_ptr<views::Button>> rail_items_;
  raw_ptr<views::View> pane_ = nullptr;
  raw_ptr<views::Textfield> search_ = nullptr;
  raw_ptr<views::View> list_ = nullptr;
  std::u16string query_;
  base::WeakPtrFactory<LibraryView> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_LIBRARY_VIEW_H_
