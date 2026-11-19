#ifndef MYARC_ONBOARDING_CONTROLLER_H_
#define MYARC_ONBOARDING_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "myarc/bridge.rs.h"
#include "myarc/welcome_view.h"
#include "ui/views/view_observer.h"

class PrefService;
class Profile;

namespace myarc {

struct ArcImportOutcome;

// The welcome tour in one browser window, owned by BrowserView next to the
// sidebar and destroyed before it. Feeds the tour's events to Rust through
// the saved record and runs the effects it returns. Only one window shows
// the welcome at a time. A committed import crossfades the welcome to the
// first imported Space's theme. Later steps (reveal, practice, handoff)
// aren't built yet, so a tour that reaches one of them ends as skipped. UI
// sequence only.
//
// With test hooks, --myarc-test-onboarding-event=skip_tour, start_browsing
// or import presses that button a second after the sheet shows, or
// --myarc-test-onboarding-event-delay=<ms> after it.
// --myarc-test-onboarding-import=<file> imports that export instead of
// asking for one, pressing Import in the preview.
// --myarc-test-onboarding-hold keeps the tour at the reveal after the
// crossfade, so a capture can kill the browser there.
// --myarc-test-onboarding-show-delay=<ms> holds the sheet back that long, and
// --myarc-test-reduce-motion plays it as Reduce Motion does.
class OnboardingController final : public views::ViewObserver {
 public:
  // `host` holds the sheet over `area`, the window's page area; both must
  // outlive this. `radius` gives the page area's corner radius.
  // `restore_focus` gives focus back to the window once the sheet closes.
  OnboardingController(Profile& profile,
                       views::View& host,
                       views::View& area,
                       WelcomeView::RadiusCallback radius,
                       base::RepeatingClosure restore_focus);
  OnboardingController(const OnboardingController&) = delete;
  OnboardingController& operator=(const OnboardingController&) = delete;
  ~OnboardingController() override;

  // Shows or ends the tour as the saved record says. Call whenever the
  // window's workspace changes; the first load decides the record.
  void Refresh();

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  void ShowWelcome(bool offers_import);
  void CloseWelcome();
  void OnAction(WelcomeView::Action action);
  void OnImportDone(const ArcImportOutcome& outcome);
  // Crossfades the welcome to the theme of `space`, then runs the reveal.
  void ShowImported(uint64_t space);
  void OpenImport();
  // Runs the effect of a saved transition.
  void Run(const foundation::OnboardingTransition& transition);
  void PressForTest();

  const raw_ptr<Profile> profile_;
  raw_ptr<views::View> host_;
  raw_ptr<views::View> area_;
  const WelcomeView::RadiusCallback radius_;
  const base::RepeatingClosure restore_focus_;
  raw_ptr<WelcomeView> sheet_ = nullptr;
  bool importing_ = false;
  // The welcome crossfades to the imported theme; the reveal follows.
  bool recolouring_ = false;
  [[maybe_unused]] bool test_event_scheduled_ = false;
  [[maybe_unused]] bool test_show_delayed_ = false;
  base::ScopedObservation<views::View, views::ViewObserver> area_observation_{
      this};
  base::WeakPtrFactory<OnboardingController> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_ONBOARDING_CONTROLLER_H_
