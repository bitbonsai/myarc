#ifndef MYARC_PROFILE_SWAP_COVER_H_
#define MYARC_PROFILE_SWAP_COVER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace myarc {

// Copies everything `widget` shows on screen, pages included. Runs `done` on
// this sequence, with a null image if the copy fails.
void CaptureWidget(views::Widget& widget,
                   base::OnceCallback<void(gfx::ImageSkia)> done);

// Paints `image` over all of `widget`, pages included, until destroyed. A
// Profile swap puts it on the incoming window, so that window looks like the
// outgoing one until its own content is on screen.
class ProfileSwapCover final : public ui::LayerDelegate {
 public:
  ProfileSwapCover(views::Widget& widget, gfx::ImageSkia image);
  ProfileSwapCover(const ProfileSwapCover&) = delete;
  ProfileSwapCover& operator=(const ProfileSwapCover&) = delete;
  ~ProfileSwapCover() override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

 private:
  gfx::ImageSkia image_;
  std::unique_ptr<ui::Layer> layer_;
};

}  // namespace myarc

#endif  // MYARC_PROFILE_SWAP_COVER_H_
