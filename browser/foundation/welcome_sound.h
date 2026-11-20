#ifndef MYARC_WELCOME_SOUND_H_
#define MYARC_WELCOME_SOUND_H_

#include <memory>

#include "base/time/time.h"

namespace myarc {

// The welcome's sound, an ambient bed that rises over about 5 s, holds and
// fades out by itself near its end. It starts with the dim and the blob, at
// t = 0 of welcome_timeline.h, and keeps playing under the welcome window.
// Deleting it fades it out over 600 ms; the player then stops from a posted
// task, never inside its own callback. Mac only.
class WelcomeSound {
 public:
  // Loads the sound and readies the player, so Start() plays at once.
  // Returns null when the user turned off "Play user interface sound
  // effects" or the file is missing. With `log`, logs why, and when it
  // starts.
  static std::unique_ptr<WelcomeSound> Load(bool log);

  // Plays it once at a modest volume, logging when from `start`.
  void Start(base::TimeTicks start);

  WelcomeSound(const WelcomeSound&) = delete;
  WelcomeSound& operator=(const WelcomeSound&) = delete;
  ~WelcomeSound();

 private:
  struct Cocoa;
  WelcomeSound(std::unique_ptr<Cocoa> cocoa, bool log);

  std::unique_ptr<Cocoa> cocoa_;
  const bool log_;
};

}  // namespace myarc

#endif  // MYARC_WELCOME_SOUND_H_
