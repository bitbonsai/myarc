#include "myarc/onboarding_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "components/prefs/pref_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "myarc/arc_import.h"
#include "myarc/browser_workspace_adapter.h"
#include "myarc/onboarding_prefs.h"
#include "myarc/sidebar_tint.h"
#include "myarc/test_buildflags.h"
#include "myarc/workspace_service.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace myarc {

namespace {

// The quick start page the tour's end opens on a fresh workspace.
constexpr char kQuickStartUrl[] = "https://myarc.dev/welcome/";

using EventKind = foundation::OnboardingEventKind;

// The window showing the welcome, so a second window doesn't show it too.
OnboardingController* g_welcome_window = nullptr;
// The window showing a coach mark, likewise.
OnboardingController* g_coach_window = nullptr;

// The sidebar's rows start to settle this long into the sheet's exit, once
// the dim has mostly lifted.
constexpr base::TimeDelta kRowsDelay = base::Milliseconds(200);
// The first coach mark shows this long after the reveal's last row lands.
constexpr base::TimeDelta kCoachDelay = base::Milliseconds(200);
// The window's tabs can trail the tour, for example a page still loading,
// so practice 2 waits up to this long for a page before it skips itself;
// a workspace refresh that shows one ends the wait.
constexpr base::TimeDelta kPinSettle = base::Milliseconds(3000);

// The reveal's end state, which Back from practice 1 shows.
constexpr char16_t kRevealTitle[] = u"Your sidebar";
constexpr char16_t kRevealBody[] =
    u"Your Spaces, pinned tabs and today's tabs live here.";
// Practice 1: open a site from the command bar.
constexpr char16_t kCommandBarTitle[] = u"Open a site";
constexpr char16_t kCommandBarBody[] =
    u"Type an address or a search in the command bar.";
constexpr char16_t kCommandBarHint[] = u"Press \u2318T";
// Practice 2: pin the tab.
constexpr char16_t kPinTitle[] = u"Pin this tab";
constexpr char16_t kPinBody[] =
    u"Pinned tabs stay in your sidebar, even after you close them.";
constexpr char16_t kPinHint[] = u"Press \u2318D";
// Back into practice 2 once the tab is pinned.
constexpr char16_t kPinnedTitle[] = u"Pinned";
constexpr char16_t kPinnedBody[] = u"This tab stays in your sidebar now.";
// Practice 3: switch Space, or make one when there's only one.
constexpr char16_t kSwitchTitle[] = u"Switch Spaces";
constexpr char16_t kSwitchBody[] =
    u"Each Space keeps its own tabs. Hop over to another one.";
constexpr char16_t kCreateTitle[] = u"Make a Space";
constexpr char16_t kCreateBody[] =
    u"Give work, home or a project its own set of tabs.";
constexpr char16_t kCreateButton[] = u"Create a Space";
// The handoff: the tour is over.
constexpr char16_t kHandoffTitle[] = u"You\u2019re ready";
constexpr char16_t kHandoffBody[] =
    u"That\u2019s the tour. You can replay it from Settings, under Advanced.";
constexpr char16_t kHandoffDefault[] = u"Make myArc your default browser";
constexpr char16_t kHandoffDone[] = u"Done";
// The handoff card fades out slower than a card between steps.
constexpr base::TimeDelta kHandoffFade = base::Milliseconds(180);
#if BUILDFLAG(MYARC_TEST_HOOKS)
// The panel's Create follows Create a Space this long later in test hooks.
constexpr base::TimeDelta kTestCreateDelay = base::Milliseconds(1500);
constexpr char kMyarcTestOnboarding[] = "myarc-test-onboarding";
constexpr char kMyarcTestOnboardingEvent[] = "myarc-test-onboarding-event";
constexpr char kMyarcTestOnboardingEventDelay[] =
    "myarc-test-onboarding-event-delay";
constexpr char kMyarcTestOnboardingShowDelay[] =
    "myarc-test-onboarding-show-delay";
constexpr char kMyarcTestReduceMotion[] = "myarc-test-reduce-motion";
constexpr char kMyarcTestOnboardingImport[] = "myarc-test-onboarding-import";
constexpr char kMyarcTestOnboardingHold[] = "myarc-test-onboarding-hold";
constexpr char kMyarcTestMuteSound[] = "myarc-test-mute-sound";
constexpr char kMyarcTestReplayIntro[] = "myarc-test-replay-intro";

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

// A Today tab practice 2 can pin: one with a web page.
bool IsWebPage(const foundation::ItemRecord& item) {
  const std::string url(item.canonical_url);
  return item.has_url &&
         (url.starts_with("https://") || url.starts_with("http://"));
}

// How `item` looks to practice 2, or nothing when the window doesn't show
// it.
std::optional<foundation::OnboardingPinTab> PinTabOf(
    const foundation::SidebarResult& model,
    uint64_t item,
    bool shown_page_is_web) {
  const auto has = [item](const auto& rows) {
    return item && std::ranges::any_of(rows, [item](const auto& row) {
             return row.id == item;
           });
  };
  if (has(model.favorites) || has(model.pins)) {
    return foundation::OnboardingPinTab::Saved;
  }
  for (const auto& row : model.temporary) {
    if (item && row.id == item) {
      return IsWebPage(row) ||
                     (item == model.selected_item_id && shown_page_is_web)
                 ? foundation::OnboardingPinTab::Today
                 : foundation::OnboardingPinTab::Missing;
    }
  }
  return std::nullopt;
}

// `space`'s theme in `profile`'s workspace; none for an unthemed Space, which
// shows the default tint.
std::optional<SpaceTheme> ThemeOf(Profile& profile, uint64_t space) {
  const auto snapshot = WorkspaceService::ForProfile(profile).ExportSnapshot();
  if (!snapshot || !space) {
    return std::nullopt;
  }
  for (const auto& record : snapshot->spaces) {
    if (record.id == space && record.theme.set) {
      return SpaceTheme{
          .color = SkColorSetA(record.theme.color, SK_AlphaOPAQUE),
          .opacity = record.theme.opacity};
    }
  }
  return std::nullopt;
}

// The welcome window's frame in `work`, a display's visible area: 16:10,
// as wide as three quarters of the area allows, at most 1440 pt, centred.
gfx::Rect WelcomeFrame(const gfx::Rect& work) {
  const int width =
      std::min({work.width() * 3 / 4, work.height() * 3 / 4 * 16 / 10, 1440});
  gfx::Rect frame = work;
  frame.ClampToCenteredSize(gfx::Size(width, width * 10 / 16));
  return frame;
}

}  // namespace

