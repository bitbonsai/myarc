#ifndef MYARC_WELCOME_RIBBON_H_
#define MYARC_WELCOME_RIBBON_H_

#include <memory>

#include "base/time/time.h"
#include "ui/gfx/image/image_skia.h"

namespace myarc {

// The welcome's 3D ribbon: a short HEVC-with-alpha video of the logo's
// showpiece (myarc_welcome_ribbon.mov, made by tools/render_welcome_ribbon.mjs
// and tools/encode_hevc_alpha.swift), read one frame at a time. It plays
// once: Next() returns each frame in turn and an empty image after the last,
// which the caller keeps showing. Frames are 2x images of `size()` DIP.
// Mac only; UI sequence only.
class WelcomeRibbon {
 public:
  // Null when the video is missing or can't be read.
  static std::unique_ptr<WelcomeRibbon> Load();
  // The last frame alone, for Reduce Motion, or an empty image.
  static gfx::ImageSkia LastFrame();

  WelcomeRibbon(const WelcomeRibbon&) = delete;
  WelcomeRibbon& operator=(const WelcomeRibbon&) = delete;
  ~WelcomeRibbon();

  gfx::ImageSkia Next();
  static base::TimeDelta FrameInterval() { return base::Hertz(60); }

 private:
  struct Cocoa;
  explicit WelcomeRibbon(std::unique_ptr<Cocoa> cocoa);
  std::unique_ptr<Cocoa> cocoa_;
};

}  // namespace myarc

#endif  // MYARC_WELCOME_RIBBON_H_
