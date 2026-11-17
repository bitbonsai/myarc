#ifndef MYARC_LITTLE_ARC_PREFS_H_
#define MYARC_LITTLE_ARC_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace myarc {

// Local State pref behind Settings > Links > "Links from other apps open in
// Little Arc". On by default, as in Arc. Off, a link another app opens goes
// to a main window as a normal tab.
inline constexpr char kLittleArcLinksPref[] =
    "myarc.little_arc.links_from_other_apps";

void RegisterLittleArcPrefs(PrefRegistrySimple& registry);

// True when the pref is on. `local_state` may be null, which reads as on.
bool IsLittleArcLinksEnabled(const PrefService* local_state);

}  // namespace myarc

#endif  // MYARC_LITTLE_ARC_PREFS_H_
