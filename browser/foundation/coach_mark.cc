#include "myarc/coach_mark.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

constexpr char16_t kBack[] = u"Back";
constexpr char16_t kNext[] = u"Next";
constexpr char16_t kSkip[] = u"Skip tour";

// The card, pointer included.
constexpr int kWidth = 300;
// How far the pointer reaches out of the card's left edge.
constexpr int kPointer = 8;
constexpr int kPointerHalf = 7;
constexpr float kRadius = 12;
constexpr int kButtonRadius = 6;
constexpr SkColor kPrimary = SkColorSetRGB(0x3D, 0x4B, 0xF2);

constexpr base::TimeDelta kGrow = base::Milliseconds(180);
constexpr base::TimeDelta kReducedFade = base::Milliseconds(150);
constexpr base::TimeDelta kFadeOut = base::Milliseconds(120);
constexpr float kStartScale = 0.96f;

gfx::FontList Font(int delta, gfx::Font::Weight weight) {
  return gfx::FontList().Derive(delta, gfx::Font::NORMAL, weight);
}

std::unique_ptr<views::Label> MakeLabel(const std::u16string& text,
                                        const gfx::FontList& font) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(font);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  // The card paints to a layer that fades.
  label->SetSubpixelRenderingEnabled(false);
  return label;
}

// The card never takes focus, so the shortcut it teaches reaches the window;
// VoiceOver can still reach its buttons.
std::unique_ptr<views::LabelButton> MakeButton(
    views::Button::PressedCallback callback,
    const std::u16string& text) {
  auto button = std::make_unique<views::LabelButton>(std::move(callback), text);
  button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 10)));
  views::InstallRoundRectHighlightPathGenerator(button.get(), gfx::Insets(),
                                                kButtonRadius);
  for (views::View* child : button->children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(Font(0, gfx::Font::Weight::MEDIUM));
      label->SetSubpixelRenderingEnabled(false);
    }
  }
  return button;
}

}  // namespace

CoachMark::CoachMark(const Content& content, ActionCallback action)
    : action_(std::move(action)),
      announcement_(content.title + u". " + content.body +
                    (content.hint.empty() ? u"" : u" " + content.hint)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(14, kPointer + 16, 12, 14), 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(content.title);
  GetViewAccessibility().SetDescription(content.body);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  title_ = AddChildView(
      MakeLabel(content.title, Font(2, gfx::Font::Weight::SEMIBOLD)));
  title_->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
  body_ =
      AddChildView(MakeLabel(content.body, Font(0, gfx::Font::Weight::NORMAL)));
  body_->SetMultiLine(true);
  body_->SetMaximumWidth(kWidth - kPointer - 30);
  body_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(4, 0, 0, 0));
  if (!content.hint.empty()) {
    hint_ = AddChildView(
        MakeLabel(content.hint, Font(0, gfx::Font::Weight::SEMIBOLD)));
    hint_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(3, 8)));
    hint_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(10, 0, 0, 0));
  }

  auto* buttons = AddChildView(std::make_unique<views::BoxLayoutView>());
  buttons->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  buttons->SetBetweenChildSpacing(4);
  buttons->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, -10, 0, 0));
  buttons->SetPreferredSize(gfx::Size(kWidth - kPointer - 20, 28));
  buttons->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  const auto pressed = [](CoachMark* view, Action action) {
    view->Press(action);
  };
  if (content.offers_back) {
    back_ = buttons->AddChildView(MakeButton(
        base::BindRepeating(pressed, base::Unretained(this), Action::kBack),
        kBack));
  }
  skip_ = buttons->AddChildView(MakeButton(
      base::BindRepeating(pressed, base::Unretained(this), Action::kSkipTour),
      kSkip));
  auto* spacer = buttons->AddChildView(std::make_unique<views::View>());
  buttons->SetFlexForView(spacer, 1);
  if (content.offers_next) {
    next_ = buttons->AddChildView(MakeButton(
        base::BindRepeating(pressed, base::Unretained(this), Action::kNext),
        content.next_label.empty() ? std::u16string(kNext)
                                   : content.next_label));
  }
}

CoachMark::~CoachMark() = default;

void CoachMark::Place(const gfx::Rect& anchor, const gfx::Rect& bounds) {
  const gfx::Size size(kWidth, GetHeightForWidth(kWidth));
  if (anchor.IsEmpty()) {
    pointer_y_ = -1;
    SetBoundsRect(gfx::Rect(bounds.origin() + gfx::Vector2d(16, 16), size));
    return;
  }
  const int anchor_y = anchor.CenterPoint().y();
  const int x = std::max(
      bounds.x(), std::min(anchor.right() + 4, bounds.right() - size.width()));
  const int y =
      std::clamp(anchor_y - 28, bounds.y() + 8,
                 std::max(bounds.y() + 8, bounds.bottom() - size.height() - 8));
  pointer_y_ = std::clamp(anchor_y - y, static_cast<int>(kRadius) + 8,
                          size.height() - static_cast<int>(kRadius) - 8);
  SetBoundsRect(gfx::Rect(gfx::Point(x, y), size));
  SchedulePaint();
}

