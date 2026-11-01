#include "myarc/footer_button.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/utils/SkParsePath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

// Paths from design/icons/library-morph.html, in its 60x53 viewBox. Each
// sparkle is centered on its own origin; the beam and box sit in place.
constexpr char kBeamPath[] =
    "M11 27C11 27 0.052 2.287 0 2C-0.051 1.713 13.442 4.312 13.442 4.312"
    "C13.442 4.312 17.552 9.006 17.256 9.45C16.96 9.894 24.54 2.604 24.884 "
    "2.256C25.056 2.083 27.174 -0.001 29 0C30.826 0.001 32.361 2.088 32.512 "
    "2.256C32.814 2.593 35.372 6.715 35.372 6.715L42 3C42 3 39.828 6.777 39 10"
    "C38.172 13.223 39.69 18.233 39.612 18.053C39.457 17.692 31.89 26.741 "
    "32.243 26.92C32.595 27.1 11 27 11 27Z";
constexpr char kStarPath[] =
    "M9.006 0.916C9.522 -0.305 11.253 -0.305 11.769 0.916L13.56 5.157C13.777 "
    "5.67 14.259 6.021 14.814 6.068L19.401 6.461C20.722 6.574 21.257 8.221 "
    "20.255 9.089L16.775 12.103C16.354 12.468 16.17 13.035 16.296 13.577"
    "L17.34 18.061C17.64 19.353 16.24 20.37 15.104 19.685L11.162 17.307"
    "C10.686 17.019 10.089 17.019 9.613 17.307L5.671 19.685C4.535 20.37 3.134 "
    "19.353 3.435 18.061L4.479 13.577C4.605 13.035 4.421 12.468 4 12.103"
    "L0.52 9.089C-0.482 8.221 0.053 6.574 1.374 6.461L5.961 6.068C6.515 6.021 "
    "6.998 5.67 7.215 5.157Z";
constexpr char kTrianglePath[] =
    "M2.16 0.485C2.548 -0.162 3.485 -0.162 3.874 0.485L5.889 3.835C6.29 4.501 "
    "5.81 5.35 5.032 5.35L1.002 5.35C0.224 5.35 -0.256 4.501 0.145 3.835Z";
constexpr char kBoxPath[] =
    "M0 4C0 0.375 0 0 4 0L6 0L20 0C24 0 24 0 24 4L24 6L24 12C24 15.314 "
    "21.314 18 18 18L6 18C2.686 18 0 15.314 0 12L0 4ZM9.25 2C8.56 2 8 2.56 8 "
    "3.25L8 4.75C8 5.44 8.56 6 9.25 6L14.75 6C15.44 6 16 5.44 16 4.75L16 3.25"
    "C16 2.56 15.44 2 14.75 2L9.25 2Z";

struct Paths {
  SkPath beam;
  SkPath star;
  SkPath dot;
  SkPath triangle;
  SkPath box;
};

SkPath Parse(const char* svg, float dx, float dy) {
  SkPath path;
  CHECK(SkParsePath::FromSVGString(svg, &path));
  return path.makeOffset(dx, dy);
}

const Paths& GetPaths() {
  static const base::NoDestructor<Paths> paths(Paths{
      Parse(kBeamPath, 9, 9), Parse(kStarPath, -10.39f, -10.2f),
      SkPath::Circle(0, 0, 2), Parse(kTrianglePath, -3.02f, -3),
      Parse(kBoxPath, 19, 35).makeFillType(SkPathFillType::kEvenOdd)});
  return *paths;
}

// A sparkle's CSS transform: translate, rotate (degrees), scale.
struct Pose {
  float x;
  float y;
  float rotate;
  float scale;
};

struct Sparkle {
  Pose idle;
  Pose glow;
};

constexpr Sparkle kStarMotion = {{26.27f, 30.84f, -7.5f, 0.857f},
                           {26.13f, 27.18f, -30.4f, 1}};
constexpr Sparkle kDotMotion = {{35, 22, 0, 1}, {37, 17, 0, 1}};
constexpr Sparkle kTriangleMotion = {{40.81f, 29.74f, -27.3f, 1},
                               {42.69f, 26.5f, -27.3f, 1}};

