#include "myarc/settings_window.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "myarc/arc_import.h"
#include "myarc/control_prefs.h"
#include "myarc/little_arc_prefs.h"
#include "myarc/test_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRRect.h"
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
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
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

// Lucide "picture-in-picture-2", a card over a page, for the Peek row.
constexpr gfx::PathElement kPeekRowPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 21, 9, gfx::V_LINE_TO, 6,
    gfx::ARC_TO, 2, 2, 0, 0, 0, 19, 4, gfx::H_LINE_TO, 4,
    gfx::ARC_TO, 2, 2, 0, 0, 0, 2, 6, gfx::V_LINE_TO, 16,
    gfx::ARC_TO, 2, 2, 0, 0, 0, 4, 18, gfx::H_LINE_TO, 8,
    gfx::NEW_PATH, gfx::STROKE, 2, gfx::ROUND_RECT, 12, 13, 10, 7, 2};

// Lucide "plus" and "minus", for the Profile list's buttons.
constexpr gfx::PathElement kPlusPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 5, 12, gfx::H_LINE_TO, 19,
    gfx::MOVE_TO, 12, 5, gfx::V_LINE_TO, 19};
constexpr gfx::PathElement kMinusPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2,
    gfx::MOVE_TO, 5, 12, gfx::H_LINE_TO, 19};

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
MYARC_ICON(kPeekRowIcon, kPeekRowPath);
MYARC_ICON(kPlusIcon, kPlusPath);
MYARC_ICON(kMinusIcon, kMinusPath);
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
// The Profiles page's list, as in Arc's: rows of icon, name and Spaces count.
constexpr int kListWidth = 232;
constexpr int kListRowHeight = 34;
constexpr int kListRowPad = 10;
constexpr int kListRowGap = 8;
constexpr int kListIconSize = 18;
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
constexpr SkColor kListSelectedFill = SkColorSetRGB(0x26, 0x30, 0x42);
constexpr SkColor kDeleteButton = SkColorSetRGB(0xD7, 0x3A, 0x31);
constexpr SkColor kErrorText = SkColorSetRGB(0xFF, 0x9F, 0x8F);
constexpr SkColor kSwitchTrack = SkColorSetRGB(0x37, 0x37, 0x38);
constexpr SkColor kSwitchKnob = SkColorSetRGB(0xC8, 0xC8, 0xC8);
// Arc's grouped switch rows: a 20 DIP rounded tile 10 DIP in, the label 8 DIP
// after it, rows 41 DIP tall, or 50 with a second grey line, and a hairline
// between rows from the tile's left edge.
constexpr int kTileSize = 20;
constexpr float kTileRadius = 5;
constexpr int kTileIcon = 13;
constexpr int kGroupPad = 10;
constexpr int kGroupGap = 8;
constexpr int kGroupRowHeight = 41;
constexpr int kGroupTallRowHeight = 50;
constexpr SkColor kTileBlue = SkColorSetRGB(0x0A, 0x84, 0xFF);
constexpr SkColor kTileGreen = SkColorSetRGB(0x30, 0xC1, 0x59);
// myArc brand colors (design.md): ivory name and quiet grays. The card is
// frosted glass tinted by the ribbon's blue, violet, pink and orange.
constexpr SkColor kBrandName = SkColorSetRGB(0xF5, 0xF2, 0xEC);
constexpr SkColor kBrandQuiet = SkColorSetRGB(0xB1, 0xBC, 0xC9);
constexpr SkColor kBrandFaint = SkColorSetRGB(0x7D, 0x8A, 0x99);

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

// The myArc card: a faint white pane that catches light along its top edge,
// tinted across its width by soft glows in the ribbon's colors.
class GlassCard : public views::View {
  METADATA_HEADER(GlassCard, views::View)

 public:
  GlassCard() = default;

  void OnPaintBackground(gfx::Canvas* canvas) override {
    struct Glow {
      float x, y, radius;
      SkColor4f color;
    };
    // Centers and radii in DIPs from the card's top left. A negative x
    // counts from the right edge, so the button side keeps its tint at any
    // width.
    static constexpr Glow kGlows[] = {
        {30, 44, 150, {0.20f, 0.42f, 1.00f, 0.30f}},
        {100, 8, 140, {0.55f, 0.32f, 1.00f, 0.20f}},
        {150, 70, 140, {1.00f, 0.30f, 0.62f, 0.14f}},
        {230, 84, 130, {1.00f, 0.58f, 0.22f, 0.08f}},
        {-150, 10, 210, {0.55f, 0.32f, 1.00f, 0.14f}},
        {-40, 66, 170, {0.20f, 0.42f, 1.00f, 0.15f}},
    };

    gfx::ScopedCanvas scoped(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();
    cc::PaintCanvas* paint = canvas->sk_canvas();
    gfx::RectF rect(GetLocalBounds());
    rect.Scale(scale);
    const float radius = kBrandRadius * scale;
    const SkRect sk_rect =
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
    paint->clipRRect(SkRRect::MakeRectXY(sk_rect, radius, radius), true);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(SK_ColorWHITE, 0x0B));
    paint->drawRect(sk_rect, flags);
    // Shaders below carry their own alpha; the paint's alpha would scale it.
    flags.setColor(SK_ColorBLACK);

    for (const Glow& glow : kGlows) {
      SkColor4f clear = glow.color;
      clear.fA = 0;
      const SkColor4f colors[] = {glow.color, clear};
      flags.setShader(cc::PaintShader::MakeRadialGradient(
          SkPoint::Make(
              (glow.x < 0 ? rect.right() : rect.x()) + glow.x * scale,
              rect.y() + glow.y * scale),
          glow.radius * scale, colors, nullptr, 2, SkTileMode::kClamp));
      paint->drawRect(sk_rect, flags);
    }

    // A sheen over the top half, brightest at the edge.
    const SkPoint sheen[] = {{0, rect.y()}, {0, rect.y() + rect.height() / 2}};
    const SkColor4f sheen_colors[] = {{1, 1, 1, 0.07f}, {1, 1, 1, 0}};
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        sheen, sheen_colors, nullptr, 2, SkTileMode::kClamp));
    paint->drawRect(sk_rect, flags);

    // A hairline that is bright along the top and fades down the sides.
    const SkPoint edge[] = {{0, rect.y()}, {0, rect.bottom()}};
    const SkColor4f edge_colors[] = {{1, 1, 1, 0.26f}, {1, 1, 1, 0.07f}};
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        edge, edge_colors, nullptr, 2, SkTileMode::kClamp));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    paint->drawRRect(SkRRect::MakeRectXY(sk_rect.makeInset(0.5f, 0.5f),
                                         radius - 0.5f, radius - 0.5f),
                     flags);
  }
};

