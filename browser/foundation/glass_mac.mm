#include "myarc/glass_mac.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/scroll_view.h"
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

// Crisp 1px specular rim hairline layered above a frosted glass bubble's
// compositor view. Ignores pointer events and tracks superview resizes with
// exact edge insets matching MyarcGlassBackdropView.
@interface MyarcGlassRimView : NSView
@property(nonatomic) NSEdgeInsets myarcInsets;
@property(nonatomic) CGFloat myarcCornerRadius;
@property(nonatomic) BOOL myarcDark;
- (void)updateFrameForSuperview;
@end

@implementation MyarcGlassRimView
@synthesize myarcInsets = _myarcInsets;
@synthesize myarcCornerRadius = _myarcCornerRadius;
@synthesize myarcDark = _myarcDark;

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

- (BOOL)wantsUpdateLayer {
  return YES;
}

- (void)updateLayer {
  CALayer* layer = self.layer;
  if (!layer) {
    return;
  }
  layer.backgroundColor = nil;
  layer.cornerRadius = _myarcCornerRadius;
  layer.borderWidth = 1.0;
  layer.borderColor =
      (_myarcDark
           ? [NSColor colorWithSRGBRed:1.0 green:1.0 blue:1.0 alpha:0.20]
           : [NSColor colorWithSRGBRed:0.0 green:0.0 blue:0.0 alpha:0.14])
          .CGColor;
}
@end

namespace myarc {
namespace {

const char kMyarcGlassStyledKey = 0;

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

void MarkWindowGlassStyled(NSWindow* window) {
  if (window) {
    objc_setAssociatedObject(window, &kMyarcGlassStyledKey, @YES,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
}

MyarcGlassRimView* FindRim(NSView* content) {
  for (NSView* view in content.subviews) {
    if ([view isKindOfClass:[MyarcGlassRimView class]]) {
      return static_cast<MyarcGlassRimView*>(view);
    }
  }
  return nil;
}

void ApplyBubbleRim(NSWindow* window,
                    CGFloat corner_radius,
                    bool dark,
                    NSEdgeInsets insets,
                    bool show) {
  if (!window || !window.contentView) {
    return;
  }
  NSView* content = window.contentView;
  if (!show) {
    if (MyarcGlassRimView* existing = FindRim(content)) {
      [existing removeFromSuperview];
    }
    return;
  }
  MyarcGlassRimView* rim = FindRim(content);
  if (!rim) {
    rim = [[MyarcGlassRimView alloc] initWithFrame:content.bounds];
    rim.wantsLayer = YES;
    rim.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [content addSubview:rim positioned:NSWindowAbove relativeTo:nil];
  }
  rim.myarcInsets = insets;
  rim.myarcCornerRadius = corner_radius;
  rim.myarcDark = dark;
  [rim updateFrameForSuperview];
  [rim setNeedsDisplay:YES];
}

}  // namespace

bool ShouldReduceTransparency() {
  return NSWorkspace.sharedWorkspace
      .accessibilityDisplayShouldReduceTransparency;
}

bool HasWidgetGlass(const views::Widget& widget) {
  NSWindow* window = widget.GetNativeWindow().GetNativeNSWindow();
  return window &&
         objc_getAssociatedObject(window, &kMyarcGlassStyledKey) != nil;
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
  MarkWindowGlassStyled(window);

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
                      SkColor opaque_fill,
                      bool draw_rim) {
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
    // Modal BubbleDialogModelHost wraps its contents in a ScrollView whose
    // default opaque kColorDialogBackground would cover the frosted backdrop.
    if (auto* scroll = views::AsViewClass<views::ScrollView>(
            widget.widget_delegate()->GetContentsView())) {
      scroll->SetBackgroundColor(std::nullopt);
    }
  }
  if (frame) {
    frame->SetBackgroundColor(fill);
  }

  NSWindow* window = widget.GetNativeWindow().GetNativeNSWindow();
  if (reduced) {
    ApplyBubbleRim(window, 0, dark, NSEdgeInsetsZero, /*show=*/false);
    ApplyWidgetGlass(widget, material, 0, dark);
    return false;
  }

  gfx::Insets insets;
  float radius = 12.f;
  bool has_visible_arrow = false;
  if (frame) {
    radius = frame->GetRoundedCorners().upper_left();
    if (radius > 12.f) {
      frame->SetRoundedCorners(gfx::RoundedCornersF(12.f));
      radius = 12.f;
    }
    has_visible_arrow = frame->GetDisplayVisibleArrow();
    if (frame->GetBorder()) {
      insets = frame->GetBorder()->GetInsets();
    }
  }
  const bool applied =
      ApplyWidgetGlass(widget, material, radius, dark, insets);
  ApplyBubbleRim(
      window, radius, dark,
      NSEdgeInsetsMake(insets.top(), insets.left(), insets.bottom(),
                       insets.right()),
      applied && draw_rim && !has_visible_arrow);
  return applied;
}

}  // namespace myarc
