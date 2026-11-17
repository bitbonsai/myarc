#include "myarc/little_arc.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "myarc/bridge.rs.h"
#include "myarc/browser_workspace_adapter.h"
#include "myarc/icons.h"
#include "myarc/little_arc_prefs.h"
#include "myarc/space_icon_catalog.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/base_window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "url/url_constants.h"

namespace myarc {

namespace {

// Arc's Little Arc is a small window. No capture of a real one exists yet;
// this size fits Arc's Settings illustration of it on a 14-inch screen.
constexpr int kWindowWidth = 760;
constexpr int kWindowHeight = 600;
// Each further Little Arc opens this far down and right of the last.
constexpr int kCascade = 22;

constexpr int kControlHeight = 28;
constexpr int kControlRadius = 8;
constexpr int kChipHeight = 20;
constexpr int kGap = 8;

struct Palette {
  SkColor frame;
  SkColor pill;
  SkColor chip;
  SkColor button;
  SkColor stroke;
  SkColor hover;
  SkColor ink;
  SkColor muted;
  SkColor accent;
};

constexpr Palette kLight = {
    .frame = SkColorSetRGB(0xe4, 0xe4, 0xeb),
    .pill = SkColorSetRGB(0xd6, 0xd6, 0xdf),
    .chip = SkColorSetRGB(0xf6, 0xf6, 0xf8),
    .button = SkColorSetRGB(0xf6, 0xf6, 0xf8),
    .stroke = SkColorSetARGB(0x24, 0x00, 0x00, 0x00),
    .hover = SkColorSetARGB(0x10, 0x00, 0x00, 0x00),
    .ink = SkColorSetRGB(0x3a, 0x3a, 0x44),
    .muted = SkColorSetRGB(0x86, 0x86, 0x92),
    .accent = SkColorSetRGB(0x3b, 0x5f, 0xc8),
};

constexpr Palette kDark = {
    .frame = SkColorSetRGB(0x2a, 0x2a, 0x30),
    .pill = SkColorSetRGB(0x3a, 0x3a, 0x42),
    .chip = SkColorSetRGB(0x4a, 0x4a, 0x53),
    .button = SkColorSetRGB(0x3a, 0x3a, 0x42),
    .stroke = SkColorSetARGB(0x30, 0xff, 0xff, 0xff),
    .hover = SkColorSetARGB(0x14, 0xff, 0xff, 0xff),
    .ink = SkColorSetRGB(0xe8, 0xe8, 0xec),
    .muted = SkColorSetRGB(0x9a, 0x9a, 0xa4),
    .accent = SkColorSetRGB(0x93, 0xab, 0xff),
};

bool IsLittleArcMode() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch("myarc-visual-parity") &&
         command_line.HasSwitch("myarc-workspace-sidebar");
}

gfx::FontList Font(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.Derive(size - base.GetFontSize(), gfx::Font::NORMAL, weight);
}

views::Label* AddLabel(views::View* parent,
                       int size,
                       gfx::Font::Weight weight) {
  auto* label = parent->AddChildView(std::make_unique<views::Label>());
  label->SetFontList(Font(size, weight));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetCanProcessEventsWithinSubtree(false);
  return label;
}

void FillRoundRect(gfx::Canvas* canvas,
                   const gfx::Rect& bounds,
                   int radius,
                   SkColor fill,
                   std::optional<SkColor> stroke = std::nullopt) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(fill);
  canvas->DrawRoundRect(gfx::RectF(bounds), radius, flags);
  if (stroke) {
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(*stroke);
    gfx::RectF edge(bounds);
    edge.Inset(0.5f);
    canvas->DrawRoundRect(edge, radius - 0.5f, flags);
  }
}

// What the header shows about the Space "Open in" moves the page to.
struct TargetSpace {
  bool found = false;
  std::u16string name;
  SpaceIconChoice icon;
};

