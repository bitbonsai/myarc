#include "myarc/welcome_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
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
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

constexpr char16_t kTitle[] = u"Welcome to myArc";
constexpr char16_t kBody[] =
    u"Everything stays on this Mac, and there's no account to make. Bring "
    u"your Spaces over from Arc, or start with a clean sidebar.";
constexpr char16_t kStart[] = u"Start browsing";
constexpr char16_t kImport[] = u"Import from Arc…";
constexpr char16_t kImported[] = u"Imported from Arc";
constexpr char16_t kSkip[] = u"Skip tour";

constexpr int kColumnWidth = 360;
constexpr int kButtonWidth = 240;
constexpr int kButtonHeight = 36;
constexpr int kButtonRadius = 8;
constexpr SkColor kPrimary = SkColorSetRGB(0x3D, 0x4B, 0xF2);

// A field of two quiet tints, light or dark.
constexpr SkColor kFieldLightTop = SkColorSetRGB(0xF1, 0xF0, 0xFF);
constexpr SkColor kFieldLightBottom = SkColorSetRGB(0xFB, 0xF1, 0xF6);
constexpr SkColor kFieldDarkTop = SkColorSetRGB(0x1F, 0x20, 0x33);
constexpr SkColor kFieldDarkBottom = SkColorSetRGB(0x2A, 0x20, 0x33);
// The dim over the window around the sheet, sidebar included.
constexpr SkAlpha kDimLight = 0x47;
constexpr SkAlpha kDimDark = 0x73;

// The intro. Each phase is a window of milliseconds within it.
constexpr base::TimeDelta kIntro = base::Milliseconds(1000);
constexpr base::TimeDelta kReducedFade = base::Milliseconds(150);
constexpr int kIntroFrameRate = 60;
struct Phase {
  int start;
  int end;
};
constexpr Phase kDimIn{0, 400};
constexpr Phase kGather{0, 600};
constexpr Phase kMerge{480, 620};
constexpr Phase kGrow{520, 850};
constexpr Phase kFieldIn{520, 620};
constexpr Phase kStrip{700, 1000};
constexpr Phase kTitleIn{600, 850};
constexpr Phase kBodyIn{720, 970};
constexpr Phase kButtonsIn{840, 1000};
constexpr float kRise = 8;
constexpr float kTextBlur = 6;
constexpr float kDotRadius = 18;
constexpr float kSeedSize = 56;

// The Space theme swatches, one dot each, and where on the window's edge
// (as fractions of its size) each one starts.
struct Dot {
  SkColor color;
  float x;
  float y;
};
constexpr Dot kDots[] = {
    {SkColorSetRGB(0xF2, 0x9B, 0xBB), 0.0f, 0.22f},
    {SkColorSetRGB(0xA6, 0x72, 0x9E), 0.28f, 0.0f},
    {SkColorSetRGB(0xF2, 0x5E, 0x6C), 0.74f, 0.0f},
    {SkColorSetRGB(0xFF, 0x86, 0x57), 1.0f, 0.36f},
    {SkColorSetRGB(0xF8, 0xD5, 0x58), 1.0f, 0.86f},
    {SkColorSetRGB(0x33, 0xE8, 0x95), 0.52f, 1.0f},
    {SkColorSetRGB(0x6D, 0xBA, 0xD9), 0.0f, 0.78f},
};

// Where `progress` sits in `phase`, 0 to 1, eased by `tween`.
double At(double progress,
          Phase phase,
          gfx::Tween::Type tween = gfx::Tween::LINEAR) {
  const double ms = progress * kIntro.InMillisecondsF();
  return gfx::Tween::CalculateValue(
      tween,
      std::clamp((ms - phase.start) / (phase.end - phase.start), 0.0, 1.0));
}

gfx::FontList Font(int delta, gfx::Font::Weight weight) {
  return gfx::FontList().Derive(delta, gfx::Font::NORMAL, weight);
}

