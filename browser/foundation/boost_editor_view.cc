#include "myarc/boost_editor_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "myarc/boosts_service.h"
#include "myarc/boosts_tab_helper.h"
#include "myarc/bridge.rs.h"
#include "myarc/command_ids.h"
#include "myarc/glass_mac.h"
#include "myarc/icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/slider.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;
using MainAxis = views::BoxLayout::MainAxisAlignment;
using CrossAxis = views::BoxLayout::CrossAxisAlignment;

// Floating white card palette matching Arc's Boost editor card.
constexpr int kCardWidth = 244;
constexpr int kCodeCardWidth = 380;
constexpr int kPadWidth = 220;
constexpr int kPadHeight = 152;
constexpr int kCardCornerRadius = 16;
constexpr SkColor kCardBg = SkColorSetRGB(0x1C, 0x1D, 0x24);
constexpr SkColor kCardGlassTint = SkColorSetARGB(0x52, 0x12, 0x13, 0x1A);
constexpr SkColor kPillBg = SkColorSetARGB(0x1E, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPillHoverBg = SkColorSetARGB(0x33, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPillBorder = SkColorSetARGB(0x28, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPillHoverBorder = SkColorSetARGB(0x44, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPillActiveBg = SK_ColorWHITE;
constexpr SkColor kPillActiveInk = SkColorSetRGB(0x14, 0x14, 0x18);
constexpr SkColor kInkPrimary = SkColorSetRGB(0xF5, 0xF5, 0xF7);
constexpr SkColor kInkSecondary = SkColorSetARGB(0xC4, 0xEB, 0xEB, 0xF5);
constexpr SkColor kBorderSubtle = SkColorSetARGB(0x26, 0xFF, 0xFF, 0xFF);
constexpr SkColor kCodeWellBg = SkColorSetARGB(0x9C, 0x0C, 0x0D, 0x12);
constexpr SkColor kCodeWellBorder = SkColorSetARGB(0x2C, 0xFF, 0xFF, 0xFF);
constexpr SkColor kCodeInk = SkColorSetRGB(0xE5, 0xE7, 0xEB);
constexpr SkColor kCodeComment = SkColorSetRGB(0x6B, 0x72, 0x80);
constexpr SkColor kCodeSelector = SkColorSetRGB(0x7D, 0xD3, 0xFC);
constexpr SkColor kCodeProperty = SkColorSetRGB(0xC4, 0xB5, 0xFD);
constexpr SkColor kCodeNumber = SkColorSetRGB(0xFD, 0xBA, 0x74);
constexpr SkColor kCodeHex = SkColorSetRGB(0xFD, 0xE0, 0x47);
constexpr SkColor kCodeString = SkColorSetRGB(0x86, 0xEF, 0xAC);
constexpr SkColor kCodeKeyword = SkColorSetRGB(0xF4, 0x72, 0xB6);
constexpr SkColor kCodePunct = SkColorSetRGB(0x9C, 0xA3, 0xAF);
constexpr SkColor kCaseUpperBg = SkColorSetRGB(0xE8, 0x9B, 0x1E);
constexpr SkColor kCaseLowerBg = SkColorSetRGB(0xE8, 0x56, 0x1E);
constexpr SkColor kCaseCapBg = SkColorSetRGB(0xDE, 0x1B, 0x1B);
constexpr SkColor kWarnBg = SkColorSetRGB(0xFF, 0xF3, 0xCD);
constexpr SkColor kWarnText = SkColorSetRGB(0x7A, 0x4F, 0x01);

constexpr std::array<uint16_t, 8> kTextScales = {100, 110, 125, 150,
                                                 175, 200, 75,  90};

struct FontPresetInfo {
  foundation::BoostFontPreset preset;
  const char* family;
  const char16_t* name;
  int style;
};

constexpr std::array<FontPresetInfo, 16> kFontPresets = {{
    {foundation::BoostFontPreset::Default, "", u"Default", gfx::Font::NORMAL},
    {foundation::BoostFontPreset::SystemSans, "Helvetica Neue", u"System Sans",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::SystemSerif, "Georgia", u"System Serif",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::SystemMono, "Menlo", u"System Mono",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Rounded, "Arial Rounded MT Bold", u"Rounded",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Geometric, "Avenir Next", u"Geometric",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Humanist, "Gill Sans", u"Humanist",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Grotesk, "Helvetica", u"Grotesk",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Didone, "Didot", u"Didone",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::SlabSerif, "Rockwell", u"Slab Serif",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::ClassicSerif, "Baskerville", u"Classic Serif",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Typewriter, "American Typewriter",
     u"Typewriter", gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Handwritten, "Marker Felt", u"Handwritten",
     gfx::Font::ITALIC},
    {foundation::BoostFontPreset::Condensed, "Arial Narrow", u"Condensed",
     gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Wide, "Verdana", u"Wide", gfx::Font::NORMAL},
    {foundation::BoostFontPreset::Comic, "Chalkboard SE", u"Comic",
     gfx::Font::NORMAL},
}};

bool IsEligibleHttpOrigin(const url::Origin& origin) {
  return !origin.opaque() && (origin.scheme() == url::kHttpScheme ||
                              origin.scheme() == url::kHttpsScheme);
}

url::Origin ActiveOriginForContents(content::WebContents* contents) {
  if (!contents) {
    return url::Origin();
  }
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (!rfh) {
    return url::Origin();
  }
  return rfh->GetLastCommittedOrigin();
}

uint32_t SkColorToRgb24(SkColor color) {
  return (static_cast<uint32_t>(SkColorGetR(color)) << 16) |
         (static_cast<uint32_t>(SkColorGetG(color)) << 8) |
         static_cast<uint32_t>(SkColorGetB(color));
}

SkColor Rgb24ToSkColor(uint32_t rgb) {
  return SkColorSetRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

std::string FormatRgb24Hex(uint32_t rgb) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out(7, '#');
  out[1] = kHex[(rgb >> 20) & 0xF];
  out[2] = kHex[(rgb >> 16) & 0xF];
  out[3] = kHex[(rgb >> 12) & 0xF];
  out[4] = kHex[(rgb >> 8) & 0xF];
  out[5] = kHex[(rgb >> 4) & 0xF];
  out[6] = kHex[rgb & 0xF];
  return out;
}

std::optional<uint32_t> ParseHexColor(std::string_view input) {
  std::string_view s = base::TrimWhitespaceASCII(input, base::TRIM_ALL);
  if (!s.empty() && s.front() == '#') {
    s.remove_prefix(1);
  }
  if (s.size() == 3) {
    uint32_t rgb = 0;
    for (char c : s) {
      int digit = 0;
      if (c >= '0' && c <= '9') {
        digit = c - '0';
      } else if (c >= 'a' && c <= 'f') {
        digit = 10 + (c - 'a');
      } else if (c >= 'A' && c <= 'F') {
        digit = 10 + (c - 'A');
      } else {
        return std::nullopt;
      }
      rgb = (rgb << 8) | static_cast<uint32_t>((digit << 4) | digit);
    }
    return rgb;
  }
  if (s.size() != 6) {
    return std::nullopt;
  }
  uint32_t rgb = 0;
  if (!base::HexStringToUInt(s, &rgb)) {
    return std::nullopt;
  }
  return rgb & 0xFF'FFFF;
}

foundation::BoostPaletteRecord PaletteFromRgb24(
    uint32_t rgb,
    const foundation::BoostPaletteRecord& prev) {
  const SkColor bg = Rgb24ToSkColor(rgb);
  color_utils::HSL bg_hsl;
  color_utils::SkColorToHSL(bg, &bg_hsl);
  const bool dark_bg = color_utils::IsDark(bg);
  const SkColor text = dark_bg ? SkColorSetRGB(0xF5, 0xF5, 0xF7)
                               : SkColorSetRGB(0x18, 0x18, 0x1B);
  const color_utils::HSL accent_hsl{bg_hsl.h, std::min(1.0, bg_hsl.s + 0.25),
                                    dark_bg ? 0.72 : 0.38};
  const SkColor accent = color_utils::HSLToSkColor(accent_hsl, SK_AlphaOPAQUE);

  foundation::BoostPaletteRecord palette{};
  palette.set = true;
  palette.background = rgb & 0xFF'FFFF;
  palette.text = SkColorToRgb24(text);
  palette.accent = SkColorToRgb24(accent);
  palette.contrast = prev.set ? prev.contrast : 50;
  palette.brightness = prev.set ? prev.brightness : 50;
  palette.saturation = prev.set ? prev.saturation : 50;
  palette.tint_strength =
      static_cast<uint8_t>(std::round(std::clamp(bg_hsl.s, 0.0, 1.0) * 100.0));
  return palette;
}

// Converts (x, y) in [0, 1] x [0, 1] on the 2D pad to a BoostPaletteRecord.
foundation::BoostPaletteRecord PadPositionToPalette(
    foundation::BoostColorMode mode,
    float x,
    float y) {
  x = std::clamp(x, 0.f, 1.f);
  y = std::clamp(y, 0.f, 1.f);

  foundation::BoostPaletteRecord palette{};
  palette.set = true;

  if (mode == foundation::BoostColorMode::Invert) {
    palette.background = 0x111114;
    palette.text = 0xF5F5F7;
    palette.accent = 0x6E7BFF;
    palette.contrast = static_cast<uint8_t>(std::round(x * 100.f));
    palette.brightness = 50;
    palette.saturation = 50;
    palette.tint_strength = static_cast<uint8_t>(std::round((1.f - y) * 100.f));
    return palette;
  }

  // Palette mode: X = hue [0, 1), Y = lightness (top = light, bottom = dark).
  // Center region gives clean near-neutral tones; outer region increases chroma.
  const float dx = x - 0.5f;
  const float dy = y - 0.5f;
  const float dist = std::min(1.f, std::hypot(dx, dy) * 2.f);

  const double hue = static_cast<double>(x);
  const double lightness = 0.94 - static_cast<double>(y) * 0.80;  // 0.94 .. 0.14
  const double saturation = 0.15 + static_cast<double>(dist) * 0.65;

  const color_utils::HSL bg_hsl{hue, saturation, lightness};
  const SkColor bg = color_utils::HSLToSkColor(bg_hsl, SK_AlphaOPAQUE);
  const bool dark_bg = color_utils::IsDark(bg);
  const SkColor text = dark_bg ? SkColorSetRGB(0xF5, 0xF5, 0xF7)
                               : SkColorSetRGB(0x18, 0x18, 0x1B);
  const color_utils::HSL accent_hsl{hue, std::min(1.0, saturation + 0.25),
                                    dark_bg ? 0.72 : 0.38};
  const SkColor accent = color_utils::HSLToSkColor(accent_hsl, SK_AlphaOPAQUE);

  palette.background = SkColorToRgb24(bg);
  palette.text = SkColorToRgb24(text);
  palette.accent = SkColorToRgb24(accent);
  palette.contrast = 50;
  palette.brightness = 50;
  palette.saturation = 50;
  palette.tint_strength = static_cast<uint8_t>(std::round(dist * 100.f));
  return palette;
}

gfx::PointF PaletteToPadPosition(foundation::BoostColorMode mode,
                                 const foundation::BoostPaletteRecord& p) {
  if (!p.set) {
    return gfx::PointF(0.5f, 0.5f);
  }
  if (mode == foundation::BoostColorMode::Invert) {
    return gfx::PointF(std::clamp(p.contrast / 100.f, 0.f, 1.f),
                       std::clamp(1.f - p.tint_strength / 100.f, 0.f, 1.f));
  }
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(Rgb24ToSkColor(p.background), &hsl);
  const float x = std::clamp(static_cast<float>(hsl.h), 0.f, 1.f);
  const float y = std::clamp(static_cast<float>((0.94 - hsl.l) / 0.80), 0.f, 1.f);
  return gfx::PointF(x, y);
}

std::unique_ptr<views::View> MakeBox(Orientation orientation,
                                     const gfx::Insets& insets,
                                     int spacing) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(orientation, insets, spacing));
  return view;
}

std::unique_ptr<views::Label> MakeLabel(const std::u16string& text,
                                        const gfx::FontList& font,
                                        SkColor color) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(font);
  label->SetEnabledColor(color);
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  return label;
}

// Clean pill button used for mode toggles, Size, Case, Zap, and Code rows.
class PillButton final : public views::Button {
  METADATA_HEADER(PillButton, views::Button)

 public:
  PillButton(PressedCallback callback,
             const std::u16string& text,
             const std::u16string& trailing_glyph = {},
             bool centered = true,
             int font_size_delta = 0,
             const gfx::VectorIcon* icon = nullptr,
             const gfx::VectorIcon* trailing_icon = nullptr)
      : views::Button(std::move(callback)),
        icon_(icon),
        trailing_icon_(trailing_icon) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 10);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal,
        icon_ ? gfx::Insets::VH(0, 6) : gfx::Insets::VH(0, 14), 8));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    layout->set_main_axis_alignment(centered ? MainAxis::kCenter
                                             : MainAxis::kStart);

    if (icon_) {
      icon_view_ = AddChildView(std::make_unique<views::ImageView>());
      icon_view_->SetCanProcessEventsWithinSubtree(false);
    } else {
      label_ = AddChildView(MakeLabel(
          text,
          gfx::FontList().Derive(font_size_delta, gfx::Font::NORMAL,
                                 gfx::Font::Weight::SEMIBOLD),
          kInkPrimary));
      if (!centered) {
        layout->SetFlexForView(label_, 1);
        label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      }
    }
    if (!trailing_glyph.empty() || trailing_icon_) {
      badge_ = AddChildView(MakeLabel(
          std::u16string(),
          gfx::FontList().Derive(-2, gfx::Font::NORMAL,
                                 gfx::Font::Weight::BOLD),
          kInkPrimary));
      badge_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      badge_->SetLineHeight(20);
      badge_->SetPreferredSize(gfx::Size(20, 20));
      badge_->SetVisible(false);
    }
    if (trailing_icon_) {
      trailing_icon_view_ = AddChildView(std::make_unique<views::ImageView>());
      trailing_icon_view_->SetCanProcessEventsWithinSubtree(false);
    } else if (!trailing_glyph.empty()) {
      trailing_ = AddChildView(MakeLabel(
          trailing_glyph,
          gfx::FontList().Derive(1, gfx::Font::NORMAL,
                                 gfx::Font::Weight::SEMIBOLD),
          kInkPrimary));
    }
    SetPreferredSize(gfx::Size(0, 38));
    GetViewAccessibility().SetName(text);
    UpdateVisuals();
  }

  void SetBadge(const std::u16string& text, SkColor bg, SkColor fg) {
    if (!badge_) {
      return;
    }
    badge_bg_ = bg;
    badge_fg_ = fg;
    badge_->SetText(text);
    const bool show = !text.empty();
    if (show) {
      badge_->SetPreferredSize(std::nullopt);
      const int text_w = badge_->GetPreferredSize({}).width();
      const int w = text.size() <= 1 ? 20 : std::max(20, text_w + 10);
      badge_->SetPreferredSize(gfx::Size(w, 20));
    }
    badge_->SetVisible(show);
    UpdateVisuals();
    InvalidateLayout();
  }

  void SetActive(bool active) {
    if (active_ == active) {
      return;
    }
    active_ = active;
    GetViewAccessibility().SetCheckedState(
        active_ ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse);
    UpdateVisuals();
  }

  void SetActiveColor(std::optional<SkColor> color) {
    if (active_color_ == color) {
      return;
    }
    active_color_ = color;
    UpdateVisuals();
  }

  void SetText(const std::u16string& text) {
    if (label_) {
      label_->SetText(text);
    }
    GetViewAccessibility().SetName(text);
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    UpdateVisuals();
  }

 private:
  void UpdateVisuals() {
    const bool hovered =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    const SkColor bg = active_ ? active_color_.value_or(kPillActiveBg)
                               : (hovered ? kPillHoverBg : kPillBg);
    const SkColor fg = active_
                           ? (active_color_.has_value() ? SK_ColorWHITE
                                                        : kPillActiveInk)
                           : kInkPrimary;
    SetBackground(views::CreateRoundedRectBackground(bg, 10));
    SetBorder(active_ ? nullptr
                      : views::CreateRoundedRectBorder(
                            1, 10, hovered ? kPillHoverBorder : kPillBorder));
    if (icon_view_ && icon_) {
      icon_view_->SetImage(ui::ImageModel::FromVectorIcon(*icon_, fg, 18));
    }
    if (label_) {
      label_->SetEnabledColor(fg);
    }
    if (trailing_icon_view_ && trailing_icon_) {
      const SkColor icon_fg = active_ ? fg : kInkSecondary;
      trailing_icon_view_->SetImage(
          ui::ImageModel::FromVectorIcon(*trailing_icon_, icon_fg, 16));
    }
    if (trailing_) {
      trailing_->SetEnabledColor(fg);
    }
    if (badge_ && badge_->GetVisible()) {
      badge_->SetBackground(views::CreateRoundedRectBackground(badge_bg_, 10));
      badge_->SetEnabledColor(badge_fg_);
    }
  }

  bool active_ = false;
  std::optional<SkColor> active_color_;
  SkColor badge_bg_ = kPillHoverBg;
  SkColor badge_fg_ = kInkPrimary;
  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;
  raw_ptr<const gfx::VectorIcon> trailing_icon_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::Label> badge_ = nullptr;
  raw_ptr<views::ImageView> trailing_icon_view_ = nullptr;
  raw_ptr<views::Label> trailing_ = nullptr;
};

BEGIN_METADATA(PillButton)
END_METADATA

// Centered header title button with a Lucide chevron-down icon on the right.
class TitleMenuButton final : public views::Button {
  METADATA_HEADER(TitleMenuButton, views::Button)

 public:
  explicit TitleMenuButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 6);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(2, 6), 4));
    layout->set_main_axis_alignment(MainAxis::kCenter);
    layout->set_cross_axis_alignment(CrossAxis::kCenter);

    label_ = AddChildView(MakeLabel(
        u"My Boost",
        gfx::FontList().Derive(0, gfx::Font::NORMAL,
                               gfx::Font::Weight::SEMIBOLD),
        kInkPrimary));
    label_->SetElideBehavior(gfx::ELIDE_TAIL);
    label_->SetCanProcessEventsWithinSubtree(false);

    chevron_ = AddChildView(std::make_unique<views::ImageView>());
    chevron_->SetCanProcessEventsWithinSubtree(false);
    SetPreferredSize(gfx::Size(0, 26));
    SetTooltipText(u"Boost options");
    GetViewAccessibility().SetName(u"My Boost");
    GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
    UpdateVisuals();
  }

  void SetTitle(const std::u16string& text, bool enabled_boost) {
    label_->SetText(text);
    GetViewAccessibility().SetName(text);
    enabled_boost_ = enabled_boost;
    UpdateVisuals();
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    UpdateVisuals();
  }

 private:
  void UpdateVisuals() {
    const bool hovered =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    SetBackground(hovered ? views::CreateRoundedRectBackground(kPillBg, 6)
                          : nullptr);
    const SkColor ink = enabled_boost_ ? kInkPrimary : kInkSecondary;
    label_->SetEnabledColor(ink);
    chevron_->SetImage(
        ui::ImageModel::FromVectorIcon(kChevronDownIcon, kInkSecondary, 14));
  }

  bool enabled_boost_ = true;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> chevron_ = nullptr;
};