// Arc's box is 12 DIP wide, so its 24 units draw at 0.5 DIP. In the 32 DIP
// tile its edges land on whole DIPs: 10 and 22 across, 15 and 24 down.
constexpr float kArtScale = 0.5f;
constexpr float kBoxCenterX = 31;
constexpr float kBoxBottom = 53;
// Box bottom below the button's center.
constexpr float kBoxBottomOffset = 8;
// Sparkles show only above the box's top edge.
constexpr SkRect kSparkleClip = SkRect::MakeLTRB(0, 0, 60, 32);
constexpr float kBeamOriginX = 30;
constexpr float kBeamOriginY = 36;
constexpr float kBeamTop = 9;
constexpr float kBeamBottom = 36;
constexpr float kBeamBlur = 3;
constexpr float kBeamIdleScale = 0.4f;
constexpr float kSparkleBlur = 0.5f;

constexpr base::TimeDelta kSparkleDuration = base::Milliseconds(450);
constexpr base::TimeDelta kFadeDuration = base::Milliseconds(350);
constexpr base::TimeDelta kLastDelay = base::Milliseconds(120);
constexpr base::TimeDelta kTotal = kSparkleDuration + kLastDelay;

float Lerp(float from, float to, float t) {
  return from + (to - from) * t;
}

Pose Lerp(const Sparkle& sparkle, float t) {
  return {Lerp(sparkle.idle.x, sparkle.glow.x, t),
          Lerp(sparkle.idle.y, sparkle.glow.y, t),
          Lerp(sparkle.idle.rotate, sparkle.glow.rotate, t),
          Lerp(sparkle.idle.scale, sparkle.glow.scale, t)};
}

// Footer tooltip, measured from Arc's (DIP): a 28 tall pill 4 above the button,
// 10 before the label, then 18 DIP keycaps 4 apart with a 2 DIP lip, 5 from
// the pill's edge.
constexpr base::TimeDelta kTooltipDelay = base::Seconds(1);
// Arc's Library tooltip shows 0.65-0.8 s after hover, right of the button.
constexpr base::TimeDelta kBesideDelay = base::Milliseconds(700);
constexpr int kTooltipHeight = 28;
constexpr int kTooltipGap = 4;
constexpr int kTooltipMargin = 6;
constexpr int kTooltipInset = 10;
constexpr int kKeysGap = 10;
constexpr int kKeySize = 18;
constexpr int kKeyGap = 4;
constexpr int kKeyLip = 2;
constexpr float kKeyRadius = 5;
constexpr int kKeysInset = (kTooltipHeight - kKeySize) / 2;
constexpr int kLabelSize = 13;
constexpr int kKeyLabelSize = 11;

struct TooltipColors {
  SkColor fill;
  SkColor border;
  SkColor label;
  SkColor key;
  SkColor key_lip;
  SkColor key_label;
};

// Sampled from Arc's dark tooltip. Light mode keeps the keycaps on a white
// pill with dark text.
constexpr TooltipColors kDarkTooltip = {
    SkColorSetRGB(0x0A, 0x0C, 0x4B), SkColorSetA(SK_ColorWHITE, 0x1A),
    SkColorSetRGB(0xDE, 0xDF, 0xFE), SkColorSetRGB(0xD3, 0xD5, 0xFD),
    SkColorSetRGB(0x83, 0x87, 0xE4), SkColorSetRGB(0x21, 0x2A, 0xFC)};
constexpr TooltipColors kLightTooltip = {
    SK_ColorWHITE,                   SkColorSetA(SK_ColorBLACK, 0x1F),
    SkColorSetRGB(0x1D, 0x1B, 0x26), SkColorSetRGB(0xE4, 0xE5, 0xFE),
    SkColorSetRGB(0xB0, 0xB4, 0xF4), SkColorSetRGB(0x21, 0x2A, 0xFC)};

gfx::FontList FontOfSize(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.Derive(size - base.GetFontSize(), gfx::Font::NORMAL, weight);
}

class TooltipView : public views::View {
  METADATA_HEADER(TooltipView, views::View)

