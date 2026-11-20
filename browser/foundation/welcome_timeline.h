#ifndef MYARC_WELCOME_TIMELINE_H_
#define MYARC_WELCOME_TIMELINE_H_

#include "base/time/time.h"

// When each part of the welcome's arrival happens, from t = 0, when the
// sound, the dim and the blob all start. The sound
// (design/brand/sounds/welcome.m4a, 16.2 s with a 0.4 s fade-in) sets them:
// its first accent lands at 1.6 s and its second at 4.3 s. WelcomeIntro,
// WelcomeView and WelcomeSound all read this table, and the times hold
// whether or not the sound plays.
namespace myarc::welcome {

// The blob holds, then swells, and the window's frame clips it on the first
// accent.
inline constexpr base::TimeDelta kGrowStart = base::Milliseconds(1300);
inline constexpr base::TimeDelta kGrowLands = base::Milliseconds(1600);
// The window shows under the grown blob as the dim lifts, and the blob's
// copy of its background goes once the window has drawn.
inline constexpr base::TimeDelta kWindowShown = base::Milliseconds(1700);
inline constexpr base::TimeDelta kDimGone = base::Milliseconds(1900);
// The traffic lights fade in 300 ms after the grow lands.
inline constexpr base::TimeDelta kTrafficLights = base::Milliseconds(1900);
inline constexpr base::TimeDelta kTrafficLightsFade = base::Milliseconds(250);

// The ribbon logo: it rises in over 1.1 s, drifts once in 1.2 s and fades as
// the drift returns, on screen from 1.7 to 4.0 s.
inline constexpr base::TimeDelta kMarkIn = base::Milliseconds(1700);
inline constexpr base::TimeDelta kMarkRise = base::Milliseconds(1100);
inline constexpr base::TimeDelta kMarkDrift = base::Milliseconds(1200);
inline constexpr base::TimeDelta kMarkFadeAt = base::Milliseconds(3600);
inline constexpr base::TimeDelta kMarkFade = base::Milliseconds(400);

// The headline slides in on the second accent, and the choice fades in
// 0.4 s after.
inline constexpr base::TimeDelta kHeadlineAt = base::Milliseconds(4300);
inline constexpr base::TimeDelta kHeadlineSlide = base::Milliseconds(800);
inline constexpr base::TimeDelta kChoiceAt = base::Milliseconds(4700);
inline constexpr base::TimeDelta kChoiceFade = base::Milliseconds(400);

// The sound fades out over this long once the user picks a button or the
// window closes, unless it has ended on its own.
inline constexpr base::TimeDelta kSoundFadeOut = base::Milliseconds(600);

}  // namespace myarc::welcome

#endif  // MYARC_WELCOME_TIMELINE_H_
