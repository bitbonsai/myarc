#include "myarc/site_control_center_view.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/security_state/core/security_state.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "myarc/boost_editor_view.h"
#include "myarc/boosts_service.h"
#include "myarc/bridge.rs.h"
#include "myarc/command_ids.h"
#include "myarc/glass_mac.h"
#include "myarc/icons.h"
#include "myarc/local_page_marker.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;
using CrossAxis = views::BoxLayout::CrossAxisAlignment;

constexpr int kPopoverWidth = 286;
constexpr int kPopoverPadding = 14;
constexpr int kPopoverCornerRadius = 16;
constexpr int kTopButtonHeight = 36;
constexpr int kTopButtonRadius = 8;
constexpr int kCircleSize = 34;
constexpr int kFooterPillHeight = 32;
constexpr int kMaxBoostRows = 4;

// Dark frosted-glass palette matching Arc's Site Control Center.
constexpr SkColor kGlassTint = SkColorSetARGB(0x9E, 0x1D, 0x1D, 0x22);
constexpr SkColor kOpaqueBg = SkColorSetRGB(0x1E, 0x1E, 0x23);
constexpr SkColor kTextPrimary = SkColorSetRGB(0xF2, 0xF2, 0xF6);
constexpr SkColor kTextSecondary = SkColorSetA(SK_ColorWHITE, 0x7A);
constexpr SkColor kTextHover = SkColorSetA(SK_ColorWHITE, 0xCC);
constexpr SkColor kTopBtnFill = SkColorSetARGB(0x88, 0x12, 0x12, 0x16);
constexpr SkColor kTopBtnHover = SkColorSetARGB(0xB8, 0x22, 0x22, 0x29);
constexpr SkColor kTopBtnBorder = SkColorSetA(SK_ColorWHITE, 0x1F);
constexpr SkColor kTopBtnBorderHover = SkColorSetA(SK_ColorWHITE, 0x33);
constexpr SkColor kPillFill = SkColorSetA(SK_ColorWHITE, 0x0F);
constexpr SkColor kPillHover = SkColorSetA(SK_ColorWHITE, 0x1C);
constexpr SkColor kCircleActiveFill = SkColorSetRGB(0xEC, 0xED, 0xF6);
constexpr SkColor kCircleActiveHover = SK_ColorWHITE;
constexpr SkColor kCircleInactiveFill = SkColorSetA(SK_ColorWHITE, 0x28);
constexpr SkColor kCircleInactiveHover = SkColorSetA(SK_ColorWHITE, 0x38);
constexpr SkColor kCircleIconDark = SkColorSetRGB(0x1B, 0x1B, 0x20);
constexpr SkColor kDividerColor = SkColorSetA(SK_ColorWHITE, 0x18);
constexpr SkColor kMoreFill = SkColorSetA(SK_ColorWHITE, 0x14);
constexpr SkColor kMoreHover = SkColorSetA(SK_ColorWHITE, 0x26);

enum OverflowCommandId {
  kCmdClearCache = 1,
  kCmdClearCookies,
  kCmdManageBoosts,
  kCmdNewBoost,
  kCmdManageExtensions,
  kCmdAddExtension,
  kCmdAllSiteSettings,
};

gfx::FontList MakeFont(int size_delta, gfx::Font::Weight weight) {
  return gfx::FontList().Derive(size_delta, gfx::Font::NORMAL, weight);
}

// Rounded translucent icon button for the 4-up top action row and Extensions +.
class ActionTileButton final : public views::Button {
  METADATA_HEADER(ActionTileButton, views::Button)

 public:
  ActionTileButton(PressedCallback callback,
                   const gfx::VectorIcon& icon,
                   int icon_size,
                   std::u16string tooltip,
                   bool bordered)
      : views::Button(std::move(callback)),
        icon_(icon),
        icon_size_(icon_size),
        bordered_(bordered) {
    SetTooltipText(tooltip);
    GetViewAccessibility().SetName(std::move(tooltip));
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    SetPreferredSize(gfx::Size(46, kTopButtonHeight));
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return GetEnabled() ? ui::mojom::CursorType::kHand
                        : ui::mojom::CursorType::kPointer;
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::RectF bounds(GetLocalBounds());
    const bool hovered =
        GetEnabled() &&
        (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED);
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setStyle(cc::PaintFlags::kFill_Style);
    if (bordered_) {
      fill.setColor(hovered ? kTopBtnHover : kTopBtnFill);
    } else {
      fill.setColor(hovered ? kPillHover : kPillFill);
    }
    canvas->DrawRoundRect(bounds, kTopButtonRadius, fill);

    if (bordered_) {
      gfx::RectF stroke_bounds = bounds;
      stroke_bounds.Inset(0.5f);
      cc::PaintFlags stroke;
      stroke.setAntiAlias(true);
      stroke.setStyle(cc::PaintFlags::kStroke_Style);
      stroke.setStrokeWidth(1.f);
      stroke.setColor(hovered ? kTopBtnBorderHover : kTopBtnBorder);
      canvas->DrawRoundRect(stroke_bounds, kTopButtonRadius - 0.5f, stroke);
    }

    const SkColor ink =
        GetEnabled() ? kTextPrimary : SkColorSetA(kTextPrimary, 0x48);
    const gfx::ImageSkia img =
        gfx::CreateVectorIcon(*icon_, icon_size_, ink);
    const int x = (width() - img.width()) / 2;
    const int y = (height() - img.height()) / 2;
    canvas->DrawImageInt(img, x, y);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
  const int icon_size_;
  const bool bordered_;
};

BEGIN_METADATA(ActionTileButton)
END_METADATA

// Circular toggle button (white when active, muted translucent when inactive).
class CircleToggleButton final : public views::Button {
  METADATA_HEADER(CircleToggleButton, views::Button)