BEGIN_METADATA(GlassCard)
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

    brand_ = AddChildView(std::make_unique<GlassCard>());
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
        std::move(open_site), u"Visit myarc.dev", kBlueButton, SK_ColorWHITE,
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
  raw_ptr<views::View> brand_ = nullptr;
  base::WeakPtrFactory<GeneralPage> weak_factory_{this};
};

BEGIN_METADATA(GeneralPage)
END_METADATA

// Advanced: the way out to Chromium's full settings page, Import from Arc,
// Replay welcome tour, then the Browser control switch that lets tools such as
// Claude Code drive the Agent Space through myarc.
class AdvancedPage : public views::View {
  METADATA_HEADER(AdvancedPage, views::View)

 public:
  AdvancedPage(PrefService* local_state,
               base::RepeatingClosure more_settings,
               base::RepeatingClosure import_arc,
               base::RepeatingClosure replay_tour)
      : local_state_(local_state) {
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

    import_panel_ = AddRowPanel(
        this, u"Spaces, folders and pinned tabs from an Arc export",
        /*multi_line=*/false,
        std::make_unique<PillButton>(std::move(import_arc),
                                     u"Import from Arc…", kGrayButton,
                                     kButtonText, gfx::Font::Weight::NORMAL,
                                     nullptr));
    static_cast<views::BoxLayout*>(import_panel_->GetLayoutManager())
        ->set_inside_border_insets(gfx::Insets::VH(0, kBannerPad));

    replay_panel_ = AddRowPanel(
        this, u"The welcome and the three practice steps",
        /*multi_line=*/false,
        std::make_unique<PillButton>(
            std::move(replay_tour), u"Replay welcome tour", kGrayButton,
            kButtonText, gfx::Font::Weight::NORMAL, nullptr));
    static_cast<views::BoxLayout*>(replay_panel_->GetLayoutManager())
        ->set_inside_border_insets(gfx::Insets::VH(0, kBannerPad));

    control_title_ = AddLabel(this, u"Browser control", 13,
                              gfx::Font::Weight::SEMIBOLD, kCardText);
    control_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    auto control_switch = std::make_unique<Switch>(
        base::BindRepeating(&AdvancedPage::ToggleControl,
                            base::Unretained(this)),
        u"Allow agents to control myArc");
    control_switch_ = control_switch.get();
    control_panel_ =
        AddRowPanel(this, u"Allow agents to control myArc",
                    /*multi_line=*/false, std::move(control_switch));
    // Line the switch row up with the Chromium Settings card above it.
    static_cast<views::BoxLayout*>(control_panel_->GetLayoutManager())
        ->set_inside_border_insets(gfx::Insets::VH(0, kBannerPad));
    control_help_ = AddLabel(
        this,
        u"Lets tools like Claude Code use the myarc command to open and "
        u"use tabs in your Agent Space. You approve each tool the first time "
        u"it connects.",
        12, gfx::Font::Weight::NORMAL, kTabText);
    control_help_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    control_help_->SetMultiLine(true);
    if (local_state_) {
      registrar_.Init(local_state_);
      registrar_.Add(kControlEnabledPref,
                     base::BindRepeating(&AdvancedPage::UpdateControl,
                                         base::Unretained(this)));
    }
    UpdateControl();
  }

  void Layout(PassKey) override {
    const int panel_width = width() - 2 * kSideMargin;
    card_->SetBounds(kSideMargin, kContentTop, panel_width, kRowHeight);
    import_panel_->SetBounds(kSideMargin, card_->bounds().bottom() + 8,
                             panel_width, kRowHeight);
    replay_panel_->SetBounds(kSideMargin, import_panel_->bounds().bottom() + 8,
                             panel_width, kRowHeight);
    const int title_height = control_title_->GetPreferredSize().height();
    control_title_->SetBounds(kSideMargin + kBannerPad,
                              replay_panel_->bounds().bottom() + 24,
                              panel_width - kBannerPad, title_height);
    control_panel_->SetBounds(kSideMargin, control_title_->bounds().bottom() + 8,
                              panel_width, kSwitchRowHeight);
    const int help_width = panel_width - 2 * kBannerPad;
    control_help_->SetBounds(kSideMargin + kBannerPad,
                             control_panel_->bounds().bottom() + 8, help_width,
                             control_help_->GetHeightForWidth(help_width));
  }

