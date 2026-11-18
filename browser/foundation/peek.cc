#include "myarc/peek.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/page_navigator.h"
#include "myarc/browser_workspace_adapter.h"
#include "myarc/icons.h"
#include "myarc/little_arc_prefs.h"
#include "myarc/peek_hook.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace myarc {

namespace {

// Card geometry, measured on Arc 1.x screenshots of a Peek with the sidebar
// shown and hidden: the card takes 81% of the page area's width, centred,
// and its full height but for 2 DIP at the top and bottom. It has no header;
// the page fills it. Round 29 DIP buttons stack 37 DIP centre to centre,
// 11 DIP right of its top right corner and 7 DIP down from its top, each
// with a 13 DIP icon.
constexpr float kCardWidthShare = 0.81f;
constexpr int kCardInset = 2;
constexpr int kCardRadius = 10;
constexpr int kRoundButton = 29;
constexpr int kRoundButtonGap = 8;
constexpr int kRoundButtonMargin = 11;
constexpr int kRoundButtonTop = 7;
constexpr int kRoundButtonIcon = 13;
// Arc's scrim takes a #f6f3ee page to #68636f: a dark navy at 60%. The page
// under it is blurred a little, enough to set the card apart while the page
// stays recognisable. The sidebar stays as it was.
constexpr SkColor kScrim = SkColorSetARGB(0x99, 0x09, 0x03, 0x1a);
constexpr float kScrimBlurSigma = 3;
constexpr SkColor kCard = SK_ColorWHITE;
constexpr SkColor kButton = SkColorSetRGB(0x05, 0x06, 0x27);
constexpr SkColor kButtonHover = SkColorSetRGB(0x1d, 0x1e, 0x44);
constexpr SkColor kButtonInk = SkColorSetRGB(0xec, 0xed, 0xff);
// The card grows out of the clicked link and shrinks back into it, quickly,
// as in Arc. The scale it starts from keeps a hint of a card on screen.
constexpr base::TimeDelta kOpenDuration = base::Milliseconds(220);
constexpr base::TimeDelta kCloseDuration = base::Milliseconds(160);
constexpr float kStartScale = 0.12f;

std::vector<PeekController*>& Controllers() {
  static base::NoDestructor<std::vector<PeekController*>> controllers;
  return *controllers;
}

// The site key Rust compares: the registrable domain, or the host itself
// when it has none (IP addresses, localhost), as site isolation treats them.
// Empty for non-web URLs, which never Peek.
std::string SiteKey(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() || !url.has_host()) {
    return std::string();
  }
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? std::string(url.host()) : domain;
}

// A link the user clicked in a tab's page, as opposed to script, form posts,
// redirects, reloads and history moves.
bool IsLinkClick(content::NavigationHandle& handle) {
  const ui::PageTransition transition = handle.GetPageTransition();
  return handle.IsInPrimaryMainFrame() && handle.IsRendererInitiated() &&
         !handle.IsSameDocument() && handle.HasUserGesture() &&
         !handle.IsPost() &&
         ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) &&
         !(transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) &&
         !(transition & ui::PAGE_TRANSITION_FORWARD_BACK);
}

// Cancels a Peek link in its tab, so the pinned page stays put, and opens
// the link in that window's card instead.
class PeekThrottle final : public content::NavigationThrottle {
 public:
  using content::NavigationThrottle::NavigationThrottle;

  ThrottleCheckResult WillStartRequest() override {
    content::NavigationHandle& handle = *navigation_handle();
    if (!IsLinkClick(handle)) {
      return PROCEED;
    }
    content::WebContents* tab = handle.GetWebContents();
    for (PeekController* peek : Controllers()) {
      const foundation::PeekRoute route = peek->Route(*tab, handle.GetURL());
      if (route == foundation::PeekRoute::NewTab) {
        // Peek is off: the link opens in a new tab and the pinned page stays.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](base::WeakPtr<content::WebContents> tab, GURL url,
                   content::Referrer referrer) {
                  if (tab) {
                    tab->OpenURL(
                        content::OpenURLParams(
                            url, referrer,
                            WindowOpenDisposition::kNewForegroundTab,
                            ui::PAGE_TRANSITION_LINK,
                            /*is_renderer_initiated=*/false),
                        {});
                  }
                },
                tab->GetWeakPtr(), handle.GetURL(),
                content::Referrer(handle.GetReferrer())));
        return CANCEL_AND_IGNORE;
      }
      if (route != foundation::PeekRoute::Peek) {
        continue;
      }
      // Not while the throttle runs: opening creates a WebContents.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<PeekController> peek, GURL url,
                 content::Referrer referrer) {
                if (peek) {
                  peek->Open(url, referrer);
                }
              },
              peek->GetWeakPtr(), handle.GetURL(),
              content::Referrer(handle.GetReferrer())));
      return CANCEL_AND_IGNORE;
    }
    return PROCEED;
  }

  const char* GetNameForLogging() override { return "MyarcPeekThrottle"; }
};

