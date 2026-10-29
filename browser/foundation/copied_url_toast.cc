#include "myarc/copied_url_toast.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

// Lucide "share" (ISC, lucide-static 1.61.0): 24 DIP canvas, stroke 2.
constexpr gfx::PathElement kSharePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 12, 2,
    gfx::R_V_LINE_TO, 13, gfx::NEW_PATH, gfx::STROKE, 2, gfx::MOVE_TO, 16, 6,
    gfx::R_LINE_TO, -4, -4, gfx::R_LINE_TO, -4, 4, gfx::NEW_PATH, gfx::STROKE,
    2, gfx::MOVE_TO, 4, 12, gfx::R_V_LINE_TO, 8, gfx::R_ARC_TO, 2, 2, 0, 0, 0,
    2, 2, gfx::R_H_LINE_TO, 12, gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, -2,
    gfx::R_V_LINE_TO, -8};
constexpr gfx::VectorIconRep kShareRep[] = {{kSharePath}};
constexpr gfx::VectorIcon kShareIcon(kShareRep,
                                     std::size(kShareRep),
                                     "myarc_share");

// Measured from Arc's toast (DIP): 46 tall, 10 radius, 12 side insets, a
// 31x27 icon well with 6 radius, 13 between glyphs and well. views::Label adds
// about 1 DIP leading and 6 trailing, so the layout values are smaller.
constexpr int kHeight = 46;
constexpr float kRadius = 10;
constexpr int kInset = 11;
constexpr int kGap = 7;
constexpr gfx::Size kWellSize(31, 27);
constexpr float kWellRadius = 6;
constexpr int kIconSize = 17;
constexpr int kLabelSize = 14;
// Offset from the window's top-right corner.
constexpr int kTopMargin = 19;
constexpr int kRightMargin = 17;
constexpr SkColor kFillTop = SkColorSetRGB(0x29, 0x13, 0x52);
constexpr SkColor kFillBottom = SkColorSetRGB(0x20, 0x0F, 0x40);
constexpr SkColor kWell = SkColorSetA(SK_ColorWHITE, 0x33);
constexpr base::TimeDelta kFadeIn = base::Milliseconds(150);
constexpr base::TimeDelta kHold = base::Milliseconds(2000);
constexpr base::TimeDelta kFadeOut = base::Milliseconds(200);
constexpr char16_t kText[] = u"Copied Current URL";

class ToastView : public views::BoxLayoutView {
  METADATA_HEADER(ToastView, views::BoxLayoutView)

 public:
  ToastView() {
    SetInsideBorderInsets(gfx::Insets::VH(0, kInset));
    SetBetweenChildSpacing(kGap);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
    GetViewAccessibility().SetName(std::u16string(kText));

    auto* label = AddChildView(std::make_unique<views::Label>(kText));
    const gfx::FontList base;
    label->SetFontList(base.Derive(kLabelSize - base.GetFontSize(),
                                   gfx::Font::NORMAL,
                                   gfx::Font::Weight::SEMIBOLD));
    label->SetEnabledColor(SK_ColorWHITE);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);

    auto* well = AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(kShareIcon, SK_ColorWHITE, kIconSize)));
    well->SetPreferredSize(kWellSize);
    well->SetBackground(views::CreateRoundedRectBackground(kWell, kWellRadius));

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(
        views::BoxLayoutView::CalculatePreferredSize(available_size).width(),
        kHeight);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(), gfx::Point(0, height()), kFillTop, kFillBottom));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kRadius, flags);
  }
};

BEGIN_METADATA(ToastView)
END_METADATA

}  // namespace

CopiedUrlToast::CopiedUrlToast(views::Widget* parent) : parent_(parent) {}

CopiedUrlToast::~CopiedUrlToast() = default;

void CopiedUrlToast::Show(bool hold) {
  if (!widget_) {
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.SetParent(parent_);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = false;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    params.name = "MyarcCopiedUrlToast";
    widget_ = std::make_unique<views::Widget>(std::move(params));
    contents_ = widget_->SetContentsView(std::make_unique<ToastView>());
  }

  const gfx::Rect window = parent_->GetClientAreaBoundsInScreen();
  const gfx::Size size = contents_->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(window.right() - kRightMargin - size.width(),
                               window.y() + kTopMargin, size.width(),
                               size.height()));

  ui::Layer* layer = contents_->layer();
  if (!widget_->IsVisible()) {
    layer->SetOpacity(0);
  }
  widget_->ShowInactive();
  {
    ui::ScopedLayerAnimationSettings fade(layer->GetAnimator());
    fade.SetTransitionDuration(gfx::Animation::PrefersReducedMotion()
                                   ? base::TimeDelta()
                                   : kFadeIn);
    layer->SetOpacity(1);
  }
  contents_->GetViewAccessibility().AnnounceText(std::u16string(kText));

  timer_.Stop();
  if (!hold) {
    timer_.Start(FROM_HERE, kHold,
                 base::BindOnce(&CopiedUrlToast::FadeOut,
                                base::Unretained(this)));
  }
}

bool CopiedUrlToast::IsShowing() const {
  return widget_ && widget_->IsVisible();
}

void CopiedUrlToast::FadeOut() {
  if (gfx::Animation::PrefersReducedMotion()) {
    Hide();
    return;
  }
  ui::Layer* layer = contents_->layer();
  {
    ui::ScopedLayerAnimationSettings fade(layer->GetAnimator());
    fade.SetTransitionDuration(kFadeOut);
    layer->SetOpacity(0);
  }
  timer_.Start(FROM_HERE, kFadeOut,
               base::BindOnce(&CopiedUrlToast::Hide, base::Unretained(this)));
}

void CopiedUrlToast::Hide() {
  if (widget_) {
    widget_->Hide();
  }
}

}  // namespace myarc