 private:
  void UpdateControl() {
    control_switch_->SetOn(IsControlEnabled(local_state_));
    // --myarc-enable-control keeps it on for this run.
    control_switch_->SetEnabled(local_state_ && !IsControlForcedOn());
  }

  void ToggleControl() {
    if (!local_state_ || IsControlForcedOn()) {
      return;
    }
    local_state_->SetBoolean(kControlEnabledPref,
                             !local_state_->GetBoolean(kControlEnabledPref));
    UpdateControl();
  }

  const raw_ptr<PrefService> local_state_;
  PrefChangeRegistrar registrar_;
  raw_ptr<Panel> card_ = nullptr;
  raw_ptr<Panel> import_panel_ = nullptr;
  raw_ptr<Panel> replay_panel_ = nullptr;
  raw_ptr<views::Label> control_title_ = nullptr;
  raw_ptr<Panel> control_panel_ = nullptr;
  raw_ptr<Switch> control_switch_ = nullptr;
  raw_ptr<views::Label> control_help_ = nullptr;
};

BEGIN_METADATA(AdvancedPage)
END_METADATA

// One row of Arc's grouped switch card: a coloured tile with a white symbol,
// a label with an optional grey line under it, and a switch at the right.
class SwitchRow : public views::View {
  METADATA_HEADER(SwitchRow, views::View)

 public:
  SwitchRow(const gfx::VectorIcon& icon,
            SkColor tile,
            std::u16string label,
            std::u16string detail,
            views::Button::PressedCallback toggle)
      : tile_(tile) {
    icon_ = gfx::CreateVectorIcon(icon, kTileIcon, SK_ColorWHITE);
    label_ = AddLabel(this, label, 13, gfx::Font::Weight::NORMAL, kCardText);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!detail.empty()) {
      detail_ = AddLabel(this, detail, 11, gfx::Font::Weight::NORMAL, kTabText);
      detail_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
    switch_ = AddChildView(std::make_unique<Switch>(std::move(toggle), label));
  }

  Switch* toggle() { return switch_; }
  int RowHeight() const {
    return detail_ ? kGroupTallRowHeight : kGroupRowHeight;
  }

  void Layout(PassKey) override {
    const gfx::Size size = switch_->GetPreferredSize();
    switch_->SetBounds(width() - kGroupPad - size.width(),
                       (height() - size.height()) / 2, size.width(),
                       size.height());
    const int x = kGroupPad + kTileSize + kGroupGap;
    const int text_width = std::max(0, switch_->x() - kGroupGap - x);
    const int label_height = label_->GetPreferredSize().height();
    if (!detail_) {
      label_->SetBounds(x, (height() - label_height) / 2, text_width,
                        label_height);
      return;
    }
    const int detail_height = detail_->GetPreferredSize().height();
    const int top = (height() - label_height - detail_height) / 2;
    label_->SetBounds(x, top, text_width, label_height);
    detail_->SetBounds(x, top + label_height, text_width, detail_height);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::Rect tile(kGroupPad, (height() - kTileSize) / 2, kTileSize,
                         kTileSize);
    PaintPanel(canvas, tile, kTileRadius, tile_, std::nullopt);
    canvas->DrawImageInt(icon_, tile.x() + (kTileSize - icon_.width()) / 2,
                         tile.y() + (kTileSize - icon_.height()) / 2);
  }

 private:
  const SkColor tile_;
  gfx::ImageSkia icon_;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::Label> detail_ = nullptr;
  raw_ptr<Switch> switch_ = nullptr;
};

BEGIN_METADATA(SwitchRow)
END_METADATA

// Arc's grouped card: rows stacked in one rounded panel, a hairline between
// each pair from the rows' tile edge to their right padding.
class SwitchGroup : public Panel {
  METADATA_HEADER(SwitchGroup, Panel)

 public:
  SwitchGroup() : Panel(kCardFill, kCardStroke, kCardRadius) {}

  SwitchRow* AddRow(std::unique_ptr<SwitchRow> row) {
    rows_.push_back(AddChildView(std::move(row)));
    return rows_.back();
  }

  int GroupHeight() const {
    int height = 0;
    for (const SwitchRow* row : rows_) {
      height += row->RowHeight();
    }
    return height;
  }

  void Layout(PassKey) override {
    int y = 0;
    for (SwitchRow* row : rows_) {
      row->SetBounds(0, y, width(), row->RowHeight());
      y += row->RowHeight();
    }
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    Panel::OnPaintBackground(canvas);
    for (size_t i = 0; i + 1 < rows_.size(); ++i) {
      canvas->FillRect(gfx::Rect(kGroupPad, rows_[i]->bounds().bottom(),
                                 width() - 2 * kGroupPad, 1),
                       kCardStroke);
    }
  }

 private:
  std::vector<raw_ptr<SwitchRow>> rows_;
};

BEGIN_METADATA(SwitchGroup)
END_METADATA

// Links: Arc's Little Arc and Peek switches. One decides whether links other
// apps open go to Little Arc or to a main window as tabs, the other whether
// links from pinned tabs to other sites open in Peek or in a new tab.
class LinksPage : public views::View {
  METADATA_HEADER(LinksPage, views::View)