// The compositor frames the window starts during the reveal, and the longest
// gap between two of them.
class RevealFrames : public ui::CompositorObserver {
 public:
  explicit RevealFrames(ui::Compositor* compositor) {
    observation_.Observe(compositor);
  }

  int frames() const { return frames_; }
  base::TimeDelta longest_gap() const { return longest_gap_; }

  // ui::CompositorObserver:
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override {
    if (!last_.is_null()) {
      longest_gap_ = std::max(longest_gap_, start_time - last_);
    }
    last_ = start_time;
    ++frames_;
  }
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    observation_.Reset();
  }

 private:
  int frames_ = 0;
  base::TimeTicks last_;
  base::TimeDelta longest_gap_;
  base::ScopedObservation<ui::Compositor, ui::CompositorObserver> observation_{
      this};
};

OnboardingController::OnboardingController(Profile& profile,
                                           views::View& host,
                                           views::View& area,
                                           base::RepeatingClosure restore_focus,
                                           SettleRowsCallback settle_rows,
                                           SelectSpaceCallback select_space,
                                           AnchorCallback anchor,
                                           Window window)
    : profile_(&profile),
      host_(&host),
      area_(&area),
      restore_focus_(std::move(restore_focus)),
      settle_rows_(std::move(settle_rows)),
      select_space_(std::move(select_space)),
      anchor_(std::move(anchor)),
      window_(std::move(window)) {
  area_observation_.Observe(&area);
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kMyarcTestReplayIntro)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnboardingController::PlayIntro,
                       weak_factory_.GetWeakPtr()),
        SwitchDelay(kMyarcTestReplayIntro, 3000));
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
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
  CloseCoach(/*fade=*/false);
  RemoveLeavingCoach();
}

void OnboardingController::Refresh() {
  PrefService* local_state = LocalState();
  // PlayIntro()'s welcome isn't the tour's, so a refresh leaves it be.
  if (showcase_ || importing_ || recolouring_ || report_open_ || revealing_ ||
      !local_state) {
    return;
  }
  // Another window shows the tour's coach mark.
  if (g_coach_window && g_coach_window != this) {
    return;
  }
  const auto tour = ReadOnboardingTour(*local_state);
  if (!tour) {
    return;
  }
  const std::string state(tour->state);
  if (state == "welcome") {
    CloseCoach();
    if (!sheet_ && !g_welcome_window) {
      ShowWelcome(tour->offers_import);
    }
    return;
  }
  if (state == "reveal") {
    // Back from practice 1 shows the reveal's end state, which stays.
    if (coach_state_ != "reveal") {
      StartReveal();
    }
    return;
  }
  // Another window may have ended the tour.
  CloseWelcome();
  if (state == "practice" || state == "handoff") {
    ShowCoach(*tour, base::TimeDelta());
    return;
  }
  CloseCoach();
}

void OnboardingController::OnCommandBarOpenedPage() {
  PrefService* local_state = LocalState();
  if (!local_state || (g_coach_window && g_coach_window != this)) {
    return;
  }
  const auto tour = ReadOnboardingTour(*local_state);
  if (!tour || std::string(tour->state) != "practice" || tour->step != 1) {
    return;
  }
  auto event = Event(EventKind::StepDone);
  event.step = 1;
  by_back_ = false;
  Run(Feed(*local_state, event));
}

