#ifndef MYARC_GLASS_MAC_H_
#define MYARC_GLASS_MAC_H_

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

#if BUILDFLAG(IS_MAC) && defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif

namespace views {
class Widget;
}  // namespace views

namespace myarc {

// Frosted glass material presets for myArc-owned surfaces.
enum class GlassMaterial {
  // Frosted window backdrop (e.g. Settings window).
  kWindow,
  // Floating popover, picker, or command bar.
  kPopover,
  // High-contrast HUD, prompt, toast, or tooltip.
  kHud,
  // Modal dialog or window sheet.
  kSheet,
};

#if BUILDFLAG(IS_MAC)

// True when macOS "Reduce transparency" is enabled.
bool ShouldReduceTransparency();

// Installs or updates an NSVisualEffectView behind `widget`'s compositor view
// with behind-window blending, a rounded mask matching `corner_radius` (0 for
// unmasked), and Dark/Light appearance. Removes any installed backdrop and
// returns false when Reduce Transparency is on.
bool ApplyWidgetGlass(views::Widget& widget,
                      GlassMaterial material,
                      float corner_radius,
                      bool dark,
                      const gfx::Insets& insets = gfx::Insets());

// Styles a BubbleFrameView-backed widget (BubbleDialogDelegate,
// BubbleDialogModelHost, or DialogDelegate) with a frosted glass backdrop and
// translucent `glass_tint`, or `opaque_fill` when Reduce Transparency is on.
// Returns true when frosted glass is active.
bool ApplyBubbleGlass(views::Widget& widget,
                      GlassMaterial material,
                      bool dark,
                      SkColor glass_tint,
                      SkColor opaque_fill);

#if defined(__OBJC__)
// Installs or updates a behind-window NSVisualEffectView in `window`'s
// contentView, or removes it and returns nil when Reduce Transparency is on.
NSVisualEffectView* ApplyNSWindowGlass(NSWindow* window,
                                       GlassMaterial material,
                                       CGFloat corner_radius,
                                       bool dark,
                                       NSEdgeInsets insets = NSEdgeInsetsZero);
#endif  // defined(__OBJC__)

#else  // !BUILDFLAG(IS_MAC)

inline bool ShouldReduceTransparency() {
  return true;
}

inline bool ApplyWidgetGlass(views::Widget& widget,
                             GlassMaterial material,
                             float corner_radius,
                             bool dark,
                             const gfx::Insets& insets = gfx::Insets()) {
  return false;
}

inline bool ApplyBubbleGlass(views::Widget& widget,
                             GlassMaterial material,
                             bool dark,
                             SkColor glass_tint,
                             SkColor opaque_fill) {
  return false;
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace myarc

#endif  // MYARC_GLASS_MAC_H_
