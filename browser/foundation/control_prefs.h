#ifndef MYARC_CONTROL_PREFS_H_
#define MYARC_CONTROL_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace myarc {

// Local State pref behind Settings > Advanced > "Allow agents to control
// myArc". Off by default; the browser serves the control endpoint for
// `myarc` and its MCP server only while it is on.
inline constexpr char kControlEnabledPref[] = "myarc.control.enabled";

// Developer override: turns browser control on for this run whatever the
// pref says. The Settings switch shows on and can't be changed then.
inline constexpr char kEnableControlSwitch[] = "myarc-enable-control";

void RegisterControlPrefs(PrefRegistrySimple& registry);

// True when --myarc-enable-control was passed.
bool IsControlForcedOn();

// True when the pref is on or the switch was passed. `local_state` may be
// null.
bool IsControlEnabled(const PrefService* local_state);

}  // namespace myarc

#endif  // MYARC_CONTROL_PREFS_H_