void OnboardingController::Replay() {
  PrefService* local_state = LocalState();
  if (!local_state || importing_ || recolouring_ || report_open_) {
    return;
  }
  // The tour may show in another window: this one takes it over.
  if (g_coach_window && g_coach_window != this) {
    g_coach_window->CloseCoach(/*fade=*/false);
  }
  if (g_welcome_window && g_welcome_window != this) {
    g_welcome_window->CloseWelcome();
  }
  CloseWelcome();
  CloseCoach(/*fade=*/false);
  by_back_ = false;
  replaying_ = true;
  Run(Feed(*local_state, Event(EventKind::Replay)));
  replaying_ = false;
}

// static
void OnboardingController::ReplayRecord() {
  if (PrefService* local_state = LocalState()) {
    // A replay never ends the tour, so it never opens the quick start page.
    FeedOnboardingEvent(*local_state, Event(EventKind::Replay));
  }
}

void OnboardingController::OnViewBoundsChanged(views::View* observed_view) {
  // The welcome covers the whole window, sidebar and toolbar included.
  if (sheet_ && host_) {
    sheet_->SetBoundsRect(host_->GetLocalBounds());
  }
  PlaceCoach();
}

void OnboardingController::OnViewIsDeleting(views::View* observed_view) {
  CloseWelcome();
  CloseCoach(/*fade=*/false);
  RemoveLeavingCoach();
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
  // Replay keeps the window where it is and as big as it is.
  const bool replay = std::exchange(replaying_, false);
  sheet_ = host_->AddChildView(std::make_unique<WelcomeView>(
      offers_import,
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
  // The sound, the desktop dim and the blob start together at t = 0 of
  // welcome_timeline.h, the blob grows into the window and the welcome plays
  // on the same times. Reduce Motion plays it at once, as a fade, silently.
  base::TimeTicks start = base::TimeTicks::Now();
#if BUILDFLAG(IS_MAC)
  if (!gfx::Animation::PrefersReducedMotion() && host_->GetWidget()) {
    // The welcome window is 16:10, three quarters of the display's visible
    // area where that fits and at most 1440 pt wide, centred, so the blob at
    // the display's centre grows into it and an ultrawide display doesn't
    // stretch it into a band. First run uses the main display, Replay the
    // window's own.
    display::Screen* screen = display::Screen::Get();
    const gfx::Rect work = (replay ? screen->GetDisplayNearestView(
                                         host_->GetWidget()->GetNativeView())
                                   : screen->GetPrimaryDisplay())
                               .work_area();
    // PlayIntro() leaves the user's window as it is.
    if (!showcase_) {
      host_->GetWidget()->SetBounds(WelcomeFrame(work));
    }
    bool log = false;
#if BUILDFLAG(MYARC_TEST_HOOKS)
    log =
        base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding);
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
    bool sound = true;
#if BUILDFLAG(MYARC_TEST_HOOKS)
    sound =
        !base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestMuteSound);
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
    // Loaded first, so it starts right after the dim is ordered front, in
    // the same task.
    if (sound) {
      sound_ = WelcomeSound::Load(log);
    }
    intro_ = std::make_unique<WelcomeIntro>(
        host_->GetWidget()->GetNativeWindow(), sheet_->BaseColor(), log);
    start = intro_->start();
    if (sound_) {
      sound_->Start(start);
    }
  }
#endif  // BUILDFLAG(IS_MAC)
  sheet_->Play(start);
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

void OnboardingController::PlayIntro() {
  if (sheet_ || coach_ || g_welcome_window || g_coach_window || importing_ ||
      recolouring_ || report_open_ || revealing_ || !host_ || !area_) {
    return;
  }
  showcase_ = true;
  ShowWelcome(/*offers_import=*/true);
  if (!sheet_) {
    showcase_ = false;
    return;
  }
  sheet_->SetClosesOnClick();
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding)) {
    LOG(WARNING) << "Test onboarding: intro replay shown";
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

void OnboardingController::EndShowcase() {
  intro_.reset();
#if BUILDFLAG(IS_MAC)
  sound_.reset();
#endif  // BUILDFLAG(IS_MAC)
  ReleaseFocus();
  const base::TimeDelta exit = sheet_ ? sheet_->Exit() : base::TimeDelta();
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding)) {
    LOG(WARNING) << "Test onboarding: intro replay closed";
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
  // Posted: the press runs inside the welcome, which this removes.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OnboardingController::CloseWelcome,
                     weak_factory_.GetWeakPtr()),
      exit);
}

void OnboardingController::CloseWelcome() {
  showcase_ = false;
  // Both fade out rather than stopping dead.
  intro_.reset();
#if BUILDFLAG(IS_MAC)
  sound_.reset();
#endif  // BUILDFLAG(IS_MAC)
  recolouring_ = false;
  revealing_ = false;
  reveal_timer_.Stop();
  reveal_frames_.reset();
  if (g_welcome_window == this) {
    g_welcome_window = nullptr;
  }
  if (!sheet_) {
    return;
  }
  const bool had_focus = sheet_->Contains(
      sheet_->GetFocusManager() ? sheet_->GetFocusManager()->GetFocusedView()
                                : nullptr);
  // Clears the pointer before the view goes, so no raw_ptr dangles.
  WelcomeView* const sheet = std::exchange(sheet_, nullptr);
  host_->RemoveChildViewT(sheet);
  if (had_focus && restore_focus_) {
    restore_focus_.Run();
  }
}

