#include "myarc/updater_mac.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "myarc/glass_mac.h"
#include "myarc/release_buildflags.h"
#include "myarc/test_buildflags.h"
#include "myarc/welcome_mark.h"

// Sparkle.framework is embedded by tools/package_release.sh, not by the build,
// so it is loaded at runtime and only these parts of its API are declared.
// Enum values match Sparkle 2.10.1's SPUUserUpdateState.h.
typedef NS_ENUM(NSInteger, MyarcSparkleUpdateChoice) {
  MyarcSparkleUpdateChoiceSkip = 0,
  MyarcSparkleUpdateChoiceInstall = 1,
  MyarcSparkleUpdateChoiceDismiss = 2,
};

typedef NS_ENUM(NSInteger, MyarcSparkleUpdateStage) {
  MyarcSparkleUpdateStageNotDownloaded = 0,
  MyarcSparkleUpdateStageDownloaded = 1,
  MyarcSparkleUpdateStageInstalling = 2,
};

@protocol MyarcSparkleAppcastItem <NSObject>
@property(nonatomic, copy, readonly) NSString* versionString;
@property(nonatomic, copy, readonly) NSString* displayVersionString;
@property(nonatomic, readonly, nullable) NSURL* infoURL;
@property(nonatomic, getter=isInformationOnlyUpdate, readonly)
    BOOL informationOnlyUpdate;
@property(nonatomic, readonly, nullable) NSURL* releaseNotesURL;
@property(nonatomic, copy, readonly, nullable) NSString* itemDescription;
@property(nonatomic, readonly, nullable) NSURL* fullReleaseNotesURL;
@property(nonatomic, getter=isCriticalUpdate, readonly) BOOL criticalUpdate;
@end

@protocol MyarcSparkleUserUpdateState <NSObject>
@property(nonatomic, readonly) MyarcSparkleUpdateStage stage;
@property(nonatomic, readonly) BOOL userInitiated;
@end

@protocol MyarcSparkleDownloadData <NSObject>
@property(nonatomic, readonly) NSData* data;
@property(nonatomic, readonly, nullable, copy) NSString* textEncodingName;
@end

@protocol MyarcSparklePermissionResponse <NSObject>
- (instancetype)initWithAutomaticUpdateChecks:(BOOL)automaticUpdateChecks
                   automaticUpdateDownloading:
                       (NSNumber* _Nullable)automaticUpdateDownloading
                            sendSystemProfile:(BOOL)sendSystemProfile;
@end

@protocol MyarcSparkleUpdater <NSObject>
- (instancetype)initWithHostBundle:(NSBundle*)hostBundle
                 applicationBundle:(NSBundle*)applicationBundle
                        userDriver:(id)userDriver
                          delegate:(id)delegate;
- (BOOL)startUpdater:(NSError* __autoreleasing*)error;
- (void)checkForUpdates;
- (void)checkForUpdatesInBackground;
@property(nonatomic, readonly) BOOL canCheckForUpdates;
@end

namespace myarc {
namespace {

#if BUILDFLAG(MYARC_TEST_HOOKS)
// Points Sparkle at a local appcast, downloads in the background and checks
// right away, so a test sees the result without any window.
constexpr char kTestUpdateFeed[] = "myarc-test-update-feed";
// Opens the frosted updater UI in a preview state for background capture.
constexpr char kCaptureUpdaterUi[] = "myarc-capture-updater-ui";
#endif

// Seconds between building the menu and starting Sparkle, so loading the
// framework and its first check stay off the startup path.
constexpr int64_t kStartDelaySeconds = 5;
constexpr CGFloat kWindowWidth = 448;
constexpr CGFloat kWindowCornerRadius = dialog::kCornerRadius;
// Bounds on untrusted release notes read from an appcast or remote URL.
constexpr size_t kMaxReleaseNotesInputBytes = 16 * 1024;
constexpr NSUInteger kMaxReleaseNotesChars = 360;

NSString* TestFeed() {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTestUpdateFeed)) {
    return base::SysUTF8ToNSString(
        command_line->GetSwitchValueASCII(kTestUpdateFeed));
  }
#endif
  return nil;
}

NSString* CurrentVersionLabel() {
  NSString* version = base::apple::ObjCCast<NSString>(
      [base::apple::OuterBundle() objectForInfoDictionaryKey:@"MyarcVersion"]);
  if (version.length) {
    return version;
  }
  return @"Development build";
}

NSString* ChromiumVersionLabel() {
  return base::apple::ObjCCast<NSString>([base::apple::OuterBundle()
      objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
}

NSString* UpToDateSubtitle() {
  NSString* version = CurrentVersionLabel();
  NSString* chromium = ChromiumVersionLabel();
  if (chromium.length) {
    return [NSString stringWithFormat:@"myArc %@ is the newest version "
                                      @"available.\nChromium %@",
                                      version, chromium];
  }
  return [NSString
      stringWithFormat:@"myArc %@ is the newest version available.", version];
}

NSString* FormatUpdateVersion(id<MyarcSparkleAppcastItem> item) {
  if (!item) {
    return @"the latest version";
  }
  NSString* label = item.displayVersionString.length ? item.displayVersionString
                                                     : item.versionString;
  if (!label.length) {
    return @"the latest version";
  }
  if ([label hasPrefix:@"myArc "]) {
    return label;
  }
  return [NSString stringWithFormat:@"myArc %@", label];
}

NSImage* UpdaterAppIcon() {
  NSData* data =
      [NSData dataWithBytesNoCopy:const_cast<uint8_t*>(kWelcomeMarkPng)
                           length:sizeof(kWelcomeMarkPng)
                     freeWhenDone:NO];
  NSImage* mark = [[NSImage alloc] initWithData:data];
  if (mark) {
    return mark;
  }
  NSString* path =
      [base::apple::OuterBundle() pathForResource:@"app" ofType:@"icns"];
  return path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
}

BOOL IsSafeExternalURL(NSURL* url) {
  if (!url) {
    return NO;
  }
  NSString* scheme = url.scheme.lowercaseString;
  return [scheme isEqualToString:@"https"] || [scheme isEqualToString:@"http"];
}

// Strips HTML tags and decodes common entities linearly with bounded length so
// untrusted appcast/release-notes text can be shown safely as plain text.
NSString* SanitizeReleaseNotes(NSString* raw) {
  if (!raw.length) {
    return nil;
  }
  NSString* bounded =
      raw.length > kMaxReleaseNotesInputBytes
          ? [raw substringToIndex:kMaxReleaseNotesInputBytes]
          : raw;
  NSMutableString* out = [NSMutableString stringWithCapacity:256];
  BOOL in_tag = NO;
  BOOL last_space = YES;
  for (NSUInteger i = 0; i < bounded.length && out.length < kMaxReleaseNotesChars + 32;
       ++i) {
    unichar ch = [bounded characterAtIndex:i];
    if (ch == '<') {
      in_tag = YES;
      // Preserve line breaks across block tags and list items.
      NSString* tail =
          [bounded substringWithRange:NSMakeRange(
                                          i, std::min<NSUInteger>(
                                                 6, bounded.length - i))]
              .lowercaseString;
      if ([tail hasPrefix:@"<br"] || [tail hasPrefix:@"<li"] ||
          [tail hasPrefix:@"</p"] || [tail hasPrefix:@"</l"] ||
          [tail hasPrefix:@"</h"] || [tail hasPrefix:@"<p"]) {
        if (out.length && ![out hasSuffix:@"\n"]) {
          [out appendString:@"\n"];
        }
        if ([tail hasPrefix:@"<li"]) {
          [out appendString:@"• "];
        }
        last_space = YES;
      }
      continue;
    }
    if (ch == '>') {
      in_tag = NO;
      continue;
    }
    if (in_tag) {
      continue;
    }
    if (ch == '\r' || ch == '\n') {
      if (out.length && ![out hasSuffix:@"\n"]) {
        [out appendString:@"\n"];
      }
      last_space = YES;
      continue;
    }
    if (ch == ' ' || ch == '\t') {
      if (!last_space) {
        [out appendString:@" "];
        last_space = YES;
      }
      continue;
    }
    [out appendFormat:@"%C", ch];
    last_space = NO;
  }

  NSDictionary<NSString*, NSString*>* entities = @{
    @"&amp;" : @"&",
    @"&lt;" : @"<",
    @"&gt;" : @">",
    @"&quot;" : @"\"",
    @"&#39;" : @"'",
    @"&apos;" : @"'",
    @"&nbsp;" : @" ",
    @"&mdash;" : @"—",
    @"&ndash;" : @"–",
    @"&hellip;" : @"…",
  };
  for (NSString* key in entities) {
    [out replaceOccurrencesOfString:key
                         withString:entities[key]
                            options:NSLiteralSearch
                              range:NSMakeRange(0, out.length)];
  }
  NSString* trimmed = [out
      stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
  if (!trimmed.length) {
    return nil;
  }
  if (trimmed.length > kMaxReleaseNotesChars) {
    trimmed = [[trimmed substringToIndex:kMaxReleaseNotesChars]
        stringByAppendingString:@"…"];
  }
  return trimmed;
}

NSString* FormatByteProgress(uint64_t received, uint64_t expected) {
  auto format = [](uint64_t bytes) -> NSString* {
    if (bytes >= 1024ULL * 1024ULL) {
      return [NSString
          stringWithFormat:@"%.1f MB",
                           static_cast<double>(bytes) / (1024.0 * 1024.0)];
    }
    if (bytes >= 1024ULL) {
      return [NSString
          stringWithFormat:@"%.0f KB", static_cast<double>(bytes) / 1024.0];
    }
    return [NSString stringWithFormat:@"%llu B", bytes];
  };
  if (expected > 0) {
    return [NSString
        stringWithFormat:@"%@ of %@", format(received), format(expected)];
  }
  if (received > 0) {
    return [NSString stringWithFormat:@"%@ downloaded", format(received)];
  }
  return @"Connecting to update server…";
}

}  // namespace
}  // namespace myarc

// Frosted glass backdrop layer with subtle ribbon glow, top sheen, and 1px
// specular hairline (falling back to crisp opaque dark fill under Reduce
// Transparency).
@interface MyarcUpdaterCanvasView : NSView
@end

@implementation MyarcUpdaterCanvasView

- (BOOL)isFlipped {
  return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
  const BOOL glass = !myarc::ShouldReduceTransparency();
  const NSRect bounds = self.bounds;
  NSBezierPath* outer =
      [NSBezierPath bezierPathWithRoundedRect:bounds
                                      xRadius:myarc::kWindowCornerRadius
                                      yRadius:myarc::kWindowCornerRadius];
  [NSGraphicsContext saveGraphicsState];
  [outer addClip];

  if (glass) {
    [[NSColor colorWithSRGBRed:myarc::dialog::kDarkGlassRed
                         green:myarc::dialog::kDarkGlassGreen
                          blue:myarc::dialog::kDarkGlassBlue
                         alpha:myarc::dialog::kDarkGlassAlpha] setFill];
    NSRectFillUsingOperation(bounds, NSCompositingOperationSourceOver);

    // Subtle ribbon-tinted ambient glow behind the app mark.
    NSGradient* blueGlow = [[NSGradient alloc]
        initWithStartingColor:[NSColor colorWithSRGBRed:0.20
                                                  green:0.44
                                                   blue:1.00
                                                  alpha:0.18]
                  endingColor:[NSColor colorWithSRGBRed:0.20
                                                  green:0.44
                                                   blue:1.00
                                                  alpha:0.0]];
    [blueGlow drawFromCenter:NSMakePoint(56, 72)
                      radius:0
                    toCenter:NSMakePoint(56, 72)
                      radius:150
                     options:0];

    NSGradient* violetGlow = [[NSGradient alloc]
        initWithStartingColor:[NSColor colorWithSRGBRed:0.62
                                                  green:0.32
                                                   blue:1.00
                                                  alpha:0.12]
                  endingColor:[NSColor colorWithSRGBRed:0.62
                                                  green:0.32
                                                   blue:1.00
                                                  alpha:0.0]];
    [violetGlow drawFromCenter:NSMakePoint(126, 42)
                        radius:0
                      toCenter:NSMakePoint(126, 42)
                        radius:140
                       options:0];

    // Top glass sheen.
    NSGradient* sheen = [[NSGradient alloc]
        initWithStartingColor:[NSColor colorWithWhite:1.0 alpha:0.07]
                  endingColor:[NSColor colorWithWhite:1.0 alpha:0.0]];
    [sheen drawInRect:NSMakeRect(0, 0, NSWidth(bounds), 76) angle:90];
  } else {
    [[NSColor colorWithSRGBRed:0x22 / 255.0
                         green:0x22 / 255.0
                          blue:0x24 / 255.0
                         alpha:1.0] setFill];
    NSRectFill(bounds);
  }

  [NSGraphicsContext restoreGraphicsState];

  // Crisp 1px outer specular hairline.
  NSRect inset = NSInsetRect(bounds, 0.5, 0.5);
  NSBezierPath* rim =
      [NSBezierPath bezierPathWithRoundedRect:inset
                                      xRadius:myarc::kWindowCornerRadius - 0.5
                                      yRadius:myarc::kWindowCornerRadius - 0.5];
  rim.lineWidth = 1.0;
  if (glass) {
    [[NSColor colorWithWhite:1.0 alpha:myarc::dialog::kDarkRimAlpha] setStroke];
  } else {
    [[NSColor colorWithSRGBRed:0x3A / 255.0
                         green:0x3A / 255.0
                          blue:0x3D / 255.0
                         alpha:1.0] setStroke];
  }
  [rim stroke];
}

@end

// Translucent inset box for release notes summary.
@interface MyarcUpdaterNotesCard : NSView
@end

@implementation MyarcUpdaterNotesCard

- (BOOL)isFlipped {
  return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
  const BOOL glass = !myarc::ShouldReduceTransparency();
  const NSRect bounds = self.bounds;
  NSBezierPath* bg = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                     xRadius:8
                                                     yRadius:8];
  [(glass ? [NSColor colorWithWhite:1.0 alpha:0.06]
          : [NSColor colorWithSRGBRed:0x2A / 255.0
                                green:0x2A / 255.0
                                 blue:0x2E / 255.0
                                alpha:1.0]) setFill];
  [bg fill];

  NSRect inset = NSInsetRect(bounds, 0.5, 0.5);
  NSBezierPath* rim = [NSBezierPath bezierPathWithRoundedRect:inset
                                                      xRadius:7.5
                                                      yRadius:7.5];
  rim.lineWidth = 1.0;
  [[NSColor colorWithWhite:1.0 alpha:glass ? 0.14 : 0.10] setStroke];
  [rim stroke];
}

