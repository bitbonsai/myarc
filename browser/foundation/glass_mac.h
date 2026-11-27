#ifndef MYARC_GLASS_MAC_H_
#define MYARC_GLASS_MAC_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_MAC) && defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif

namespace views {
class Widget;
}  // namespace views

namespace myarc {

// Canonical design tokens shared by all myArc frosted-glass dialogs,
// modal panels, About myArc, and the Sparkle updater UI.
namespace dialog {

inline constexpr float kCornerRadius = 16.0f;
inline constexpr float kButtonCornerRadius = 7.0f;
inline constexpr int kPadding = 24;
inline constexpr int kTitleTopPadding = 36;
inline constexpr int kButtonHeight = 36;
inline constexpr int kButtonMinWidth = 84;
inline constexpr float kCloseDotSize = 12.0f;
inline constexpr float kCloseDotInset = 12.0f;

// Frosted dark/light surface fills and 1pt specular rim alphas.
inline constexpr float kDarkGlassRed = 0x18 / 255.0f;
inline constexpr float kDarkGlassGreen = 0x18 / 255.0f;
inline constexpr float kDarkGlassBlue = 0x1D / 255.0f;
inline constexpr float kDarkGlassAlpha = 0.63f;
inline constexpr SkColor kDarkGlassTint = SkColorSetARGB(0xA1, 0x18, 0x18, 0x1D);
inline constexpr SkColor kLightGlassTint = SkColorSetARGB(0xD4, 0xFA, 0xFA, 0xFD);
inline constexpr float kDarkRimAlpha = 0.20f;
inline constexpr float kLightRimAlpha = 0.14f;

}  // namespace dialog

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

// True if ApplyWidgetGlass or ApplyBubbleGlass has already styled `widget`.
bool HasWidgetGlass(const views::Widget& widget);

// Styles a BubbleFrameView-backed widget (BubbleDialogDelegate,
// BubbleDialogModelHost, or DialogDelegate) with a frosted glass backdrop,
// translucent `glass_tint`, and an optional 1px specular rim hairline (or
// `opaque_fill` when Reduce Transparency is on). Returns true when frosted
// glass is active.
bool ApplyBubbleGlass(views::Widget& widget,
                      GlassMaterial material,
                      bool dark,
                      SkColor glass_tint,
                      SkColor opaque_fill,
                      bool draw_rim = true);

// Spins `native_view` 180 degrees around its vertical center with perspective
// (0 -> +-90 deg, invokes `on_midpoint`, then -+90 -> 0 deg). Falls back to
// invoking `on_midpoint` immediately when Reduce Motion is enabled.
void FlipNativeView3D(gfx::NativeView native_view,
                      bool reverse,
                      base::OnceClosure on_midpoint);

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

inline bool HasWidgetGlass(const views::Widget& widget) {
  return false;
}

inline bool ApplyBubbleGlass(views::Widget& widget,
                             GlassMaterial material,
                             bool dark,
                             SkColor glass_tint,
                             SkColor opaque_fill,
                             bool draw_rim = true) {
  return false;
}

inline void FlipNativeView3D(gfx::NativeView native_view,
                             bool reverse,
                             base::OnceClosure on_midpoint) {
  if (on_midpoint) {
    std::move(on_midpoint).Run();
  }
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace myarc

#endif  // MYARC_GLASS_MAC_H_