BEGIN_METADATA(TitleMenuButton)
END_METADATA

// 2D color / contrast pad matching Arc's floating Boost card.
class ColorPadView final : public views::View {
  METADATA_HEADER(ColorPadView, views::View)

 public:
  using ChangeCallback = base::RepeatingCallback<void(float, float, bool)>;

  explicit ColorPadView(ChangeCallback on_change)
      : on_change_(std::move(on_change)) {
    SetPreferredSize(gfx::Size(kPadWidth, kPadHeight));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 14);
    GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);
    GetViewAccessibility().SetName(u"Boost color and contrast pad");
  }

  void SetState(foundation::BoostColorMode mode,
                const foundation::BoostPaletteRecord& palette) {
    mode_ = mode;
    pos_ = PaletteToPadPosition(mode, palette);
    palette_ = palette;
    SchedulePaint();
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (!event.IsOnlyLeftMouseButton()) {
      return false;
    }
    RequestFocus();
    dragging_ = true;
    UpdateFromPoint(event.location(), /*commit=*/false);
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (!dragging_) {
      return false;
    }
    UpdateFromPoint(event.location(), /*commit=*/false);
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (!dragging_) {
      return;
    }
    dragging_ = false;
    UpdateFromPoint(event.location(), /*commit=*/true);
  }

  void OnMouseCaptureLost() override {
    if (!dragging_) {
      return;
    }
    dragging_ = false;
    on_change_.Run(pos_.x(), pos_.y(), /*commit=*/true);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    constexpr float kStep = 0.05f;
    float nx = pos_.x();
    float ny = pos_.y();
    switch (event.key_code()) {
      case ui::VKEY_LEFT:
        nx = std::clamp(nx - kStep, 0.f, 1.f);
        break;
      case ui::VKEY_RIGHT:
        nx = std::clamp(nx + kStep, 0.f, 1.f);
        break;
      case ui::VKEY_UP:
        ny = std::clamp(ny - kStep, 0.f, 1.f);
        break;
      case ui::VKEY_DOWN:
        ny = std::clamp(ny + kStep, 0.f, 1.f);
        break;
      default:
        return false;
    }
    pos_ = gfx::PointF(nx, ny);
    SchedulePaint();
    on_change_.Run(pos_.x(), pos_.y(), /*commit=*/true);
    return true;
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::RectF bounds(GetLocalBounds());
    const float radius = 14.f;

    // 1. Outer gradient border (rainbow in Palette mode, monochrome in Invert).
    cc::PaintFlags border_flags;
    border_flags.setAntiAlias(true);
    if (mode_ == foundation::BoostColorMode::Palette) {
      const SkColor4f rainbow[] = {
          SkColor4f::FromColor(SkColorSetRGB(0x5B, 0x6C, 0xFF)),
          SkColor4f::FromColor(SkColorSetRGB(0xD9, 0x46, 0xEF)),
          SkColor4f::FromColor(SkColorSetRGB(0xFF, 0x5E, 0x5B)),
          SkColor4f::FromColor(SkColorSetRGB(0xFF, 0xB8, 0x30)),
          SkColor4f::FromColor(SkColorSetRGB(0x34, 0xD3, 0x99)),
          SkColor4f::FromColor(SkColorSetRGB(0x38, 0xBD, 0xF8)),
          SkColor4f::FromColor(SkColorSetRGB(0x5B, 0x6C, 0xFF)),
      };
      border_flags.setShader(cc::PaintShader::MakeSweepGradient(
          bounds.CenterPoint().x(), bounds.CenterPoint().y(), rainbow, nullptr,
          std::size(rainbow), SkTileMode::kClamp, 0, 360));
    } else if (mode_ == foundation::BoostColorMode::Invert) {
      const SkColor4f mono[] = {
          SkColor4f::FromColor(SkColorSetRGB(0x1E, 0x1E, 0x22)),
          SkColor4f::FromColor(SkColorSetRGB(0xB8, 0xB8, 0xC0)),
      };
      const SkPoint pts[2] = {SkPoint::Make(0, 0),
                              SkPoint::Make(bounds.width(), bounds.height())};
      border_flags.setShader(cc::PaintShader::MakeLinearGradient(
          pts, mono, nullptr, 2, SkTileMode::kClamp));
    } else {
      border_flags.setColor(kBorderSubtle);
    }
    canvas->DrawRoundRect(bounds, radius, border_flags);

    // 2. Inner white surface inset by border thickness.
    const float inset =
        mode_ == foundation::BoostColorMode::None ? 2.f : 5.f;
    gfx::RectF inner = bounds;
    inner.Inset(inset);
    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorWHITE);
    canvas->DrawRoundRect(inner, radius - inset * 0.5f, fill);

    // 3. Soft tint wash reflecting current mode/position.
    if (mode_ == foundation::BoostColorMode::Palette && palette_.set) {
      const SkColor tint =
          SkColorSetA(Rgb24ToSkColor(palette_.background), 0x48);
      cc::PaintFlags wash;
      wash.setAntiAlias(true);
      wash.setColor(tint);
      canvas->DrawRoundRect(inner, radius - inset * 0.5f, wash);
    } else if (mode_ == foundation::BoostColorMode::Invert) {
      const SkColor4f shades[] = {
          SkColor4f::FromColor(SkColorSetA(SK_ColorBLACK, 0x18)),
          SkColor4f::FromColor(SkColorSetA(SK_ColorBLACK, 0x04)),
      };
      const SkPoint pts[2] = {SkPoint::Make(inner.x(), inner.y()),
                              SkPoint::Make(inner.right(), inner.bottom())};
      cc::PaintFlags wash;
      wash.setAntiAlias(true);
      wash.setShader(cc::PaintShader::MakeLinearGradient(
          pts, shades, nullptr, 2, SkTileMode::kClamp));
      canvas->DrawRoundRect(inner, radius - inset * 0.5f, wash);
    }

    // 4. Subtle dotted grid inside the pad.
    cc::PaintFlags dot_flags;
    dot_flags.setAntiAlias(true);
    dot_flags.setColor(SkColorSetARGB(0x18, 0x1C, 0x1C, 0x1E));
    for (float y = inner.y() + 10.f; y < inner.bottom() - 6.f; y += 10.f) {
      for (float x = inner.x() + 10.f; x < inner.right() - 6.f; x += 10.f) {
        canvas->DrawCircle(gfx::PointF(x, y), 0.9f, dot_flags);
      }
    }

    // 5. Draggable circular handle.
    const float handle_r = 14.f;
    const float hx =
        inner.x() + handle_r + pos_.x() * (inner.width() - handle_r * 2.f);
    const float hy =
        inner.y() + handle_r + pos_.y() * (inner.height() - handle_r * 2.f);
    const gfx::PointF center(hx, hy);

    cc::PaintFlags shadow;
    shadow.setAntiAlias(true);
    shadow.setColor(SkColorSetA(SK_ColorBLACK, 0x26));
    canvas->DrawCircle(gfx::PointF(hx, hy + 1.5f), handle_r + 1.5f, shadow);

    cc::PaintFlags knob;
    knob.setAntiAlias(true);
    if (mode_ == foundation::BoostColorMode::Palette && palette_.set) {
      knob.setColor(Rgb24ToSkColor(palette_.background));
    } else if (mode_ == foundation::BoostColorMode::Invert) {
      knob.setColor(SkColorSetRGB(0x2B, 0x2B, 0x30));
    } else {
      knob.setColor(SK_ColorWHITE);
    }
    canvas->DrawCircle(center, handle_r, knob);

    cc::PaintFlags ring;
    ring.setAntiAlias(true);
    ring.setStyle(cc::PaintFlags::kStroke_Style);
    ring.setStrokeWidth(2.5f);
    ring.setColor(SK_ColorWHITE);
    canvas->DrawCircle(center, handle_r, ring);

    cc::PaintFlags outline;
    outline.setAntiAlias(true);
    outline.setStyle(cc::PaintFlags::kStroke_Style);
    outline.setStrokeWidth(1.f);
    outline.setColor(SkColorSetA(SK_ColorBLACK, 0x1A));
    canvas->DrawCircle(center, handle_r + 1.25f, outline);
  }

 private:
  void UpdateFromPoint(const gfx::Point& point, bool commit) {
    const float pad = 16.f;
    const float w = std::max(1.f, static_cast<float>(width()) - pad * 2.f);
    const float h = std::max(1.f, static_cast<float>(height()) - pad * 2.f);
    pos_ = gfx::PointF(std::clamp((point.x() - pad) / w, 0.f, 1.f),
                       std::clamp((point.y() - pad) / h, 0.f, 1.f));
    SchedulePaint();
    on_change_.Run(pos_.x(), pos_.y(), commit);
  }

  ChangeCallback on_change_;
  foundation::BoostColorMode mode_ = foundation::BoostColorMode::None;
  foundation::BoostPaletteRecord palette_{};
  gfx::PointF pos_{0.5f, 0.5f};
  bool dragging_ = false;
};

BEGIN_METADATA(ColorPadView)
END_METADATA

// One cell in the 4x4 "Aa" typography preset grid.
class FontPresetCell final : public views::Button {
  METADATA_HEADER(FontPresetCell, views::Button)

 public:
  FontPresetCell(PressedCallback callback, const FontPresetInfo& info)
      : views::Button(std::move(callback)), preset_(info.preset) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 6);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets(), 0));
    layout->set_main_axis_alignment(MainAxis::kCenter);
    layout->set_cross_axis_alignment(CrossAxis::kCenter);

    gfx::FontList font =
        info.family[0] != '\0'
            ? gfx::FontList(gfx::Font(info.family, 13))
            : gfx::FontList().Derive(0, info.style, gfx::Font::Weight::MEDIUM);
    label_ = AddChildView(MakeLabel(u"Aa", font, kInkPrimary));
    SetPreferredSize(gfx::Size(48, 28));
    SetTooltipText(info.name);
    GetViewAccessibility().SetName(std::u16string(u"Font: ") + info.name);
    UpdateVisuals();
  }

  foundation::BoostFontPreset preset() const { return preset_; }

  void SetSelected(bool selected) {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    GetViewAccessibility().SetCheckedState(
        selected_ ? ax::mojom::CheckedState::kTrue
                  : ax::mojom::CheckedState::kFalse);
    UpdateVisuals();
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    UpdateVisuals();
  }

 private:
  void UpdateVisuals() {
    const bool hovered =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    if (selected_) {
      SetBackground(views::CreateRoundedRectBackground(SK_ColorWHITE, 6));
      label_->SetEnabledColor(kPillActiveInk);
    } else if (hovered) {
      SetBackground(views::CreateRoundedRectBackground(kPillBg, 6));
      label_->SetEnabledColor(kInkPrimary);
    } else {
      SetBackground(nullptr);
      label_->SetEnabledColor(kInkSecondary);
    }
  }

  const foundation::BoostFontPreset preset_;
  bool selected_ = false;
  raw_ptr<views::Label> label_ = nullptr;
};

BEGIN_METADATA(FontPresetCell)
END_METADATA

// Arc-style light horizontal slider for Contrast, Brightness, and Saturation.
class BoostAdjustSlider final : public views::Slider {
  METADATA_HEADER(BoostAdjustSlider, views::Slider)

 public:
  explicit BoostAdjustSlider(views::SliderListener* listener)
      : views::Slider(listener) {
    SetValueIndicatorRadius(kThumbRadius);
    SetPreferredSize(gfx::Size(0, kThumbRadius * 2 + 4));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::Rect content = GetContentsBounds();
    const float cy = static_cast<float>(content.CenterPoint().y());
    const gfx::RectF track(content.x() + kThumbRadius, cy - kTrackHeight * 0.5f,
                           std::max(1.f, content.width() - kThumbRadius * 2.f),
                           kTrackHeight);
    cc::PaintFlags track_flags;
    track_flags.setAntiAlias(true);
    track_flags.setColor(SkColorSetARGB(0x38, 0xFF, 0xFF, 0xFF));
    canvas->DrawRoundRect(track, kTrackHeight * 0.5f, track_flags);

    // Subtle center tick at 50% (neutral).
    const float mid_x = track.CenterPoint().x();
    cc::PaintFlags tick_flags;
    tick_flags.setAntiAlias(true);
    tick_flags.setColor(SkColorSetARGB(0x66, 0xFF, 0xFF, 0xFF));
    canvas->DrawRoundRect(gfx::RectF(mid_x - 0.75f, cy - 4.f, 1.5f, 8.f),
                          0.75f, tick_flags);

    const float thumb_x = track.x() + GetValue() * track.width();
    const gfx::PointF thumb(thumb_x, cy);

    cc::PaintFlags shadow;
    shadow.setAntiAlias(true);
    shadow.setColor(SkColorSetA(SK_ColorBLACK, 0x24));
    canvas->DrawCircle(gfx::PointF(thumb.x(), thumb.y() + 1.f),
                       kThumbRadius + 0.5f, shadow);

    cc::PaintFlags knob;
    knob.setAntiAlias(true);
    knob.setColor(SK_ColorWHITE);
    canvas->DrawCircle(thumb, kThumbRadius, knob);

    cc::PaintFlags border;
    border.setAntiAlias(true);
    border.setStyle(cc::PaintFlags::kStroke_Style);
    border.setStrokeWidth(1.f);
    border.setColor(SkColorSetA(SK_ColorBLACK, 0x1A));
    canvas->DrawCircle(thumb, kThumbRadius, border);
  }

 private:
  static constexpr float kTrackHeight = 4.f;
  static constexpr float kThumbRadius = 7.f;
};

BEGIN_METADATA(BoostAdjustSlider)
END_METADATA

