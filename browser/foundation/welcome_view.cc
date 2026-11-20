#include "myarc/welcome_view.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "myarc/coach_mark.h"
#include "myarc/welcome_mark.h"
#include "myarc/welcome_palette.h"
#include "myarc/welcome_timeline.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

using welcome::kChoiceAt;
using welcome::kChoiceFade;
using welcome::kHeadlineAt;
using welcome::kHeadlineSlide;
using welcome::kMarkDrift;
using welcome::kMarkFade;
using welcome::kMarkFadeAt;
using welcome::kMarkIn;
using welcome::kMarkRise;
using welcome::kWindowShown;

// myArc's own line, as on the myarc.dev hero.
constexpr char16_t kHeadline[] = u"myArc, forever yours.";
constexpr char16_t kImport[] = u"Import from Arc…";
constexpr char16_t kImported[] = u"Imported from Arc";
constexpr char16_t kStart[] = u"Start fresh";
constexpr char16_t kSkip[] = u"Skip tour";

constexpr int kMarkSize = 176;
constexpr int kButtonWidth = 240;
constexpr int kButtonHeight = 40;
// The base in dark mode, myarc.dev's dark page.
constexpr uint32_t kMidnightDark = 0x0B1420;

// The logo plays the site hero's rise-art and drift keyframes, the drift
// shortened, and the headline slides in while a blurred copy of it fades
// out, so its blur clears.
constexpr float kMarkRiseBy = 20;   // 1.25rem
constexpr float kMarkDriftBy = 14;  // .875rem
constexpr float kMarkTurn = 1;      // degrees
constexpr float kHeadlineSlideBy = 56;
constexpr float kHeadlineBlur = 12;
constexpr base::TimeDelta kSharpIn = base::Milliseconds(200);
constexpr base::TimeDelta kBlurIn = base::Milliseconds(300);
// The site's ease runs as this many straight legs on the compositor.
constexpr int kEaseLegs = 10;

constexpr base::TimeDelta kReducedFade = base::Milliseconds(150);
constexpr base::TimeDelta kThemeFade = base::Milliseconds(350);
constexpr base::TimeDelta kExit = base::Milliseconds(350);
constexpr float kExitSlide = 64;

// myarc.dev's ease, cubic-bezier(.2, .7, .2, 1).
double SiteEase(double t) {
  static const gfx::CubicBezier ease(0.2, 0.7, 0.2, 1);
  return ease.Solve(std::clamp(t, 0.0, 1.0));
}

constexpr SkColor Rgb(uint32_t rgb, SkAlpha alpha = SK_AlphaOPAQUE) {
  return SkColorSetARGB(alpha, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF,
                        rgb & 0xFF);
}

constexpr SkColor kInk = Rgb(welcome::kIvory);

gfx::FontList Font(int delta, gfx::Font::Weight weight) {
  return gfx::FontList().Derive(delta, gfx::Font::NORMAL, weight);
}

std::unique_ptr<views::Label> Headline() {
  auto label = std::make_unique<views::Label>(kHeadline);
  label->SetFontList(Font(17, gfx::Font::Weight::BOLD));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  // It paints to a translucent layer.
  label->SetSubpixelRenderingEnabled(false);
  label->SetEnabledColor(kInk);
  return label;
}

// LabelButton hides its label, so set the font on its Label child.
void SetButtonFont(views::LabelButton& button, const gfx::FontList& font) {
  for (views::View* child : button.children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(font);
      label->SetSubpixelRenderingEnabled(false);
    }
  }
}

// The ribbon logo, decoded once, kMarkSize DIP wide.
const gfx::ImageSkia& Mark() {
  static const gfx::ImageSkia* const mark = [] {
    const SkBitmap bitmap = gfx::PNGCodec::Decode(base::span(kWelcomeMarkPng));
    return new gfx::ImageSkia(
        bitmap.isNull()
            ? gfx::ImageSkia()
            : gfx::ImageSkia::CreateFromBitmap(
                  bitmap, static_cast<float>(bitmap.width()) / kMarkSize));
  }();
  return *mark;
}

// The logo `up` DIP up and turned `turn` degrees about its centre.
gfx::Transform MarkAt(float up, float turn) {
  const float half = kMarkSize / 2.f;
  gfx::Transform transform = gfx::Transform::MakeTranslation(0, -up);
  transform.Translate(half, half);
  transform.Rotate(turn);
  transform.Translate(-half, -half);
  return transform;
}

