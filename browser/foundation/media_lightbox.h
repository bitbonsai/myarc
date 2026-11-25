#ifndef MYARC_MEDIA_LIGHTBOX_H_
#define MYARC_MEDIA_LIGHTBOX_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "myarc/sidebar_view.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/widget/widget.h"

namespace views {
class ImageButton;
class Label;
class MdTextButton;
class MenuRunner;
class View;
class WidgetDelegateView;
}  // namespace views

namespace myarc {

// Window-modal lightbox for Library -> Media items: dimmed scrim over the
// browser window, dark header bar with close / < > stepping / filename / age /
// ... menu / Open in Preview, and centered aspect-fit preview canvas.
class MediaLightbox final : public ui::SimpleMenuModel::Delegate {
 public:
  MediaLightbox(views::Widget& parent,
                SidebarView::Delegate& delegate,
                std::vector<SidebarView::MediaItemPresentation> items,
                size_t index,
                base::OnceClosure on_closed);
  MediaLightbox(const MediaLightbox&) = delete;
  MediaLightbox& operator=(const MediaLightbox&) = delete;
  ~MediaLightbox() override;

  void Close();
  void Step(int delta);
  void ShowMoreMenu(views::View* anchor);
  void OpenCurrentInPreview();
  void OnWidgetClosing();

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  class CanvasView;
  class RootView;

  void UpdateCurrent();
  void RequestPreviewForIndex(size_t index, bool apply_if_current);
  void OnPreviewDecoded(uint64_t id,
                        bool apply_if_current,
                        gfx::ImageSkia image);

  const raw_ref<SidebarView::Delegate> delegate_;
  std::vector<SidebarView::MediaItemPresentation> items_;
  size_t index_ = 0;
  base::OnceClosure on_closed_;
  std::map<uint64_t, gfx::ImageSkia> previews_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::ImageButton> prev_button_ = nullptr;
  raw_ptr<views::ImageButton> next_button_ = nullptr;
  raw_ptr<views::Label> filename_label_ = nullptr;
  raw_ptr<views::Label> age_label_ = nullptr;
  raw_ptr<views::ImageButton> more_button_ = nullptr;
  raw_ptr<views::MdTextButton> preview_button_ = nullptr;
  raw_ptr<CanvasView> canvas_ = nullptr;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  bool closing_ = false;
  base::WeakPtrFactory<MediaLightbox> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_MEDIA_LIGHTBOX_H_
