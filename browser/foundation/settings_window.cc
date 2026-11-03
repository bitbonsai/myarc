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
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
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
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
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
// "Nightly <label>" from the packaged bundle, else "Development build".
std::u16string MyarcVersionLabel();
ui::ImageModel AppIconImage();
#else
bool IsSettingsAppActive() {
  return true;
}
std::u16string MyarcVersionLabel() {
  return u"Development build";
}
ui::ImageModel AppIconImage() {
  return ui::ImageModel();
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
// Lucide "chevrons-up-down", the arrows of Arc's pop-up buttons.
constexpr gfx::PathElement kChevronsUpDownPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 7, 15, gfx::R_LINE_TO, 5, 5, gfx::R_LINE_TO, 5, -5,
    gfx::NEW_PATH, gfx::STROKE, 2,
    gfx::MOVE_TO, 7, 9, gfx::R_LINE_TO, 5, -5, gfx::R_LINE_TO, 5, 5};
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
MYARC_ICON(kChevronsUpDownIcon, kChevronsUpDownPath);
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
// Arc's General panels: wider margins than the other pages, 30 DIP under the
// banner, 9 DIP apart.
constexpr int kGeneralMargin = 64;
constexpr int kGeneralTop = kContentTop + kBannerHeight + 30;
constexpr int kGeneralGap = 9;
constexpr int kGeneralRowPad = 10;
constexpr int kSwitchRowHeight = 36;
constexpr int kProfileRowHeight = 47;
constexpr int kProfileTextWidth = 340;
// AppKit's regular switch: a 36 × 16 track around a 22 × 12 pill knob.
constexpr gfx::Size kSwitchSize(36, 16);
constexpr gfx::Size kKnobSize(22, 12);
// The myArc card at the foot of General.
constexpr int kBrandHeight = 76;
constexpr int kBrandBottom = 40;
constexpr int kBrandPad = 16;
constexpr int kBrandIconSize = 44;
constexpr float kBrandRadius = 10;

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
constexpr SkColor kSwitchTrack = SkColorSetRGB(0x37, 0x37, 0x38);
constexpr SkColor kSwitchKnob = SkColorSetRGB(0xC8, 0xC8, 0xC8);
// myArc brand colors (design.md): midnight card, ivory name, primary blue.
constexpr SkColor kBrandFill = SkColorSetRGB(0x10, 0x1C, 0x2B);
constexpr SkColor kBrandStroke = SkColorSetRGB(0x22, 0x31, 0x44);
constexpr SkColor kBrandName = SkColorSetRGB(0xF5, 0xF2, 0xEC);
constexpr SkColor kBrandQuiet = SkColorSetRGB(0xB1, 0xBC, 0xC9);
constexpr SkColor kBrandFaint = SkColorSetRGB(0x7D, 0x8A, 0x99);
constexpr SkColor kBrandBlue = SkColorSetRGB(0x03, 0x4B, 0xF4);

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
    label_ = AddLabel(this, text, 13, weight, text_color);
    SetAccessibleName(text);
  }

  void SetText(const std::u16string& text) {
    label_->SetText(text);
    SetAccessibleName(text);
    PreferredSizeChanged();
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
  raw_ptr<views::Label> label_ = nullptr;
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

// Arc's on/off switch: a gray track with a light pill knob, blue when on.
class Switch : public views::Button {
  METADATA_HEADER(Switch, views::Button)

 public:
  Switch(PressedCallback callback, std::u16string name)
      : views::Button(std::move(callback)) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kSwitch);
    SetAccessibleName(name);
    SetOn(false);
  }

  void SetOn(bool on) {
    on_ = on;
    GetViewAccessibility().SetCheckedState(
        on ? ax::mojom::CheckedState::kTrue : ax::mojom::CheckedState::kFalse);
    SchedulePaint();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return kSwitchSize;
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    const gfx::Rect track(
        gfx::Point(0, (height() - kSwitchSize.height()) / 2), kSwitchSize);
    const SkColor alpha = GetEnabled() ? 0xFF : 0x66;
    PaintPanel(canvas, track, kSwitchSize.height() / 2.f,
               SkColorSetA(on_ ? kAccent : kSwitchTrack, alpha), std::nullopt);
    const int inset_x = 1;
    const int inset_y = (kSwitchSize.height() - kKnobSize.height()) / 2;
    const int x = on_ ? track.right() - inset_x - kKnobSize.width()
                      : track.x() + inset_x;
    const gfx::Rect knob(gfx::Point(x, track.y() + inset_y), kKnobSize);
    PaintPanel(canvas, knob, kKnobSize.height() / 2.f,
               SkColorSetA(on_ ? SK_ColorWHITE : kSwitchKnob, alpha),
               std::nullopt);
  }

 private:
  bool on_ = false;
};

