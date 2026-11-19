#ifndef MYARC_ONBOARDING_PREFS_H_
#define MYARC_ONBOARDING_PREFS_H_

#include <optional>
#include <string>

#include "myarc/bridge.rs.h"

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
// --myarc-test-onboarding=<state> first replaces the saved record with a
// fresh one at that state: welcome, importing, reveal, practice1 to
// practice3, handoff or complete.
void DecideOnboardingOnLoad(PrefService* local_state,
                            bool first_run,
                            const std::string& profile_dir);

// The saved tour, or nothing while no load in this run decided it yet, so a
// relaunch always passes through the decision first.
std::optional<foundation::OnboardingTour> ReadOnboardingTour(
    PrefService& local_state);

// Applies `event` to the saved record and commits the new record before
// returning, so the caller runs the returned effect only once it is saved.
foundation::OnboardingTransition FeedOnboardingEvent(
    PrefService& local_state,
    const foundation::OnboardingEvent& event);

}  // namespace myarc

#endif  // MYARC_ONBOARDING_PREFS_H_