 public:
  CircleToggleButton(PressedCallback callback,
                     const gfx::VectorIcon& icon,
                     int icon_size,
                     bool active,
                     std::u16string accessible_name)
      : views::Button(std::move(callback)),
        icon_(icon),
        icon_size_(icon_size),
        active_(active) {
    GetViewAccessibility().SetName(std::move(accessible_name));
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    SetPreferredSize(gfx::Size(kCircleSize, kCircleSize));
  }

  void SetActive(bool active) {
    if (active_ != active) {
      active_ = active;
      SchedulePaint();
    }
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::RectF bounds(GetLocalBounds());
    const bool hovered =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setStyle(cc::PaintFlags::kFill_Style);
    if (active_) {
      fill.setColor(hovered ? kCircleActiveHover : kCircleActiveFill);
    } else {
      fill.setColor(hovered ? kCircleInactiveHover : kCircleInactiveFill);
    }
    canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2.f, fill);

    const SkColor ink =
        active_ ? kCircleIconDark : SkColorSetA(kCircleIconDark, 0xCC);
    const gfx::ImageSkia img =
        gfx::CreateVectorIcon(*icon_, icon_size_, ink);
    const int x = (width() - img.width()) / 2;
    const int y = (height() - img.height()) / 2;
    canvas->DrawImageInt(img, x, y);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
  const int icon_size_;
  bool active_;
};

BEGIN_METADATA(CircleToggleButton)
END_METADATA

// Text link button for section headers (e.g. "Manage" next to "Boosts").
class HeaderLinkButton final : public views::Button {
  METADATA_HEADER(HeaderLinkButton, views::Button)

 public:
  HeaderLinkButton(PressedCallback callback, std::u16string text)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    GetViewAccessibility().SetName(text);
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(2, 4), 0));
    label_ = AddChildView(std::make_unique<views::Label>(
        std::move(text), views::style::CONTEXT_LABEL));
    label_->SetFontList(MakeFont(0, gfx::Font::Weight::MEDIUM));
    label_->SetEnabledColor(kTextSecondary);
    label_->SetAutoColorReadabilityEnabled(false);
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    const bool hovered =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    label_->SetEnabledColor(hovered ? kTextHover : kTextSecondary);
  }

 private:
  raw_ptr<views::Label> label_ = nullptr;
};

BEGIN_METADATA(HeaderLinkButton)
END_METADATA

// Clickable text + optional chevron portion of a Boost or Settings row.
class ControlRowBodyButton final : public views::Button {
  METADATA_HEADER(ControlRowBodyButton, views::Button)

 public:
  ControlRowBodyButton(PressedCallback callback,
                       std::u16string title,
                       std::u16string subtitle,
                       bool show_chevron)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    GetViewAccessibility().SetName(title);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(2, 0), 8));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);

    auto* text_col = AddChildView(std::make_unique<views::BoxLayoutView>());
    text_col->SetOrientation(Orientation::kVertical);
    text_col->SetCrossAxisAlignment(CrossAxis::kStart);
    text_col->SetBetweenChildSpacing(1);

    auto* title_label = text_col->AddChildView(std::make_unique<views::Label>(
        std::move(title), views::style::CONTEXT_LABEL));
    title_label->SetFontList(MakeFont(1, gfx::Font::Weight::SEMIBOLD));
    title_label->SetEnabledColor(kTextPrimary);
    title_label->SetAutoColorReadabilityEnabled(false);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetElideBehavior(gfx::ELIDE_TAIL);

    subtitle_label_ = text_col->AddChildView(std::make_unique<views::Label>(
        std::move(subtitle), views::style::CONTEXT_LABEL));
    subtitle_label_->SetFontList(MakeFont(0, gfx::Font::Weight::NORMAL));
    subtitle_label_->SetEnabledColor(kTextSecondary);
    subtitle_label_->SetAutoColorReadabilityEnabled(false);
    subtitle_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle_label_->SetElideBehavior(gfx::ELIDE_TAIL);

    layout->SetFlexForView(text_col, 1);

    if (show_chevron) {
      chevron_ = AddChildView(std::make_unique<views::ImageView>());
      chevron_->SetImage(ui::ImageModel::FromVectorIcon(
          kChevronRightIcon, kTextSecondary, 16));
    }
  }

  void SetSubtitle(const std::u16string& subtitle) {
    subtitle_label_->SetText(subtitle);
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    if (chevron_) {
      const bool hovered =
          GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
      chevron_->SetImage(ui::ImageModel::FromVectorIcon(
          kChevronRightIcon, hovered ? kTextPrimary : kTextSecondary, 16));
    }
  }

 private:
  raw_ptr<views::Label> subtitle_label_ = nullptr;
  raw_ptr<views::ImageView> chevron_ = nullptr;
};

