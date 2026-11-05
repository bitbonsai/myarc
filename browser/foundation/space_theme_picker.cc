#include "myarc/space_theme_picker.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace myarc {

const std::array<SkColor, SpaceThemePickerView::kChipCount>
    SpaceThemePickerView::kChipColors = {
        SkColorSetRGB(0xF3, 0xEA, 0xE4), SkColorSetRGB(0xF2, 0x9B, 0xBB),
        SkColorSetRGB(0xA6, 0x72, 0x9E), SkColorSetRGB(0xF2, 0x5E, 0x6C),
        SkColorSetRGB(0xFF, 0x86, 0x57), SkColorSetRGB(0xF8, 0xD5, 0x58),
        SkColorSetRGB(0x33, 0xE8, 0x95), SkColorSetRGB(0x6D, 0xBA, 0xD9),
        SkColorSetRGB(0x66, 0x67, 0x89),
};

namespace {

constexpr std::array<const char16_t*, SpaceThemePickerView::kChipCount>
    kChipNames = {u"Cream",  u"Pink",  u"Purple", u"Red",  u"Orange",
                  u"Yellow", u"Green", u"Blue",   u"Slate"};

// Arc's chips are 24 DIP across, 29.5 DIP apart; the cell leaves room for
// the selection ring.
constexpr int kChipDiameter = 24;
constexpr int kChipCell = 30;
constexpr int kTrackHeight = 8;
constexpr int kKnobRadius = 7;
constexpr int kCheckerSize = 4;

bool IsDark(const ui::ColorProvider* provider) {
  return !provider ||
         color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
}

}  // namespace

// views::Button's constructor is protected.
class ThemeChip : public views::Button {
  METADATA_HEADER(ThemeChip, views::Button)

 public:
  ThemeChip(SkColor color, PressedCallback callback)
      : views::Button(std::move(callback)), color_(color) {
    SetPreferredSize(gfx::Size(kChipCell, kChipCell));
    views::InstallCircleHighlightPathGenerator(this);
  }

  void SetSelected(bool selected) {
    selected_ = selected;
    GetViewAccessibility().SetIsSelected(selected);
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    canvas->DrawCircle(center, kChipDiameter / 2.0f, flags);
    if (selected_) {
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(2);
      flags.setColor(IsDark(GetColorProvider())
                         ? SK_ColorWHITE
                         : SkColorSetRGB(0x33, 0x33, 0x33));
      canvas->DrawCircle(center, kChipDiameter / 2.0f + 2.5f, flags);
    }
  }

 private:
  const SkColor color_;
  bool selected_ = false;
};

BEGIN_METADATA(ThemeChip)
END_METADATA

// Arc's opacity slider: a checkerboard track fading into the colour and a
// round thumb.
class OpacitySlider : public views::Slider {
  METADATA_HEADER(OpacitySlider, views::Slider)

 public:
  explicit OpacitySlider(views::SliderListener* listener)
      : views::Slider(listener) {
    SetValueIndicatorRadius(kKnobRadius);
    SetPreferredSize(gfx::Size(0, kKnobRadius * 2 + 4));
  }

  void SetColor(SkColor color) {
    color_ = color;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const bool dark = IsDark(GetColorProvider());
    const gfx::Rect content = GetContentsBounds();
    const gfx::RectF track(content.x(),
                           content.CenterPoint().y() - kTrackHeight / 2.0f,
                           content.width(), kTrackHeight);
    {
      gfx::ScopedCanvas scoped(canvas);
      canvas->ClipPath(SkPath::RRect(gfx::RectFToSkRect(track),
                                     kTrackHeight / 2.0f, kTrackHeight / 2.0f),
                       true);
      const SkColor light = dark ? SkColorSetRGB(0x6E, 0x6E, 0x6E)
                                 : SkColorSetRGB(0xFF, 0xFF, 0xFF);
      const SkColor shade = dark ? SkColorSetRGB(0x2C, 0x2C, 0x2C)
                                 : SkColorSetRGB(0xCC, 0xCC, 0xCC);
      canvas->FillRect(gfx::ToEnclosingRect(track), light);
      for (int x = 0; x < track.width(); x += kCheckerSize) {
        for (int row = 0; row * kCheckerSize < kTrackHeight; ++row) {
          if ((x / kCheckerSize + row) % 2) {
            canvas->FillRect(
                gfx::Rect(track.x() + x, track.y() + row * kCheckerSize,
                          kCheckerSize, kCheckerSize),
                shade);
          }
        }
      }
      const SkPoint points[] = {{track.x(), 0}, {track.right(), 0}};
      const SkColor4f colors[] = {
          SkColor4f::FromColor(SkColorSetA(color_, 0)),
          SkColor4f::FromColor(SkColorSetA(color_, SK_AlphaOPAQUE))};
      cc::PaintFlags gradient;
      gradient.setShader(cc::PaintShader::MakeLinearGradient(
          points, colors, nullptr, 2, SkTileMode::kClamp));
      canvas->DrawRect(track, gradient);
    }
    const float travel = content.width() - 2.0f * kKnobRadius;
    const gfx::PointF thumb(content.x() + kKnobRadius + GetValue() * travel,
                            track.CenterPoint().y());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(dark ? SkColorSetRGB(0x3A, 0x3A, 0x3E) : SK_ColorWHITE);
    canvas->DrawCircle(thumb, kKnobRadius, flags);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.5f);
    flags.setColor(dark ? SkColorSetRGB(0xC8, 0xC8, 0xC8)
                        : SkColorSetRGB(0x8E, 0x8E, 0x93));
    canvas->DrawCircle(thumb, kKnobRadius - 0.75f, flags);
  }

 private:
  SkColor color_ = SK_ColorWHITE;
};