 public:
  explicit LinksPage(PrefService* local_state) : local_state_(local_state) {
    intro_ = AddLabel(
        this,
        u"Little Arc is a smaller, simpler window for quick lookups and for "
        u"links from other apps. Press ⌥⌘N to open one, and ⌘O to move its "
        u"page into a Space.",
        13, gfx::Font::Weight::NORMAL, kCardText);
    intro_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    intro_->SetMultiLine(true);
    group_ = AddChildView(std::make_unique<SwitchGroup>());
    little_arc_ = group_->AddRow(std::make_unique<SwitchRow>(
        kAppWindowIcon, kTileGreen, u"Links from other apps open in Little Arc",
        std::u16string(),
        base::BindRepeating(&LinksPage::ToggleLittleArc,
                            base::Unretained(this))));
    // Arc's own Links switch for Peek, with its wording.
    peek_ = group_->AddRow(std::make_unique<SwitchRow>(
        kPeekRowIcon, kTileBlue,
        u"Open a Peek window when clicking on links to other sites",
        u"Links in pinned tabs and Favorites. Off, they open in a new tab.",
        base::BindRepeating(&LinksPage::TogglePeek, base::Unretained(this))));
    if (local_state_) {
      registrar_.Init(local_state_);
      const auto update = base::BindRepeating(&LinksPage::UpdateSwitches,
                                              base::Unretained(this));
      registrar_.Add(kLittleArcLinksPref, update);
      registrar_.Add(kPeekEnabledPref, update);
    }
    UpdateSwitches();
  }

  void Layout(PassKey) override {
    const int panel_width = width() - 2 * kSideMargin;
    const int text_width = panel_width - 2 * kBannerPad;
    intro_->SetBounds(kSideMargin + kBannerPad, kContentTop, text_width,
                      intro_->GetHeightForWidth(text_width));
    group_->SetBounds(kSideMargin, intro_->bounds().bottom() + 16, panel_width,
                      group_->GroupHeight());
  }

 private:
  void UpdateSwitches() {
    little_arc_->toggle()->SetOn(IsLittleArcLinksEnabled(local_state_));
    peek_->toggle()->SetOn(IsPeekEnabled(local_state_));
    little_arc_->toggle()->SetEnabled(local_state_ != nullptr);
    peek_->toggle()->SetEnabled(local_state_ != nullptr);
  }

  void ToggleLittleArc() {
    if (local_state_) {
      local_state_->SetBoolean(kLittleArcLinksPref,
                               !IsLittleArcLinksEnabled(local_state_));
      UpdateSwitches();
    }
  }

  void TogglePeek() {
    if (local_state_) {
      local_state_->SetBoolean(kPeekEnabledPref, !IsPeekEnabled(local_state_));
      UpdateSwitches();
    }
  }

  const raw_ptr<PrefService> local_state_;
  PrefChangeRegistrar registrar_;
  raw_ptr<views::Label> intro_ = nullptr;
  raw_ptr<SwitchGroup> group_ = nullptr;
  raw_ptr<SwitchRow> little_arc_ = nullptr;
  raw_ptr<SwitchRow> peek_ = nullptr;
};

BEGIN_METADATA(LinksPage)
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

// One Profile in the Profiles page's list: its icon, its name and how many
// Spaces use it.
class ProfileListRow : public views::Button {
  METADATA_HEADER(ProfileListRow, views::Button)

 public:
  ProfileListRow(PressedCallback callback,
                 const SettingsWindow::ProfileRow& row,
                 bool selected)
      : views::Button(std::move(callback)), selected_(selected) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::VH(0, kListRowPad), kListRowGap));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    auto* icon = AddChildView(std::make_unique<views::ImageView>(row.icon));
    icon->SetImageSize(gfx::Size(kListIconSize, kListIconSize));
    icon->SetPreferredSize(gfx::Size(kListIconSize, kListIconSize));
    auto* name =
        AddLabel(this, row.name, 13, gfx::Font::Weight::NORMAL, kButtonText);
    name->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    name->SetElideBehavior(gfx::ELIDE_TAIL);
    layout->SetFlexForView(name, 1);
    const std::u16string used = SpacesCount(row.in_use ? row.spaces.size() : 0);
    AddLabel(this, used, 12, gfx::Font::Weight::NORMAL, kTabText);
    SetAccessibleName(row.name + u", " + used);
    GetViewAccessibility().SetRole(ax::mojom::Role::kListBoxOption);
    GetViewAccessibility().SetIsSelected(selected);
  }

  static std::u16string SpacesCount(size_t spaces) {
    if (!spaces) {
      return u"Not used";
    }
    return base::NumberToString16(spaces) +
           (spaces == 1 ? u" Space" : u" Spaces");
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(
        views::Button::CalculatePreferredSize(available_size).width(),
        kListRowHeight);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    if (selected_) {
      PaintPanel(canvas, GetLocalBounds(), kButtonRadius, kListSelectedFill,
                 std::nullopt);
    }
  }

 private:
  const bool selected_;
};

BEGIN_METADATA(ProfileListRow)
END_METADATA