void OnboardingController::ReleaseFocus() {
  if (sheet_ && sheet_->GetFocusManager() &&
      sheet_->Contains(sheet_->GetFocusManager()->GetFocusedView()) &&
      restore_focus_) {
    restore_focus_.Run();
  }
}

void OnboardingController::OnAction(WelcomeView::Action action) {
  PrefService* local_state = LocalState();
  if (showcase_) {
    if (sheet_) {
      EndShowcase();
    }
    return;
  }
  if (!local_state || !sheet_ || importing_ || revealing_) {
    return;
  }
  // A press ends the desktop intro, showing the window, and fades the dim
  // and the sound out together.
  intro_.reset();
#if BUILDFLAG(IS_MAC)
  sound_.reset();
#endif  // BUILDFLAG(IS_MAC)
  switch (action) {
    case WelcomeView::Action::kStartBrowsing:
      Run(Feed(*local_state, Event(EventKind::StartBrowsing)));
      break;
    case WelcomeView::Action::kImport:
      Run(Feed(*local_state, Event(EventKind::ImportStarted)));
      break;
    case WelcomeView::Action::kSkipTour:
      Run(Feed(*local_state, Event(EventKind::SkipTour)));
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
  // A commit lands here inside the import dialog's press, right after the
  // workspace is saved: save the receipt now, so a kill from here on
  // relaunches past the welcome, and change the screen once the press is
  // over. Until then a workspace refresh leaves the welcome up. The reveal
  // also waits for the report the commit shows to close.
  recolouring_ = outcome.committed;
  report_open_ = outcome.committed;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OnboardingController::Run, weak_factory_.GetWeakPtr(),
                     Feed(*local_state, event)));
}

void OnboardingController::ShowImported(uint64_t space) {
  // The import adds its Spaces next to the current one: show the first, so
  // the reveal settles the imported rows. Refresh waits while recolouring_.
  if (space && select_space_) {
    select_space_.Run(space);
  }
  if (!sheet_) {
    recolouring_ = false;
    Refresh();
    return;
  }
  const ui::ColorProvider* provider = sheet_->GetColorProvider();
  const bool dark =
      provider && color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
  recolouring_ = true;
  sheet_->CrossfadeTo(
      GetSidebarTint(dark, ThemeOf(*profile_, space), /*solid=*/true),
      base::BindOnce(
          [](base::WeakPtr<OnboardingController> controller) {
            if (!controller || !controller->recolouring_) {
              return;
            }
#if BUILDFLAG(MYARC_TEST_HOOKS)
            // Holds the tour at the reveal, so a capture can kill it there.
            if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                    kMyarcTestOnboardingHold)) {
              return;
            }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
            controller->recolouring_ = false;
            // Posted: this runs inside the sheet's animation, and Refresh
            // may close the sheet.
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(&OnboardingController::Refresh, controller));
          },
          weak_factory_.GetWeakPtr()));
}

void OnboardingController::OnReportClosed() {
  if (!report_open_) {
    return;
  }
  report_open_ = false;
  Refresh();
}

void OnboardingController::StartReveal() {
  if (revealing_) {
    return;
  }
  if (!sheet_) {
    // Nothing to reveal from, for example another window's tour.
    FinishReveal();
    return;
  }
  revealing_ = true;
  reveal_start_ = base::TimeTicks::Now();
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding) &&
      host_->GetWidget() && host_->GetWidget()->GetCompositor()) {
    reveal_frames_ =
        std::make_unique<RevealFrames>(host_->GetWidget()->GetCompositor());
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
  // The window keeps the keyboard while the sheet leaves.
  ReleaseFocus();
  const base::TimeDelta exit = sheet_->Exit();
  const base::TimeDelta rows =
      settle_rows_ ? settle_rows_.Run(kRowsDelay) : base::TimeDelta();
  reveal_timer_.Start(FROM_HERE, std::max(exit, rows),
                      base::BindOnce(&OnboardingController::FinishReveal,
                                     weak_factory_.GetWeakPtr()));
}