TargetSpace FindTargetSpace(BrowserWindowInterface& little_arc) {
  TargetSpace target;
  BrowserWindowInterface* window = LittleArcMoveTarget(little_arc);
  auto* adapter =
      window ? BrowserWorkspaceAdapter::ForBrowser(*window) : nullptr;
  if (!adapter) {
    return target;
  }
  const auto sidebar = adapter->Sidebar();
  if (!sidebar.ok) {
    return target;
  }
  target.found = true;
  target.name = base::UTF8ToUTF16(std::string(sidebar.current_space.name));
  switch (sidebar.current_space.icon_kind) {
    case foundation::SpaceIconKind::Emoji:
      target.icon.kind = SpaceIconChoice::Kind::kEmoji;
      break;
    case foundation::SpaceIconKind::Named:
      target.icon.kind = SpaceIconChoice::Kind::kNamed;
      break;
    default:
      break;
  }
  target.icon.value = std::string(sidebar.current_space.icon);
  return target;
}

gfx::Rect InitialBounds(Profile& profile) {
  int open = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        open += IsLittleArc(*browser) && browser->GetProfile() == &profile;
        return true;
      });
  const gfx::Rect area =
      display::Screen::Get()->GetDisplayForNewWindows().work_area();
  const gfx::Size size(std::min(kWindowWidth, area.width()),
                       std::min(kWindowHeight, area.height()));
  const int step = kCascade * (open % 8);
  gfx::Rect bounds(area.x() + (area.width() - size.width()) / 2 + step,
                   area.y() + (area.height() - size.height()) / 4 + step,
                   size.width(), size.height());
  bounds.AdjustToFit(area);
  return bounds;
}

}  // namespace

// The address pill: the target Space's chip at its left and the page's
// security sign and host centred. A press opens the command bar on the
// address.
class LittleArcAddressPill final : public views::Button {
  METADATA_HEADER(LittleArcAddressPill, views::Button)

 public:
  explicit LittleArcAddressPill(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetAccessibleName(u"Address");
    SetTooltipText(u"Edit the address (⌘L)");
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    chip_icon_ = AddChildView(std::make_unique<views::ImageView>());
    chip_icon_->SetCanProcessEventsWithinSubtree(false);
    chip_name_ = AddLabel(this, 11, gfx::Font::Weight::MEDIUM);
    security_ = AddChildView(std::make_unique<views::ImageView>());
    security_->SetCanProcessEventsWithinSubtree(false);
    host_ = AddLabel(this, 13, gfx::Font::Weight::NORMAL);
    host_->SetElideBehavior(gfx::ELIDE_HEAD);
  }

  void Set(const Palette& palette,
           const TargetSpace& space,
           const std::u16string& host,
           bool blank,
           const gfx::VectorIcon* security_icon,
           const std::u16string& security_name) {
    palette_ = palette;
    chip_shown_ = space.found;
    chip_icon_->SetVisible(space.found);
    chip_name_->SetVisible(space.found);
    if (space.found) {
      ui::ImageModel icon = SpaceIconImage(space.icon, palette.ink, 12);
      chip_icon_->SetImage(icon.IsEmpty() ? ui::ImageModel::FromVectorIcon(
                                                kSmileIcon, palette.ink, 12)
                                          : icon);
      chip_name_->SetText(space.name);
      chip_name_->SetEnabledColor(palette.ink);
    }
    host_->SetText(blank ? u"Search or Enter URL…" : host);
    host_->SetEnabledColor(blank ? palette.muted : palette.ink);
    security_->SetVisible(security_icon != nullptr);
    if (security_icon) {
      security_->SetImage(
          ui::ImageModel::FromVectorIcon(*security_icon, palette.muted, 11));
      security_->SetTooltipText(security_name);
    }
    GetViewAccessibility().SetDescription(blank ? std::u16string() : host);
    InvalidateLayout();
    SchedulePaint();
  }