// Isolated-world interactive Zap picker overlay (closed Shadow DOM so page JS
// cannot inspect, style, or forge Zap actions).
constexpr char16_t kZapStartScript[] = uR"JS((() => {
  if (window.__myarcZap && window.__myarcZap.active) return
  const queue = []
  const host = document.createElement('div')
  host.setAttribute('data-myarc-zap-host', '1')
  host.style.cssText = 'position:fixed;inset:0;pointer-events:none;z-index:2147483647;'
  const shadow = host.attachShadow({ mode: 'closed' })

  const cursorStyle = document.createElement('style')
  cursorStyle.setAttribute('data-myarc-zap-cursor', '1')
  cursorStyle.textContent = '*, *::before, *::after { cursor: crosshair !important; user-select: none !important; -webkit-user-select: none !important; }'
  ;(document.head || document.documentElement).appendChild(cursorStyle)

  const previewStyle = document.createElement('style')
  previewStyle.setAttribute('data-myarc-zap-preview', '1')
  ;(document.head || document.documentElement).appendChild(previewStyle)

  const style = document.createElement('style')
  style.textContent = `
    .frame {
      position: fixed; inset: 0; pointer-events: none;
      border: 2px solid #F9671D; border-radius: 10px; box-sizing: border-box;
    }
    .box {
      position: fixed; pointer-events: none; display: none;
      background: rgba(255, 210, 60, 0.14);
      border: 1.5px solid #EAB308; border-radius: 6px;
      box-sizing: border-box;
    }
    .bar {
      position: fixed; left: 2px; right: 2px; bottom: 2px;
      height: 52px; padding: 0 14px;
      border-bottom-left-radius: 8px; border-bottom-right-radius: 8px;
      display: flex; align-items: center; justify-content: center;
      background: rgba(18, 18, 22, 0.94); color: #F5F5F7;
      font: 500 13px -apple-system, BlinkMacSystemFont, sans-serif;
      pointer-events: auto; box-sizing: border-box; cursor: default;
    }
    .bar strong { font-weight: 700; color: #FFFFFF; }
    .done {
      position: absolute; right: 14px;
      background: #FFFFFF; color: #1C1C1E; border: none;
      border-radius: 8px; padding: 7px 14px; cursor: pointer;
      font: 600 13px -apple-system, BlinkMacSystemFont, sans-serif;
    }
    .done:hover { background: #EBEBF0; }
    .pill {
      position: fixed; display: none; pointer-events: auto; cursor: default;
      background: #FFFFFF; color: #1C1C1E; border-radius: 14px;
      padding: 8px 10px 7px; max-width: 380px;
      box-shadow: 0 8px 28px rgba(0, 0, 0, 0.28), 0 0 0 1px rgba(0, 0, 0, 0.08);
      font-family: -apple-system, BlinkMacSystemFont, sans-serif;
    }
    .pill-row { display: flex; align-items: center; gap: 6px; }
    .pill-btn {
      border: none; border-radius: 9px; padding: 7px 12px; cursor: pointer;
      font: 600 12px -apple-system, BlinkMacSystemFont, sans-serif;
      color: #FFFFFF; white-space: nowrap;
    }
    .btn-this { background: #EF4444; }
    .btn-this:hover { background: #DC2626; }
    .btn-all { background: #F9671D; }
    .btn-all:hover { background: #EA580C; }
    .btn-cancel { background: #8E8E93; }
    .btn-cancel:hover { background: #6E6E73; }
    .pill-meta {
      margin-top: 6px; font: 400 11px Menlo, ui-monospace, monospace;
      color: #6E6E73; white-space: nowrap; overflow: hidden;
      text-overflow: ellipsis; max-width: 360px;
    }
  `
  shadow.appendChild(style)

  const frame = document.createElement('div')
  frame.className = 'frame'
  const box = document.createElement('div')
  box.className = 'box'
  const bar = document.createElement('div')
  bar.className = 'bar'
  const msg = document.createElement('span')
  msg.append('Click elements on the page to ')
  const strong = document.createElement('strong')
  strong.textContent = 'Zap'
  msg.append(strong, ' them')
  const doneBtn = document.createElement('button')
  doneBtn.className = 'done'
  doneBtn.textContent = 'Done'
  bar.append(msg, doneBtn)

  const pill = document.createElement('div')
  pill.className = 'pill'
  const row = document.createElement('div')
  row.className = 'pill-row'
  const btnThis = document.createElement('button')
  btnThis.className = 'pill-btn btn-this'
  btnThis.textContent = 'Zap this'
  const btnAll = document.createElement('button')
  btnAll.className = 'pill-btn btn-all'
  btnAll.textContent = 'Zap all related elements'
  const btnCancel = document.createElement('button')
  btnCancel.className = 'pill-btn btn-cancel'
  btnCancel.textContent = 'Cancel'
  row.append(btnThis, btnAll, btnCancel)
  const meta = document.createElement('div')
  meta.className = 'pill-meta'
  pill.append(row, meta)

  shadow.append(frame, box, pill, bar)
  ;(document.documentElement || document.body).appendChild(host)

  const safeIdent = (s) => typeof s === 'string' && /^[A-Za-z_][A-Za-z0-9_-]*$/.test(s)
  const validClasses = (el) => {
    if (!el || !el.classList) return []
    return Array.from(el.classList).filter((c) => safeIdent(c) && c.length <= 48).slice(0, 2)
  }
  const nthIndex = (el) => {
    let i = 1
    let sib = el
    while ((sib = sib.previousElementSibling)) i += 1
    return i
  }
  const specificSelector = (el) => {
    const parts = []
    let cur = el
    let depth = 0
    while (cur && cur.nodeType === 1 && cur !== document.documentElement && depth < 4) {
      const tag = cur.tagName.toLowerCase()
      if (cur.id && safeIdent(cur.id)) {
        parts.unshift('#' + cur.id)
        break
      }
      const cls = validClasses(cur)
      let seg = tag + (cls.length ? '.' + cls.join('.') : '')
      if (cur.parentElement && cur.parentElement.children.length > 1) {
        seg += ':nth-child(' + nthIndex(cur) + ')'
      }
      parts.unshift(seg)
      cur = cur.parentElement
      depth += 1
    }
    return parts.join(' > ') || el.tagName.toLowerCase()
  }
  const relatedSelector = (el) => {
    const tag = el.tagName.toLowerCase()
    const cls = validClasses(el)
    if (cls.length) return tag + '.' + cls.join('.')
    if (el.parentElement && el.parentElement !== document.documentElement) {
      const pCls = validClasses(el.parentElement)
      const pTag = el.parentElement.tagName.toLowerCase()
      return pTag + (pCls.length ? '.' + pCls[0] : '') + ' > ' + tag
    }
    return tag
  }
  const matchCount = (sel) => {
    try { return document.querySelectorAll(sel).length } catch { return 1 }
  }

  let selected = null
  let specSel = ''
  let relSel = ''
  let hovered = null

  const pad = 4
  const positionBox = (el) => {
    if (!el || el === host || el === document.documentElement || el === document.body) {
      box.style.display = 'none'
      return
    }
    const r = el.getBoundingClientRect()
    if (r.width <= 0 || r.height <= 0) {
      box.style.display = 'none'
      return
    }
    const left = Math.max(2, r.left - pad)
    const top = Math.max(2, r.top - pad)
    const right = Math.min(window.innerWidth - 2, r.right + pad)
    const bottom = Math.min(window.innerHeight - 2, r.bottom + pad)
    box.style.display = 'block'
    box.style.left = left + 'px'
    box.style.top = top + 'px'
    box.style.width = Math.max(0, right - left) + 'px'
    box.style.height = Math.max(0, bottom - top) + 'px'
  }

  const clearSelection = () => {
    selected = null
    specSel = ''
    relSel = ''
    pill.style.display = 'none'
  }

  const showPillFor = (el) => {
    selected = el
    specSel = specificSelector(el)
    relSel = relatedSelector(el)
    positionBox(el)
    const specCount = matchCount(specSel)
    meta.textContent = specCount + '  ' + specSel
    pill.style.display = 'block'
    const r = el.getBoundingClientRect()
    const pr = pill.getBoundingClientRect()
    const safeRight = window.innerWidth > 680 ? 276 : 12
    let left = Math.max(12, Math.min(r.left - pad, window.innerWidth - pr.width - safeRight))
    let top = r.bottom + pad + 8
    if (top + pr.height > window.innerHeight - 64) {
      top = Math.max(12, r.top - pr.height - pad - 8)
    }
    pill.style.left = left + 'px'
    pill.style.top = top + 'px'
  }

  const isOwnEvent = (e) => e.composedPath && e.composedPath().includes(host)

  const onMove = (e) => {
    if (selected || isOwnEvent(e)) return
    const el = document.elementFromPoint(e.clientX, e.clientY)
    if (el && el !== hovered && el !== host) {
      hovered = el
      positionBox(el)
    }
  }
  const onMouseDown = (e) => {
    if (isOwnEvent(e)) return
    if (e.button !== 0) return
    e.preventDefault()
    e.stopImmediatePropagation()
    const el = document.elementFromPoint(e.clientX, e.clientY)
    if (!el || el === host || el === document.documentElement || el === document.body) return
    showPillFor(el)
  }
  const blockPointer = (e) => {
    if (isOwnEvent(e)) return
    e.preventDefault()
    e.stopImmediatePropagation()
  }
  const onKey = (e) => {
    if (e.key === 'Escape') {
      e.preventDefault()
      e.stopImmediatePropagation()
      if (selected) {
        clearSelection()
      } else {
        queue.push({ type: 'done' })
        stop()
      }
    }
  }

  btnThis.addEventListener('mouseenter', () => {
    if (specSel) meta.textContent = matchCount(specSel) + '  ' + specSel
  })
  btnAll.addEventListener('mouseenter', () => {
    if (relSel) meta.textContent = matchCount(relSel) + '  ' + relSel
  })
  btnThis.addEventListener('click', (e) => {
    e.stopPropagation()
    if (specSel) {
      queue.push({ type: 'zap', selector: specSel })
      previewStyle.textContent += specSel + ' { display: none !important; }\n'
    }
    clearSelection()
    box.style.display = 'none'
  })
  btnAll.addEventListener('click', (e) => {
    e.stopPropagation()
    if (relSel) {
      queue.push({ type: 'zap', selector: relSel })
      previewStyle.textContent += relSel + ' { display: none !important; }\n'
    }
    clearSelection()
    box.style.display = 'none'
  })
  btnCancel.addEventListener('click', (e) => {
    e.stopPropagation()
    clearSelection()
  })
  doneBtn.addEventListener('click', (e) => {
    e.stopPropagation()
    queue.push({ type: 'done' })
    stop()
  })

  const clearPreview = () => {
    previewStyle.textContent = ''
  }

  const stop = () => {
    window.removeEventListener('mousemove', onMove, true)
    window.removeEventListener('mousedown', onMouseDown, true)
    window.removeEventListener('mouseup', blockPointer, true)
    window.removeEventListener('click', blockPointer, true)
    window.removeEventListener('keydown', onKey, true)
    cursorStyle.remove()
    previewStyle.remove()
    host.remove()
    if (window.__myarcZap) window.__myarcZap.active = false
  }

  window.addEventListener('mousemove', onMove, true)
  window.addEventListener('mousedown', onMouseDown, true)
  window.addEventListener('mouseup', blockPointer, true)
  window.addEventListener('click', blockPointer, true)
  window.addEventListener('keydown', onKey, true)

  window.__myarcZap = { active: true, queue, stop, clearPreview }
})())JS";

constexpr char16_t kZapPollScript[] = uR"JS((() => {
  const z = window.__myarcZap
  if (!z || !z.queue || !z.queue.length) return []
  return z.queue.splice(0, z.queue.length)
})())JS";

constexpr char16_t kZapStopScript[] = uR"JS((() => {
  const z = window.__myarcZap
  if (z && typeof z.stop === 'function') z.stop()
  delete window.__myarcZap
})())JS";

// Isolated-world single-element selector picker for the Boost Code editor.
constexpr char16_t kCssPickStartScript[] = uR"JS((() => {
  if (window.__myarcZap && typeof window.__myarcZap.stop === 'function') {
    window.__myarcZap.stop()
  }
  const queue = []
  const host = document.createElement('div')
  host.setAttribute('data-myarc-zap-host', '1')
  host.style.cssText = 'position:fixed;inset:0;pointer-events:none;z-index:2147483647;'
  const shadow = host.attachShadow({ mode: 'closed' })

  const cursorStyle = document.createElement('style')
  cursorStyle.setAttribute('data-myarc-zap-cursor', '1')
  cursorStyle.textContent = '*, *::before, *::after { cursor: crosshair !important; user-select: none !important; -webkit-user-select: none !important; }'
  ;(document.head || document.documentElement).appendChild(cursorStyle)

  const style = document.createElement('style')
  style.textContent = `
    .frame {
      position: fixed; inset: 0; pointer-events: none;
      border: 2px solid #4F5BFF; border-radius: 10px; box-sizing: border-box;
    }
    .box {
      position: fixed; pointer-events: none; display: none;
      background: rgba(79, 91, 255, 0.14);
      border: 1.5px solid #4F5BFF; border-radius: 6px;
      box-sizing: border-box;
    }
    .tip {
      position: fixed; pointer-events: none; display: none;
      background: #1C1C1E; color: #F5F5F7; border-radius: 6px;
      padding: 4px 8px; font: 500 11px Menlo, ui-monospace, monospace;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
      max-width: 320px; white-space: nowrap; overflow: hidden;
      text-overflow: ellipsis;
    }
  `
  shadow.appendChild(style)
  const frame = document.createElement('div')
  frame.className = 'frame'
  const box = document.createElement('div')
  box.className = 'box'
  const tip = document.createElement('div')
  tip.className = 'tip'
  shadow.append(frame, box, tip)
  ;(document.documentElement || document.body).appendChild(host)

  const safeIdent = (s) => typeof s === 'string' && /^[A-Za-z_][A-Za-z0-9_-]*$/.test(s)
  const buildSelector = (el) => {
    const tag = el.tagName.toLowerCase()
    if (el.id && safeIdent(el.id)) return '#' + el.id
    const cls = el.classList
      ? Array.from(el.classList).filter((c) => safeIdent(c) && c.length <= 48).slice(0, 2)
      : []
    if (cls.length) return tag + '.' + cls.join('.')
    return tag
  }

  const pad = 4
  let hovered = null
  const onMove = (e) => {
    const el = document.elementFromPoint(e.clientX, e.clientY)
    if (!el || el === host || el === document.documentElement || el === document.body) {
      box.style.display = 'none'
      tip.style.display = 'none'
      return
    }
    if (el !== hovered) {
      hovered = el
      const r = el.getBoundingClientRect()
      const left = Math.max(2, r.left - pad)
      const top = Math.max(2, r.top - pad)
      const right = Math.min(window.innerWidth - 2, r.right + pad)
      const bottom = Math.min(window.innerHeight - 2, r.bottom + pad)
      box.style.display = 'block'
      box.style.left = left + 'px'
      box.style.top = top + 'px'
      box.style.width = Math.max(0, right - left) + 'px'
      box.style.height = Math.max(0, bottom - top) + 'px'
      tip.textContent = buildSelector(el)
      tip.style.display = 'block'
      const safeRight = window.innerWidth > 760 ? 412 : 12
      tip.style.left = Math.max(10, Math.min(left, window.innerWidth - 200 - safeRight)) + 'px'
      tip.style.top = Math.max(10, top - 28) + 'px'
    }
  }
  let picked = false
  const onMouseDown = (e) => {
    if (e.button !== 0) return
    e.preventDefault()
    e.stopImmediatePropagation()
    const el = document.elementFromPoint(e.clientX, e.clientY)
    if (el && el !== host && el !== document.documentElement && el !== document.body) {
      picked = true
      queue.push({ type: 'pick', selector: buildSelector(el) })
      stop()
    }
  }
  const blockPointer = (e) => {
    e.preventDefault()
    e.stopImmediatePropagation()
  }
  const swallowOneClick = (e) => {
    e.preventDefault()
    e.stopImmediatePropagation()
  }
  const onKey = (e) => {
    if (e.key === 'Escape') {
      e.preventDefault()
      e.stopImmediatePropagation()
      queue.push({ type: 'pick_cancel' })
      stop()
    }
  }
  const stop = () => {
    window.removeEventListener('mousemove', onMove, true)
    window.removeEventListener('mousedown', onMouseDown, true)
    window.removeEventListener('mouseup', blockPointer, true)
    window.removeEventListener('click', blockPointer, true)
    window.removeEventListener('keydown', onKey, true)
    if (picked) {
      window.addEventListener('click', swallowOneClick, { capture: true, once: true })
      setTimeout(() => window.removeEventListener('click', swallowOneClick, true), 400)
    }
    cursorStyle.remove()
    host.remove()
    if (window.__myarcZap) window.__myarcZap.active = false
  }

  window.addEventListener('mousemove', onMove, true)
  window.addEventListener('mousedown', onMouseDown, true)
  window.addEventListener('mouseup', blockPointer, true)
  window.addEventListener('click', blockPointer, true)
  window.addEventListener('keydown', onKey, true)
  window.__myarcZap = { active: true, queue, stop }
})())JS";

class CssCodeTextarea final : public views::Textarea {
  METADATA_HEADER(CssCodeTextarea, views::Textarea)

 public:
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_TAB && !event.IsCommandDown() &&
        !event.IsControlDown()) {
      return true;
    }
    return views::Textarea::SkipDefaultKeyEventProcessing(event);
  }
};

BEGIN_METADATA(CssCodeTextarea)
END_METADATA

// Bounded single-pass CSS syntax highlighter for the dark Boost Code editor.
void ApplyCssSyntaxHighlighting(views::Textarea* editor) {
  if (!editor) {
    return;
  }
  const std::u16string_view text = editor->GetText();
  editor->SetColor(kCodeInk);
  constexpr size_t kMaxHighlightChars = 65536;
  const size_t n = std::min(text.size(), kMaxHighlightChars);
  if (n == 0) {
    return;
  }

  auto is_ident_char = [](char16_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_';
  };
  auto is_hex_char = [](char16_t c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
  };
  auto is_space = [](char16_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  };

  int brace_depth = 0;
  bool in_value = false;
  size_t i = 0;
  while (i < n) {
    const char16_t c = text[i];
    if (is_space(c)) {
      ++i;
      continue;
    }
    // Block comment /* ... */
    if (c == '/' && i + 1 < n && text[i + 1] == '*') {
      const size_t start = i;
      i += 2;
      while (i + 1 < n && !(text[i] == '*' && text[i + 1] == '/')) {
        ++i;
      }
      i = (i + 1 < n) ? i + 2 : n;
      editor->ApplyColor(kCodeComment, gfx::Range(start, i));
      continue;
    }
    // Quoted string '...' or "..."
    if (c == '"' || c == '\'') {
      const char16_t quote = c;
      const size_t start = i++;
      while (i < n) {
        if (text[i] == '\\' && i + 1 < n) {
          i += 2;
          continue;
        }
        if (text[i] == quote) {
          ++i;
          break;
        }
        ++i;
      }
      editor->ApplyColor(kCodeString, gfx::Range(start, i));
      continue;
    }
    // Structural punctuation
    if (c == '{') {
      editor->ApplyColor(kCodePunct, gfx::Range(i, i + 1));
      ++brace_depth;
      in_value = false;
      ++i;
      continue;
    }
    if (c == '}') {
      editor->ApplyColor(kCodePunct, gfx::Range(i, i + 1));
      brace_depth = std::max(0, brace_depth - 1);
      in_value = false;
      ++i;
      continue;
    }
    if (c == ':' && brace_depth > 0 && !in_value) {
      editor->ApplyColor(kCodePunct, gfx::Range(i, i + 1));
      in_value = true;
      ++i;
      continue;
    }
    if (c == ';') {
      editor->ApplyColor(kCodePunct, gfx::Range(i, i + 1));
      in_value = false;
      ++i;
      continue;
    }
    if (c == '(' || c == ')' || c == ',') {
      editor->ApplyColor(kCodePunct, gfx::Range(i, i + 1));
      ++i;
      continue;
    }
    // At-rule (@media, @keyframes, ...)
    if (c == '@') {
      const size_t start = i++;
      while (i < n && is_ident_char(text[i])) {
        ++i;
      }
      editor->ApplyColor(kCodeKeyword, gfx::Range(start, i));
      continue;
    }

    if (brace_depth == 0) {
      // Selector run up to '{', '}', ';', ',', comment, or string.
      const size_t start = i;
      while (i < n && text[i] != '{' && text[i] != '}' && text[i] != ';' &&
             text[i] != ',' && text[i] != '"' && text[i] != '\'' &&
             !(text[i] == '/' && i + 1 < n && text[i + 1] == '*')) {
        ++i;
      }
      editor->ApplyColor(kCodeSelector, gfx::Range(start, i));
      continue;
    }

    if (!in_value) {
      // Property name up to ':', ';', '}', or whitespace.
      const size_t start = i;
      while (i < n && text[i] != ':' && text[i] != ';' && text[i] != '}' &&
             text[i] != '{' && !is_space(text[i])) {
        ++i;
      }
      editor->ApplyColor(kCodeProperty, gfx::Range(start, i));
      continue;
    }

    // Inside property value:
    // !important
    if (c == '!') {
      const size_t start = i++;
      while (i < n && is_ident_char(text[i])) {
        ++i;
      }
      editor->ApplyColor(kCodeKeyword, gfx::Range(start, i));
      continue;
    }
    // Hex color (#fff, #0026e4, ...)
    if (c == '#' && i + 1 < n && is_hex_char(text[i + 1])) {
      const size_t start = i++;
      while (i < n && is_hex_char(text[i])) {
        ++i;
      }
      editor->ApplyColor(kCodeHex, gfx::Range(start, i));
      continue;
    }
    // Number + optional unit (1rem, .5rem, -12px, 50%)
    if ((c >= '0' && c <= '9') ||
        (c == '.' && i + 1 < n && text[i + 1] >= '0' && text[i + 1] <= '9') ||
        (c == '-' && i + 1 < n &&
         ((text[i + 1] >= '0' && text[i + 1] <= '9') ||
          (text[i + 1] == '.' && i + 2 < n && text[i + 2] >= '0' &&
           text[i + 2] <= '9')))) {
      const size_t start = i++;
      while (i < n && ((text[i] >= '0' && text[i] <= '9') || text[i] == '.')) {
        ++i;
      }
      if (i < n && text[i] == '%') {
        ++i;
      } else {
        while (i < n && ((text[i] >= 'a' && text[i] <= 'z') ||
                         (text[i] >= 'A' && text[i] <= 'Z'))) {
          ++i;
        }
      }
      editor->ApplyColor(kCodeNumber, gfx::Range(start, i));
      continue;
    }
    // CSS function name (rgba(, calc(, var(, ...)
    if (is_ident_char(c)) {
      const size_t start = i;
      while (i < n && is_ident_char(text[i])) {
        ++i;
      }
      if (i < n && text[i] == '(') {
        editor->ApplyColor(kCodeSelector, gfx::Range(start, i));
      }
      continue;
    }
    ++i;
  }
}