 public:
  TooltipView(const FooterTooltip::Content& content,
              const TooltipColors& colors)
      : content_(content),
        colors_(colors),
        label_font_(FontOfSize(kLabelSize, gfx::Font::Weight::MEDIUM)),
        key_font_(FontOfSize(kKeyLabelSize, gfx::Font::Weight::BOLD)) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kTooltip);
    GetViewAccessibility().SetName(content_.label);
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    int width = kTooltipInset + LabelWidth();
    if (content_.keys.empty()) {
      width += kTooltipInset;
    } else {
      width += kKeysGap + KeysWidth() + kKeysInset;
    }
    return gfx::Size(width, kTooltipHeight);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::RectF bounds(GetLocalBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(colors_.fill);
    canvas->DrawRoundRect(bounds, bounds.height() / 2, flags);
    gfx::RectF edge = bounds;
    edge.Inset(0.5f);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(colors_.border);
    canvas->DrawRoundRect(edge, edge.height() / 2, flags);

    constexpr int kTextFlags = gfx::Canvas::NO_SUBPIXEL_RENDERING;
    canvas->DrawStringRectWithFlags(
        content_.label, label_font_, colors_.label,
        gfx::Rect(kTooltipInset, 0, LabelWidth(), height()), kTextFlags);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    int x = kTooltipInset + LabelWidth() + kKeysGap;
    for (const auto& key : content_.keys) {
      const gfx::RectF cap(x, kKeysInset, kKeySize, kKeySize);
      flags.setColor(colors_.key_lip);
      canvas->DrawRoundRect(cap, kKeyRadius, flags);
      gfx::RectF face = cap;
      face.set_height(kKeySize - kKeyLip);
      flags.setColor(colors_.key);
      canvas->DrawRoundRect(face, kKeyRadius, flags);
      canvas->DrawStringRectWithFlags(
          key, key_font_, colors_.key_label, gfx::ToEnclosingRect(face),
          kTextFlags | gfx::Canvas::TEXT_ALIGN_CENTER);
      x += kKeySize + kKeyGap;
    }
  }

 private:
  int LabelWidth() const {
    return gfx::GetStringWidth(content_.label, label_font_);
  }

  int KeysWidth() const {
    const int count = static_cast<int>(content_.keys.size());
    return count * kKeySize + (count - 1) * kKeyGap;
  }

  const FooterTooltip::Content content_;
  const TooltipColors colors_;
  const gfx::FontList label_font_;
  const gfx::FontList key_font_;
};

BEGIN_METADATA(TooltipView)
END_METADATA

}  // namespace

SkColor GetFooterIconColor(const ui::ColorProvider* provider, bool active) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  return SkColorSetA(dark ? SK_ColorWHITE : SK_ColorBLACK,
                     active ? 0xF2 : 0x99);
}

FooterTooltip::FooterTooltip() = default;

FooterTooltip::~FooterTooltip() = default;

void FooterTooltip::Hover(views::View& anchor, const Content& content) {
  if (hold_) {
    return;
  }
  if (pressed_at_ && anchor.GetBoundsInScreen().Contains(*pressed_at_)) {
    return;
  }
  pressed_at_.reset();
  Hide();
  views::Widget* window = anchor.GetWidget();
  if (!window) {
    return;
  }
  anchor_ = &anchor;
  content_ = content;
  anchor_observation_.Observe(&anchor);
  window_observation_.Observe(window);
  key_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, window->GetNativeWindow(), {ui::EventType::kKeyPressed});
  delay_.Start(FROM_HERE, content.beside ? kBesideDelay : kTooltipDelay,
               base::BindOnce(&FooterTooltip::Show, base::Unretained(this)));
}

void FooterTooltip::Unhover(const views::View& anchor) {
  pressed_at_.reset();
  if (anchor_ == &anchor) {
    HideUnlessHeld();
  }
}

void FooterTooltip::Press(const gfx::Point& screen_point) {
  Hide();
  pressed_at_ = screen_point;
}

void FooterTooltip::ShowNow(views::View& anchor, const Content& content) {
  Hide();
  anchor_ = &anchor;
  content_ = content;
  anchor_observation_.Observe(&anchor);
  hold_ = true;
  Show();
}

