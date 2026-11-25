#ifndef MYARC_MEDIA_LIBRARY_MAC_H_
#define MYARC_MEDIA_LIBRARY_MAC_H_

#include <cstddef>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_ui_types.h"

namespace myarc {

inline constexpr int kMediaThumbnailMaxPx = 280;
inline constexpr int kMediaLightboxMaxPx = 2048;
inline constexpr int64_t kMaxMediaPreviewBytes = 64LL * 1024 * 1024;

// Resolves macOS's com.apple.screencapture location, falling back to ~/Desktop.
base::FilePath GetMacScreenshotsDirectory();

// Generates a bounded thumbnail via QuickLookThumbnailing with ImageIO fallback
// for raster images. Invokes `callback` on the caller's sequence.
void GenerateMediaThumbnailMac(
    const base::FilePath& path,
    int max_px,
    base::OnceCallback<void(gfx::ImageSkia)> callback);

// Decodes a high-resolution preview (capped at `max_px`, skipping files larger
// than kMaxMediaPreviewBytes for raster images). Videos decode a poster frame
// via AVAssetImageGenerator. Invokes `callback` on the caller's sequence.
void DecodeMediaPreviewMac(const base::FilePath& path,
                           bool is_video,
                           int max_px,
                           base::OnceCallback<void(gfx::ImageSkia)> callback);

// Opens `path` in Preview (for images) or the default handler (for videos).
void OpenMediaInPreviewMac(const base::FilePath& path, bool is_video);

// Reveals `path` in Finder.
void RevealMediaInFinderMac(const base::FilePath& path);

// Copies `path` as a file URL (and raster image data for images) to the
// macOS pasteboard.
void CopyMediaFileMac(const base::FilePath& path, bool is_video);

// Opens macOS System Settings > Privacy & Security > Files and Folders.
void OpenFilesAndFoldersSettingsMac();

// Starts a native macOS file drag from `native_view` for `path`.
void DragMediaFileMac(const base::FilePath& path,
                      const gfx::ImageSkia& thumbnail,
                      gfx::NativeView native_view);

}  // namespace myarc

#endif  // MYARC_MEDIA_LIBRARY_MAC_H_
