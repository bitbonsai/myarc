#include "myarc/media_library_mac.h"

#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>
#import <ImageIO/ImageIO.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>

#include <utility>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/gfx/geometry/clamp_float_geometry.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

gfx::ImageSkia CGImageToImageSkia(CGImageRef cg_image, float scale) {
  if (!cg_image) {
    return gfx::ImageSkia();
  }
  SkBitmap bitmap = skia::CGImageToSkBitmap(cg_image);
  if (bitmap.drawsNothing()) {
    return gfx::ImageSkia();
  }
  gfx::ImageSkia image;
  image.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  return image;
}

gfx::ImageSkia DecodeWithImageIO(const base::FilePath& path,
                                 int max_px,
                                 float scale,
                                 bool check_size_limit) {
  if (check_size_limit) {
    if (std::optional<int64_t> size = base::GetFileSize(path);
        !size || *size <= 0 || *size > kMaxMediaPreviewBytes) {
      return gfx::ImageSkia();
    }
  }
  NSURL* url = base::apple::FilePathToNSURL(path);
  if (!url) {
    return gfx::ImageSkia();
  }
  base::apple::ScopedCFTypeRef<CGImageSourceRef> source(
      CGImageSourceCreateWithURL(base::apple::NSToCFPtrCast(url), nullptr));
  if (!source) {
    return gfx::ImageSkia();
  }
  NSDictionary* options = @{
    (__bridge NSString*)kCGImageSourceCreateThumbnailFromImageAlways : @YES,
    (__bridge NSString*)kCGImageSourceThumbnailMaxPixelSize : @(max_px),
    (__bridge NSString*)kCGImageSourceCreateThumbnailWithTransform : @YES,
    (__bridge NSString*)kCGImageSourceShouldCacheImmediately : @YES,
  };
  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      CGImageSourceCreateThumbnailAtIndex(
          source.get(), 0, base::apple::NSToCFPtrCast(options)));
  return CGImageToImageSkia(cg_image.get(), scale);
}

gfx::ImageSkia DecodeVideoPoster(const base::FilePath& path, int max_px) {
  NSURL* url = base::apple::FilePathToNSURL(path);
  if (!url) {
    return gfx::ImageSkia();
  }
  AVURLAsset* asset = [AVURLAsset URLAssetWithURL:url options:nil];
  AVAssetImageGenerator* generator =
      [[AVAssetImageGenerator alloc] initWithAsset:asset];
  generator.appliesPreferredTrackTransform = YES;
  generator.maximumSize = CGSizeMake(max_px, max_px);
  NSError* error = nil;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      [generator copyCGImageAtTime:kCMTimeZero actualTime:nullptr error:&error]);
#pragma clang diagnostic pop
  return CGImageToImageSkia(cg_image.get(), 1.0f);
}

bool IsVideoPath(const base::FilePath& path) {
  const std::string ext = base::ToLowerASCII(path.Extension());
  return ext == ".mov" || ext == ".mp4" || ext == ".m4v" || ext == ".webm";
}

gfx::ImageSkia DecodeThumbnailFallback(const base::FilePath& path,
                                       int max_px,
                                       float scale) {
  @autoreleasepool {
    if (IsVideoPath(path)) {
      return DecodeVideoPoster(path, max_px);
    }
    return DecodeWithImageIO(path, max_px, scale, /*check_size_limit=*/false);
  }
}

}  // namespace

base::FilePath GetMacScreenshotsDirectory() {
  @autoreleasepool {
    base::apple::ScopedCFTypeRef<CFPropertyListRef> value(
        CFPreferencesCopyAppValue(CFSTR("location"),
                                  CFSTR("com.apple.screencapture")));
    if (value && CFGetTypeID(value.get()) == CFStringGetTypeID()) {
      NSString* raw =
          (__bridge NSString*)static_cast<CFStringRef>(value.get());
      if (raw.length > 0) {
        NSString* expanded = [raw stringByExpandingTildeInPath];
        base::FilePath dir = base::apple::NSStringToFilePath(expanded);
        if (!dir.empty()) {
          return dir;
        }
      }
    }
    base::FilePath desktop;
    if (base::PathService::Get(base::DIR_USER_DESKTOP, &desktop) &&
        !desktop.empty()) {
      return desktop;
    }
    return base::GetHomeDir().Append("Desktop");
  }
}

