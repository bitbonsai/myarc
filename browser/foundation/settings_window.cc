#include "myarc/settings_window.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/shell_integration.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"

namespace myarc {

#if BUILDFLAG(IS_MAC)
// settings_window_mac.mm: a titled window whose content fills the title bar.
views::NativeWidget* CreateSettingsNativeWidget(views::Widget* widget);
bool IsSettingsAppActive();
#else
bool IsSettingsAppActive() {
  return true;
}
#endif

namespace {

// Lucide icons (ISC, lucide-static 1.61.0) on their 24 DIP canvas. Arc's
// toolbar symbols are lighter than Lucide's default, so tabs stroke at 1.5.
constexpr gfx::PathElement kSettingsPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 1.5f,
    gfx::MOVE_TO, 9.671f, 4.136f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 1, 4.659f, 0,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 0, 3.319f, 1.915f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 1, 2.33f, 4.033f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 0, 0, 3.831f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 1, -2.33f, 4.033f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 0, -3.319f, 1.915f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 1, -4.659f, 0,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 0, -3.32f, -1.915f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 1, -2.33f, -4.033f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 0, 0, -3.831f,
    gfx::ARC_TO, 2.34f, 2.34f, 0, 0, 1, 6.35f, 6.051f,
    gfx::R_ARC_TO, 2.34f, 2.34f, 0, 0, 0, 3.319f, -1.915f,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::CIRCLE, 12, 12, 3};
constexpr gfx::PathElement kUserPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 1.5f,
    gfx::MOVE_TO, 19, 21, gfx::R_V_LINE_TO, -2,
    gfx::R_ARC_TO, 4, 4, 0, 0, 0, -4, -4, gfx::H_LINE_TO, 9,
    gfx::R_ARC_TO, 4, 4, 0, 0, 0, -4, 4, gfx::R_V_LINE_TO, 2,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::CIRCLE, 12, 7, 4};
constexpr gfx::PathElement kAppWindowPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 1.5f,
    gfx::ROUND_RECT, 2, 4, 20, 16, 2,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 10, 4, gfx::R_V_LINE_TO, 4,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 2, 8, gfx::R_H_LINE_TO, 20,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 6, 4, gfx::R_V_LINE_TO, 4};
constexpr gfx::PathElement kKeyboardPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 1.5f,
    gfx::MOVE_TO, 10, 8, gfx::R_H_LINE_TO, 0.01f,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 12, 12, gfx::R_H_LINE_TO, 0.01f,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 14, 8, gfx::R_H_LINE_TO, 0.01f,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 16, 12, gfx::R_H_LINE_TO, 0.01f,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 18, 8, gfx::R_H_LINE_TO, 0.01f,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 6, 8, gfx::R_H_LINE_TO, 0.01f,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 7, 16, gfx::R_H_LINE_TO, 10,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 8, 12, gfx::R_H_LINE_TO, 0.01f,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::ROUND_RECT, 2, 4, 20, 16, 2};
constexpr gfx::PathElement kSlidersPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 1.5f,
    gfx::MOVE_TO, 10, 5, gfx::H_LINE_TO, 3,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 12, 19, gfx::H_LINE_TO, 3,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 14, 3, gfx::R_V_LINE_TO, 4,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 16, 17, gfx::R_V_LINE_TO, 4,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 21, 12, gfx::R_H_LINE_TO, -9,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 21, 19, gfx::R_H_LINE_TO, -5,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 21, 5, gfx::R_H_LINE_TO, -7,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 8, 10, gfx::R_V_LINE_TO, 4,
    gfx::NEW_PATH, gfx::STROKE, 1.5f, gfx::MOVE_TO, 8, 12, gfx::H_LINE_TO, 3};
