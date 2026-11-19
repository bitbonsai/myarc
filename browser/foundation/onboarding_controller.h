#ifndef MYARC_ONBOARDING_CONTROLLER_H_
#define MYARC_ONBOARDING_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "myarc/bridge.rs.h"
#include "myarc/coach_mark.h"
#include "myarc/welcome_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_observer.h"

class PrefService;
class Profile;

namespace myarc {

struct ArcImportOutcome;
class RevealFrames;

// The welcome tour in one browser window, owned by BrowserView next to the
// sidebar and destroyed before it. Feeds the tour's events to Rust through
// the saved record and runs the effects it returns. Only one window shows
// the welcome at a time. A committed import crossfades the welcome to the
// first imported Space's theme. The reveal follows Start browsing, or a
// committed import once its report closes: the window switches to the first
// imported Space, the sheet slides away and the sidebar's rows settle in,
// then `reveal_done` moves the tour to practice 1. Its coach mark shows 200 ms
// after the last row lands and points at the address area: opening a page
// from the command bar finishes the step. Back from it shows the reveal's end
// state with a card of its own. Practice 2, 3 and the handoff aren't built
// yet, so the tour ends as skipped once it gets there. UI sequence only.
//
// With test hooks, --myarc-test-onboarding-event=skip_tour, start_browsing
// or import presses that button a second after the sheet shows, or
// --myarc-test-onboarding-event-delay=<ms> after it; back, next or
// skip_tour presses that coach mark button the same way once a coach mark
// shows.
// --myarc-test-onboarding-import=<file> imports that export instead of
// asking for one, pressing Import in the preview.
// --myarc-test-onboarding-hold keeps the tour at the reveal after the
// crossfade, so a capture can kill the browser there.
// --myarc-test-onboarding-show-delay=<ms> holds the sheet back that long, and
// --myarc-test-reduce-motion plays it as Reduce Motion does. With
// --myarc-test-onboarding, the reveal logs its length and frame gaps.
class OnboardingController final : public views::ViewObserver {
 public:
  // `host` holds the sheet over `area`, the window's page area; both must
  // outlive this. `radius` gives the page area's corner radius.
  // `restore_focus` gives focus back to the window once the sheet closes.
  // `settle_rows` runs the sidebar's row settle after the delay it gets and
  // returns how long until its last row lands. `select_space` shows a Space
  // in the window. `anchor` gives the address area's bounds in `host`'s
  // coordinates, or an empty rect.
  using SettleRowsCallback =
      base::RepeatingCallback<base::TimeDelta(base::TimeDelta delay)>;
  using SelectSpaceCallback = base::RepeatingCallback<void(uint64_t space)>;
  using AnchorCallback = base::RepeatingCallback<gfx::Rect()>;
  OnboardingController(Profile& profile,
                       views::View& host,
                       views::View& area,
                       WelcomeView::RadiusCallback radius,
                       base::RepeatingClosure restore_focus,
                       SettleRowsCallback settle_rows,
                       SelectSpaceCallback select_space,
                       AnchorCallback anchor);
  OnboardingController(const OnboardingController&) = delete;
  OnboardingController& operator=(const OnboardingController&) = delete;
  ~OnboardingController() override;

  // Shows or ends the tour as the saved record says. Call whenever the
  // window's workspace changes; the first load decides the record.
  void Refresh();

  // The command bar opened a page in this window: finishes practice 1.
  void OnCommandBarOpenedPage();

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  void ShowWelcome(bool offers_import);
  void CloseWelcome();
  // Gives focus back to the window if the sheet holds it.
  void ReleaseFocus();
  void OnAction(WelcomeView::Action action);
  void OnImportDone(const ArcImportOutcome& outcome);
  // Crossfades the welcome to the theme of `space`, then runs the reveal.
  void ShowImported(uint64_t space);
  void OpenImport();
  void OnReportClosed();
  // The sheet slides away while the rows settle, then FinishReveal feeds
  // `reveal_done`.
  void StartReveal();
  void FinishReveal();
  // Shows the coach mark for the saved `tour`, which is at the reveal or
  // practice 1, after `delay`. Keeps a card already showing for it.
  void ShowCoach(const foundation::OnboardingTour& tour, base::TimeDelta delay);
  // Adds the card ShowCoach set up.
  void OpenCoach();
  void CloseCoach();
  void PlaceCoach();
  void OnCoachAction(CoachMark::Action action);
  // Runs the effect of a saved transition.
  void Run(const foundation::OnboardingTransition& transition);
  void PressForTest();
  void PressCoachForTest();

  const raw_ptr<Profile> profile_;
  raw_ptr<views::View> host_;
  raw_ptr<views::View> area_;
  const WelcomeView::RadiusCallback radius_;
  const base::RepeatingClosure restore_focus_;
  const SettleRowsCallback settle_rows_;
  const SelectSpaceCallback select_space_;
  const AnchorCallback anchor_;
  raw_ptr<WelcomeView> sheet_ = nullptr;
  raw_ptr<CoachMark> coach_ = nullptr;
  // The state and step the coach mark shows, or is about to show.
  std::string coach_state_;
  uint8_t coach_step_ = 0;
  bool coach_offers_back_ = false;
  base::OneShotTimer coach_timer_;
  bool importing_ = false;
  // The welcome crossfades to the imported theme; the reveal follows.
  bool recolouring_ = false;
  // The import's report shows; the reveal waits for it to close.
  bool report_open_ = false;
  bool revealing_ = false;
  base::OneShotTimer reveal_timer_;
  base::TimeTicks reveal_start_;
  // Counts the reveal's frames for the test log.
  std::unique_ptr<RevealFrames> reveal_frames_;
  [[maybe_unused]] bool test_event_scheduled_ = false;
  [[maybe_unused]] bool test_show_delayed_ = false;
  [[maybe_unused]] bool test_coach_event_scheduled_ = false;
  base::ScopedObservation<views::View, views::ViewObserver> area_observation_{
      this};
  base::WeakPtrFactory<OnboardingController> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_ONBOARDING_CONTROLLER_H_
