#include "myarc/control_prefs.h"

#include "base/command_line.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace myarc {

void RegisterControlPrefs(PrefRegistrySimple& registry) {
  registry.RegisterBooleanPref(kControlEnabledPref, false);
}

bool IsControlForcedOn() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableControlSwitch);
}

bool IsControlEnabled(const PrefService* local_state) {
  return IsControlForcedOn() ||
         (local_state && local_state->FindPreference(kControlEnabledPref) &&
          local_state->GetBoolean(kControlEnabledPref));
}

}  // namespace myarc