BEGIN_METADATA(Switch)
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

// One panel row: a label at left, `control` at right.
Panel* AddRowPanel(views::View* parent,
                   std::u16string text,
                   bool multi_line,
                   std::unique_ptr<views::View> control) {
  auto* panel = parent->AddChildView(
      std::make_unique<Panel>(kCardFill, kCardStroke, kCardRadius));
  auto* layout = panel->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kGeneralRowPad)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* label =
      AddLabel(panel, text, 13, gfx::Font::Weight::NORMAL, kCardText);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  if (multi_line) {
    label->SetMultiLine(true);
    label->SetMaximumWidth(kProfileTextWidth);
  }
  layout->SetFlexForView(label, 1);
  panel->AddChildView(std::move(control));
  return panel;
}

// General: Arc's default-browser banner, backed by Chromium's worker, its
// quit warning and Profile Settings rows, and a card about myArc.
class GeneralPage : public views::View {
  METADATA_HEADER(GeneralPage, views::View)

 public:
  GeneralPage(PrefService* local_state,
              base::RepeatingClosure show_profiles,
              base::RepeatingClosure open_site)
      : local_state_(local_state) {
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

    // Chromium's Local State pref behind the app menu's "Warn Before
    // Quitting"; either side sees the other's change.
    auto quit_switch = std::make_unique<Switch>(
        base::BindRepeating(&GeneralPage::ToggleQuitWarning,
                            base::Unretained(this)),
        u"Warn before quitting");
    quit_switch_ = quit_switch.get();
    quit_panel_ = AddRowPanel(this, u"Warn before quitting",
                              /*multi_line=*/false, std::move(quit_switch));
#if BUILDFLAG(IS_MAC)
    registrar_.Init(local_state_);
    registrar_.Add(prefs::kConfirmToQuitEnabled,
                   base::BindRepeating(&GeneralPage::UpdateQuitWarning,
                                       base::Unretained(this)));
#endif
    UpdateQuitWarning();

    profile_panel_ = AddRowPanel(
        this,
        u"Manage your default search engine, Auto Archive and downloads in "
        u"Profile Settings.",
        /*multi_line=*/true,
        std::make_unique<PillButton>(std::move(show_profiles),
                                     u"Profile Settings…", kGrayButton,
                                     kButtonText, gfx::Font::Weight::NORMAL,
                                     nullptr));

    brand_ = AddChildView(
        std::make_unique<Panel>(kBrandFill, kBrandStroke, kBrandRadius));
    auto* brand_layout =
        brand_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets::VH(0, kBrandPad), 12));
    brand_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    auto* icon = brand_->AddChildView(
        std::make_unique<views::ImageView>(AppIconImage()));
    icon->SetImageSize(gfx::Size(kBrandIconSize, kBrandIconSize));
    auto* text = brand_->AddChildView(std::make_unique<views::View>());
    text->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 1));
    AddLabel(text, u"myArc", 15, gfx::Font::Weight::SEMIBOLD, kBrandName)
        ->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddLabel(text, u"forever yours.", 12, gfx::Font::Weight::NORMAL,
             kBrandQuiet)
        ->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddLabel(text, MyarcVersionLabel(), 11, gfx::Font::Weight::NORMAL,
             kBrandFaint)
        ->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    brand_layout->SetFlexForView(text, 1);
    brand_->AddChildView(std::make_unique<PillButton>(
        std::move(open_site), u"Visit myarc.dev", kBrandBlue, SK_ColorWHITE,
        gfx::Font::Weight::NORMAL, nullptr));

    Refresh();
  }

  void Refresh() {
    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartCheckIsDefault(base::BindOnce(&GeneralPage::OnDefaultState,
                                             weak_factory_.GetWeakPtr()));
    UpdateQuitWarning();
  }

  void Layout(PassKey) override {
    const gfx::Size size = banner_->GetPreferredSize();
    banner_->SetBounds((width() - size.width()) / 2, kContentTop,
                       size.width(), kBannerHeight);
    const int panel_width = width() - 2 * kGeneralMargin;
    quit_panel_->SetBounds(kGeneralMargin, kGeneralTop, panel_width,
                           kSwitchRowHeight);
    profile_panel_->SetBounds(kGeneralMargin,
                              quit_panel_->bounds().bottom() + kGeneralGap,
                              panel_width, kProfileRowHeight);
    brand_->SetBounds(kGeneralMargin, height() - kBrandBottom - kBrandHeight,
                      panel_width, kBrandHeight);
  }

 private:
  // The pref is Mac only; elsewhere the switch stays off and disabled.
  const PrefService::Preference* QuitPref() const {
#if BUILDFLAG(IS_MAC)
    return local_state_
               ? local_state_->FindPreference(prefs::kConfirmToQuitEnabled)
               : nullptr;
#else
    return nullptr;
#endif
  }

  void UpdateQuitWarning() {
    const PrefService::Preference* pref = QuitPref();
    quit_switch_->SetOn(pref && pref->GetValue()->is_bool() &&
                        pref->GetValue()->GetBool());
    // Like the menu item, a managed value can't be changed here.
    quit_switch_->SetEnabled(pref && pref->IsUserModifiable());
  }

  void ToggleQuitWarning() {
    const PrefService::Preference* pref = QuitPref();
    if (!pref || !pref->IsUserModifiable() || !pref->GetValue()->is_bool()) {
      return;
    }
    local_state_->SetBoolean(pref->name(), !pref->GetValue()->GetBool());
    UpdateQuitWarning();
  }

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

  const raw_ptr<PrefService> local_state_;
  PrefChangeRegistrar registrar_;
  raw_ptr<Panel> banner_ = nullptr;
  raw_ptr<views::Label> message_ = nullptr;
  raw_ptr<PillButton> make_default_ = nullptr;
  raw_ptr<Panel> quit_panel_ = nullptr;
  raw_ptr<Switch> quit_switch_ = nullptr;
  raw_ptr<Panel> profile_panel_ = nullptr;
  raw_ptr<Panel> brand_ = nullptr;
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

