#include "myarc/glass_mac.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

// Behind-window frosted glass view placed beneath Chromium's compositor view.
// Ignores pointer events so clicks always reach the hosted content, and tracks
// superview resizes with exact edge insets even if installed before layout.
@interface MyarcGlassBackdropView : NSVisualEffectView
@property(nonatomic) NSEdgeInsets myarcInsets;
- (void)updateFrameForSuperview;
@end

@implementation MyarcGlassBackdropView
@synthesize myarcInsets = _myarcInsets;

- (NSView*)hitTest:(NSPoint)point {
  return nil;
}

- (void)updateFrameForSuperview {
  NSView* superview = self.superview;
  if (!superview) {
    return;
  }
  const NSRect bounds = superview.bounds;
  self.frame = NSMakeRect(
      _myarcInsets.left, _myarcInsets.bottom,
      std::max<CGFloat>(0,
                        NSWidth(bounds) - _myarcInsets.left - _myarcInsets.right),
      std::max<CGFloat>(
          0, NSHeight(bounds) - _myarcInsets.top - _myarcInsets.bottom));
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize {
  [self updateFrameForSuperview];
}
@end

namespace myarc {
namespace {

NSVisualEffectMaterial ToAppKitMaterial(GlassMaterial material) {
  switch (material) {
    case GlassMaterial::kWindow:
    case GlassMaterial::kHud:
    case GlassMaterial::kPopover:
    case GlassMaterial::kSheet:
      return NSVisualEffectMaterialHUDWindow;
  }
}

// Stretchable rounded-rect mask cached by half-point corner radius.
NSImage* RoundedMask(CGFloat radius) {
  if (radius <= 0) {
    return nil;
  }
  static NSMutableDictionary<NSNumber*, NSImage*>* cache =
      [[NSMutableDictionary alloc] init];
  const int key_val = static_cast<int>(std::lround(radius * 2.0));
  NSNumber* key = @(key_val);
  if (NSImage* existing = cache[key]) {
    return existing;
  }
  const CGFloat r = key_val / 2.0;
  const CGFloat side = r * 2.0 + 1.0;
  NSImage* image =
      [NSImage imageWithSize:NSMakeSize(side, side)
                     flipped:NO
              drawingHandler:^BOOL(NSRect rect) {
                [NSColor.blackColor setFill];
                [[NSBezierPath bezierPathWithRoundedRect:rect
                                                 xRadius:r
                                                 yRadius:r] fill];
                return YES;
              }];
  image.capInsets = NSEdgeInsetsMake(r, r, r, r);
  image.resizingMode = NSImageResizingModeStretch;
  cache[key] = image;
  return image;
}

MyarcGlassBackdropView* FindBackdrop(NSView* content) {
  for (NSView* view in content.subviews) {
    if ([view isKindOfClass:[MyarcGlassBackdropView class]]) {
      return static_cast<MyarcGlassBackdropView*>(view);
    }
  }
  return nil;
}

}  // namespace

bool ShouldReduceTransparency() {
  return NSWorkspace.sharedWorkspace
      .accessibilityDisplayShouldReduceTransparency;
}

NSVisualEffectView* ApplyNSWindowGlass(NSWindow* window,
                                       GlassMaterial material,
                                       CGFloat corner_radius,
                                       bool dark,
                                       NSEdgeInsets insets) {
  if (!window) {
    return nil;
  }
  NSView* content = window.contentView;
  if (!content) {
    return nil;
  }
  NSAppearance* appearance = [NSAppearance
      appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
  window.appearance = appearance;

  if (ShouldReduceTransparency()) {
    if (MyarcGlassBackdropView* existing = FindBackdrop(content)) {
      [existing removeFromSuperview];
    }
    return nil;
  }

  // Sheets keep window.opaque = YES by default in RemoteCocoa; mark the window
  // non-opaque with an almost-transparent background so the behind-window blur
  // shows through without triggering AppKit's clearColor continuous redraw.
  if (window.opaque) {
    [window setOpaque:NO];
    [window setBackgroundColor:[NSColor.windowBackgroundColor
                                   colorWithAlphaComponent:0.001]];
  }

  MyarcGlassBackdropView* glass = FindBackdrop(content);
  if (!glass) {
    glass = [[MyarcGlassBackdropView alloc] initWithFrame:content.bounds];
    glass.wantsLayer = YES;
    glass.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    glass.state = NSVisualEffectStateActive;
    glass.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [content addSubview:glass positioned:NSWindowBelow relativeTo:nil];
  }
  glass.myarcInsets = insets;
  [glass updateFrameForSuperview];
  glass.material = ToAppKitMaterial(material);
  glass.appearance = appearance;
  glass.maskImage = RoundedMask(corner_radius);
  return glass;
}

bool ApplyWidgetGlass(views::Widget& widget,
                      GlassMaterial material,
                      float corner_radius,
                      bool dark,
                      const gfx::Insets& insets) {
  NSWindow* window = widget.GetNativeWindow().GetNativeNSWindow();
  return ApplyNSWindowGlass(
             window, material, corner_radius, dark,
             NSEdgeInsetsMake(insets.top(), insets.left(), insets.bottom(),
                              insets.right())) != nil;
}

bool ApplyBubbleGlass(views::Widget& widget,
                      GlassMaterial material,
                      bool dark,
                      SkColor glass_tint,
                      SkColor opaque_fill) {
  const bool reduced = ShouldReduceTransparency();
  const SkColor fill = reduced ? opaque_fill : glass_tint;

  views::BubbleFrameView* frame =
      widget.non_client_view()
          ? views::AsViewClass<views::BubbleFrameView>(
                widget.non_client_view()->frame_view())
          : nullptr;
  if (widget.widget_delegate()) {
    if (auto* bubble = widget.widget_delegate()->AsBubbleDialogDelegate()) {
      bubble->SetBackgroundColor(fill);
    }
  }
  if (frame) {
    frame->SetBackgroundColor(fill);
  }

  if (reduced) {
    ApplyWidgetGlass(widget, material, 0, dark);
    return false;
  }

  gfx::Insets insets;
  float radius = 12.f;
  if (frame) {
    radius = frame->GetRoundedCorners().upper_left();
    if (frame->GetBorder()) {
      insets = frame->GetBorder()->GetInsets();
    }
  }
  return ApplyWidgetGlass(widget, material, radius, dark, insets);
}

}  // namespace myarc
