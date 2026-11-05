#include "myarc/profile_swap_cover.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/widget/widget.h"

namespace myarc {

void CaptureWidget(views::Widget& widget,
                   base::OnceCallback<void(gfx::ImageSkia)> done) {
  // The window server's copy, rather than a compositor copy request: the
  // request renders that frame through an extra surface, which drops the
  // sidebar tint for one frame on screen.
  const gfx::Size size = widget.GetWindowBoundsInScreen().size();
  ui::GrabWindowSnapshot(
      widget.GetNativeWindow(), gfx::Rect(size),
      base::BindOnce(
          [](gfx::Size size, base::OnceCallback<void(gfx::ImageSkia)> done,
             gfx::Image image) {
            // The image comes in pixels, at the display's scale.
            const SkBitmap bitmap =
                image.IsEmpty() ? SkBitmap() : image.AsBitmap();
            if (bitmap.drawsNothing() || size.IsEmpty()) {
              std::move(done).Run(gfx::ImageSkia());
              return;
            }
            std::move(done).Run(gfx::ImageSkia::CreateFromBitmap(
                bitmap, static_cast<float>(bitmap.width()) / size.width()));
          },
          size, std::move(done)));
}

ProfileSwapCover::ProfileSwapCover(views::Widget& widget, gfx::ImageSkia image)
    : image_(std::move(image)), layer_(ui::Layer::Create(ui::LAYER_TEXTURED)) {
  ui::Layer* root = widget.GetLayer();
  layer_->set_delegate(this);
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetName("MyarcProfileSwapCover");
  if (root) {
    // Views stack their own layers below foreign ones, so it stays on top.
    layer_->SetBounds(gfx::Rect(root->bounds().size()));
    root->Add(layer_.get());
    root->StackAtTop(layer_.get());
  }
  layer_->SchedulePaint(gfx::Rect(layer_->size()));
}

ProfileSwapCover::~ProfileSwapCover() {
  layer_->set_delegate(nullptr);
}

void ProfileSwapCover::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer_->size());
  recorder.canvas()->DrawImageInt(image_, 0, 0);
}

}  // namespace myarc