void OnboardingController::FinishReveal() {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (reveal_frames_) {
    LOG(WARNING) << "Test onboarding: reveal took "
                 << (base::TimeTicks::Now() - reveal_start_).InMilliseconds()
                 << " ms, frames=" << reveal_frames_->frames()
                 << " longest_gap="
                 << reveal_frames_->longest_gap().InMilliseconds() << " ms";
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
  CloseWelcome();
  if (PrefService* local_state = LocalState()) {
    Run(Feed(*local_state, Event(EventKind::RevealDone)));
  }
}

void OnboardingController::OpenImport() {
  if (!host_ || !host_->GetWidget()) {
    return;
  }
  importing_ = true;
  auto done = base::BindOnce(&OnboardingController::OnImportDone,
                             weak_factory_.GetWeakPtr());
  auto report_closed = base::BindOnce(&OnboardingController::OnReportClosed,
                                      weak_factory_.GetWeakPtr());
#if BUILDFLAG(MYARC_TEST_HOOKS)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kMyarcTestOnboardingImport)) {
    ImportArcFileForCapture(
        *profile_, host_->GetWidget()->GetNativeWindow(),
        command_line.GetSwitchValuePath(kMyarcTestOnboardingImport),
        base::DoNothing(), std::move(done), std::move(report_closed));
    return;
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
  ImportFromArc(*profile_, host_->GetWidget()->GetNativeWindow(),
                std::move(done), std::move(report_closed));
}

foundation::OnboardingTransition OnboardingController::Feed(
    PrefService& local_state,
    const foundation::OnboardingEvent& event) {
  const std::string before = local_state.GetString(kOnboardingTourPref);
  auto transition = FeedOnboardingEvent(local_state, event);
  WorkspaceService& service = WorkspaceService::ForProfile(*profile_);
  const auto quick_start = foundation::onboarding_quick_start(
      before, std::string(transition.record), service.QuickStartPending());
  if (quick_start == foundation::OnboardingQuickStart::Keep) {
    return transition;
  }
  // Cleared first, so a kill right after never opens the page twice.
  const auto cleared = service.ClearQuickStart();
  LOG_IF(ERROR, !cleared.ok)
      << "Quick start flag not cleared: " << std::string(cleared.error);
  if (quick_start == foundation::OnboardingQuickStart::Open && cleared.ok) {
    // The tour's window is the most recently used one of this Profile.
    BrowserWorkspaceAdapter* adapter = nullptr;
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* browser) {
          if (browser->GetProfile() == profile_) {
            adapter = BrowserWorkspaceAdapter::ForBrowser(*browser);
          }
          return !adapter;
        });
    const auto opened =
        adapter ? adapter->OpenInCurrentSpace(GURL(kQuickStartUrl))
                : foundation::MutationResult{false, 0, "No workspace window."};
    LOG_IF(ERROR, !opened.ok)
        << "Quick start page not opened: " << std::string(opened.error);
  }
  return transition;
}

void OnboardingController::Run(
    const foundation::OnboardingTransition& transition) {
  const std::string state(transition.tour.state);
  if (transition.effect == foundation::OnboardingEffect::OpenImport) {
    OpenImport();
    return;
  }
  if (transition.effect == foundation::OnboardingEffect::Imported) {
    ShowImported(transition.tour.theme_space);
    return;
  }
  if (state == "welcome") {
    CloseCoach();
    ShowWelcome(transition.tour.offers_import);
    return;
  }
  if (state == "reveal") {
    // Back from practice 1: the reveal as it ended, with its own card.
    if (!transition.animate && !sheet_) {
      ShowCoach(transition.tour, base::TimeDelta());
      return;
    }
    CloseCoach();
    StartReveal();
    return;
  }
  CloseWelcome();
  if (state == "practice" || state == "handoff") {
    // The first card waits for the reveal's rows to land; the others come
    // in as the last one fades out.
    ShowCoach(transition.tour, transition.animate && state == "practice" &&
                                       transition.tour.step == 1
                                   ? kCoachDelay
                                   : base::TimeDelta());
    return;
  }
  CloseCoach();
}

void OnboardingController::ShowCoach(const foundation::OnboardingTour& tour,
                                     base::TimeDelta delay) {
  if (feed_pending_) {
    return;
  }
  const std::string state(tour.state);
  if (state == coach_state_ && tour.step == coach_step_ &&
      (coach_ || coach_timer_.IsRunning())) {
    // Practice 2 waiting for a page: show the card once one is there.
    if (!coach_ && pin_settled_) {
      const Card card = PracticeCard(/*starting=*/true, /*peek=*/true);
      if (card != Card::kNone) {
        coach_timer_.Stop();
        AddCard(card);
      }
      return;
    }
    CheckProgress();
    return;
  }
  if (!host_ || !area_ || (g_coach_window && g_coach_window != this)) {
    return;
  }
  CloseCoach();
  pin_settled_ = false;
  g_coach_window = this;
  coach_state_ = state;
  coach_step_ = tour.step;
  coach_offers_back_ = tour.offers_back;
  if (delay.is_positive()) {
    coach_timer_.Start(FROM_HERE, delay,
                       base::BindOnce(&OnboardingController::OpenCoach,
                                      weak_factory_.GetWeakPtr()));
    return;
  }
  OpenCoach();
}

void OnboardingController::OpenCoach() {
  if (!host_ || coach_ || coach_state_.empty()) {
    return;
  }
  const Card card = coach_state_ == "reveal"    ? Card::kReveal
                    : coach_state_ == "handoff" ? Card::kHandoff
                    : coach_step_ == 1 ? Card::kCommandBar
                                       : PracticeCard(/*starting=*/true);
  if (card == Card::kNone) {
    return;
  }
  AddCard(card);
}

