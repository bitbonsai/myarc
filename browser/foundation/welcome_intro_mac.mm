#include "myarc/welcome_intro.h"

#import <Accelerate/Accelerate.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "myarc/welcome_palette.h"
#include "myarc/welcome_timeline.h"

namespace myarc {
namespace {

// The dim and the blob come in together at t = 0; the rest follows
// welcome_timeline.h.
constexpr base::TimeDelta kDimIn = base::Milliseconds(750);
constexpr base::TimeDelta kBlobIn = base::Milliseconds(400);

constexpr CGFloat kDim = 0.85;
// The blob's width as a share of the main display's, and its blur as a
// share of that width.
constexpr CGFloat kBlobShare = 0.25;
constexpr CGFloat kBlurShare = 0.06;
// The welcome window's corner radius, which clips the grow.
constexpr CGFloat kWindowRadius = 12;

// myarc.dev's midnight, for the dim, and its blue, the blob's body.
constexpr uint32_t kMidnight = welcome::kMidnight;
constexpr uint32_t kSiteBlue = welcome::kSiteBlue;
// Highlights in the ribbon logo's colours (design/brand/assets/
// myarc-matte.png), drifting across the body, each at its own speed. Sizes
// and reaches are shares of the blob's width; periods are in seconds.
struct Highlight {
  uint32_t rgb;
  CGFloat size;
  CGFloat reach;
  double period_x;
  double period_y;
  double phase;
};
constexpr Highlight kHighlights[] = {
    {0x6331FA, 0.75, 0.22, 4.9, 4.1, 0.0},  // violet, slow
    {0xF047DE, 0.55, 0.26, 2.3, 2.9, 2.1},  // pink, quick
    {0xFC9378, 0.6, 0.28, 3.4, 1.9, 4.2},   // coral, quickest up and down
};

// The outline: kPoints points around the centre, each radius wobbling on its
// own sine wave, out of phase with the others, by kWobble, plus a soft
// three-lobed bias that turns, so the shape reads as a rounded triangle that
// keeps morphing. Every wave repeats within kLoop, so the outline loops.
constexpr size_t kPoints = 7;
constexpr std::array<int, kPoints> kCycles = {1, 2, 1, 3, 2, 1, 2};
constexpr double kWobble = 0.15;
constexpr double kTriangle = 0.1;
constexpr double kLoop = 6;
// The loop is blurred once, at the start, into this many frames this many
// pixels wide; Core Animation crossfades between them. A blurred shape
// loses nothing at that size.
constexpr int kOutlineFrames = 60;
constexpr size_t kFramePixels = 256;

// The pop as the blob comes in and the grow's settle: springs with a little
// overshoot.
constexpr CGFloat kPopStiffness = 220;
constexpr CGFloat kPopDamping = 19;
constexpr CGFloat kGrowStiffness = 320;
constexpr CGFloat kGrowDamping = 25;

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

// The outline's loop, blurred, as alpha-only images for a blob of base
// radius `radius` points in a `side`-point square, blurred by about `blur`
// points. Made once, on the CPU in a few milliseconds: three box blurs in a
// row come close to a Gaussian, and no frame blurs anything after that.
NSArray* BlurredOutlines(CGFloat side, CGFloat radius, CGFloat blur) {
  const CGFloat scale = kFramePixels / side;
  CGContextRef bitmap =
      CGBitmapContextCreate(nullptr, kFramePixels, kFramePixels, 8,
                            kFramePixels, nullptr, kCGImageAlphaOnly);
  if (!bitmap) {
    return @[];
  }
  vImage_Buffer shape = {CGBitmapContextGetData(bitmap), kFramePixels,
                         kFramePixels, kFramePixels};
  std::vector<uint8_t> scratch(kFramePixels * kFramePixels);
  vImage_Buffer other = {scratch.data(), kFramePixels, kFramePixels,
                         kFramePixels};
  // Three box passes of this odd width spread like a Gaussian of `blur`.
  const uint32_t box =
      static_cast<uint32_t>(std::lround(blur * scale * 1.2)) | 1;
  const CGRect extent = CGRectMake(0, 0, kFramePixels, kFramePixels);
  const CGPoint middle = CGPointMake(side / 2, side / 2);
  NSMutableArray* frames = [NSMutableArray arrayWithCapacity:kOutlineFrames];
  for (int i = 0; i < kOutlineFrames; ++i) {
    CGContextClearRect(bitmap, extent);
    CGContextSaveGState(bitmap);
    CGContextScaleCTM(bitmap, scale, scale);
    CGContextAddPath(bitmap, (__bridge CGPathRef)Outline(
                                 middle, radius, kLoop * i / kOutlineFrames));
    CGContextSetGrayFillColor(bitmap, 1, 1);
    CGContextFillPath(bitmap);
    CGContextRestoreGState(bitmap);
    vImageBoxConvolve_Planar8(&shape, &other, nullptr, 0, 0, box, box, 0,
                              kvImageEdgeExtend);
    vImageBoxConvolve_Planar8(&other, &shape, nullptr, 0, 0, box, box, 0,
                              kvImageEdgeExtend);
    vImageBoxConvolve_Planar8(&shape, &other, nullptr, 0, 0, box, box, 0,
                              kvImageEdgeExtend);
    vImageCopyBuffer(&other, &shape, 1, kvImageNoFlags);
    if (CGImageRef image = CGBitmapContextCreateImage(bitmap)) {
      [frames addObject:CFBridgingRelease(image)];
    }
  }
  CGContextRelease(bitmap);
  return frames;
}

// The blurred outlines as a looping contents animation.
CAKeyframeAnimation* Morph(NSArray* frames) {
  CAKeyframeAnimation* animation =
      [CAKeyframeAnimation animationWithKeyPath:@"contents"];
  animation.values = [frames arrayByAddingObject:frames.firstObject];
  animation.duration = kLoop;
  animation.repeatCount = HUGE_VALF;
  return animation;
}

// A spring from `from` to the layer's value at `key_path`.
CASpringAnimation* Spring(NSString* key_path,
                          id from,
                          CGFloat stiffness,
                          CGFloat damping) {
  CASpringAnimation* spring = [CASpringAnimation animationWithKeyPath:key_path];
  spring.fromValue = from;
  spring.stiffness = stiffness;
  spring.damping = damping;
  spring.duration = spring.settlingDuration;
  return spring;
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

// The welcome window's background as it starts, `size` points big: the
// base with the blurred fields where WelcomeView paints them before they
// drift, so the grow hands off to the window with no jump.
CALayer* Field(CGSize size, uint32_t base) {
  CALayer* field = [CALayer layer];
  field.frame = CGRectMake(0, 0, size.width, size.height);
  field.backgroundColor = (__bridge CGColorRef)Color(base, 1);
  const CGFloat longer = std::max(size.width, size.height);
  for (const welcome::Field& spec : welcome::kFields) {
    CAGradientLayer* glow = [CAGradientLayer layer];
    glow.type = kCAGradientLayerRadial;
    glow.startPoint = CGPointMake(0.5, 0.5);
    glow.endPoint = CGPointMake(1, 1);
    glow.colors = @[
      Color(spec.rgb, spec.alpha),
      Color(spec.rgb, spec.alpha * welcome::kFieldMid), Color(spec.rgb, 0)
    ];
    glow.locations = @[ @0, @(welcome::kFieldMidStop), @1 ];
    const CGFloat radius = std::floor(spec.radius * longer);
    glow.bounds = CGRectMake(0, 0, 2 * radius, 2 * radius);
    // Layers count y up, the window's views down.
    glow.position = CGPointMake(std::floor(spec.x * size.width),
                                size.height - std::floor(spec.y * size.height));
    [field addSublayer:glow];
  }
  return field;
}

// The welcome window's traffic lights.
NSArray<NSButton*>* Buttons(NSWindow* window) {
  NSMutableArray<NSButton*>* buttons = [NSMutableArray array];
  for (NSWindowButton kind :
       {NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton}) {
    if (NSButton* button = [window standardWindowButton:kind]) {
      [buttons addObject:button];
    }
  }
  return buttons;
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
  // Its traffic lights, hidden until they fade in after the grow.
  NSArray<NSButton*>* buttons = nil;
  bool buttons_shown = false;
  // One dim window per display, each with its shade.
  NSMutableArray<NSWindow*>* dims = [NSMutableArray array];
  NSMutableArray<CALayer*>* shades = [NSMutableArray array];
  // In the main display's dim window: the welcome window's frame, which
  // clips the grow, the blob in it, and the field the blob smears into.
  CALayer* clip = nil;
  CALayer* blob = nil;
  CALayer* field = nil;
  // The main display's shade, which leaves the window's frame undimmed once
  // the window shows.
  CALayer* home_shade = nil;
};

WelcomeIntro::WelcomeIntro(gfx::NativeWindow window,
                           uint32_t base,
                           base::OnceClosure done,
                           bool log)
    : cocoa_(std::make_unique<Cocoa>()),
      done_(std::move(done)),
      log_(log),
      start_(base::TimeTicks::Now()) {
  NSWindow* welcome = window.GetNativeNSWindow();
  // The welcome window's display: the main one on first run, wherever the
  // window is on Replay.
  NSScreen* home = welcome.screen ?: NSScreen.screens.firstObject;
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
    cocoa_->home_shade = shade;
    // The welcome window's frame; it clips the blob only once it grows.
    NSRect target = welcome ? welcome.frame : screen.visibleFrame;
    target.origin.x -= area.origin.x;
    target.origin.y -= area.origin.y;
    CALayer* clip = [CALayer layer];
    clip.frame = target;
    clip.cornerRadius = kWindowRadius;
    [root addSublayer:clip];
    cocoa_->clip = clip;

    // The blob sits at the window's centre, which on first run is the
    // display's, whatever the pointer does.
    const CGFloat width = NSWidth(screen.frame) * kBlobShare;
    const CGFloat side = width * 1.8;
    const CGPoint at = CGPointMake(NSWidth(target) / 2, NSHeight(target) / 2);
    CALayer* blob = [CALayer layer];
    blob.bounds = CGRectMake(0, 0, side, side);
    blob.position = at;

    // One body in the site blue, cut to the blurred, morphing outline.
    const CGPoint middle = CGPointMake(side / 2, side / 2);
    NSArray* frames = BlurredOutlines(side, width / 2, width * kBlurShare);
    CALayer* body = [CALayer layer];
    body.frame = blob.bounds;
    body.backgroundColor = (__bridge CGColorRef)Color(kSiteBlue, 1);
    CALayer* outline = [CALayer layer];
    outline.frame = body.bounds;
    outline.contents = frames.firstObject;
    if (frames.count) {
      [outline addAnimation:Morph(frames) forKey:@"morph"];
    }
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

    // It pops in: a quick fade and a spring from 60% with a little
    // overshoot.
    [blob addAnimation:From(@"opacity", @0, kBlobIn,
                            kCAMediaTimingFunctionEaseOut)
                forKey:@"in"];
    [blob addAnimation:Spring(@"transform.scale", @0.6, kPopStiffness,
                              kPopDamping)
                forKey:@"pop"];
    [clip addSublayer:blob];
    cocoa_->blob = blob;

    CALayer* field = Field(clip.bounds.size, base);
    field.opacity = 0;
    [clip addSublayer:field];
    cocoa_->field = field;
  }
  [CATransaction commit];
  if (welcome) {
    cocoa_->welcome_alpha = welcome.alphaValue;
    welcome.alphaValue = 0;
    cocoa_->buttons = Buttons(welcome);
    for (NSButton* button in cocoa_->buttons) {
      button.alphaValue = 0;
    }
  }
  Log("set up");
  // t = 0: the dim and the blob show from here.
  start_ = base::TimeTicks::Now();
  // Front without activating the app, so a background launch shows it too.
  for (NSWindow* dim in cocoa_->dims) {
    [dim orderFrontRegardless];
  }
  Log("dim");
  At(welcome::kGrowStart, &WelcomeIntro::Grow);
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
  // Ended early, by a press or a close: the traffic lights show at once. A
  // fade already under way finishes on its own.
  if (!cocoa_->buttons_shown) {
    for (NSButton* button in cocoa_->buttons) {
      button.alphaValue = 1;
    }
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
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    // The blob swells unevenly past the window's edges, still morphing, and
    // the window's rounded frame clips it.
    clip.masksToBounds = YES;
    const CATransform3D swell = CATransform3DMakeScale(
        frame.size.width / side * 2.1, frame.size.height / side * 2.6, 1);
    blob.position = center;
    blob.transform = swell;
    // A spring, so the swell overshoots a little and settles.
    [blob addAnimation:Spring(@"position", [NSValue valueWithPoint:from],
                              kGrowStiffness, kGrowDamping)
                forKey:@"move"];
    [blob addAnimation:Spring(@"transform",
                              [NSValue
                                  valueWithCATransform3D:CATransform3DIdentity],
                              kGrowStiffness, kGrowDamping)
                forKey:@"grow"];
    // Its highlights smear into the window's background.
    CALayer* field = cocoa_->field;
    field.opacity = 1;
    CAKeyframeAnimation* show =
        [CAKeyframeAnimation animationWithKeyPath:@"opacity"];
    show.values = @[ @0, @0.15, @1 ];
    show.keyTimes = @[ @0, @0.5, @1 ];
    show.duration = Seconds(welcome::kGrowLands - welcome::kGrowStart);
    [field addAnimation:show forKey:@"grow"];
    [CATransaction commit];
  }
  At(welcome::kGrowLands, &WelcomeIntro::Lift);
}

void WelcomeIntro::Lift() {
  Log("lift");
  // The window shows under the clipped field as the dim lifts. It shows the
  // same picture, so the field stays opaque until the window has drawn: a
  // crossfade of two copies would dip in brightness halfway.
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
  const base::TimeDelta lift = welcome::kDimGone - welcome::kGrowLands;
  for (CALayer* shade in cocoa_->shades) {
    shade.opacity = 0;
    [shade addAnimation:From(@"opacity", @(kDim), lift,
                             kCAMediaTimingFunctionEaseOut)
                 forKey:@"out"];
  }
  // The main display's shade no longer covers the window's frame, which the
  // field covers until the window has drawn.
  CALayer* shade = cocoa_->home_shade;
  CALayer* clip = cocoa_->clip;
  if (shade && clip) {
    CAShapeLayer* hole = [CAShapeLayer layer];
    hole.frame = shade.bounds;
    CGMutablePathRef path = CGPathCreateMutable();
    CGPathAddRect(path, nullptr, shade.bounds);
    CGPathAddRoundedRect(path, nullptr, clip.frame, kWindowRadius,
                         kWindowRadius);
    hole.path = path;
    CGPathRelease(path);
    hole.fillRule = kCAFillRuleEvenOdd;
    shade.mask = hole;
  }
  [CATransaction commit];
  At(welcome::kWindowShown, &WelcomeIntro::Shown);
}

void WelcomeIntro::Shown() {
  Log("shown");
  // The window has drawn under the field, so the field can go.
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  cocoa_->clip.hidden = YES;
  [CATransaction commit];
  At(welcome::kDimGone, &WelcomeIntro::Finish);
}

void WelcomeIntro::At(base::TimeDelta at, void (WelcomeIntro::*phase)()) {
  timer_.Start(
      FROM_HERE,
      std::max(base::TimeDelta(), at - (base::TimeTicks::Now() - start_)),
      base::BindOnce(phase, base::Unretained(this)));
}

void WelcomeIntro::Finish() {
  Log("done");
  // The traffic lights fade in on their own. Core Animation owns the fade,
  // so it finishes even once the intro is gone.
  cocoa_->buttons_shown = true;
  NSArray<NSButton*>* buttons = cocoa_->buttons;
  [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
    context.duration = Seconds(welcome::kTrafficLightsFade);
    context.timingFunction =
        [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
    for (NSButton* button in buttons) {
      button.animator.alphaValue = 1;
    }
  }];
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
