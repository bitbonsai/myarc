#ifndef MYARC_WELCOME_INTRO_H_
#define MYARC_WELCOME_INTRO_H_

#include <cstdint>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/native_ui_types.h"

namespace myarc {

// The welcome's arrival on the desktop, before its window shows. A
// borderless, click-through window covering each display below its menu bar
// fades to 85% of myarc.dev's midnight over 750 ms, above other apps'
// windows; the menu bar never dims. With it, a blob pops in at the centre of
// the welcome window, which on first run is the main display's centre. It's
// about a quarter of the display's width: one heavily blurred shape in the
// site blue whose outline keeps morphing, a closed curve through 7 points
// whose radii wobble out of phase, with highlights in the ribbon logo's
// violet, pink and coral drifting across it at different speeds. At 1.3 s it
// swells unevenly past the window's edges on a spring, clipped by the
// window's rounded frame, and by 1.6 s its highlights have smeared into the
// welcome's background as it starts: the base with WelcomeView's blurred
// fields. The welcome window stays hidden until then, and is then ordered
// front without activating when the app isn't active, so a background launch
// lands on it too. It shows the same picture, so it shows under the field,
// and at 1.7 s, once the window has drawn, the field goes and the dim moves
// just behind the window. The dim stays there, still click-through, as long
// as the intro lives. The window's traffic lights fade in on their own at
// 1.9 s, 300 ms after the grow lands. The times are welcome_timeline.h's.
//
// Everything moves in Core Animation, at the display's rate. The outline's
// blur is worked out once, at the start, as a loop of images, so no frame
// blurs anything.
//
// The intro owns its dim windows and the blob, and it never deletes itself.
// Its owner deletes it once a button is pressed or the welcome closes:
// that shows the welcome window at once and fades the dim out over
// welcome::kDimFadeOut, after which Core Animation's completion closes the
// dim windows, holding no pointer to the intro. The windows are ordered
// front without activating the app, so a background launch shows them too.
// Mac only; UI sequence only.
class WelcomeIntro {
 public:
  // `window` is the welcome window and `base` its background's base colour,
  // as 0xRRGGBB. With `log`, each phase logs its start.
  WelcomeIntro(gfx::NativeWindow window, uint32_t base, bool log);
  WelcomeIntro(const WelcomeIntro&) = delete;
  WelcomeIntro& operator=(const WelcomeIntro&) = delete;
  ~WelcomeIntro();

  // t = 0 of welcome_timeline.h: when the dim and the blob were ordered
  // front, at the end of the constructor.
  base::TimeTicks start() const { return start_; }

 private:
  struct Cocoa;

  void Grow();
  void Land();
  void Shown();
  void Finish();
  // Runs `phase` at `at` from the start.
  void At(base::TimeDelta at, void (WelcomeIntro::*phase)());
  void Log(const char* phase) const;

  std::unique_ptr<Cocoa> cocoa_;
  const bool log_;
  base::TimeTicks start_;
  base::OneShotTimer timer_;
};

}  // namespace myarc

#endif  // MYARC_WELCOME_INTRO_H_