BEGIN_METADATA(ControlRowBodyButton)
END_METADATA

// Footer "Secure" / "Not Secure" / "Local" pill button.
class SecurityPillButton final : public views::Button {
  METADATA_HEADER(SecurityPillButton, views::Button)

 public:
  SecurityPillButton(PressedCallback callback,
                     const gfx::VectorIcon& icon,
                     std::u16string text)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    SetTooltipText(u"View site information");
    GetViewAccessibility().SetName(text);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(0, 11), 6));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);

    auto* icon_view = AddChildView(std::make_unique<views::ImageView>());
    icon_view->SetImage(
        ui::ImageModel::FromVectorIcon(icon, kTextPrimary, 14));

    auto* label = AddChildView(std::make_unique<views::Label>(
        std::move(text), views::style::CONTEXT_LABEL));
    label->SetFontList(MakeFont(0, gfx::Font::Weight::MEDIUM));
    label->SetEnabledColor(kTextPrimary);
    label->SetAutoColorReadabilityEnabled(false);

    SetPreferredSize(
        gfx::Size(GetPreferredSize({}).width(), kFooterPillHeight));
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::RectF bounds(GetLocalBounds());
    const bool hovered =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setStyle(cc::PaintFlags::kFill_Style);
    fill.setColor(hovered ? kPillHover : kPillFill);
    canvas->DrawRoundRect(bounds, kTopButtonRadius, fill);
  }
};

BEGIN_METADATA(SecurityPillButton)
END_METADATA

// Circular "..." overflow menu button in the footer.
class OverflowCircleButton final : public views::Button {
  METADATA_HEADER(OverflowCircleButton, views::Button)

 public:
  explicit OverflowCircleButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    SetTooltipText(u"More site actions");
    GetViewAccessibility().SetName(u"More site actions");
    SetPreferredSize(gfx::Size(kFooterPillHeight, kFooterPillHeight));
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::RectF bounds(GetLocalBounds());
    const bool hovered =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setStyle(cc::PaintFlags::kFill_Style);
    fill.setColor(hovered ? kMoreHover : kMoreFill);
    canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2.f, fill);

    const gfx::ImageSkia img =
        gfx::CreateVectorIcon(kSpaceMoreIcon, 16, kTextPrimary);
    const int x = (width() - img.width()) / 2;
    const int y = (height() - img.height()) / 2;
    canvas->DrawImageInt(img, x, y);
  }
};

BEGIN_METADATA(OverflowCircleButton)
END_METADATA

class SiteControlCenterView;

class SiteControlCenterBubbleDelegate final
    : public views::BubbleDialogDelegate {
 public:
  explicit SiteControlCenterBubbleDelegate(views::View* anchor)
      : views::BubbleDialogDelegate(anchor, views::BubbleBorder::TOP_CENTER) {}
};

struct ActiveControlCenterSlot {
  std::unique_ptr<views::BubbleDialogDelegate> delegate;
  std::unique_ptr<views::Widget> widget;
  raw_ptr<SiteControlCenterView> view = nullptr;
};

std::map<BrowserWindowInterface*, std::unique_ptr<ActiveControlCenterSlot>>&
ActivePopovers() {
  static base::NoDestructor<
      std::map<BrowserWindowInterface*,
               std::unique_ptr<ActiveControlCenterSlot>>>
      popovers;
  return *popovers;
}

void ReleasePopoverSlot(std::unique_ptr<ActiveControlCenterSlot> slot) {
  if (!slot) {
    return;
  }
  slot->view = nullptr;
  if (slot->delegate) {
    slot->delegate->SetAnchorView(nullptr);
  }
  if (slot->widget) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(slot->widget));
  }
  if (slot->delegate) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(slot->delegate));
  }
}

void OpenStandardPageInfo(base::WeakPtr<BrowserWindowInterface> browser) {
  if (!browser) {
    return;
  }
  tabs::TabInterface* tab = browser->GetActiveTabInterface();
  if (!tab) {
    return;
  }
  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return;
  }
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry || entry->IsInitialEntry()) {
    return;
  }
  BrowserElementsViews* elements = BrowserElementsViews::From(browser.get());
  views::View* anchor = elements ? elements->GetView(kLocationBarElementId)
                                 : nullptr;
  if (!anchor || !anchor->GetWidget()) {
    return;
  }
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          PageInfoBubbleSpecification::Builder(
              views::BubbleAnchor(anchor),
              anchor->GetWidget()->GetNativeWindow(), contents,
              entry->GetVirtualURL())
              .Build());
  bubble->SetHighlightedElement(kLocationIconElementId);
  bubble->GetWidget()->Show();
}

