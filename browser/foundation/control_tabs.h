#ifndef MYARC_CONTROL_TABS_H_
#define MYARC_CONTROL_TABS_H_

#include "myarc/workspace_service.h"

namespace myarc {

class BrowserWorkspaceAdapter;

// UI thread. Runs a browser control tab command that the core has checked
// against the agent Space, with `target` as the window the request runs in.
// List, activate and close answer `reply` at once. Open, navigate, back,
// forward and reload answer once the tab's navigation commits, or with
// outcome_unknown and the latest state when it ends without committing or
// the wait runs out. Waiting never blocks the UI thread.
void RunTabAction(BrowserWorkspaceAdapter& target,
                  rust::Box<foundation::ControlReply> reply,
                  const foundation::ControlPlan& plan);

}  // namespace myarc

#endif  // MYARC_CONTROL_TABS_H_