// A dark navy circle with one white icon: Arc's X, expand and split buttons
// beside the card.
class RoundButton final : public views::Button {
  METADATA_HEADER(RoundButton, views::Button)

 public:
  RoundButton(PressedCallback callback,
              const gfx::VectorIcon& icon,
              std::u16string name)
      : views::Button(std::move(callback)), icon_(icon) {
    SetPreferredSize(gfx::Size(kRoundButton, kRoundButton));
    SetTooltipText(name);
    GetViewAccessibility().SetName(name);
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::RectF bounds(GetLocalBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetState() == STATE_HOVERED || GetState() == STATE_PRESSED
                       ? kButtonHover
                       : kButton);
    canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2, flags);
    const gfx::ImageSkia image =
        gfx::CreateVectorIcon(*icon_, kRoundButtonIcon, kButtonInk);
    canvas->DrawImageInt(image, (width() - image.width()) / 2,
                         (height() - image.height()) / 2);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
};

BEGIN_METADATA(RoundButton)
END_METADATA

}  // namespace

// The dimmed page area with the card and its buttons. The page fills the
// card, as in Arc.
class PeekView final : public views::View, public views::AnimationDelegateViews {
  METADATA_HEADER(PeekView, views::View)

 public:
  using TriggerCallback =
      base::RepeatingCallback<void(foundation::PeekTrigger)>;

  PeekView(content::BrowserContext* context, TriggerCallback trigger)
      : views::AnimationDelegateViews(this), trigger_(std::move(trigger)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().SetName(u"Peek");

    card_ = AddChildView(std::make_unique<views::View>());
    card_->SetPaintToLayer();
    card_->layer()->SetFillsBoundsOpaquely(false);
    card_->SetBackground(
        views::CreateRoundedRectBackground(kCard, kCardRadius));
    web_view_ =
        card_->AddChildView(std::make_unique<views::WebView>(context));

    close_ = AddChildView(std::make_unique<RoundButton>(
        base::BindRepeating(trigger_, foundation::PeekTrigger::CloseButton),
        kPeekCloseIcon, u"Close Peek"));
    expand_ = AddChildView(std::make_unique<RoundButton>(
        base::BindRepeating(trigger_, foundation::PeekTrigger::ExpandButton),
        kPeekExpandIcon, u"Open in New Tab"));
    split_ = AddChildView(std::make_unique<RoundButton>(
        base::BindRepeating(trigger_, foundation::PeekTrigger::SplitButton),
        kPeekSplitIcon, u"Open in Split View"));
    animation_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  }

  views::WebView* web_view() { return web_view_; }

  // Grows the card out of `from`, a point in this view, or scales it in
  // from its centre. Reduce Motion shows it at once.
  void AnimateIn(std::optional<gfx::Point> from) {
    from_ = from;
    if (gfx::Animation::PrefersReducedMotion()) {
      animation_.Reset(1);
      SetProgress(1);
      return;
    }
    animation_.SetSlideDuration(kOpenDuration);
    animation_.Reset(0);
    SetProgress(0);
    animation_.Show();
  }

  // Shrinks the card back to where it came from, then runs `done`. Reduce
  // Motion runs `done` at once.
  void AnimateOut(base::OnceClosure done) {
    closing_ = true;
    done_ = std::move(done);
    if (gfx::Animation::PrefersReducedMotion()) {
      std::move(done_).Run();  // Deletes this.
      return;
    }
    animation_.SetSlideDuration(kCloseDuration);
    SetProgress(animation_.GetCurrentValue());
    animation_.Hide();
  }