// Profiles: Arc's tab. The list of Profiles with their icons and how many
// Spaces use each, with + to create one and - to delete the selected one.
// On the right, the selected Profile's name, icon, "Archive tabs after" and
// the Spaces that use it.
class ProfilesPage : public views::View,
                     public ui::SimpleMenuModel::Delegate,
                     public views::TextfieldController {
  METADATA_HEADER(ProfilesPage, views::View)

 public:
  explicit ProfilesPage(SettingsWindow::Profiles profiles)
      : profiles_(std::move(profiles)) {
    intro_ = AddLabel(this,
                      u"Profiles keep history, logins, cookies and extensions "
                      u"apart. Several Spaces can share one Profile.",
                      13, gfx::Font::Weight::NORMAL, kCardText);
    intro_->SetMultiLine(true);
    intro_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    list_panel_ = AddChildView(
        std::make_unique<Panel>(kCardFill, kCardStroke, kCardRadius));
    list_title_ = AddLabel(list_panel_, u"Your Profiles", 12,
                           gfx::Font::Weight::MEDIUM, kTabText);
    list_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    rows_ = list_panel_->AddChildView(std::make_unique<views::View>());
    rows_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
    rows_->GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
    rows_->GetViewAccessibility().SetName(u"Your Profiles");
    add_ = list_panel_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&ProfilesPage::Create, base::Unretained(this)), u"",
        kCardFill, kButtonText, gfx::Font::Weight::NORMAL, &kPlusIcon));
    add_->SetAccessibleName(u"New Profile");
    add_->SetTooltipText(u"New Profile");
    remove_ = list_panel_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&ProfilesPage::AskRemove, base::Unretained(this)),
        u"", kCardFill, kButtonText, gfx::Font::Weight::NORMAL, &kMinusIcon));
    remove_->SetAccessibleName(u"Delete Profile");
    remove_->SetTooltipText(u"Delete Profile");

    detail_ = AddChildView(
        std::make_unique<Panel>(kCardFill, kCardStroke, kCardRadius));
    name_title_ =
        AddLabel(detail_, u"Name", 13, gfx::Font::Weight::NORMAL, kCardText);
    name_ = detail_->AddChildView(std::make_unique<views::Textfield>());
    name_->set_controller(this);
    name_->SetBackgroundColor(kWindowFill);
    name_->SetColor(kButtonText);
    name_->SetFontList(Font(13, gfx::Font::Weight::NORMAL));
    name_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 6)));
    name_->GetViewAccessibility().SetName(u"Profile name");
    icon_title_ =
        AddLabel(detail_, u"Icon", 13, gfx::Font::Weight::NORMAL, kCardText);
    icon_ = detail_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&ProfilesPage::OpenIconMenu,
                            base::Unretained(this)),
        u"Change…", kGrayButton, kButtonText, gfx::Font::Weight::NORMAL,
        nullptr));
    icon_->GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
    icon_->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
    icon_image_ =
        icon_->AddChildViewAt(std::make_unique<views::ImageView>(), 0);
    icon_image_->SetImageSize(gfx::Size(kListIconSize, kListIconSize));
    archive_title_ = AddLabel(detail_, u"Archive tabs after", 13,
                              gfx::Font::Weight::NORMAL, kCardText);
    archive_title_->SetTooltipText(
        u"Unpinned tabs you haven’t viewed for this long move to "
        u"Archived Tabs.");
    archive_ = detail_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&ProfilesPage::OpenArchiveMenu,
                            base::Unretained(this)),
        u"", kGrayButton, kButtonText, gfx::Font::Weight::NORMAL, nullptr));
    archive_->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(kChevronsUpDownIcon, kButtonText, 12)));
    archive_->GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
    archive_->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
    spaces_title_ =
        AddLabel(this, u"Used by", 12, gfx::Font::Weight::MEDIUM, kTabText);
    spaces_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    spaces_ = AddLabel(this, u"", 13, gfx::Font::Weight::NORMAL, kCardText);
    spaces_->SetMultiLine(true);
    spaces_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    // Delete's confirmation, and any reason an action was refused.
    status_ = AddLabel(this, u"", 12, gfx::Font::Weight::NORMAL, kCardText);
    status_->SetMultiLine(true);
    status_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    status_->GetViewAccessibility().SetRole(ax::mojom::Role::kStatus);
    cancel_ = AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&ProfilesPage::CancelRemove,
                            base::Unretained(this)),
        u"Cancel", kGrayButton, kButtonText, gfx::Font::Weight::NORMAL,
        nullptr));
    confirm_ = AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&ProfilesPage::Remove, base::Unretained(this)),
        u"Delete Profile", kDeleteButton, SK_ColorWHITE,
        gfx::Font::Weight::NORMAL, nullptr));
    Refresh();
#if BUILDFLAG(MYARC_TEST_HOOKS)
    ScheduleTestSteps();