// macOS views buttons ignore Return and take focus only with Full Keyboard
// Access; the welcome needs both.
class SheetButton : public views::LabelButton {
  METADATA_HEADER(SheetButton, views::LabelButton)

 public:
  SheetButton(PressedCallback callback, const std::u16string& text, int radius)
      : views::LabelButton(std::move(callback), text) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    // The welcome keeps its contrast while another app is active, as on a
    // first launch from the Finder.
    SetAppearDisabledInInactiveWidget(false);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), radius);
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

// A view on its own layer that takes no events.
void OnLayer(views::View& view) {
  view.SetPaintToLayer();
  view.layer()->SetFillsBoundsOpaquely(false);
  view.SetCanProcessEventsWithinSubtree(false);
}

// One of the background's blurred fields: a radial gradient filling its
// square bounds, on its own layer so the compositor drifts it.
class Field : public views::View {
  METADATA_HEADER(Field, views::View)

 public:
  explicit Field(const welcome::Field& field) : field_(field) {
    OnLayer(*this);
  }

  const welcome::Field& field() const { return field_; }

  void OnPaint(gfx::Canvas* canvas) override {
    const float radius = width() / 2.f;
    if (radius <= 0) {
      return;
    }
    SkColor4f color = SkColor4f::FromColor(Rgb(field_.rgb));
    color.fA = field_.alpha;
    SkColor4f mid = color;
    mid.fA = field_.alpha * welcome::kFieldMid;
    SkColor4f clear = color;
    clear.fA = 0;
    const SkColor4f colors[] = {color, mid, clear};
    const float stops[] = {0, welcome::kFieldMidStop, 1};
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setShader(cc::PaintShader::MakeRadialGradient(
        SkPoint::Make(radius, radius), radius, colors, stops, 3,
        SkTileMode::kClamp));
    canvas->DrawRect(gfx::RectF(GetLocalBounds()), flags);
  }

 private:
  const welcome::Field field_;
};

BEGIN_METADATA(Field)
END_METADATA

// An imported Space's theme over the whole window, diagonal like the
// sidebar's tint.
class Theme : public views::View {
  METADATA_HEADER(Theme, views::View)

 public:
  Theme() { OnLayer(*this); }

  void SetTint(SidebarTint tint) {
    tint_ = tint;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    if (!tint_) {
      return;
    }
    cc::PaintFlags flags;
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(), gfx::Point(width(), height()), tint_->start, tint_->end));
    canvas->DrawRect(gfx::RectF(GetLocalBounds()), flags);
  }

 private:
  std::optional<SidebarTint> tint_;
};

BEGIN_METADATA(Theme)
END_METADATA

}  // namespace