void OnboardingController::AddCard(Card card) {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kMyarcTestReduceMotion)) {
    gfx::Animation::SetPrefersReducedMotionForTesting(true);
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
  card_ = card;
  CoachMark::Content content{.offers_back = coach_offers_back_};
  switch (card) {
    case Card::kNone:
      return;
    case Card::kReveal:
      content.title = kRevealTitle;
      content.body = kRevealBody;
      content.offers_next = true;
      break;
    case Card::kCommandBar:
      content.title = kCommandBarTitle;
      content.body = kCommandBarBody;
      content.hint = kCommandBarHint;
      break;
    case Card::kPin:
      content.title = kPinTitle;
      content.body = kPinBody;
      content.hint = kPinHint;
      break;
    case Card::kPinned:
      content.title = kPinnedTitle;
      content.body = kPinnedBody;
      content.offers_next = true;
      break;
    case Card::kSwitch:
      content.title = kSwitchTitle;
      content.body = kSwitchBody;
      content.hint = u"Press \u2303" + base::NumberToString16(space_key_);
      break;
    case Card::kCreate:
      content.title = kCreateTitle;
      content.body = kCreateBody;
      content.offers_next = true;
      content.next_label = kCreateButton;
      break;
    case Card::kHandoff:
      content.title = kHandoffTitle;
      content.body = kHandoffBody;
      content.offers_skip = false;
      content.offers_next = true;
      content.next_label = kHandoffDone;
      content.extra_label = kHandoffDefault;
      // The card never takes focus, so the row keeps it.
      if (window_.focus_current_row) {
        window_.focus_current_row.Run();
      }
      break;
  }
  coach_ = host_->AddChildView(std::make_unique<CoachMark>(
      content,
      base::BindRepeating(
          [](base::WeakPtr<OnboardingController> controller,
             CoachMark::Action action) {
            // Posted: the press runs inside the card, which the next step
            // removes.
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(&OnboardingController::OnCoachAction,
                                          controller, action));
          },
          weak_factory_.GetWeakPtr())));
  PlaceCoach();
  coach_->Show();
  if (card == Card::kHandoff) {
    // Only reads the default; asking to change it waits for a press.
    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartCheckIsDefault(base::BindOnce(
            [](base::WeakPtr<OnboardingController> controller,
               shell_integration::DefaultWebClientState state) {
              if (controller) {
                controller->OnDefaultState(state ==
                                           shell_integration::IS_DEFAULT);
              }
            },
            weak_factory_.GetWeakPtr()));
  }
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding)) {
    LOG(WARNING) << "Test onboarding: coach mark state=" << coach_state_
                 << " step=" << static_cast<int>(coach_step_)
                 << " card=" << static_cast<int>(card)
                 << " back=" << coach_offers_back_ << " item=" << pin_item_
                 << " key=" << static_cast<int>(space_key_)
                 << " bounds=" << coach_->bounds().ToString();
  }
  const std::string event =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kMyarcTestOnboardingEvent);
  // do acts on every card, so one run walks several steps.
  if ((!test_coach_event_scheduled_ &&
       (event == "back" || event == "next" || event == "skip_tour")) ||
      event == "do") {
    test_coach_event_scheduled_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnboardingController::PressCoachForTest,
                       weak_factory_.GetWeakPtr()),
        SwitchDelay(kMyarcTestOnboardingEventDelay, 1000));
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

