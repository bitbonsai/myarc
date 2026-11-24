#ifndef MYARC_UPDATER_MAC_H_
#define MYARC_UPDATER_MAC_H_

#import <Foundation/Foundation.h>

@class NSMenu;

namespace myarc {

// Inserts "Check for Updates…" into `app_menu` at `index` on release builds
// and starts Sparkle a few seconds later, or on the first click if that comes
// sooner. Does nothing when the app bundle has no Sparkle.framework, as in dev
// builds. Builds with test hooks start it only under
// --myarc-test-update-feed=<appcast URL>, right away.
void AddUpdaterMenuItem(NSMenu* app_menu, NSInteger index);

}  // namespace myarc

#endif  // MYARC_UPDATER_MAC_H_