WelcomeView::WelcomeView(bool offers_import, ActionCallback action)
    : action_(std::move(action)),
      reduced_motion_(gfx::Animation::PrefersReducedMotion()) {
  SetPaintToLayer();
  layer()->SetMasksToBounds(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(std::u16string(kHeadline));

  size_t i = 0;
  for (const welcome::Field& spec : welcome::kFields) {
    fields_[i++] = AddChildView(std::make_unique<Field>(spec));
  }
  theme_ = AddChildView(std::make_unique<Theme>());
  theme_->layer()->SetOpacity(0);

  content_ = AddChildView(std::make_unique<views::View>());
  content_->SetPaintToLayer();
  content_->layer()->SetFillsBoundsOpaquely(false);

  auto mark =
      std::make_unique<views::ImageView>(ui::ImageModel::FromImageSkia(Mark()));
  mark->SetImageSize(gfx::Size(kMarkSize, kMarkSize));
  mark->GetViewAccessibility().SetIsIgnored(true);
  mark_ = content_->AddChildView(std::move(mark));
  OnLayer(*mark_);

  // The blurred copy sits under the headline and fades out as it sharpens.
  headline_blur_ = content_->AddChildView(Headline());
  headline_blur_->GetViewAccessibility().SetIsIgnored(true);
  OnLayer(*headline_blur_);
  headline_blur_->layer()->SetLayerBlur(kHeadlineBlur);
  headline_ = content_->AddChildView(Headline());
  headline_->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
  OnLayer(*headline_);

  const auto pressed = [](WelcomeView* view, Action action) {
    view->Press(action);
  };
  import_ = content_->AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this), Action::kImport),
      offers_import ? kImport : kImported, kButtonHeight / 2));
  import_->SetEnabled(offers_import);
  start_ = content_->AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this),
                          Action::kStartBrowsing),
      kStart, kButtonHeight / 2));
  // The primary button is the site blue: Import from Arc…, or Start fresh
  // once the import is done. The other one is translucent ivory.
  views::LabelButton* primary = offers_import ? import_.get() : start_.get();
  views::LabelButton* secondary = offers_import ? start_.get() : import_.get();
  primary->SetBackground(views::CreateRoundedRectBackground(
      Rgb(welcome::kSiteBlue), kButtonHeight / 2));
  secondary->SetBackground(views::CreateRoundedRectBackground(
      SkColorSetA(kInk, 0x2E), kButtonHeight / 2));
  for (views::LabelButton* button : {import_.get(), start_.get()}) {
    button->SetEnabledTextColors(kInk);
    button->SetTextColor(views::Button::STATE_DISABLED,
                         SkColorSetA(kInk, 0x99));
    button->SetPreferredSize(gfx::Size(kButtonWidth, kButtonHeight));
    button->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    SetButtonFont(*button, Font(1, gfx::Font::Weight::SEMIBOLD));
  }
  skip_ = content_->AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this), Action::kSkipTour),
      kSkip, 6));
  skip_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 10)));
  skip_->SetEnabledTextColors(SkColorSetA(kInk, 0xB0));
  SetButtonFont(*skip_, Font(0, gfx::Font::Weight::MEDIUM));
  for (views::View* button :
       std::initializer_list<views::View*>{import_, start_, skip_}) {
    button->SetPaintToLayer();
    button->layer()->SetFillsBoundsOpaquely(false);
  }

  // Before Play(): only the background shows.
  for (views::View* view : std::initializer_list<views::View*>{
           mark_, headline_blur_, headline_, import_, start_, skip_}) {
    view->layer()->SetOpacity(0);
  }
  if (!reduced_motion_) {
    mark_->layer()->SetTransform(MarkAt(-kMarkRiseBy, 0));
    const gfx::Transform left =
        gfx::Transform::MakeTranslation(-kHeadlineSlideBy, 0);
    headline_->layer()->SetTransform(left);
    headline_blur_->layer()->SetTransform(left);
  }
}

WelcomeView::~WelcomeView() = default;

