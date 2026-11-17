#ifndef MYARC_BLOCKER_POPUP_H_
#define MYARC_BLOCKER_POPUP_H_

class BrowserWindowInterface;

namespace myarc {

// Window > Ad Blocking for This Site…: opens the bundled blocker's own popup
// for the active tab, under the address field (myArc hides the extensions
// toolbar). Its switch turns blocking off or on for the current site. When
// the blocker is removed, turned off or not allowed in a private window, this
// opens the Extensions page instead, so the command never does nothing.
void ShowBlockerPopup(BrowserWindowInterface& browser);

// --myarc-capture-blocker-popup[=<ms>]: opens the popup once, <ms> (default
// 4000) after the first window's commands are set up, for captures.
void MaybeScheduleBlockerPopupForCapture(BrowserWindowInterface& browser);

}  // namespace myarc

#endif  // MYARC_BLOCKER_POPUP_H_