// LabelButton hides its label, so set the font on its Label child.
void SetButtonFont(views::LabelButton& button, const gfx::FontList& font) {
  for (views::View* child : button.children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(font);
      // The sheet paints to a translucent layer.
      label->SetSubpixelRenderingEnabled(false);
    }
  }
}

// macOS views buttons ignore Return and take focus only with Full Keyboard
// Access; the sheet needs both.
class SheetButton : public views::LabelButton {
  METADATA_HEADER(SheetButton, views::LabelButton)

 public:
  SheetButton(PressedCallback callback, const std::u16string& text)
      : views::LabelButton(std::move(callback), text) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kButtonRadius);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_RETURN) {
      if (GetEnabled() && !event.is_repeat()) {
        NotifyClick(event);
      }
      return true;
    }
    return views::LabelButton::OnKeyPressed(event);
  }
  // Keeps Return from reaching the window's accelerators first.
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override {
    return event.key_code() == ui::VKEY_RETURN ||
           views::LabelButton::SkipDefaultKeyEventProcessing(event);
  }
};

BEGIN_METADATA(SheetButton)
END_METADATA

bool IsDark(const views::View& view) {
  const ui::ColorProvider* provider = view.GetColorProvider();
  return provider &&
         color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
}

}  // namespace

WelcomeView::WelcomeView(bool offers_import,
                         RadiusCallback radius,
                         ActionCallback action)
    : radius_(std::move(radius)),
      action_(std::move(action)),
      reduced_motion_(gfx::Animation::PrefersReducedMotion()),
      intro_(kIntro, kIntroFrameRate, this) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(24), 0));
  layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(std::u16string(kTitle));
  GetViewAccessibility().SetDescription(std::u16string(kBody));

  auto title = std::make_unique<views::Label>(kTitle);
  title->SetFontList(Font(14, gfx::Font::Weight::BOLD));
  title->SetAutoColorReadabilityEnabled(false);
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetSubpixelRenderingEnabled(false);
  title->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
  title_ = AddChildView(std::move(title));

  auto body = std::make_unique<views::Label>(kBody);
  body->SetFontList(Font(2, gfx::Font::Weight::NORMAL));
  body->SetMultiLine(true);
  body->SetLineHeight(22);
  body->SetMaximumWidth(kColumnWidth);
  body->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  body->SetAutoColorReadabilityEnabled(false);
  body->SetBackgroundColor(SK_ColorTRANSPARENT);
  body->SetSubpixelRenderingEnabled(false);
  body->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, 0, 28, 0));
  body_ = AddChildView(std::move(body));

  const auto pressed = [](WelcomeView* view, Action action) {
    view->Press(action);
  };
  start_ = AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this),
                          Action::kStartBrowsing),
      kStart));
  import_ = AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this), Action::kImport),
      offers_import ? kImport : kImported));
  import_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(10, 0, 0, 0));
  import_->SetEnabled(offers_import);
  skip_ = AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this), Action::kSkipTour),
      kSkip));
  skip_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(16, 0, 0, 0));
  skip_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 10)));
  for (views::LabelButton* button : {start_.get(), import_.get()}) {
    button->SetPreferredSize(gfx::Size(kButtonWidth, kButtonHeight));
    button->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    SetButtonFont(*button, Font(1, gfx::Font::Weight::SEMIBOLD));
  }
  SetButtonFont(*skip_, Font(0, gfx::Font::Weight::MEDIUM));

  for (views::View* child : children()) {
    child->SetPaintToLayer();
    child->layer()->SetFillsBoundsOpaquely(false);
  }
  if (reduced_motion_) {
    progress_ = 1;
    layer()->SetOpacity(0);
  } else {
    ApplyIntro(0);
  }
}

WelcomeView::~WelcomeView() = default;

void WelcomeView::SetSheetBounds(const gfx::Rect& sheet) {
  sheet_ = sheet;
  layout_->set_inside_border_insets(
      gfx::Insets::TLBR(sheet.y() + 24, sheet.x() + 24,
                        std::max(0, height() - sheet.bottom()) + 24,
                        std::max(0, width() - sheet.right()) + 24));
  InvalidateLayout();
  SchedulePaint();
  // A layer at zero opacity never paints, so the reduced fade can't wait for
  // the first paint the way the intro does.
  if (reduced_motion_ && !intro_started_ && !sheet_.IsEmpty()) {
    intro_started_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WelcomeView::StartIntro, weak_factory_.GetWeakPtr()));
  }
}

