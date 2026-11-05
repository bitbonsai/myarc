#ifndef MYARC_SPACE_THEME_PICKER_H_
#define MYARC_SPACE_THEME_PICKER_H_

#include <array>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "myarc/sidebar_tint.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace myarc {

class ThemeChip;
class OpacitySlider;

// A cut-down Arc theme popover: a row of colour chips and, below it, an
// opacity slider. Each chip press or slider move runs `change`, so the owner
// can preview the theme; the slider waits for a chip.
class SpaceThemePickerView final : public views::View,
                                   public views::SliderListener {
  METADATA_HEADER(SpaceThemePickerView, views::View)

 public:
  using ChangeCallback = base::RepeatingCallback<void(SpaceTheme)>;
  static constexpr size_t kChipCount = 9;
  // Cream, pink, purple, red, orange, yellow, green, blue and slate, sampled
  // from Arc's popover.
  static const std::array<SkColor, kChipCount> kChipColors;

  SpaceThemePickerView(const std::optional<SpaceTheme>& current,
                       ChangeCallback change);
  ~SpaceThemePickerView() override;

  // Selects chip `index` and sets the opacity, as if the user had, for
  // background captures.
  void Select(size_t index, int opacity);

 private:
  // views::View:
  void OnThemeChanged() override;
  // views::SliderListener:
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

  void PickChip(size_t index);
  void Update();

  ChangeCallback change_;
  std::optional<size_t> chip_;
  int opacity_ = 100;
  std::array<raw_ptr<ThemeChip>, kChipCount> chips_ = {};
  raw_ptr<views::Label> percent_ = nullptr;
  raw_ptr<OpacitySlider> slider_ = nullptr;
};

}  // namespace myarc

#endif  // MYARC_SPACE_THEME_PICKER_H_
