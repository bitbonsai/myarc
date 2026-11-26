#ifndef MYARC_UPDATER_MAC_H_
#define MYARC_UPDATER_MAC_H_

#import <Foundation/Foundation.h>

@class NSMenu;
@class NSMenuItem;

namespace myarc {

// Inserts "Check for Updates…" into `app_menu` at `index` on release builds
// and starts Sparkle a few seconds later, or on the first click if that comes
// sooner. Does nothing when the app bundle has no Sparkle.framework, as in dev
// builds. Builds with test hooks start it only under
// --myarc-test-update-feed=<appcast URL>, right away.
void AddUpdaterMenuItem(NSMenu* app_menu, NSInteger index);

// Rebinds the macOS "About myArc" menu item to open myArc's native About
// window instead of Chromium's chrome://settings/help page.
void BindAboutMenuItem(NSMenuItem* about_item);

}  // namespace myarc

#endif  // MYARC_UPDATER_MAC_H_
