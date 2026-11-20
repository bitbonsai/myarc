#ifndef MYARC_WELCOME_INTRO_H_
#define MYARC_WELCOME_INTRO_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/native_ui_types.h"

namespace myarc {

// The welcome's arrival on the desktop, before its window shows. A
// borderless, click-through window covering each display below its menu bar
// fades to 85% of myarc.dev's midnight over 750 ms, above other apps'
// windows; the menu bar never dims. With it, a blob comes in at the centre of
// the main display, about a quarter of its width: one heavily blurred shape
// in the site blue whose outline keeps morphing, a closed curve through 7
// points whose radii wobble out of phase, with highlights in the ribbon
// logo's violet, pink and coral drifting across it. After 3 s it swells
// unevenly past the welcome window's edges over 300 ms, clipped by the
// window's rounded frame, and its highlights smear into the window's field.
// The field then crossfades into the window while the dim lifts, over
// 300 ms. The welcome window stays hidden until the blob lands, and is then
// ordered front without activating when the app isn't active, so a
// background launch lands on it too.
//
// The intro owns its dim windows and the blob, and deleting it removes them
// at once and shows the welcome window. It never deletes itself: `done` runs
// posted once the dim has lifted, and its owner deletes it from there. The
// windows are ordered front without activating the app, so a background
// launch shows them too. Mac only; UI sequence only.
class WelcomeIntro {
 public:
  // `window` is the welcome window. With `log`, each phase logs its start.
  WelcomeIntro(gfx::NativeWindow window, base::OnceClosure done, bool log);
  WelcomeIntro(const WelcomeIntro&) = delete;
  WelcomeIntro& operator=(const WelcomeIntro&) = delete;
  ~WelcomeIntro();

 private:
  struct Cocoa;

  void Grow();
  void Lift();
  void Finish();
  void Log(const char* phase) const;

  std::unique_ptr<Cocoa> cocoa_;
  base::OnceClosure done_;
  const bool log_;
  const base::TimeTicks start_;
  base::OneShotTimer timer_;
};

}  // namespace myarc

#endif  // MYARC_WELCOME_INTRO_H_