void WelcomeView::Play(base::TimeTicks start) {
  if (playing_) {
    return;
  }
  playing_ = true;
  views::AnimationBuilder builder;
  builder.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  if (reduced_motion_) {
    // Nothing moves: the headline and the choice fade in together.
    views::AnimationSequenceBlock& fade =
        builder.Once().SetDuration(kReducedFade);
    for (views::View* view : std::initializer_list<views::View*>{
             headline_, import_, start_, skip_}) {
      fade.SetOpacity(view->layer(), 1, gfx::Tween::EASE_OUT);
    }
    return;
  }
  // welcome_timeline.h's times, from `start`.
  const base::TimeDelta elapsed = base::TimeTicks::Now() - start;
  const auto at = [elapsed](base::TimeDelta t) {
    return std::max(base::TimeDelta(), t - elapsed);
  };
  // Each At() ends the block before it, so `block` is always the newest.
  views::AnimationSequenceBlock* block = &builder.Once();
  ui::Layer* mark = mark_->layer();
  ui::Layer* headline = headline_->layer();
  ui::Layer* blurred = headline_blur_->layer();
  // The site's ease, leg by leg: the logo's rise and the headline's slide.
  for (int leg = 0; leg < kEaseLegs; ++leg) {
    const double from = static_cast<double>(leg) / kEaseLegs;
    const float eased = static_cast<float>(SiteEase((leg + 1.0) / kEaseLegs));
    block = &block->At(at(kMarkIn + kMarkRise * from))
                 .SetDuration(kMarkRise / kEaseLegs)
                 .SetOpacity(mark, eased)
                 .SetTransform(mark, MarkAt(-kMarkRiseBy * (1 - eased), 0));
    const gfx::Transform slide =
        gfx::Transform::MakeTranslation(-kHeadlineSlideBy * (1 - eased), 0);
    block = &block->At(at(kHeadlineAt + kHeadlineSlide * from))
                 .SetDuration(kHeadlineSlide / kEaseLegs)
                 .SetTransform(headline, slide)
                 .SetTransform(blurred, slide);
  }
  // One drift, up and turned, then back, fading as it returns.
  block = &block->At(at(kMarkIn + kMarkRise))
               .SetDuration(kMarkDrift / 2)
               .SetTransform(mark, MarkAt(kMarkDriftBy, kMarkTurn),
                             gfx::Tween::EASE_IN_OUT)
               .Then()
               .SetDuration(kMarkDrift / 2)
               .SetTransform(mark, MarkAt(0, 0), gfx::Tween::EASE_IN_OUT);
  block = &block->At(at(kMarkFadeAt))
               .SetDuration(kMarkFade)
               .SetOpacity(mark, 0, gfx::Tween::EASE_OUT);
  // The blur clears: the blurred copy comes in, then gives way to the sharp
  // one.
  block = &block->At(at(kHeadlineAt))
               .SetDuration(kBlurIn)
               .SetOpacity(blurred, 0.9f, gfx::Tween::EASE_OUT)
               .Then()
               .SetDuration(kHeadlineSlide - kBlurIn)
               .SetOpacity(blurred, 0, gfx::Tween::EASE_IN);
  block = &block->At(at(kHeadlineAt + kSharpIn))
               .SetDuration(kHeadlineSlide - kSharpIn)
               .SetOpacity(headline, 1, gfx::Tween::EASE_OUT);
  // The choice fades in as the headline lands. It only fades, so it never
  // moves while it can be pressed.
  block = &block->At(at(kChoiceAt)).SetDuration(kChoiceFade);
  for (views::View* view :
       std::initializer_list<views::View*>{import_, start_, skip_}) {
    block->SetOpacity(view->layer(), 1, gfx::Tween::EASE_OUT);
  }
  // The fields hold still until the window shows, so the blob's copy of
  // them hands off with no jump.
  drift_timer_.Start(
      FROM_HERE, at(kWindowShown),
      base::BindOnce(&WelcomeView::StartDrift, base::Unretained(this)));
}

void WelcomeView::StartDrift() {
  if (drifting_ || !playing_ || reduced_motion_ || width() <= 0 ||
      drift_timer_.IsRunning()) {
    return;
  }
  drifting_ = true;
  // A slow loop around where each field starts, easing at each turn.
  for (views::View* view : fields_) {
    const welcome::Field& spec = views::AsViewClass<Field>(view)->field();
    const float dx = spec.reach * width();
    const float dy = spec.reach * height();
    const std::array<gfx::Transform, 4> stops = {
        gfx::Transform::MakeTranslation(dx, dy / 2),
        gfx::Transform::MakeTranslation(0, dy),
        gfx::Transform::MakeTranslation(-dx, dy / 2),
        gfx::Transform(),
    };
    const base::TimeDelta leg = base::Seconds(spec.period / stops.size());
    views::AnimationBuilder builder;
    builder.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    views::AnimationSequenceBlock* block = &builder.Repeatedly();
    bool first = true;
    for (const gfx::Transform& stop : stops) {
      if (!std::exchange(first, false)) {
        block = &block->Then();
      }
      block->SetDuration(leg).SetTransform(view->layer(), stop,
                                           gfx::Tween::EASE_IN_OUT);
    }
  }
}

void WelcomeView::FinishIntro() {
  playing_ = true;
  for (views::View* view : std::initializer_list<views::View*>{
           mark_, headline_blur_, headline_, import_, start_, skip_}) {
    view->layer()->GetAnimator()->StopAnimating();
  }
  mark_->layer()->SetOpacity(0);
  headline_blur_->layer()->SetOpacity(0);
  headline_blur_->layer()->SetTransform(gfx::Transform());
  headline_->layer()->SetTransform(gfx::Transform());
  for (views::View* view :
       std::initializer_list<views::View*>{headline_, import_, start_, skip_}) {
    view->layer()->SetOpacity(1);
  }
  if (drift_timer_.IsRunning()) {
    drift_timer_.Stop();
  }
  StartDrift();
}