// Bounded CSS pretty-printer for the Boost Code editor Format button.
std::u16string FormatCssText(std::u16string_view input) {
  constexpr size_t kMaxFormatChars = 65536;
  const size_t n = std::min(input.size(), kMaxFormatChars);
  auto is_space = [](char16_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  };
  auto trim_trailing_spaces = [](std::u16string& s) {
    while (!s.empty() && (s.back() == u' ' || s.back() == u'\t')) {
      s.pop_back();
    }
  };

  std::u16string out;
  out.reserve(n + 64);
  int indent = 0;
  bool at_line_start = true;
  bool pending_space = false;

  auto emit_indent = [&]() {
    out.append(static_cast<size_t>(indent) * 2, u' ');
    at_line_start = false;
  };
  auto emit_newline = [&]() {
    trim_trailing_spaces(out);
    if (!out.empty() && out.back() != u'\n') {
      out.push_back(u'\n');
    }
    at_line_start = true;
    pending_space = false;
  };

  size_t i = 0;
  while (i < n) {
    const char16_t c = input[i];
    if (is_space(c)) {
      if (!at_line_start) {
        pending_space = true;
      }
      ++i;
      continue;
    }
    // Preserve block comments verbatim on their own line when at line start.
    if (c == u'/' && i + 1 < n && input[i + 1] == u'*') {
      const size_t start = i;
      i += 2;
      while (i + 1 < n && !(input[i] == u'*' && input[i + 1] == u'/')) {
        ++i;
      }
      i = (i + 1 < n) ? i + 2 : n;
      if (at_line_start) {
        emit_indent();
      } else if (pending_space) {
        out.push_back(u' ');
      }
      pending_space = false;
      out.append(input.substr(start, i - start));
      emit_newline();
      continue;
    }
    // Preserve quoted strings verbatim.
    if (c == u'"' || c == u'\'') {
      const char16_t quote = c;
      const size_t start = i++;
      while (i < n) {
        if (input[i] == u'\\' && i + 1 < n) {
          i += 2;
          continue;
        }
        if (input[i] == quote) {
          ++i;
          break;
        }
        ++i;
      }
      if (at_line_start) {
        emit_indent();
      } else if (pending_space) {
        out.push_back(u' ');
      }
      pending_space = false;
      out.append(input.substr(start, i - start));
      continue;
    }
    if (c == u'{') {
      trim_trailing_spaces(out);
      if (!at_line_start) {
        out.push_back(u' ');
      } else {
        emit_indent();
      }
      out.push_back(u'{');
      ++indent;
      emit_newline();
      ++i;
      continue;
    }
    if (c == u'}') {
      indent = std::max(0, indent - 1);
      if (!at_line_start) {
        emit_newline();
      }
      emit_indent();
      out.push_back(u'}');
      emit_newline();
      if (indent == 0) {
        out.push_back(u'\n');
      }
      ++i;
      continue;
    }
    if (c == u';') {
      trim_trailing_spaces(out);
      if (at_line_start) {
        emit_indent();
      }
      out.push_back(u';');
      emit_newline();
      ++i;
      continue;
    }
    if (c == u':' && indent > 0) {
      trim_trailing_spaces(out);
      if (at_line_start) {
        emit_indent();
      }
      out.append(u": ");
      pending_space = false;
      ++i;
      continue;
    }
    if (c == u',' && indent == 0) {
      trim_trailing_spaces(out);
      if (at_line_start) {
        emit_indent();
      }
      out.append(u", ");
      pending_space = false;
      ++i;
      continue;
    }
    if (at_line_start) {
      emit_indent();
    } else if (pending_space && !out.empty() && out.back() != u' ' &&
               out.back() != u'(' && c != u')') {
      out.push_back(u' ');
    }
    pending_space = false;
    out.push_back(c);
    ++i;
  }

  while (!out.empty() && (out.back() == u'\n' || out.back() == u' ')) {
    out.pop_back();
  }
  if (!out.empty()) {
    out.push_back(u'\n');
  }
  return out;
}

struct CssCompletionResult {
  size_t replace_start = 0;
  bool is_property = false;
  std::vector<std::u16string> items;
};

// Lightweight CSS property & value autocomplete for the Boost Code editor.
CssCompletionResult ComputeCssCompletions(std::u16string_view text,
                                          size_t caret) {
  CssCompletionResult result;
  if (caret == 0 || caret > text.size() || text.size() > 65536) {
    return result;
  }
  // Must be inside a declaration block (brace_depth > 0) and not in a comment.
  int brace_depth = 0;
  bool in_comment = false;
  bool in_string = false;
  char16_t quote = 0;
  for (size_t i = 0; i < caret; ++i) {
    const char16_t c = text[i];
    if (in_comment) {
      if (c == u'*' && i + 1 < caret && text[i + 1] == u'/') {
        in_comment = false;
        ++i;
      }
      continue;
    }
    if (in_string) {
      if (c == u'\\') {
        ++i;
      } else if (c == quote) {
        in_string = false;
      }
      continue;
    }
    if (c == u'/' && i + 1 < caret && text[i + 1] == u'*') {
      in_comment = true;
      ++i;
    } else if (c == u'"' || c == u'\'') {
      in_string = true;
      quote = c;
    } else if (c == u'{') {
      ++brace_depth;
    } else if (c == u'}') {
      brace_depth = std::max(0, brace_depth - 1);
    }
  }
  if (brace_depth <= 0 || in_comment || in_string) {
    return result;
  }

  // Scan back to start of current declaration (after '{', '}', ';', or '\n').
  size_t decl_start = caret;
  while (decl_start > 0 && text[decl_start - 1] != u'{' &&
         text[decl_start - 1] != u'}' && text[decl_start - 1] != u';' &&
         text[decl_start - 1] != u'\n') {
    --decl_start;
  }
  const std::u16string_view decl = text.substr(decl_start, caret - decl_start);
  const size_t colon = decl.find(u':');

  auto is_tok = [](char16_t c) {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') ||
           (c >= u'0' && c <= u'9') || c == u'-' || c == u'!';
  };
  size_t tok_start = caret;
  while (tok_start > decl_start && is_tok(text[tok_start - 1])) {
    --tok_start;
  }
  if (tok_start == caret) {
    return result;
  }
  std::u16string prefix =
      base::ToLowerASCII(text.substr(tok_start, caret - tok_start));
  result.replace_start = tok_start;

  if (colon == std::u16string_view::npos) {
    result.is_property = true;
    static constexpr const char16_t* kCssProps[] = {
        u"accent-color",     u"align-content",    u"align-items",
        u"align-self",       u"all",              u"animation",
        u"aspect-ratio",     u"backdrop-filter",  u"background",
        u"background-color", u"background-image", u"background-size",
        u"border",           u"border-bottom",    u"border-color",
        u"border-left",      u"border-radius",    u"border-right",
        u"border-top",       u"bottom",           u"box-shadow",
        u"box-sizing",       u"color",            u"column-gap",
        u"content",          u"cursor",           u"display",
        u"fill",             u"filter",           u"flex",
        u"flex-direction",   u"flex-grow",        u"flex-shrink",
        u"flex-wrap",        u"float",            u"font",
        u"font-family",      u"font-size",        u"font-style",
        u"font-weight",      u"gap",              u"grid",
        u"grid-template-columns", u"height",      u"inset",
        u"justify-content",  u"left",             u"letter-spacing",
        u"line-height",      u"list-style",       u"margin",
        u"margin-bottom",    u"margin-left",      u"margin-right",
        u"margin-top",       u"max-height",       u"max-width",
        u"min-height",       u"min-width",        u"object-fit",
        u"opacity",          u"outline",          u"overflow",
        u"overflow-x",       u"overflow-y",       u"padding",
        u"padding-bottom",   u"padding-left",     u"padding-right",
        u"padding-top",      u"pointer-events",   u"position",
        u"right",            u"row-gap",          u"text-align",
        u"text-decoration",  u"text-overflow",    u"text-transform",
        u"top",              u"transform",        u"transition",
        u"user-select",      u"visibility",       u"white-space",
        u"width",            u"z-index",
    };
    for (const char16_t* prop : kCssProps) {
      std::u16string_view sv(prop);
      if (sv.starts_with(prefix) && sv != prefix) {
        result.items.emplace_back(sv);
        if (result.items.size() == 3) {
          break;
        }
      }
    }
    return result;
  }

  // Value completion (after ':').
  result.is_property = false;
  if (prefix[0] == u'!') {
    if (std::u16string_view(u"!important").starts_with(prefix) &&
        prefix != u"!important") {
      result.items.emplace_back(u"!important");
    }
    return result;
  }

  // Extract property name before ':'.
  std::u16string_view raw_prop = decl.substr(0, colon);
  while (!raw_prop.empty() && (raw_prop.front() == u' ' || raw_prop.front() == u'\t')) {
    raw_prop.remove_prefix(1);
  }
  while (!raw_prop.empty() && (raw_prop.back() == u' ' || raw_prop.back() == u'\t')) {
    raw_prop.remove_suffix(1);
  }
  const std::u16string prop = base::ToLowerASCII(raw_prop);

  auto add_matching = [&](std::initializer_list<const char16_t*> candidates) {
    for (const char16_t* c : candidates) {
      std::u16string_view sv(c);
      if (sv.starts_with(prefix) && sv != prefix) {
        result.items.emplace_back(sv);
        if (result.items.size() == 3) {
          return;
        }
      }
    }
  };

  if (prop == u"display") {
    add_matching({u"none", u"block", u"inline", u"inline-block", u"flex",
                  u"inline-flex", u"grid", u"contents"});
  } else if (prop == u"position") {
    add_matching({u"relative", u"absolute", u"fixed", u"sticky", u"static"});
  } else if (prop == u"text-decoration") {
    add_matching({u"none", u"underline", u"line-through", u"overline"});
  } else if (prop == u"text-align") {
    add_matching({u"left", u"center", u"right", u"justify"});
  } else if (prop == u"text-transform") {
    add_matching({u"none", u"uppercase", u"lowercase", u"capitalize"});
  } else if (prop == u"font-weight") {
    add_matching({u"normal", u"bold", u"lighter", u"bolder"});
  } else if (prop == u"font-style") {
    add_matching({u"normal", u"italic", u"oblique"});
  } else if (prop == u"overflow" || prop == u"overflow-x" ||
             prop == u"overflow-y") {
    add_matching({u"hidden", u"auto", u"scroll", u"visible", u"clip"});
  } else if (prop == u"cursor") {
    add_matching({u"pointer", u"default", u"auto", u"not-allowed", u"grab"});
  } else if (prop == u"align-items" || prop == u"align-self") {
    add_matching({u"center", u"flex-start", u"flex-end", u"stretch",
                  u"baseline"});
  } else if (prop == u"justify-content") {
    add_matching({u"center", u"space-between", u"space-around", u"space-evenly",
                  u"flex-start", u"flex-end"});
  } else if (prop == u"flex-direction") {
    add_matching({u"row", u"column", u"row-reverse", u"column-reverse"});
  } else if (prop == u"visibility") {
    add_matching({u"hidden", u"visible", u"collapse"});
  } else if (prop == u"pointer-events") {
    add_matching({u"none", u"auto"});
  } else if (prop == u"box-sizing") {
    add_matching({u"border-box", u"content-box"});
  } else if (prop == u"white-space") {
    add_matching({u"nowrap", u"normal", u"pre", u"pre-wrap"});
  } else if (prop == u"color" || prop == u"background" ||
             prop == u"background-color" || prop == u"border-color") {
    add_matching({u"transparent", u"currentColor", u"inherit", u"white",
                  u"black"});
  } else {
    add_matching({u"none", u"auto", u"inherit", u"initial", u"unset"});
  }
  return result;
}

// Small circular swatch beside the editable hex color field.
class ColorSwatchDot final : public views::View {
  METADATA_HEADER(ColorSwatchDot, views::View)

 public:
  ColorSwatchDot() { SetPreferredSize(gfx::Size(20, 20)); }

  void SetColor(std::optional<SkColor> color) {
    color_ = color;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::PointF center(width() * 0.5f, height() * 0.5f);
    const float r = 7.f;

    cc::PaintFlags shadow;
    shadow.setAntiAlias(true);
    shadow.setColor(SkColorSetA(SK_ColorBLACK, 0x45));
    canvas->DrawCircle(gfx::PointF(center.x(), center.y() + 1.f), r + 1.25f,
                       shadow);

    cc::PaintFlags fill;
    fill.setAntiAlias(true);
    fill.setColor(color_.value_or(SK_ColorWHITE));
    canvas->DrawCircle(center, r, fill);

    cc::PaintFlags ring;
    ring.setAntiAlias(true);
    ring.setStyle(cc::PaintFlags::kStroke_Style);
    ring.setStrokeWidth(1.5f);
    ring.setColor(SK_ColorWHITE);
    canvas->DrawCircle(center, r, ring);
  }

 private:
  std::optional<SkColor> color_;
};

BEGIN_METADATA(ColorSwatchDot)
END_METADATA

class BoostEditorView;

class BoostEditorBubbleDelegate final : public views::BubbleDialogDelegate {
 public:
  explicit BoostEditorBubbleDelegate(views::View* anchor)
      : views::BubbleDialogDelegate(anchor, views::BubbleBorder::TOP_RIGHT) {}

  gfx::Rect GetAnchorRect() const override {
    const gfx::Rect rect = views::BubbleDialogDelegate::GetAnchorRect();
    if (rect.IsEmpty()) {
      return rect;
    }
    // Hang the floating card inside the top-right corner of the page card.
    return gfx::Rect(rect.right() - 16, rect.y() + 16, 1, 1);
  }
};

struct ActiveEditorSlot {
  std::unique_ptr<views::BubbleDialogDelegate> delegate;
  std::unique_ptr<views::Widget> widget;
  raw_ptr<BoostEditorView> view = nullptr;
};

std::map<BrowserWindowInterface*, std::unique_ptr<ActiveEditorSlot>>&
ActiveEditors() {
  static base::NoDestructor<
      std::map<BrowserWindowInterface*, std::unique_ptr<ActiveEditorSlot>>>
      editors;
  return *editors;
}