void CoachMark::Show() {
  // Read at every show, so a change mid-tour applies from the next card.
  const bool reduced = gfx::Animation::PrefersReducedMotion();
  layer()->SetOpacity(0);
  if (reduced) {
    layer()->SetTransform(gfx::Transform());
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(kReducedFade)
        .SetOpacity(layer(), 1.f, gfx::Tween::EASE_OUT);
  } else {
    // Grows from the side of its anchor, where the pointer is.
    const gfx::PointF pivot(0, pointer_y_ >= 0 ? pointer_y_ : height() / 2);
    gfx::Transform start;
    start.Scale(kStartScale, kStartScale);
    layer()->SetTransform(gfx::TransformAboutPivot(pivot, start));
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(kGrow)
        .SetOpacity(layer(), 1.f, gfx::Tween::EASE_OUT)
        .SetTransform(layer(), gfx::Transform(), gfx::Tween::EASE_OUT);
  }
  GetViewAccessibility().AnnounceText(announcement_);
}

void CoachMark::FadeOut(base::OnceClosure done) {
  pressed_ = true;
  SetCanProcessEventsWithinSubtree(false);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(done))
      .Once()
      .SetDuration(kFadeOut)
      .SetOpacity(layer(), 0.f, gfx::Tween::EASE_OUT);
}

void CoachMark::Press(Action action) {
  if (pressed_) {
    return;
  }
  pressed_ = true;
  // A copy: the action may remove the card and delete this view.
  auto run = action_;
  run.Run(action);
}

bool CoachMark::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_ESCAPE) {
    return false;
  }
  Press(Action::kSkipTour);
  return true;
}

void CoachMark::OnPaint(gfx::Canvas* canvas) {
  const gfx::RectF card(kPointer + 0.5f, 0.5f, width() - kPointer - 1.f,
                        height() - 1.f);
  SkPathBuilder builder;
  builder.addRRect(
      SkRRect::MakeRectXY(gfx::RectFToSkRect(card), kRadius, kRadius));
  if (pointer_y_ >= 0) {
    builder.moveTo(kPointer + 1.f, pointer_y_ - kPointerHalf);
    builder.lineTo(0.5f, pointer_y_);
    builder.lineTo(kPointer + 1.f, pointer_y_ + kPointerHalf);
    builder.close();
  }
  const SkPath path = builder.detach();
  cc::PaintFlags fill;
  fill.setAntiAlias(true);
  fill.setStyle(cc::PaintFlags::kFill_Style);
  fill.setColor(dark_ ? SkColorSetRGB(0x27, 0x28, 0x2D) : SK_ColorWHITE);
  canvas->DrawPath(path, fill);
  cc::PaintFlags stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(cc::PaintFlags::kStroke_Style);
  stroke.setStrokeWidth(1);
  stroke.setColor(dark_ ? SkColorSetRGB(0x42, 0x43, 0x4A)
                        : SkColorSetRGB(0xDD, 0xDD, 0xE3));
  canvas->DrawPath(path, stroke);
  // The pointer joins the card without a seam.
  if (pointer_y_ >= 0) {
    canvas->FillRect(gfx::Rect(kPointer, pointer_y_ - kPointerHalf + 1, 2,
                               2 * kPointerHalf - 1),
                     fill.getColor());
  }
}

void CoachMark::OnThemeChanged() {
  views::View::OnThemeChanged();
  const ui::ColorProvider* provider = GetColorProvider();
  dark_ =
      provider && color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
  const SkColor ink = dark_ ? SK_ColorWHITE : SkColorSetRGB(0x1A, 0x1A, 0x22);
  title_->SetEnabledColor(ink);
  body_->SetEnabledColor(SkColorSetA(ink, 0xB8));
  if (hint_) {
    hint_->SetEnabledColor(ink);
    hint_->SetBackground(views::CreateRoundedRectBackground(
        SkColorSetA(ink, dark_ ? 0x1F : 0x12), kButtonRadius));
  }
  if (back_) {
    back_->SetEnabledTextColors(SkColorSetA(ink, 0xCC));
  }
  skip_->SetEnabledTextColors(SkColorSetA(ink, 0x99));
  if (next_) {
    next_->SetBackground(
        views::CreateRoundedRectBackground(kPrimary, kButtonRadius));
    next_->SetEnabledTextColors(SK_ColorWHITE);
  }
  SchedulePaint();
}

BEGIN_METADATA(CoachMark)
END_METADATA

}  // namespace myarc