// Arc's "Archive tabs after" choices; Auto Archive can't be turned off.
struct ArchiveChoice {
  uint64_t secs;
  const char16_t* label;
};

constexpr uint64_t kHourSecs = 60 * 60;
constexpr std::array<ArchiveChoice, 4> kArchiveChoices = {{
    {12 * kHourSecs, u"12 hours (default)"},
    {24 * kHourSecs, u"24 hours"},
    {7 * 24 * kHourSecs, u"7 days"},
    {30 * 24 * kHourSecs, u"30 days"},
}};

// Profiles: Arc's "Archive tabs after" row for the bound Profile.
class ProfilesPage : public views::View, public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(ProfilesPage, views::View)

 public:
  explicit ProfilesPage(SettingsWindow::ArchiveTimer timer)
      : timer_(std::move(timer)) {
    card_ = AddChildView(
        std::make_unique<Panel>(kCardFill, kCardStroke, kCardRadius));
    auto* layout = card_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::VH(0, kBannerPad)));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    // The title over the Profile it applies to, which may not be the Profile
    // of the window in front (Incognito edits its regular Profile).
    auto* text = card_->AddChildView(std::make_unique<views::View>());
    text->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    auto* label = AddLabel(text, u"Archive tabs after", 13,
                           gfx::Font::Weight::NORMAL, kCardText);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    auto* profile = AddLabel(text, u"Profile: " + timer_.profile_name, 11,
                             gfx::Font::Weight::NORMAL, kTabText);
    profile->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    const std::u16string tooltip =
        u"Unpinned tabs you haven\u2019t viewed for this long move to "
        u"Archived Tabs. Applies to the " +
        timer_.profile_name + u" Profile.";
    label->SetTooltipText(tooltip);
    profile->SetTooltipText(tooltip);
    layout->SetFlexForView(text, 1);
    picker_ = card_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&ProfilesPage::OpenMenu, base::Unretained(this)),
        u"", kGrayButton, kButtonText, gfx::Font::Weight::NORMAL, nullptr));
    picker_->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(kChevronsUpDownIcon, kButtonText, 12)));
    picker_->GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
    picker_->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
    Refresh();
  }

  void Refresh() {
    secs_ = timer_.get.Run();
    std::u16string text = u"Unavailable";
    for (const ArchiveChoice& choice : kArchiveChoices) {
      if (choice.secs == secs_) {
        text = choice.label;
      }
    }
    picker_->SetText(text);
    picker_->SetAccessibleName(u"Archive tabs after: " + text);
    picker_->SetEnabled(secs_ != 0);
    InvalidateLayout();
  }

  void Layout(PassKey) override {
    card_->SetBounds(kSideMargin, kContentTop, width() - 2 * kSideMargin,
                     kRowHeight);
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    return kArchiveChoices[command_id].secs == secs_;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    timer_.set.Run(kArchiveChoices[command_id].secs);
    Refresh();
  }

 private:
  void OpenMenu() {
    if (!GetWidget() || (runner_ && runner_->IsRunning())) {
      return;
    }
    // Another window may have changed the timer, or the workspace finished
    // loading, since the page last looked.
    Refresh();
    if (!secs_) {
      return;
    }
    runner_.reset();
    model_ = std::make_unique<ui::SimpleMenuModel>(this);
    for (size_t i = 0; i < kArchiveChoices.size(); ++i) {
      model_->AddRadioItem(static_cast<int>(i), kArchiveChoices[i].label,
                           /*group_id=*/0);
    }
    runner_ = std::make_unique<views::MenuRunner>(model_.get(),
                                                  views::MenuRunner::NO_FLAGS);
    runner_->RunMenuAt(GetWidget(), nullptr, picker_->GetBoundsInScreen(),
                       views::MenuAnchorPosition::kTopLeft,
                       ui::mojom::MenuSourceType::kNone);
  }

  SettingsWindow::ArchiveTimer timer_;
  uint64_t secs_ = 0;
  raw_ptr<Panel> card_ = nullptr;
  raw_ptr<PillButton> picker_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> model_;
  std::unique_ptr<views::MenuRunner> runner_;
};