  void Layout(PassKey) override {
    card_->SetBoundsRect(CardBounds());
    web_view_->SetBoundsRect(card_->GetLocalBounds());
    web_view_->holder()->SetNativeViewCornerRadii(
        gfx::RoundedCornersF(kCardRadius));

    const gfx::Rect card = card_->bounds();
    const int buttons_x = card.right() + kRoundButtonMargin;
    close_->SetBounds(buttons_x, card.y() + kRoundButtonTop, kRoundButton,
                      kRoundButton);
    expand_->SetBounds(buttons_x, close_->bounds().bottom() + kRoundButtonGap,
                       kRoundButton, kRoundButton);
    split_->SetBounds(buttons_x, expand_->bounds().bottom() + kRoundButtonGap,
                      kRoundButton, kRoundButton);
    SetProgress(progress_);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->DrawColor(SkColorSetA(
        kScrim, static_cast<U8CPU>(SkColorGetA(kScrim) * progress_)));
    // The card's layer carries it while it moves; its shadow joins it at rest.
    if (progress_ < 1) {
      return;
    }
    cc::PaintFlags shadow;
    shadow.setAntiAlias(true);
    shadow.setColor(kCard);
    shadow.setLooper(gfx::CreateShadowDrawLooper(
        {gfx::ShadowValue(gfx::Vector2d(0, 6), 28,
                          SkColorSetA(SK_ColorBLACK, 0x45))}));
    canvas->DrawRoundRect(card_->bounds(), kCardRadius, shadow);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Buttons take their own presses; the rest of the card ignores them, and
    // a closing card ignores everything.
    if (!closing_ && !card_->bounds().Contains(event.location())) {
      trigger_.Run(foundation::PeekTrigger::OutsideClick);
    }
    return true;
  }

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    SetProgress(animation->GetCurrentValue());
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    SetProgress(animation->GetCurrentValue());
    if (closing_ && done_ && animation->GetCurrentValue() == 0) {
      // Posted: running it deletes this view and the animation calling here.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(done_));
    }
  }

  // 0 is a speck at `from_` with no scrim or blur, 1 the card at rest. The
  // page and the round buttons show only at rest: the page is a native view
  // that can't scale with the card's layer.
  void SetProgress(double progress) {
    progress_ = progress;
    const gfx::Rect card = CardBounds();
    const gfx::PointF end = gfx::RectF(card).CenterPoint();
    const gfx::PointF start =
        from_ && GetLocalBounds().Contains(*from_) ? gfx::PointF(*from_) : end;
    const float t = static_cast<float>(progress);
    const float scale = kStartScale + (1 - kStartScale) * t;
    gfx::Transform transform;
    transform.Translate(start.x() + (end.x() - start.x()) * t - card.x(),
                        start.y() + (end.y() - start.y()) * t - card.y());
    transform.Scale(scale, scale);
    transform.Translate(-card.width() / 2.f, -card.height() / 2.f);
    card_->layer()->SetTransform(transform);
    card_->layer()->SetOpacity(std::min(1.f, 2 * t));
    layer()->SetBackgroundBlur(kScrimBlurSigma * t);
    const bool rest = progress >= 1 && !closing_;
    web_view_->SetVisible(rest);
    for (views::View* button : {close_.get(), expand_.get(), split_.get()}) {
      button->SetVisible(rest);
    }
    SchedulePaint();
  }

  gfx::Rect CardBounds() const {
    const int room = width() - 2 * (kRoundButton + 2 * kRoundButtonMargin);
    const int card_width =
        std::clamp(static_cast<int>(width() * kCardWidthShare), 0,
                   std::max(0, room));
    return gfx::Rect((width() - card_width) / 2, kCardInset, card_width,
                     std::max(0, height() - 2 * kCardInset));
  }

  TriggerCallback trigger_;
  raw_ptr<views::View> card_ = nullptr;
  raw_ptr<views::WebView> web_view_ = nullptr;
  raw_ptr<views::Button> close_ = nullptr;
  raw_ptr<views::Button> expand_ = nullptr;
  raw_ptr<views::Button> split_ = nullptr;
  gfx::SlideAnimation animation_{this};
  double progress_ = 1;
  std::optional<gfx::Point> from_;
  // Set while the card shrinks away; `done_` runs once it's gone.
  bool closing_ = false;
  base::OnceClosure done_;
};

BEGIN_METADATA(PeekView)
END_METADATA

