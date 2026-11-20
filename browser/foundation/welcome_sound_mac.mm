#include "myarc/welcome_sound.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <utility>

#include "base/apple/bundle_locations.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "myarc/welcome_timeline.h"

namespace myarc {
namespace {

// The sound's file in the app's resources, from //myarc:welcome_sound.
constexpr char kSoundFile[] = "myarc_welcome.m4a";
constexpr float kVolume = 0.6;

// "Play user interface sound effects", which is on unless it says otherwise.
bool UiSoundsOn() {
  id value = [[NSUserDefaults standardUserDefaults]
      persistentDomainForName:NSGlobalDomain]
      [@"com.apple.sound.uiaudio.enabled"];
  return ![value respondsToSelector:@selector(boolValue)] || [value boolValue];
}

}  // namespace

struct WelcomeSound::Cocoa {
  AVAudioPlayer* player = nil;
};

// static
std::unique_ptr<WelcomeSound> WelcomeSound::Load(bool log) {
  if (!UiSoundsOn()) {
    LOG_IF(WARNING, log) << "Test onboarding: sound off by the system setting";
    return nullptr;
  }
  NSString* file = @(kSoundFile);
  NSURL* url = [base::apple::FrameworkBundle()
      URLForResource:file.stringByDeletingPathExtension
       withExtension:file.pathExtension];
  if (!url) {
    LOG_IF(WARNING, log) << "Test onboarding: sound missing";
    return nullptr;
  }
  auto cocoa = std::make_unique<Cocoa>();
  cocoa->player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:nil];
  if (!cocoa->player || ![cocoa->player prepareToPlay]) {
    LOG_IF(WARNING, log) << "Test onboarding: sound failed";
    return nullptr;
  }
  // The first play starts the audio output, which takes tens of
  // milliseconds; do it silently now, so Start() is in step with the dim.
  cocoa->player.volume = 0;
  [cocoa->player play];
  [cocoa->player pause];
  cocoa->player.currentTime = 0;
  cocoa->player.volume = kVolume;
  return base::WrapUnique(new WelcomeSound(std::move(cocoa), log));
}

WelcomeSound::WelcomeSound(std::unique_ptr<Cocoa> cocoa, bool log)
    : cocoa_(std::move(cocoa)), log_(log) {}

void WelcomeSound::Start(base::TimeTicks start) {
  const bool playing = [cocoa_->player play];
  LOG_IF(WARNING, log_) << "Test onboarding: sound "
                        << (playing ? "start" : "failed") << " at "
                        << (base::TimeTicks::Now() - start).InMilliseconds()
                        << " ms";
}

WelcomeSound::~WelcomeSound() {
  AVAudioPlayer* player = cocoa_->player;
  if (!player.playing) {
    return;
  }
  LOG_IF(WARNING, log_) << "Test onboarding: sound fades out";
  // Fades out, then stops from a posted block that keeps the player alive
  // until then.
  const NSTimeInterval fade = welcome::kSoundFadeOut.InSecondsF();
  [player setVolume:0 fadeDuration:fade];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               static_cast<int64_t>(fade * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [player stop];
                 });
}

}  // namespace myarc
