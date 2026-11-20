#include "myarc/welcome_intro.h"

#import <Cocoa/Cocoa.h>
#import <CoreImage/CoreImage.h>
#import <QuartzCore/QuartzCore.h>

#include <array>
#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace myarc {
namespace {

// The phases, from the intro's start. The dim and the blob come in together.
constexpr base::TimeDelta kDimIn = base::Milliseconds(750);
constexpr base::TimeDelta kBlobIn = base::Milliseconds(600);
// The blob holds, morphing, until the grow.
constexpr base::TimeDelta kGrowAt = base::Milliseconds(3000);
constexpr base::TimeDelta kGrow = base::Milliseconds(300);
constexpr base::TimeDelta kLift = base::Milliseconds(300);

constexpr CGFloat kDim = 0.85;
// The blob's width as a share of the main display's, and its blur as a
// share of that width.
constexpr CGFloat kBlobShare = 0.25;
constexpr CGFloat kBlurShare = 0.06;
// The welcome window's corner radius, which clips the grow.
constexpr CGFloat kWindowRadius = 12;

// myarc.dev's midnight, for the dim, and its blue, the blob's body.
constexpr uint32_t kMidnight = 0x101C2B;
constexpr uint32_t kSiteBlue = 0x034BF4;
// Highlights in the ribbon logo's colours (design/brand/assets/
// myarc-matte.png), drifting across the body. Sizes and reaches are shares
// of the blob's width; periods are in seconds.
struct Highlight {
  uint32_t rgb;
  CGFloat size;
  CGFloat reach;
  double period_x;
  double period_y;
  double phase;
};
constexpr Highlight kHighlights[] = {
    {0x6331FA, 0.75, 0.22, 3.7, 2.9, 0.0},  // violet
    {0xF047DE, 0.55, 0.26, 2.9, 3.5, 2.1},  // pink
    {0xFC9378, 0.6, 0.28, 3.3, 2.5, 4.2},   // coral
};
// The window's field the blob smears into: the ribbon's sky at the top left,
// the site blue, then the ribbon's violet.
constexpr uint32_t kFieldTopLeft = 0x0B9EFC;
constexpr uint32_t kFieldBottomRight = 0x6331FA;

// The outline: kPoints points around the centre, each radius wobbling on its
// own sine wave, out of phase with the others, by kWobble, plus a soft
// three-lobed bias that turns, so the shape reads as a rounded triangle that
// keeps morphing. Every wave repeats within kLoop, so the outline loops.
constexpr size_t kPoints = 7;
constexpr std::array<int, kPoints> kCycles = {1, 2, 1, 3, 2, 1, 2};
constexpr double kWobble = 0.15;
constexpr double kTriangle = 0.1;
constexpr double kLoop = 6;
constexpr int kOutlineFrames = 120;

id Color(uint32_t rgb, CGFloat alpha) {
  return (__bridge id)[NSColor colorWithSRGBRed:((rgb >> 16) & 0xFF) / 255.0
                                          green:((rgb >> 8) & 0xFF) / 255.0
                                           blue:(rgb & 0xFF) / 255.0
                                          alpha:alpha]
      .CGColor;
}

CFTimeInterval Seconds(base::TimeDelta delta) {
  return delta.InSecondsF();
}

// The outline at `t` seconds into its loop, of base radius `radius` around
// `center`: a closed Catmull-Rom curve through the points, as cubic Beziers.
id Outline(CGPoint center, CGFloat radius, double t) {
  std::array<CGPoint, kPoints> points;
  for (size_t i = 0; i < kPoints; ++i) {
    const double angle = 2 * M_PI * i / kPoints;
    const double phase = 2 * M_PI * t / kLoop;
    const double r =
        radius * (1 + kWobble * std::sin(kCycles[i] * phase + i * 2.1) +
                  kTriangle * std::cos(3 * angle - phase));
    points[i] = CGPointMake(center.x + r * std::cos(angle),
                            center.y + r * std::sin(angle));
  }
  CGMutablePathRef path = CGPathCreateMutable();
  CGPathMoveToPoint(path, nullptr, points[0].x, points[0].y);
  for (size_t i = 0; i < kPoints; ++i) {
    const CGPoint p0 = points[(i + kPoints - 1) % kPoints];
    const CGPoint p1 = points[i];
    const CGPoint p2 = points[(i + 1) % kPoints];
    const CGPoint p3 = points[(i + 2) % kPoints];
    CGPathAddCurveToPoint(path, nullptr, p1.x + (p2.x - p0.x) / 6,
                          p1.y + (p2.y - p0.y) / 6, p2.x - (p3.x - p1.x) / 6,
                          p2.y - (p3.y - p1.y) / 6, p2.x, p2.y);
  }
  CGPathCloseSubpath(path);
  return CFBridgingRelease(path);
}

// The outline's loop as a path animation.
CAKeyframeAnimation* Morph(CGPoint center, CGFloat radius) {
  NSMutableArray* values = [NSMutableArray arrayWithCapacity:kOutlineFrames];
  for (int i = 0; i <= kOutlineFrames; ++i) {
    [values addObject:Outline(center, radius, kLoop * i / kOutlineFrames)];
  }
  CAKeyframeAnimation* animation =
      [CAKeyframeAnimation animationWithKeyPath:@"path"];
  animation.values = values;
  animation.duration = kLoop;
  animation.repeatCount = HUGE_VALF;
  return animation;
}

// A sine path for one axis around `center`, looping every `period` seconds.
CAKeyframeAnimation* Drift(NSString* key_path,
                           CGFloat center,
                           CGFloat reach,
                           double period,
                           double phase) {
  constexpr int kSteps = 32;
  NSMutableArray* values = [NSMutableArray arrayWithCapacity:kSteps + 1];
  for (int i = 0; i <= kSteps; ++i) {
    [values
        addObject:@(center + reach * std::sin(phase + 2 * M_PI * i / kSteps))];
  }
  CAKeyframeAnimation* animation =
      [CAKeyframeAnimation animationWithKeyPath:key_path];
  animation.values = values;
  animation.duration = period;
  animation.repeatCount = HUGE_VALF;
  animation.calculationMode = kCAAnimationCubic;
  return animation;
}

CABasicAnimation* From(NSString* key_path,
                       id from,
                       base::TimeDelta duration,
                       CAMediaTimingFunctionName timing) {
  CABasicAnimation* animation =
      [CABasicAnimation animationWithKeyPath:key_path];
  animation.fromValue = from;
  animation.duration = Seconds(duration);
  animation.timingFunction = [CAMediaTimingFunction functionWithName:timing];
  return animation;
}

// `screen` below its menu bar, in screen coordinates.
NSRect BelowMenuBar(NSScreen* screen) {
  NSRect rect = screen.frame;
  rect.size.height = NSMaxY(screen.visibleFrame) - NSMinY(rect);
  return rect;
}

}  // namespace