PeekController::PeekController(BrowserWindowInterface& browser,
                               views::View& host,
                               views::View& area,
                               BrowserWorkspaceAdapter& adapter)
    : browser_(&browser), host_(&host), area_(&area), adapter_(&adapter) {
  Controllers().push_back(this);
  SetPeekThrottleFactory(&PeekController::MaybeAddThrottle);
  browser.GetTabStripModel()->AddObserver(this);
  area_observation_.Observe(&area);
  workspace_subscription_ = adapter.Subscribe(base::BindRepeating(
      &PeekController::OnWorkspaceChanged, base::Unretained(this)));
}

PeekController::~PeekController() {
  if (contents_) {
    Detach(/*animate=*/false);
  }
  RemoveClosingView();
  std::erase(Controllers(), this);
}

// static
PeekController* PeekController::ForBrowser(BrowserWindowInterface& browser) {
  for (PeekController* peek : Controllers()) {
    if (peek->browser_ == &browser) {
      return peek;
    }
  }
  return nullptr;
}

// static
void PeekController::MaybeAddThrottle(
    content::NavigationThrottleRegistry& registry) {
  if (!Controllers().empty() &&
      registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    registry.AddThrottle(std::make_unique<PeekThrottle>(registry));
  }
}

bool PeekController::CanReopen() const {
  return foundation::can_reopen_peek(*reopen_);
}

foundation::PeekRoute PeekController::Route(content::WebContents& tab,
                                            const GURL& to) const {
  if (browser_->GetTabStripModel()->GetIndexOfWebContents(&tab) ==
      TabStripModel::kNoTab) {
    return foundation::PeekRoute::CurrentTab;
  }
  const std::optional<foundation::PlacementKind> placement =
      adapter_->PlacementOf(tab);
  if (!placement) {
    return foundation::PeekRoute::CurrentTab;
  }
  const GURL& from = tab.GetLastCommittedURL();
  // A Cmd-click asks for a new tab before it gets here.
  return foundation::route_peek_navigation(
      *placement, std::string(from.scheme()), SiteKey(from),
      std::string(to.scheme()), SiteKey(to), /*new_tab=*/false,
      IsPeekEnabled(g_browser_process->local_state()));
}

void PeekController::Open(const GURL& url, const content::Referrer& referrer) {
  Close();
  // A new Peek replaces the one Cmd+Shift+T could bring back.
  ForgetClosed();
  content::WebContents::CreateParams params(browser_->GetProfile());
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(params);
  content::NavigationController::LoadURLParams load(url);
  load.referrer = referrer;
  load.transition_type = ui::PAGE_TRANSITION_LINK;
  contents->GetController().LoadURLWithParams(load);
  // The card grows out of the link the pointer just clicked.
  gfx::Point cursor = display::Screen::Get()->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(host_, &cursor);
  Show(std::move(contents), cursor);
}

void PeekController::Show(std::unique_ptr<content::WebContents> contents,
                          std::optional<gfx::Point> grow_from) {
  RemoveClosingView();
  auto sidebar = adapter_->Sidebar();
  if (!sidebar.ok || !area_) {
    return;
  }
  space_ = sidebar.current_space.id;
  content::WebContents* tab = ActiveTab();
  source_item_ = tab ? BrowserWorkspaceAdapter::ItemIdForContents(*tab) : 0;
  contents_ = std::move(contents);
  contents_->SetDelegate(this);

  auto view = std::make_unique<PeekView>(
      browser_->GetProfile(),
      base::BindRepeating(&PeekController::Trigger,
                          weak_factory_.GetWeakPtr()));
  view->web_view()->SetWebContents(contents_.get());
  view_ = host_->AddChildView(std::move(view));
  view_->SetBoundsRect(area_->bounds());
  if (grow_from) {
    grow_from->Offset(-area_->x(), -area_->y());
  }
  view_->AnimateIn(grow_from);

  // The pages beneath, both panes of a split included, take no input while
  // the card is up; pressing on one closes it.
  TabStripModel& tabs = *browser_->GetTabStripModel();
  for (int i = 0; i < tabs.count(); ++i) {
    content::WebContents* page = tabs.GetWebContentsAt(i);
    if (page->GetVisibility() != content::Visibility::VISIBLE) {
      continue;
    }
    ignored_.push_back(page->IgnoreInputEvents(base::BindRepeating(
        [](base::WeakPtr<PeekController> peek,
           const blink::WebInputEvent& event) {
          if (event.GetType() == blink::WebInputEvent::Type::kMouseDown) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(&PeekController::OnOutsideMouseDown,
                                          peek));
          }
          return false;
        },
        weak_factory_.GetWeakPtr())));
  }

  // A reopened page loads its last entry again now that it shows.
  contents_->GetController().LoadIfNecessary();
  view_->web_view()->RequestFocus();
}

