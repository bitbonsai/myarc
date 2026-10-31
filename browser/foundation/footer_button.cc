#include "myarc/footer_button.h"

#include <algorithm>
#include <array>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/utils/SkParsePath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/scoped_canvas.h"

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

// Box size matches the old 16 DIP Library glyph: 24 units wide at 0.4 DIP.
constexpr float kArtScale = 0.4f;
constexpr float kBoxCenterX = 31;
constexpr float kBoxBottom = 53;
// Box bottom below the button's center, as the old glyph's.
constexpr float kBoxBottomOffset = 6.33f;
// Sparkles show only above the box's top edge.
constexpr SkRect kSparkleClip = SkRect::MakeLTRB(0, 0, 60, 32);
constexpr float kBeamOriginX = 30;
constexpr float kBeamOriginY = 36;
constexpr float kBeamTop = 9;
constexpr float kBeamBottom = 36;
constexpr float kBeamBlur = 3;
constexpr float kBeamIdleScale = 0.4f;
constexpr float kSparkleBlur = 0.5f;
constexpr float kIdleAlpha = 0.55f;
constexpr float kGlowAlpha = 0.95f;

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

}  // namespace

LibraryButton::~LibraryButton() = default;

void LibraryButton::SetOpen(bool open, bool animate) {
  open_ = open;
  UpdateGlow(animate);
}

void LibraryButton::SetHovered(bool hovered, bool animate) {
  hovered_ = hovered;
  UpdateGlow(animate);
}

void LibraryButton::UpdateGlow(bool animate) {
  const bool glowing = open_ || hovered_;
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

void LibraryButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation != &animation_) {
    views::MdTextButton::AnimationProgressed(animation);
    return;
  }
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

void LibraryButton::OnMouseEntered(const ui::MouseEvent& event) {
  views::MdTextButton::OnMouseEntered(event);
  SetHovered(true, true);
}

void LibraryButton::OnMouseExited(const ui::MouseEvent& event) {
  views::MdTextButton::OnMouseExited(event);
  SetHovered(false, true);
}

void LibraryButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::MdTextButton::OnBoundsChanged(previous_bounds);
  // The sidebar rebuilds its footer on every workspace change. A replacement
  // under the pointer keeps the glow instead of replaying the morph.
  if (!hovered_ && IsMouseHovered()) {
    SetHovered(true, false);
  }
}

void LibraryButton::PaintButtonContents(gfx::Canvas* canvas) {
  const Paths& paths = GetPaths();
  const SkColor foreground =
      GetColorProvider()->GetColor(ui::kColorSysOnSurface);
  SkColor4f color = SkColor4f::FromColor(foreground);
  color.fA = Lerp(kIdleAlpha, kGlowAlpha, now_[kColor]);

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
