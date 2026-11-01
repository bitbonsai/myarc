#import <Cocoa/Cocoa.h>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

// The system blur behind the footer tooltip's pill. It ignores the pointer, as
// the tooltip does.
@interface MyarcTooltipGlassView : NSVisualEffectView
@end

@implementation MyarcTooltipGlassView
- (NSView*)hitTest:(NSPoint)point {
  return nil;
}
@end

namespace myarc {
namespace {

// Half the tooltip's 28 DIP height: a full pill at any width.
constexpr CGFloat kRadius = 14;

NSImage* PillMask() {
  static NSImage* const mask = [] {
    const CGFloat side = kRadius * 2 + 1;
    NSImage* image =
        [NSImage imageWithSize:NSMakeSize(side, side)
                       flipped:NO
                drawingHandler:^BOOL(NSRect rect) {
                  [NSColor.blackColor setFill];
                  [[NSBezierPath bezierPathWithRoundedRect:rect
                                                   xRadius:kRadius
                                                   yRadius:kRadius] fill];
                  return YES;
                }];
    image.capInsets = NSEdgeInsetsMake(kRadius, kRadius, kRadius, kRadius);
    image.resizingMode = NSImageResizingModeStretch;
    return image;
  }();
  return mask;
}

}  // namespace

void ApplyFooterTooltipGlass(views::Widget& tooltip, bool dark) {
  NSWindow* window = tooltip.GetNativeWindow().GetNativeNSWindow();
  NSView* content = window.contentView;
  if (!content) {
    return;
  }
  MyarcTooltipGlassView* glass = nil;
  for (NSView* view in content.subviews) {
    if ([view isKindOfClass:[MyarcTooltipGlassView class]]) {
      glass = static_cast<MyarcTooltipGlassView*>(view);
      break;
    }
  }
  if (!glass) {
    glass = [[MyarcTooltipGlassView alloc] initWithFrame:content.bounds];
    glass.material = NSVisualEffectMaterialHUDWindow;
    glass.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    glass.state = NSVisualEffectStateActive;
    glass.maskImage = PillMask();
    glass.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    // Below the compositor's content, as the browser window's material.
    [content addSubview:glass positioned:NSWindowBelow relativeTo:nil];
  }
  glass.frame = content.bounds;
  glass.appearance = [NSAppearance
      appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

}  // namespace myarc
