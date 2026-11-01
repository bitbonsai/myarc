#include "myarc/sidebar_tint.h"

#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"

namespace myarc {

SidebarTint GetSidebarTint(bool dark) {
  return dark ? SidebarTint{SkColorSetARGB(191, 26, 23, 46),
                            SkColorSetARGB(191, 51, 31, 51)}
              : SidebarTint{SkColorSetARGB(166, 227, 224, 242),
                            SkColorSetARGB(166, 240, 230, 240)};
}

SkColor GetSidebarPeekColor(const ui::ColorProvider& provider) {
  const bool dark =
      color_utils::IsDark(provider.GetColor(ui::kColorSysSurface));
  const SidebarTint tint = GetSidebarTint(dark);
  // The sidebar covers about the first quarter of the gradient.
  const SkColor sidebar = color_utils::AlphaBlend(tint.end, tint.start, 0.25f);
  return color_utils::GetResultingPaintColor(
      sidebar, provider.GetColor(ui::kColorFrameActive));
}

}  // namespace myarc
