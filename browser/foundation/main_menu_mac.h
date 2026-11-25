#ifndef MYARC_MAIN_MENU_MAC_H_
#define MYARC_MAIN_MENU_MAC_H_

@class NSApplication;
@class NSMenu;

namespace myarc {

// Reshapes Chromium's macOS menu bar into Arc's layout (myArc, File, Edit,
// View, Spaces, Tabs, Archive, Extensions, Window, Help) and attaches the
// dynamic Spaces menu delegate. Safe to call in unit tests with nil `nsapp`.
void ApplyMainMenu(NSMenu* main_menu, NSApplication* nsapp);

}  // namespace myarc

#endif  // MYARC_MAIN_MENU_MAC_H_