  void Layout(PassKey) override {
    const int chip_y = (height() - kChipHeight) / 2;
    chip_bounds_ = gfx::Rect();
    if (chip_shown_) {
      const int name_width = std::min(chip_name_->GetPreferredSize().width(),
                                      std::max(0, width() / 4));
      chip_bounds_ =
          gfx::Rect(4, chip_y, 6 + 12 + 4 + name_width + 7, kChipHeight);
      chip_icon_->SetBounds(chip_bounds_.x() + 6,
                            chip_y + (kChipHeight - 12) / 2, 12, 12);
      chip_name_->SetBounds(chip_icon_->bounds().right() + 4, chip_y,
                            name_width, kChipHeight);
    }
    // The host centres on the whole pill, as in Arc, unless the chip is in
    // its way.
    const int security_width = security_->GetVisible() ? 11 + 5 : 0;
    const int left = chip_bounds_.IsEmpty() ? 10 : chip_bounds_.right() + 8;
    const int room = std::max(0, width() - left - 10);
    const int host_width =
        std::min(host_->GetPreferredSize().width(), room - security_width);
    const int content = security_width + std::max(0, host_width);
    const int x =
        std::clamp((width() - content) / 2, left, left + room - content);
    if (security_->GetVisible()) {
      security_->SetBounds(x, (height() - 11) / 2, 11, 11);
    }
    host_->SetBounds(x + security_width, 0, std::max(0, host_width), height());
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    FillRoundRect(canvas, GetLocalBounds(), kControlRadius, palette_.pill);
    if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED) {
      FillRoundRect(canvas, GetLocalBounds(), kControlRadius, palette_.hover);
    }
    if (!chip_bounds_.IsEmpty()) {
      FillRoundRect(canvas, chip_bounds_, 6, palette_.chip);
    }
  }

 private:
  Palette palette_ = kLight;
  bool chip_shown_ = false;
  gfx::Rect chip_bounds_;
  raw_ptr<views::ImageView> chip_icon_ = nullptr;
  raw_ptr<views::Label> chip_name_ = nullptr;
  raw_ptr<views::ImageView> security_ = nullptr;
  raw_ptr<views::Label> host_ = nullptr;
};

BEGIN_METADATA(LittleArcAddressPill)
END_METADATA

// "Open in <Space> ⌘O": moves the page into that Space and closes the
// window. Disabled, with a tooltip saying why, while no main window can take
// it.
class LittleArcOpenInButton final : public views::Button {
  METADATA_HEADER(LittleArcOpenInButton, views::Button)

 public:
  explicit LittleArcOpenInButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    prefix_ = AddLabel(this, 12, gfx::Font::Weight::NORMAL);
    prefix_->SetText(u"Open in");
    name_ = AddLabel(this, 12, gfx::Font::Weight::MEDIUM);
    key_ = AddLabel(this, 10, gfx::Font::Weight::MEDIUM);
    key_->SetText(u"⌘O");
    key_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }

  void Set(const Palette& palette, const TargetSpace& space) {
    palette_ = palette;
    SetEnabled(space.found);
    name_->SetText(space.found ? space.name : u"Space");
    const SkColor ink = space.found ? palette.ink : palette.muted;
    prefix_->SetEnabledColor(ink);
    name_->SetEnabledColor(space.found ? palette.accent : palette.muted);
    key_->SetEnabledColor(palette.muted);
    const std::u16string action =
        space.found ? u"Open in " + space.name
                    : u"Open a myArc window to move this page into a Space";
    SetAccessibleName(space.found ? action : u"Open in Space");
    SetTooltipText(space.found ? action + u" (⌘O)" : action);
    PreferredSizeChanged();
    SchedulePaint();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(10 + prefix_->GetPreferredSize().width() + 4 +
                         std::min(name_->GetPreferredSize().width(), 160) + 6 +
                         KeyWidth() + 6,
                     kControlHeight);
  }

  void Layout(PassKey) override {
    int x = 10;
    const int prefix_width = prefix_->GetPreferredSize().width();
    prefix_->SetBounds(x, 0, prefix_width, height());
    x += prefix_width + 4;
    const int name_width =
        std::max(0, std::min(name_->GetPreferredSize().width(),
                             width() - x - 6 - KeyWidth() - 6));
    name_->SetBounds(x, 0, name_width, height());
    key_bounds_ = gfx::Rect(width() - 6 - KeyWidth(), (height() - 18) / 2,
                            KeyWidth(), 18);
    key_->SetBoundsRect(key_bounds_);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    FillRoundRect(canvas, GetLocalBounds(), kControlRadius, palette_.button,
                  palette_.stroke);
    if (GetEnabled() &&
        (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED)) {
      FillRoundRect(canvas, GetLocalBounds(), kControlRadius, palette_.hover);
    }
    FillRoundRect(canvas, key_bounds_, 4, palette_.hover);
  }

 private:
  int KeyWidth() const { return key_->GetPreferredSize().width() + 8; }

  Palette palette_ = kLight;
  gfx::Rect key_bounds_;
  raw_ptr<views::Label> prefix_ = nullptr;
  raw_ptr<views::Label> name_ = nullptr;
  raw_ptr<views::Label> key_ = nullptr;
};