BEGIN_METADATA(OpacitySlider)
END_METADATA

SpaceThemePickerView::SpaceThemePickerView(
    const std::optional<SpaceTheme>& current,
    ChangeCallback change)
    : change_(std::move(change)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(14, 14), 12));
  auto* chips = AddChildView(std::make_unique<views::View>());
  chips->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  for (size_t i = 0; i < kChipCount; ++i) {
    chips_[i] = chips->AddChildView(std::make_unique<ThemeChip>(
        kChipColors[i], base::BindRepeating(&SpaceThemePickerView::PickChip,
                                            base::Unretained(this), i)));
    chips_[i]->GetViewAccessibility().SetName(kChipNames[i]);
    chips_[i]->SetTooltipText(kChipNames[i]);
    if (current && current->color == kChipColors[i]) {
      chip_ = i;
    }
  }
  if (current) {
    opacity_ = std::clamp(current->opacity, 0, 100);
  }

  auto* row = AddChildView(std::make_unique<views::View>());
  auto* row_layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(0, 2), 8));
  row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  percent_ = row->AddChildView(std::make_unique<views::Label>());
  percent_->SetFontList(
      gfx::FontList().Derive(1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  percent_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  percent_->SetAutoColorReadabilityEnabled(false);
  percent_->SetBackgroundColor(SK_ColorTRANSPARENT);
  percent_->SetPreferredSize(gfx::Size(44, 20));
  slider_ = row->AddChildView(std::make_unique<OpacitySlider>(this));
  slider_->GetViewAccessibility().SetName(u"Theme opacity");
  row_layout->SetFlexForView(slider_, 1);
  slider_->SetValue(opacity_ / 100.0f);
  Update();
}

SpaceThemePickerView::~SpaceThemePickerView() = default;

void SpaceThemePickerView::Select(size_t index, int opacity) {
  opacity_ = std::clamp(opacity, 0, 100);
  slider_->SetValue(opacity_ / 100.0f);
  PickChip(index);
}

void SpaceThemePickerView::OnThemeChanged() {
  views::View::OnThemeChanged();
  percent_->SetEnabledColor(SkColorSetA(
      IsDark(GetColorProvider()) ? SK_ColorWHITE : SK_ColorBLACK, 0x80));
}

void SpaceThemePickerView::SliderValueChanged(
    views::Slider* sender,
    float value,
    float old_value,
    views::SliderChangeReason reason) {
  const int opacity = static_cast<int>(std::lround(value * 100));
  if (opacity == opacity_) {
    return;
  }
  opacity_ = opacity;
  Update();
  if (chip_) {
    change_.Run({kChipColors[*chip_], opacity_});
  }
}

void SpaceThemePickerView::PickChip(size_t index) {
  if (index >= kChipCount) {
    return;
  }
  chip_ = index;
  Update();
  change_.Run({kChipColors[index], opacity_});
}

void SpaceThemePickerView::Update() {
  for (size_t i = 0; i < kChipCount; ++i) {
    chips_[i]->SetSelected(chip_ == i);
  }
  percent_->SetText(base::NumberToString16(opacity_) + u"%");
  slider_->SetColor(chip_ ? kChipColors[*chip_] : SK_ColorWHITE);
  slider_->SetEnabled(chip_.has_value());
}

BEGIN_METADATA(SpaceThemePickerView)
END_METADATA

}  // namespace myarc
