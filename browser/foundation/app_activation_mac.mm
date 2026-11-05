#import <Cocoa/Cocoa.h>

#include "myarc/app_activation.h"

#include "base/strings/sys_string_conversions.h"

namespace myarc {

bool IsAppActive() {
  return NSApp.active;
}

void SetShownInWindowsMenu(gfx::NativeWindow window, bool shown) {
  NSWindow* ns_window = window.GetNativeNSWindow();
  ns_window.excludedFromWindowsMenu = !shown;
}

std::string DescribeWindowsMenu() {
  std::string titles;
  for (NSMenuItem* item in NSApp.windowsMenu.itemArray) {
    if ([item.target isKindOfClass:[NSWindow class]] ||
        item.action == @selector(makeKeyAndOrderFront:)) {
      titles += base::SysNSStringToUTF8(item.title) + ";";
    }
  }
  return titles;
}

}  // namespace myarc