@end

// Sleek rounded pill progress bar that stays crisp on dark frosted glass and
// supports both determinate progress and smooth indeterminate animation.
@interface MyarcUpdaterProgressBar : NSView
@property(nonatomic) double progress;
@property(nonatomic) BOOL indeterminate;
@end

@implementation MyarcUpdaterProgressBar {
  NSTimer* _timer;
  CGFloat _phase;
}

@synthesize progress = _progress;
@synthesize indeterminate = _indeterminate;

- (void)dealloc {
  [_timer invalidate];
}

- (void)setProgress:(double)progress {
  _progress = std::clamp(progress, 0.0, 1.0);
  [self setNeedsDisplay:YES];
}

- (void)setIndeterminate:(BOOL)indeterminate {
  if (_indeterminate == indeterminate) {
    return;
  }
  _indeterminate = indeterminate;
  [_timer invalidate];
  _timer = nil;
  if (indeterminate) {
    _phase = 0.0;
    __weak MyarcUpdaterProgressBar* weakSelf = self;
    _timer = [NSTimer
        scheduledTimerWithTimeInterval:1.0 / 30.0
                               repeats:YES
                                 block:^(NSTimer* timer) {
                                   MyarcUpdaterProgressBar* strong = weakSelf;
                                   if (!strong) {
                                     [timer invalidate];
                                     return;
                                   }
                                   strong->_phase += 0.025;
                                   if (strong->_phase > 1.35) {
                                     strong->_phase = -0.35;
                                   }
                                   [strong setNeedsDisplay:YES];
                                 }];
  }
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  const NSRect bounds = self.bounds;
  const CGFloat radius = NSHeight(bounds) / 2.0;
  NSBezierPath* track = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                        xRadius:radius
                                                        yRadius:radius];
  [[NSColor colorWithWhite:1.0 alpha:0.14] setFill];
  [track fill];

  [NSGraphicsContext saveGraphicsState];
  [track addClip];

  NSRect fillRect = NSZeroRect;
  if (_indeterminate) {
    const CGFloat barWidth = NSWidth(bounds) * 0.34;
    const CGFloat x = _phase * NSWidth(bounds);
    fillRect = NSMakeRect(x, 0, barWidth, NSHeight(bounds));
  } else if (_progress > 0.0) {
    const CGFloat w =
        std::max<CGFloat>(NSHeight(bounds), NSWidth(bounds) * _progress);
    fillRect = NSMakeRect(0, 0, w, NSHeight(bounds));
  }

  if (!NSIsEmptyRect(fillRect)) {
    NSBezierPath* bar = [NSBezierPath bezierPathWithRoundedRect:fillRect
                                                        xRadius:radius
                                                        yRadius:radius];
    NSGradient* grad = [[NSGradient alloc]
        initWithStartingColor:[NSColor colorWithSRGBRed:0x0A / 255.0
                                                  green:0x84 / 255.0
                                                   blue:1.0
                                                  alpha:1.0]
                  endingColor:[NSColor colorWithSRGBRed:0x4D / 255.0
                                                  green:0xA3 / 255.0
                                                   blue:1.0
                                                  alpha:1.0]];
    [grad drawInBezierPath:bar angle:0];
  }
  [NSGraphicsContext restoreGraphicsState];
}

@end

typedef NS_ENUM(NSInteger, MyarcUpdaterButtonStyle) {
  MyarcUpdaterButtonStylePrimary,
  MyarcUpdaterButtonStyleSecondary,
  MyarcUpdaterButtonStyleLink,
};

// Pill push button matching Settings > Advanced's primary blue & glass buttons.
@interface MyarcUpdaterButton : NSButton
@property(nonatomic) MyarcUpdaterButtonStyle pillStyle;
@property(nonatomic, strong) NSImage* leadingIcon;
@end

@implementation MyarcUpdaterButton {
  BOOL _hovered;
  NSTrackingArea* _tracking;
}

@synthesize pillStyle = _pillStyle;
@synthesize leadingIcon = _leadingIcon;

- (instancetype)initWithStyle:(MyarcUpdaterButtonStyle)style {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    _pillStyle = style;
    self.bordered = NO;
    self.bezelStyle = NSBezelStyleInline;
    self.focusRingType = NSFocusRingTypeExterior;
  }
  return self;
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if (_tracking) {
    [self removeTrackingArea:_tracking];
  }
  _tracking = [[NSTrackingArea alloc]
      initWithRect:self.bounds
           options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
             owner:self
          userInfo:nil];
  [self addTrackingArea:_tracking];
}

