#ifndef MYARC_CONTROL_HOST_H_
#define MYARC_CONTROL_HOST_H_

#include <string>

#include "myarc/control_prefs.h"

class Profile;

namespace myarc {

// UI thread. While browser control is on (the Local State pref behind
// Settings > Advanced, or --myarc-enable-control), starts serving the control
// endpoint in $MYARC_CONTROL_DIR or the default directory, binding it on a
// dedicated background thread. Does nothing when already serving. The first
// call also starts watching the pref, so the host starts and stops as the
// switch changes.
void MaybeStartControlHost();

// UI thread. Stops serving, answers waiting requests with cancelled, closes
// any approval prompt and removes control.json and the socket before
// returning.
void StopControlHost();

// UI thread. The approved client controlling the agent Space of `profile`,
// cleaned for display: connected now or within the last 30 seconds. Empty
// when none, or when control runs in another Profile.
std::u16string ActiveControlClient(const Profile& profile);

// UI thread. Disconnects the active client, cancels its waiting requests and
// forgets its approval, so its next connection asks the user again.
void StopControlClient();

}  // namespace myarc

#endif  // MYARC_CONTROL_HOST_H_