void PeekController::Trigger(foundation::PeekTrigger trigger) {
  if (!is_open()) {
    return;
  }
  switch (foundation::peek_action(trigger)) {
    case foundation::PeekActionKind::OpenInTab:
      Expand();
      break;
    case foundation::PeekActionKind::OpenInSplit:
      SplitBesideSource();
      break;
    default:
      Close(trigger);
      break;
  }
}

bool PeekController::HandleCommand(int command) {
  if (!is_open()) {
    return command == IDC_RESTORE_TAB && Reopen();
  }
  switch (command) {
    case IDC_CLOSE_TAB:
      Trigger(foundation::PeekTrigger::CommandW);
      return true;
    case IDC_OPEN_FILE:
      Trigger(foundation::PeekTrigger::CommandO);
      return true;
    default:
      return false;
  }
}

std::unique_ptr<content::WebContents> PeekController::Detach(bool animate) {
  ignored_.clear();
  std::unique_ptr<content::WebContents> contents = std::move(contents_);
  contents->SetDelegate(nullptr);
  if (view_) {
    view_->web_view()->SetWebContents(nullptr);
    PeekView* view = std::exchange(view_, nullptr);
    if (animate) {
      RemoveClosingView();
      closing_view_ = view;
      // Runs at once under Reduce Motion.
      view->AnimateOut(base::BindOnce(&PeekController::RemoveClosingView,
                                      weak_factory_.GetWeakPtr()));
    } else {
      host_->RemoveChildViewT(view);
    }
  }
  return contents;
}

void PeekController::RemoveClosingView() {
  if (closing_view_) {
    host_->RemoveChildViewT(std::exchange(closing_view_, nullptr));
  }
}

void PeekController::ForgetClosed() {
  foundation::forget_closed_peek(*reopen_);
  closed_.reset();
}

void PeekController::Close(std::optional<foundation::PeekTrigger> trigger) {
  if (!contents_) {
    return;
  }
  std::unique_ptr<content::WebContents> contents = Detach(/*animate=*/true);
  foundation::peek_closed(*reopen_, trigger.has_value(),
                          trigger.value_or(foundation::PeekTrigger::Escape),
                          space_, source_item_);
  // A copy keeps the page's history for Cmd+Shift+T without its renderer;
  // the page itself goes with `contents`, never touching the tab strip or
  // the workspace.
  closed_ = foundation::can_reopen_peek(*reopen_) ? contents->Clone() : nullptr;
  // The pinned page beneath gets its focus back.
  if (content::WebContents* tab = ActiveTab()) {
    tab->Focus();
  }
}

bool PeekController::Reopen() {
  content::WebContents* tab = ActiveTab();
  auto sidebar = adapter_->Sidebar();
  const uint64_t item =
      tab ? BrowserWorkspaceAdapter::ItemIdForContents(*tab) : 0;
  const bool reopen =
      sidebar.ok && closed_ &&
      foundation::take_closed_peek(*reopen_, sidebar.current_space.id, item);
  std::unique_ptr<content::WebContents> contents = std::move(closed_);
  if (!reopen) {
    ForgetClosed();
    return false;
  }
  Show(std::move(contents), std::nullopt);
  return true;
}

void PeekController::Expand() {
  if (!contents_) {
    return;
  }
  std::unique_ptr<content::WebContents> contents = Detach(/*animate=*/false);
  ForgetClosed();
  // The page moves into the tab strip with its history, as exactly one new
  // Today tab of this Space.
  adapter_->AdoptAsTodayTab(contents);
}