void FooterTooltip::Hide() {
  delay_.Stop();
  key_monitor_.reset();
  window_observation_.Reset();
  anchor_observation_.Reset();
  anchor_ = nullptr;
  hold_ = false;
  if (widget_) {
    widget_->Hide();
  }
}

void FooterTooltip::HideUnlessHeld() {
  if (!hold_) {
    Hide();
  }
}

void FooterTooltip::OnViewVisibilityChanged(views::View* observed_view,
                                            views::View* starting_view,
                                            bool visible) {
  if (!visible) {
    HideUnlessHeld();
  }
}

void FooterTooltip::OnViewBoundsChanged(views::View* observed_view) {
  HideUnlessHeld();
}

void FooterTooltip::OnViewRemovedFromWidget(views::View* observed_view) {
  Hide();
}

void FooterTooltip::OnViewIsDeleting(views::View* observed_view) {
  Hide();
}

void FooterTooltip::OnWidgetActivationChanged(views::Widget* widget,
                                              bool active) {
  if (!active) {
    HideUnlessHeld();
  }
}

void FooterTooltip::OnWidgetDestroying(views::Widget* widget) {
  Hide();
}

void FooterTooltip::OnEvent(const ui::Event& event) {
  HideUnlessHeld();
}

void FooterTooltip::Show() {
  if (!anchor_ || !anchor_->GetWidget() || content_.label.empty()) {
    return;
  }
  views::Widget* parent = anchor_->GetWidget();
  // Captures run in a background window.
  if (!hold_ && !parent->IsActive()) {
    Hide();
    return;
  }
  if (!widget_) {
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.SetParent(parent);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = false;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    params.name = "MyarcFooterTooltip";
    widget_ = std::make_unique<views::Widget>(std::move(params));
  }
  const ui::ColorProvider* provider = anchor_->GetColorProvider();
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  auto* contents = widget_->SetContentsView(
      std::make_unique<TooltipView>(content_, dark ? kDarkTooltip
                                                   : kLightTooltip));
  const gfx::Size size = contents->GetPreferredSize();
  const gfx::Rect anchor = anchor_->GetBoundsInScreen();
  if (content_.beside) {
    // Right of the anchor, or left of it when the screen has no room.
    const gfx::Rect screen =
        display::Screen::Get()->GetDisplayMatching(anchor).work_area();
    int x = anchor.right() + kTooltipGap;
    if (x + size.width() > screen.right() - kTooltipMargin) {
      x = anchor.x() - kTooltipGap - size.width();
    }
    x = std::clamp(x, screen.x() + kTooltipMargin,
                   std::max(screen.x() + kTooltipMargin,
                            screen.right() - kTooltipMargin - size.width()));
    const int y = std::clamp(
        anchor.CenterPoint().y() - size.height() / 2,
        screen.y() + kTooltipMargin,
        std::max(screen.y() + kTooltipMargin,
                 screen.bottom() - kTooltipMargin - size.height()));
    widget_->SetBounds(gfx::Rect(gfx::Point(x, y), size));
    widget_->ShowInactive();
    return;
  }
  const gfx::Rect window = parent->GetClientAreaBoundsInScreen();
  const int x = std::clamp(anchor.CenterPoint().x() - size.width() / 2,
                           window.x() + kTooltipMargin,
                           std::max(window.x() + kTooltipMargin,
                                    window.right() - kTooltipMargin -
                                        size.width()));
  widget_->SetBounds(gfx::Rect(gfx::Point(x, anchor.y() - kTooltipGap -
                                                 size.height()),
                               size));
  widget_->ShowInactive();
}

FooterButton::FooterButton(PressedCallback callback,
                           std::u16string_view text,
                           int button_context,
                           bool use_text_color_for_icon)
    : views::MdTextButton(std::move(callback),
                          text,
                          button_context,
                          use_text_color_for_icon) {}

FooterButton::~FooterButton() = default;

