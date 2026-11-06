#ifndef MYARC_CONTROL_HOST_H_
#define MYARC_CONTROL_HOST_H_

namespace myarc {

// Turns on the browser control endpoint for `myarcctl` and its MCP server.
// Off without it. A Settings toggle and grant UI replace it later.
inline constexpr char kEnableControlSwitch[] = "myarc-enable-control";

// UI thread. With --myarc-enable-control, starts serving the control
// endpoint in $MYARC_CONTROL_DIR or the default directory, binding it on a
// dedicated background thread. Does nothing when already serving.
void MaybeStartControlHost();

// UI thread. Stops serving, answers waiting requests with cancelled and
// removes control.json and the socket before returning.
void StopControlHost();

}  // namespace myarc

#endif  // MYARC_CONTROL_HOST_H_