void WelcomeView::CrossfadeTo(SidebarTint tint, base::OnceClosure done) {
  FinishIntro();
  import_->SetText(kImported);
  import_->SetEnabled(false);
  views::AsViewClass<Theme>(theme_.get())->SetTint(tint);
  const base::TimeDelta fade = reduced_motion_ ? kReducedFade : kThemeFade;
  // The fields give way to the theme.
  views::AnimationBuilder builder;
  builder.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  views::AnimationSequenceBlock& block = builder.Once().SetDuration(fade);
  block.SetOpacity(theme_->layer(), 1, gfx::Tween::EASE_OUT);
  for (views::View* field : fields_) {
    block.SetOpacity(field->layer(), 0, gfx::Tween::EASE_OUT);
  }
  theme_timer_.Start(FROM_HERE, fade, std::move(done));
}

base::TimeDelta WelcomeView::Exit() {
  if (exiting_) {
    return base::TimeDelta();
  }
  FinishIntro();
  theme_timer_.Stop();
  exiting_ = true;
  SetCanProcessEventsWithinSubtree(false);
  // Read again: Reduce Motion may have changed since the welcome showed.
  const bool reduced = gfx::Animation::PrefersReducedMotion();
  const base::TimeDelta duration = reduced ? kReducedFade : kExit;
  views::AnimationBuilder builder;
  builder.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  views::AnimationSequenceBlock& block =
      builder.Once().SetDuration(duration).SetOpacity(layer(), 0,
                                                      gfx::Tween::EASE_OUT);
  if (!reduced) {
    block.SetTransform(content_->layer(),
                       gfx::Transform::MakeTranslation(kExitSlide, 0),
                       gfx::Tween::EASE_OUT);
  }
  return duration;
}

void WelcomeView::Layout(PassKey) {
  const int w = width();
  const int h = height();
  const float longer = std::max(w, h);
  for (views::View* view : fields_) {
    const welcome::Field& spec = views::AsViewClass<Field>(view)->field();
    const int radius = static_cast<int>(spec.radius * longer);
    view->SetBounds(static_cast<int>(spec.x * w) - radius,
                    static_cast<int>(spec.y * h) - radius, 2 * radius,
                    2 * radius);
  }
  theme_->SetBoundsRect(GetLocalBounds());
  content_->SetBoundsRect(GetLocalBounds());
  mark_->SetBounds((w - kMarkSize) / 2, (h - kMarkSize) / 2, kMarkSize,
                   kMarkSize);

  // The headline and the choice under it, centred together.
  const gfx::Size headline = headline_->GetPreferredSize();
  const gfx::Size skip = skip_->GetPreferredSize();
  const int column = headline.height() + 28 + kButtonHeight + 10 +
                     kButtonHeight + 14 + skip.height();
  int y = std::max(0, (h - column) / 2);
  for (views::View* view :
       std::initializer_list<views::View*>{headline_, headline_blur_}) {
    view->SetBounds((w - headline.width()) / 2, y, headline.width(),
                    headline.height());
  }
  y += headline.height() + 28;
  import_->SetBounds((w - kButtonWidth) / 2, y, kButtonWidth, kButtonHeight);
  y += kButtonHeight + 10;
  start_->SetBounds((w - kButtonWidth) / 2, y, kButtonWidth, kButtonHeight);
  y += kButtonHeight + 14;
  skip_->SetBounds((w - skip.width()) / 2, y, skip.width(), skip.height());
  StartDrift();
}

void WelcomeView::TakeFocus() {
  SetPaneFocusAndFocusDefault();
  GetViewAccessibility().AnnounceText(std::u16string(kHeadline));
}

uint32_t WelcomeView::BaseColor() const {
  return IsDark(*this) ? kMidnightDark : welcome::kMidnight;
}

bool WelcomeView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE && EscSkipsTour(*this)) {
    Press(Action::kSkipTour);
    return true;
  }
  return views::AccessiblePaneView::AcceleratorPressed(accelerator);
}

views::View* WelcomeView::GetDefaultFocusableChild() {
  // Return picks the primary button.
  return import_->GetEnabled() ? import_.get() : start_.get();
}

void WelcomeView::OnPaint(gfx::Canvas* canvas) {
  canvas->DrawColor(Rgb(BaseColor()));
}

void WelcomeView::Press(Action action) {
  if (exiting_) {
    return;
  }
  FinishIntro();
  // A copy: the action may close the welcome and delete this view.
  auto run = action_;
  run.Run(action);
}

BEGIN_METADATA(WelcomeView)
END_METADATA

}  // namespace myarc
