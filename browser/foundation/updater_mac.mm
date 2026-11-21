#include "myarc/updater_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "myarc/nightly_buildflags.h"
#include "myarc/test_buildflags.h"

// Sparkle.framework is embedded by tools/package_nightly.sh, not by the build,
// so it is loaded at runtime and only these parts of its API are declared.
@protocol MyarcSparkleUpdater <NSObject>
- (void)checkForUpdatesInBackground;
@end

@protocol MyarcSparkleController <NSObject>
- (instancetype)initWithStartingUpdater:(BOOL)startUpdater
                        updaterDelegate:(id)updaterDelegate
                     userDriverDelegate:(id)userDriverDelegate;
@property(nonatomic, readonly) id<MyarcSparkleUpdater> updater;
- (void)checkForUpdates:(id)sender;
@end

@protocol MyarcSparkleAppcastItem <NSObject>
@property(nonatomic, readonly) NSString* displayVersionString;
@end

// Sparkle's SPUUpdaterDelegate. The feed comes from Info.plist's SUFeedURL
// unless a test hook set `feedURL`. It is also the SPUStandardUserDriverDelegate
// and SUVersionDisplay, so Sparkle's dialogs show the nightly label
// (Info.plist `MyarcVersion`) instead of CFBundleShortVersionString, which
// stays Chromium's version.
@interface MyarcUpdaterDelegate : NSObject
@property(nonatomic, copy) NSString* feedURL;
@end

@implementation MyarcUpdaterDelegate

@synthesize feedURL = _feedURL;

- (NSString*)feedURLStringForUpdater:(id)updater {
  return _feedURL;
}

- (id)standardUserDriverRequestsVersionDisplayer {
  return self;
}

- (NSString*)myarcVersionOr:(NSString*)fallback {
  NSString* version = base::apple::ObjCCast<NSString>(
      [base::apple::OuterBundle() objectForInfoDictionaryKey:@"MyarcVersion"]);
  return version.length ? version : fallback;
}

- (NSString*)formatUpdateDisplayVersionFromUpdate:
                 (id<MyarcSparkleAppcastItem>)update
                          andBundleDisplayVersion:
                              (NSString* __autoreleasing*)inOutBundleVersion
                                withBundleVersion:(NSString*)bundleVersion {
  *inOutBundleVersion = [self myarcVersionOr:*inOutBundleVersion];
  return update.displayVersionString;
}

- (NSString*)formatBundleDisplayVersion:(NSString*)bundleDisplayVersion
                      withBundleVersion:(NSString*)bundleVersion
                         matchingUpdate:(id)matchingUpdate {
  return [self myarcVersionOr:bundleDisplayVersion];
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

#if BUILDFLAG(MYARC_TEST_HOOKS)
// Points Sparkle at a local appcast, downloads in the background and checks
// right away, so a test sees the result without any window.
constexpr char kTestUpdateFeed[] = "myarc-test-update-feed";
#endif

// Seconds between building the menu and starting Sparkle, so loading the
// framework and its first check stay off the startup path.
constexpr int64_t kStartDelaySeconds = 5;

MyarcUpdaterDelegate* g_delegate = nil;
id<MyarcSparkleController> g_controller = nil;
bool g_started = false;

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

NSString* SparklePath() {
  return [base::apple::OuterBundle().privateFrameworksPath
      stringByAppendingPathComponent:@"Sparkle.framework"];
}

// True when StartUpdater() can run: a nightly or a test feed, and an embedded
// Sparkle.framework. Only checks the file, so it is cheap at menu build time.
bool UpdaterAvailable() {
#if !BUILDFLAG(MYARC_NIGHTLY)
  if (!TestFeed()) {
    return false;
  }
#endif
  return [NSFileManager.defaultManager fileExistsAtPath:SparklePath()];
}

id<MyarcSparkleController> StartUpdater() {
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
  Class controller_class = NSClassFromString(@"SPUStandardUpdaterController");
  if (!controller_class) {
    return nil;
  }
  g_delegate = [[MyarcUpdaterDelegate alloc] init];
  g_delegate.feedURL = feed;
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
  id<MyarcSparkleController> controller =
      [(id<MyarcSparkleController>)[controller_class alloc]
          initWithStartingUpdater:YES
                  updaterDelegate:g_delegate
               userDriverDelegate:g_delegate];
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (feed) {
    [controller.updater checkForUpdatesInBackground];
  }
#endif
  return controller;
}

// Starts Sparkle once, from the delayed start or the first menu click.
id<MyarcSparkleController> EnsureUpdater() {
  if (!g_started) {
    g_started = true;
    g_controller = StartUpdater();
  }
  return g_controller;
}

}  // namespace
}  // namespace myarc

// The menu item's target. Starts Sparkle on the first click if the delayed
// start hasn't yet, then forwards to Sparkle's controller.
@interface MyarcUpdaterMenuTarget : NSObject <NSMenuItemValidation>
@end

@implementation MyarcUpdaterMenuTarget

- (void)checkForUpdates:(id)sender {
  [myarc::EnsureUpdater() checkForUpdates:sender];
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
  if (!myarc::g_started) {
    return YES;
  }
  id controller = myarc::g_controller;
  if (!controller) {
    return NO;
  }
  return ![controller respondsToSelector:@selector(validateMenuItem:)] ||
         [controller validateMenuItem:item];
}

@end

namespace myarc {

void AddUpdaterMenuItem(NSMenu* app_menu, NSInteger index) {
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

}  // namespace myarc
