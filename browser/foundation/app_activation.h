#ifndef MYARC_APP_ACTIVATION_H_
#define MYARC_APP_ACTIVATION_H_

#include <string>

#include "build/build_config.h"
#include "ui/gfx/native_ui_types.h"

namespace myarc {

// Whether this app is the one the user is using. A Mac app in the background
// still reports its key window as active, and showing a window with
// activation there would bring the app forward.
#if BUILDFLAG(IS_MAC)
bool IsAppActive();
// Lists `window` in the Window menu, or leaves it out. An ordered-out window
// of a Profile window group is left out.
void SetShownInWindowsMenu(gfx::NativeWindow window, bool shown);
// The Window menu's window titles, for capture logs.
std::string DescribeWindowsMenu();
#else
inline bool IsAppActive() {
  return true;
}
inline void SetShownInWindowsMenu(gfx::NativeWindow window, bool shown) {}
inline std::string DescribeWindowsMenu() {
  return {};
}
#endif

}  // namespace myarc

#endif  // MYARC_APP_ACTIVATION_H_
