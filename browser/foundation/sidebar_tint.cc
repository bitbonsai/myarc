#include "myarc/sidebar_tint.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/private/chromium/SkPMColor.h"
#include "ui/base/class_property.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

struct SidebarThemeState {
  std::optional<SpaceTheme> theme;
  base::RepeatingCallback<void(bool)> observer;
};

}  // namespace
}  // namespace myarc

DEFINE_UI_CLASS_PROPERTY_TYPE(myarc::SidebarThemeState*)

namespace myarc {
namespace {

// Kept on the root view, which lives as long as its widget.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(SidebarThemeState, kSidebarThemeStateKey)

SidebarThemeState* GetState(const views::Widget* widget) {
  const views::View* root = widget ? widget->GetRootView() : nullptr;
  return root ? root->GetProperty(kSidebarThemeStateKey) : nullptr;
}

SidebarThemeState* EnsureState(views::Widget& widget) {
  views::View* root = widget.GetRootView();
  if (!root) {
    return nullptr;
  }
  if (!root->GetProperty(kSidebarThemeStateKey)) {
    root->SetProperty(kSidebarThemeStateKey,
                      std::make_unique<SidebarThemeState>());
  }
  return root->GetProperty(kSidebarThemeStateKey);
}

}  // namespace

SidebarTint GetSidebarTint(bool dark,
                           const std::optional<SpaceTheme>& theme,
                           bool solid) {
  if (!theme) {
    // Dark: Dusk, Arc's deep indigo (#151228) at the top-leading corner to
    // a dark plum (#261229) at the bottom-trailing one. Over the glass the
    // stops are darker and about 85% opaque, so the blurred desktop lifts
    // them to those colours; solid draws them as they are.
    if (dark) {
      return solid ? SidebarTint{SkColorSetARGB(242, 0x15, 0x12, 0x28),
                                 SkColorSetARGB(242, 0x26, 0x12, 0x29)}
                   : SidebarTint{SkColorSetARGB(217, 0x0F, 0x0D, 0x26),
                                 SkColorSetARGB(217, 0x23, 0x0D, 0x27)};
    }
    return SidebarTint{SkColorSetARGB(166, 227, 224, 242),
                       SkColorSetARGB(166, 240, 230, 240)};
  }
  const SkColor color = SkColorSetA(theme->color, SK_AlphaOPAQUE);
  // At 100% the sidebar is two thirds tint over the glass, or nearly opaque
  // when solid; at 0% the material shows plain.
  const SkAlpha alpha = static_cast<SkAlpha>(
      (solid ? 230 : 170) * std::clamp(theme->opacity, 0, 100) / 100);
  // White labels need a deep shade, black ones a pale tint. The end stop is a
  // step stronger, like the default gradient.
  const SkColor base = dark ? SK_ColorBLACK : SK_ColorWHITE;
  const SkColor start = color_utils::AlphaBlend(color, base, 0.40f);
  const SkColor end =
      color_utils::AlphaBlend(color, base, dark ? 0.55f : 0.60f);
  return {SkColorSetA(start, alpha), SkColorSetA(end, alpha)};
}

SkColor GetSidebarPeekColor(const ui::ColorProvider& provider,
                            const std::optional<SpaceTheme>& theme) {
  const bool dark =
      color_utils::IsDark(provider.GetColor(ui::kColorSysSurface));
  const SidebarTint tint = GetSidebarTint(dark, theme);
  // The sidebar covers about the first quarter of the gradient.
  const SkColor sidebar = color_utils::AlphaBlend(tint.end, tint.start, 0.25f);
  return color_utils::GetResultingPaintColor(
      sidebar, provider.GetColor(ui::kColorFrameActive));
}

void SetSidebarTheme(views::Widget& widget,
                     std::optional<SpaceTheme> theme,
                     bool fade) {
  SidebarThemeState* state = EnsureState(widget);
  if (!state || state->theme == theme) {
    return;
  }
  state->theme = theme;
  if (state->observer) {
    state->observer.Run(fade);
  }
}

std::optional<SpaceTheme> GetSidebarTheme(const views::Widget* widget) {
  const SidebarThemeState* state = GetState(widget);
  return state ? state->theme : std::nullopt;
}

void SetSidebarThemeObserver(
    views::Widget& widget,
    base::RepeatingCallback<void(bool fade)> observer) {
  if (SidebarThemeState* state = EnsureState(widget)) {
    state->observer = std::move(observer);
  }
}

namespace {

// A dark icon on a dark sidebar needs inversion only when almost none of its
// visible pixels clear a 2.5:1 contrast ratio against the sidebar fill; badges
// with light glyphs inside a dark square already have readable pixels.
bool NeedsLightnessInvert(const SkBitmap& bitmap, SkColor background) {
  if (bitmap.drawsNothing() || bitmap.width() <= 0 || bitmap.height() <= 0) {
    return false;
  }
  double visible_alpha = 0.0;
  double readable_alpha = 0.0;
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      const SkColor color = bitmap.getColor(x, y);
      const SkAlpha alpha = SkColorGetA(color);
      if (alpha < 32) {
        continue;
      }
      const double weight = static_cast<double>(alpha) / 255.0;
      visible_alpha += weight;
      if (color_utils::GetContrastRatio(SkColorSetA(color, SK_AlphaOPAQUE),
                                        background) >= 2.5f) {
        readable_alpha += weight;
      }
    }
  }
  return visible_alpha >= 1.0 && (readable_alpha / visible_alpha) < 0.08;
}

