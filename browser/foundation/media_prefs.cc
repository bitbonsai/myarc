#include "myarc/media_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace myarc {

void RegisterMediaProfilePrefs(user_prefs::PrefRegistrySyncable& registry) {
  registry.RegisterBooleanPref(kMediaLibraryEnabledPref, false);
}

}  // namespace myarc
