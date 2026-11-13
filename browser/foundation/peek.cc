#include "myarc/peek.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "myarc/browser_workspace_adapter.h"
#include "myarc/icons.h"
#include "myarc/peek_hook.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace myarc {

namespace {

// Card geometry, measured on the screenshots in Arc's Peek help article: the
// card takes about 82% of the page area, inset a little from its top and
// bottom, with round buttons stacked beside its top right corner.
constexpr float kCardWidthShare = 0.82f;
constexpr int kCardInset = 12;
constexpr int kCardRadius = 10;
constexpr int kHeaderHeight = 36;
constexpr int kHeaderButton = 28;
constexpr int kRoundButton = 28;
constexpr int kRoundButtonGap = 8;
constexpr int kRoundButtonMargin = 10;
constexpr SkColor kScrim = SkColorSetA(SK_ColorBLACK, 0x38);
constexpr SkColor kCard = SK_ColorWHITE;
constexpr SkColor kInk = SkColorSetRGB(0x3c, 0x40, 0x43);
constexpr SkColor kFaintInk = SkColorSetRGB(0xb0, 0xb4, 0xb8);
constexpr SkColor kWarningInk = SkColorSetRGB(0xc5, 0x22, 0x1f);

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
      if (!peek->ShouldPeek(*tab, handle.GetURL())) {
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

// A white circle with one icon: Arc's X and expand buttons beside the card.
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
                       ? SkColorSetRGB(0xf1, 0xf3, 0xf4)
                       : kCard);
    canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2 - 1, flags);
    const gfx::ImageSkia image = gfx::CreateVectorIcon(*icon_, 14, kInk);
    canvas->DrawImageInt(image, (width() - image.width()) / 2,
                         (height() - image.height()) / 2);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
};

BEGIN_METADATA(RoundButton)
END_METADATA

// A flat icon button in the card's header.
class HeaderButton final : public views::Button {
  METADATA_HEADER(HeaderButton, views::Button)

 public:
  HeaderButton(PressedCallback callback,
               const gfx::VectorIcon& icon,
               std::u16string name)
      : views::Button(std::move(callback)), icon_(icon) {
    SetPreferredSize(gfx::Size(kHeaderButton, kHeaderButton));
    SetTooltipText(name);
    GetViewAccessibility().SetName(name);
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (GetEnabled() &&
        (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED)) {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(SkColorSetA(SK_ColorBLACK, 0x0f));
      canvas->DrawRoundRect(GetLocalBounds(), 6, flags);
    }
    const gfx::ImageSkia image =
        gfx::CreateVectorIcon(*icon_, 16, GetEnabled() ? kInk : kFaintInk);
    canvas->DrawImageInt(image, (width() - image.width()) / 2,
                         (height() - image.height()) / 2);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
};

BEGIN_METADATA(HeaderButton)
END_METADATA

}  // namespace

// The dimmed page area with the card and its buttons. The card's header has
// back, forward and reload, the page's security state and its host; the page
// fills the rest.
class PeekView final : public views::View {
  METADATA_HEADER(PeekView, views::View)

 public:
  using TriggerCallback =
      base::RepeatingCallback<void(foundation::PeekTrigger)>;

  PeekView(content::BrowserContext* context, TriggerCallback trigger)
      : trigger_(std::move(trigger)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().SetName(u"Peek");

    card_ = AddChildView(std::make_unique<views::View>());
    card_->SetBackground(
        views::CreateRoundedRectBackground(kCard, kCardRadius));
    back_ = card_->AddChildView(std::make_unique<HeaderButton>(
        base::BindRepeating(&PeekView::GoBack, base::Unretained(this)),
        kPeekBackIcon, u"Back"));
    forward_ = card_->AddChildView(std::make_unique<HeaderButton>(
        base::BindRepeating(&PeekView::GoForward, base::Unretained(this)),
        kPeekForwardIcon, u"Forward"));
    reload_ = card_->AddChildView(std::make_unique<HeaderButton>(
        base::BindRepeating(&PeekView::Reload, base::Unretained(this)),
        kPeekReloadIcon, u"Reload"));
    security_ = card_->AddChildView(std::make_unique<views::ImageView>());
    host_ = card_->AddChildView(std::make_unique<views::Label>());
    host_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    host_->SetEnabledColor(kInk);
    host_->SetAutoColorReadabilityEnabled(false);
    host_->SetElideBehavior(gfx::ELIDE_HEAD);
    web_view_ =
        card_->AddChildView(std::make_unique<views::WebView>(context));

    close_ = AddChildView(std::make_unique<RoundButton>(
        base::BindRepeating(trigger_, foundation::PeekTrigger::CloseButton),
        kPeekCloseIcon, u"Close Peek"));
    expand_ = AddChildView(std::make_unique<RoundButton>(
        base::BindRepeating(trigger_, foundation::PeekTrigger::ExpandButton),
        kPeekExpandIcon, u"Open in New Tab"));
  }