// Lucide "heart", filled like Arc's banner button.
constexpr gfx::PathElement kHeartPath[] = {
    gfx::CANVAS_DIMENSIONS, 24,
    gfx::MOVE_TO, 2, 9.5f,
    gfx::R_ARC_TO, 5.5f, 5.5f, 0, 0, 1, 9.591f, -3.676f,
    gfx::R_ARC_TO, 0.56f, 0.56f, 0, 0, 0, 0.818f, 0,
    gfx::ARC_TO, 5.49f, 5.49f, 0, 0, 1, 22, 9.5f,
    gfx::R_CUBIC_TO, 0, 2.29f, -1.5f, 4, -3, 5.5f,
    gfx::R_LINE_TO, -5.492f, 5.313f,
    gfx::R_ARC_TO, 2, 2, 0, 0, 1, -3, 0.019f,
    gfx::LINE_TO, 5, 15,
    gfx::R_CUBIC_TO, -1.5f, -1.5f, -3, -3.2f, -3, -5.5f,
    gfx::CLOSE};

#define MYARC_ICON(name, path)                                   \
  constexpr gfx::VectorIconRep name##Rep[] = {{path}};           \
  constexpr gfx::VectorIcon name(name##Rep, std::size(name##Rep), \
                                 "myarc_settings_" #name)
MYARC_ICON(kSettingsIcon, kSettingsPath);
MYARC_ICON(kUserIcon, kUserPath);
MYARC_ICON(kAppWindowIcon, kAppWindowPath);
MYARC_ICON(kKeyboardIcon, kKeyboardPath);
MYARC_ICON(kSlidersIcon, kSlidersPath);
MYARC_ICON(kHeartIcon, kHeartPath);
#undef MYARC_ICON

// Measured from Arc's dark Settings window at 2x (DIP).
constexpr gfx::Size kWindowSize(684, 650);
constexpr int kTitleBarHeight = 30;
constexpr int kTabTop = 32;
constexpr int kTabHeight = 48;
constexpr int kTabMinWidth = 56;
constexpr int kTabLabelPad = 6;
constexpr float kTabRadius = 8;
constexpr int kTabIconSize = 22;
constexpr int kTabIconTop = 5;
constexpr int kTabIconGap = 0;
constexpr int kContentTop = 122;
constexpr int kSideMargin = 35;
constexpr int kBannerHeight = 36;
constexpr float kBannerRadius = 6;
constexpr int kBannerPad = 16;
constexpr int kBannerGap = 9;
constexpr int kRowHeight = 55;
constexpr float kCardRadius = 6;
constexpr int kButtonHeight = 24;
constexpr float kButtonRadius = 5;
constexpr int kButtonPad = 12;

constexpr SkColor kWindowFill = SkColorSetRGB(0x22, 0x22, 0x23);
constexpr SkColor kTitleText = SkColorSetRGB(0x9E, 0x9E, 0x9F);
constexpr SkColor kTabText = SkColorSetRGB(0x90, 0x91, 0x91);
constexpr SkColor kTabIcon = SkColorSetRGB(0x9F, 0x9E, 0xA0);
constexpr SkColor kTabDisabled = SkColorSetARGB(0x66, 0x90, 0x91, 0x91);
constexpr SkColor kTabSelectedFill = SkColorSetRGB(0x28, 0x28, 0x29);
constexpr SkColor kTabSelectedStroke = SkColorSetRGB(0x3B, 0x3B, 0x3C);
constexpr SkColor kAccent = SkColorSetRGB(0x0B, 0x86, 0xFE);
constexpr SkColor kBannerFill = SkColorSetRGB(0x65, 0x60, 0x68);
constexpr SkColor kBannerText = SkColorSetRGB(0xD4, 0xD3, 0xD5);
constexpr SkColor kBlueButton = SkColorSetRGB(0x00, 0x7A, 0xFF);
constexpr SkColor kCardFill = SkColorSetRGB(0x25, 0x25, 0x26);
constexpr SkColor kCardStroke = SkColorSetRGB(0x3A, 0x3A, 0x3B);
constexpr SkColor kCardText = SkColorSetRGB(0xC2, 0xC2, 0xC3);
constexpr SkColor kGrayButton = SkColorSetRGB(0x45, 0x45, 0x46);
constexpr SkColor kButtonText = SkColorSetRGB(0xE3, 0xE3, 0xE3);

constexpr char16_t kComingLater[] = u"Not in myArc yet";

gfx::FontList Font(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.Derive(size - base.GetFontSize(), gfx::Font::NORMAL, weight);
}

views::Label* AddLabel(views::View* parent,
                       std::u16string text,
                       int size,
                       gfx::Font::Weight weight,
                       SkColor color) {
  auto* label = parent->AddChildView(std::make_unique<views::Label>(text));
  label->SetFontList(Font(size, weight));
  label->SetEnabledColor(color);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  return label;
}

// Fills a rounded rect and strokes a one-pixel hairline, as AppKit does.
void PaintPanel(gfx::Canvas* canvas,
                const gfx::Rect& bounds,
                float radius,
                SkColor fill,
                std::optional<SkColor> stroke) {
  gfx::ScopedCanvas scoped(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  gfx::RectF rect(bounds);
  rect.Scale(scale);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(fill);
  canvas->DrawRoundRect(rect, radius * scale, flags);
  if (stroke) {
    rect.Inset(0.5f);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(*stroke);
    canvas->DrawRoundRect(rect, radius * scale - 0.5f, flags);
  }
}

// Arc's rounded push button: an optional icon, then the title.
class PillButton : public views::Button {
  METADATA_HEADER(PillButton, views::Button)

 public:
  PillButton(PressedCallback callback,
             std::u16string text,
             SkColor fill,
             SkColor text_color,
             gfx::Font::Weight weight,
             const gfx::VectorIcon* icon)
      : views::Button(std::move(callback)), fill_(fill) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::VH(0, kButtonPad - (icon ? 1 : 0)), 3));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    if (icon) {
      AddChildView(std::make_unique<views::ImageView>(
          ui::ImageModel::FromVectorIcon(*icon, text_color, 16)));
    }
    AddLabel(this, text, 13, weight, text_color);
    SetAccessibleName(text);
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(
        views::Button::CalculatePreferredSize(available_size).width(),
        kButtonHeight);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    SkColor fill = fill_;
    if (GetState() == STATE_PRESSED) {
      fill = SkColorSetA(fill, 0xC0);
    }
    PaintPanel(canvas, GetLocalBounds(), kButtonRadius, fill, std::nullopt);
  }

 private:
  const SkColor fill_;
};

