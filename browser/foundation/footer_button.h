#ifndef MYARC_FOOTER_BUTTON_H_
#define MYARC_FOOTER_BUTTON_H_

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/button/md_text_button.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace myarc {

// Arc's footer tooltip: after a hover delay, a navy pill above the hovered
// button with its label and the keycaps of its shortcut. A separate widget, so
// the footer's bounds don't clip it. One per sidebar, on the UI sequence.
class FooterTooltip {
 public:
  struct Content {
    std::u16string label;
    // Keycap glyphs, for example u"⌃" and u"1". Only shortcuts that work.
    std::vector<std::u16string> keys;
    bool operator==(const Content&) const = default;
  };

  FooterTooltip();
  FooterTooltip(const FooterTooltip&) = delete;
  FooterTooltip& operator=(const FooterTooltip&) = delete;
  ~FooterTooltip();

  // The pointer entered `anchor`. A replacement anchor for the content already
  // showing or pending takes it over without restarting the delay.
  void Hover(views::View& anchor, const Content& content);
  // The pointer left `anchor`.
  void Unhover(const views::View& anchor);
  // `anchor` is being destroyed. The tooltip waits briefly for a rebuilt
  // button to take it over, then hides.
  void Detach(const views::View& anchor);
  // A press hides the tooltip; `content` stays hidden until the pointer
  // leaves, even across a rebuilt button.
  void Press(const Content& content);
  // Shows `content` above `anchor` at once and keeps it up, for captures.
  void ShowNow(views::View& anchor, const Content& content);
  void Hide();

  base::WeakPtr<FooterTooltip> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool IsActive() const;
  void Show();

  raw_ptr<views::View> anchor_ = nullptr;
  Content content_;
  bool hold_ = false;
  bool pressed_ = false;
  base::OneShotTimer delay_;
  base::OneShotTimer orphan_;
  std::unique_ptr<views::Widget> widget_;
  base::WeakPtrFactory<FooterTooltip> weak_factory_{this};
};

// A footer button that shows a FooterTooltip instead of the native tooltip.
class FooterButton : public views::MdTextButton {
  METADATA_HEADER(FooterButton, views::MdTextButton)

 public:
  FooterButton(PressedCallback callback,
               std::u16string_view text,
               int button_context,
               bool use_text_color_for_icon);
  ~FooterButton() override;

  void SetFooterTooltip(base::WeakPtr<FooterTooltip> tooltip,
                        FooterTooltip::Content content);
  void ShowFooterTooltipNow();

 protected:
  bool hovered() const { return hovered_; }
  // `animate` is false when a rebuilt button finds the pointer already on it.
  virtual void OnHoverChanged(bool animate) {}

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void SetHovered(bool hovered, bool animate);

  bool hovered_ = false;
  base::WeakPtr<FooterTooltip> tooltip_;
  FooterTooltip::Content content_;
};

// The Library button. It draws the user-designed design/icons/library-idle.svg
// and library-glow.svg with Skia, in the theme's foreground color, and morphs
// to the glow (sparkles rise out of the box under a light beam) while hovered
// or while the Library is open, as design/icons/library-morph.html does.
// Reduced motion jumps straight to the end state.
class LibraryButton : public FooterButton {
  METADATA_HEADER(LibraryButton, FooterButton)

 public:
  LibraryButton(PressedCallback callback,
                std::u16string_view text,
                int button_context,
                bool use_text_color_for_icon);
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

  // FooterButton:
  void OnHoverChanged(bool animate) override;
  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

  bool open_ = false;
  bool glowing_ = false;
  // Per channel progress from idle (0) to glow (1); overshoot leaves [0, 1].
  std::array<float, kChannelCount> from_ = {};
  std::array<float, kChannelCount> now_ = {};
  gfx::LinearAnimation animation_{this};
};

}  // namespace myarc

#endif  // MYARC_FOOTER_BUTTON_H_