- (void)mouseEntered:(NSEvent*)event {
  _hovered = YES;
  [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
  _hovered = NO;
  [self setNeedsDisplay:YES];
}

- (NSSize)intrinsicContentSize {
  NSFont* font = [self labelFont];
  NSDictionary* attrs = @{NSFontAttributeName : font};
  NSSize textSize = [self.title sizeWithAttributes:attrs];
  if (_pillStyle == MyarcUpdaterButtonStyleLink) {
    return NSMakeSize(std::ceil(textSize.width) + 4, 24);
  }
  const CGFloat iconExtra = _leadingIcon ? 22.0 : 0.0;
  return NSMakeSize(
      std::max<CGFloat>(74, std::ceil(textSize.width) + iconExtra + 28), 36);
}

- (NSFont*)labelFont {
  if (_pillStyle == MyarcUpdaterButtonStyleLink) {
    return [NSFont systemFontOfSize:12 weight:NSFontWeightMedium];
  }
  if (_pillStyle == MyarcUpdaterButtonStylePrimary) {
    return [NSFont systemFontOfSize:14 weight:NSFontWeightSemibold];
  }
  return [NSFont systemFontOfSize:14 weight:NSFontWeightMedium];
}

- (void)drawRect:(NSRect)dirtyRect {
  const BOOL glass = !myarc::ShouldReduceTransparency();
  const BOOL pressed = self.cell.isHighlighted;
  const NSRect bounds = self.bounds;

  if (_pillStyle != MyarcUpdaterButtonStyleLink) {
    const CGFloat radius = myarc::dialog::kButtonCornerRadius;
    NSBezierPath* pill = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                         xRadius:radius
                                                         yRadius:radius];
    NSColor* fill = nil;
    NSColor* stroke = nil;
    if (_pillStyle == MyarcUpdaterButtonStylePrimary) {
      fill = [NSColor colorWithSRGBRed:0x0A / 255.0
                                 green:0x84 / 255.0
                                  blue:1.0
                                 alpha:pressed ? 0.78 : (_hovered ? 0.94 : 1.0)];
      stroke = [NSColor colorWithWhite:1.0 alpha:0.26];
    } else if (glass) {
      fill = [NSColor colorWithWhite:1.0
                               alpha:pressed ? 0.24 : (_hovered ? 0.20 : 0.14)];
      stroke = [NSColor colorWithWhite:1.0 alpha:0.18];
    } else {
      fill = [NSColor colorWithSRGBRed:0x3E / 255.0
                                 green:0x3E / 255.0
                                  blue:0x42 / 255.0
                                 alpha:pressed ? 0.8 : 1.0];
      stroke = [NSColor colorWithWhite:1.0 alpha:0.14];
    }
    [fill setFill];
    [pill fill];

    NSRect inset = NSInsetRect(bounds, 0.5, 0.5);
    NSBezierPath* rim = [NSBezierPath bezierPathWithRoundedRect:inset
                                                        xRadius:radius - 0.5
                                                        yRadius:radius - 0.5];
    rim.lineWidth = 1.0;
    [stroke setStroke];
    [rim stroke];
  }

  NSColor* textColor = nil;
  if (_pillStyle == MyarcUpdaterButtonStylePrimary) {
    textColor = NSColor.whiteColor;
  } else if (_pillStyle == MyarcUpdaterButtonStyleLink) {
    textColor = _hovered ? [NSColor colorWithSRGBRed:0x74 / 255.0
                                               green:0xB8 / 255.0
                                                blue:1.0
                                               alpha:1.0]
                         : [NSColor colorWithSRGBRed:0x4D / 255.0
                                               green:0xA3 / 255.0
                                                blue:1.0
                                               alpha:1.0];
  } else {
    textColor = [NSColor colorWithSRGBRed:0xF0 / 255.0
                                    green:0xF0 / 255.0
                                     blue:0xF4 / 255.0
                                    alpha:1.0];
  }
  if (!self.enabled) {
    textColor = [textColor colorWithAlphaComponent:0.45];
  }

  NSMutableParagraphStyle* para = [[NSMutableParagraphStyle alloc] init];
  para.alignment = (_pillStyle == MyarcUpdaterButtonStyleLink || _leadingIcon)
                       ? NSTextAlignmentLeft
                       : NSTextAlignmentCenter;
  NSDictionary* attrs = @{
    NSFontAttributeName : [self labelFont],
    NSForegroundColorAttributeName : textColor,
    NSParagraphStyleAttributeName : para,
  };
  NSSize textSize = [self.title sizeWithAttributes:attrs];
  if (_leadingIcon) {
    constexpr CGFloat kIconSize = 15.0;
    constexpr CGFloat kGap = 7.0;
    const CGFloat totalW = kIconSize + kGap + std::ceil(textSize.width);
    const CGFloat startX = std::floor((NSWidth(bounds) - totalW) / 2.0);
    const CGFloat iconY = std::floor((NSHeight(bounds) - kIconSize) / 2.0);
    [_leadingIcon drawInRect:NSMakeRect(startX, iconY, kIconSize, kIconSize)
                    fromRect:NSZeroRect
                   operation:NSCompositingOperationSourceOver
                    fraction:self.enabled ? 0.90 : 0.45
              respectFlipped:YES
                       hints:nil];
    NSRect textRect = NSMakeRect(
        startX + kIconSize + kGap,
        std::floor((NSHeight(bounds) - textSize.height) / 2.0) - 0.5,
        std::ceil(textSize.width) + 2, textSize.height + 2);
    [self.title drawInRect:textRect withAttributes:attrs];
  } else {
    NSRect textRect = NSMakeRect(
        0, std::floor((NSHeight(bounds) - textSize.height) / 2.0) - 0.5,
        NSWidth(bounds), textSize.height + 2);
    [self.title drawInRect:textRect withAttributes:attrs];
  }
}

@end

// Custom Sparkle 2 SPUUserDriver backed by a frosted-glass AppKit window.
@interface MyarcSparkleUserDriver : NSObject <NSWindowDelegate>
- (void)showPreviewState:(NSString*)state;
@end

@implementation MyarcSparkleUserDriver {
  NSWindow* _window;
  MyarcUpdaterCanvasView* _canvas;
  NSImageView* _iconView;
  NSTextField* _titleField;
  NSTextField* _subtitleField;
  MyarcUpdaterNotesCard* _notesCard;
  NSTextField* _notesField;
  MyarcUpdaterButton* _notesLinkButton;
  MyarcUpdaterProgressBar* _progressBar;
  MyarcUpdaterButton* _primaryButton;
  MyarcUpdaterButton* _secondaryButton;
  MyarcUpdaterButton* _skipButton;

  // Pending Sparkle replies/callbacks. Each is invoked at most once.
  void (^_choiceReply)(MyarcSparkleUpdateChoice);
  void (^_acknowledgement)(void);
  void (^_cancellation)(void);
  void (^_retryTermination)(void);
  BOOL _dismissingProgrammatically;

  // State tracked across Sparkle callbacks in one update session.
  NSString* _pendingVersionLabel;
  NSURL* _releaseNotesURL;
  NSString* _releaseNotesSummary;
  BOOL _infoOnlyUpdate;
  uint64_t _expectedBytes;
  uint64_t _receivedBytes;
}

- (NSTextField*)makeLabelWithSize:(CGFloat)size
                           weight:(NSFontWeight)weight
                            color:(NSColor*)color {
  NSTextField* label = [NSTextField labelWithString:@""];
  label.font = [NSFont systemFontOfSize:size weight:weight];
  label.textColor = color;
  label.backgroundColor = NSColor.clearColor;
  label.drawsBackground = NO;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.maximumNumberOfLines = 0;
  label.usesSingleLineMode = NO;
  label.selectable = NO;
  return label;
}