void FooterButton::SetFooterTooltip(base::WeakPtr<FooterTooltip> tooltip,
                                    FooterTooltip::Content content) {
  tooltip_ = std::move(tooltip);
  content_ = std::move(content);
  // The keycaps as ARIA key shortcuts, for example "Control+1".
  std::string shortcuts;
  for (const auto& key : content_.keys) {
    if (!shortcuts.empty()) {
      shortcuts += '+';
    }
    shortcuts += key == u"\u2303" ? "Control" : base::UTF16ToUTF8(key);
  }
  if (shortcuts.empty()) {
    GetViewAccessibility().RemoveKeyShortcuts();
  } else {
    GetViewAccessibility().SetKeyShortcuts(shortcuts);
  }
}

void FooterButton::ShowFooterTooltipNow() {
  if (tooltip_) {
    tooltip_->ShowNow(*this, content_);
  }
}

void FooterButton::SetHovered(bool hovered) {
  if (hovered == hovered_) {
    return;
  }
  hovered_ = hovered;
  if (tooltip_) {
    if (hovered_) {
      tooltip_->Hover(*this, content_);
    } else {
      tooltip_->Unhover(*this);
    }
  }
  OnHoverChanged();
}

void FooterButton::OnMouseEntered(const ui::MouseEvent& event) {
  views::MdTextButton::OnMouseEntered(event);
  SetHovered(true);
}

void FooterButton::OnMouseExited(const ui::MouseEvent& event) {
  views::MdTextButton::OnMouseExited(event);
  SetHovered(false);
}

bool FooterButton::OnMousePressed(const ui::MouseEvent& event) {
  if (tooltip_) {
    gfx::Point point = event.location();
    views::View::ConvertPointToScreen(this, &point);
    tooltip_->Press(point);
  }
  return views::MdTextButton::OnMousePressed(event);
}

BEGIN_METADATA(FooterButton)
END_METADATA

LibraryButton::LibraryButton(PressedCallback callback,
                             std::u16string_view text,
                             int button_context,
                             bool use_text_color_for_icon)
    : FooterButton(std::move(callback),
                   text,
                   button_context,
                   use_text_color_for_icon) {}

LibraryButton::~LibraryButton() = default;

LibraryButton::MorphDelegate::MorphDelegate(LibraryButton* button)
    : views::AnimationDelegateViews(button), button_(button) {}

void LibraryButton::MorphDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  button_->MorphProgressed();
}

void LibraryButton::SetOpen(bool open, bool animate) {
  open_ = open;
  UpdateGlow(animate);
}

void LibraryButton::SettleFromGlow() {
  animation_.Stop();
  glowing_ = true;
  now_.fill(1);
  SchedulePaint();
  UpdateGlow(/*animate=*/true);
}

void LibraryButton::OnHoverChanged() {
  UpdateGlow(/*animate=*/true);
  if (hover_changed_) {
    hover_changed_.Run(hovered());
  }
}

void LibraryButton::UpdateGlow(bool animate) {
  const bool glowing = open_ || hovered();
  if (glowing == glowing_) {
    return;
  }
  glowing_ = glowing;
  animation_.Stop();
  if (!animate || gfx::Animation::PrefersReducedMotion()) {
    now_.fill(glowing_ ? 1 : 0);
    SchedulePaint();
    return;
  }
  from_ = now_;
  animation_.SetDuration(kTotal);
  animation_.Start();
}

void LibraryButton::MorphProgressed() {
  // CSS easings from the prototype: ease, an overshoot for the sparkles and an
  // ease-out for the beam's growth.
  static const gfx::CubicBezier ease(0.25, 0.1, 0.25, 1);
  static const gfx::CubicBezier overshoot(0.34, 1.56, 0.64, 1);
  static const gfx::CubicBezier rise(0.22, 1, 0.36, 1);
  // Per channel, in Channel order. Delays apply only while rising, so the
  // sparkles settle together.
  const std::array<const gfx::CubicBezier*, kChannelCount> curves = {
      &ease, &ease, &rise, &ease, &overshoot, &overshoot, &overshoot};
  constexpr std::array<base::TimeDelta, kChannelCount> kDurations = {
      kFadeDuration,     kFadeDuration,     kSparkleDuration, kSparkleDuration,
      kSparkleDuration, kSparkleDuration, kSparkleDuration};
  constexpr std::array<base::TimeDelta, kChannelCount> kDelays = {
      base::TimeDelta(),      base::TimeDelta(),      base::TimeDelta(),
      base::TimeDelta(),      base::Milliseconds(30), base::Milliseconds(80),
      kLastDelay};
  const base::TimeDelta elapsed = kTotal * animation_.GetCurrentValue();
  const float target = glowing_ ? 1 : 0;
  for (size_t i = 0; i < kChannelCount; ++i) {
    const base::TimeDelta delay = glowing_ ? kDelays[i] : base::TimeDelta();
    const double t =
        std::clamp((elapsed - delay) / kDurations[i], 0.0, 1.0);
    now_[i] = Lerp(from_[i], target, curves[i]->Solve(t));
  }
  SchedulePaint();
}