void WelcomeView::FinishIntro() {
  // Drops a start that is still posted.
  intro_started_ = true;
  weak_factory_.InvalidateWeakPtrs();
  if (reduced_motion_) {
    layer()->GetAnimator()->StopAnimating();
    layer()->SetOpacity(1);
    return;
  }
  if (intro_.is_animating()) {
    intro_.End();
    return;
  }
  progress_ = 1;
  ApplyIntro(1);
  SchedulePaint();
}

void WelcomeView::StartIntro() {
  if (reduced_motion_) {
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(kReducedFade)
        .SetOpacity(layer(), 1.f, gfx::Tween::EASE_OUT);
    return;
  }
  intro_.Start();
}

void WelcomeView::ApplyIntro(double progress) {
  const auto text = [progress](views::View* view, Phase phase) {
    const float in =
        static_cast<float>(At(progress, phase, gfx::Tween::EASE_OUT));
    view->layer()->SetOpacity(in);
    view->layer()->SetTransform(
        gfx::Transform::MakeTranslation(0, kRise * (1 - in)));
    view->layer()->SetLayerBlur(kTextBlur * (1 - in));
  };
  text(title_, kTitleIn);
  text(body_, kBodyIn);
  // The buttons only fade, so they never move while they can be pressed.
  for (views::View* button : {start_.get(), import_.get(), skip_.get()}) {
    button->layer()->SetOpacity(
        static_cast<float>(At(progress, kButtonsIn, gfx::Tween::EASE_OUT)));
  }
}

bool WelcomeView::DoesIntersectRect(const views::View* target,
                                    const gfx::Rect& rect) const {
  return sheet_.Intersects(rect);
}

void WelcomeView::AnimationProgressed(const gfx::Animation* animation) {
  progress_ = animation->GetCurrentValue();
  ApplyIntro(progress_);
  SchedulePaint();
}

void WelcomeView::AnimationEnded(const gfx::Animation* animation) {
  progress_ = 1;
  ApplyIntro(1);
  SchedulePaint();
}

void WelcomeView::TakeFocus() {
  SetPaneFocusAndFocusDefault();
  GetViewAccessibility().AnnounceText(std::u16string(kTitle) + u". " +
                                      std::u16string(kBody));
}

bool WelcomeView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    Press(Action::kSkipTour);
    return true;
  }
  return views::AccessiblePaneView::AcceleratorPressed(accelerator);
}

views::View* WelcomeView::GetDefaultFocusableChild() {
  return start_;
}

