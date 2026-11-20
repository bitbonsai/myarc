#include "myarc/welcome_ribbon.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <VideoToolbox/VideoToolbox.h>

#include "base/apple/bundle_locations.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace myarc {
namespace {

constexpr char kRibbonFile[] = "myarc_welcome_ribbon";
// The frames are drawn at twice their size in DIP.
constexpr float kScale = 2;

NSURL* RibbonURL() {
  return [base::apple::FrameworkBundle() URLForResource:@(kRibbonFile)
                                          withExtension:@"mov"];
}

// A decoded frame, through a CGImage, so Skia copies its pixels.
gfx::ImageSkia ToImage(CVPixelBufferRef pixels) {
  CGImageRef image = nullptr;
  if (VTCreateCGImageFromCVPixelBuffer(pixels, nullptr, &image) != noErr ||
      !image) {
    return gfx::ImageSkia();
  }
  const SkBitmap bitmap = skia::CGImageToSkBitmap(image);
  CGImageRelease(image);
  return bitmap.drawsNothing()
             ? gfx::ImageSkia()
             : gfx::ImageSkia::CreateFromBitmap(bitmap, kScale);
}

}  // namespace

struct WelcomeRibbon::Cocoa {
  AVAssetReader* reader = nil;
  AVAssetReaderTrackOutput* output = nil;
};

// static
std::unique_ptr<WelcomeRibbon> WelcomeRibbon::Load() {
  NSURL* url = RibbonURL();
  if (!url) {
    return nullptr;
  }
  AVURLAsset* asset = [AVURLAsset URLAssetWithURL:url options:nil];
  AVAssetTrack* track =
      [asset tracksWithMediaType:AVMediaTypeVideo].firstObject;
  if (!track) {
    return nullptr;
  }
  auto cocoa = std::make_unique<Cocoa>();
  cocoa->reader = [[AVAssetReader alloc] initWithAsset:asset error:nil];
  cocoa->output = [[AVAssetReaderTrackOutput alloc]
       initWithTrack:track
      outputSettings:@{
        (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)
      }];
  cocoa->output.alwaysCopiesSampleData = NO;
  if (!cocoa->reader || ![cocoa->reader canAddOutput:cocoa->output]) {
    return nullptr;
  }
  [cocoa->reader addOutput:cocoa->output];
  if (![cocoa->reader startReading]) {
    return nullptr;
  }
  return std::unique_ptr<WelcomeRibbon>(new WelcomeRibbon(std::move(cocoa)));
}

// static
gfx::ImageSkia WelcomeRibbon::LastFrame() {
  NSURL* url = RibbonURL();
  if (!url) {
    return gfx::ImageSkia();
  }
  AVURLAsset* asset = [AVURLAsset URLAssetWithURL:url options:nil];
  AVAssetImageGenerator* generator =
      [AVAssetImageGenerator assetImageGeneratorWithAsset:asset];
  generator.requestedTimeToleranceBefore = kCMTimeZero;
  generator.requestedTimeToleranceAfter = kCMTimeZero;
  const CMTime last = CMTimeSubtract(asset.duration, CMTimeMake(1, 60));
  CGImageRef image = [generator copyCGImageAtTime:last
                                       actualTime:nullptr
                                            error:nil];
  if (!image) {
    return gfx::ImageSkia();
  }
  const SkBitmap bitmap = skia::CGImageToSkBitmap(image);
  CGImageRelease(image);
  return bitmap.drawsNothing()
             ? gfx::ImageSkia()
             : gfx::ImageSkia::CreateFromBitmap(bitmap, kScale);
}

WelcomeRibbon::WelcomeRibbon(std::unique_ptr<Cocoa> cocoa)
    : cocoa_(std::move(cocoa)) {}

WelcomeRibbon::~WelcomeRibbon() {
  [cocoa_->reader cancelReading];
}

gfx::ImageSkia WelcomeRibbon::Next() {
  if (cocoa_->reader.status != AVAssetReaderStatusReading) {
    return gfx::ImageSkia();
  }
  CMSampleBufferRef sample = [cocoa_->output copyNextSampleBuffer];
  if (!sample) {
    return gfx::ImageSkia();
  }
  gfx::ImageSkia image;
  if (CVImageBufferRef pixels = CMSampleBufferGetImageBuffer(sample)) {
    image = ToImage(pixels);
  }
  CFRelease(sample);
  return image;
}

}  // namespace myarc
