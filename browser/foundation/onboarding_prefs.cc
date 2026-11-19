#include "myarc/onboarding_prefs.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "myarc/bridge.rs.h"
#include "myarc/test_buildflags.h"

namespace myarc {

namespace {

bool g_decided = false;

#if BUILDFLAG(MYARC_TEST_HOOKS)
// Logs the tour decided at the first workspace load, for captures of a fresh
// Profile and an existing workspace.
constexpr char kMyarcTestOnboarding[] = "myarc-test-onboarding";

const char* EffectName(foundation::OnboardingEffect effect) {
  switch (effect) {
    case foundation::OnboardingEffect::Show:
      return "show";
    case foundation::OnboardingEffect::OpenImport:
      return "open-import";
    case foundation::OnboardingEffect::Handoff:
      return "handoff";
    default:
      return "none";
  }
}
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)

}  // namespace

void RegisterOnboardingPrefs(PrefRegistrySimple& registry) {
  registry.RegisterStringPref(kOnboardingTourPref, std::string());
}

void DecideOnboardingOnLoad(PrefService* local_state,
                            bool first_run,
                            const std::string& profile_dir) {
  if (g_decided || !local_state ||
      !local_state->FindPreference(kOnboardingTourPref)) {
    return;
  }
  g_decided = true;
  const bool had_record = local_state->HasPrefPath(kOnboardingTourPref);
  const std::string saved =
      had_record ? local_state->GetString(kOnboardingTourPref) : std::string();
  auto decided = foundation::onboarding_on_load(had_record, saved, first_run);
  const std::string record(decided.record);
  if (!had_record || record != saved) {
    local_state->SetString(kOnboardingTourPref, record);
    // Saved before the tour shows anything, so a kill keeps the choice.
    local_state->CommitPendingWrite();
  }
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kMyarcTestOnboarding)) {
    LOG(WARNING) << "Test onboarding: profile=" << profile_dir
                 << " first_run=" << first_run
                 << " had_record=" << had_record
                 << " state=" << std::string(decided.tour.state)
                 << " step=" << static_cast<int>(decided.tour.step)
                 << " effect=" << EffectName(decided.effect)
                 << " animate=" << decided.animate << " record=" << record;
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

}  // namespace myarc