#endif
  }

  void Refresh() {
    rows_data_ = profiles_.list.Run();
    if (std::ranges::none_of(rows_data_, [this](const auto& row) {
          return row.id == selected_;
        })) {
      selected_.clear();
      for (const auto& row : rows_data_) {
        if (selected_.empty() || row.current) {
          selected_ = row.id;
        }
      }
    }
    rows_->RemoveAllChildViews();
    for (const auto& row : rows_data_) {
      // Selecting rebuilds the rows, so it runs after the press unwinds.
      rows_->AddChildView(std::make_unique<ProfileListRow>(
          base::BindRepeating(
              [](base::WeakPtr<ProfilesPage> page, std::string id) {
                base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                    FROM_HERE, base::BindOnce(&ProfilesPage::Select,
                                              std::move(page), std::move(id)));
              },
              weak_factory_.GetWeakPtr(), row.id),
          row, row.id == selected_));
    }
    const SettingsWindow::ProfileRow* row = Selected();
    detail_->SetVisible(row);
    spaces_title_->SetVisible(row);
    spaces_->SetVisible(row);
    remove_->SetEnabled(row && rows_data_.size() > 1);
    if (row) {
      // A name being typed stays, unless another Profile was selected.
      if (!name_->HasFocus() || named_ != row->id) {
        name_->SetText(row->name);
        named_ = row->id;
      }
      icon_image_->SetImage(row->icon);
      icon_->SetAccessibleName(u"Change icon of " + row->name);
      std::u16string used;
      for (const std::u16string& space : row->spaces) {
        used += (used.empty() ? u"" : u", ") + space;
      }
      spaces_->SetText(!row->in_use || used.empty()
                           ? u"No Spaces use this Profile yet."
                           : used);
      secs_ = profiles_.get_archive.Run(row->id);
    } else {
      secs_ = 0;
    }
    std::u16string text = u"Unavailable";
    for (const ArchiveChoice& choice : kArchiveChoices) {
      if (choice.secs == secs_) {
        text = choice.label;
      }
    }
    archive_->SetText(text);
    archive_->SetAccessibleName(u"Archive tabs after: " + text);
    archive_->SetEnabled(secs_ != 0);
    archive_->SetTooltipText(
        secs_ ? std::u16string()
              : u"Open a window of this Profile to change its timer.");
    if (!confirming_) {
      cancel_->SetVisible(false);
      confirm_->SetVisible(false);
    }
    InvalidateLayout();
    SchedulePaint();
  }

  void Layout(PassKey) override {
    const int content = width() - 2 * kSideMargin;
    intro_->SetMaximumWidth(content);
    intro_->SetBounds(kSideMargin, kContentTop, content,
                      intro_->GetHeightForWidth(content));
    const int top = intro_->bounds().bottom() + 16;
    const int bottom = height() - kSideMargin;
    list_panel_->SetBounds(kSideMargin, top, kListWidth, bottom - top);
    list_title_->SetBounds(kListRowPad, 8, kListWidth - 2 * kListRowPad, 20);
    const int footer = kButtonHeight + 12;
    rows_->SetBounds(6, 34, kListWidth - 12,
                     list_panel_->height() - 34 - footer);
    add_->SetBounds(8, list_panel_->height() - footer + 6, kButtonHeight + 6,
                    kButtonHeight);
    remove_->SetBounds(add_->bounds().right() + 4, add_->y(), kButtonHeight + 6,
                       kButtonHeight);

    const int x = list_panel_->bounds().right() + 14;
    const int w = width() - kSideMargin - x;
    detail_->SetBounds(x, top, w, 3 * kRowHeight);
    const int label_w = 140;
    const int field_x = kBannerPad + label_w;
    const int field_w = w - field_x - kBannerPad;
    const auto row_y = [](int i, int h) {
      return i * kRowHeight + (kRowHeight - h) / 2;
    };
    name_title_->SetBounds(kBannerPad, row_y(0, 20), label_w, 20);
    name_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    name_->SetBounds(field_x, row_y(0, kButtonHeight + 4), field_w,
                     kButtonHeight + 4);
    icon_title_->SetBounds(kBannerPad, row_y(1, 20), label_w, 20);
    icon_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    const int icon_w = icon_->GetPreferredSize().width();
    icon_->SetBounds(w - kBannerPad - icon_w, row_y(1, kButtonHeight), icon_w,
                     kButtonHeight);
    archive_title_->SetBounds(kBannerPad, row_y(2, 20), label_w, 20);
    archive_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    const int archive_w = archive_->GetPreferredSize().width();
    archive_->SetBounds(w - kBannerPad - archive_w, row_y(2, kButtonHeight),
                        archive_w, kButtonHeight);

    int y = detail_->bounds().bottom() + 16;
    spaces_title_->SetBounds(x + 2, y, w, 18);
    y += 20;
    spaces_->SetMaximumWidth(w);
    spaces_->SetBounds(x + 2, y, w, spaces_->GetHeightForWidth(w));
    y = spaces_->bounds().bottom() + 18;
    status_->SetMaximumWidth(w);
    status_->SetBounds(x + 2, y, w, status_->GetHeightForWidth(w));
    y = status_->bounds().bottom() + 8;
    const int confirm_w = confirm_->GetPreferredSize().width();
    const int cancel_w = cancel_->GetPreferredSize().width();
    confirm_->SetBounds(x + w - confirm_w, y, confirm_w, kButtonHeight);
    cancel_->SetBounds(confirm_->x() - 8 - cancel_w, y, cancel_w,
                       kButtonHeight);
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    return command_id >= kArchiveCommand &&
           kArchiveChoices[command_id - kArchiveCommand].secs == secs_;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    const SettingsWindow::ProfileRow* row = Selected();
    if (!row) {
      return;
    }
    if (command_id >= kArchiveCommand) {
      profiles_.set_archive.Run(
          row->id, kArchiveChoices[command_id - kArchiveCommand].secs);
      Refresh();
      return;
    }
    Show(profiles_.set_icon.Run(row->id, static_cast<size_t>(command_id)));
  }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::EventType::kKeyPressed) {
      return false;
    }
    if (key_event.key_code() == ui::VKEY_RETURN) {
      CommitName();
      return true;
    }
    if (key_event.key_code() == ui::VKEY_ESCAPE) {
      if (const SettingsWindow::ProfileRow* row = Selected()) {
        name_->SetText(row->name);
      }
      return true;
    }
    return false;
  }

 private:
  static constexpr int kArchiveCommand = 1000;

  const SettingsWindow::ProfileRow* Selected() const {
    for (const auto& row : rows_data_) {
      if (row.id == selected_) {
        return &row;
      }
    }
    return nullptr;
  }

  // By value: Refresh replaces the rows `id` may come from.
  void Select(std::string id) {
    CommitName();
    selected_ = std::move(id);
    confirming_ = false;
    status_->SetText(u"");
    Refresh();
  }

  // Shows `error`, or refreshes the page once an action is done.
  void Show(const std::u16string& error) {
    confirming_ = false;
    status_->SetText(error);
    status_->SetEnabledColor(error.empty() ? kCardText : kErrorText);
    Refresh();
  }

  void CommitName() {
    const SettingsWindow::ProfileRow* row = Selected();
    if (!row || name_->GetText() == row->name) {
      return;
    }
    Show(profiles_.rename.Run(row->id, std::u16string(name_->GetText())));
  }

  void Create() {
    if (creating_) {
      return;
    }
    // "Profile 2", or the next number no Profile uses.
    std::u16string name;
    for (size_t n = rows_data_.size() + 1;; ++n) {
      name = u"Profile " + base::NumberToString16(n);
      if (std::ranges::none_of(rows_data_, [&name](const auto& row) {
            return row.name == name;
          })) {
        break;
      }
    }
    creating_ = true;
    add_->SetEnabled(false);
    status_->SetText(u"Creating " + name + u"…");
    profiles_.create.Run(
        name, base::BindOnce(&ProfilesPage::Created, weak_factory_.GetWeakPtr(),
                             name));
  }

  void Created(const std::u16string& name, const std::u16string& error) {
    creating_ = false;
    add_->SetEnabled(true);
    if (error.empty()) {
      for (const auto& row : profiles_.list.Run()) {
        if (row.name == name) {
          selected_ = row.id;
        }
      }
    }
    Show(error);
    if (error.empty()) {
      name_->RequestFocus();
      name_->SelectAll(false);
    }
  }

  void AskRemove() {
    const SettingsWindow::ProfileRow* row = Selected();
    if (!row) {
      return;
    }
    // The same refusals Delete gives, before asking.
    std::u16string refusal;
    if (rows_data_.size() < 2) {
      refusal = u"myArc needs at least one Profile, so the last one stays.";
    } else if (row->in_use) {
      refusal = row->name +
                u" can’t be deleted while Spaces use it. Delete those "
                u"Spaces, or move them to another Profile, first.";
    }
    if (!refusal.empty()) {
      Show(refusal);
      return;
    }
    confirming_ = true;
    status_->SetEnabledColor(kCardText);
    status_->SetText(u"Delete " + row->name +
                     u"? Its history, logins, cookies and extensions are "
                     u"removed from this Mac.");
    cancel_->SetVisible(true);
    confirm_->SetVisible(true);
    InvalidateLayout();
  }

  void CancelRemove() { Show(u""); }

  void Remove() {
    const SettingsWindow::ProfileRow* row = Selected();
    if (!row || !confirming_) {
      return;
    }
    const std::string id = row->id;
    Show(profiles_.remove.Run(id));
  }

  void OpenIconMenu() {
    if (!GetWidget() || (runner_ && runner_->IsRunning()) || !Selected()) {
      return;
    }
    runner_.reset();
    model_ = std::make_unique<ui::SimpleMenuModel>(this);
    const auto icons = profiles_.icons.Run();
    for (size_t i = 0; i < icons.size(); ++i) {
      model_->AddItemWithIcon(static_cast<int>(i), icons[i].second,
                              icons[i].first);
    }
    RunMenu(icon_);
  }

  void OpenArchiveMenu() {
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
      model_->AddRadioItem(kArchiveCommand + static_cast<int>(i),
                           kArchiveChoices[i].label, /*group_id=*/0);
    }
    RunMenu(archive_);
  }

  void RunMenu(views::View* anchor) {
    runner_ = std::make_unique<views::MenuRunner>(model_.get(),
                                                  views::MenuRunner::NO_FLAGS);
    runner_->RunMenuAt(GetWidget(), nullptr, anchor->GetBoundsInScreen(),
                       views::MenuAnchorPosition::kTopLeft,
                       ui::mojom::MenuSourceType::kNone);
  }

