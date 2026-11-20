#ifndef MYARC_UPDATER_MAC_H_
#define MYARC_UPDATER_MAC_H_

#import <Foundation/Foundation.h>

@class NSMenu;

namespace myarc {

// Starts Sparkle on nightly builds and inserts "Check for Updates…" into
// `app_menu` at `index`. Does nothing when the app bundle has no
// Sparkle.framework, as in dev builds. Builds with test hooks start it only
// under --myarc-test-update-feed=<appcast URL>.
void AddUpdaterMenuItem(NSMenu* app_menu, NSInteger index);

}  // namespace myarc

#endif  // MYARC_UPDATER_MAC_H_
