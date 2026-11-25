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
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "myarc/boosts_service.h"
#include "myarc/boosts_tab_helper.h"
#include "myarc/bridge.rs.h"
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
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
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
constexpr int kPadWidth = 220;
constexpr int kPadHeight = 152;
constexpr int kCardCornerRadius = 16;
constexpr SkColor kCardBg = SK_ColorWHITE;
constexpr SkColor kPillBg = SkColorSetRGB(0xF1, 0xF1, 0xF4);
constexpr SkColor kPillHoverBg = SkColorSetRGB(0xE6, 0xE6, 0xEC);
constexpr SkColor kPillActiveBg = SkColorSetRGB(0x2B, 0x2B, 0x30);
constexpr SkColor kInkPrimary = SkColorSetRGB(0x1C, 0x1C, 0x1E);
constexpr SkColor kInkSecondary = SkColorSetRGB(0x6E, 0x6E, 0x73);
constexpr SkColor kBorderSubtle = SkColorSetRGB(0xE5, 0xE5, 0xEA);
constexpr SkColor kAccentBlue = SkColorSetRGB(0x4F, 0x5B, 0xFF);
constexpr SkColor kWarnBg = SkColorSetRGB(0xFF, 0xF3, 0xCD);
constexpr SkColor kWarnText = SkColorSetRGB(0x7A, 0x4F, 0x01);
constexpr SkColor kErrorText = SkColorSetRGB(0xC6, 0x28, 0x28);

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
             int font_size_delta = 0)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 10);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(0, 14), 6));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    layout->set_main_axis_alignment(centered ? MainAxis::kCenter
                                             : MainAxis::kStart);

    label_ = AddChildView(MakeLabel(
        text,
        gfx::FontList().Derive(font_size_delta, gfx::Font::NORMAL,
                               gfx::Font::Weight::SEMIBOLD),
        kInkPrimary));
    if (!centered) {
      layout->SetFlexForView(label_, 1);
      label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
    if (!trailing_glyph.empty()) {
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

  void SetText(const std::u16string& text) {
    label_->SetText(text);
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
    const SkColor bg = active_ ? kPillActiveBg : (hovered ? kPillHoverBg : kPillBg);
    SetBackground(views::CreateRoundedRectBackground(bg, 10));
    label_->SetEnabledColor(active_ ? SK_ColorWHITE : kInkPrimary);
    if (trailing_) {
      trailing_->SetEnabledColor(active_ ? SK_ColorWHITE : kInkPrimary);
    }
  }

  bool active_ = false;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::Label> trailing_ = nullptr;
};

BEGIN_METADATA(PillButton)
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
    dot_flags.setColor(SkColorSetA(kInkPrimary, 0x14));
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
      SetBackground(views::CreateRoundedRectBackground(
          SkColorSetA(kAccentBlue, 0x22), 6));
      label_->SetEnabledColor(kAccentBlue);
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

class BoostEditorView;

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
                              public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(BoostEditorView, views::View)

 public:
  enum TitleCommand {
    kCmdToggleEnabled = 1,
    kCmdToggleSubdomains,
    kCmdToggleSubframes,
    kCmdRename,
    kCmdUndo,
    kCmdDelete,
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
    SetPreferredSize(gfx::Size(kCardWidth, 0));

    service_observation_.Observe(service_.get());

    BuildUi();
    ReloadFromService();
  }

  ~BoostEditorView() override {
    ClearPreview();
    service_observation_.Reset();
    menu_runner_.reset();
    menu_model_.reset();
    bubble_delegate_ = nullptr;
    ClearChildPointers();
  }

  uint64_t boost_id() const { return boost_id_; }

  void OnWindowClosing() {
    CommitPendingCssIfValid();
    ClearPreview();
    service_observation_.Reset();
    bubble_delegate_ = nullptr;
    if (auto it = ActiveEditors().find(browser_);
        it != ActiveEditors().end() && it->second && it->second->view == this) {
      auto slot = std::move(it->second);
      ActiveEditors().erase(it);
      ReleaseEditorSlot(std::move(slot));
    }
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_ESCAPE) {
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
      UpdateCssValidationUi();
      PushLivePreview();
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
      CloseEditor();
      return true;
    }
    if (key_event.key_code() == ui::VKEY_RETURN) {
      if (sender == rename_field_) {
        EndInlineRename(/*commit=*/true);
        return true;
      }
      if (sender == zap_input_) {
        AddZapSelector();
        return true;
      }
    }
    return false;
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
    return true;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    if (!record_) {
      return;
    }
    switch (command_id) {
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
      case kCmdRename:
        BeginInlineRename();
        break;
      case kCmdUndo:
        ClearPreview();
        service_->UndoBoost(boost_id_);
        break;
      case kCmdDelete:
        ClearPreview();
        service_->DeleteBoost(boost_id_);
        CloseEditor();
        break;
    }
  }

 private:
  void ClearChildPointers() {
    title_button_ = nullptr;
    rename_field_ = nullptr;
    color_pad_ = nullptr;
    mode_invert_ = nullptr;
    mode_palette_ = nullptr;
    mode_none_ = nullptr;
    font_cells_.fill(nullptr);
    size_button_ = nullptr;
    case_button_ = nullptr;
    zap_toggle_ = nullptr;
    zap_section_ = nullptr;
    zap_input_ = nullptr;
    zap_error_label_ = nullptr;
    zap_list_ = nullptr;
    code_toggle_ = nullptr;
    code_section_ = nullptr;
    css_editor_ = nullptr;
    css_banner_ = nullptr;
    css_bytes_label_ = nullptr;
  }

  void BuildUi() {
    // 1. Header row: Close (x), Title dropdown / rename, Reset (↺).
    auto* header =
        AddChildView(MakeBox(Orientation::kHorizontal,
                             gfx::Insets::TLBR(0, 0, 8, 0), 4));
    header->SetBorder(
        views::CreateSolidSidedBorder(gfx::Insets::TLBR(0, 0, 1, 0), kBorderSubtle));
    auto* header_layout =
        static_cast<views::BoxLayout*>(header->GetLayoutManager());
    header_layout->set_cross_axis_alignment(CrossAxis::kCenter);

    auto close = std::make_unique<views::ImageButton>(base::BindRepeating(
        &BoostEditorView::CloseEditor, base::Unretained(this)));
    close->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kPeekCloseIcon, SK_ColorWHITE, 11));
    close->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    close->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    close->SetPreferredSize(gfx::Size(22, 22));
    close->SetBackground(
        views::CreateRoundedRectBackground(SkColorSetRGB(0xAE, 0xAE, 0xB2), 11));
    close->SetTooltipText(u"Close Boost Editor");
    close->GetViewAccessibility().SetName(u"Close Boost Editor");
    views::InstallCircleHighlightPathGenerator(close.get());
    header->AddChildView(std::move(close));

    auto title = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&BoostEditorView::ShowTitleMenu,
                            base::Unretained(this)),
        u"My Boost \u2304");
    title->SetStyle(ui::ButtonStyle::kText);
    title->SetEnabledTextColors(kInkPrimary);
    title->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    title->SetElideBehavior(gfx::ELIDE_TAIL);
    title->SetMinSize(gfx::Size(0, 26));
    title->SetTooltipText(u"Boost options");
    title_button_ = header->AddChildView(std::move(title));
    header_layout->SetFlexForView(title_button_, 1);

    auto rename = std::make_unique<views::Textfield>();
    rename->SetVisible(false);
    rename->set_controller(this);
    rename->SetPlaceholderText(u"Boost name");
    rename_field_ = header->AddChildView(std::move(rename));
    header_layout->SetFlexForView(rename_field_, 1);

    auto reset = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&BoostEditorView::OnResetPressed,
                            base::Unretained(this)),
        u"\u21BA");
    reset->SetStyle(ui::ButtonStyle::kText);
    reset->SetEnabledTextColors(kInkSecondary);
    reset->SetCustomPadding(gfx::Insets::VH(2, 6));
    reset->SetMinSize(gfx::Size(26, 26));
    reset->SetTooltipText(u"Reset Boost");
    reset->GetViewAccessibility().SetName(u"Reset Boost");
    header->AddChildView(std::move(reset));

    // 2. 2D Color / Contrast Pad.
    color_pad_ = AddChildView(std::make_unique<ColorPadView>(
        base::BindRepeating(&BoostEditorView::OnPadChanged,
                            base::Unretained(this))));

    // 3. Three lighting / color mode buttons: Invert, Palette, Clear.
    auto* modes =
        AddChildView(MakeBox(Orientation::kHorizontal, gfx::Insets(), 8));
    auto* modes_layout =
        static_cast<views::BoxLayout*>(modes->GetLayoutManager());
    mode_invert_ = modes->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::SetColorMode,
                            base::Unretained(this),
                            foundation::BoostColorMode::Invert),
        u"\u25D0", std::u16string(), /*centered=*/true, /*font_size_delta=*/3));
    mode_invert_->SetTooltipText(u"Invert colors");
    mode_invert_->GetViewAccessibility().SetName(u"Invert colors");
    modes_layout->SetFlexForView(mode_invert_, 1);

    mode_palette_ = modes->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::SetColorMode,
                            base::Unretained(this),
                            foundation::BoostColorMode::Palette),
        u"\u2261", std::u16string(), /*centered=*/true, /*font_size_delta=*/3));
    mode_palette_->SetTooltipText(u"Color palette");
    mode_palette_->GetViewAccessibility().SetName(u"Color palette");
    modes_layout->SetFlexForView(mode_palette_, 1);

    mode_none_ = modes->AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::SetColorMode,
                            base::Unretained(this),
                            foundation::BoostColorMode::None),
        u"\u2298", std::u16string(), /*centered=*/true, /*font_size_delta=*/3));
    mode_none_->SetTooltipText(u"No color override");
    mode_none_->GetViewAccessibility().SetName(u"No color override");
    modes_layout->SetFlexForView(mode_none_, 1);

    // 4. 4x4 "Aa" typography preset grid.
    auto* grid_card = AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets(8), 2));
    grid_card->SetBackground(
        views::CreateRoundedRectBackground(kCardBg, 12));
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
    auto* type_row =
        AddChildView(MakeBox(Orientation::kHorizontal, gfx::Insets(), 8));
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
    zap_toggle_ = AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::ToggleZapSection,
                            base::Unretained(this)),
        u"Zap", u"\u26A1", /*centered=*/false));
    zap_toggle_->SetTooltipText(u"Hide page elements by CSS selector");

    zap_section_ = AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets::VH(4, 2), 6));
    zap_section_->SetVisible(false);
    {
      auto* input_row = zap_section_->AddChildView(
          MakeBox(Orientation::kHorizontal, gfx::Insets(), 4));
      auto* input_layout =
          static_cast<views::BoxLayout*>(input_row->GetLayoutManager());
      auto input = std::make_unique<views::Textfield>();
      input->SetPlaceholderText(u".banner, #ads");
      input->GetViewAccessibility().SetName(u"CSS selector to hide");
      input->set_controller(this);
      zap_input_ = input_row->AddChildView(std::move(input));
      input_layout->SetFlexForView(zap_input_, 1);

      auto add_btn = std::make_unique<views::MdTextButton>(
          base::BindRepeating(&BoostEditorView::AddZapSelector,
                              base::Unretained(this)),
          u"Hide");
      add_btn->SetStyle(ui::ButtonStyle::kTonal);
      add_btn->SetCustomPadding(gfx::Insets::VH(4, 10));
      input_row->AddChildView(std::move(add_btn));

      zap_error_label_ = zap_section_->AddChildView(MakeLabel(
          u"", gfx::FontList().Derive(-1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL),
          kErrorText));
      zap_error_label_->SetMultiLine(true);
      zap_error_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      zap_error_label_->SetVisible(false);

      zap_list_ = zap_section_->AddChildView(
          MakeBox(Orientation::kVertical, gfx::Insets(), 4));
    }

    // 7. Code ({}) button + expandable custom CSS editor.
    code_toggle_ = AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&BoostEditorView::ToggleCodeSection,
                            base::Unretained(this)),
        u"Code", u"{}", /*centered=*/false));
    code_toggle_->SetTooltipText(u"Edit custom CSS");

    code_section_ = AddChildView(
        MakeBox(Orientation::kVertical, gfx::Insets::VH(4, 2), 6));
    code_section_->SetVisible(false);
    {
      css_banner_ = code_section_->AddChildView(MakeLabel(
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

      auto editor = std::make_unique<views::Textarea>();
      editor->SetFontList(gfx::FontList(gfx::Font("Menlo", 11)));
      editor->SetPlaceholderText(u"/* Custom CSS */");
      editor->GetViewAccessibility().SetName(u"Custom CSS");
      editor->SetPreferredSize(gfx::Size(kPadWidth, 108));
      editor->set_controller(this);
      css_editor_ = code_section_->AddChildView(std::move(editor));

      auto* footer = code_section_->AddChildView(
          MakeBox(Orientation::kHorizontal, gfx::Insets(), 6));
      auto* footer_layout =
          static_cast<views::BoxLayout*>(footer->GetLayoutManager());
      footer_layout->set_cross_axis_alignment(CrossAxis::kCenter);
      css_bytes_label_ = footer->AddChildView(MakeLabel(
          u"0 / 64 KB",
          gfx::FontList().Derive(-2, gfx::Font::NORMAL,
                                 gfx::Font::Weight::NORMAL),
          kInkSecondary));
      css_bytes_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      footer_layout->SetFlexForView(css_bytes_label_, 1);

      auto apply = std::make_unique<views::MdTextButton>(
          base::BindRepeating(&BoostEditorView::CommitCustomCss,
                              base::Unretained(this)),
          u"Apply");
      apply->SetStyle(ui::ButtonStyle::kProminent);
      apply->SetCustomPadding(gfx::Insets::VH(4, 10));
      footer->AddChildView(std::move(apply));
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
      }
    }

    const std::u16string title_name =
        base::UTF8ToUTF16(std::string(record_->name.data(), record_->name.size()));
    if (title_button_) {
      title_button_->SetText(title_name + u" \u2304");
      title_button_->SetEnabledTextColors(record_->enabled ? kInkPrimary
                                                           : kInkSecondary);
    }

    if (color_pad_) {
      color_pad_->SetState(draft_appearance_.color_mode,
                           draft_appearance_.palette);
    }
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
      switch (draft_appearance_.text_transform) {
        case foundation::BoostTextTransform::None:
          break;
        case foundation::BoostTextTransform::Uppercase:
          label = u"CASE";
          break;
        case foundation::BoostTextTransform::Lowercase:
          label = u"case";
          break;
        case foundation::BoostTextTransform::Capitalize:
          label = u"Case Aa";
          break;
      }
      case_button_->SetText(label);
      case_button_->SetActive(draft_appearance_.text_transform !=
                              foundation::BoostTextTransform::None);
    }

    if (zap_toggle_) {
      const size_t count = record_->hidden_selectors.size();
      zap_toggle_->SetText(
          count == 0 ? u"Zap"
                     : u"Zap (" + base::NumberToString16(count) + u")");
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
          MakeBox(Orientation::kHorizontal, gfx::Insets::VH(2, 4), 4));
      auto* row_layout =
          static_cast<views::BoxLayout*>(row->GetLayoutManager());
      row_layout->set_cross_axis_alignment(CrossAxis::kCenter);
      row->SetBackground(views::CreateRoundedRectBackground(kPillBg, 6));

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
      remove->SetPreferredSize(gfx::Size(20, 20));
      remove->SetTooltipText(u"Restore element");
      remove->GetViewAccessibility().SetName(
          u"Restore " + base::UTF8ToUTF16(sel));
      row->AddChildView(std::move(remove));
    }
    if (!record_->hidden_selectors.empty()) {
      auto clear = std::make_unique<views::MdTextButton>(
          base::BindRepeating(&BoostEditorView::ClearZapSelectors,
                              base::Unretained(this)),
          u"Clear all");
      clear->SetStyle(ui::ButtonStyle::kText);
      clear->SetEnabledTextColors(kInkSecondary);
      clear->SetCustomPadding(gfx::Insets::VH(2, 6));
      zap_list_->AddChildView(std::move(clear));
    }
  }

  void UpdateCssValidationUi() {
    if (!css_banner_ || !css_bytes_label_) {
      return;
    }
    css_bytes_label_->SetText(
        base::NumberToString16(draft_css_.size()) + u" / 64 KB");

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
    draft_appearance_.palette =
        PadPositionToPalette(draft_appearance_.color_mode, x, y);
    if (color_pad_) {
      color_pad_->SetState(draft_appearance_.color_mode,
                           draft_appearance_.palette);
    }
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

  void SetColorMode(foundation::BoostColorMode mode) {
    draft_appearance_.color_mode = mode;
    if (mode != foundation::BoostColorMode::None &&
        !draft_appearance_.palette.set) {
      draft_appearance_.palette = PadPositionToPalette(mode, 0.55f, 0.25f);
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
    const bool show = !zap_section_->GetVisible();
    zap_section_->SetVisible(show);
    if (zap_toggle_) {
      zap_toggle_->SetActive(show);
    }
    if (show && zap_input_) {
      zap_input_->RequestFocus();
    }
    SizeToContents();
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

  void ToggleCodeSection() {
    if (!code_section_) {
      return;
    }
    const bool show = !code_section_->GetVisible();
    code_section_->SetVisible(show);
    if (code_toggle_) {
      code_toggle_->SetActive(show);
    }
    if (show && css_editor_) {
      css_editor_->RequestFocus();
    }
    SizeToContents();
  }

  void CommitCustomCss() {
    auto res = service_->SetCustomCss(boost_id_, draft_css_);
    if (!res.ok) {
      if (css_banner_) {
        css_banner_->SetText(base::UTF8ToUTF16(
            std::string(res.error.data(), res.error.size())));
        css_banner_->SetVisible(true);
        SizeToContents();
      }
      return;
    }
    css_dirty_ = false;
    ClearPreview();
    ReloadFromService();
  }

  void CommitPendingCssIfValid() {
    if (!css_dirty_ || !record_) {
      return;
    }
    if (BoostsService::InspectCss(draft_css_).empty()) {
      suppress_reload_ = true;
      service_->SetCustomCss(boost_id_, draft_css_);
      suppress_reload_ = false;
      css_dirty_ = false;
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
    menu_model_->AddCheckItem(kCmdToggleEnabled, u"Enabled");
    menu_model_->AddCheckItem(kCmdToggleSubdomains, u"Include Subdomains");
    menu_model_->AddCheckItem(kCmdToggleSubframes, u"Apply to Subframes");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kCmdRename, u"Rename Boost\u2026");
    menu_model_->AddItem(kCmdUndo, u"Undo Last Change");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kCmdDelete, u"Delete Boost");
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

  void CloseEditor() {
    if (GetWidget() && !GetWidget()->IsClosed()) {
      GetWidget()->Close();
    }
  }

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;
  const raw_ptr<BoostsService> service_;
  const uint64_t boost_id_;
  const std::string origin_;
  base::ScopedObservation<BoostsService, BoostsService::Observer>
      service_observation_{this};
  std::optional<foundation::BoostRecord> record_;
  foundation::BoostAppearanceRecord draft_appearance_{};
  std::string draft_css_;
  bool css_dirty_ = false;
  bool suppress_reload_ = false;

  raw_ptr<views::MdTextButton> title_button_ = nullptr;
  raw_ptr<views::Textfield> rename_field_ = nullptr;
  raw_ptr<ColorPadView> color_pad_ = nullptr;
  raw_ptr<PillButton> mode_invert_ = nullptr;
  raw_ptr<PillButton> mode_palette_ = nullptr;
  raw_ptr<PillButton> mode_none_ = nullptr;
  std::array<raw_ptr<FontPresetCell>, 16> font_cells_ = {};
  raw_ptr<PillButton> size_button_ = nullptr;
  raw_ptr<PillButton> case_button_ = nullptr;
  raw_ptr<PillButton> zap_toggle_ = nullptr;
  raw_ptr<views::View> zap_section_ = nullptr;
  raw_ptr<views::Textfield> zap_input_ = nullptr;
  raw_ptr<views::Label> zap_error_label_ = nullptr;
  raw_ptr<views::View> zap_list_ = nullptr;
  raw_ptr<PillButton> code_toggle_ = nullptr;
  raw_ptr<views::View> code_section_ = nullptr;
  raw_ptr<views::Textarea> css_editor_ = nullptr;
  raw_ptr<views::Label> css_banner_ = nullptr;
  raw_ptr<views::Label> css_bytes_label_ = nullptr;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
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
    CloseBoostEditor(browser);
    // Toggling via command/button with no specific new target closes the card.
    if (boost_id == 0 || boost_id == current_id) {
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
  slot->delegate = std::make_unique<views::BubbleDialogDelegate>(
      anchor, views::BubbleBorder::NONE);
  slot->delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  slot->delegate->SetShowCloseButton(false);
  slot->delegate->SetShowTitle(false);
  slot->delegate->SetTitle(u"Boost Editor");
  slot->delegate->set_margins(gfx::Insets(14));
  slot->delegate->set_corner_radius(kCardCornerRadius);
  slot->delegate->set_close_on_deactivate(false);
  slot->delegate->SetBackgroundColor(kCardBg);
  if (anchor) {
    const gfx::Rect anchor_screen = anchor->GetBoundsInScreen();
    const gfx::Rect target(anchor_screen.right() - kCardWidth - 16,
                           anchor_screen.y() + 64, kCardWidth, 1);
    slot->delegate->SetAnchorRect(target);
    slot->delegate->SetArrow(views::BubbleBorder::TOP_CENTER);
  }
  slot->view = slot->delegate->SetContentsView(std::make_unique<BoostEditorView>(
      browser, slot->delegate.get(), contents, service, boost_id, origin_str));
  slot->delegate->RegisterWindowClosingCallback(base::BindOnce(
      &BoostEditorView::OnWindowClosing, base::Unretained(slot->view)));
  slot->widget =
      views::BubbleDialogDelegate::CreateBubble(slot->delegate.get());
  views::Widget* raw_widget = slot->widget.get();
  editors[&browser] = std::move(slot);
  raw_widget->Show();
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