OnboardingController::Card OnboardingController::PracticeCard(bool starting,
                                                              bool peek) {
  if (!window_.model) {
    return Card::kNone;
  }
  const foundation::SidebarResult model = window_.model.Run();
  if (!model.ok) {
    return starting ? Card::kNone : card_;
  }
  if (coach_step_ == 2) {
    // Going forward, the step points at the tab showing as it starts: the
    // one practice 1 just opened, or the current tab when practice 1 was
    // skipped or the tour resumed.
    if (starting && !by_back_) {
      pin_item_ = 0;
    }
    const bool shown_web =
        window_.shown_page_is_web && window_.shown_page_is_web.Run();
    std::optional<foundation::OnboardingPinTab> tab =
        PinTabOf(model, pin_item_, shown_web);
    if (!tab || *tab == foundation::OnboardingPinTab::Missing) {
      // That tab closed: point at the current one if it's a page to pin. A
      // current tab pinned already counts only as the step starts.
      const auto current = PinTabOf(model, model.selected_item_id, shown_web);
      if (current &&
          (*current == foundation::OnboardingPinTab::Today ||
           (starting && *current == foundation::OnboardingPinTab::Saved))) {
        pin_item_ = model.selected_item_id;
        tab = current;
      }
    }
    if (!tab || *tab == foundation::OnboardingPinTab::Missing) {
      // Nor is the current tab a page: point at another Today tab's row
      // when one has a web page. The step skips only with no web tab open.
      for (const auto& row : model.temporary) {
        if (IsWebPage(row)) {
          pin_item_ = row.id;
          tab = foundation::OnboardingPinTab::Other;
          break;
        }
      }
    }
#if BUILDFLAG(MYARC_TEST_HOOKS)
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            kMyarcTestOnboarding)) {
      LOG(WARNING) << "Test onboarding: pin starting=" << starting
                   << " back=" << by_back_ << " item=" << pin_item_
                   << " selected=" << model.selected_item_id
                   << " web=" << shown_web << " tab="
                   << static_cast<int>(
                          tab.value_or(foundation::OnboardingPinTab::Missing));
    }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
    const auto shown = card_ == Card::kPinned
                           ? foundation::OnboardingPin::Pinned
                           : foundation::OnboardingPin::Pin;
    switch (foundation::onboarding_pin_practice(
        starting, shown, tab.value_or(foundation::OnboardingPinTab::Missing),
        by_back_)) {
      case foundation::OnboardingPin::Pin:
        return Card::kPin;
      case foundation::OnboardingPin::Pinned:
        return Card::kPinned;
      case foundation::OnboardingPin::Done:
        FeedLater(EventKind::StepDone, 2);
        return Card::kNone;
      case foundation::OnboardingPin::Skip:
        if (peek) {
          return Card::kNone;
        }
        if (starting && !pin_settled_) {
          pin_settled_ = true;
          coach_timer_.Start(FROM_HERE, kPinSettle,
                             base::BindOnce(&OnboardingController::OpenCoach,
                                            weak_factory_.GetWeakPtr()));
          return Card::kNone;
        }
        FeedLater(EventKind::SkipStep, 2);
        return Card::kNone;
      default:
        FeedLater(EventKind::Back, 2);
        return Card::kNone;
    }
  }
  const size_t count = model.spaces.size();
  size_t current = 0;
  for (size_t i = 0; i < count; ++i) {
    if (model.spaces[i].id == model.current_space.id) {
      current = i;
    }
  }
  if (starting) {
    start_space_ = model.current_space.id;
    start_space_count_ = count;
  } else if (foundation::onboarding_space_done(start_space_, start_space_count_,
                                               model.current_space.id, count)) {
    FeedLater(EventKind::StepDone, 3);
    return Card::kNone;
  }
  space_key_ = foundation::onboarding_space_key(count, current);
  return space_key_ ? Card::kSwitch : Card::kCreate;
}

void OnboardingController::CheckProgress() {
  if (!coach_ || coach_state_ != "practice" || coach_step_ < 2) {
    return;
  }
  const Card card = PracticeCard(/*starting=*/false);
  if (card == Card::kNone) {
    CloseCoach();
    return;
  }
  if (card != card_) {
    RetireCard(/*fade=*/true);
    AddCard(card);
    return;
  }
  // A rebuilt row may have moved what the card points at.
  PlaceCoach();
}

void OnboardingController::FeedLater(EventKind kind, uint8_t step) {
  feed_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<OnboardingController> controller, EventKind kind,
             uint8_t step) {
            if (!controller) {
              return;
            }
            controller->feed_pending_ = false;
            PrefService* local_state = LocalState();
            const auto tour =
                local_state ? ReadOnboardingTour(*local_state) : std::nullopt;
            // The tour moved on meanwhile, for example in another window.
            if (!tour || std::string(tour->state) != "practice" ||
                tour->step != step) {
              return;
            }
            auto event = Event(kind);
            event.step = step;
            controller->by_back_ = kind == EventKind::Back;
            controller->Run(controller->Feed(*local_state, event));
          },
          weak_factory_.GetWeakPtr(), kind, step));
}

void OnboardingController::CloseCoach(bool fade) {
  coach_timer_.Stop();
  coach_state_.clear();
  coach_step_ = 0;
  if (g_coach_window == this) {
    g_coach_window = nullptr;
  }
  RetireCard(fade);
}

void OnboardingController::RetireCard(bool fade) {
  const base::TimeDelta duration =
      card_ == Card::kHandoff ? kHandoffFade : base::Milliseconds(120);
  card_ = Card::kNone;
  if (!coach_ || !host_) {
    coach_ = nullptr;
    return;
  }
  if (!fade) {
    CoachMark* const coach = std::exchange(coach_, nullptr);
    host_->RemoveChildViewT(coach);
    return;
  }
  // One card leaves at a time; an older one still fading goes at once.
  RemoveLeavingCoach();
  leaving_coach_ = std::exchange(coach_, nullptr);
  leaving_coach_->FadeOut(
      base::BindOnce(
          [](base::WeakPtr<OnboardingController> controller) {
            // Posted: the fade's end runs inside the card's layer animation.
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(&OnboardingController::RemoveLeavingCoach,
                               controller));
          },
          weak_factory_.GetWeakPtr()),
      duration);
}

void OnboardingController::RemoveLeavingCoach() {
  CoachMark* const leaving = std::exchange(leaving_coach_, nullptr);
  if (leaving && host_) {
    host_->RemoveChildViewT(leaving);
  }
}

