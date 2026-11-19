#ifndef MYARC_ONBOARDING_PREFS_H_
#define MYARC_ONBOARDING_PREFS_H_

#include <string>

class PrefRegistrySimple;
class PrefService;

namespace myarc {

// Local State pref holding the welcome tour record, one per install. Rust
// (onboarding.rs) encodes and decodes it and treats it as untrusted; an
// unreadable record reads as a finished tour.
inline constexpr char kOnboardingTourPref[] = "myarc.onboarding.tour";

void RegisterOnboardingPrefs(PrefRegistrySimple& registry);

// Decides the tour the first time a Profile's workspace loads in this
// process, and saves the record before anything shows. `first_run` means the
// load found no workspace file and no backup. Only a fresh workspace with no
// record starts the tour; an existing workspace gets a finished one. Later
// loads, as other Profiles open, leave the record alone. `local_state` may
// be null, which decides nothing.
//
// With test hooks, --myarc-test-onboarding logs the decision for captures.
void DecideOnboardingOnLoad(PrefService* local_state,
                            bool first_run,
                            const std::string& profile_dir);

}  // namespace myarc

#endif  // MYARC_ONBOARDING_PREFS_H_