- (void)ensureWindow {
  if (_window) {
    myarc::ApplyNSWindowGlass(_window, myarc::GlassMaterial::kHud,
                              myarc::kWindowCornerRadius, /*dark=*/true);
    return;
  }

  NSRect rect = NSMakeRect(0, 0, myarc::kWindowWidth, 212);
  NSWindowStyleMask style = NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskFullSizeContentView;
  _window = [[NSWindow alloc] initWithContentRect:rect
                                        styleMask:style
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  _window.title = @"Software Update";
  _window.titlebarAppearsTransparent = YES;
  _window.titleVisibility = NSWindowTitleHidden;
  _window.movableByWindowBackground = YES;
  _window.releasedWhenClosed = NO;
  _window.delegate = self;
  [[_window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
  [[_window standardWindowButton:NSWindowZoomButton] setHidden:YES];

  NSView* root = [[NSView alloc] initWithFrame:rect];
  root.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  _window.contentView = root;

  myarc::ApplyNSWindowGlass(_window, myarc::GlassMaterial::kHud,
                            myarc::kWindowCornerRadius, /*dark=*/true);

  _canvas = [[MyarcUpdaterCanvasView alloc] initWithFrame:rect];
  _canvas.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [root addSubview:_canvas];

  _iconView = [[NSImageView alloc] initWithFrame:NSMakeRect(24, 34, 58, 58)];
  _iconView.image = myarc::UpdaterAppIcon();
  _iconView.imageScaling = NSImageScaleProportionallyUpOrDown;
  _iconView.wantsLayer = YES;
  _iconView.layer.shadowColor = NSColor.blackColor.CGColor;
  _iconView.layer.shadowOpacity = 0.35f;
  _iconView.layer.shadowRadius = 8.0f;
  _iconView.layer.shadowOffset = CGSizeMake(0, -3);
  [_canvas addSubview:_iconView];

  _titleField = [self
      makeLabelWithSize:15
                 weight:NSFontWeightSemibold
                  color:[NSColor colorWithSRGBRed:0xF5 / 255.0
                                            green:0xF5 / 255.0
                                             blue:0xF7 / 255.0
                                            alpha:1.0]];
  [_canvas addSubview:_titleField];

  _subtitleField = [self
      makeLabelWithSize:12.5
                 weight:NSFontWeightRegular
                  color:[NSColor colorWithSRGBRed:0xBA / 255.0
                                            green:0xBA / 255.0
                                             blue:0xC2 / 255.0
                                            alpha:1.0]];
  [_canvas addSubview:_subtitleField];

  _notesCard = [[MyarcUpdaterNotesCard alloc] initWithFrame:NSZeroRect];
  _notesField = [self
      makeLabelWithSize:11.5
                 weight:NSFontWeightRegular
                  color:[NSColor colorWithSRGBRed:0xD6 / 255.0
                                            green:0xD6 / 255.0
                                             blue:0xDC / 255.0
                                            alpha:1.0]];
  [_notesCard addSubview:_notesField];
  [_canvas addSubview:_notesCard];

  _notesLinkButton =
      [[MyarcUpdaterButton alloc] initWithStyle:MyarcUpdaterButtonStyleLink];
  _notesLinkButton.title = @"View release notes ↗";
  _notesLinkButton.target = self;
  _notesLinkButton.action = @selector(openReleaseNotes:);
  [_canvas addSubview:_notesLinkButton];

  _progressBar = [[MyarcUpdaterProgressBar alloc] initWithFrame:NSZeroRect];
  [_canvas addSubview:_progressBar];

  _primaryButton =
      [[MyarcUpdaterButton alloc] initWithStyle:MyarcUpdaterButtonStylePrimary];
  _primaryButton.target = self;
  _primaryButton.action = @selector(primaryPressed:);
  _primaryButton.keyEquivalent = @"\r";
  [_canvas addSubview:_primaryButton];

  _secondaryButton = [[MyarcUpdaterButton alloc]
      initWithStyle:MyarcUpdaterButtonStyleSecondary];
  _secondaryButton.target = self;
  _secondaryButton.action = @selector(secondaryPressed:);
  _secondaryButton.keyEquivalent = @"\033";
  [_canvas addSubview:_secondaryButton];

  _skipButton =
      [[MyarcUpdaterButton alloc] initWithStyle:MyarcUpdaterButtonStyleLink];
  _skipButton.title = @"Skip This Version";
  _skipButton.target = self;
  _skipButton.action = @selector(skipPressed:);
  [_canvas addSubview:_skipButton];

  [_window center];
}

- (void)layoutWindow {
  const CGFloat textX = 98;
  const CGFloat rightPad = 24;
  const CGFloat textW = myarc::kWindowWidth - textX - rightPad;

  CGFloat y = 34;
  NSSize titleSize =
      [_titleField sizeThatFits:NSMakeSize(textW, CGFLOAT_MAX)];
  _titleField.frame =
      NSMakeRect(textX, y, textW, std::ceil(titleSize.height) + 2);
  y = NSMaxY(_titleField.frame) + 5;

  if (!_subtitleField.hidden && _subtitleField.stringValue.length) {
    NSSize subSize =
        [_subtitleField sizeThatFits:NSMakeSize(textW, CGFLOAT_MAX)];
    _subtitleField.frame =
        NSMakeRect(textX, y, textW, std::ceil(subSize.height) + 2);
    y = NSMaxY(_subtitleField.frame) + 12;
  } else {
    y += 6;
  }

  if (!_progressBar.hidden) {
    _progressBar.frame = NSMakeRect(textX, y, textW, 6);
    y = NSMaxY(_progressBar.frame) + 16;
  }

  if (!_notesCard.hidden) {
    const CGFloat cardPadX = 12;
    const CGFloat cardPadY = 10;
    NSSize notesSize = [_notesField
        sizeThatFits:NSMakeSize(textW - cardPadX * 2, CGFLOAT_MAX)];
    const CGFloat textH =
        std::min<CGFloat>(88, std::ceil(notesSize.height) + 2);
    const CGFloat cardH = textH + cardPadY * 2;
    _notesCard.frame = NSMakeRect(textX, y, textW, cardH);
    _notesField.frame =
        NSMakeRect(cardPadX, cardPadY, textW - cardPadX * 2, textH);
    y = NSMaxY(_notesCard.frame) + 8;
  }

  if (!_notesLinkButton.hidden) {
    NSSize linkSize = _notesLinkButton.intrinsicContentSize;
    _notesLinkButton.frame =
        NSMakeRect(textX, y, linkSize.width, linkSize.height);
    y = NSMaxY(_notesLinkButton.frame) + 12;
  }

  // Ensure the icon column never collides with the button strip.
  const CGFloat buttonsY = std::max<CGFloat>(y + 8, 122);
  const CGFloat buttonH = 30;
  CGFloat rightX = myarc::kWindowWidth - rightPad;

  if (!_primaryButton.hidden) {
    NSSize size = _primaryButton.intrinsicContentSize;
    rightX -= size.width;
    _primaryButton.frame = NSMakeRect(rightX, buttonsY, size.width, buttonH);
    rightX -= 10;
  }
  if (!_secondaryButton.hidden) {
    NSSize size = _secondaryButton.intrinsicContentSize;
    rightX -= size.width;
    _secondaryButton.frame = NSMakeRect(rightX, buttonsY, size.width, buttonH);
  }
  if (!_skipButton.hidden) {
    NSSize size = _skipButton.intrinsicContentSize;
    _skipButton.frame =
        NSMakeRect(24, buttonsY + 3, size.width, size.height);
  }

  const CGFloat totalH = buttonsY + buttonH + 22;
  NSRect frame = _window.frame;
  const CGFloat deltaH = totalH - NSHeight(frame);
  if (std::abs(deltaH) > 0.5) {
    frame.origin.y -= deltaH;
    frame.size.height = totalH;
    [_window setFrame:frame display:YES];
  }
  [_canvas setNeedsDisplay:YES];
}

- (void)presentWindow {
  [self layoutWindow];
  if (NSApp.active) {
    [_window makeKeyAndOrderFront:nil];
  } else {
    // Keep background launches and captures from stealing focus.
    [_window orderFront:nil];
  }
}

- (void)clearCallbacks {
  _choiceReply = nil;
  _acknowledgement = nil;
  _cancellation = nil;
  _retryTermination = nil;
}

- (void)closeWindowSilently {
  [self clearCallbacks];
  _progressBar.indeterminate = NO;
  if (_window && _window.visible) {
    _dismissingProgrammatically = YES;
    [_window orderOut:nil];
    _dismissingProgrammatically = NO;
  }
}

- (void)configureTitle:(NSString*)title
              subtitle:(NSString*)subtitle
          showProgress:(BOOL)showProgress
         indeterminate:(BOOL)indeterminate
              progress:(double)progress
                 notes:(NSString*)notes
              notesURL:(NSURL*)notesURL
          primaryTitle:(NSString*)primaryTitle
        secondaryTitle:(NSString*)secondaryTitle
              showSkip:(BOOL)showSkip {
  [self ensureWindow];
  _titleField.stringValue = title ?: @"";
  _subtitleField.stringValue = subtitle ?: @"";
  _subtitleField.hidden = (subtitle.length == 0);

  _progressBar.hidden = !showProgress;
  _progressBar.indeterminate = showProgress && indeterminate;
  if (showProgress && !indeterminate) {
    _progressBar.progress = progress;
  }

  _notesField.stringValue = notes ?: @"";
  _notesCard.hidden = (notes.length == 0);
  _notesLinkButton.hidden = !myarc::IsSafeExternalURL(notesURL);

  _primaryButton.title = primaryTitle ?: @"";
  _primaryButton.hidden = (primaryTitle.length == 0);
  _primaryButton.enabled = YES;

  _secondaryButton.title = secondaryTitle ?: @"";
  _secondaryButton.hidden = (secondaryTitle.length == 0);
  _secondaryButton.enabled = YES;

  _skipButton.hidden = !showSkip;
}

// Button actions.
- (void)primaryPressed:(id)sender {
  if (_infoOnlyUpdate && myarc::IsSafeExternalURL(_releaseNotesURL)) {
    [NSWorkspace.sharedWorkspace openURL:_releaseNotesURL];
    auto reply = _choiceReply;
    [self closeWindowSilently];
    if (reply) {
      reply(MyarcSparkleUpdateChoiceDismiss);
    }
    return;
  }
  if (_choiceReply) {
    auto reply = _choiceReply;
    [self clearCallbacks];
    _primaryButton.enabled = NO;
    reply(MyarcSparkleUpdateChoiceInstall);
    return;
  }
  if (_acknowledgement) {
    auto ack = _acknowledgement;
    [self closeWindowSilently];
    ack();
    return;
  }
  if (_retryTermination) {
    _retryTermination();
    return;
  }
  [self closeWindowSilently];
}

- (void)secondaryPressed:(id)sender {
  if (_cancellation) {
    auto cancel = _cancellation;
    [self closeWindowSilently];
    cancel();
    return;
  }
  if (_choiceReply) {
    auto reply = _choiceReply;
    [self closeWindowSilently];
    reply(MyarcSparkleUpdateChoiceDismiss);
    return;
  }
  if (_acknowledgement) {
    auto ack = _acknowledgement;
    [self closeWindowSilently];
    ack();
    return;
  }
  [self closeWindowSilently];
}

- (void)skipPressed:(id)sender {
  auto reply = _choiceReply;
  [self closeWindowSilently];
  if (reply) {
    reply(MyarcSparkleUpdateChoiceSkip);
  }
}

- (void)openReleaseNotes:(id)sender {
  if (myarc::IsSafeExternalURL(_releaseNotesURL)) {
    [NSWorkspace.sharedWorkspace openURL:_releaseNotesURL];
  }
}

// NSWindowDelegate: closing via the red traffic light dismisses or cancels
// cleanly so Sparkle never hangs waiting on a reply block.
- (void)windowWillClose:(NSNotification*)notification {
  if (_dismissingProgrammatically) {
    return;
  }
  auto choice = _choiceReply;
  auto ack = _acknowledgement;
  auto cancel = _cancellation;
  [self clearCallbacks];
  _progressBar.indeterminate = NO;
  if (choice) {
    choice(MyarcSparkleUpdateChoiceDismiss);
  } else if (ack) {
    ack();
  } else if (cancel) {
    cancel();
  }
}

// --- SPUUserDriver protocol ---

- (void)showUpdatePermissionRequest:(id)request
                              reply:(void (^)(id))reply {
  // myArc sets SUEnableAutomaticChecks in Info.plist; if Sparkle still asks,
  // allow automatic checks without sending system profiles.
  Class response_class = NSClassFromString(@"SUUpdatePermissionResponse");
  if (!response_class || !reply) {
    return;
  }
  id response = [(id<MyarcSparklePermissionResponse>)[response_class alloc]
      initWithAutomaticUpdateChecks:YES
         automaticUpdateDownloading:nil
                  sendSystemProfile:NO];
  reply(response);
}

- (void)showUserInitiatedUpdateCheckWithCancellation:
    (void (^)(void))cancellation {
  [self clearCallbacks];
  _cancellation = [cancellation copy];
  _expectedBytes = 0;
  _receivedBytes = 0;
  [self configureTitle:@"Checking for Updates…"
              subtitle:[NSString stringWithFormat:@"Current version: %@",
                                                  myarc::CurrentVersionLabel()]
          showProgress:YES
         indeterminate:YES
              progress:0
                 notes:nil
              notesURL:nil
          primaryTitle:nil
        secondaryTitle:@"Cancel"
              showSkip:NO];
  [self presentWindow];
}

- (void)showUpdateFoundWithAppcastItem:(id<MyarcSparkleAppcastItem>)appcastItem
                                 state:(id<MyarcSparkleUserUpdateState>)state
                                 reply:(void (^)(MyarcSparkleUpdateChoice))reply {
  const BOOL userInitiated = state && state.userInitiated;
  const MyarcSparkleUpdateStage stage =
      state ? state.stage : MyarcSparkleUpdateStageNotDownloaded;

  // Automated test feeds stay completely non-interactive.
  if (myarc::TestFeed() != nil) {
    if (reply) {
      reply(MyarcSparkleUpdateChoiceDismiss);
    }
    return;
  }

  // Keep background automatic checks silent until an update is ready to
  // install, or until the user clicks "Check for Updates…".
  if (!userInitiated && stage == MyarcSparkleUpdateStageNotDownloaded) {
    if (reply) {
      reply(MyarcSparkleUpdateChoiceDismiss);
    }
    return;
  }

  [self clearCallbacks];
  _choiceReply = [reply copy];
  _pendingVersionLabel = myarc::FormatUpdateVersion(appcastItem);
  _infoOnlyUpdate = appcastItem && appcastItem.informationOnlyUpdate;
  _releaseNotesURL =
      appcastItem.releaseNotesURL ?: appcastItem.infoURL;
  _releaseNotesSummary = myarc::SanitizeReleaseNotes(appcastItem.itemDescription);

  const BOOL ready = (stage == MyarcSparkleUpdateStageInstalling);
  NSString* title =
      ready ? @"Update Ready to Install"
            : [NSString stringWithFormat:@"%@ is available",
                                         _pendingVersionLabel];
  NSString* subtitle =
      ready ? [NSString stringWithFormat:
                            @"%@ has been downloaded and verified. Relaunch "
                            @"myArc to finish updating.",
                            _pendingVersionLabel]
            : [NSString stringWithFormat:
                            @"You’re currently on %@. Install the latest "
                            @"version to get recent fixes and improvements.",
                            myarc::CurrentVersionLabel()];
  NSString* primary =
      _infoOnlyUpdate ? @"Learn More"
                      : (ready ? @"Install and Relaunch" : @"Install Update");
  BOOL showSkip =
      !ready && !_infoOnlyUpdate && !(appcastItem && appcastItem.criticalUpdate);

  [self configureTitle:title
              subtitle:subtitle
          showProgress:NO
         indeterminate:NO
              progress:0
                 notes:_releaseNotesSummary
              notesURL:_releaseNotesURL
          primaryTitle:primary
        secondaryTitle:@"Later"
              showSkip:showSkip];
  [self presentWindow];
}

- (void)showUpdateReleaseNotesWithDownloadData:
    (id<MyarcSparkleDownloadData>)downloadData {
  if (!downloadData || !downloadData.data.length || !_window ||
      !_window.visible) {
    return;
  }
  NSData* slice =
      downloadData.data.length > myarc::kMaxReleaseNotesInputBytes
          ? [downloadData.data
                subdataWithRange:NSMakeRange(
                                     0, myarc::kMaxReleaseNotesInputBytes)]
          : downloadData.data;
  NSString* text = [[NSString alloc] initWithData:slice
                                         encoding:NSUTF8StringEncoding];
  NSString* summary = myarc::SanitizeReleaseNotes(text);
  if (!summary.length) {
    return;
  }
  _releaseNotesSummary = summary;
  _notesField.stringValue = summary;
  _notesCard.hidden = NO;
  [self layoutWindow];
}

- (void)showUpdateReleaseNotesFailedToDownloadWithError:(NSError*)error {
  // The release notes link remains clickable if available; no modal error.
}

- (void)showUpdateNotFoundWithError:(NSError*)error
                    acknowledgement:(void (^)(void))acknowledgement {
  if (myarc::TestFeed() != nil) {
    if (acknowledgement) {
      acknowledgement();
    }
    return;
  }
  [self clearCallbacks];
  _acknowledgement = [acknowledgement copy];
  [self configureTitle:@"You’re up to date"
              subtitle:myarc::UpToDateSubtitle()
          showProgress:NO
         indeterminate:NO
              progress:0
                 notes:nil
              notesURL:nil
          primaryTitle:@"OK"
        secondaryTitle:nil
              showSkip:NO];
  [self presentWindow];
}

- (void)showUpdaterError:(NSError*)error
         acknowledgement:(void (^)(void))acknowledgement {
  if (myarc::TestFeed() != nil) {
    if (acknowledgement) {
      acknowledgement();
    }
    return;
  }
  [self clearCallbacks];
  _acknowledgement = [acknowledgement copy];
  NSString* detail = error.localizedDescription ?: @"The update could not be "
                                                   @"completed.";
  if (error.localizedRecoverySuggestion.length &&
      ![detail containsString:error.localizedRecoverySuggestion]) {
    detail = [NSString stringWithFormat:@"%@ %@", detail,
                                        error.localizedRecoverySuggestion];
  }
  [self configureTitle:@"Update Error"
              subtitle:detail
          showProgress:NO
         indeterminate:NO
              progress:0
                 notes:nil
              notesURL:nil
          primaryTitle:@"OK"
        secondaryTitle:nil
              showSkip:NO];
  [self presentWindow];
}

- (void)showDownloadInitiatedWithCancellation:(void (^)(void))cancellation {
  [self clearCallbacks];
  _cancellation = [cancellation copy];
  _expectedBytes = 0;
  _receivedBytes = 0;
  [self configureTitle:@"Downloading Update…"
              subtitle:@"Connecting to update server…"
          showProgress:YES
         indeterminate:YES
              progress:0
                 notes:nil
              notesURL:nil
          primaryTitle:nil
        secondaryTitle:@"Cancel"
              showSkip:NO];
  [self presentWindow];
}

- (void)showDownloadDidReceiveExpectedContentLength:
    (uint64_t)expectedContentLength {
  _expectedBytes = expectedContentLength;
  _receivedBytes = 0;
  if (_expectedBytes > 0) {
    _progressBar.indeterminate = NO;
    _progressBar.progress = 0.0;
  }
  _subtitleField.stringValue =
      myarc::FormatByteProgress(_receivedBytes, _expectedBytes);
}

- (void)showDownloadDidReceiveDataOfLength:(uint64_t)length {
  _receivedBytes += length;
  if (_expectedBytes > 0) {
    _progressBar.indeterminate = NO;
    _progressBar.progress =
        static_cast<double>(_receivedBytes) /
        static_cast<double>(_expectedBytes);
  }
  _subtitleField.stringValue =
      myarc::FormatByteProgress(_receivedBytes, _expectedBytes);
}

- (void)showDownloadDidStartExtractingUpdate {
  [self clearCallbacks];
  [self configureTitle:@"Extracting Update…"
              subtitle:@"Verifying signature and preparing the update…"
          showProgress:YES
         indeterminate:YES
              progress:0
                 notes:nil
              notesURL:nil
          primaryTitle:nil
        secondaryTitle:nil
              showSkip:NO];
  [self presentWindow];
}

- (void)showExtractionReceivedProgress:(double)progress {
  _progressBar.indeterminate = NO;
  _progressBar.progress = progress;
  const int pct =
      static_cast<int>(std::lround(std::clamp(progress, 0.0, 1.0) * 100.0));
  _subtitleField.stringValue =
      [NSString stringWithFormat:@"Extracting update… %d%%", pct];
}

- (void)showReadyToInstallAndRelaunch:
    (void (^)(MyarcSparkleUpdateChoice))reply {
  if (myarc::TestFeed() != nil) {
    if (reply) {
      reply(MyarcSparkleUpdateChoiceDismiss);
    }
    return;
  }
  [self clearCallbacks];
  _choiceReply = [reply copy];
  NSString* version = _pendingVersionLabel.length ? _pendingVersionLabel
                                                  : @"The new version of myArc";
  [self configureTitle:@"Ready to Install"
              subtitle:[NSString stringWithFormat:
                                     @"%@ is verified and ready. Relaunch "
                                     @"myArc to finish updating.",
                                     version]
          showProgress:NO
         indeterminate:NO
              progress:0
                 notes:nil
              notesURL:nil
          primaryTitle:@"Install and Relaunch"
        secondaryTitle:@"Later"
              showSkip:NO];
  [self presentWindow];
}

- (void)showInstallingUpdateWithApplicationTerminated:
            (BOOL)applicationTerminated
                          retryTerminatingApplication:
                              (void (^)(void))retryTerminatingApplication {
  [self clearCallbacks];
  if (!applicationTerminated && retryTerminatingApplication) {
    _retryTermination = [retryTerminatingApplication copy];
  }
  [self configureTitle:@"Installing Update…"
              subtitle:applicationTerminated
                           ? @"Finishing installation…"
                           : @"Closing myArc to install the update…"
          showProgress:YES
         indeterminate:YES
              progress:0
                 notes:nil
              notesURL:nil
          primaryTitle:_retryTermination ? @"Relaunch Now" : nil
        secondaryTitle:nil
              showSkip:NO];
  [self presentWindow];
}

- (void)showUpdateInstalledAndRelaunched:(BOOL)relaunched
                         acknowledgement:(void (^)(void))acknowledgement {
  [self closeWindowSilently];
  if (acknowledgement) {
    acknowledgement();
  }
}

- (void)dismissUpdateInstallation {
  [self closeWindowSilently];
}

- (void)showUpdateInFocus {
  if (_window && _window.visible) {
    [self presentWindow];
  }
}

- (void)showPreviewState:(NSString*)state {
  [self clearCallbacks];
  _pendingVersionLabel = @"myArc 2026.11.24";
  _releaseNotesURL = [NSURL URLWithString:@"https://myarc.dev/download/"];
  _infoOnlyUpdate = NO;

  if ([state isEqualToString:@"checking"]) {
    [self configureTitle:@"Checking for Updates…"
                subtitle:[NSString stringWithFormat:@"Current version: %@",
                                                    myarc::CurrentVersionLabel()]
            showProgress:YES
           indeterminate:YES
                progress:0
                   notes:nil
                notesURL:nil
            primaryTitle:nil
          secondaryTitle:@"Cancel"
                showSkip:NO];
  } else if ([state isEqualToString:@"downloading"]) {
    [self configureTitle:@"Downloading Update…"
                subtitle:@"118.4 MB of 184.0 MB"
            showProgress:YES
           indeterminate:NO
                progress:0.64
                   notes:nil
                notesURL:nil
            primaryTitle:nil
          secondaryTitle:@"Cancel"
                showSkip:NO];
  } else if ([state isEqualToString:@"extracting"]) {
    [self configureTitle:@"Extracting Update…"
                subtitle:@"Extracting update… 72%"
            showProgress:YES
           indeterminate:NO
                progress:0.72
                   notes:nil
                notesURL:nil
            primaryTitle:nil
          secondaryTitle:nil
                showSkip:NO];
  } else if ([state isEqualToString:@"up-to-date"]) {
    [self configureTitle:@"You’re up to date"
                subtitle:myarc::UpToDateSubtitle()
            showProgress:NO
           indeterminate:NO
                progress:0
                   notes:nil
                notesURL:nil
            primaryTitle:@"OK"
          secondaryTitle:nil
                showSkip:NO];
  } else if ([state isEqualToString:@"error"]) {
    [self configureTitle:@"Update Error"
                subtitle:@"The downloaded update failed EdDSA signature "
                         @"verification and was discarded."
            showProgress:NO
           indeterminate:NO
                progress:0
                   notes:nil
                notesURL:nil
            primaryTitle:@"OK"
          secondaryTitle:nil
                showSkip:NO];
  } else if ([state isEqualToString:@"ready"]) {
    [self configureTitle:@"Ready to Install"
                subtitle:@"myArc 2026.11.24 is verified and ready. "
                         @"Relaunch myArc to finish updating."
            showProgress:NO
           indeterminate:NO
                progress:0
                   notes:nil
                notesURL:nil
            primaryTitle:@"Install and Relaunch"
          secondaryTitle:@"Later"
                showSkip:NO];
  } else {
    // Default preview: "Update Available" with release notes card + link.
    [self configureTitle:@"myArc 2026.11.24 is available"
                subtitle:[NSString stringWithFormat:
                                       @"You’re currently on %@. Install the "
                                       @"latest version to get recent fixes.",
                                       myarc::CurrentVersionLabel()]
            showProgress:NO
           indeterminate:NO
                progress:0
                   notes:@"• Frosted glass styling across Settings, dialogs, "
                         @"popovers, and coach marks\n"
                         @"• Faster startup with lazy hidden Profile windows\n"
                         @"• Simplified one-click Arc import"
                notesURL:_releaseNotesURL
            primaryTitle:@"Install and Relaunch"
          secondaryTitle:@"Later"
                showSkip:YES];
  }
  [self presentWindow];
}

@end

// Sparkle's SPUUpdaterDelegate. The feed comes from Info.plist's SUFeedURL
// unless a test hook set `feedURL`.
@interface MyarcUpdaterDelegate : NSObject
@property(nonatomic, copy) NSString* feedURL;
@end

@implementation MyarcUpdaterDelegate

@synthesize feedURL = _feedURL;

- (NSString*)feedURLStringForUpdater:(id)updater {
  return _feedURL;
}

#if BUILDFLAG(MYARC_TEST_HOOKS)
// tools/check_updates.sh reads these lines from the log file.
- (void)log:(NSString*)event item:(id)item error:(NSError*)error {
  std::string line = "myarc-update: " + base::SysNSStringToUTF8(event);
  if (item) {
    line += " " + base::SysNSStringToUTF8([item valueForKey:@"versionString"]);
  }
  if (error) {
    line += " error " + base::SysNSStringToUTF8(error.localizedDescription);
    if (NSError* cause = error.userInfo[NSUnderlyingErrorKey]) {
      line += " (" + base::SysNSStringToUTF8(cause.localizedDescription) + ")";
    }
  }
  LOG(WARNING) << line;
}

- (void)updater:(id)updater didFindValidUpdate:(id)item {
  [self log:@"found" item:item error:nil];
}

- (void)updaterDidNotFindUpdate:(id)updater error:(NSError*)error {
  [self log:@"none" item:nil error:error];
}

- (void)updater:(id)updater didDownloadUpdate:(id)item {
  [self log:@"downloaded" item:item error:nil];
}

- (void)updater:(id)updater didExtractUpdate:(id)item {
  [self log:@"extracted" item:item error:nil];
}

- (BOOL)updater:(id)updater
                 willInstallUpdateOnQuit:(id)item
              immediateInstallationBlock:(void (^)(void))block {
  [self log:@"ready" item:item error:nil];
  return NO;
}

- (void)updater:(id)updater didAbortWithError:(NSError*)error {
  [self log:@"aborted" item:nil error:error];
}
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)

@end

namespace myarc {
namespace {

MyarcUpdaterDelegate* g_delegate = nil;
MyarcSparkleUserDriver* g_driver = nil;
id<MyarcSparkleUpdater> g_updater = nil;
bool g_started = false;

NSString* SparklePath() {
  return [base::apple::OuterBundle().privateFrameworksPath
      stringByAppendingPathComponent:@"Sparkle.framework"];
}

// True when StartUpdater() can run: a release build or a test feed, and an
// embedded Sparkle.framework. Only checks the file, so it is cheap at menu
// build time.
bool UpdaterAvailable() {
#if !BUILDFLAG(MYARC_RELEASE)
  if (!TestFeed()) {
    return false;
  }
#endif
  return [NSFileManager.defaultManager fileExistsAtPath:SparklePath()];
}

id<MyarcSparkleUpdater> StartUpdater() {
  NSString* feed = TestFeed();
  NSBundle* sparkle = [NSBundle bundleWithPath:SparklePath()];
  NSError* error = nil;
  if (!sparkle || ![sparkle loadAndReturnError:&error]) {
    LOG(WARNING) << "No updater: Sparkle.framework did not load"
                 << (error ? ": " + base::SysNSStringToUTF8(
                                        error.localizedDescription)
                           : std::string());
    return nil;
  }
  Class updater_class = NSClassFromString(@"SPUUpdater");
  if (!updater_class) {
    return nil;
  }
  g_delegate = [[MyarcUpdaterDelegate alloc] init];
  g_delegate.feedURL = feed;
  g_driver = [[MyarcSparkleUserDriver alloc] init];
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (feed) {
    // The argument domain is never saved, so the user's settings stay as
    // they were.
    NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
    NSMutableDictionary* arguments =
        [[defaults volatileDomainForName:NSArgumentDomain] mutableCopy]
            ?: [NSMutableDictionary dictionary];
    arguments[@"SUAutomaticallyUpdate"] = @YES;
    [defaults setVolatileDomain:arguments forName:NSArgumentDomain];
  }
#endif
  NSBundle* bundle = base::apple::OuterBundle();
  id<MyarcSparkleUpdater> updater =
      [(id<MyarcSparkleUpdater>)[updater_class alloc]
          initWithHostBundle:bundle
           applicationBundle:bundle
                  userDriver:g_driver
                    delegate:g_delegate];
  NSError* start_error = nil;
  if (![updater startUpdater:&start_error]) {
    LOG(WARNING) << "No updater: SPUUpdater failed to start"
                 << (start_error
                         ? ": " + base::SysNSStringToUTF8(
                                      start_error.localizedDescription)
                         : std::string());
    return nil;
  }
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (feed) {
    [updater checkForUpdatesInBackground];
  }
#endif
  return updater;
}

// Starts Sparkle once, from the delayed start or the first menu click.
id<MyarcSparkleUpdater> EnsureUpdater() {
  if (!g_started) {
    g_started = true;
    g_updater = StartUpdater();
  }
  return g_updater;
}

#if BUILDFLAG(MYARC_TEST_HOOKS)
void MaybeShowUpdaterPreview() {
  static bool scheduled = false;
  if (scheduled) {
    return;
  }
  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (!cmd->HasSwitch(kCaptureUpdaterUi)) {
    return;
  }
  scheduled = true;
  NSString* state =
      base::SysUTF8ToNSString(cmd->GetSwitchValueASCII(kCaptureUpdaterUi));
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC),
                 dispatch_get_main_queue(), ^{
                   if (!g_driver) {
                     g_driver = [[MyarcSparkleUserDriver alloc] init];
                   }
                   [g_driver showPreviewState:state];
                 });
}
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)

}  // namespace
}  // namespace myarc