void PeekController::SplitBesideSource() {
  content::WebContents* source = ActiveTab();
  if (!contents_ || !source) {
    return;
  }
  // A split holds two Today tabs and the page beneath is a Pin or Favorite,
  // which stays where it is. A copy of it, with its history, takes the left
  // pane and the Peek page the right, as one new split row in Today.
  std::unique_ptr<content::WebContents> left = source->Clone();
  std::unique_ptr<content::WebContents> right = Detach(/*animate=*/false);
  ForgetClosed();
  const foundation::MutationResult peek = adapter_->AdoptAsTodayTab(right);
  if (!peek.ok) {
    LOG(ERROR) << "Peek could not open in Split View: "
               << std::string(peek.error);
    return;
  }
  // Showing the copy leaves the Peek page's row beside it.
  const foundation::MutationResult copy = adapter_->AdoptAsTodayTab(left);
  const foundation::MutationResult split =
      copy.ok ? adapter_->OpenInSplitView(peek.id) : copy;
  if (!split.ok) {
    LOG(ERROR) << "Peek could not open in Split View: "
               << std::string(split.error);
  }
}

content::WebContents* PeekController::ActiveTab() const {
  return browser_->GetTabStripModel()->GetActiveWebContents();
}

void PeekController::OnOutsideMouseDown() {
  Trigger(foundation::PeekTrigger::OutsideClick);
}

void PeekController::OnWorkspaceChanged() {
  if (!is_open()) {
    return;
  }
  auto sidebar = adapter_->Sidebar();
  if (!sidebar.ok || sidebar.current_space.id != space_) {
    Close();
  }
}

content::WebContents* PeekController::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (source != contents_.get()) {
    return nullptr;
  }
  if (params.disposition == WindowOpenDisposition::kCurrentTab) {
    content::NavigationController::LoadURLParams load(params);
    auto handle = contents_->GetController().LoadURLWithParams(load);
    if (handle && navigation_handle_callback) {
      std::move(navigation_handle_callback).Run(*handle);
    }
    return contents_.get();
  }
  // New tabs and windows open as they would from the tab beneath.
  content::WebContents* tab = ActiveTab();
  if (!tab || !tab->GetDelegate()) {
    return nullptr;
  }
  return tab->GetDelegate()->OpenURLFromTab(
      tab, params, std::move(navigation_handle_callback));
}

content::WebContents* PeekController::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  content::WebContents* tab = ActiveTab();
  if (!tab || !tab->GetDelegate()) {
    return nullptr;
  }
  return tab->GetDelegate()->AddNewContents(
      tab, std::move(new_contents), target_url, disposition, window_features,
      user_gesture, was_blocked);
}

void PeekController::CloseContents(content::WebContents* source) {
  if (source == contents_.get()) {
    // window.close() from the card's page; posted, as the page is mid-call.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&PeekController::Close, weak_factory_.GetWeakPtr(),
                       std::optional<foundation::PeekTrigger>()));
  }
}

content::KeyboardEventProcessingResult PeekController::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Browser shortcuts work as over the tab beneath; Cmd+W and Cmd+O reach
  // HandleCommand through them.
  content::WebContents* tab = ActiveTab();
  return tab && tab->GetDelegate()
             ? tab->GetDelegate()->PreHandleKeyboardEvent(tab, event)
             : content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool PeekController::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Esc the page didn't use closes Peek.
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown &&
      event.windows_key_code == ui::VKEY_ESCAPE &&
      !(event.GetModifiers() & blink::WebInputEvent::kKeyModifiers)) {
    Trigger(foundation::PeekTrigger::Escape);
    return true;
  }
  content::WebContents* tab = ActiveTab();
  return tab && tab->GetDelegate() &&
         tab->GetDelegate()->HandleKeyboardEvent(tab, event);
}

void PeekController::OnTabStripModelChanged(
    TabStripModel* model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Showing another tab, or the tab beneath closing, ends the Peek.
  if (selection.active_tab_changed()) {
    Close();
  }
  // A closed tab is newer than the closed Peek, so Cmd+Shift+T restores it.
  if (change.type() == TabStripModelChange::kRemoved) {
    ForgetClosed();
  }
}

void PeekController::OnViewBoundsChanged(views::View* observed_view) {
  if (view_) {
    view_->SetBoundsRect(observed_view->bounds());
  }
}

void PeekController::OnViewIsDeleting(views::View* observed_view) {
  if (contents_) {
    Detach(/*animate=*/false);
  }
  ForgetClosed();
  area_observation_.Reset();
  area_ = nullptr;
}

}  // namespace myarc
