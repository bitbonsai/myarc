#include "myarc/onboarding_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "myarc/arc_import.h"
#include "myarc/onboarding_prefs.h"
#include "myarc/test_buildflags.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace myarc {

namespace {

using EventKind = foundation::OnboardingEventKind;

// The window showing the welcome, so a second window doesn't show it too.
OnboardingController* g_welcome_window = nullptr;

#if BUILDFLAG(MYARC_TEST_HOOKS)
constexpr char kMyarcTestOnboardingEvent[] = "myarc-test-onboarding-event";
constexpr char kMyarcTestOnboardingEventDelay[] =
    "myarc-test-onboarding-event-delay";
constexpr char kMyarcTestOnboardingShowDelay[] =
    "myarc-test-onboarding-show-delay";
constexpr char kMyarcTestReduceMotion[] = "myarc-test-reduce-motion";

// The switch's milliseconds, or `fallback`, up to 30 seconds.
base::TimeDelta SwitchDelay(const char* name, int fallback) {
  int ms = fallback;
  if (!base::StringToInt(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(name),
          &ms)) {
    ms = fallback;
  }
  return base::Milliseconds(std::clamp(ms, 0, 30000));
}
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)

PrefService* LocalState() {
  return g_browser_process ? g_browser_process->local_state() : nullptr;
}

foundation::OnboardingEvent Event(EventKind kind) {
  return foundation::OnboardingEvent{kind, 0, {}, {}};
}

}  // namespace

OnboardingController::OnboardingController(Profile& profile,
                                           views::View& host,
                                           views::View& area,
                                           WelcomeView::RadiusCallback radius,
                                           base::RepeatingClosure restore_focus)
    : profile_(&profile),
      host_(&host),
      area_(&area),
      radius_(std::move(radius)),
      restore_focus_(std::move(restore_focus)) {
  area_observation_.Observe(&area);
}

OnboardingController::~OnboardingController() {
  weak_factory_.InvalidateWeakPtrs();
  // The import dialog closes with the window, and its Cancel changes
  // nothing, so the tour goes back to the welcome.
  if (importing_) {
    if (PrefService* local_state = LocalState()) {
      FeedOnboardingEvent(*local_state, Event(EventKind::WindowClosed));
    }
  }
  CloseWelcome();
}

void OnboardingController::Refresh() {
  PrefService* local_state = LocalState();
  if (importing_ || !local_state) {
    return;
  }
  const auto tour = ReadOnboardingTour(*local_state);
  if (!tour) {
    return;
  }
  const std::string state(tour->state);
  if (state == "welcome") {
    if (!sheet_ && !g_welcome_window) {
      ShowWelcome(tour->offers_import);
    }
    return;
  }
  // Another window may have ended the tour.
  CloseWelcome();
  if (state == "complete" || state == "importing") {
    return;
  }
  // Reveal, practice and handoff aren't built yet: end the tour as skipped.
  Run(FeedOnboardingEvent(*local_state, Event(EventKind::SkipTour)));
}

void OnboardingController::OnViewBoundsChanged(views::View* observed_view) {
  if (sheet_ && host_) {
    sheet_->SetBoundsRect(host_->GetLocalBounds());
    sheet_->SetSheetBounds(views::View::ConvertRectToTarget(
        observed_view->parent(), host_, observed_view->bounds()));
  }
}

void OnboardingController::OnViewIsDeleting(views::View* observed_view) {
  CloseWelcome();
  area_observation_.Reset();
  area_ = nullptr;
}

