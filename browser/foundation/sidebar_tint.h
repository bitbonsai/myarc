#ifndef MYARC_SIDEBAR_TINT_H_
#define MYARC_SIDEBAR_TINT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {
class ColorProvider;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace myarc {

// A Space's theme from Choose a Theme: an opaque colour and how strongly it
// tints the sidebar, from 0 to 100 percent.
struct SpaceTheme {
  SkColor color;
  int opacity;
  bool operator==(const SpaceTheme&) const = default;
};

// The Space tint over the window's sidebar material: a translucent diagonal
// gradient from the top-leading corner to the bottom-trailing one.
struct SidebarTint {
  SkColor start;
  SkColor end;
};

// Without a theme, fixed stops per appearance. A theme shades its colour
// toward black in dark mode and toward white in light mode, so the sidebar's
// labels stay readable, and scales the stops' alpha by its opacity.
SidebarTint GetSidebarTint(bool dark, const std::optional<SpaceTheme>& theme);

// Arc paints its peeked sidebar opaque in the Space colour. This is the tint
// where the sidebar sits, over the frame colour standing in for the material.
SkColor GetSidebarPeekColor(const ui::ColorProvider& provider,
                            const std::optional<SpaceTheme>& theme);

// The theme `widget`'s sidebar shows: the current Space's, or the one the
// Create a Space panel previews. Setting it runs the widget's observer, with
// `fade` for a crossfade.
void SetSidebarTheme(views::Widget& widget,
                     std::optional<SpaceTheme> theme,
                     bool fade);
std::optional<SpaceTheme> GetSidebarTheme(const views::Widget* widget);
// The frame that paints `widget`'s tint repaints from this; one per widget.
void SetSidebarThemeObserver(views::Widget& widget,
                             base::RepeatingCallback<void(bool fade)> observer);

}  // namespace myarc

#endif  // MYARC_SIDEBAR_TINT_H_