class SiteControlCenterView final : public views::View,
                                    public content::WebContentsObserver,
                                    public BoostsService::Observer,
                                    public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(SiteControlCenterView, views::View)

 public:
  SiteControlCenterView(BrowserWindowInterface& browser,
                        views::BubbleDialogDelegate* bubble_delegate,
                        content::WebContents* contents)
      : content::WebContentsObserver(contents),
        browser_(&browser),
        bubble_delegate_(bubble_delegate) {
    if (Profile* profile = browser_->GetProfile()) {
      boosts_service_ = &BoostsService::ForProfile(*profile);
      boosts_observation_.Observe(boosts_service_);
    }

    auto* root = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kVertical, gfx::Insets(0), 0));
    root->set_cross_axis_alignment(CrossAxis::kStretch);

    GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().SetName(u"Site Controls");

    BuildContent();
  }

  ~SiteControlCenterView() override {
    boosts_observation_.Reset();
    Observe(nullptr);
    menu_runner_.reset();
    menu_model_.reset();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int content_width = kPopoverWidth - kPopoverPadding * 2;
    return gfx::Size(content_width, GetHeightForWidth(content_width));
  }

  void OnWindowClosing() {
    Observe(nullptr);
    boosts_observation_.Reset();
    menu_runner_.reset();
    menu_model_.reset();
    bubble_delegate_ = nullptr;
    BrowserWindowInterface* browser = browser_;
    browser_ = nullptr;
    boosts_service_ = nullptr;
    if (browser) {
      if (auto it = ActivePopovers().find(browser);
          it != ActivePopovers().end() && it->second &&
          it->second->view == this) {
        auto slot = std::move(it->second);
        ActivePopovers().erase(it);
        ReleasePopoverSlot(std::move(slot));
      }
    }
  }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { CloseSelf(); }

  // BoostsService::Observer:
  void OnBoostsChanged() override { RebuildBoostsSection(); }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override {
    content::WebContents* contents = web_contents();
    const GURL url = contents ? contents->GetLastCommittedURL() : GURL();
    switch (command_id) {
      case kCmdNewBoost:
        return CanBoostWebContents(contents);
      case kCmdAllSiteSettings:
        return url.SchemeIsHTTPOrHTTPS();
      default:
        return true;
    }
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    if (!browser_) {
      return;
    }
    base::WeakPtr<BrowserWindowInterface> weak_browser = browser_->GetWeakPtr();
    const GURL url =
        web_contents() ? web_contents()->GetLastCommittedURL() : GURL();
    CloseSelf();

    if (!weak_browser) {
      return;
    }
    switch (command_id) {
      case kCmdClearCache:
        chrome::ExecuteCommand(weak_browser.get(),
                               IDC_MYARC_CLEAR_CACHE_RELOAD);
        break;
      case kCmdClearCookies:
        chrome::ExecuteCommand(weak_browser.get(),
                               IDC_MYARC_CLEAR_COOKIES_RELOAD);
        break;
      case kCmdManageBoosts:
        chrome::ExecuteCommand(weak_browser.get(), IDC_MYARC_VIEW_BOOSTS);
        break;
      case kCmdNewBoost:
        chrome::ExecuteCommand(weak_browser.get(), IDC_MYARC_EDIT_BOOST);
        break;
      case kCmdManageExtensions:
        chrome::ExecuteCommand(weak_browser.get(), IDC_MANAGE_EXTENSIONS);
        break;
      case kCmdAddExtension:
        chrome::ExecuteCommand(weak_browser.get(), IDC_MYARC_ADD_EXTENSION);
        break;
      case kCmdAllSiteSettings:
        if (url.is_valid()) {
          chrome::ShowSiteSettings(weak_browser.get(), url);
        }
        break;
      default:
        break;
    }
  }

 private:
  void CloseSelf() {
    if (browser_) {
      CloseSiteControlCenter(*browser_);
    } else if (GetWidget() && !GetWidget()->IsClosed()) {
      GetWidget()->Close();
    }
  }

  void RunBrowserCommandAndClose(int command_id) {
    if (!browser_) {
      return;
    }
    base::WeakPtr<BrowserWindowInterface> weak_browser = browser_->GetWeakPtr();
    CloseSelf();
    if (weak_browser) {
      chrome::ExecuteCommand(weak_browser.get(), command_id);
    }
  }

  std::unique_ptr<views::Label> MakeSectionHeader(std::u16string title) {
    auto label = std::make_unique<views::Label>(std::move(title),
                                                views::style::CONTEXT_LABEL);
    label->SetFontList(MakeFont(1, gfx::Font::Weight::SEMIBOLD));
    label->SetEnabledColor(kTextPrimary);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    return label;
  }

  void BuildContent() {
    content::WebContents* contents = web_contents();
    const GURL url = contents ? contents->GetLastCommittedURL() : GURL();
    const bool can_boost = CanBoostWebContents(contents);

    // 1. Top row: 4 equal-width rounded icon action buttons.
    auto* top_row = AddChildView(std::make_unique<views::View>());
    auto* top_layout = top_row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(Orientation::kHorizontal,
                                           gfx::Insets(0), 8));
    top_layout->set_cross_axis_alignment(CrossAxis::kCenter);

    auto* share_btn = top_row->AddChildView(std::make_unique<ActionTileButton>(
        base::BindRepeating(&SiteControlCenterView::RunBrowserCommandAndClose,
                            base::Unretained(this), IDC_COPY_URL),
        kShareIcon, 17, u"Copy URL", /*bordered=*/true));
    auto* boost_btn = top_row->AddChildView(std::make_unique<ActionTileButton>(
        base::BindRepeating(&SiteControlCenterView::OnTopBoostPressed,
                            base::Unretained(this)),
        kBrushIcon, 17, u"Boost this site", /*bordered=*/true));
    boost_btn->SetEnabled(can_boost);
    auto* camera_btn = top_row->AddChildView(std::make_unique<ActionTileButton>(
        base::BindRepeating(&SiteControlCenterView::RunBrowserCommandAndClose,
                            base::Unretained(this), IDC_SHARING_HUB_SCREENSHOT),
        kCameraIcon, 17, u"Capture Screenshot", /*bordered=*/true));
    auto* reader_btn = top_row->AddChildView(std::make_unique<ActionTileButton>(
        base::BindRepeating(&SiteControlCenterView::RunBrowserCommandAndClose,
                            base::Unretained(this), IDC_SHOW_READING_MODE),
        kReaderModeIcon, 17, u"Reading Mode", /*bordered=*/true));

    top_layout->SetFlexForView(share_btn, 1);
    top_layout->SetFlexForView(boost_btn, 1);
    top_layout->SetFlexForView(camera_btn, 1);
    top_layout->SetFlexForView(reader_btn, 1);

    // 2. Extensions section.
    auto* ext_header = AddChildView(MakeSectionHeader(u"Extensions"));
    ext_header->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(14, 0, 8, 0));

    auto* ext_row = AddChildView(std::make_unique<views::BoxLayoutView>());
    ext_row->SetOrientation(Orientation::kHorizontal);
    ext_row->SetBetweenChildSpacing(8);
    ext_row->AddChildView(std::make_unique<ActionTileButton>(
        base::BindRepeating(&SiteControlCenterView::RunBrowserCommandAndClose,
                            base::Unretained(this), IDC_MYARC_ADD_EXTENSION),
        kPlusIcon, 16, u"Add Extension", /*bordered=*/false));

    // 3. Boosts section.
    auto* boosts_header_row = AddChildView(std::make_unique<views::View>());
    boosts_header_row->SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(16, 0, 8, 0));
    auto* boosts_header_layout = boosts_header_row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(Orientation::kHorizontal,
                                           gfx::Insets(0), 6));
    boosts_header_layout->set_cross_axis_alignment(CrossAxis::kCenter);
    auto* boosts_title =
        boosts_header_row->AddChildView(MakeSectionHeader(u"Boosts"));
    boosts_header_layout->SetFlexForView(boosts_title, 1);
    boosts_header_row->AddChildView(std::make_unique<HeaderLinkButton>(
        base::BindRepeating(&SiteControlCenterView::RunBrowserCommandAndClose,
                            base::Unretained(this), IDC_MYARC_VIEW_BOOSTS),
        u"Manage"));

    boosts_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    boosts_container_->SetOrientation(Orientation::kVertical);
    boosts_container_->SetCrossAxisAlignment(CrossAxis::kStretch);
    boosts_container_->SetBetweenChildSpacing(10);
    RebuildBoostsSection();

    // 4. Settings section.
    auto* settings_header = AddChildView(MakeSectionHeader(u"Settings"));
    settings_header->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(16, 0, 8, 0));

    auto* settings_col = AddChildView(std::make_unique<views::BoxLayoutView>());
    settings_col->SetOrientation(Orientation::kVertical);
    settings_col->SetCrossAxisAlignment(CrossAxis::kStretch);
    settings_col->SetBetweenChildSpacing(10);

    // 4a. Developer Mode row.
    const bool dev_open =
        contents &&
        DevToolsWindow::GetInstanceForInspectedWebContents(contents) != nullptr;
    auto* dev_row = settings_col->AddChildView(std::make_unique<views::View>());
    auto* dev_layout = dev_row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(Orientation::kHorizontal,
                                           gfx::Insets(0), 12));
    dev_layout->set_cross_axis_alignment(CrossAxis::kCenter);

    dev_circle_ = dev_row->AddChildView(std::make_unique<CircleToggleButton>(
        base::BindRepeating(&SiteControlCenterView::OnToggleDeveloperMode,
                            base::Unretained(this)),
        kHammerIcon, 16, dev_open, u"Toggle Developer Mode"));
    dev_body_ = dev_row->AddChildView(std::make_unique<ControlRowBodyButton>(
        base::BindRepeating(&SiteControlCenterView::OnToggleDeveloperMode,
                            base::Unretained(this)),
        u"Developer Mode", dev_open ? u"On" : u"Off", /*show_chevron=*/false));
    dev_layout->SetFlexForView(dev_body_, 1);

    // 4b. Automatic Picture-In-Picture row (HTTP/HTTPS pages).
    if (url.SchemeIsHTTPOrHTTPS() && browser_ && browser_->GetProfile()) {
      HostContentSettingsMap* settings_map =
          HostContentSettingsMapFactory::GetForProfile(browser_->GetProfile());
      if (settings_map) {
        const ContentSetting pip_setting = settings_map->GetContentSetting(
            url, url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
        const bool pip_allowed = pip_setting == CONTENT_SETTING_ALLOW;
        const std::u16string pip_status =
            pip_allowed
                ? u"Allowed"
                : (pip_setting == CONTENT_SETTING_BLOCK ? u"Blocked" : u"Ask");

        auto* pip_row =
            settings_col->AddChildView(std::make_unique<views::View>());
        auto* pip_layout = pip_row->SetLayoutManager(
            std::make_unique<views::BoxLayout>(Orientation::kHorizontal,
                                               gfx::Insets(0), 12));
        pip_layout->set_cross_axis_alignment(CrossAxis::kCenter);

        pip_circle_ =
            pip_row->AddChildView(std::make_unique<CircleToggleButton>(
                base::BindRepeating(&SiteControlCenterView::OnToggleAutoPip,
                                    base::Unretained(this)),
                kSettingsGearIcon, 16, pip_allowed,
                u"Toggle Automatic Picture-In-Picture"));
        pip_body_ =
            pip_row->AddChildView(std::make_unique<ControlRowBodyButton>(
                base::BindRepeating(&SiteControlCenterView::OnToggleAutoPip,
                                    base::Unretained(this)),
                u"Automatic Picture-In-Picture", pip_status,
                /*show_chevron=*/false));
        pip_layout->SetFlexForView(pip_body_, 1);
      }
    }

    // 5. Hairline divider + Footer.
    auto* divider = AddChildView(std::make_unique<views::View>());
    divider->SetPreferredSize(gfx::Size(1, 1));
    divider->SetBackground(views::CreateSolidBackground(kDividerColor));
    divider->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(14, 0, 12, 0));

    auto* footer = AddChildView(std::make_unique<views::View>());
    auto* footer_layout = footer->SetLayoutManager(
        std::make_unique<views::BoxLayout>(Orientation::kHorizontal,
                                           gfx::Insets(0), 8));
    footer_layout->set_cross_axis_alignment(CrossAxis::kCenter);

    const gfx::VectorIcon* sec_icon = &kInfoIcon;
    std::u16string sec_label = u"Site Info";
    if (contents) {
      const security_state::SecurityLevel level =
          chrome_security_state::GetSecurityLevel(contents);
      const bool web = url.SchemeIsHTTPOrHTTPS();
      const bool secure =
          level == security_state::SECURE ||
          (level == security_state::NONE && url.SchemeIsCryptographic());
      if (secure) {
        sec_icon = &kPeekLockIcon;
        sec_label = u"Secure";
      } else if (IsLocalPage(url) || url.SchemeIsFile()) {
        sec_icon = &kInfoIcon;
        sec_label = u"Local";
      } else if (web) {
        sec_icon = &kPeekNotSecureIcon;
        sec_label = u"Not Secure";
      }
    }

    footer->AddChildView(std::make_unique<SecurityPillButton>(
        base::BindRepeating(&SiteControlCenterView::OnSecurityPillPressed,
                            base::Unretained(this)),
        *sec_icon, std::move(sec_label)));

    auto* spacer = footer->AddChildView(std::make_unique<views::View>());
    footer_layout->SetFlexForView(spacer, 1);

    more_button_ = footer->AddChildView(std::make_unique<OverflowCircleButton>(
        base::BindRepeating(&SiteControlCenterView::OnMoreButtonPressed,
                            base::Unretained(this))));
  }

  void RebuildBoostsSection() {
    if (!boosts_container_) {
      return;
    }
    boosts_container_->RemoveAllChildViews();

    content::WebContents* contents = web_contents();
    const bool can_boost = CanBoostWebContents(contents);
    if (!can_boost || !boosts_service_) {
      auto* row =
          boosts_container_->AddChildView(std::make_unique<views::View>());
      auto* layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          Orientation::kHorizontal, gfx::Insets(0), 12));
      layout->set_cross_axis_alignment(CrossAxis::kCenter);
      auto* circle = row->AddChildView(std::make_unique<CircleToggleButton>(
          base::BindRepeating(&SiteControlCenterView::RunBrowserCommandAndClose,
                              base::Unretained(this), IDC_MYARC_VIEW_BOOSTS),
          kBrushIcon, 16, /*active=*/false, u"Manage Boosts"));
      circle->SetEnabled(false);
      auto* body = row->AddChildView(std::make_unique<ControlRowBodyButton>(
          base::BindRepeating(&SiteControlCenterView::RunBrowserCommandAndClose,
                              base::Unretained(this), IDC_MYARC_VIEW_BOOSTS),
          u"Boosts unavailable", u"Open an http(s) site to Boost",
          /*show_chevron=*/false));
      layout->SetFlexForView(body, 1);
      SizeToContentsIfReady();
      return;
    }

    const url::Origin origin =
        url::Origin::Create(contents->GetLastCommittedURL());
    const bool global_enabled = boosts_service_->GlobalEnabled();
    auto matching = boosts_service_->MatchingBoosts(origin);

    if (matching.empty()) {
      auto* row =
          boosts_container_->AddChildView(std::make_unique<views::View>());
      auto* layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          Orientation::kHorizontal, gfx::Insets(0), 12));
      layout->set_cross_axis_alignment(CrossAxis::kCenter);

      row->AddChildView(std::make_unique<CircleToggleButton>(
          base::BindRepeating(&SiteControlCenterView::OnTopBoostPressed,
                              base::Unretained(this)),
          kBrushIcon, 16, /*active=*/false, u"New Boost"));
      auto* body = row->AddChildView(std::make_unique<ControlRowBodyButton>(
          base::BindRepeating(&SiteControlCenterView::OnTopBoostPressed,
                              base::Unretained(this)),
          u"New Boost", u"Customize this site", /*show_chevron=*/true));
      layout->SetFlexForView(body, 1);
      SizeToContentsIfReady();
      return;
    }

    const size_t count =
        std::min(static_cast<size_t>(kMaxBoostRows), matching.size());
    for (size_t i = 0; i < count; ++i) {
      const auto& boost = matching[i];
      const uint64_t boost_id = boost.id;
      const bool active = boost.enabled && global_enabled;
      const std::u16string name =
          boost.name.empty()
              ? u"My Boost"
              : base::UTF8ToUTF16(std::string(boost.name.data(),
                                              boost.name.size()));

      auto* row =
          boosts_container_->AddChildView(std::make_unique<views::View>());
      auto* layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          Orientation::kHorizontal, gfx::Insets(0), 12));
      layout->set_cross_axis_alignment(CrossAxis::kCenter);

      row->AddChildView(std::make_unique<CircleToggleButton>(
          base::BindRepeating(&SiteControlCenterView::OnToggleBoost,
                              base::Unretained(this), boost_id, !active),
          kBrushIcon, 16, active,
          active ? u"Disable Boost" : u"Enable Boost"));

      auto* body = row->AddChildView(std::make_unique<ControlRowBodyButton>(
          base::BindRepeating(&SiteControlCenterView::OnOpenBoostEditor,
                              base::Unretained(this), boost_id),
          name, active ? u"Enabled" : u"Disabled", /*show_chevron=*/true));
      layout->SetFlexForView(body, 1);
    }

    SizeToContentsIfReady();
  }

  void SizeToContentsIfReady() {
    InvalidateLayout();
    if (bubble_delegate_ && GetWidget() && !GetWidget()->IsClosed()) {
      bubble_delegate_->SizeToContents();
    }
  }

  void OnTopBoostPressed() {
    if (!browser_) {
      return;
    }
    base::WeakPtr<BrowserWindowInterface> weak_browser = browser_->GetWeakPtr();
    CloseSelf();
    if (weak_browser) {
      ShowBoostEditor(*weak_browser);
    }
  }

  void OnOpenBoostEditor(uint64_t boost_id) {
    if (!browser_) {
      return;
    }
    base::WeakPtr<BrowserWindowInterface> weak_browser = browser_->GetWeakPtr();
    CloseSelf();
    if (weak_browser) {
      ShowBoostEditor(*weak_browser, boost_id);
    }
  }

  void OnToggleBoost(uint64_t boost_id, bool enable) {
    if (!boosts_service_) {
      return;
    }
    if (enable && !boosts_service_->GlobalEnabled()) {
      boosts_service_->SetGlobalEnabled(true);
    }
    boosts_service_->SetBoostEnabled(boost_id, enable);
  }

  void OnToggleDeveloperMode() {
    if (!browser_) {
      return;
    }
    chrome::ExecuteCommand(browser_, IDC_DEV_TOOLS_TOGGLE);
    const bool dev_open =
        web_contents() &&
        DevToolsWindow::GetInstanceForInspectedWebContents(web_contents()) !=
            nullptr;
    if (dev_circle_) {
      dev_circle_->SetActive(dev_open);
    }
    if (dev_body_) {
      dev_body_->SetSubtitle(dev_open ? u"On" : u"Off");
    }
  }

  void OnToggleAutoPip() {
    content::WebContents* contents = web_contents();
    if (!contents || !browser_ || !browser_->GetProfile()) {
      return;
    }
    const GURL url = contents->GetLastCommittedURL();
    if (!url.SchemeIsHTTPOrHTTPS()) {
      return;
    }
    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(browser_->GetProfile());
    if (!settings_map) {
      return;
    }
    const ContentSetting current = settings_map->GetContentSetting(
        url, url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
    const ContentSetting next = current == CONTENT_SETTING_ALLOW
                                    ? CONTENT_SETTING_BLOCK
                                    : CONTENT_SETTING_ALLOW;
    settings_map->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::AUTO_PICTURE_IN_PICTURE, next);

    const bool allowed = next == CONTENT_SETTING_ALLOW;
    if (pip_circle_) {
      pip_circle_->SetActive(allowed);
    }
    if (pip_body_) {
      pip_body_->SetSubtitle(allowed ? u"Allowed" : u"Blocked");
    }
  }

  void OnSecurityPillPressed() {
    if (!browser_) {
      return;
    }
    base::WeakPtr<BrowserWindowInterface> weak_browser = browser_->GetWeakPtr();
    CloseSelf();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&OpenStandardPageInfo, weak_browser));
  }

  void OnMoreButtonPressed() {
    if (!more_button_ || !GetWidget()) {
      return;
    }
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_model_->AddItem(kCmdClearCache, u"Clear Cache");
    menu_model_->AddItem(kCmdClearCookies, u"Clear Cookies");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kCmdManageBoosts, u"Manage Boosts...");
    menu_model_->AddItem(kCmdNewBoost, u"New Boost...");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kCmdManageExtensions, u"Manage Extensions...");
    menu_model_->AddItem(kCmdAddExtension, u"Add Extension...");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kCmdAllSiteSettings, u"All Site Settings...");

    if (bubble_delegate_) {
      bubble_delegate_->set_close_on_deactivate(false);
    }
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::CONTEXT_MENU,
        base::BindRepeating(&SiteControlCenterView::OnOverflowMenuClosed,
                            weak_factory_.GetWeakPtr()));
    menu_runner_->RunMenuAt(
        GetWidget(), nullptr, more_button_->GetBoundsInScreen(),
        views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kMouse);
  }

  void OnOverflowMenuClosed() {
    if (bubble_delegate_) {
      bubble_delegate_->set_close_on_deactivate(true);
    }
  }

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;
  raw_ptr<BoostsService> boosts_service_ = nullptr;
  base::ScopedObservation<BoostsService, BoostsService::Observer>
      boosts_observation_{this};

  raw_ptr<views::BoxLayoutView> boosts_container_ = nullptr;
  raw_ptr<CircleToggleButton> dev_circle_ = nullptr;
  raw_ptr<ControlRowBodyButton> dev_body_ = nullptr;
  raw_ptr<CircleToggleButton> pip_circle_ = nullptr;
  raw_ptr<ControlRowBodyButton> pip_body_ = nullptr;
  raw_ptr<OverflowCircleButton> more_button_ = nullptr;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  base::WeakPtrFactory<SiteControlCenterView> weak_factory_{this};
};