BEGIN_METADATA(PillButton)
END_METADATA

// One icon-over-label item in the section tab row.
class SectionTab : public views::Button {
  METADATA_HEADER(SectionTab, views::Button)

 public:
  SectionTab(PressedCallback callback,
             std::u16string title,
             const gfx::VectorIcon& icon)
      : views::Button(std::move(callback)), icon_(icon) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::TLBR(kTabIconTop, kTabLabelPad, 0, kTabLabelPad),
        kTabIconGap));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    image_ = AddChildView(std::make_unique<views::ImageView>());
    image_->SetImageSize(gfx::Size(kTabIconSize, kTabIconSize));
    label_ = AddLabel(this, title, 11, gfx::Font::Weight::NORMAL, kTabText);
    GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
    SetAccessibleName(title);
    UpdateColors();
  }

  void SetSelected(bool selected) {
    selected_ = selected;
    GetViewAccessibility().SetIsSelected(selected);
    UpdateColors();
    SchedulePaint();
  }

  void RefreshColors() { UpdateColors(); }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int width = label_->GetPreferredSize().width() + 2 * kTabLabelPad;
    return gfx::Size(std::max(kTabMinWidth, width), kTabHeight);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    if (selected_) {
      PaintPanel(canvas, GetLocalBounds(), kTabRadius, kTabSelectedFill,
                 kTabSelectedStroke);
    }
  }

 private:
  void UpdateColors() {
    const SkColor text = !GetEnabled() ? kTabDisabled
                         : selected_   ? kAccent
                                       : kTabText;
    const SkColor icon = !GetEnabled() ? kTabDisabled
                         : selected_   ? kAccent
                                       : kTabIcon;
    label_->SetEnabledColor(text);
    image_->SetImage(
        ui::ImageModel::FromVectorIcon(*icon_, icon, kTabIconSize));
  }

  const raw_ref<const gfx::VectorIcon> icon_;
  raw_ptr<views::ImageView> image_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  bool selected_ = false;
};

