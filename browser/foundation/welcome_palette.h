#ifndef MYARC_WELCOME_PALETTE_H_
#define MYARC_WELCOME_PALETTE_H_

#include <cstdint>

namespace myarc::welcome {

// myarc.dev's midnight, the welcome window's base, its blue and its ivory.
inline constexpr uint32_t kMidnight = 0x101C2B;
inline constexpr uint32_t kSiteBlue = 0x034BF4;
inline constexpr uint32_t kIvory = 0xF5F2EC;

// The large blurred fields over the midnight base, in the site blue and the
// ribbon logo's colours (sampled from design/brand/assets/myarc-matte.png).
// Each is a radial gradient centred at (x, y), shares of the window's width
// and height from its top left, with a radius that's a share of the window's
// longer side. WelcomeIntro paints them where they start, so the blob's grow
// hands off to the window with no jump; the window then drifts each one by
// `reach` (a share of the window's size) on a loop of `period` seconds.
struct Field {
  uint32_t rgb;
  float alpha;
  float x;
  float y;
  float radius;
  float reach;
  double period;
};
inline constexpr Field kFields[] = {
    {kSiteBlue, 0.75f, 0.24f, 0.22f, 0.62f, 0.08f, 24},
    {0x532BFC, 0.6f, 0.82f, 0.8f, 0.55f, 0.07f, 28},   // violet
    {0xF54BE4, 0.3f, 0.86f, 0.12f, 0.38f, 0.09f, 21},  // pink
};
// Each field's gradient: full alpha at the centre, `kFieldMid` of it at
// `kFieldMidStop` of the radius, none at the edge.
inline constexpr float kFieldMid = 0.45f;
inline constexpr float kFieldMidStop = 0.45f;

}  // namespace myarc::welcome

#endif  // MYARC_WELCOME_PALETTE_H_
