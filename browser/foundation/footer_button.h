#ifndef MYARC_FOOTER_BUTTON_H_
#define MYARC_FOOTER_BUTTON_H_

#include <array>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/button/md_text_button.h"

namespace myarc {

// The Library button. It draws the user-designed design/icons/library-idle.svg
// and library-glow.svg with Skia, in the theme's foreground color, and morphs
// to the glow (sparkles rise out of the box under a light beam) while hovered
// or while the Library is open, as design/icons/library-morph.html does.
// Reduced motion jumps straight to the end state.
class LibraryButton : public views::MdTextButton {
  METADATA_HEADER(LibraryButton, views::MdTextButton)

 public:
  using views::MdTextButton::MdTextButton;
  ~LibraryButton() override;

  // Glows while `open`. `animate` false jumps to the new state.
  void SetOpen(bool open, bool animate = true);

 private:
  enum Channel {
    kColor,
    kBeamOpacity,
    kBeamScale,
    kBlur,
    kStar,
    kDot,
    kTriangle,
    kChannelCount
  };

  void UpdateGlow(bool animate);
  void SetHovered(bool hovered, bool animate);

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

  bool open_ = false;
  bool hovered_ = false;
  bool glowing_ = false;
  // Per channel progress from idle (0) to glow (1); overshoot leaves [0, 1].
  std::array<float, kChannelCount> from_ = {};
  std::array<float, kChannelCount> now_ = {};
  gfx::LinearAnimation animation_{this};
};

}  // namespace myarc

#endif  // MYARC_FOOTER_BUTTON_H_