BEGIN_METADATA(LittleArcOpenInButton)
END_METADATA

bool IsLittleArc(const BrowserWindowInterface& browser) {
  const BrowserInitState* state = BrowserInitState::From(&browser);
  return state && state->create_params().myarc_little_arc;
}

BrowserWindowInterface* OpenLittleArc(Profile& profile, const GURL& url) {
  if (!IsLittleArcMode()) {
    return nullptr;
  }
  // Incognito windows open a Little Arc of their regular Profile; Guest keeps
  // its own.
  Profile& owner =
      profile.IsGuestSession() ? profile : *profile.GetOriginalProfile();
  if (GetBrowserWindowCreationStatusForProfile(owner) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    return nullptr;
  }
  BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_POPUP, owner,
                                   /*from_user_gesture=*/true);
  params.myarc_little_arc = true;
  // Arc archives Little Arcs instead of restoring them; until myArc does, a
  // relaunch leaves them closed.
  params.omit_from_session_restore = true;
  params.initial_bounds = InitialBounds(owner);
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  if (!browser) {
    return nullptr;
  }
  const bool blank = !url.is_valid();
  NavigateParams navigate(
      browser, blank ? GURL(url::kAboutBlankURL) : url,
      blank ? ui::PAGE_TRANSITION_TYPED : ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  // An empty popup takes a new tab, so the page stays in this window.
  navigate.disposition = WindowOpenDisposition::kNewForegroundTab;
  navigate.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&navigate);
  return browser;
}

bool OpenLinkFromOtherApp(Profile& profile, const GURL& url) {
  if (!IsLittleArcMode() || profile.IsOffTheRecord() || !url.is_valid()) {
    return false;
  }
  std::vector<BrowserWindowInterface*> little_arcs;
  rust::Vec<rust::String> shown;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (!IsLittleArc(*browser) || browser->GetProfile() != &profile) {
          return true;
        }
        content::WebContents* contents =
            browser->GetTabStripModel()->GetActiveWebContents();
        little_arcs.push_back(browser);
        shown.push_back(rust::String(contents ? contents->GetVisibleURL().spec()
                                              : std::string()));
        return true;
      });
  const foundation::LittleArcRoute route =
      foundation::route_link_from_other_app(
          IsLittleArcLinksEnabled(g_browser_process->local_state()),
          std::string(url.scheme()), url.spec(), std::move(shown));
  switch (route.kind) {
    case foundation::LittleArcRouteKind::FocusLittleArc:
      if (route.index < little_arcs.size()) {
        little_arcs[route.index]->GetWindow()->Show();
        return true;
      }
      return OpenLittleArc(profile, url) != nullptr;
    case foundation::LittleArcRouteKind::NewLittleArc:
      return OpenLittleArc(profile, url) != nullptr;
    default:
      return false;
  }
}

BrowserWindowInterface* LittleArcMoveTarget(
    BrowserWindowInterface& little_arc) {
  std::vector<BrowserWindowInterface*> windows;
  std::vector<foundation::LittleArcMainWindow> candidates;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
          return true;
        }
        const auto* adapter = BrowserWorkspaceAdapter::ForBrowser(*browser);
        const ui::BaseWindow* window = browser->GetWindow();
        windows.push_back(browser);
        // An ordered-out window of a Profile window group can't show the
        // page; its group's shown window can.
        candidates.push_back(
            {.same_profile = browser->GetProfile() == little_arc.GetProfile(),
             .workspace_ready =
                 adapter && adapter->ready() && window &&
                 (window->IsVisible() || window->IsMinimized())});
        return true;
      });
  const int64_t index = foundation::little_arc_move_target(
      rust::Slice<const foundation::LittleArcMainWindow>(candidates.data(),
                                                         candidates.size()));
  return index >= 0 && static_cast<size_t>(index) < windows.size()
             ? windows[index]
             : nullptr;
}

