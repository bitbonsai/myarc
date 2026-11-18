#include "myarc/little_arc_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace myarc {

void RegisterLittleArcPrefs(PrefRegistrySimple& registry) {
  registry.RegisterBooleanPref(kLittleArcLinksPref, true);
  registry.RegisterBooleanPref(kPeekEnabledPref, true);
}

bool IsLittleArcLinksEnabled(const PrefService* local_state) {
  return !local_state || !local_state->FindPreference(kLittleArcLinksPref) ||
         local_state->GetBoolean(kLittleArcLinksPref);
}

bool IsPeekEnabled(const PrefService* local_state) {
  return !local_state || !local_state->FindPreference(kPeekEnabledPref) ||
         local_state->GetBoolean(kPeekEnabledPref);
}

}  // namespace myarc