void OnboardingController::PlaceCoach() {
  if (!coach_ || !host_ || !area_ || !area_->parent()) {
    return;
  }
  const gfx::Rect bounds =
      views::View::ConvertRectToTarget(area_->parent(), host_, area_->bounds());
  Target target = Target::kAddress;
  uint64_t item = pin_item_;
  switch (card_) {
    case Card::kPin:
    case Card::kPinned:
      target = Target::kItem;
      break;
    case Card::kHandoff:
      // Beside the current row, which has focus.
      target = Target::kItem;
      item = window_.model ? window_.model.Run().selected_item_id : 0;
      break;
    case Card::kSwitch:
      target = Target::kSpaces;
      break;
    case Card::kCreate:
      target = Target::kCreate;
      break;
    default:
      break;
  }
  const gfx::Rect anchor = anchor_ ? anchor_.Run(target, item) : gfx::Rect();
  // A row mid-rebuild has no bounds yet: the card stays where it was.
  if (anchor.IsEmpty() && target != Target::kAddress &&
      !coach_->bounds().IsEmpty()) {
    return;
  }
  coach_->Place(anchor, bounds);
}

void OnboardingController::OnCoachAction(CoachMark::Action action) {
  PrefService* local_state = LocalState();
  if (!local_state || !coach_) {
    return;
  }
  if (card_ == Card::kHandoff) {
    switch (action) {
      case CoachMark::Action::kExtra:
        // Chromium's own request, as Settings > General makes it. The card
        // stays for Done.
        coach_->Rearm();
        base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
            ->StartSetAsDefault(base::BindOnce(
                [](base::WeakPtr<OnboardingController> controller,
                   shell_integration::DefaultWebClientState state) {
                  if (controller) {
                    controller->OnDefaultState(state ==
                                               shell_integration::IS_DEFAULT);
                  }
                },
                weak_factory_.GetWeakPtr()));
        return;
      case CoachMark::Action::kBack:
        break;
      case CoachMark::Action::kNext:
      case CoachMark::Action::kSkipTour:
        // Done, or Esc.
        FinishHandoff();
        return;
    }
  }
  switch (action) {
    case CoachMark::Action::kExtra:
      break;
    case CoachMark::Action::kBack:
      by_back_ = true;
      Run(Feed(*local_state, Event(EventKind::Back)));
      break;
    case CoachMark::Action::kNext:
      if (card_ == Card::kCreate) {
        // Opens the panel; its Create finishes the step. The card stays.
        coach_->Rearm();
        if (window_.create_space) {
          window_.create_space.Run();
        }
        break;
      }
      by_back_ = false;
      if (card_ == Card::kPinned) {
        auto event = Event(EventKind::StepDone);
        event.step = 2;
        Run(Feed(*local_state, event));
      } else {
        Run(Feed(*local_state, Event(EventKind::RevealDone)));
      }
      break;
    case CoachMark::Action::kSkipTour:
      Run(Feed(*local_state, Event(EventKind::SkipTour)));
      break;
  }
}

void OnboardingController::FinishHandoff() {
  PrefService* local_state = LocalState();
  if (!local_state) {
    return;
  }
  // Finish keeps the steps done and skipped; the card fades as it closes.
  Run(Feed(*local_state, Event(EventKind::Finish)));
  [[maybe_unused]] const bool focused =
      window_.focus_current_row && window_.focus_current_row.Run();
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding)) {
    LOG(WARNING) << "Test onboarding: handoff done, row focused=" << focused;
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

void OnboardingController::OnDefaultState(bool is_default) {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kMyarcTestOnboarding)) {
    LOG(WARNING) << "Test onboarding: handoff default=" << is_default;
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
  if (is_default && coach_ && card_ == Card::kHandoff) {
    coach_->HideExtra();
    PlaceCoach();
  }
}

void OnboardingController::PressCoachForTest() {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  const std::string event =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kMyarcTestOnboardingEvent);
  if (!coach_) {
    return;
  }
  if (event == "do") {
    ActForTest();
  } else if (event == "back") {
    coach_->Press(CoachMark::Action::kBack);
  } else if (event == "next") {
    coach_->Press(CoachMark::Action::kNext);
  } else if (event == "skip_tour") {
    coach_->Press(CoachMark::Action::kSkipTour);
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

void OnboardingController::ActForTest() {
#if BUILDFLAG(MYARC_TEST_HOOKS)
  if (!coach_ || !window_.act_for_test) {
    return;
  }
  LOG(WARNING) << "Test onboarding: do card=" << static_cast<int>(card_);
  switch (card_) {
    case Card::kPin:
      window_.act_for_test.Run("pin");
      break;
    case Card::kSwitch:
      window_.act_for_test.Run("space:" + base::NumberToString(space_key_));
      break;
    case Card::kCreate:
      coach_->Press(CoachMark::Action::kNext);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<OnboardingController> controller) {
                if (controller && controller->window_.act_for_test) {
                  controller->window_.act_for_test.Run("create");
                }
              },
              weak_factory_.GetWeakPtr()),
          kTestCreateDelay);
      break;
    case Card::kReveal:
    case Card::kPinned:
    case Card::kHandoff:
      coach_->Press(CoachMark::Action::kNext);
      break;
    default:
      break;
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
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
  } else if (event == "import") {
    OnAction(WelcomeView::Action::kImport);
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

}  // namespace myarc
