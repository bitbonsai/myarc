#include "myarc/sidebar_tint.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "ui/base/class_property.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
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

SidebarTint GetSidebarTint(bool dark, const std::optional<SpaceTheme>& theme) {
  if (!theme) {
    return dark ? SidebarTint{SkColorSetARGB(191, 26, 23, 46),
                              SkColorSetARGB(191, 51, 31, 51)}
                : SidebarTint{SkColorSetARGB(166, 227, 224, 242),
                              SkColorSetARGB(166, 240, 230, 240)};
  }
  const SkColor color = SkColorSetA(theme->color, SK_AlphaOPAQUE);
  // At 100% the sidebar is nearly opaque; at 0% the material shows plain.
  const SkAlpha alpha =
      static_cast<SkAlpha>(230 * std::clamp(theme->opacity, 0, 100) / 100);
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

}  // namespace myarc
