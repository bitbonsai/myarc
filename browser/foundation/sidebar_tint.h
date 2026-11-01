#ifndef MYARC_SIDEBAR_TINT_H_
#define MYARC_SIDEBAR_TINT_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ui {
class ColorProvider;
}  // namespace ui

namespace myarc {

// The Space tint over the window's sidebar material: a translucent diagonal
// gradient from the top-leading corner to the bottom-trailing one.
struct SidebarTint {
  SkColor start;
  SkColor end;
};

// shortcut: fixed stops per appearance; use Space themes when available.
SidebarTint GetSidebarTint(bool dark);

// Arc paints its peeked sidebar opaque in the Space colour. This is the tint
// where the sidebar sits, over the frame colour standing in for the material.
SkColor GetSidebarPeekColor(const ui::ColorProvider& provider);

}  // namespace myarc

#endif  // MYARC_SIDEBAR_TINT_H_