BEGIN_METADATA(ProfilesPage)
END_METADATA

struct SectionSpec {
  const char16_t* title;
  // Static icons in a constexpr table.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
  std::optional<SettingsWindow::Section> section;  // Unset: not built yet.
};

// Arc's tab row minus Account (myArc has no account), Max (AI is off) and
// Icon (no app icon picker). Links and Shortcuts stay visible but disabled so
// the row keeps Arc's rhythm.
constexpr std::array<SectionSpec, 5> kSections = {{
    {u"General", &kSettingsIcon, SettingsWindow::Section::kGeneral},
    {u"Profiles", &kUserIcon, SettingsWindow::Section::kProfiles},
    {u"Links", &kAppWindowIcon, std::nullopt},
    {u"Shortcuts", &kKeyboardIcon, std::nullopt},
    {u"Advanced", &kSlidersIcon, SettingsWindow::Section::kAdvanced},
}};

class SettingsView : public views::View {
  METADATA_HEADER(SettingsView, views::View)

 public:
  SettingsView(PrefService* local_state,
               base::RepeatingClosure more_settings,
               base::RepeatingClosure open_site,
               SettingsWindow::ArchiveTimer archive_timer,
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
    general_ = AddChildView(std::make_unique<GeneralPage>(
        local_state,
        base::BindRepeating(&SettingsView::Select, base::Unretained(this),
                            SettingsWindow::Section::kProfiles),
        std::move(open_site)));
    profiles_ =
        AddChildView(std::make_unique<ProfilesPage>(std::move(archive_timer)));
    advanced_ = AddChildView(std::make_unique<AdvancedPage>(more_settings));
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
    AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_PLATFORM_ACCELERATOR));
    GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  }

  void Select(SettingsWindow::Section section) {
    general_->SetVisible(section == SettingsWindow::Section::kGeneral);
    profiles_->SetVisible(section == SettingsWindow::Section::kProfiles);
    advanced_->SetVisible(section == SettingsWindow::Section::kAdvanced);
    if (section == SettingsWindow::Section::kGeneral) {
      general_->Refresh();
    } else if (section == SettingsWindow::Section::kProfiles) {
      profiles_->Refresh();
    }
    std::u16string title;
    for (size_t i = 0; i < kSections.size(); ++i) {
      tabs_[i]->SetSelected(kSections[i].section == section);
      if (kSections[i].section == section) {
        title = kSections[i].title;
      }
    }
    title_->SetText(title);
    if (GetWidget() && GetWidget()->widget_delegate()) {
      GetWidget()->widget_delegate()->SetTitle(title);
    }
    InvalidateLayout();
  }

  void RefreshProfiles() { profiles_->Refresh(); }

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
    profiles_->SetBoundsRect(GetLocalBounds());
    advanced_->SetBoundsRect(GetLocalBounds());
  }

 private:
  base::RepeatingClosure close_;
  raw_ptr<views::Label> title_ = nullptr;
  std::vector<raw_ptr<SectionTab>> tabs_;
  raw_ptr<GeneralPage> general_ = nullptr;
  raw_ptr<ProfilesPage> profiles_ = nullptr;
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