BEGIN_METADATA(SectionTab)
END_METADATA

// Rounded fill with a hairline edge, laid out by its own layout manager.
class Panel : public views::View {
  METADATA_HEADER(Panel, views::View)

 public:
  Panel(SkColor fill, std::optional<SkColor> stroke, float radius)
      : fill_(fill), stroke_(stroke), radius_(radius) {}

  void OnPaintBackground(gfx::Canvas* canvas) override {
    PaintPanel(canvas, GetLocalBounds(), radius_, fill_, stroke_);
  }

 private:
  const SkColor fill_;
  const std::optional<SkColor> stroke_;
  const float radius_;
};

BEGIN_METADATA(Panel)
END_METADATA

// General: Arc's default-browser banner, backed by Chromium's worker.
class GeneralPage : public views::View {
  METADATA_HEADER(GeneralPage, views::View)

 public:
  GeneralPage() {
    banner_ = AddChildView(
        std::make_unique<Panel>(kBannerFill, std::nullopt, kBannerRadius));
    auto* layout = banner_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::VH(0, kBannerPad), kBannerGap));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    message_ = AddLabel(banner_, u"Checking your default browser…", 13,
                        gfx::Font::Weight::NORMAL, kBannerText);
    make_default_ = banner_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&GeneralPage::MakeDefault, base::Unretained(this)),
        u"Make myArc your default", kBlueButton, SK_ColorWHITE,
        gfx::Font::Weight::NORMAL, &kHeartIcon));
    make_default_->SetVisible(false);
    Refresh();
  }

  void Refresh() {
    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartCheckIsDefault(base::BindOnce(&GeneralPage::OnDefaultState,
                                             weak_factory_.GetWeakPtr()));
  }

  void Layout(PassKey) override {
    const gfx::Size size = banner_->GetPreferredSize();
    banner_->SetBounds((width() - size.width()) / 2, kContentTop,
                       size.width(), kBannerHeight);
  }

 private:
  void MakeDefault() {
    make_default_->SetEnabled(false);
    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartSetAsDefault(base::BindOnce(&GeneralPage::OnDefaultState,
                                           weak_factory_.GetWeakPtr()));
  }

  void OnDefaultState(shell_integration::DefaultWebClientState state) {
    const bool is_default = state == shell_integration::IS_DEFAULT;
    message_->SetText(
        is_default ? u"myArc is your default browser."
        : state == shell_integration::UNKNOWN_DEFAULT
            ? u"myArc can\u2019t tell which browser is your default."
            : u"myArc isn\u2019t your default browser.");
    make_default_->SetVisible(!is_default);
    make_default_->SetEnabled(true);
    InvalidateLayout();
  }

  raw_ptr<Panel> banner_ = nullptr;
  raw_ptr<views::Label> message_ = nullptr;
  raw_ptr<PillButton> make_default_ = nullptr;
  base::WeakPtrFactory<GeneralPage> weak_factory_{this};
};

BEGIN_METADATA(GeneralPage)
END_METADATA

// Advanced: the way out to Chromium's full settings page.
class AdvancedPage : public views::View {
  METADATA_HEADER(AdvancedPage, views::View)

 public:
  explicit AdvancedPage(base::RepeatingClosure more_settings) {
    card_ = AddChildView(
        std::make_unique<Panel>(kCardFill, kCardStroke, kCardRadius));
    auto* layout = card_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::VH(0, kBannerPad)));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    auto* label = AddLabel(card_, u"Chromium Settings", 13,
                           gfx::Font::Weight::NORMAL, kCardText);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->SetFlexForView(label, 1);
    card_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(std::move(more_settings)), u"More Settings…",
        kGrayButton, kButtonText, gfx::Font::Weight::NORMAL, nullptr));
  }

  void Layout(PassKey) override {
    card_->SetBounds(kSideMargin, kContentTop, width() - 2 * kSideMargin,
                     kRowHeight);
  }

 private:
  raw_ptr<Panel> card_ = nullptr;
};