void GenerateMediaThumbnailMac(
    const base::FilePath& path,
    int max_px,
    base::OnceCallback<void(gfx::ImageSkia)> callback) {
  scoped_refptr<base::SequencedTaskRunner> reply_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  @autoreleasepool {
    NSURL* url = base::apple::FilePathToNSURL(path);
    if (!url) {
      reply_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), gfx::ImageSkia()));
      return;
    }
    const CGFloat scale = 2.0;
    const CGSize size = CGSizeMake(max_px / scale, max_px / scale);
    QLThumbnailGenerationRequest* request =
        [[QLThumbnailGenerationRequest alloc]
              initWithFileAtURL:url
                           size:size
                          scale:scale
            representationTypes:QLThumbnailGenerationRequestRepresentationTypeThumbnail];
    __block base::OnceCallback<void(gfx::ImageSkia)> block_cb =
        std::move(callback);
    [[QLThumbnailGenerator sharedGenerator]
        generateBestRepresentationForRequest:request
                           completionHandler:^(
                               QLThumbnailRepresentation* representation,
                               NSError* error) {
                             gfx::ImageSkia result;
                             if (representation && representation.CGImage) {
                               result = CGImageToImageSkia(
                                   representation.CGImage,
                                   static_cast<float>(scale));
                             }
                             if (!result.isNull()) {
                               reply_runner->PostTask(
                                   FROM_HERE,
                                   base::BindOnce(std::move(block_cb),
                                                  std::move(result)));
                               return;
                             }
                             // Fall back to AVFoundation (video) or ImageIO
                             // (raster) off the UI thread.
                             base::ThreadPool::PostTaskAndReplyWithResult(
                                 FROM_HERE,
                                 {base::MayBlock(),
                                  base::TaskPriority::USER_VISIBLE},
                                 base::BindOnce(&DecodeThumbnailFallback, path,
                                                max_px,
                                                static_cast<float>(scale)),
                                 base::BindOnce(
                                     [](scoped_refptr<base::SequencedTaskRunner>
                                            runner,
                                        base::OnceCallback<void(gfx::ImageSkia)>
                                            cb,
                                        gfx::ImageSkia img) {
                                       runner->PostTask(
                                           FROM_HERE,
                                           base::BindOnce(std::move(cb),
                                                          std::move(img)));
                                     },
                                     reply_runner, std::move(block_cb)));
                           }];
  }
}

void DecodeMediaPreviewMac(const base::FilePath& path,
                           bool is_video,
                           int max_px,
                           base::OnceCallback<void(gfx::ImageSkia)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](base::FilePath path, bool is_video, int max_px) -> gfx::ImageSkia {
            @autoreleasepool {
              if (is_video) {
                return DecodeVideoPoster(path, max_px);
              }
              return DecodeWithImageIO(path, max_px, 1.0f,
                                       /*check_size_limit=*/true);
            }
          },
          path, is_video, max_px),
      std::move(callback));
}

void OpenMediaInPreviewMac(const base::FilePath& path, bool is_video) {
  @autoreleasepool {
    NSURL* url = base::apple::FilePathToNSURL(path);
    if (!url) {
      return;
    }
    NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
    if (!is_video) {
      NSURL* preview_app =
          [workspace URLForApplicationWithBundleIdentifier:@"com.apple.Preview"];
      if (preview_app) {
        NSWorkspaceOpenConfiguration* config =
            [NSWorkspaceOpenConfiguration configuration];
        [workspace openURLs:@[ url ]
            withApplicationAtURL:preview_app
                   configuration:config
               completionHandler:nil];
        return;
      }
    }
    [workspace openURL:url];
  }
}

void RevealMediaInFinderMac(const base::FilePath& path) {
  @autoreleasepool {
    NSURL* url = base::apple::FilePathToNSURL(path);
    if (!url) {
      return;
    }
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
  }
}

void CopyMediaFileMac(const base::FilePath& path, bool is_video) {
  @autoreleasepool {
    NSURL* url = base::apple::FilePathToNSURL(path);
    if (!url) {
      return;
    }
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    NSMutableArray* objects = [NSMutableArray arrayWithObject:url];
    if (!is_video) {
      NSImage* image = [[NSImage alloc] initWithContentsOfURL:url];
      if (image) {
        [objects addObject:image];
      }
    }
    [pasteboard writeObjects:objects];
  }
}

void OpenFilesAndFoldersSettingsMac() {
  @autoreleasepool {
    NSURL* url = [NSURL
        URLWithString:@"x-apple.systempreferences:com.apple.preference.security"
                      @"?Privacy_FilesAndFolders"];
    if (url) {
      [[NSWorkspace sharedWorkspace] openURL:url];
    }
  }
}

void DragMediaFileMac(const base::FilePath& path,
                      const gfx::ImageSkia& thumbnail,
                      gfx::NativeView native_view) {
  if (!native_view || path.empty()) {
    return;
  }
  NSView* view = native_view.GetNativeNSView();
  if (!view || !view.window) {
    return;
  }
  NSPoint mouse_location = view.window.mouseLocationOutsideOfEventStream;
  if (views::Widget* widget =
          views::Widget::GetWidgetForNativeView(native_view)) {
    widget->ReleaseCapture();
  }
  views::NativeWidgetMacNSWindowHost* host =
      views::NativeWidgetMacNSWindowHost::GetFromNativeView(native_view);
  remote_cocoa::mojom::NativeWidgetNSWindow* mojo_window =
      host ? host->GetNSWindowMojo() : nullptr;
  if (!mojo_window) {
    return;
  }
  auto file_drag_data = remote_cocoa::mojom::FileDragData::New();
  file_drag_data->file_path = path;
  if (!thumbnail.isNull()) {
    file_drag_data->drag_image = thumbnail;
    const gfx::Size size = thumbnail.size();
    file_drag_data->image_offset =
        gfx::Vector2d(size.width() / 2, size.height() / 2);
  } else {
    file_drag_data->image_offset = gfx::Vector2d(8, 8);
  }
  gfx::PointF mouse_point(gfx::ClampFloatGeometry(mouse_location.x),
                          gfx::ClampFloatGeometry(mouse_location.y));
  mojo_window->BeginFileDrag(std::move(file_drag_data), mouse_point);
}

}  // namespace myarc
