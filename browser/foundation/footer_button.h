#ifndef MYARC_FOOTER_BUTTON_H_
#define MYARC_FOOTER_BUTTON_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class ColorProvider;
}  // namespace ui

namespace views {
class EventMonitor;
class View;
class Widget;
}  // namespace views

namespace myarc {

// The footer icons' color, Arc's muted #A9A2AF over its dark sidebar.
// `active` is the brighter color of a lit icon, such as the glowing Library.
SkColor GetFooterIconColor(const ui::ColorProvider* provider, bool active);

// The current Space's glyph: 0.85 of the footer icons' color.
SkColor GetCurrentSpaceColor(const ui::ColorProvider* provider);

// Arc's footer tooltip: after a hover delay, a glass pill above the hovered
// button, or beside it, with its label and the keycaps of its shortcut. It
// fades in and out; while one is up, or just after, the next button's shows
// at once. A separate widget, so the footer's bounds don't clip it. One per
// sidebar, shared with its Library, on the UI sequence.
class FooterTooltip : public views::ViewObserver,
                      public views::WidgetObserver,
                      public ui::EventObserver,
                      public gfx::AnimationDelegate {
 public:
  struct Content {
    std::u16string label;
    // Keycap glyphs, for example u"⌃" and u"1". Only shortcuts that work.
    std::vector<std::u16string> keys;
    // Right of the button after a shorter delay, as Arc's Library tooltip,
    // clear of the downloads stack above it.
    bool beside = false;
    bool operator==(const Content&) const = default;
  };

  FooterTooltip();
  FooterTooltip(const FooterTooltip&) = delete;
  FooterTooltip& operator=(const FooterTooltip&) = delete;
  ~FooterTooltip() override;

  // The pointer entered `anchor`.
  void Hover(views::View& anchor, const Content& content);
  // The pointer left `anchor`.
  void Unhover(const views::View& anchor);
  // A press at `screen_point` hides the tooltip. A button under that point,
  // the pressed one or one the press put there, stays quiet until the pointer
  // leaves it.
  void Press(const gfx::Point& screen_point);
  // Shows `content` by `anchor` at once and keeps it up, for captures.
  void ShowNow(views::View& anchor, const Content& content);
  void Hide();

  base::WeakPtr<FooterTooltip> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Drops the anchor and its observers; the widget stays as it is.
  void Release();
  // Shows at full opacity unless `fade`.
  void Show(bool fade);
  void HideUnlessHeld();
  // Closes the widget at once, for the anchor or its window going away.
  void HideNow();
  // A tooltip is up or faded out a moment ago.
  bool IsWarm() const;

  // gfx::AnimationDelegate, for the fade:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // views::ViewObserver, for the anchor:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view,
                               bool visible) override;
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewRemovedFromWidget(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;
  // views::WidgetObserver, for the anchor's window:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  // ui::EventObserver, for key presses in the anchor's window:
  void OnEvent(const ui::Event& event) override;

  raw_ptr<views::View> anchor_ = nullptr;
  Content content_;
  bool hold_ = false;
  std::optional<gfx::Point> pressed_at_;
  base::OneShotTimer delay_;
  std::unique_ptr<views::Widget> widget_;
  gfx::SlideAnimation fade_{this};
  base::TimeTicks faded_at_;
  base::ScopedObservation<views::View, views::ViewObserver> anchor_observation_{
      this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      window_observation_{this};
  std::unique_ptr<views::EventMonitor> key_monitor_;
  base::WeakPtrFactory<FooterTooltip> weak_factory_{this};
};

// A footer button that shows a FooterTooltip instead of the native tooltip.
// It paints Arc's hover tile, which fades in and out, instead of an ink drop.
class FooterButton : public views::MdTextButton {
  METADATA_HEADER(FooterButton, views::MdTextButton)

 public:
  FooterButton(PressedCallback callback,
               std::u16string_view text,
               int button_context,
               bool use_text_color_for_icon);
  ~FooterButton() override;

  // Call again to change the content.
  void SetFooterTooltip(base::WeakPtr<FooterTooltip> tooltip,
                        FooterTooltip::Content content);
  const FooterTooltip::Content& footer_tooltip() const { return content_; }
  void ShowFooterTooltipNow();

 protected:
  bool hovered() const { return hovered_; }
  virtual void OnHoverChanged() {}
  // The tile stays lit while this is true, as the Create button's with its
  // menu open. Call UpdateTile() when it changes.
  virtual bool HoldsTile() const;
  void UpdateTile();

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

 private:
  // The tile's own delegate, as LibraryButton's morph has.
  class TileDelegate : public views::AnimationDelegateViews {
   public:
    explicit TileDelegate(FooterButton* button);
    void AnimationProgressed(const gfx::Animation* animation) override;

   private:
    const raw_ptr<FooterButton> button_;
  };

  void SetHovered(bool hovered);

  bool hovered_ = false;
  base::WeakPtr<FooterTooltip> tooltip_;
  FooterTooltip::Content content_;
  TileDelegate tile_delegate_{this};
  gfx::SlideAnimation tile_{&tile_delegate_};
};

// The Library button. It draws the user-designed design/icons/library-idle.svg
// and library-glow.svg with Skia, in the footer icons' color, and morphs to
// the glow (sparkles rise out of the box under a light beam) while hovered or
// while the Library is open, as design/icons/library-morph.html does. Reduced
// motion jumps straight to the end state.
class LibraryButton : public FooterButton {
  METADATA_HEADER(LibraryButton, FooterButton)

 public:
  LibraryButton(PressedCallback callback,
                std::u16string_view text,
                int button_context,
                bool use_text_color_for_icon);
  ~LibraryButton() override;

  // Runs with true when the pointer enters and false when it leaves.
  void SetHoverChangedCallback(base::RepeatingCallback<void(bool)> callback) {
    hover_changed_ = std::move(callback);
  }
  // Glows while `open`. `animate` false jumps to the new state.
  void SetOpen(bool open, bool animate = true);
  // Starts at the glow and animates to the current state, for the sidebar's
  // button right after the Library closes.
  void SettleFromGlow();

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

  // The morph's own delegate, so its animation container doesn't replace the
  // one Button's hover animation registered with Button's delegate.
  class MorphDelegate : public views::AnimationDelegateViews {
   public:
    explicit MorphDelegate(LibraryButton* button);
    void AnimationProgressed(const gfx::Animation* animation) override;

   private:
    const raw_ptr<LibraryButton> button_;
  };

  void UpdateGlow(bool animate);
  void MorphProgressed();

  // FooterButton:
  void OnHoverChanged() override;
  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  base::RepeatingCallback<void(bool)> hover_changed_;
  bool open_ = false;
  bool glowing_ = false;
  // Per channel progress from idle (0) to glow (1); overshoot leaves [0, 1].
  std::array<float, kChannelCount> from_ = {};
  std::array<float, kChannelCount> now_ = {};
  MorphDelegate morph_delegate_{this};
  gfx::LinearAnimation animation_{&morph_delegate_};
};

// The Create button. It draws the plus itself and turns it 45 degrees into a
// cross while the creation menu is open, holding its hover tile. Reduced
// motion turns it at once.
class CreateButton : public FooterButton {
  METADATA_HEADER(CreateButton, FooterButton)

 public:
  CreateButton(PressedCallback callback,
               std::u16string_view text,
               int button_context,
               bool use_text_color_for_icon);
  ~CreateButton() override;

  void SetOpen(bool open);

 private:
  // The turn's own delegate, as LibraryButton's morph has.
  class TurnDelegate : public views::AnimationDelegateViews {
   public:
    explicit TurnDelegate(CreateButton* button);
    void AnimationProgressed(const gfx::Animation* animation) override;

   private:
    const raw_ptr<CreateButton> button_;
  };

  // FooterButton:
  bool HoldsTile() const override;
  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  bool open_ = false;
  // Degrees.
  float angle_ = 0;
  float from_angle_ = 0;
  TurnDelegate turn_delegate_{this};
  gfx::LinearAnimation turn_{&turn_delegate_};
};

}  // namespace myarc

#endif  // MYARC_FOOTER_BUTTON_H_