#if BUILDFLAG(MYARC_TEST_HOOKS)
  // --myarc-test-settings-profiles=<step>[,<step>…] runs one step every two
  // seconds and logs the page after each: "select:<n>" selects the nth
  // Profile, "create" presses +, "rename:<name>" types a name and Return,
  // "icon:<n>" picks the nth icon, "delete" presses - and "confirm" presses
  // Delete Profile. For background captures.
  void ScheduleTestSteps() {
    const std::string steps =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            "myarc-test-settings-profiles");
    int delay = 0;
    for (const std::string& step : base::SplitString(
             steps, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      delay += 2;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ProfilesPage::RunTestStep, weak_factory_.GetWeakPtr(),
                         step),
          base::Seconds(delay));
    }
  }

  void RunTestStep(const std::string& step) {
    const auto [verb, value] = [&step] {
      const size_t colon = step.find(':');
      return std::pair(step.substr(0, colon), colon == std::string::npos
                                                  ? std::string()
                                                  : step.substr(colon + 1));
    }();
    size_t index = 0;
    if (verb == "select" && base::StringToSizeT(value, &index) &&
        index < rows_data_.size()) {
      Select(rows_data_[index].id);
    } else if (verb == "create") {
      Create();
    } else if (verb == "rename") {
      name_->SetText(base::UTF8ToUTF16(value));
      CommitName();
    } else if (verb == "icon" && base::StringToSizeT(value, &index)) {
      ExecuteCommand(static_cast<int>(index), 0);
    } else if (verb == "delete") {
      AskRemove();
    } else if (verb == "confirm") {
      Remove();
    }
    std::string rows;
    for (const auto& row : rows_data_) {
      rows += " [" + row.id + " \"" + base::UTF16ToUTF8(row.name) + "\" " +
              base::NumberToString(row.spaces.size()) + " spaces" +
              (row.in_use ? "" : " unused") +
              (row.id == selected_ ? " selected" : "") + "]";
    }
    LOG(WARNING) << "Test settings profiles: " << step << " ->" << rows
                 << " status=\"" << base::UTF16ToUTF8(status_->GetText())
                 << "\"";
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)

  SettingsWindow::Profiles profiles_;
  std::vector<SettingsWindow::ProfileRow> rows_data_;
  std::string selected_;
  // The Profile whose name the field shows.
  std::string named_;
  uint64_t secs_ = 0;
  bool confirming_ = false;
  bool creating_ = false;
  raw_ptr<views::Label> intro_ = nullptr;
  raw_ptr<Panel> list_panel_ = nullptr;
  raw_ptr<views::Label> list_title_ = nullptr;
  raw_ptr<views::View> rows_ = nullptr;
  raw_ptr<PillButton> add_ = nullptr;
  raw_ptr<PillButton> remove_ = nullptr;
  raw_ptr<Panel> detail_ = nullptr;
  raw_ptr<views::Label> name_title_ = nullptr;
  raw_ptr<views::Textfield> name_ = nullptr;
  raw_ptr<views::Label> icon_title_ = nullptr;
  raw_ptr<PillButton> icon_ = nullptr;
  raw_ptr<views::ImageView> icon_image_ = nullptr;
  raw_ptr<views::Label> archive_title_ = nullptr;
  raw_ptr<PillButton> archive_ = nullptr;
  raw_ptr<views::Label> spaces_title_ = nullptr;
  raw_ptr<views::Label> spaces_ = nullptr;
  raw_ptr<views::Label> status_ = nullptr;
  raw_ptr<PillButton> cancel_ = nullptr;
  raw_ptr<PillButton> confirm_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> model_;
  std::unique_ptr<views::MenuRunner> runner_;
  base::WeakPtrFactory<ProfilesPage> weak_factory_{this};
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
// Icon (no app icon picker). Shortcuts stays visible but disabled so the row
// keeps Arc's rhythm.
constexpr std::array<SectionSpec, 5> kSections = {{
    {u"General", &kSettingsIcon, SettingsWindow::Section::kGeneral},
    {u"Profiles", &kUserIcon, SettingsWindow::Section::kProfiles},
    {u"Links", &kAppWindowIcon, SettingsWindow::Section::kLinks},
    {u"Shortcuts", &kKeyboardIcon, std::nullopt},
    {u"Advanced", &kSlidersIcon, SettingsWindow::Section::kAdvanced},
}};

