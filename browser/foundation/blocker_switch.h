#ifndef MYARC_BLOCKER_SWITCH_H_
#define MYARC_BLOCKER_SWITCH_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}

namespace views {
class View;
}

namespace myarc {

// The site info popover's "Block ads on this site" row, for the page in
// `contents`. Its switch reads and sets the bundled blocker's own filtering
// level for the page's host, through the same messages the blocker's popup
// sends its service worker: on is the Profile's default level, off is no
// filtering. A change reloads the page when the blocker's "reload on change"
// setting is on, as its popup does. "More blocker settings…" under the switch
// opens the blocker's popup, with its four levels.
//
// Returns null when the page isn't http(s) or the Profile's blocker is
// removed, turned off or not allowed in a private window.
std::unique_ptr<views::View> CreateBlockerSwitchRow(
    content::WebContents* contents);

// Test hooks: presses the switch of the newest open row, as a click does.
// Returns false when no row is open or its state hasn't loaded.
bool PressBlockerSwitchForTesting();

// Test hooks: `observer` gets "<host> level=<n> on|off|unknown" each time a
// row learns or sets its host's level.
void SetBlockerSwitchObserverForTesting(
    base::RepeatingCallback<void(const std::string&)> observer);

}  // namespace myarc

#endif  // MYARC_BLOCKER_SWITCH_H_