SkBitmap InvertBitmapLightness(const SkBitmap& source) {
  SkBitmap out;
  out.allocN32Pixels(source.width(), source.height());
  out.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 0; y < source.height(); ++y) {
    for (int x = 0; x < source.width(); ++x) {
      const SkColor color = source.getColor(x, y);
      const SkAlpha alpha = SkColorGetA(color);
      if (alpha == 0) {
        continue;
      }
      color_utils::HSL hsl;
      color_utils::SkColorToHSL(color, &hsl);
      hsl.l = 1.0 - hsl.l;
      *out.getAddr32(x, y) =
          SkPreMultiplyColor(color_utils::HSLToSkColor(hsl, alpha));
    }
  }
  out.setImmutable();
  return out;
}

gfx::ImageSkia AdaptFaviconSkia(gfx::ImageSkia source,
                                std::optional<SpaceTheme> theme,
                                const ui::ColorProvider* provider) {
  if (!provider) {
    return source;
  }
  const SkColor background = GetSidebarPeekColor(*provider, theme);
  if (!color_utils::IsDark(background)) {
    return source;
  }
  source.EnsureRepsForSupportedScales();
  const std::vector<gfx::ImageSkiaRep> reps = source.image_reps();
  if (reps.empty()) {
    return source;
  }
  const bool needs_invert = std::ranges::any_of(
      reps, [background](const gfx::ImageSkiaRep& rep) {
        return NeedsLightnessInvert(rep.GetBitmap(), background);
      });
  if (!needs_invert) {
    return source;
  }
  gfx::ImageSkia lifted;
  for (const gfx::ImageSkiaRep& rep : reps) {
    const SkBitmap& bitmap = rep.GetBitmap();
    if (bitmap.drawsNothing()) {
      continue;
    }
    lifted.AddRepresentation(
        gfx::ImageSkiaRep(InvertBitmapLightness(bitmap), rep.scale()));
  }
  return lifted.isNull() ? source : lifted;
}

}  // namespace

ui::ImageModel EnsureFaviconContrast(const ui::ImageModel& icon,
                                     const std::optional<SpaceTheme>& theme) {
  if (icon.IsEmpty() || !icon.IsImage()) {
    return icon;
  }
  const gfx::ImageSkia source = icon.GetImage().AsImageSkia();
  if (source.isNull()) {
    return icon;
  }
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(&AdaptFaviconSkia, source, theme), source.size());
}

}  // namespace myarc