void ReleaseEditorSlot(std::unique_ptr<ActiveEditorSlot> slot) {
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

class BoostEditorView final : public views::View,
                              public content::WebContentsObserver,
                              public BoostsService::Observer,
                              public views::TextfieldController,
                              public views::SliderListener,
                              public gfx::AnimationDelegate,
                              public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(BoostEditorView, views::View)

 public:
  enum TitleCommand {
    kCmdRename = 1,
    kCmdShuffle,
    kCmdReset,
    kCmdDelete,
    kCmdAllBoosts,
    kCmdToggleEnabled,
    kCmdToggleSubdomains,
    kCmdToggleSubframes,
    kCmdUndo,
  };

  BoostEditorView(BrowserWindowInterface& browser,
                  views::BubbleDialogDelegate* bubble_delegate,
                  content::WebContents* contents,
                  BoostsService& service,
                  uint64_t boost_id,
                  std::string origin)
      : content::WebContentsObserver(contents),
        browser_(&browser),
        bubble_delegate_(bubble_delegate),
        service_(&service),
        boost_id_(boost_id),
        origin_(std::move(origin)) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kVertical, gfx::Insets(), 10));

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    bounds_anim_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    bounds_anim_.SetSlideDuration(base::Milliseconds(180));
    fade_anim_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    fade_anim_.SetSlideDuration(base::Milliseconds(160));

    service_observation_.Observe(service_.get());

    BuildUi();
    ReloadFromService();
  }

  ~BoostEditorView() override {
    bounds_anim_.Stop();
    fade_anim_.Stop();
    StopZapPicker();
    ClearPreview();
    service_observation_.Reset();
    menu_runner_.reset();
    menu_model_.reset();
    bubble_delegate_ = nullptr;
    browser_ = nullptr;
    service_ = nullptr;
    ClearChildPointers();
  }

  void StartOpenAnimation() {
    if (!layer()) {
      return;
    }
    fade_anim_.Stop();
    fade_anim_.Reset(0.0);
    layer()->SetOpacity(0.0f);
    fade_anim_.Show();
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    if (animation == &fade_anim_) {
      if (layer()) {
        const double v = fade_anim_.GetCurrentValue();
        layer()->SetOpacity(static_cast<float>(
            fade_from_opacity_ + (1.0 - fade_from_opacity_) * v));
      }
      return;
    }
    if (animation == &bounds_anim_) {
      if (views::Widget* widget = GetWidget(); widget && !widget->IsClosed()) {
        const gfx::Rect current = gfx::Tween::RectValueBetween(
            bounds_anim_.GetCurrentValue(), anim_start_bounds_,
            anim_target_bounds_);
        widget->SetBounds(current);
      }
    }
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    AnimationProgressed(animation);
    if (animation == &fade_anim_) {
      fade_from_opacity_ = 0.0;
    }
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int target_width = code_mode_ ? kCodeCardWidth : kCardWidth;
    const gfx::Size size = views::View::CalculatePreferredSize(
        views::SizeBounds(target_width, available_size.height()));
    return gfx::Size(target_width, size.height());
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (hex_input_) {
      hex_input_->SetColor(kInkPrimary);
    }
    if (zap_input_) {
      zap_input_->SetColor(kInkPrimary);
    }
    if (rename_field_) {
      rename_field_->SetColor(kInkPrimary);
    }
    if (css_editor_) {
      ApplyCssSyntaxHighlighting(css_editor_);
    }
  }

  uint64_t boost_id() const { return boost_id_; }
  bool is_hidden_by_tab_switch() const { return hidden_by_tab_switch_; }

  void OnWindowClosing() {
    bounds_anim_.Stop();
    fade_anim_.Stop();
    css_autosave_timer_.Stop();
    StopZapPicker();
    CommitPendingCssIfValid();
    ClearPreview();
    Observe(nullptr);
    service_observation_.Reset();
    menu_runner_.reset();
    menu_model_.reset();
    bubble_delegate_ = nullptr;
    BrowserWindowInterface* browser = browser_;
    browser_ = nullptr;
    service_ = nullptr;
    if (browser) {
      if (auto it = ActiveEditors().find(browser);
          it != ActiveEditors().end() && it->second &&
          it->second->view == this) {
        auto slot = std::move(it->second);
        ActiveEditors().erase(it);
        ReleaseEditorSlot(std::move(slot));
      }
    }
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_ESCAPE) {
      if (code_mode_) {
        ExitCodeMode();
        return true;
      }
      CloseEditor();
      return true;
    }
    return views::View::OnKeyPressed(event);
  }

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted() ||
        !navigation_handle->IsInPrimaryMainFrame() ||
        navigation_handle->IsSameDocument()) {
      return;
    }
    const url::Origin next_origin = ActiveOriginForContents(web_contents());
    if (!IsEligibleHttpOrigin(next_origin) ||
        next_origin.Serialize() != origin_) {
      CloseEditor();
      return;
    }
    if (zap_picker_active_) {
      StopZapPicker();
      if (zap_section_) {
        zap_section_->SetVisible(false);
      }
      if (zap_toggle_) {
        zap_toggle_->SetActive(false);
      }
      SizeToContents();
    }
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    views::Widget* widget = GetWidget();
    if (!widget || widget->IsClosed()) {
      return;
    }
    if (visibility == content::Visibility::HIDDEN) {
      TabStripModel* tab_strip = browser_->GetTabStripModel();
      if (!tab_strip || tab_strip->GetActiveWebContents() != web_contents()) {
        if (zap_picker_active_) {
          StopZapPicker();
          if (zap_section_) {
            zap_section_->SetVisible(false);
          }
          if (zap_toggle_) {
            zap_toggle_->SetActive(false);
          }
          if (code_picker_button_) {
            code_picker_button_->SetActive(false);
          }
        }
        CommitPendingCssIfValid();
        hidden_by_tab_switch_ = true;
        widget->Hide();
      }
    } else if (visibility == content::Visibility::VISIBLE &&
               hidden_by_tab_switch_) {
      hidden_by_tab_switch_ = false;
      widget->ShowInactive();
    }
  }

  void WebContentsDestroyed() override {
    Observe(nullptr);
    CloseEditor();
  }

  // BoostsService::Observer:
  void OnBoostsChanged() override {
    if (suppress_reload_) {
      return;
    }
    ReloadFromService();
  }

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    if (sender == css_editor_) {
      draft_css_ = base::UTF16ToUTF8(new_contents);
      css_dirty_ = true;
      ApplyCssSyntaxHighlighting(css_editor_);
      UpdateCssSuggestions();
      UpdateCssValidationUi();
      PushLivePreview();
      ScheduleCssAutoSave();
    } else if (sender == hex_input_) {
      if (suppress_hex_change_) {
        return;
      }
      const std::string raw = base::UTF16ToUTF8(new_contents);
      std::string_view trimmed = base::TrimWhitespaceASCII(raw, base::TRIM_ALL);
      if (!trimmed.empty() && trimmed.front() == '#') {
        trimmed.remove_prefix(1);
      }
      if (trimmed.size() == 6) {
        if (auto rgb = ParseHexColor(raw)) {
          ApplyHexColor(*rgb, /*commit=*/false);
        }
      }
    } else if (sender == zap_input_) {
      if (zap_error_label_) {
        zap_error_label_->SetVisible(false);
      }
      PushLivePreview();
    }
  }

  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::EventType::kKeyPressed) {
      return false;
    }
    if (key_event.key_code() == ui::VKEY_ESCAPE) {
      if (sender == rename_field_) {
        EndInlineRename(/*commit=*/false);
        return true;
      }
      if (sender == hex_input_) {
        SyncHexFieldFromDraft(/*force=*/true);
        return true;
      }
      if (sender == css_editor_ && !css_completion_.items.empty()) {
        ClearCssSuggestions();
        return true;
      }
      if (code_mode_) {
        ExitCodeMode();
        return true;
      }
      CloseEditor();
      return true;
    }
    if (sender == css_editor_ && !css_completion_.items.empty() &&
        !key_event.IsCommandDown() && !key_event.IsControlDown()) {
      if (key_event.key_code() == ui::VKEY_TAB && !key_event.IsShiftDown()) {
        AcceptCssSuggestion(css_completion_index_);
        return true;
      }
      if (key_event.key_code() == ui::VKEY_DOWN) {
        css_completion_index_ =
            (css_completion_index_ + 1) % css_completion_.items.size();
        RebuildCssSuggestionChips();
        return true;
      }
      if (key_event.key_code() == ui::VKEY_UP) {
        css_completion_index_ =
            (css_completion_index_ + css_completion_.items.size() - 1) %
            css_completion_.items.size();
        RebuildCssSuggestionChips();
        return true;
      }
    }
    if (sender == css_editor_ && key_event.key_code() == ui::VKEY_S &&
        key_event.IsCommandDown()) {
      CommitPendingCssIfValid(/*format_now=*/true);
      return true;
    }
    if (key_event.key_code() == ui::VKEY_RETURN) {
      if (sender == rename_field_) {
        EndInlineRename(/*commit=*/true);
        return true;
      }
      if (sender == hex_input_) {
        CommitHexInput();
        return true;
      }
      if (sender == zap_input_) {
        AddZapSelector();
        return true;
      }
      if (sender == css_editor_ && !key_event.IsCommandDown() &&
          !key_event.IsControlDown() && !key_event.IsAltDown()) {
        return HandleCssReturnKey();
      }
    }
    if (sender == css_editor_ && key_event.key_code() == ui::VKEY_TAB &&
        !key_event.IsCommandDown() && !key_event.IsControlDown()) {
      return HandleCssTabKey(key_event.IsShiftDown());
    }
    if (sender == css_editor_ && key_event.GetCharacter() == u'}' &&
        !key_event.IsCommandDown() && !key_event.IsControlDown()) {
      return HandleCssCloseBraceKey();
    }
    return false;
  }

  void ApplyCssProgrammaticEdit(const std::u16string& next_text,
                                const gfx::Range& next_selection) {
    if (!css_editor_) {
      return;
    }
    css_editor_->SetText(next_text);
    css_editor_->SetSelectedRange(next_selection);
    draft_css_ = base::UTF16ToUTF8(next_text);
    css_dirty_ = true;
    ApplyCssSyntaxHighlighting(css_editor_);
    UpdateCssValidationUi();
    PushLivePreview();
    ScheduleCssAutoSave();
  }

  bool HandleCssTabKey(bool shift) {
    if (!css_editor_) {
      return false;
    }
    const std::u16string text = std::u16string(css_editor_->GetText());
    const gfx::Range sel = css_editor_->GetSelectedRange();
    const size_t min_pos = sel.GetMin();
    const size_t max_pos = sel.GetMax();

    if (!shift && min_pos == max_pos) {
      css_editor_->InsertOrReplaceText(u"  ");
      return true;
    }

    // Find start of first selected line.
    size_t line_start = min_pos;
    while (line_start > 0 && text[line_start - 1] != u'\n') {
      --line_start;
    }
    // If selection ends at column 0 of a later line, don't touch that line.
    size_t scan_end = max_pos;
    if (scan_end > min_pos && scan_end > 0 && text[scan_end - 1] == u'\n') {
      --scan_end;
    }

    std::u16string out;
    out.reserve(text.size() + 32);
    out.append(text, 0, line_start);

    size_t new_start = min_pos;
    size_t new_end = max_pos;
    bool first_line = true;
    size_t pos = line_start;
    while (pos <= text.size()) {
      size_t nl = text.find(u'\n', pos);
      const size_t line_end = (nl == std::u16string::npos) ? text.size() : nl;
      if (!shift) {
        out.append(u"  ");
        if (first_line) {
          new_start += 2;
        }
        new_end += 2;
        out.append(text, pos, line_end - pos);
      } else {
        size_t removed = 0;
        if (pos < line_end && text[pos] == u'\t') {
          removed = 1;
        } else {
          while (removed < 2 && pos + removed < line_end &&
                 text[pos + removed] == u' ') {
            ++removed;
          }
        }
        if (first_line) {
          const size_t offset_in_line = min_pos - pos;
          new_start -= std::min(removed, offset_in_line);
        }
        new_end -= removed;
        out.append(text, pos + removed, line_end - (pos + removed));
      }
      first_line = false;
      if (nl == std::u16string::npos) {
        break;
      }
      out.push_back(u'\n');
      pos = nl + 1;
      if (pos > scan_end) {
        out.append(text, pos, text.size() - pos);
        break;
      }
    }

    if (sel.is_reversed()) {
      ApplyCssProgrammaticEdit(out, gfx::Range(new_end, new_start));
    } else {
      ApplyCssProgrammaticEdit(out, gfx::Range(new_start, new_end));
    }
    return true;
  }

  bool HandleCssReturnKey() {
    if (!css_editor_) {
      return false;
    }
    const std::u16string text = std::u16string(css_editor_->GetText());
    const gfx::Range sel = css_editor_->GetSelectedRange();
    const size_t start = sel.GetMin();
    const size_t end = sel.GetMax();

    size_t line_start = start;
    while (line_start > 0 && text[line_start - 1] != u'\n') {
      --line_start;
    }
    size_t indent_len = 0;
    while (line_start + indent_len < start &&
           (text[line_start + indent_len] == u' ' ||
            text[line_start + indent_len] == u'\t')) {
      ++indent_len;
    }
    const std::u16string indent = text.substr(line_start, indent_len);

    // Check last non-space char before caret on this line.
    size_t prev = start;
    while (prev > line_start &&
           (text[prev - 1] == u' ' || text[prev - 1] == u'\t')) {
      --prev;
    }
    const bool after_open_brace = prev > line_start && text[prev - 1] == u'{';
    const bool before_close_brace = end < text.size() && text[end] == u'}';

    std::u16string insert = u"\n" + indent;
    if (after_open_brace) {
      insert += u"  ";
    }
    size_t caret = start + insert.size();
    if (after_open_brace && before_close_brace) {
      insert += u"\n" + indent;
    }

    std::u16string next =
        text.substr(0, start) + insert + text.substr(end);
    ApplyCssProgrammaticEdit(next, gfx::Range(caret));
    return true;
  }

  bool HandleCssCloseBraceKey() {
    if (!css_editor_) {
      return false;
    }
    const gfx::Range sel = css_editor_->GetSelectedRange();
    if (!sel.is_empty()) {
      return false;
    }
    const std::u16string text = std::u16string(css_editor_->GetText());
    const size_t pos = sel.start();
    size_t line_start = pos;
    while (line_start > 0 && text[line_start - 1] != u'\n') {
      --line_start;
    }
    // Only auto-outdent if the line before the caret is purely >= 2 spaces.
    if (pos - line_start < 2) {
      return false;
    }
    for (size_t i = line_start; i < pos; ++i) {
      if (text[i] != u' ') {
        return false;
      }
    }
    std::u16string next =
        text.substr(0, pos - 2) + u"}" + text.substr(pos);
    ApplyCssProgrammaticEdit(next, gfx::Range(pos - 1));
    return true;
  }

  // views::SliderListener:
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override {
    if (reason != views::SliderChangeReason::kByUser) {
      return;
    }
    EnsurePaletteForSliders();
    const uint8_t v =
        static_cast<uint8_t>(std::round(std::clamp(value, 0.f, 1.f) * 100.f));
    if (sender == contrast_slider_) {
      draft_appearance_.palette.contrast = v;
    } else if (sender == brightness_slider_) {
      draft_appearance_.palette.brightness = v;
    } else if (sender == saturation_slider_) {
      draft_appearance_.palette.saturation = v;
    }
    PushLivePreview();
    if (!slider_dragging_) {
      CommitAppearance();
    }
  }

  void SliderDragStarted(views::Slider* sender) override {
    slider_dragging_ = true;
  }

  void SliderDragEnded(views::Slider* sender) override {
    slider_dragging_ = false;
    CommitAppearance();
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    if (!record_) {
      return false;
    }
    switch (command_id) {
      case kCmdToggleEnabled:
        return record_->enabled;
      case kCmdToggleSubdomains:
        return record_->include_subdomains;
      case kCmdToggleSubframes:
        return record_->apply_to_subframes;
      default:
        return false;
    }
  }

  bool IsCommandIdEnabled(int command_id) const override {
    if (!record_) {
      return false;
    }
    if (command_id == kCmdUndo) {
      return record_->can_undo;
    }
    if (command_id == kCmdReset) {
      return HasAnyEdits();
    }
    return true;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    if (!record_) {
      return;
    }
    switch (command_id) {
      case kCmdRename:
        BeginInlineRename();
        break;
      case kCmdShuffle:
        ShuffleBoost();
        break;
      case kCmdReset:
        OnResetPressed();
        break;
      case kCmdDelete:
        StopZapPicker();
        ClearPreview();
        service_->DeleteBoost(boost_id_);
        CloseEditor();
        break;
      case kCmdAllBoosts: {
        BrowserWindowInterface* browser = browser_;
        CloseEditor();
        if (browser) {
          chrome::ExecuteCommand(browser, IDC_MYARC_VIEW_BOOSTS);
        }
        break;
      }
      case kCmdToggleEnabled:
        service_->SetBoostEnabled(boost_id_, !record_->enabled);
        break;
      case kCmdToggleSubdomains: {
        std::vector<std::string> origins;
        for (const auto& o : record_->origins) {
          origins.emplace_back(o.data(), o.size());
        }
        service_->SetBoostScope(boost_id_, origins,
                                !record_->include_subdomains,
                                record_->apply_to_subframes);
        break;
      }
      case kCmdToggleSubframes: {
        std::vector<std::string> origins;
        for (const auto& o : record_->origins) {
          origins.emplace_back(o.data(), o.size());
        }
        service_->SetBoostScope(boost_id_, origins, record_->include_subdomains,
                                !record_->apply_to_subframes);
        break;
      }
      case kCmdUndo:
        ClearPreview();
        service_->UndoBoost(boost_id_);
        break;
    }
  }

 private:
  void ClearChildPointers() {
    main_panel_ = nullptr;
    code_panel_ = nullptr;
    title_button_ = nullptr;
    rename_field_ = nullptr;
    color_pad_ = nullptr;
    hex_swatch_ = nullptr;
    hex_input_ = nullptr;
    mode_invert_ = nullptr;
    mode_palette_ = nullptr;
    mode_adjust_ = nullptr;
    mode_none_ = nullptr;
    adjust_section_ = nullptr;
    contrast_slider_ = nullptr;
    brightness_slider_ = nullptr;
    saturation_slider_ = nullptr;
    font_cells_.fill(nullptr);
    size_button_ = nullptr;
    case_button_ = nullptr;
    zap_toggle_ = nullptr;
    zap_section_ = nullptr;
    zap_input_ = nullptr;
    zap_error_label_ = nullptr;
    zap_list_ = nullptr;
    code_toggle_ = nullptr;
    code_picker_button_ = nullptr;
    css_editor_ = nullptr;
    css_banner_ = nullptr;
    css_bytes_label_ = nullptr;
    css_suggestions_row_ = nullptr;
  }

  void BuildUi() {
    auto style_light_textfield = [](views::Textfield* tf) {
      tf->SetBackgroundColor(kPillBg);
      tf->SetColor(kInkPrimary);
      tf->SetBorder(views::CreatePaddedBorder(
          views::CreateRoundedRectBorder(1, 8, kBorderSubtle),
          gfx::Insets::VH(4, 8)));
    };

    main_panel_ = AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets(), 10));

    // 1. Header row: Close (x), Title dropdown / rename, Reset (↺).
    auto* header = main_panel_->AddChildView(
        MakeBox(Orientation::kHorizontal, gfx::Insets::TLBR(0, 0, 2, 0), 4));
    auto* header_layout =
        static_cast<views::BoxLayout*>(header->GetLayoutManager());
    header_layout->set_cross_axis_alignment(CrossAxis::kCenter);

    auto make_close_button = [&]() {
      auto close = std::make_unique<views::ImageButton>(base::BindRepeating(
          &BoostEditorView::CloseEditor, base::Unretained(this)));
      close->SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(kPeekCloseIcon, SK_ColorWHITE, 12));
      close->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
      close->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      close->SetPreferredSize(gfx::Size(24, 24));
      close->SetBackground(views::CreateRoundedRectBackground(
          SkColorSetARGB(0x2E, 0xFF, 0xFF, 0xFF), 12));
      close->SetBorder(views::CreateRoundedRectBorder(1, 12, kBorderSubtle));
      close->SetTooltipText(u"Close Boost Editor");
      close->GetViewAccessibility().SetName(u"Close Boost Editor");
      views::InstallCircleHighlightPathGenerator(close.get());
      return close;
    };
    header->AddChildView(make_close_button());

    title_button_ = header->AddChildView(std::make_unique<TitleMenuButton>(
        base::BindRepeating(&BoostEditorView::ShowTitleMenu,
                            base::Unretained(this))));
    header_layout->SetFlexForView(title_button_, 1);

    auto rename = std::make_unique<views::Textfield>();
    rename->SetVisible(false);
    rename->set_controller(this);
    rename->SetPlaceholderText(u"Boost name");
    style_light_textfield(rename.get());
    rename_field_ = header->AddChildView(std::move(rename));
    header_layout->SetFlexForView(rename_field_, 1);

    auto reset = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&BoostEditorView::OnResetPressed,
                            base::Unretained(this)),
        u"\u21BA");
    reset->SetStyle(ui::ButtonStyle::kText);
    reset->SetBgColorOverrideDeprecated(SK_ColorTRANSPARENT);
    reset->SetEnabledTextColors(kInkSecondary);
    reset->SetCustomPadding(gfx::Insets::VH(2, 6));
    reset->SetMinSize(gfx::Size(26, 26));
    reset->SetTooltipText(u"Reset Boost");
    reset->GetViewAccessibility().SetName(u"Reset Boost");
    header->AddChildView(std::move(reset));

    // 2. 2D Color / Contrast Pad.
    color_pad_ = main_panel_->AddChildView(std::make_unique<ColorPadView>(
        base::BindRepeating(&BoostEditorView::OnPadChanged,
                            base::Unretained(this))));

    // 2b. Compact hex color readout & editable input.
    auto* hex_row = main_panel_->AddChildView(MakeBox(
        Orientation::kHorizontal, gfx::Insets::VH(3, 10), 8));
    hex_row->SetBackground(views::CreateRoundedRectBackground(kPillBg, 10));
    hex_row->SetBorder(views::CreateRoundedRectBorder(1, 10, kPillBorder));
    auto* hex_layout =
        static_cast<views::BoxLayout*>(hex_row->GetLayoutManager());
    hex_layout->set_cross_axis_alignment(CrossAxis::kCenter);

    hex_swatch_ = hex_row->AddChildView(std::make_unique<ColorSwatchDot>());

    auto hex_tf = std::make_unique<views::Textfield>();
    hex_tf->SetFontList(gfx::FontList(gfx::Font("Menlo", 12)));
    hex_tf->SetPlaceholderText(u"#rrggbb");
    hex_tf->GetViewAccessibility().SetName(u"Boost hex color");
    hex_tf->SetBackgroundColor(SK_ColorTRANSPARENT);
    hex_tf->SetColor(kInkPrimary);
    hex_tf->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(2, 0)));
    hex_tf->set_controller(this);
    hex_input_ = hex_row->AddChildView(std::move(hex_tf));
    hex_layout->SetFlexForView(hex_input_, 1);

    // 3. Four lighting / color mode buttons: Invert, Palette, Adjust, Reset.
    auto* modes = main_panel_->AddChildView(
        MakeBox(Orientation::kHorizontal, gfx::Insets(), 6));
    auto* modes_layout =
        static_cast<views::BoxLayout*>(modes->GetLayoutManager());
    mode_invert_ = modes->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::OnInvertPressed,
                            base::Unretained(this)),
        u"\u25D0", std::u16string(), /*centered=*/true, /*font_size_delta=*/3));
    mode_invert_->SetTooltipText(u"Invert colors");
    mode_invert_->GetViewAccessibility().SetName(u"Invert colors");
    modes_layout->SetFlexForView(mode_invert_, 1);

    mode_palette_ = modes->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::SetColorMode,
                            base::Unretained(this),
                            foundation::BoostColorMode::Palette),
        u"Color palette", std::u16string(), /*centered=*/true,
        /*font_size_delta=*/0, &kPaletteIcon));
    mode_palette_->SetTooltipText(u"Color palette");
    mode_palette_->GetViewAccessibility().SetName(u"Color palette");
    modes_layout->SetFlexForView(mode_palette_, 1);

    mode_adjust_ = modes->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::ToggleAdjustSection,
                            base::Unretained(this)),
        u"Adjust contrast, brightness, and saturation", std::u16string(),
        /*centered=*/true, /*font_size_delta=*/0, &kSlidersHorizontalIcon));
    mode_adjust_->SetTooltipText(u"Contrast, Brightness & Saturation");
    mode_adjust_->GetViewAccessibility().SetName(
        u"Contrast, Brightness, and Saturation");
    modes_layout->SetFlexForView(mode_adjust_, 1);

    // 3b. Collapsible Contrast / Brightness / Original Saturation sliders.
    adjust_section_ = main_panel_->AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets::VH(10, 12), 8));
    adjust_section_->SetBackground(
        views::CreateRoundedRectBackground(kPillBg, 12));
    adjust_section_->SetVisible(false);
    auto add_slider_row = [&](const std::u16string& title) -> BoostAdjustSlider* {
      auto* col = adjust_section_->AddChildView(
          MakeBox(Orientation::kVertical, gfx::Insets(), 2));
      auto* lbl = col->AddChildView(MakeLabel(
          title,
          gfx::FontList().Derive(-1, gfx::Font::NORMAL,
                                 gfx::Font::Weight::MEDIUM),
          kInkPrimary));
      lbl->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      auto* slider = col->AddChildView(std::make_unique<BoostAdjustSlider>(this));
      slider->GetViewAccessibility().SetName(title);
      slider->SetValue(0.5f);
      return slider;
    };
    contrast_slider_ = add_slider_row(u"Contrast");
    brightness_slider_ = add_slider_row(u"Brightness");
    saturation_slider_ = add_slider_row(u"Original Saturation");

    // 4. 4x4 "Aa" typography preset grid.
    auto* grid_card = main_panel_->AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets(8), 2));
    grid_card->SetBackground(
        views::CreateRoundedRectBackground(kPillBg, 12));
    grid_card->SetBorder(
        views::CreateRoundedRectBorder(1, 12, kBorderSubtle));
    for (size_t row = 0; row < 4; ++row) {
      auto* row_view = grid_card->AddChildView(
          MakeBox(Orientation::kHorizontal, gfx::Insets(), 2));
      auto* row_layout =
          static_cast<views::BoxLayout*>(row_view->GetLayoutManager());
      for (size_t col = 0; col < 4; ++col) {
        const size_t idx = row * 4 + col;
        const auto& info = kFontPresets[idx];
        auto* cell = row_view->AddChildView(std::make_unique<FontPresetCell>(
            base::BindRepeating(&BoostEditorView::SetFontPreset,
                                base::Unretained(this), info.preset),
            info));
        row_layout->SetFlexForView(cell, 1);
        font_cells_[idx] = cell;
      }
    }

    // 5. Size and Case buttons.
    auto* type_row = main_panel_->AddChildView(
        MakeBox(Orientation::kHorizontal, gfx::Insets(), 8));
    auto* type_layout =
        static_cast<views::BoxLayout*>(type_row->GetLayoutManager());
    size_button_ = type_row->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::CycleTextScale,
                            base::Unretained(this)),
        u"Size"));
    size_button_->SetTooltipText(u"Cycle text size (50%\u2013200%)");
    type_layout->SetFlexForView(size_button_, 1);

    case_button_ = type_row->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::CycleTextTransform,
                            base::Unretained(this)),
        u"Case"));
    case_button_->SetTooltipText(u"Cycle text case");
    type_layout->SetFlexForView(case_button_, 1);

    // 6. Zap button + expandable hidden-selectors list.
    zap_toggle_ = main_panel_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::ToggleZapSection,
                            base::Unretained(this)),
        u"Zap", std::u16string(), /*centered=*/false, /*font_size_delta=*/0,
        /*icon=*/nullptr, &kZapIcon));
    zap_toggle_->SetTooltipText(u"Click elements on the page to hide them");

    zap_section_ = main_panel_->AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets::VH(2, 0), 6));
    zap_section_->SetVisible(false);
    zap_list_ = zap_section_->AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets(), 6));

    // 7. Code ({}) button — morphs the card into the spacious Code Editor view.
    code_toggle_ = main_panel_->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::EnterCodeMode,
                            base::Unretained(this)),
        u"Code", std::u16string(), /*centered=*/false, /*font_size_delta=*/0,
        /*icon=*/nullptr, &kBracesIcon));
    code_toggle_->SetTooltipText(u"Edit custom CSS");

    // 8. Morphed Code Editor panel (380px wide, Arc-style modal view).
    code_panel_ = AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets(), 10));
    code_panel_->SetVisible(false);
    {
      auto* code_header = code_panel_->AddChildView(
          MakeBox(Orientation::kHorizontal, gfx::Insets::TLBR(0, 0, 2, 0), 8));
      auto* ch_layout =
          static_cast<views::BoxLayout*>(code_header->GetLayoutManager());
      ch_layout->set_cross_axis_alignment(CrossAxis::kCenter);

      auto back = std::make_unique<views::ImageButton>(base::BindRepeating(
          &BoostEditorView::ExitCodeMode, base::Unretained(this)));
      back->SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(kArrowLeftIcon, SK_ColorWHITE, 12));
      back->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
      back->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      back->SetPreferredSize(gfx::Size(24, 24));
      back->SetBackground(views::CreateRoundedRectBackground(
          SkColorSetARGB(0x2E, 0xFF, 0xFF, 0xFF), 12));
      back->SetBorder(views::CreateRoundedRectBorder(1, 12, kBorderSubtle));
      back->SetTooltipText(u"Back to Boost");
      back->GetViewAccessibility().SetName(u"Back to Boost");
      views::InstallCircleHighlightPathGenerator(back.get());
      code_header->AddChildView(std::move(back));

      auto* spacer_l = code_header->AddChildView(std::make_unique<views::View>());
      ch_layout->SetFlexForView(spacer_l, 1);

      auto* heading = code_header->AddChildView(MakeLabel(
          u"Custom CSS",
          gfx::FontList().Derive(0, gfx::Font::NORMAL,
                                 gfx::Font::Weight::SEMIBOLD),
          kInkPrimary));
      heading->SetHorizontalAlignment(gfx::ALIGN_CENTER);

      auto* spacer_r = code_header->AddChildView(std::make_unique<views::View>());
      ch_layout->SetFlexForView(spacer_r, 1);

      code_header->AddChildView(make_close_button());

      css_banner_ = code_panel_->AddChildView(MakeLabel(
          u"",
          gfx::FontList().Derive(-1, gfx::Font::NORMAL,
                                 gfx::Font::Weight::MEDIUM),
          kWarnText));
      css_banner_->SetMultiLine(true);
      css_banner_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      css_banner_->SetBackground(
          views::CreateRoundedRectBackground(kWarnBg, 8));
      css_banner_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(6, 8)));
      css_banner_->SetVisible(false);

      auto* editor_well = code_panel_->AddChildView(
          MakeBox(Orientation::kVertical, gfx::Insets(10), 0));
      editor_well->SetBackground(
          views::CreateRoundedRectBackground(kCodeWellBg, 10));
      editor_well->SetBorder(
          views::CreateRoundedRectBorder(1, 10, kCodeWellBorder));

      auto editor = std::make_unique<CssCodeTextarea>();
      editor->SetFontList(gfx::FontList(gfx::Font("Menlo", 12)));
      editor->SetPlaceholderText(u"/* Custom CSS */");
      editor->GetViewAccessibility().SetName(u"Custom CSS");
      editor->SetBackgroundColor(SK_ColorTRANSPARENT);
      editor->SetColor(kCodeInk);
      editor->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
      editor->SetPreferredSize(gfx::Size(kCodeCardWidth - 48, 250));
      editor->set_controller(this);
      css_editor_ = editor_well->AddChildView(std::move(editor));

      auto* footer = code_panel_->AddChildView(
          MakeBox(Orientation::kHorizontal, gfx::Insets(), 8));
      auto* footer_layout =
          static_cast<views::BoxLayout*>(footer->GetLayoutManager());
      footer_layout->set_cross_axis_alignment(CrossAxis::kCenter);

      code_picker_button_ = footer->AddChildView(std::make_unique<PillButton>(
          base::BindRepeating(&BoostEditorView::ToggleCodeElementPicker,
                              base::Unretained(this)),
          u"Pick element on page", std::u16string(), /*centered=*/true,
          /*font_size_delta=*/0, &kInspectElementIcon));
      code_picker_button_->SetPreferredSize(gfx::Size(36, 32));
      code_picker_button_->SetTooltipText(u"Pick an element on the page");

      css_bytes_label_ = footer->AddChildView(MakeLabel(
          u"0 / 64 KB \u00B7 Auto-saved",
          gfx::FontList().Derive(-2, gfx::Font::NORMAL,
                                 gfx::Font::Weight::NORMAL),
          kInkSecondary));
      css_bytes_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      footer_layout->SetFlexForView(css_bytes_label_, 1);

      css_suggestions_row_ = footer->AddChildView(
          MakeBox(Orientation::kHorizontal, gfx::Insets(), 4));
      auto* sug_layout = static_cast<views::BoxLayout*>(
          css_suggestions_row_->GetLayoutManager());
      sug_layout->set_cross_axis_alignment(CrossAxis::kCenter);
      css_suggestions_row_->SetVisible(false);
      footer_layout->SetFlexForView(css_suggestions_row_, 1);
    }
  }

  void ReloadFromService() {
    auto maybe = service_->GetBoost(boost_id_);
    if (!maybe) {
      CloseEditor();
      return;
    }
    record_ = std::move(maybe);
    draft_appearance_ = record_->appearance;
    if (!css_dirty_) {
      draft_css_.assign(record_->custom_css.data(), record_->custom_css.size());
      if (css_editor_) {
        css_editor_->SetText(base::UTF8ToUTF16(draft_css_));
        ApplyCssSyntaxHighlighting(css_editor_);
      }
    }

    const std::u16string title_name =
        base::UTF8ToUTF16(std::string(record_->name.data(), record_->name.size()));
    if (title_button_) {
      title_button_->SetTitle(title_name, record_->enabled);
    }

    if (color_pad_) {
      color_pad_->SetState(draft_appearance_.color_mode,
                           draft_appearance_.palette);
    }
    SyncHexFieldFromDraft(/*force=*/false);
    if (mode_invert_) {
      mode_invert_->SetActive(draft_appearance_.color_mode ==
                              foundation::BoostColorMode::Invert);
    }
    if (mode_palette_) {
      mode_palette_->SetActive(draft_appearance_.color_mode ==
                               foundation::BoostColorMode::Palette);
    }
    if (mode_none_) {
      mode_none_->SetActive(draft_appearance_.color_mode ==
                            foundation::BoostColorMode::None);
    }
    const bool palette_set = draft_appearance_.palette.set;
    if (contrast_slider_) {
      contrast_slider_->SetValue(
          palette_set ? draft_appearance_.palette.contrast / 100.f : 0.5f);
    }
    if (brightness_slider_) {
      brightness_slider_->SetValue(
          palette_set ? draft_appearance_.palette.brightness / 100.f : 0.5f);
    }
    if (saturation_slider_) {
      saturation_slider_->SetValue(
          palette_set ? draft_appearance_.palette.saturation / 100.f : 0.5f);
    }
    if (mode_adjust_) {
      const bool sliders_non_default =
          palette_set && (draft_appearance_.palette.contrast != 50 ||
                          draft_appearance_.palette.brightness != 50 ||
                          draft_appearance_.palette.saturation != 50);
      mode_adjust_->SetActive(
          (adjust_section_ && adjust_section_->GetVisible()) ||
          sliders_non_default);
    }
    for (auto& cell : font_cells_) {
      if (cell) {
        cell->SetSelected(cell->preset() == draft_appearance_.font);
      }
    }
    if (size_button_) {
      const uint16_t pct = draft_appearance_.text_scale_pct;
      size_button_->SetText(pct == 100
                                ? u"Size"
                                : u"Size " + base::NumberToString16(pct) + u"%");
      size_button_->SetActive(pct != 100);
    }
    if (case_button_) {
      std::u16string label = u"Case";
      std::optional<SkColor> active_color;
      switch (draft_appearance_.text_transform) {
        case foundation::BoostTextTransform::None:
          break;
        case foundation::BoostTextTransform::Uppercase:
          label = u"AA";
          active_color = kCaseUpperBg;
          break;
        case foundation::BoostTextTransform::Lowercase:
          label = u"aa";
          active_color = kCaseLowerBg;
          break;
        case foundation::BoostTextTransform::Capitalize:
          label = u"Aa";
          active_color = kCaseCapBg;
          break;
      }
      case_button_->SetText(label);
      case_button_->SetActiveColor(active_color);
      case_button_->SetActive(draft_appearance_.text_transform !=
                              foundation::BoostTextTransform::None);
    }

    if (zap_toggle_) {
      const size_t count = record_->hidden_selectors.size();
      zap_toggle_->SetText(u"Zap");
      zap_toggle_->SetBadge(
          count == 0 ? std::u16string() : base::NumberToString16(count),
          SkColorSetRGB(0xF9, 0x67, 0x1D), SK_ColorWHITE);
      zap_toggle_->SetTooltipText(
          count == 0 ? u"Hide page elements by CSS selector"
                     : u"Hide page elements (" +
                           base::NumberToString16(count) + u" hidden)");
    }
    if (code_toggle_) {
      const std::string css(record_->custom_css.data(),
                            record_->custom_css.size());
      const bool has_code =
          css.find_first_not_of(" \t\r\n") != std::string::npos;
      size_t rule_count = 0;
      if (has_code) {
        rule_count = static_cast<size_t>(std::count(css.begin(), css.end(), '{'));
        if (rule_count == 0) {
          rule_count = 1;
        }
      }
      code_toggle_->SetBadge(
          rule_count == 0 ? std::u16string()
                          : base::NumberToString16(rule_count),
          SkColorSetRGB(0x58, 0x65, 0xF2), SK_ColorWHITE);
      code_toggle_->SetTooltipText(
          rule_count == 0
              ? u"Edit custom CSS"
              : u"Edit custom CSS (" + base::NumberToString16(rule_count) +
                    (rule_count == 1 ? u" rule)" : u" rules)"));
    }
    RebuildZapList();
    UpdateCssValidationUi();
    SizeToContents();
  }

  void RebuildZapList() {
    if (!zap_list_ || !record_) {
      return;
    }
    zap_list_->RemoveAllChildViews();
    for (const auto& sel_rust : record_->hidden_selectors) {
      std::string sel(sel_rust.data(), sel_rust.size());
      auto* row = zap_list_->AddChildView(
          MakeBox(Orientation::kHorizontal, gfx::Insets::VH(5, 8), 6));
      auto* row_layout =
          static_cast<views::BoxLayout*>(row->GetLayoutManager());
      row_layout->set_cross_axis_alignment(CrossAxis::kCenter);
      row->SetBackground(views::CreateRoundedRectBackground(kPillBg, 8));
      row->SetBorder(views::CreateRoundedRectBorder(1, 8, kPillBorder));

      auto* label = row->AddChildView(MakeLabel(
          base::UTF8ToUTF16(sel), gfx::FontList(gfx::Font("Menlo", 11)),
          kInkPrimary));
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      label->SetElideBehavior(gfx::ELIDE_TAIL);
      row_layout->SetFlexForView(label, 1);

      auto remove = std::make_unique<views::ImageButton>(base::BindRepeating(
          &BoostEditorView::RemoveZapSelector, base::Unretained(this), sel));
      remove->SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(kPeekCloseIcon, kInkSecondary, 12));
      remove->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
      remove->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
      remove->SetPreferredSize(gfx::Size(20, 20));
      remove->SetTooltipText(u"Restore element");
      remove->GetViewAccessibility().SetName(
          u"Restore " + base::UTF8ToUTF16(sel));
      row->AddChildView(std::move(remove));
    }
    if (!record_->hidden_selectors.empty()) {
      auto clear = std::make_unique<PillButton>(
          base::BindRepeating(&BoostEditorView::ClearZapSelectors,
                              base::Unretained(this)),
          u"Clear all", std::u16string(), /*centered=*/true,
          /*font_size_delta=*/-1);
      clear->SetPreferredSize(gfx::Size(0, 32));
      zap_list_->AddChildView(std::move(clear));
    }
    if (zap_section_) {
      const bool should_show =
          zap_picker_active_ && !record_->hidden_selectors.empty();
      zap_section_->SetVisible(should_show);
    }
  }

  void UpdateCssValidationUi() {
    if (!css_banner_ || !css_bytes_label_) {
      return;
    }
    css_bytes_label_->SetText(base::NumberToString16(draft_css_.size()) +
                              u" / 64 KB \u00B7 Auto-saved");

    auto findings = BoostsService::InspectCss(draft_css_);
    if (findings.empty()) {
      css_banner_->SetVisible(false);
    } else {
      const auto& first = findings[0];
      std::string msg = "Line " + base::NumberToString(first.line) + ": " +
                        std::string(first.reason.data(), first.reason.size());
      if (findings.size() > 1) {
        msg += " (+" + base::NumberToString(findings.size() - 1) + " more)";
      }
      css_banner_->SetText(base::UTF8ToUTF16(msg));
      css_banner_->SetVisible(true);
    }
    SizeToContents();
  }

  void PushLivePreview() {
    if (!web_contents() || !record_) {
      return;
    }
    auto* helper = BoostsTabHelper::FromWebContents(web_contents());
    if (!helper) {
      return;
    }
    std::vector<std::string> selectors;
    for (const auto& s : record_->hidden_selectors) {
      selectors.emplace_back(s.data(), s.size());
    }
    if (zap_input_ && zap_section_ && zap_section_->GetVisible()) {
      std::string pending =
          std::string(base::TrimWhitespaceASCII(
              base::UTF16ToUTF8(zap_input_->GetText()), base::TRIM_ALL));
      if (!pending.empty()) {
        selectors.push_back(std::move(pending));
      }
    }
    std::string css = BoostsService::CompilePreview(draft_appearance_,
                                                    selectors, draft_css_);
    helper->SetPreviewOverride(origin_, std::move(css),
                               record_->apply_to_subframes);
  }

  void ClearPreview() {
    if (!web_contents()) {
      return;
    }
    if (auto* helper = BoostsTabHelper::FromWebContents(web_contents())) {
      helper->ClearPreviewOverride();
    }
  }

  void OnPadChanged(float x, float y, bool commit) {
    if (draft_appearance_.color_mode == foundation::BoostColorMode::None) {
      draft_appearance_.color_mode = foundation::BoostColorMode::Palette;
    }
    const bool had_palette = draft_appearance_.palette.set;
    const uint8_t prev_contrast = draft_appearance_.palette.contrast;
    const uint8_t prev_brightness = draft_appearance_.palette.brightness;
    const uint8_t prev_saturation = draft_appearance_.palette.saturation;
    draft_appearance_.palette =
        PadPositionToPalette(draft_appearance_.color_mode, x, y);
    if (had_palette) {
      if (draft_appearance_.color_mode == foundation::BoostColorMode::Palette) {
        draft_appearance_.palette.contrast = prev_contrast;
      }
      draft_appearance_.palette.brightness = prev_brightness;
      draft_appearance_.palette.saturation = prev_saturation;
    }
    if (color_pad_) {
      color_pad_->SetState(draft_appearance_.color_mode,
                           draft_appearance_.palette);
    }
    SyncHexFieldFromDraft(/*force=*/true);
    if (mode_invert_) {
      mode_invert_->SetActive(draft_appearance_.color_mode ==
                              foundation::BoostColorMode::Invert);
    }
    if (mode_palette_) {
      mode_palette_->SetActive(draft_appearance_.color_mode ==
                               foundation::BoostColorMode::Palette);
    }
    if (mode_none_) {
      mode_none_->SetActive(false);
    }
    PushLivePreview();
    if (commit) {
      CommitAppearance();
    }
  }

  void SyncHexFieldFromDraft(bool force) {
    const bool has_palette_color =
        draft_appearance_.color_mode == foundation::BoostColorMode::Palette &&
        draft_appearance_.palette.set;
    if (hex_swatch_) {
      if (has_palette_color) {
        hex_swatch_->SetColor(
            Rgb24ToSkColor(draft_appearance_.palette.background));
      } else if (draft_appearance_.color_mode ==
                 foundation::BoostColorMode::Invert) {
        hex_swatch_->SetColor(SkColorSetRGB(0x2B, 0x2B, 0x30));
      } else {
        hex_swatch_->SetColor(std::nullopt);
      }
    }
    if (!hex_input_) {
      return;
    }
    if (!force && hex_input_->HasFocus()) {
      return;
    }
    suppress_hex_change_ = true;
    hex_input_->SetText(
        has_palette_color
            ? base::UTF8ToUTF16(
                  FormatRgb24Hex(draft_appearance_.palette.background))
            : std::u16string());
    suppress_hex_change_ = false;
  }

  void ApplyHexColor(uint32_t rgb, bool commit) {
    draft_appearance_.color_mode = foundation::BoostColorMode::Palette;
    draft_appearance_.palette =
        PaletteFromRgb24(rgb, draft_appearance_.palette);
    if (color_pad_) {
      color_pad_->SetState(draft_appearance_.color_mode,
                           draft_appearance_.palette);
    }
    if (hex_swatch_) {
      hex_swatch_->SetColor(Rgb24ToSkColor(rgb));
    }
    if (mode_invert_) {
      mode_invert_->SetActive(false);
    }
    if (mode_palette_) {
      mode_palette_->SetActive(true);
    }
    if (mode_none_) {
      mode_none_->SetActive(false);
    }
    PushLivePreview();
    if (commit) {
      CommitAppearance();
    }
  }

  void CommitHexInput() {
    if (!hex_input_) {
      return;
    }
    if (auto rgb = ParseHexColor(base::UTF16ToUTF8(hex_input_->GetText()))) {
      ApplyHexColor(*rgb, /*commit=*/true);
      SyncHexFieldFromDraft(/*force=*/true);
    } else {
      SyncHexFieldFromDraft(/*force=*/true);
    }
  }

  void OnInvertPressed() {
    if (draft_appearance_.color_mode == foundation::BoostColorMode::Palette &&
        draft_appearance_.palette.set) {
      const uint32_t inverted_rgb =
          (~draft_appearance_.palette.background) & 0xFF'FFFF;
      ApplyHexColor(inverted_rgb, /*commit=*/true);
      SyncHexFieldFromDraft(/*force=*/true);
      return;
    }
    if (draft_appearance_.color_mode == foundation::BoostColorMode::Invert) {
      SetColorMode(foundation::BoostColorMode::None);
    } else {
      SetColorMode(foundation::BoostColorMode::Invert);
    }
  }

  void EnsurePaletteForSliders() {
    if (draft_appearance_.color_mode == foundation::BoostColorMode::None) {
      draft_appearance_.color_mode = foundation::BoostColorMode::Palette;
    }
    if (!draft_appearance_.palette.set) {
      draft_appearance_.palette =
          PadPositionToPalette(draft_appearance_.color_mode, 0.55f, 0.25f);
    }
  }

  void ToggleAdjustSection() {
    if (!adjust_section_) {
      return;
    }
    const bool show = !adjust_section_->GetVisible();
    adjust_section_->SetVisible(show);
    if (mode_adjust_) {
      const bool sliders_non_default =
          draft_appearance_.palette.set &&
          (draft_appearance_.palette.contrast != 50 ||
           draft_appearance_.palette.brightness != 50 ||
           draft_appearance_.palette.saturation != 50);
      mode_adjust_->SetActive(show || sliders_non_default);
    }
    AnimateSizeToContents(/*crossfade=*/false);
  }

  bool HasAnyEdits() const {
    if (!record_) {
      return false;
    }
    const auto& a = record_->appearance;
    if (a.color_mode != foundation::BoostColorMode::None ||
        a.font != foundation::BoostFontPreset::Default ||
        a.text_scale_pct != 100 ||
        a.text_transform != foundation::BoostTextTransform::None) {
      return true;
    }
    if (!record_->hidden_selectors.empty() || !record_->custom_css.empty()) {
      return true;
    }
    return false;
  }

  void ShuffleBoost() {
    const float x = static_cast<float>(base::RandDouble());
    const float y = static_cast<float>(0.15 + base::RandDouble() * 0.75);
    draft_appearance_.color_mode = foundation::BoostColorMode::Palette;
    draft_appearance_.palette =
        PadPositionToPalette(foundation::BoostColorMode::Palette, x, y);
    const size_t font_idx =
        static_cast<size_t>(base::RandGenerator(kFontPresets.size()));
    draft_appearance_.font = kFontPresets[font_idx].preset;
    PushLivePreview();
    CommitAppearance();
  }

  void SetColorMode(foundation::BoostColorMode mode) {
    if (draft_appearance_.color_mode == mode &&
        mode != foundation::BoostColorMode::None) {
      draft_appearance_.color_mode = foundation::BoostColorMode::None;
    } else {
      draft_appearance_.color_mode = mode;
      if (mode != foundation::BoostColorMode::None &&
          !draft_appearance_.palette.set) {
        draft_appearance_.palette = PadPositionToPalette(mode, 0.55f, 0.25f);
      }
    }
    PushLivePreview();
    CommitAppearance();
  }

  void SetFontPreset(foundation::BoostFontPreset preset) {
    draft_appearance_.font = preset;
    PushLivePreview();
    CommitAppearance();
  }

  void CycleTextScale() {
    auto it = std::ranges::find(kTextScales, draft_appearance_.text_scale_pct);
    const size_t next =
        it == kTextScales.end()
            ? 1
            : (static_cast<size_t>(it - kTextScales.begin()) + 1) %
                  kTextScales.size();
    draft_appearance_.text_scale_pct = kTextScales[next];
    PushLivePreview();
    CommitAppearance();
  }

  void CycleTextTransform() {
    switch (draft_appearance_.text_transform) {
      case foundation::BoostTextTransform::None:
        draft_appearance_.text_transform =
            foundation::BoostTextTransform::Uppercase;
        break;
      case foundation::BoostTextTransform::Uppercase:
        draft_appearance_.text_transform =
            foundation::BoostTextTransform::Lowercase;
        break;
      case foundation::BoostTextTransform::Lowercase:
        draft_appearance_.text_transform =
            foundation::BoostTextTransform::Capitalize;
        break;
      case foundation::BoostTextTransform::Capitalize:
        draft_appearance_.text_transform = foundation::BoostTextTransform::None;
        break;
    }
    PushLivePreview();
    CommitAppearance();
  }

  void CommitAppearance() {
    if (!record_) {
      return;
    }
    suppress_reload_ = true;
    service_->SetBoostAppearance(boost_id_, draft_appearance_);
    suppress_reload_ = false;
    ClearPreview();
    ReloadFromService();
  }

  void ToggleZapSection() {
    if (!zap_section_) {
      return;
    }
    const bool active = !zap_picker_active_;
    const bool has_items = record_ && !record_->hidden_selectors.empty();
    zap_section_->SetVisible(active && has_items);
    if (zap_toggle_) {
      zap_toggle_->SetActive(active);
    }
    if (active) {
      StartZapPicker();
    } else {
      StopZapPicker();
    }
    AnimateSizeToContents(/*crossfade=*/false);
  }

  void StartZapPicker() {
    if (zap_picker_active_ || !web_contents()) {
      return;
    }
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    if (!rfh || !rfh->IsRenderFrameLive()) {
      return;
    }
    zap_picker_active_ = true;
    rfh->ExecuteJavaScriptInIsolatedWorld(
        kZapStartScript, base::NullCallback(),
        ISOLATED_WORLD_ID_CHROME_INTERNAL);
    if (views::Widget* parent = GetWidget() ? GetWidget()->parent() : nullptr) {
      parent->Activate();
    }
    web_contents()->Focus();
    zap_poll_timer_.Start(
        FROM_HERE, base::Milliseconds(100),
        base::BindRepeating(&BoostEditorView::PollZapPicker,
                            base::Unretained(this)));
  }

  void StopZapPicker() {
    zap_poll_timer_.Stop();
    if (!zap_picker_active_) {
      return;
    }
    zap_picker_active_ = false;
    if (!web_contents()) {
      return;
    }
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    if (rfh && rfh->IsRenderFrameLive()) {
      rfh->ExecuteJavaScriptInIsolatedWorld(
          kZapStopScript, base::NullCallback(),
          ISOLATED_WORLD_ID_CHROME_INTERNAL);
    }
  }

  void PollZapPicker() {
    if (!zap_picker_active_ || !web_contents()) {
      StopZapPicker();
      return;
    }
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    if (!rfh || !rfh->IsRenderFrameLive()) {
      StopZapPicker();
      return;
    }
    rfh->ExecuteJavaScriptInIsolatedWorld(
        kZapPollScript,
        base::BindOnce(&BoostEditorView::HandleZapEvents,
                       weak_factory_.GetWeakPtr()),
        ISOLATED_WORLD_ID_CHROME_INTERNAL);
  }

  void HandleZapEvents(base::Value result) {
    if (!result.is_list() || !service_ || !record_) {
      return;
    }
    bool changed = false;
    bool done = false;
    std::string picked_selector;
    bool pick_cancelled = false;
    for (const base::Value& item : result.GetList()) {
      if (!item.is_dict()) {
        continue;
      }
      const std::string* type = item.GetDict().FindString("type");
      if (!type) {
        continue;
      }
      if (*type == "done") {
        done = true;
      } else if (*type == "pick_cancel") {
        pick_cancelled = true;
      } else if (*type == "pick") {
        const std::string* sel = item.GetDict().FindString("selector");
        if (sel && !sel->empty()) {
          picked_selector = *sel;
        }
      } else if (*type == "zap") {
        const std::string* sel = item.GetDict().FindString("selector");
        if (sel && !sel->empty()) {
          auto res = service_->AddHiddenSelector(boost_id_, *sel);
          if (res.ok) {
            changed = true;
          }
        }
      }
    }
    if (!picked_selector.empty() || pick_cancelled) {
      StopZapPicker();
      if (code_picker_button_) {
        code_picker_button_->SetActive(false);
      }
      if (!picked_selector.empty() && css_editor_) {
        if (!draft_css_.empty() && draft_css_.back() != '\n') {
          draft_css_.push_back('\n');
        }
        const std::string prefix = picked_selector + " {\n  ";
        draft_css_ += prefix;
        const size_t caret_utf16 = base::UTF8ToUTF16(draft_css_).size();
        draft_css_ += "\n}\n";
        css_dirty_ = true;
        css_editor_->SetText(base::UTF8ToUTF16(draft_css_));
        css_editor_->SetSelectedRange(gfx::Range(caret_utf16));
        ApplyCssSyntaxHighlighting(css_editor_);
        if (GetWidget()) {
          GetWidget()->Activate();
        }
        css_editor_->RequestFocus();
        UpdateCssValidationUi();
        PushLivePreview();
      }
    }
    if (changed) {
      ClearPreview();
      ReloadFromService();
      if (web_contents()) {
        if (content::RenderFrameHost* rfh =
                web_contents()->GetPrimaryMainFrame();
            rfh && rfh->IsRenderFrameLive()) {
          rfh->ExecuteJavaScriptInIsolatedWorld(
              u"if(window.__myarcZap&&window.__myarcZap.clearPreview)"
              u"window.__myarcZap.clearPreview()",
              base::NullCallback(), ISOLATED_WORLD_ID_CHROME_INTERNAL);
        }
      }
    }
    if (done) {
      StopZapPicker();
      if (zap_section_) {
        zap_section_->SetVisible(false);
      }
      if (zap_toggle_) {
        zap_toggle_->SetActive(false);
      }
      AnimateSizeToContents(/*crossfade=*/false);
    }
  }

  void AddZapSelector() {
    if (!zap_input_) {
      return;
    }
    const std::string selector = base::UTF16ToUTF8(zap_input_->GetText());
    auto res = service_->AddHiddenSelector(boost_id_, selector);
    if (!res.ok) {
      if (zap_error_label_) {
        zap_error_label_->SetText(base::UTF8ToUTF16(
            std::string(res.error.data(), res.error.size())));
        zap_error_label_->SetVisible(true);
        SizeToContents();
      }
      return;
    }
    zap_input_->SetText(std::u16string());
    if (zap_error_label_) {
      zap_error_label_->SetVisible(false);
    }
    ClearPreview();
    ReloadFromService();
  }

  void RemoveZapSelector(const std::string& selector) {
    service_->RemoveHiddenSelector(boost_id_, selector);
    ClearPreview();
    ReloadFromService();
  }

  void ClearZapSelectors() {
    service_->ClearHiddenSelectors(boost_id_);
    ClearPreview();
    ReloadFromService();
  }

  void EnterCodeMode() {
    if (code_mode_ || !main_panel_ || !code_panel_) {
      return;
    }
    if (zap_picker_active_) {
      StopZapPicker();
      if (zap_section_) {
        zap_section_->SetVisible(false);
      }
      if (zap_toggle_) {
        zap_toggle_->SetActive(false);
      }
    }
    code_mode_ = true;
    main_panel_->SetVisible(false);
    code_panel_->SetVisible(true);
    AnimateSizeToContents(/*crossfade=*/true);
    if (css_editor_) {
      ApplyCssSyntaxHighlighting(css_editor_);
      css_editor_->RequestFocus();
    }
  }

  void ExitCodeMode() {
    if (!code_mode_ || !main_panel_ || !code_panel_) {
      return;
    }
    StopZapPicker();
    ClearCssSuggestions();
    if (code_picker_button_) {
      code_picker_button_->SetActive(false);
    }
    CommitPendingCssIfValid();
    code_mode_ = false;
    code_panel_->SetVisible(false);
    main_panel_->SetVisible(true);
    AnimateSizeToContents(/*crossfade=*/true);
  }

  void ToggleCodeElementPicker() {
    if (!code_picker_button_ || !web_contents()) {
      return;
    }
    if (zap_picker_active_) {
      StopZapPicker();
      code_picker_button_->SetActive(false);
      return;
    }
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    if (!rfh || !rfh->IsRenderFrameLive()) {
      return;
    }
    zap_picker_active_ = true;
    code_picker_button_->SetActive(true);
    rfh->ExecuteJavaScriptInIsolatedWorld(
        kCssPickStartScript, base::NullCallback(),
        ISOLATED_WORLD_ID_CHROME_INTERNAL);
    if (views::Widget* parent = GetWidget() ? GetWidget()->parent() : nullptr) {
      parent->Activate();
    }
    web_contents()->Focus();
    zap_poll_timer_.Start(
        FROM_HERE, base::Milliseconds(100),
        base::BindRepeating(&BoostEditorView::PollZapPicker,
                            base::Unretained(this)));
  }

  void UpdateCssSuggestions() {
    if (!css_editor_ || !css_suggestions_row_ || !css_bytes_label_) {
      return;
    }
    const gfx::Range sel = css_editor_->GetSelectedRange();
    if (!sel.is_empty()) {
      ClearCssSuggestions();
      return;
    }
    css_completion_ =
        ComputeCssCompletions(css_editor_->GetText(), sel.start());
    css_completion_index_ = 0;
    RebuildCssSuggestionChips();
  }

  void ClearCssSuggestions() {
    css_completion_ = {};
    css_completion_index_ = 0;
    if (css_suggestions_row_) {
      css_suggestions_row_->RemoveAllChildViews();
      css_suggestions_row_->SetVisible(false);
    }
    if (css_bytes_label_) {
      css_bytes_label_->SetVisible(true);
    }
  }

  void RebuildCssSuggestionChips() {
    if (!css_suggestions_row_ || !css_bytes_label_) {
      return;
    }
    css_suggestions_row_->RemoveAllChildViews();
    if (css_completion_.items.empty()) {
      css_suggestions_row_->SetVisible(false);
      css_bytes_label_->SetVisible(true);
      return;
    }
    css_bytes_label_->SetVisible(false);
    css_suggestions_row_->SetVisible(true);
    for (size_t i = 0; i < css_completion_.items.size(); ++i) {
      const bool selected = (i == css_completion_index_);
      auto chip = std::make_unique<views::LabelButton>(
          base::BindRepeating(&BoostEditorView::AcceptCssSuggestion,
                              base::Unretained(this), i),
          css_completion_.items[i]);
      chip->SetEnabledTextColors(selected ? kPillActiveInk : kInkPrimary);
      chip->SetBackground(views::CreateRoundedRectBackground(
          selected ? SK_ColorWHITE : kPillBg, 6));
      chip->SetBorder(views::CreatePaddedBorder(
          selected ? nullptr : views::CreateRoundedRectBorder(1, 6, kPillBorder),
          gfx::Insets::VH(3, 7)));
      chip->SetFocusBehavior(views::View::FocusBehavior::NEVER);
      css_suggestions_row_->AddChildView(std::move(chip));
    }
    css_suggestions_row_->InvalidateLayout();
  }

  void AcceptCssSuggestion(size_t index) {
    if (!css_editor_ || index >= css_completion_.items.size()) {
      return;
    }
    const std::u16string text = std::u16string(css_editor_->GetText());
    const gfx::Range sel = css_editor_->GetSelectedRange();
    const size_t start = std::min(css_completion_.replace_start, text.size());
    const size_t end = std::min(sel.GetMax(), text.size());
    std::u16string insert = css_completion_.items[index];
    if (css_completion_.is_property) {
      insert += u": ";
    } else if (!insert.starts_with(u"!")) {
      const bool has_semi = end < text.size() && text[end] == u';';
      if (!has_semi) {
        insert += u";";
      }
    }
    const std::u16string next =
        text.substr(0, start) + insert + text.substr(end);
    const size_t next_caret = start + insert.size();
    ClearCssSuggestions();
    ApplyCssProgrammaticEdit(next, gfx::Range(next_caret));
    ScheduleCssAutoSave();
    css_editor_->RequestFocus();
  }

  void ScheduleCssAutoSave() {
    css_autosave_timer_.Start(
        FROM_HERE, base::Milliseconds(400),
        base::BindOnce(&BoostEditorView::CommitPendingCssIfValid,
                       base::Unretained(this), /*format_now=*/false));
  }

  void CommitPendingCssIfValid(bool format_now = true) {
    css_autosave_timer_.Stop();
    if (!css_dirty_ || !record_) {
      return;
    }
    if (BoostsService::InspectCss(draft_css_).empty()) {
      if (format_now && css_editor_ && !draft_css_.empty()) {
        const std::u16string formatted = FormatCssText(css_editor_->GetText());
        draft_css_ = base::UTF16ToUTF8(formatted);
        css_editor_->SetText(formatted);
        ApplyCssSyntaxHighlighting(css_editor_);
      }
      suppress_reload_ = true;
      service_->SetCustomCss(boost_id_, draft_css_);
      suppress_reload_ = false;
      css_dirty_ = false;
      if (code_toggle_) {
        const bool has_code =
            draft_css_.find_first_not_of(" \t\r\n") != std::string::npos;
        size_t rule_count = 0;
        if (has_code) {
          rule_count = static_cast<size_t>(
              std::count(draft_css_.begin(), draft_css_.end(), '{'));
          if (rule_count == 0) {
            rule_count = 1;
          }
        }
        code_toggle_->SetBadge(
            rule_count == 0 ? std::u16string()
                            : base::NumberToString16(rule_count),
            SkColorSetRGB(0x58, 0x65, 0xF2), SK_ColorWHITE);
      }
    }
  }

  void OnResetPressed() {
    css_dirty_ = false;
    ClearPreview();
    service_->ResetBoost(boost_id_);
    ReloadFromService();
  }

  void ShowTitleMenu() {
    if (!title_button_ || !GetWidget() ||
        (menu_runner_ && menu_runner_->IsRunning())) {
      return;
    }
    menu_runner_.reset();
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_model_->AddItem(kCmdRename, u"Rename this Boost\u2026");
    menu_model_->AddItem(kCmdShuffle, u"Shuffle");
    menu_model_->AddItem(kCmdReset, u"Reset all Edits");
    menu_model_->AddItem(kCmdDelete, u"Delete this Boost");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kCmdAllBoosts, u"All Boosts\u2026");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddCheckItem(kCmdToggleEnabled, u"Enabled");
    menu_model_->AddCheckItem(kCmdToggleSubdomains, u"Include Subdomains");
    menu_model_->AddCheckItem(kCmdToggleSubframes, u"Apply to Subframes");
    menu_model_->AddItem(kCmdUndo, u"Undo Last Change");
    menu_runner_ =
        std::make_unique<views::MenuRunner>(menu_model_.get(), /*run_types=*/0);
    menu_runner_->RunMenuAt(GetWidget(), nullptr,
                            title_button_->GetBoundsInScreen(),
                            views::MenuAnchorPosition::kTopLeft,
                            ui::mojom::MenuSourceType::kMouse);
  }

  void BeginInlineRename() {
    if (!record_ || !title_button_ || !rename_field_) {
      return;
    }
    title_button_->SetVisible(false);
    rename_field_->SetText(base::UTF8ToUTF16(
        std::string(record_->name.data(), record_->name.size())));
    rename_field_->SetVisible(true);
    rename_field_->SelectAll(false);
    rename_field_->RequestFocus();
  }

  void EndInlineRename(bool commit) {
    if (!title_button_ || !rename_field_) {
      return;
    }
    if (commit) {
      const std::string name = base::UTF16ToUTF8(rename_field_->GetText());
      service_->RenameBoost(boost_id_, name);
    }
    rename_field_->SetVisible(false);
    title_button_->SetVisible(true);
    ReloadFromService();
  }

  void SizeToContents() {
    if (bubble_delegate_) {
      bubble_delegate_->SizeToContents();
    }
  }

  void AnimateSizeToContents(bool crossfade) {
    views::Widget* widget = GetWidget();
    if (!bubble_delegate_ || !widget || widget->IsClosed() ||
        !widget->IsVisible()) {
      SizeToContents();
      return;
    }
    anim_start_bounds_ = widget->GetWindowBoundsInScreen();
    bubble_delegate_->SizeToContents();
    anim_target_bounds_ = widget->GetWindowBoundsInScreen();
    if (anim_start_bounds_ != anim_target_bounds_) {
      widget->SetBounds(anim_start_bounds_);
      bounds_anim_.Stop();
      bounds_anim_.Reset(0.0);
      bounds_anim_.Show();
    }
    if (crossfade && layer()) {
      fade_from_opacity_ = 0.45;
      fade_anim_.Stop();
      fade_anim_.Reset(0.0);
      layer()->SetOpacity(static_cast<float>(fade_from_opacity_));
      fade_anim_.Show();
    }
  }

  void CloseEditor() {
    if (GetWidget() && !GetWidget()->IsClosed()) {
      GetWidget()->Close();
    }
  }

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;
  raw_ptr<BoostsService> service_;
  const uint64_t boost_id_;
  const std::string origin_;
  base::ScopedObservation<BoostsService, BoostsService::Observer>
      service_observation_{this};
  std::optional<foundation::BoostRecord> record_;
  foundation::BoostAppearanceRecord draft_appearance_{};
  std::string draft_css_;
  bool css_dirty_ = false;
  bool suppress_reload_ = false;
  bool suppress_hex_change_ = false;
  bool slider_dragging_ = false;
  bool zap_picker_active_ = false;
  bool code_mode_ = false;
  bool hidden_by_tab_switch_ = false;
  base::RepeatingTimer zap_poll_timer_;
  gfx::SlideAnimation bounds_anim_{this};
  gfx::SlideAnimation fade_anim_{this};
  gfx::Rect anim_start_bounds_;
  gfx::Rect anim_target_bounds_;
  double fade_from_opacity_ = 0.0;

  raw_ptr<views::View> main_panel_ = nullptr;
  raw_ptr<views::View> code_panel_ = nullptr;
  raw_ptr<TitleMenuButton> title_button_ = nullptr;
  raw_ptr<views::Textfield> rename_field_ = nullptr;
  raw_ptr<ColorPadView> color_pad_ = nullptr;
  raw_ptr<ColorSwatchDot> hex_swatch_ = nullptr;
  raw_ptr<views::Textfield> hex_input_ = nullptr;
  raw_ptr<PillButton> mode_invert_ = nullptr;
  raw_ptr<PillButton> mode_palette_ = nullptr;
  raw_ptr<PillButton> mode_adjust_ = nullptr;
  raw_ptr<PillButton> mode_none_ = nullptr;
  raw_ptr<views::View> adjust_section_ = nullptr;
  raw_ptr<BoostAdjustSlider> contrast_slider_ = nullptr;
  raw_ptr<BoostAdjustSlider> brightness_slider_ = nullptr;
  raw_ptr<BoostAdjustSlider> saturation_slider_ = nullptr;
  std::array<raw_ptr<FontPresetCell>, 16> font_cells_ = {};
  raw_ptr<PillButton> size_button_ = nullptr;
  raw_ptr<PillButton> case_button_ = nullptr;
  raw_ptr<PillButton> zap_toggle_ = nullptr;
  raw_ptr<views::View> zap_section_ = nullptr;
  raw_ptr<views::Textfield> zap_input_ = nullptr;
  raw_ptr<views::Label> zap_error_label_ = nullptr;
  raw_ptr<views::View> zap_list_ = nullptr;
  raw_ptr<PillButton> code_toggle_ = nullptr;
  raw_ptr<PillButton> code_picker_button_ = nullptr;
  raw_ptr<views::Textarea> css_editor_ = nullptr;
  raw_ptr<views::Label> css_banner_ = nullptr;
  raw_ptr<views::Label> css_bytes_label_ = nullptr;
  raw_ptr<views::View> css_suggestions_row_ = nullptr;
  CssCompletionResult css_completion_;
  size_t css_completion_index_ = 0;
  base::OneShotTimer css_autosave_timer_;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  base::WeakPtrFactory<BoostEditorView> weak_factory_{this};
};