struct WelcomeIntro::Cocoa {
  // The welcome window, hidden until the blob lands. Weak: the browser can
  // close it first.
  __weak NSWindow* welcome = nil;
  CGFloat welcome_alpha = 1;
  // One dim window per display, each with its shade.
  NSMutableArray<NSWindow*>* dims = [NSMutableArray array];
  NSMutableArray<CALayer*>* shades = [NSMutableArray array];
  // In the main display's dim window: the welcome window's frame, which
  // clips the grow, the blob in it, and the field the blob smears into.
  CALayer* clip = nil;
  CALayer* blob = nil;
  CAGradientLayer* field = nil;
};

WelcomeIntro::WelcomeIntro(gfx::NativeWindow window,
                           base::OnceClosure done,
                           bool log)
    : cocoa_(std::make_unique<Cocoa>()),
      done_(std::move(done)),
      log_(log),
      start_(base::TimeTicks::Now()) {
  NSWindow* welcome = window.GetNativeNSWindow();
  // The main display, the one with the menu bar.
  NSScreen* home = NSScreen.screens.firstObject;
  cocoa_->welcome = welcome;
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  for (NSScreen* screen in NSScreen.screens) {
    // The menu bar never dims.
    const NSRect area = BelowMenuBar(screen);
    NSWindow* dim =
        [[NSWindow alloc] initWithContentRect:area
                                    styleMask:NSWindowStyleMaskBorderless
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    dim.releasedWhenClosed = NO;
    dim.opaque = NO;
    dim.backgroundColor = NSColor.clearColor;
    dim.hasShadow = NO;
    dim.ignoresMouseEvents = YES;
    dim.animationBehavior = NSWindowAnimationBehaviorNone;
    // Over every app's windows and the Dock.
    dim.level = NSMainMenuWindowLevel - 1;
    dim.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                             NSWindowCollectionBehaviorStationary |
                             NSWindowCollectionBehaviorIgnoresCycle |
                             NSWindowCollectionBehaviorFullScreenAuxiliary;
    NSView* content = dim.contentView;
    content.wantsLayer = YES;
    // The blob's blur is a Core Image filter.
    content.layerUsesCoreImageFilters = YES;
    CALayer* root = content.layer;

    CALayer* shade = [CALayer layer];
    shade.frame = root.bounds;
    shade.backgroundColor = (__bridge CGColorRef)Color(kMidnight, 1);
    shade.opacity = kDim;
    [root addSublayer:shade];
    [shade
        addAnimation:From(@"opacity", @0, kDimIn, kCAMediaTimingFunctionEaseOut)
              forKey:@"in"];
    [cocoa_->dims addObject:dim];
    [cocoa_->shades addObject:shade];

    if (screen != home) {
      continue;
    }
    // The welcome window's frame; it clips the blob only once it grows.
    NSRect target = welcome ? welcome.frame : screen.visibleFrame;
    target.origin.x -= area.origin.x;
    target.origin.y -= area.origin.y;
    CALayer* clip = [CALayer layer];
    clip.frame = target;
    clip.cornerRadius = kWindowRadius;
    [root addSublayer:clip];
    cocoa_->clip = clip;

    // The blob sits at the display's centre, whatever the pointer does.
    const CGFloat width = NSWidth(screen.frame) * kBlobShare;
    const CGFloat side = width * 1.8;
    const CGPoint at =
        CGPointMake(NSMidX(screen.frame) - NSMinX(area) - NSMinX(target),
                    NSMidY(screen.frame) - NSMinY(area) - NSMinY(target));
    CALayer* blob = [CALayer layer];
    blob.bounds = CGRectMake(0, 0, side, side);
    blob.position = at;
    CIFilter* blur = [CIFilter filterWithName:@"CIGaussianBlur"];
    [blur setValue:@(width * kBlurShare) forKey:kCIInputRadiusKey];
    blob.filters = @[ blur ];

    // One body in the site blue, cut to the morphing outline.
    const CGPoint middle = CGPointMake(side / 2, side / 2);
    CALayer* body = [CALayer layer];
    body.frame = blob.bounds;
    body.backgroundColor = (__bridge CGColorRef)Color(kSiteBlue, 1);
    CAShapeLayer* outline = [CAShapeLayer layer];
    outline.frame = body.bounds;
    outline.path = (__bridge CGPathRef)Outline(middle, width / 2, 0);
    [outline addAnimation:Morph(middle, width / 2) forKey:@"morph"];
    body.mask = outline;
    // The highlights drift across the body and turn with it.
    CALayer* glow = [CALayer layer];
    glow.frame = body.bounds;
    for (const Highlight& highlight : kHighlights) {
      CAGradientLayer* patch = [CAGradientLayer layer];
      patch.type = kCAGradientLayerRadial;
      patch.startPoint = CGPointMake(0.5, 0.5);
      patch.endPoint = CGPointMake(1, 1);
      patch.colors = @[
        Color(highlight.rgb, 0.95), Color(highlight.rgb, 0.5),
        Color(highlight.rgb, 0)
      ];
      patch.locations = @[ @0, @0.4, @1 ];
      const CGFloat size = width * highlight.size;
      patch.bounds = CGRectMake(0, 0, size, size);
      patch.position = middle;
      [patch
          addAnimation:Drift(@"position.x", middle.x, width * highlight.reach,
                             highlight.period_x, highlight.phase)
                forKey:@"x"];
      [patch
          addAnimation:Drift(@"position.y", middle.y, width * highlight.reach,
                             highlight.period_y, highlight.phase + 1.3)
                forKey:@"y"];
      [glow addSublayer:patch];
    }
    CABasicAnimation* turn =
        [CABasicAnimation animationWithKeyPath:@"transform.rotation.z"];
    turn.fromValue = @0;
    turn.toValue = @(2 * M_PI);
    turn.duration = 9;
    turn.repeatCount = HUGE_VALF;
    [glow addAnimation:turn forKey:@"turn"];
    [body addSublayer:glow];
    [blob addSublayer:body];

    CAAnimationGroup* in = [CAAnimationGroup animation];
    in.animations = @[
      From(@"opacity", @0, kBlobIn, kCAMediaTimingFunctionEaseOut),
      From(@"transform.scale", @0.6, kBlobIn, kCAMediaTimingFunctionEaseOut),
    ];
    in.duration = Seconds(kBlobIn);
    [blob addAnimation:in forKey:@"in"];
    [clip addSublayer:blob];
    cocoa_->blob = blob;

    CAGradientLayer* field = [CAGradientLayer layer];
    field.colors = @[
      Color(kFieldTopLeft, 1), Color(kSiteBlue, 1), Color(kFieldBottomRight, 1)
    ];
    field.startPoint = CGPointMake(0, 1);
    field.endPoint = CGPointMake(1, 0);
    field.frame = clip.bounds;
    field.opacity = 0;
    [clip addSublayer:field];
    cocoa_->field = field;
  }
  [CATransaction commit];
  if (welcome) {
    cocoa_->welcome_alpha = welcome.alphaValue;
    welcome.alphaValue = 0;
  }
  // Front without activating the app, so a background launch shows it too.
  for (NSWindow* dim in cocoa_->dims) {
    [dim orderFrontRegardless];
  }
  Log("dim");
  timer_.Start(FROM_HERE, kGrowAt,
               base::BindOnce(&WelcomeIntro::Grow, base::Unretained(this)));
}

WelcomeIntro::~WelcomeIntro() {
  timer_.Stop();
  for (NSWindow* dim in cocoa_->dims) {
    [dim.contentView.layer removeFromSuperlayer];
    [dim orderOut:nil];
    [dim close];
  }
  [cocoa_->dims removeAllObjects];
  if (NSWindow* welcome = cocoa_->welcome) {
    welcome.alphaValue = cocoa_->welcome_alpha;
  }
}

void WelcomeIntro::Grow() {
  Log("grow");
  CALayer* clip = cocoa_->clip;
  CALayer* blob = cocoa_->blob;
  if (clip && blob) {
    const CGRect frame = clip.bounds;
    const CGFloat side = blob.bounds.size.width;
    const CGPoint from = blob.position;
    const CGPoint center = CGPointMake(CGRectGetMidX(frame) + side * 0.06,
                                       CGRectGetMidY(frame) - side * 0.04);
    CAMediaTimingFunctionName ease = kCAMediaTimingFunctionEaseIn;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    // The blob swells unevenly past the window's edges, still morphing, and
    // the window's rounded frame clips it.
    clip.masksToBounds = YES;
    const CATransform3D swell = CATransform3DMakeScale(
        frame.size.width / side * 2.1, frame.size.height / side * 2.6, 1);
    blob.position = center;
    blob.transform = swell;
    CAAnimationGroup* grow = [CAAnimationGroup animation];
    grow.animations = @[
      From(@"position", [NSValue valueWithPoint:from], kGrow, ease),
      From(@"transform", [NSValue valueWithCATransform3D:CATransform3DIdentity],
           kGrow, ease),
    ];
    grow.duration = Seconds(kGrow);
    [blob addAnimation:grow forKey:@"grow"];
    // Its highlights smear into the window's field.
    CAGradientLayer* field = cocoa_->field;
    field.opacity = 0.7;
    CAKeyframeAnimation* show =
        [CAKeyframeAnimation animationWithKeyPath:@"opacity"];
    show.values = @[ @0, @0.1, @0.7 ];
    show.keyTimes = @[ @0, @0.5, @1 ];
    show.duration = Seconds(kGrow);
    [field addAnimation:show forKey:@"grow"];
    [CATransaction commit];
  }
  timer_.Start(FROM_HERE, kGrow,
               base::BindOnce(&WelcomeIntro::Lift, base::Unretained(this)));
}

void WelcomeIntro::Lift() {
  Log("lift");
  // The window shows under the clipped blob, which crossfades into it as
  // the dim lifts.
  if (NSWindow* welcome = cocoa_->welcome) {
    welcome.alphaValue = cocoa_->welcome_alpha;
    // An app launched in the background has its window behind the others,
    // where the blob would land on nothing. Front without activating, so
    // focus stays where it is.
    if (!NSApp.active) {
      [welcome orderFrontRegardless];
    }
  }
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  for (CALayer* shade in cocoa_->shades) {
    shade.opacity = 0;
    [shade addAnimation:From(@"opacity", @(kDim), kLift,
                             kCAMediaTimingFunctionEaseOut)
                 forKey:@"out"];
  }
  if (CALayer* clip = cocoa_->clip) {
    clip.opacity = 0;
    [clip
        addAnimation:From(@"opacity", @1, kLift, kCAMediaTimingFunctionEaseOut)
              forKey:@"out"];
  }
  [CATransaction commit];
  timer_.Start(FROM_HERE, kLift,
               base::BindOnce(&WelcomeIntro::Finish, base::Unretained(this)));
}

void WelcomeIntro::Finish() {
  Log("done");
  // Posted: the owner deletes the intro from `done`, and this runs inside
  // the intro's own timer.
  if (done_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done_));
  }
}

void WelcomeIntro::Log(const char* phase) const {
  if (log_) {
    LOG(WARNING) << "Test onboarding: intro " << phase << " at "
                 << (base::TimeTicks::Now() - start_).InMilliseconds() << " ms";
  }
}

}  // namespace myarc