  views::WebView* web_view() { return web_view_; }

  void Update(content::WebContents& contents) {
    auto& controller = contents.GetController();
    back_->SetEnabled(controller.CanGoBack());
    forward_->SetEnabled(controller.CanGoForward());
    const GURL& url = contents.GetVisibleURL();
    host_->SetText(base::UTF8ToUTF16(url.has_host() ? std::string(url.host())
                                                    : url.spec()));
    const security_state::SecurityLevel level =
        chrome_security_state::GetSecurityLevel(&contents);
    const bool secure = level == security_state::SECURE;
    const bool web = url.SchemeIsHTTPOrHTTPS();
    security_->SetImage(ui::ImageModel::FromVectorIcon(
        secure ? kPeekLockIcon : (web ? kPeekNotSecureIcon : kInfoIcon),
        secure || !web ? kInk : kWarningInk, 14));
    const std::u16string state =
        secure ? u"Connection is secure"
               : (web ? u"Not secure" : u"Browser page");
    security_->SetTooltipText(state);
    security_->GetViewAccessibility().SetName(state);
  }

  void Layout(PassKey) override {
    card_->SetBoundsRect(CardBounds());
    const int y = (kHeaderHeight - kHeaderButton) / 2;
    int x = 6;
    for (views::View* button : {back_.get(), forward_.get(), reload_.get()}) {
      button->SetBounds(x, y, kHeaderButton, kHeaderButton);
      x += kHeaderButton;
    }
    x += 8;
    security_->SetBounds(x, 0, 16, kHeaderHeight);
    x += 22;
    host_->SetBounds(x, 0, std::max(0, card_->width() - x - 12),
                     kHeaderHeight);
    web_view_->SetBounds(0, kHeaderHeight, card_->width(),
                         std::max(0, card_->height() - kHeaderHeight));
    web_view_->holder()->SetNativeViewCornerRadii(
        gfx::RoundedCornersF(0, 0, kCardRadius, kCardRadius));

    const gfx::Rect card = card_->bounds();
    const int buttons_x = card.right() + kRoundButtonMargin;
    close_->SetBounds(buttons_x, card.y() + 4, kRoundButton, kRoundButton);
    expand_->SetBounds(buttons_x, close_->bounds().bottom() + kRoundButtonGap,
                       kRoundButton, kRoundButton);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->DrawColor(kScrim);
    cc::PaintFlags shadow;
    shadow.setAntiAlias(true);
    shadow.setColor(kCard);
    shadow.setLooper(gfx::CreateShadowDrawLooper(
        {gfx::ShadowValue(gfx::Vector2d(0, 6), 28,
                          SkColorSetA(SK_ColorBLACK, 0x45))}));
    canvas->DrawRoundRect(card_->bounds(), kCardRadius, shadow);
    for (views::View* button : {close_.get(), expand_.get()}) {
      shadow.setLooper(gfx::CreateShadowDrawLooper(
          {gfx::ShadowValue(gfx::Vector2d(0, 1), 6,
                            SkColorSetA(SK_ColorBLACK, 0x40))}));
      canvas->DrawRoundRect(button->bounds(), kRoundButton / 2, shadow);
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Buttons take their own presses; the rest of the card ignores them.
    if (!card_->bounds().Contains(event.location())) {
      trigger_.Run(foundation::PeekTrigger::OutsideClick);
    }
    return true;
  }

 private:
  gfx::Rect CardBounds() const {
    const int room = width() - 2 * (kRoundButton + 2 * kRoundButtonMargin);
    const int card_width =
        std::clamp(static_cast<int>(width() * kCardWidthShare), 0,
                   std::max(0, room));
    return gfx::Rect((width() - card_width) / 2, kCardInset, card_width,
                     std::max(0, height() - 2 * kCardInset));
  }

  content::WebContents* page() { return web_view_->GetWebContents(); }
  void GoBack() {
    if (page() && page()->GetController().CanGoBack()) {
      page()->GetController().GoBack();
    }
  }
  void GoForward() {
    if (page() && page()->GetController().CanGoForward()) {
      page()->GetController().GoForward();
    }
  }
  void Reload() {
    if (page()) {
      page()->GetController().Reload(content::ReloadType::NORMAL, true);
    }
  }

  TriggerCallback trigger_;
  raw_ptr<views::View> card_ = nullptr;
  raw_ptr<views::Button> back_ = nullptr;
  raw_ptr<views::Button> forward_ = nullptr;
  raw_ptr<views::Button> reload_ = nullptr;
  raw_ptr<views::ImageView> security_ = nullptr;
  raw_ptr<views::Label> host_ = nullptr;
  raw_ptr<views::WebView> web_view_ = nullptr;
  raw_ptr<views::Button> close_ = nullptr;
  raw_ptr<views::Button> expand_ = nullptr;
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
  Close();
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

bool PeekController::ShouldPeek(content::WebContents& tab,
                                const GURL& to) const {
  if (browser_->GetTabStripModel()->GetIndexOfWebContents(&tab) ==
      TabStripModel::kNoTab) {
    return false;
  }
  const std::optional<foundation::PlacementKind> placement =
      adapter_->PlacementOf(tab);
  if (!placement) {
    return false;
  }
  const GURL& from = tab.GetLastCommittedURL();
  // A Cmd-click asks for a new tab before it gets here, and the Advanced
  // setting that turns Peek off doesn't exist yet, so Peek is on.
  return foundation::route_peek_navigation(
             *placement, std::string(from.scheme()), SiteKey(from),
             std::string(to.scheme()), SiteKey(to), /*new_tab=*/false,
             /*peek_enabled=*/true) == foundation::PeekRoute::Peek;
}

void PeekController::Open(const GURL& url, const content::Referrer& referrer) {
  Close();
  auto sidebar = adapter_->Sidebar();
  if (!sidebar.ok || !area_) {
    return;
  }
  space_ = sidebar.current_space.id;
  content::WebContents::CreateParams params(browser_->GetProfile());
  contents_ = content::WebContents::Create(params);
  contents_->SetDelegate(this);

  auto view = std::make_unique<PeekView>(
      browser_->GetProfile(),
      base::BindRepeating(&PeekController::Trigger,
                          weak_factory_.GetWeakPtr()));
  view->web_view()->SetWebContents(contents_.get());
  view_ = host_->AddChildView(std::move(view));
  view_->SetBoundsRect(area_->bounds());

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

  content::NavigationController::LoadURLParams load(url);
  load.referrer = referrer;
  load.transition_type = ui::PAGE_TRANSITION_LINK;
  contents_->GetController().LoadURLWithParams(load);
  view_->Update(*contents_);
  view_->web_view()->RequestFocus();
}

void PeekController::Trigger(foundation::PeekTrigger trigger) {
  if (!is_open()) {
    return;
  }
  if (foundation::peek_action(trigger) ==
      foundation::PeekActionKind::OpenInTab) {
    Expand();
  } else {
    Close();
  }
}

bool PeekController::HandleCommand(int command) {
  if (!is_open()) {
    return false;
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

void PeekController::Close() {
  if (!contents_) {
    return;
  }
  ignored_.clear();
  std::unique_ptr<content::WebContents> contents = std::move(contents_);
  contents->SetDelegate(nullptr);
  if (view_) {
    view_->web_view()->SetWebContents(nullptr);
    host_->RemoveChildViewT(std::exchange(view_, nullptr));
  }
  // The pinned page beneath gets its focus back. The card's WebContents goes
  // with `contents`, never touching the tab strip or the workspace.
  if (content::WebContents* tab = ActiveTab()) {
    tab->Focus();
  }
}

void PeekController::Expand() {
  if (!contents_) {
    return;
  }
  ignored_.clear();
  std::unique_ptr<content::WebContents> contents = std::move(contents_);
  contents->SetDelegate(nullptr);
  if (view_) {
    view_->web_view()->SetWebContents(nullptr);
    host_->RemoveChildViewT(std::exchange(view_, nullptr));
  }
  // The page moves into the tab strip with its history, as exactly one new
  // Today tab of this Space.
  adapter_->AdoptAsTodayTab(std::move(contents));
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

void PeekController::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (view_ && source == contents_.get()) {
    view_->Update(*source);
  }
}

void PeekController::LoadingStateChanged(content::WebContents* source,
                                         bool should_show_loading_ui) {
  if (view_ && source == contents_.get()) {
    view_->Update(*source);
  }
}

void PeekController::CloseContents(content::WebContents* source) {
  if (source == contents_.get()) {
    // window.close() from the card's page; posted, as the page is mid-call.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&PeekController::Close, weak_factory_.GetWeakPtr()));
  }
}

void PeekController::VisibleSecurityStateChanged(
    content::WebContents* source) {
  if (view_ && source == contents_.get()) {
    view_->Update(*source);
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
}

void PeekController::OnViewBoundsChanged(views::View* observed_view) {
  if (view_) {
    view_->SetBoundsRect(observed_view->bounds());
  }
}

void PeekController::OnViewIsDeleting(views::View* observed_view) {
  Close();
  area_observation_.Reset();
  area_ = nullptr;
}

}  // namespace myarc