BEGIN_METADATA(AdvancedPage)
END_METADATA

struct SectionSpec {
  const char16_t* title;
  // Static icons in a constexpr table.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
  std::optional<SettingsWindow::Section> section;  // Unset: not built yet.
};

// Arc's tab row minus Account (myArc has no account), Max (AI is off) and
// Icon (no app icon picker). Profiles, Links and Shortcuts stay visible but
// disabled so the row keeps Arc's rhythm.
constexpr std::array<SectionSpec, 5> kSections = {{
    {u"General", &kSettingsIcon, SettingsWindow::Section::kGeneral},
    {u"Profiles", &kUserIcon, std::nullopt},
    {u"Links", &kAppWindowIcon, std::nullopt},
    {u"Shortcuts", &kKeyboardIcon, std::nullopt},
    {u"Advanced", &kSlidersIcon, SettingsWindow::Section::kAdvanced},
}};

class SettingsView : public views::View {
  METADATA_HEADER(SettingsView, views::View)

 public:
  SettingsView(base::RepeatingClosure more_settings,
               base::RepeatingClosure close)
      : close_(std::move(close)) {
    title_ = AddLabel(this, u"", 13, gfx::Font::Weight::BOLD, kTitleText);
    for (const SectionSpec& spec : kSections) {
      auto* tab = AddChildView(std::make_unique<SectionTab>(
          views::Button::PressedCallback(), spec.title, *spec.icon));
      if (spec.section) {
        tab->SetCallback(base::BindRepeating(&SettingsView::Select,
                                             base::Unretained(this),
                                             *spec.section));
      } else {
        tab->SetEnabled(false);
        tab->RefreshColors();
        tab->SetTooltipText(kComingLater);
      }
      tabs_.push_back(tab);
    }
    general_ = AddChildView(std::make_unique<GeneralPage>());
    advanced_ = AddChildView(std::make_unique<AdvancedPage>(more_settings));
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
    AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_PLATFORM_ACCELERATOR));
    GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  }

  void Select(SettingsWindow::Section section) {
    const bool general = section == SettingsWindow::Section::kGeneral;
    general_->SetVisible(general);
    advanced_->SetVisible(!general);
    if (general) {
      general_->Refresh();
    }
    const std::u16string title = general ? u"General" : u"Advanced";
    for (size_t i = 0; i < kSections.size(); ++i) {
      tabs_[i]->SetSelected(kSections[i].section == section);
    }
    title_->SetText(title);
    if (GetWidget() && GetWidget()->widget_delegate()) {
      GetWidget()->widget_delegate()->SetTitle(title);
    }
    InvalidateLayout();
  }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    close_.Run();
    return true;
  }

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->DrawColor(kWindowFill);
  }

  void Layout(PassKey) override {
    const gfx::Size title = title_->GetPreferredSize();
    title_->SetBounds((width() - title.width()) / 2,
                      (kTitleBarHeight - title.height()) / 2, title.width(),
                      title.height());
    int row = 0;
    for (views::View* tab : tabs_) {
      row += tab->GetPreferredSize().width();
    }
    int x = (width() - row) / 2;
    for (views::View* tab : tabs_) {
      const int w = tab->GetPreferredSize().width();
      tab->SetBounds(x, kTabTop, w, kTabHeight);
      x += w;
    }
    general_->SetBoundsRect(GetLocalBounds());
    advanced_->SetBoundsRect(GetLocalBounds());
  }

 private:
  base::RepeatingClosure close_;
  raw_ptr<views::Label> title_ = nullptr;
  std::vector<raw_ptr<SectionTab>> tabs_;
  raw_ptr<GeneralPage> general_ = nullptr;
  raw_ptr<AdvancedPage> advanced_ = nullptr;
};