SettingsWindow::SettingsWindow(const Profile* profile,
                               PrefService* local_state,
                               base::RepeatingClosure open_more_settings,
                               base::RepeatingClosure open_site,
                               ArchiveTimer archive_timer)
    : profile_(profile),
      local_state_(local_state),
      open_more_settings_(std::move(open_more_settings)),
      open_site_(std::move(open_site)),
      archive_timer_(std::move(archive_timer)) {}

SettingsWindow::~SettingsWindow() {
  widget_.reset();
  delegate_.reset();
  if (g_open == this) {
    g_open = nullptr;
  }
}

// static
bool SettingsWindow::ActivateOpen(const Profile* profile, bool inactive) {
  if (!g_open || !g_open->widget_) {
    return false;
  }
  if (g_open->profile_ != profile) {
    // Settings of another Profile: the caller opens its own in its place.
    g_open->widget_->Hide();
    g_open->Close();
    g_open = nullptr;
    return false;
  }
  static_cast<SettingsView*>(g_open->delegate_->GetContentsView())
      ->RefreshProfiles();
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
        local_state_,
        base::BindRepeating(&SettingsWindow::OpenMoreSettings,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&SettingsWindow::OpenSite,
                            weak_factory_.GetWeakPtr()),
        archive_timer_,
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

void SettingsWindow::OpenSite() {
  open_site_.Run();
  Close();
}

}  // namespace myarc
