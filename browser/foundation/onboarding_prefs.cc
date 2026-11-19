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
// Profile and an existing workspace. With a state, seeds the record first.
constexpr char kMyarcTestOnboarding[] = "myarc-test-onboarding";

// A fresh record at `state`, as onboarding.rs encodes it, or empty for an
// unknown state.
std::string SeedRecord(const std::string& state) {
  std::string name = state;
  int step = 0;
  if (state.size() == 9 && state.starts_with("practice") && state[8] >= '1' &&
      state[8] <= '3') {
    name = "practice";
    step = state[8] - '0';
  } else if (state != "welcome" && state != "importing" && state != "reveal" &&
             state != "handoff" && state != "complete") {
    return std::string();
  }
  return "{\"v\":1,\"state\":\"" + name +
         "\",\"step\":" + std::to_string(step) +
         ",\"done\":[],\"skipped\":[],\"import\":null}";
}

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
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (const std::string seed = SeedRecord(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              kMyarcTestOnboarding));
      !seed.empty()) {
    local_state->SetString(kOnboardingTourPref, seed);
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
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
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding)) {
    LOG(WARNING) << "Test onboarding: profile=" << profile_dir
                 << " first_run=" << first_run << " had_record=" << had_record
                 << " state=" << std::string(decided.tour.state)
                 << " step=" << static_cast<int>(decided.tour.step)
                 << " effect=" << EffectName(decided.effect)
                 << " animate=" << decided.animate << " record=" << record;
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

std::optional<foundation::OnboardingTour> ReadOnboardingTour(
    PrefService& local_state) {
  if (!local_state.FindPreference(kOnboardingTourPref) ||
      !local_state.HasPrefPath(kOnboardingTourPref)) {
    return std::nullopt;
  }
  return foundation::onboarding_decode(
      local_state.GetString(kOnboardingTourPref));
}

foundation::OnboardingTransition FeedOnboardingEvent(
    PrefService& local_state,
    const foundation::OnboardingEvent& event) {
  const std::string saved = local_state.GetString(kOnboardingTourPref);
  auto next = foundation::onboarding_event(saved, event);
  const std::string record(next.record);
  if (record != saved) {
    local_state.SetString(kOnboardingTourPref, record);
    // Every event is saved before its effect runs.
    local_state.CommitPendingWrite();
  }
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding)) {
    LOG(WARNING) << "Test onboarding: event state="
                 << std::string(next.tour.state)
                 << " effect=" << EffectName(next.effect)
                 << " record=" << record;
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
  return next;
}

}  // namespace myarc