void OnboardingController::ShowWelcome(bool offers_import) {
  if (sheet_) {
    sheet_->TakeFocus();
    return;
  }
  if (!host_ || !area_ || (g_welcome_window && g_welcome_window != this)) {
    return;
  }
  g_welcome_window = this;
#if BUILDFLAG(MYARC_TEST_HOOKS)
  // Holds the sheet back, so a frame capture can start before its intro.
  if (!test_show_delayed_ && base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 kMyarcTestOnboardingShowDelay)) {
    test_show_delayed_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnboardingController::ShowWelcome,
                       weak_factory_.GetWeakPtr(), offers_import),
        SwitchDelay(kMyarcTestOnboardingShowDelay, 0));
    return;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kMyarcTestReduceMotion)) {
    gfx::Animation::SetPrefersReducedMotionForTesting(true);
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
  sheet_ = host_->AddChildView(std::make_unique<WelcomeView>(
      offers_import, radius_,
      base::BindRepeating(
          [](base::WeakPtr<OnboardingController> controller,
             WelcomeView::Action action) {
            // Posted: the press runs inside the button, which closing the
            // sheet deletes.
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(&OnboardingController::OnAction,
                                          controller, action));
          },
          weak_factory_.GetWeakPtr())));
  OnViewBoundsChanged(area_);
  sheet_->TakeFocus();
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (!test_event_scheduled_ &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kMyarcTestOnboardingEvent)) {
    test_event_scheduled_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnboardingController::PressForTest,
                       weak_factory_.GetWeakPtr()),
        SwitchDelay(kMyarcTestOnboardingEventDelay, 1000));
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

void OnboardingController::CloseWelcome() {
  if (g_welcome_window == this) {
    g_welcome_window = nullptr;
  }
  if (!sheet_) {
    return;
  }
  const bool had_focus = sheet_->Contains(
      sheet_->GetFocusManager() ? sheet_->GetFocusManager()->GetFocusedView()
                                : nullptr);
  host_->RemoveChildViewT(std::exchange(sheet_, nullptr).get());
  if (had_focus && restore_focus_) {
    restore_focus_.Run();
  }
}

void OnboardingController::OnAction(WelcomeView::Action action) {
  PrefService* local_state = LocalState();
  if (!local_state || !sheet_ || importing_) {
    return;
  }
  switch (action) {
    case WelcomeView::Action::kStartBrowsing:
      Run(FeedOnboardingEvent(*local_state, Event(EventKind::StartBrowsing)));
      break;
    case WelcomeView::Action::kImport:
      Run(FeedOnboardingEvent(*local_state, Event(EventKind::ImportStarted)));
      break;
    case WelcomeView::Action::kSkipTour:
      Run(FeedOnboardingEvent(*local_state, Event(EventKind::SkipTour)));
      break;
  }
}

void OnboardingController::OnImportDone(const ArcImportOutcome& outcome) {
  importing_ = false;
  PrefService* local_state = LocalState();
  if (!local_state) {
    return;
  }
  auto event = Event(outcome.committed ? EventKind::ImportCommitted
                                       : EventKind::ImportCancelled);
  if (outcome.committed) {
    event.content_hash = outcome.content_hash;
    for (uint64_t id : outcome.space_ids) {
      event.space_ids.push_back(id);
    }
  }
  Run(FeedOnboardingEvent(*local_state, event));
}

void OnboardingController::Run(
    const foundation::OnboardingTransition& transition) {
  const std::string state(transition.tour.state);
  if (transition.effect == foundation::OnboardingEffect::OpenImport) {
    if (!host_ || !host_->GetWidget()) {
      return;
    }
    importing_ = true;
    ImportFromArc(*profile_, host_->GetWidget()->GetNativeWindow(),
                  base::BindOnce(&OnboardingController::OnImportDone,
                                 weak_factory_.GetWeakPtr()));
    return;
  }
  if (state == "welcome") {
    ShowWelcome(transition.tour.offers_import);
    return;
  }
  CloseWelcome();
  if (state == "complete" || state == "importing") {
    return;
  }
  // The reveal and the steps after it aren't built yet: end the tour as
  // skipped. The import receipt stays in the record.
  if (PrefService* local_state = LocalState()) {
    Run(FeedOnboardingEvent(*local_state, Event(EventKind::SkipTour)));
  }
}

void OnboardingController::PressForTest() {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  const std::string event =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kMyarcTestOnboardingEvent);
  if (event == "skip_tour") {
    OnAction(WelcomeView::Action::kSkipTour);
  } else if (event == "start_browsing") {
    OnAction(WelcomeView::Action::kStartBrowsing);
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

}  // namespace myarc
