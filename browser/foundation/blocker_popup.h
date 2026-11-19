#ifndef MYARC_BLOCKER_POPUP_H_
#define MYARC_BLOCKER_POPUP_H_

class BrowserWindowInterface;

namespace myarc {

// Window > Ad Blocking for This Site…: opens the site info popover of the
// active tab, whose "Block ads on this site" row (blocker_switch.h) turns
// blocking off or on for the site. For a page without that row, such as a
// new tab, it opens the blocker's popup instead.
void ShowBlockerSwitch(BrowserWindowInterface& browser);

// Opens the site info popover of the active tab, without activating the
// window when the app is in the background. Returns false when the tab isn't
// an http(s) page or has no address field.
bool ShowSiteInfo(BrowserWindowInterface& browser);

// Opens the bundled blocker's own popup for the active tab, under the address
// field (myArc hides the extensions toolbar), with its four filtering levels.
// When the blocker is removed, turned off or not allowed in a private window,
// this opens the Extensions page instead, so the command never does nothing.
void ShowBlockerPopup(BrowserWindowInterface& browser);

// --myarc-capture-blocker-popup[=<ms>]: opens the popup once, <ms> (default
// 4000) after the first window's commands are set up, for captures.
// Test-hook builds also take --myarc-test-blocker-hook=<file>: the first
// window polls <file> for one word and removes it. "switch" runs
// ShowBlockerSwitch, "toggle" presses the open row's switch and "popup" runs
// ShowBlockerPopup. <file>.log gets "hook <word> <outcome>" for each, and
// "<host> level=<n> on|off|unknown" for each state a row learns or sets.
void MaybeScheduleBlockerPopupForCapture(BrowserWindowInterface& browser);

}  // namespace myarc

#endif  // MYARC_BLOCKER_POPUP_H_
