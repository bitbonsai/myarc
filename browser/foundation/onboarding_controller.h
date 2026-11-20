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
// state with a card of its own. Practice 2 points at the tab practice 1
// opened, or the current one, and finishes once it's pinned. Practice 3
// points at the footer's Spaces, or its "+" with one Space, and finishes on
// a switch or a new Space. A card leaving fades out over 120 ms while the
// next comes in. The handoff isn't built yet, so the tour ends once it gets
// there. UI sequence only.
//
// With test hooks, --myarc-test-onboarding-event=skip_tour, start_browsing
// or import presses that button a second after the sheet shows, or
// --myarc-test-onboarding-event-delay=<ms> after it; back, next or
// skip_tour presses that coach mark button the same way once a coach mark
// shows, and do does what each card asks: Cmd+D, Ctrl+number, or Create a
// Space then the panel's Create.
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
  // in the window. `anchor` gives the bounds of what a card points at in
  // `host`'s coordinates, or an empty rect. `window` reaches the rest.
  using SettleRowsCallback =
      base::RepeatingCallback<base::TimeDelta(base::TimeDelta delay)>;
  using SelectSpaceCallback = base::RepeatingCallback<void(uint64_t space)>;
  // What a coach mark points at. kItem is a sidebar row or tile.
  enum class Target { kAddress, kItem, kSpaces, kCreate };
  using AnchorCallback =
      base::RepeatingCallback<gfx::Rect(Target target, uint64_t item)>;
  struct Window {
    // The window's sidebar model.
    base::RepeatingCallback<foundation::SidebarResult()> model;
    // True when the window shows a web page. The selected Today row's
    // saved address can trail its live page.
    base::RepeatingCallback<bool()> shown_page_is_web;
    // Opens the Create a Space panel.
    base::RepeatingClosure create_space;
    // Test hooks only: "pin" runs Cmd+D's command, "space:<n>" Ctrl+<n>'s,
    // and "create" presses Create in the open Create a Space panel.
    base::RepeatingCallback<void(const std::string&)> act_for_test;
  };
  OnboardingController(Profile& profile,
                       views::View& host,
                       views::View& area,
                       WelcomeView::RadiusCallback radius,
                       base::RepeatingClosure restore_focus,
                       SettleRowsCallback settle_rows,
                       SelectSpaceCallback select_space,
                       AnchorCallback anchor,
                       Window window);
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
  // What a coach mark shows.
  enum class Card {
    kNone,
    kReveal,
    kCommandBar,
    kPin,
    kPinned,
    kSwitch,
    kCreate
  };
  // Shows the coach mark for the saved `tour`, at the reveal or a practice
  // step, after `delay`. A card already showing for that step stays, and
  // the workspace is checked for the step's progress instead.
  void ShowCoach(const foundation::OnboardingTour& tour, base::TimeDelta delay);
  // Adds the card ShowCoach set up, or feeds the event its step calls for.
  void OpenCoach();
  void AddCard(Card card);
  // The card practice 2 or 3 shows now, or kNone once it fed an event.
  // `peek` only looks while practice 2 waits for a page, and never skips.
  Card PracticeCard(bool starting, bool peek = false);
  // Swaps the card when the step's card changed, for example an unpin
  // under the "pinned" card.
  void CheckProgress();
  // Removes the card, fading it out unless `fade` is false.
  void CloseCoach(bool fade = true);
  // Removes the card view only, fading it out with `fade`.
  void RetireCard(bool fade);
  void RemoveLeavingCoach();
  // Feeds a step event, posted, so it never runs inside a card's setup.
  void FeedLater(foundation::OnboardingEventKind kind, uint8_t step);
  void PlaceCoach();
  void OnCoachAction(CoachMark::Action action);
  // Runs the effect of a saved transition.
  void Run(const foundation::OnboardingTransition& transition);
  void PressForTest();
  void PressCoachForTest();
  void ActForTest();

  const raw_ptr<Profile> profile_;
  raw_ptr<views::View> host_;
  raw_ptr<views::View> area_;
  const WelcomeView::RadiusCallback radius_;
  const base::RepeatingClosure restore_focus_;
  const SettleRowsCallback settle_rows_;
  const SelectSpaceCallback select_space_;
  const AnchorCallback anchor_;
  const Window window_;
  raw_ptr<WelcomeView> sheet_ = nullptr;
  raw_ptr<CoachMark> coach_ = nullptr;
  // The card fading out as the next one comes in.
  raw_ptr<CoachMark> leaving_coach_ = nullptr;
  Card card_ = Card::kNone;
  // Back brought the tour to the step showing.
  bool by_back_ = false;
  // A step event is posted; no card shows until it runs.
  bool feed_pending_ = false;
  // Practice 2 looked again for a tab before skipping itself.
  bool pin_settled_ = false;
  // The tab practice 2 points at.
  uint64_t pin_item_ = 0;
  // The Ctrl+number key practice 3 teaches.
  uint8_t space_key_ = 0;
  // The Space shown, and how many there were, when practice 3 showed.
  uint64_t start_space_ = 0;
  size_t start_space_count_ = 0;
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