BEGIN_METADATA(BoostEditorView)
END_METADATA

// Page Info row ("Boost this site" / "Edit Boost…") placed in PageInfoMainView.
class BoostPageInfoRow final : public views::View,
                               public content::WebContentsObserver {
  METADATA_HEADER(BoostPageInfoRow, views::View)

 public:
  BoostPageInfoRow(content::WebContents* contents,
                   Profile* profile,
                   url::Origin origin)
      : content::WebContentsObserver(contents),
        profile_(profile),
        origin_(std::move(origin)) {
    SetUseDefaultFillLayout(true);
    row_ = AddChildView(std::make_unique<RichControlsContainerView>());
    row_->SetIcon(ui::ImageModel::FromVectorIcon(kBoostsIcon,
                                                 ui::kColorIcon, 20));

    if (profile_->IsOffTheRecord() || profile_->IsGuestSession()) {
      row_->SetTitle(u"Boosts");
      row_->AddSecondaryLabel(u"Disabled in private windows by default");
    } else {
      auto& service = BoostsService::ForProfile(*profile_);
      auto matching = service.MatchingBoosts(origin_);
      const bool has_boost = !matching.empty();
      if (has_boost) {
        boost_id_ = matching[0].id;
      }
      row_->SetTitle(has_boost ? u"Boost active on this site"
                               : u"Boost this site");
      const std::u16string link_text =
          has_boost ? u"Edit Boost\u2026" : u"New Boost\u2026";
      views::StyledLabel* link = row_->AddSecondaryStyledLabel(link_text);
      link->AddStyleRange(
          gfx::Range(0, link_text.size()),
          views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
              &BoostPageInfoRow::OpenEditor, base::Unretained(this))));

      if (has_boost) {
        auto toggle = std::make_unique<views::ToggleButton>(base::BindRepeating(
            &BoostPageInfoRow::OnToggle, base::Unretained(this)));
        toggle->SetPreferredSize(gfx::Size(toggle->GetPreferredSize().width(),
                                           row_->GetFirstLineHeight()));
        toggle->SetProperty(
            views::kMarginsKey,
            gfx::Insets::VH(0, views::LayoutProvider::Get()->GetDistanceMetric(
                                   views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
        toggle->GetViewAccessibility().SetName(u"Enable Boost on this site");
        toggle->SetIsOn(matching[0].enabled);
        toggle_ = row_->AddControl(std::move(toggle));
      }
    }

    SetProperty(views::kFlexBehaviorKey,
                views::FlexSpecification(
                    base::BindRepeating(&RichControlsContainerView::FlexRule,
                                        base::Unretained(row_))));
  }

  ~BoostPageInfoRow() override {
    toggle_ = nullptr;
    row_ = nullptr;
  }

 private:
  void OnToggle() {
    if (!boost_id_ || !toggle_) {
      return;
    }
    BoostsService::ForProfile(*profile_).SetBoostEnabled(boost_id_,
                                                         toggle_->GetIsOn());
  }

  void OpenEditor() {
    tabs::TabInterface* tab =
        web_contents() ? tabs::TabInterface::MaybeGetFromContents(web_contents())
                       : nullptr;
    BrowserWindowInterface* browser =
        tab ? tab->GetBrowserWindowInterface() : nullptr;
    if (!browser) {
      return;
    }
    const uint64_t id = boost_id_;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<BrowserWindowInterface> browser, uint64_t id) {
              if (browser) {
                ShowBoostEditor(*browser, id);
              }
            },
            browser->GetWeakPtr(), id));
    if (GetWidget()) {
      GetWidget()->Close();
    }
  }

  const raw_ptr<Profile> profile_;
  const url::Origin origin_;
  uint64_t boost_id_ = 0;
  raw_ptr<RichControlsContainerView> row_ = nullptr;
  raw_ptr<views::ToggleButton> toggle_ = nullptr;
};