void WelcomeView::OnPaint(gfx::Canvas* canvas) {
  if (!intro_started_ && !sheet_.IsEmpty()) {
    // Starts once the first frame is on screen, so none of it plays unseen.
    intro_started_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WelcomeView::StartIntro, weak_factory_.GetWeakPtr()));
  }
  const bool dark = IsDark(*this);
  const double t = progress_;
  const gfx::RectF sheet(sheet_);
  const float radius = radius_ ? radius_.Run() : 0;

  cc::PaintFlags dim;
  dim.setColor(
      SkColorSetA(SK_ColorBLACK,
                  static_cast<SkAlpha>((dark ? kDimDark : kDimLight) *
                                       At(t, kDimIn, gfx::Tween::EASE_OUT))));
  canvas->DrawRect(gfx::RectF(GetLocalBounds()), dim);

  // The dots drift in and meet in the middle of the sheet.
  const gfx::PointF center = sheet.CenterPoint();
  const double gather = At(t, kGather, gfx::Tween::EASE_OUT);
  const double merge = At(t, kMerge, gfx::Tween::EASE_IN);
  if (merge < 1) {
    for (const Dot& dot : kDots) {
      const gfx::PointF from(dot.x * width(), dot.y * height());
      const gfx::PointF at(
          gfx::Tween::FloatValueBetween(gather, from.x(), center.x()),
          gfx::Tween::FloatValueBetween(gather, from.y(), center.y()));
      const float r = kDotRadius * static_cast<float>(1 - 0.5 * merge);
      SkColor4f color = SkColor4f::FromColor(dot.color);
      color.fA = 0.95f * static_cast<float>(1 - merge);
      SkColor4f clear = color;
      clear.fA = 0;
      const SkColor4f colors[] = {color, color, clear};
      const float stops[] = {0, 0.35f, 1};
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setShader(cc::PaintShader::MakeRadialGradient(
          SkPoint::Make(at.x(), at.y()), r, colors, stops, 3,
          SkTileMode::kClamp));
      canvas->DrawCircle(at, r, flags);
    }
  }

  // They join into one rounded square that widens to the sheet.
  const double grow = At(t, kGrow, gfx::Tween::FAST_OUT_SLOW_IN);
  const double field_in = At(t, kFieldIn);
  if (field_in > 0 && !sheet.IsEmpty()) {
    const gfx::RectF seed(center.x() - kSeedSize / 2,
                          center.y() - kSeedSize / 2, kSeedSize, kSeedSize);
    const gfx::RectF field = gfx::Tween::RectFValueBetween(grow, seed, sheet);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setAlphaf(static_cast<float>(field_in));
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(0, static_cast<int>(field.y())),
        gfx::Point(0, static_cast<int>(field.bottom())),
        dark ? kFieldDarkTop : kFieldLightTop,
        dark ? kFieldDarkBottom : kFieldLightBottom));
    canvas->DrawRoundRect(
        field, gfx::Tween::FloatValueBetween(grow, kSeedSize / 2, radius),
        flags);
  }

  // A strip splits off the sheet's left edge and slides into the sidebar's
  // place, fading into the dimmed sidebar.
  const double strip = At(t, kStrip, gfx::Tween::FAST_OUT_SLOW_IN);
  const double strip_linear = At(t, kStrip);
  if (strip_linear > 0 && strip_linear < 1 && sheet.x() > 40) {
    const gfx::RectF from(sheet.x(), sheet.y() + sheet.height() * 0.2f, 6,
                          sheet.height() * 0.6f);
    const gfx::RectF to(8, sheet.y(), sheet.x() - 16, sheet.height());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(
        dark ? kFieldDarkBottom : kFieldLightTop,
        static_cast<SkAlpha>(0xE6 * std::min(1.0, strip_linear * 5) *
                             (1 - strip_linear))));
    canvas->DrawRoundRect(gfx::Tween::RectFValueBetween(strip, from, to), 10,
                          flags);
  }
}

void WelcomeView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  const bool dark = IsDark(*this);
  const SkColor ink = dark ? SK_ColorWHITE : SkColorSetRGB(0x1A, 0x1A, 0x22);
  views::AsViewClass<views::Label>(title_.get())->SetEnabledColor(ink);
  views::AsViewClass<views::Label>(body_.get())
      ->SetEnabledColor(SkColorSetA(ink, 0xB8));
  start_->SetBackground(
      views::CreateRoundedRectBackground(kPrimary, kButtonRadius));
  start_->SetEnabledTextColors(SK_ColorWHITE);
  import_->SetBackground(views::CreateRoundedRectBackground(
      SkColorSetA(ink, dark ? 0x1F : 0x12), kButtonRadius));
  import_->SetEnabledTextColors(ink);
  import_->SetTextColor(views::Button::STATE_DISABLED, SkColorSetA(ink, 0x80));
  skip_->SetEnabledTextColors(SkColorSetA(ink, 0x99));
  SchedulePaint();
}

void WelcomeView::Press(Action action) {
  FinishIntro();
  // A copy: the action may close the sheet and delete this view.
  auto run = action_;
  run.Run(action);
}

BEGIN_METADATA(WelcomeView)
END_METADATA

}  // namespace myarc