class SettingsView : public views::View {
  METADATA_HEADER(SettingsView, views::View)

 public:
  SettingsView(PrefService* local_state,
               base::RepeatingClosure more_settings,
               base::RepeatingClosure open_site,
               base::RepeatingClosure import_arc,
               base::RepeatingClosure replay_tour,
               SettingsWindow::Profiles profiles,
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
        AddChildView(std::make_unique<ProfilesPage>(std::move(profiles)));
    links_ = AddChildView(std::make_unique<LinksPage>(local_state));
    advanced_ = AddChildView(std::make_unique<AdvancedPage>(
        local_state, more_settings, std::move(import_arc),
        std::move(replay_tour)));
    // The pages fill the whole view, so the tabs go above them or the shown
    // page takes their clicks.
    for (SectionTab* tab : tabs_) {
      ReorderChildView(tab, children().size());
    }
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
    AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_PLATFORM_ACCELERATOR));
    GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  }

  void Select(SettingsWindow::Section section) {
    general_->SetVisible(section == SettingsWindow::Section::kGeneral);
    profiles_->SetVisible(section == SettingsWindow::Section::kProfiles);
    links_->SetVisible(section == SettingsWindow::Section::kLinks);
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
    links_->SetBoundsRect(GetLocalBounds());
    advanced_->SetBoundsRect(GetLocalBounds());
  }

 private:
  base::RepeatingClosure close_;
  raw_ptr<views::Label> title_ = nullptr;
  std::vector<raw_ptr<SectionTab>> tabs_;
  raw_ptr<GeneralPage> general_ = nullptr;
  raw_ptr<ProfilesPage> profiles_ = nullptr;
  raw_ptr<LinksPage> links_ = nullptr;
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

SettingsWindow::SettingsWindow(Profile* profile,
                               PrefService* local_state,
                               base::RepeatingClosure open_more_settings,
                               base::RepeatingClosure open_site,
                               base::RepeatingClosure replay_tour,
                               Profiles profiles)
    : profile_(profile),
      local_state_(local_state),
      open_more_settings_(std::move(open_more_settings)),
      open_site_(std::move(open_site)),
      replay_tour_(std::move(replay_tour)),
      profiles_(std::move(profiles)) {}

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
        base::BindRepeating(&SettingsWindow::ImportFromArc,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&SettingsWindow::ReplayTour,
                            weak_factory_.GetWeakPtr()),
        profiles_,
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

void SettingsWindow::PressReplayForCapture() {
  if (widget_) {
    ReplayTour();
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

void SettingsWindow::ReplayTour() {
  // The welcome shows in the owning browser window, so Settings gets out of
  // its way.
  replay_tour_.Run();
  Close();
}

void SettingsWindow::ImportFromArc() {
  // The picker and preview sit over Settings, which stays open.
  if (widget_) {
    myarc::ImportFromArc(*profile_, widget_->GetNativeWindow());
  }
}

}  // namespace myarc