BEGIN_METADATA(SettingsView)
END_METADATA

// The content reaches under the transparent title bar; its strip drags the
// window like a title bar.
class SettingsClientView : public views::ClientView {
  METADATA_HEADER(SettingsClientView, views::ClientView)

 public:
  using views::ClientView::ClientView;

  int NonClientHitTest(const gfx::Point& point) override {
    if (bounds().Contains(point) && point.y() < kTitleBarHeight) {
      return HTCAPTION;
    }
    return views::ClientView::NonClientHitTest(point);
  }
};

BEGIN_METADATA(SettingsClientView)
END_METADATA

SettingsWindow* g_open = nullptr;

}  // namespace

SettingsWindow::SettingsWindow(base::RepeatingClosure open_more_settings)
    : open_more_settings_(std::move(open_more_settings)) {}

SettingsWindow::~SettingsWindow() {
  widget_.reset();
  delegate_.reset();
  if (g_open == this) {
    g_open = nullptr;
  }
}

// static
bool SettingsWindow::ActivateOpen(bool inactive) {
  if (!g_open || !g_open->widget_) {
    return false;
  }
  // Widget::Show() activates the app; a background app stays behind.
  if (!inactive && IsSettingsAppActive() && !g_open->widget_->IsActive()) {
    g_open->widget_->Show();
  }
  return true;
}

void SettingsWindow::Show(Section section, bool inactive) {
  SettingsView* view = nullptr;
  if (!widget_) {
    delegate_ = std::make_unique<views::WidgetDelegate>();
    delegate_->SetCanResize(false);
    delegate_->SetCanMaximize(false);
    delegate_->SetCanFullscreen(false);
    delegate_->SetCanMinimize(true);
    delegate_->SetTitle(u"Settings");
    view = delegate_->SetContentsView(std::make_unique<SettingsView>(
        base::BindRepeating(&SettingsWindow::OpenMoreSettings,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&SettingsWindow::Close,
                            weak_factory_.GetWeakPtr())));
    delegate_->SetClientViewFactory(
        base::BindOnce([](views::Widget* widget, views::View* contents)
                           -> std::unique_ptr<views::ClientView> {
          return std::make_unique<SettingsClientView>(widget, contents);
        }));

    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate_.get();
    params.name = "MyarcSettingsWindow";
    params.bounds = gfx::Rect(kWindowSize);
    widget_ = std::make_unique<views::Widget>();
#if BUILDFLAG(IS_MAC)
    params.native_widget = CreateSettingsNativeWidget(widget_.get());
#endif
    widget_->Init(std::move(params));
    widget_->MakeCloseSynchronous(base::BindOnce(
        [](base::WeakPtr<SettingsWindow> self,
           views::Widget::ClosedReason reason) {
          if (self) {
            self->Close();
          }
        },
        weak_factory_.GetWeakPtr()));
    widget_->CenterWindow(kWindowSize);
    g_open = this;
  } else {
    view = static_cast<SettingsView*>(delegate_->GetContentsView());
  }
  view->Select(section);
  if (inactive || !IsSettingsAppActive()) {
    widget_->ShowInactive();
  } else {
    widget_->Show();
  }
}

bool SettingsWindow::IsShowing() const {
  return widget_ && widget_->IsVisible();
}

void SettingsWindow::PressMoreSettingsForCapture() {
  if (widget_) {
    OpenMoreSettings();
  }
}

void SettingsWindow::Close() {
  // Callers may be inside the widget's own event dispatch; destroy it later.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SettingsWindow> self) {
                       if (self) {
                         self->widget_.reset();
                         self->delegate_.reset();
                         if (g_open == self.get()) {
                           g_open = nullptr;
                         }
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

void SettingsWindow::OpenMoreSettings() {
  open_more_settings_.Run();
  Close();
}

}  // namespace myarc