BEGIN_METADATA(SiteControlCenterView)
END_METADATA

}  // namespace

bool ToggleSiteControlCenter(BrowserWindowInterface& browser,
                             views::View* anchor) {
  auto& popovers = ActivePopovers();
  if (auto it = popovers.find(&browser); it != popovers.end() && it->second) {
    CloseSiteControlCenter(browser);
    return true;
  }

  TabStripModel* tab_strip = browser.GetTabStripModel();
  content::WebContents* contents =
      tab_strip ? tab_strip->GetActiveWebContents() : nullptr;
  if (!contents) {
    return false;
  }
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry || entry->IsInitialEntry()) {
    return false;
  }

  if (!anchor) {
    if (auto* elements = BrowserElementsViews::From(&browser)) {
      anchor = elements->GetView(kLocationIconElementId);
      if (!anchor) {
        anchor = elements->GetView(kLocationBarElementId);
      }
    }
  }
  if (!anchor || !anchor->GetWidget()) {
    return false;
  }

  auto slot = std::make_unique<ActiveControlCenterSlot>();
  slot->delegate = std::make_unique<SiteControlCenterBubbleDelegate>(anchor);
  slot->delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  slot->delegate->SetShowCloseButton(false);
  slot->delegate->SetShowTitle(false);
  slot->delegate->SetTitle(u"Site Controls");
  slot->delegate->set_margins(gfx::Insets(kPopoverPadding));
  slot->delegate->set_corner_radius(kPopoverCornerRadius);
  slot->delegate->set_close_on_deactivate(true);
  slot->delegate->SetBackgroundColor(kOpaqueBg);
  slot->delegate->SetHighlightedElement(kLocationIconElementId);
  slot->view =
      slot->delegate->SetContentsView(std::make_unique<SiteControlCenterView>(
          browser, slot->delegate.get(), contents));
  slot->delegate->RegisterWindowClosingCallback(base::BindOnce(
      &SiteControlCenterView::OnWindowClosing, base::Unretained(slot->view)));
  slot->widget =
      views::BubbleDialogDelegate::CreateBubble(slot->delegate.get());
  views::Widget* raw_widget = slot->widget.get();
  popovers[&browser] = std::move(slot);
  raw_widget->Show();
  ApplyBubbleGlass(*raw_widget, GlassMaterial::kPopover, /*dark=*/true,
                   kGlassTint, kOpaqueBg);
  return true;
}

void CloseSiteControlCenter(BrowserWindowInterface& browser) {
  auto& popovers = ActivePopovers();
  if (auto it = popovers.find(&browser); it != popovers.end() && it->second) {
    auto slot = std::move(it->second);
    popovers.erase(it);
    if (slot->widget && !slot->widget->IsClosed()) {
      slot->widget->Close();
    }
    ReleasePopoverSlot(std::move(slot));
  }
}

bool IsSiteControlCenterShowing(BrowserWindowInterface& browser) {
  auto& popovers = ActivePopovers();
  auto it = popovers.find(&browser);
  return it != popovers.end() && it->second && it->second->widget &&
         !it->second->widget->IsClosed();
}

}  // namespace myarc