bool MoveLittleArcToSpace(BrowserWindowInterface& little_arc) {
  if (!IsLittleArc(little_arc)) {
    return false;
  }
  BrowserWindowInterface* target = LittleArcMoveTarget(little_arc);
  auto* adapter =
      target ? BrowserWorkspaceAdapter::ForBrowser(*target) : nullptr;
  TabStripModel* tabs = little_arc.GetTabStripModel();
  const int index = tabs->active_index();
  if (!adapter || index == TabStripModel::kNoTab) {
    return false;
  }
  std::unique_ptr<content::WebContents> page =
      tabs->DetachWebContentsAtForInsertion(index);
  if (!page) {
    return false;
  }
  page->SetDelegate(nullptr);
  const foundation::MutationResult result = adapter->AdoptAsTodayTab(page);
  if (!result.ok && page) {
    // The workspace refused a new row. Keep the page as a plain tab of that
    // window rather than lose it; the adapter files it as it does any new
    // tab.
    target->GetTabStripModel()->AppendWebContents(std::move(page),
                                                  /*foreground=*/true);
  }
  target->GetWindow()->Show();
  // The Little Arc has no tabs left and closes itself.
  return true;
}

LittleArcHeader::LittleArcHeader(BrowserWindowInterface& browser,
                                 base::RepeatingClosure edit_address)
    : browser_(browser) {
  address_ = AddChildView(
      std::make_unique<LittleArcAddressPill>(std::move(edit_address)));
  open_in_ = AddChildView(std::make_unique<LittleArcOpenInButton>(
      base::BindRepeating(&LittleArcHeader::Move, base::Unretained(this))));
  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(u"Little Arc");
}

LittleArcHeader::~LittleArcHeader() = default;

bool LittleArcHeader::IsDark() const {
  const ui::ColorProvider* colors = GetColorProvider();
  return colors && color_utils::IsDark(colors->GetColor(ui::kColorSysSurface));
}

SkColor LittleArcHeader::FrameColor() const {
  return (IsDark() ? kDark : kLight).frame;
}

void LittleArcHeader::Update() {
  const Palette& palette = IsDark() ? kDark : kLight;
  const TargetSpace space = FindTargetSpace(*browser_);
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  const GURL url = contents ? contents->GetVisibleURL() : GURL();
  const bool blank = url.is_empty() || url.IsAboutBlank();
  std::u16string host;
  if (!blank) {
    host = url.has_host() ? base::UTF8ToUTF16(url_formatter::StripWWW(
                                std::string(url.host())))
                          : base::UTF8ToUTF16(url.spec().substr(0, 256));
  }
  const gfx::VectorIcon* security_icon = nullptr;
  std::u16string security_name;
  if (!blank && contents) {
    const bool secure = chrome_security_state::GetSecurityLevel(contents) ==
                        security_state::SECURE;
    const bool web = url.SchemeIsHTTPOrHTTPS();
    security_icon =
        secure ? &kPeekLockIcon : (web ? &kPeekNotSecureIcon : &kInfoIcon);
    security_name = secure ? u"Connection is secure"
                           : (web ? u"Not secure" : u"Browser page");
  }
  address_->Set(palette, space, host, blank, security_icon, security_name);
  open_in_->Set(palette, space);
  InvalidateLayout();
}

bool LittleArcHeader::IsDraggableAt(const gfx::Point& point) const {
  for (const views::View* child : {static_cast<const views::View*>(address_),
                                   static_cast<const views::View*>(open_in_)}) {
    if (child->GetVisible() && child->bounds().Contains(point)) {
      return false;
    }
  }
  return true;
}

void LittleArcHeader::Layout(PassKey) {
  const int y = (height() - kControlHeight) / 2;
  const int button_width =
      std::min(open_in_->GetPreferredSize().width(), width() / 3);
  open_in_->SetBounds(width() - kCardInset - button_width, y, button_width,
                      kControlHeight);
  const int left = kLightsWidth;
  address_->SetBounds(left, y, std::max(0, open_in_->x() - kGap - left),
                      kControlHeight);
}

void LittleArcHeader::OnThemeChanged() {
  views::View::OnThemeChanged();
  Update();
}

void LittleArcHeader::Move() {
  MoveLittleArcToSpace(*browser_);
}

BEGIN_METADATA(LittleArcHeader)
END_METADATA

}  // namespace myarc