// Subtle clickable pill showing the myArc version + trailing copy icon.
@interface MyarcVersionPillButton : NSButton
@property(nonatomic, copy) NSString* displayTitle;
@property(nonatomic, strong) NSImage* trailingIcon;
@end

@implementation MyarcVersionPillButton {
  BOOL _hovered;
  NSTrackingArea* _tracking;
}

@synthesize displayTitle = _displayTitle;
@synthesize trailingIcon = _trailingIcon;

- (instancetype)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.bordered = NO;
    self.bezelStyle = NSBezelStyleInline;
    self.focusRingType = NSFocusRingTypeExterior;
  }
  return self;
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if (_tracking) {
    [self removeTrackingArea:_tracking];
  }
  _tracking = [[NSTrackingArea alloc]
      initWithRect:self.bounds
           options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
             owner:self
          userInfo:nil];
  [self addTrackingArea:_tracking];
}

- (void)mouseEntered:(NSEvent*)event {
  _hovered = YES;
  [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
  _hovered = NO;
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  const NSRect bounds = self.bounds;
  const BOOL pressed = self.cell.isHighlighted;
  NSBezierPath* pill =
      [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:6.0 yRadius:6.0];
  [[NSColor colorWithWhite:1.0
                     alpha:pressed ? 0.16 : (_hovered ? 0.11 : 0.06)] setFill];
  [pill fill];

  NSRect inset = NSInsetRect(bounds, 0.5, 0.5);
  NSBezierPath* rim =
      [NSBezierPath bezierPathWithRoundedRect:inset xRadius:5.5 yRadius:5.5];
  rim.lineWidth = 1.0;
  [[NSColor colorWithWhite:1.0 alpha:_hovered ? 0.18 : 0.10] setStroke];
  [rim stroke];

  NSDictionary* attrs = @{
    NSFontAttributeName :
        [NSFont systemFontOfSize:12 weight:NSFontWeightMedium],
    NSForegroundColorAttributeName : [NSColor colorWithWhite:1.0 alpha:0.86],
  };
  NSString* label = _displayTitle ?: @"";
  NSSize textSize = [label sizeWithAttributes:attrs];
  constexpr CGFloat kIconSize = 12.0;
  constexpr CGFloat kGap = 6.0;
  const CGFloat totalW =
      std::ceil(textSize.width) + (_trailingIcon ? (kGap + kIconSize) : 0.0);
  const CGFloat startX = std::floor((NSWidth(bounds) - totalW) / 2.0);
  const CGFloat textY =
      std::floor((NSHeight(bounds) - textSize.height) / 2.0) - 0.5;
  [label drawInRect:NSMakeRect(startX, textY, std::ceil(textSize.width) + 2,
                               textSize.height + 2)
      withAttributes:attrs];
  if (_trailingIcon) {
    const CGFloat iconX = startX + std::ceil(textSize.width) + kGap;
    const CGFloat iconY = std::floor((NSHeight(bounds) - kIconSize) / 2.0);
    [_trailingIcon drawInRect:NSMakeRect(iconX, iconY, kIconSize, kIconSize)
                     fromRect:NSZeroRect
                    operation:NSCompositingOperationSourceOver
                     fraction:_hovered ? 0.85 : 0.55
               respectFlipped:YES
                        hints:nil];
  }
}

@end

namespace {

NSImage* AboutSymbolIcon(NSString* symbol_name, CGFloat point_size) {
  NSImage* img = [NSImage imageWithSystemSymbolName:symbol_name
                           accessibilityDescription:nil];
  if (!img) {
    return nil;
  }
  NSImageSymbolConfiguration* size_cfg = [NSImageSymbolConfiguration
      configurationWithPointSize:point_size
                          weight:NSFontWeightRegular];
  NSImageSymbolConfiguration* color_cfg = [NSImageSymbolConfiguration
      configurationWithHierarchicalColor:[NSColor colorWithWhite:1.0
                                                           alpha:0.92]];
  return [img imageWithSymbolConfiguration:
                  [size_cfg configurationByApplyingConfiguration:color_cfg]];
}

NSImage* AboutGitHubIcon() {
  return [NSImage
      imageWithSize:NSMakeSize(15, 15)
            flipped:YES
     drawingHandler:^BOOL(NSRect rect) {
       // Standard 24x24 GitHub mark scaled to 15x15.
       const CGFloat s = 15.0 / 24.0;
       auto P = [s](CGFloat x, CGFloat y) { return NSMakePoint(x * s, y * s); };
       NSBezierPath* path = [NSBezierPath bezierPath];
       [path moveToPoint:P(12.0, 0.5)];
       [path curveToPoint:P(0.5, 12.0)
            controlPoint1:P(5.65, 0.5)
            controlPoint2:P(0.5, 5.65)];
       [path curveToPoint:P(8.36, 22.91)
            controlPoint1:P(0.5, 17.08)
            controlPoint2:P(3.79, 21.39)];
       [path curveToPoint:P(9.15, 22.35)
            controlPoint1:P(8.94, 23.02)
            controlPoint2:P(9.15, 22.66)];
       [path curveToPoint:P(9.13, 20.35)
            controlPoint1:P(9.15, 22.07)
            controlPoint2:P(9.14, 21.33)];
       [path curveToPoint:P(5.25, 18.81)
            controlPoint1:P(5.93, 21.05)
            controlPoint2:P(5.25, 18.81)];
       [path curveToPoint:P(3.97, 17.12)
            controlPoint1:P(4.73, 17.48)
            controlPoint2:P(3.97, 17.12)];
       [path curveToPoint:P(4.05, 16.42)
            controlPoint1:P(2.92, 16.40)
            controlPoint2:P(4.05, 16.42)];
       [path curveToPoint:P(5.82, 17.61)
            controlPoint1:P(5.21, 16.50)
            controlPoint2:P(5.82, 17.61)];
       [path curveToPoint:P(9.18, 18.57)
            controlPoint1:P(6.85, 19.38)
            controlPoint2:P(8.52, 18.87)];
       [path curveToPoint:P(9.91, 17.02)
            controlPoint1:P(9.28, 17.82)
            controlPoint2:P(9.58, 17.31)];
       [path curveToPoint:P(4.67, 11.33)
            controlPoint1:P(7.36, 16.73)
            controlPoint2:P(4.67, 15.74)];
       [path curveToPoint:P(5.86, 8.24)
            controlPoint1:P(4.67, 10.07)
            controlPoint2:P(5.12, 9.05)];
       [path curveToPoint:P(5.97, 5.19)
            controlPoint1:P(5.74, 7.95)
            controlPoint2:P(5.34, 6.78)];
       [path curveToPoint:P(9.15, 6.37)
            controlPoint1:P(5.97, 5.19)
            controlPoint2:P(6.94, 4.88)];
       [path curveToPoint:P(14.94, 6.37)
            controlPoint1:P(10.07, 6.11)
            controlPoint2:P(14.02, 6.11)];
       [path curveToPoint:P(18.12, 5.19)
            controlPoint1:P(17.15, 4.88)
            controlPoint2:P(18.12, 5.19)];
       [path curveToPoint:P(18.23, 8.24)
            controlPoint1:P(18.75, 6.78)
            controlPoint2:P(18.35, 7.95)];
       [path curveToPoint:P(19.42, 11.33)
            controlPoint1:P(18.97, 9.05)
            controlPoint2:P(19.42, 10.07)];
       [path curveToPoint:P(14.16, 17.01)
            controlPoint1:P(19.42, 15.75)
            controlPoint2:P(16.73, 16.73)];
       [path curveToPoint:P(14.94, 19.16)
            controlPoint1:P(14.57, 17.37)
            controlPoint2:P(14.94, 18.08)];
       [path curveToPoint:P(14.93, 22.34)
            controlPoint1:P(14.94, 20.71)
            controlPoint2:P(14.93, 21.96)];
       [path curveToPoint:P(15.73, 22.90)
            controlPoint1:P(14.93, 22.65)
            controlPoint2:P(15.14, 23.02)];
       [path curveToPoint:P(23.5, 12.0)
            controlPoint1:P(20.21, 21.39)
            controlPoint2:P(23.5, 17.08)];
       [path curveToPoint:P(12.0, 0.5)
            controlPoint1:P(23.5, 5.65)
            controlPoint2:P(18.35, 0.5)];
       [path closePath];
       [[NSColor colorWithWhite:1.0 alpha:0.92] setFill];
       [path fill];
       return YES;
     }];
}

}  // namespace

@interface MyarcAboutPanelController : NSObject <NSWindowDelegate>
- (void)showAbout;
@end

@implementation MyarcAboutPanelController {
  NSWindow* __strong _window;
  MyarcVersionPillButton* __strong _versionPill;
  NSString* __strong _versionLabel;
  NSString* __strong _copyPayload;
}

- (void)showAbout {
  if (_window) {
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    return;
  }

  const BOOL has_updater = myarc::UpdaterAvailable();
  constexpr CGFloat kWidth = 332.0;
  constexpr CGFloat kPadX = 28.0;
  constexpr CGFloat kInnerW = kWidth - 2.0 * kPadX;
  const CGFloat kHeight = has_updater ? 384.0 : 338.0;

  NSRect frame = NSMakeRect(0, 0, kWidth, kHeight);
  NSWindow* window = [[NSWindow alloc]
      initWithContentRect:frame
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskFullSizeContentView
                  backing:NSBackingStoreBuffered
                    defer:NO];
  window.title = @"About myArc";
  window.titleVisibility = NSWindowTitleHidden;
  window.titlebarAppearsTransparent = YES;
  window.movableByWindowBackground = YES;
  window.releasedWhenClosed = NO;
  window.delegate = self;
  [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
  [[window standardWindowButton:NSWindowZoomButton] setHidden:YES];

  NSView* root = [[NSView alloc] initWithFrame:frame];
  root.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  window.contentView = root;

  myarc::ApplyNSWindowGlass(window, myarc::GlassMaterial::kHud,
                            myarc::kWindowCornerRadius, /*dark=*/true);

  // MyarcUpdaterCanvasView is flipped (y = 0 at top).
  MyarcUpdaterCanvasView* canvas =
      [[MyarcUpdaterCanvasView alloc] initWithFrame:frame];
  canvas.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [root addSubview:canvas];

  CGFloat y = 38.0;
  constexpr CGFloat kIconSize = 76.0;
  NSImageView* icon = [[NSImageView alloc]
      initWithFrame:NSMakeRect((kWidth - kIconSize) / 2.0, y, kIconSize,
                               kIconSize)];
  icon.image = myarc::UpdaterAppIcon();
  icon.imageScaling = NSImageScaleProportionallyUpOrDown;
  icon.wantsLayer = YES;
  icon.layer.shadowColor = NSColor.blackColor.CGColor;
  icon.layer.shadowOpacity = 0.35f;
  icon.layer.shadowRadius = 10.0f;
  icon.layer.shadowOffset = CGSizeMake(0, -4);
  [canvas addSubview:icon];
  y += kIconSize + 14.0;

  NSColor* primary_ink = [NSColor colorWithSRGBRed:0xF5 / 255.0
                                             green:0xF5 / 255.0
                                              blue:0xF7 / 255.0
                                             alpha:1.0];
  NSColor* secondary_ink = [NSColor colorWithWhite:1.0 alpha:0.50];
  NSColor* footer_ink = [NSColor colorWithWhite:1.0 alpha:0.42];

  NSTextField* title = [NSTextField labelWithString:@"myArc"];
  title.font = [NSFont systemFontOfSize:22 weight:NSFontWeightBold];
  title.textColor = primary_ink;
  title.alignment = NSTextAlignmentCenter;
  title.frame = NSMakeRect(kPadX, y, kInnerW, 28.0);
  [canvas addSubview:title];
  y += 28.0 + 10.0;

  NSBundle* bundle = base::apple::OuterBundle();
  NSString* myarc_ver =
      base::apple::ObjCCast<NSString>(
          [bundle objectForInfoDictionaryKey:@"MyarcVersion"]);
  _versionLabel =
      myarc_ver.length ? [NSString stringWithFormat:@"Version %@", myarc_ver]
                       : @"Developer Build";

  NSString* chromium_ver =
      base::apple::ObjCCast<NSString>(
          [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
  NSString* engine_line =
      chromium_ver.length
          ? [NSString stringWithFormat:@"Chromium %@ · arm64", chromium_ver]
          : @"Chromium · arm64";
  _copyPayload = myarc_ver.length
                     ? [NSString stringWithFormat:@"myArc %@ (%@)", myarc_ver,
                                                  engine_line]
                     : [NSString stringWithFormat:@"myArc (%@)", engine_line];

  NSDictionary* pill_attrs = @{
    NSFontAttributeName :
        [NSFont systemFontOfSize:12 weight:NSFontWeightMedium],
  };
  const CGFloat pill_text_w =
      std::ceil([_versionLabel sizeWithAttributes:pill_attrs].width);
  const CGFloat pill_w =
      std::min(kInnerW, std::max<CGFloat>(148.0, pill_text_w + 42.0));
  _versionPill = [[MyarcVersionPillButton alloc]
      initWithFrame:NSMakeRect((kWidth - pill_w) / 2.0, y, pill_w, 26.0)];
  _versionPill.displayTitle = _versionLabel;
  _versionPill.trailingIcon = AboutSymbolIcon(@"doc.on.doc", 11.0);
  _versionPill.toolTip = @"Copy version info";
  _versionPill.target = self;
  _versionPill.action = @selector(copyVersion:);
  [canvas addSubview:_versionPill];
  y += 26.0 + 8.0;

  NSTextField* engine = [NSTextField labelWithString:engine_line];
  engine.font = [NSFont monospacedDigitSystemFontOfSize:12
                                                 weight:NSFontWeightRegular];
  engine.textColor = secondary_ink;
  engine.alignment = NSTextAlignmentCenter;
  engine.frame = NSMakeRect(kPadX, y, kInnerW, 16.0);
  [canvas addSubview:engine];
  y += 16.0 + 26.0;

  constexpr CGFloat kBtnHeight = 36.0;
  constexpr CGFloat kBtnGap = 10.0;
  if (has_updater) {
    MyarcUpdaterButton* check_btn = [[MyarcUpdaterButton alloc]
        initWithStyle:MyarcUpdaterButtonStylePrimary];
    check_btn.title = @"Check for Updates…";
    check_btn.target = self;
    check_btn.action = @selector(checkUpdates:);
    check_btn.keyEquivalent = @"\r";
    check_btn.frame = NSMakeRect(kPadX, y, kInnerW, kBtnHeight);
    [canvas addSubview:check_btn];
    y += kBtnHeight + kBtnGap;
  }

  const CGFloat half_w = (kInnerW - kBtnGap) / 2.0;
  MyarcUpdaterButton* site_btn = [[MyarcUpdaterButton alloc]
      initWithStyle:MyarcUpdaterButtonStyleSecondary];
  site_btn.title = @"myarc.dev";
  site_btn.leadingIcon = AboutSymbolIcon(@"globe", 14.0);
  site_btn.target = self;
  site_btn.action = @selector(openWebsite:);
  site_btn.frame = NSMakeRect(kPadX, y, half_w, kBtnHeight);
  [canvas addSubview:site_btn];

  MyarcUpdaterButton* gh_btn = [[MyarcUpdaterButton alloc]
      initWithStyle:MyarcUpdaterButtonStyleSecondary];
  gh_btn.title = @"GitHub";
  gh_btn.leadingIcon = AboutGitHubIcon();
  gh_btn.target = self;
  gh_btn.action = @selector(openGitHub:);
  gh_btn.frame = NSMakeRect(kPadX + half_w + kBtnGap, y, half_w, kBtnHeight);
  [canvas addSubview:gh_btn];
  y += kBtnHeight + 22.0;

  NSTextField* footer =
      [NSTextField labelWithString:@"GPL-3.0 · ungoogled-chromium"];
  footer.font = [NSFont systemFontOfSize:11 weight:NSFontWeightRegular];
  footer.textColor = footer_ink;
  footer.alignment = NSTextAlignmentCenter;
  footer.frame = NSMakeRect(kPadX, y, kInnerW, 16.0);
  [canvas addSubview:footer];

  _window = window;
  [_window center];
  [_window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
}

- (void)copyVersion:(id)sender {
  if (!_copyPayload.length) {
    return;
  }
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb clearContents];
  [pb setString:_copyPayload forType:NSPasteboardTypeString];

  _versionPill.displayTitle = @"Copied";
  _versionPill.trailingIcon = AboutSymbolIcon(@"checkmark", 11.0);
  [_versionPill setNeedsDisplay:YES];

  __weak MyarcAboutPanelController* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.4 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        MyarcAboutPanelController* strongSelf = weakSelf;
        if (!strongSelf || !strongSelf->_versionPill) {
          return;
        }
        strongSelf->_versionPill.displayTitle = strongSelf->_versionLabel;
        strongSelf->_versionPill.trailingIcon =
            AboutSymbolIcon(@"doc.on.doc", 11.0);
        [strongSelf->_versionPill setNeedsDisplay:YES];
      });
}

- (void)checkUpdates:(id)sender {
  [_window orderOut:nil];
  [myarc::EnsureUpdater() checkForUpdates];
}

- (void)openWebsite:(id)sender {
  [[NSWorkspace sharedWorkspace]
      openURL:[NSURL URLWithString:@"https://myarc.dev/"]];
}

- (void)openGitHub:(id)sender {
  [[NSWorkspace sharedWorkspace]
      openURL:[NSURL URLWithString:@"https://github.com/bitbonsai/myarc"]];
}

- (void)windowWillClose:(NSNotification*)notification {
  _window = nil;
  _versionPill = nil;
}

@end

// The menu item's target. Starts Sparkle on the first click if the delayed
// start hasn't yet, then forwards to Sparkle's updater.
@interface MyarcUpdaterMenuTarget : NSObject <NSMenuItemValidation>
@end

@implementation MyarcUpdaterMenuTarget

- (void)checkForUpdates:(id)sender {
  [myarc::EnsureUpdater() checkForUpdates];
}

- (void)showAbout:(id)sender {
  static MyarcAboutPanelController* about_controller = nil;
  if (!about_controller) {
    about_controller = [[MyarcAboutPanelController alloc] init];
  }
  [about_controller showAbout];
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
  if (item.action == @selector(showAbout:)) {
    return YES;
  }
  if (!myarc::g_started) {
    return YES;
  }
  id<MyarcSparkleUpdater> updater = myarc::g_updater;
  return updater != nil && updater.canCheckForUpdates;
}

@end

namespace myarc {

void AddUpdaterMenuItem(NSMenu* app_menu, NSInteger index) {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  MaybeShowUpdaterPreview();
#endif
  static MyarcUpdaterMenuTarget* target = nil;
  if (!target) {
    if (!UpdaterAvailable()) {
      return;
    }
    target = [[MyarcUpdaterMenuTarget alloc] init];
    // A test feed checks right away, so tools/check_updates.sh sees the
    // result without waiting.
    int64_t delay = TestFeed() ? 0 : kStartDelaySeconds * NSEC_PER_SEC;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delay),
                   dispatch_get_main_queue(), ^{
                     EnsureUpdater();
                   });
  }
  NSMenuItem* item =
      [[NSMenuItem alloc] initWithTitle:@"Check for Updates…"
                                 action:@selector(checkForUpdates:)
                          keyEquivalent:@""];
  item.target = target;
  [app_menu insertItem:item atIndex:index];
}

void BindAboutMenuItem(NSMenuItem* about_item) {
  if (!about_item) {
    return;
  }
  static MyarcUpdaterMenuTarget* about_target = nil;
  if (!about_target) {
    about_target = [[MyarcUpdaterMenuTarget alloc] init];
  }
  about_item.target = about_target;
  about_item.action = @selector(showAbout:);
}

}  // namespace myarc
