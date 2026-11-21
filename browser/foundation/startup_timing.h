#ifndef MYARC_STARTUP_TIMING_H_
#define MYARC_STARTUP_TIMING_H_

#include "base/time/time.h"

namespace content {
class NavigationHandle;
}

namespace myarc {

// --myarc-startup-timing logs, once each, how long after the process started
// the first browser window showed and the first page painted, and how long
// the first page load waited for the blocker's rules. Lines start with
// "myarc-startup-timing". Off by default, and a no-op without the switch.

// A browser window became visible.
void NoteWindowShown();

// Watches the page of a main-frame navigation for the first paint.
void WatchFirstPaint(content::NavigationHandle& handle);

// The blocker held the first page load for `held`.
void NoteBlockerHold(base::TimeDelta held);

}  // namespace myarc

#endif  // MYARC_STARTUP_TIMING_H_