void LibraryButton::PaintButtonContents(gfx::Canvas* canvas) {
  const Paths& paths = GetPaths();
  // The same color as the other footer icons, brighter while lit.
  const SkColor4f idle =
      SkColor4f::FromColor(GetFooterIconColor(GetColorProvider(), false));
  const SkColor4f glow =
      SkColor4f::FromColor(GetFooterIconColor(GetColorProvider(), true));
  const float t = now_[kColor];
  const SkColor4f color = {Lerp(idle.fR, glow.fR, t), Lerp(idle.fG, glow.fG, t),
                           Lerp(idle.fB, glow.fB, t),
                           Lerp(idle.fA, glow.fA, t)};

  gfx::ScopedCanvas scoped(canvas);
  cc::PaintCanvas* paint = canvas->sk_canvas();
  paint->translate(width() / 2.f - kBoxCenterX * kArtScale,
                   height() / 2.f + kBoxBottomOffset - kBoxBottom * kArtScale);
  paint->scale(kArtScale);

  const float beam_opacity = std::clamp(now_[kBeamOpacity], 0.f, 1.f);
  if (beam_opacity > 0) {
    paint->save();
    paint->translate(kBeamOriginX, kBeamOriginY);
    paint->scale(1, Lerp(kBeamIdleScale, 1, now_[kBeamScale]));
    paint->translate(-kBeamOriginX, -kBeamOriginY);
    const SkPoint points[] = {{0, kBeamTop}, {0, kBeamBottom}};
    SkColor4f clear = color;
    clear.fA = 0;
    SkColor4f half = color;
    half.fA *= 0.5f;
    const SkColor4f colors[] = {clear, half};
    cc::PaintFlags beam;
    beam.setAntiAlias(true);
    beam.setShader(cc::PaintShader::MakeLinearGradient(
        points, colors, nullptr, 2, SkTileMode::kClamp));
    beam.setImageFilter(sk_make_sp<cc::BlurPaintFilter>(
        kBeamBlur, kBeamBlur, SkTileMode::kDecal, nullptr));
    beam.setAlphaf(beam_opacity);
    paint->drawPath(paths.beam, beam);
    paint->restore();
  }

  cc::PaintFlags fill;
  fill.setAntiAlias(true);
  fill.setColor(color);
  cc::PaintFlags sparkle = fill;
  const float blur = kSparkleBlur * std::max(now_[kBlur], 0.f);
  if (blur > 0.01f) {
    sparkle.setImageFilter(sk_make_sp<cc::BlurPaintFilter>(
        blur, blur, SkTileMode::kDecal, nullptr));
  }
  paint->save();
  paint->clipRect(kSparkleClip, true);
  const std::array shapes = {&paths.star, &paths.dot, &paths.triangle};
  const std::array motions = {&kStarMotion, &kDotMotion, &kTriangleMotion};
  constexpr std::array kChannels = {kStar, kDot, kTriangle};
  for (size_t i = 0; i < std::size(kChannels); ++i) {
    const Pose pose = Lerp(*motions[i], now_[kChannels[i]]);
    paint->save();
    paint->translate(pose.x, pose.y);
    paint->rotate(pose.rotate);
    paint->scale(pose.scale);
    paint->drawPath(*shapes[i], sparkle);
    paint->restore();
  }
  paint->restore();
  paint->drawPath(paths.box, fill);
}

BEGIN_METADATA(LibraryButton)
END_METADATA

}  // namespace myarc