BEGIN_METADATA(BoostPageInfoRow)
END_METADATA

}  // namespace

bool CanBoostWebContents(content::WebContents* contents) {
  if (!contents) {
    return false;
  }
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!profile || profile->IsOffTheRecord() || profile->IsGuestSession()) {
    return false;
  }
  return IsEligibleHttpOrigin(ActiveOriginForContents(contents));
}

bool HasActiveBoostForWebContents(content::WebContents* contents) {
  if (!CanBoostWebContents(contents)) {
    return false;
  }
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  auto& service = BoostsService::ForProfile(*profile);
  if (!service.GlobalEnabled()) {
    return false;
  }
  auto matches = service.MatchingBoosts(ActiveOriginForContents(contents));
  for (const auto& boost : matches) {
    if (boost.enabled) {
      return true;
    }
  }
  return false;
}

bool ShowBoostEditor(BrowserWindowInterface& browser, uint64_t boost_id) {
  auto& editors = ActiveEditors();
  if (auto it = editors.find(&browser);
      it != editors.end() && it->second && it->second->view) {
    const uint64_t current_id = it->second->view->boost_id();
    const bool was_hidden = it->second->view->is_hidden_by_tab_switch();
    CloseBoostEditor(browser);
    // Toggling via command/button with no specific new target closes the card,
    // unless it was hidden on another tab (then open for the active tab).
    if (!was_hidden && (boost_id == 0 || boost_id == current_id)) {
      return true;
    }
  }

  TabStripModel* tab_strip = browser.GetTabStripModel();
  content::WebContents* contents =
      tab_strip ? tab_strip->GetActiveWebContents() : nullptr;
  if (!CanBoostWebContents(contents)) {
    return false;
  }
  Profile* profile = browser.GetProfile();
  if (!profile) {
    return false;
  }

  auto& service = BoostsService::ForProfile(*profile);
  const url::Origin origin = ActiveOriginForContents(contents);
  const std::string origin_str = origin.Serialize();

  if (boost_id == 0) {
    auto existing = service.MatchingBoosts(origin);
    if (!existing.empty()) {
      boost_id = existing[0].id;
    } else {
      auto created = service.CreateBoost("My Boost", {origin_str});
      if (!created.ok || created.id == 0) {
        return false;
      }
      boost_id = created.id;
    }
  } else if (!service.GetBoost(boost_id).has_value()) {
    return false;
  }

  views::View* anchor = nullptr;
  if (auto* elements = BrowserElementsViews::From(&browser)) {
    anchor = elements->GetView(kContentsContainerViewElementId);
    if (!anchor) {
      anchor = elements->GetView(kLocationBarElementId);
    }
  }

  auto slot = std::make_unique<ActiveEditorSlot>();
  slot->delegate = std::make_unique<BoostEditorBubbleDelegate>(anchor);
  slot->delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  slot->delegate->SetShowCloseButton(false);
  slot->delegate->SetShowTitle(false);
  slot->delegate->SetTitle(u"Boost Editor");
  slot->delegate->set_margins(gfx::Insets(14));
  slot->delegate->set_corner_radius(kCardCornerRadius);
  slot->delegate->set_close_on_deactivate(false);
  slot->delegate->SetBackgroundColor(kCardBg);
  slot->view = slot->delegate->SetContentsView(std::make_unique<BoostEditorView>(
      browser, slot->delegate.get(), contents, service, boost_id, origin_str));
  slot->delegate->RegisterWindowClosingCallback(base::BindOnce(
      &BoostEditorView::OnWindowClosing, base::Unretained(slot->view)));
  slot->widget =
      views::BubbleDialogDelegate::CreateBubble(slot->delegate.get());
  views::Widget* raw_widget = slot->widget.get();
  BoostEditorView* raw_view = slot->view;
  editors[&browser] = std::move(slot);
  raw_widget->Show();
  ApplyBubbleGlass(*raw_widget, GlassMaterial::kPopover, /*dark=*/true,
                   kCardGlassTint, kCardBg);
  raw_view->StartOpenAnimation();
  return true;
}

void CloseBoostEditor(BrowserWindowInterface& browser) {
  auto& editors = ActiveEditors();
  if (auto it = editors.find(&browser); it != editors.end() && it->second) {
    auto slot = std::move(it->second);
    editors.erase(it);
    if (slot->widget && !slot->widget->IsClosed()) {
      slot->widget->Close();
    }
    ReleaseEditorSlot(std::move(slot));
  }
}

std::unique_ptr<views::View> CreateBoostPageInfoRow(
    content::WebContents* contents) {
  if (!contents) {
    return nullptr;
  }
  const url::Origin origin = ActiveOriginForContents(contents);
  if (!IsEligibleHttpOrigin(origin)) {
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }
  return std::make_unique<BoostPageInfoRow>(contents, profile, origin);
}

}  // namespace myarc
