#ifndef MYARC_APP_ACTIVATION_H_
#define MYARC_APP_ACTIVATION_H_

#include "build/build_config.h"

namespace myarc {

// Whether this app is the one the user is using. A Mac app in the background
// still reports its key window as active, and showing a window with
// activation there would bring the app forward.
#if BUILDFLAG(IS_MAC)
bool IsAppActive();
#else
inline bool IsAppActive() {
  return true;
}
#endif

}  // namespace myarc

#endif  // MYARC_APP_ACTIVATION_H_
