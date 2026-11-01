#ifndef MYARC_CREATE_MENU_H_
#define MYARC_CREATE_MENU_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class SkPath;

namespace views {
class EventMonitor;
}  // namespace views

namespace myarc {

class CreateMenuRow;

// Arc's "+" creation menu: a navy panel with a notch, floating above the
// sidebar footer's Create button, with New Space, a separator, New Split and
// New Tab. New Tab is highlighted shortly after it opens; the pointer and the
// arrow keys move the highlight, Return chooses and Esc or a click elsewhere
// closes it. It fades and scales in and out from the notch. The sidebar owns
// and places it, hidden while closed. On the UI sequence.
class CreateMenu : public views::View,
                   public views::ViewTargeterDelegate,
                   public views::AnimationDelegateViews,
                   public views::WidgetObserver,
                   public ui::EventObserver {
  METADATA_HEADER(CreateMenu, views::View)

 public:
  enum class Item { kNewSpace, kNewSplit, kNewTab };

  // `select` runs for the chosen row once closing has started. `open_changed`
  // runs with true on opening and with false as soon as closing starts.
  // Neither may delete the menu synchronously.
  CreateMenu(base::RepeatingCallback<void(Item)> select,
             base::RepeatingCallback<void(bool)> open_changed);
  CreateMenu(const CreateMenu&) = delete;
  CreateMenu& operator=(const CreateMenu&) = delete;
  ~CreateMenu() override;

  // Opens and takes focus. Presses on `anchor`, the Create button, don't
  // count as outside clicks. `hold` keeps it open in an inactive window and
  // highlights New Tab at once, for captures.
  void Open(views::View& anchor, bool hold = false);
  // Starts closing. A click outside closes faster.
  void Close(bool fast = false);
  bool is_open() const { return open_; }

  // Sizes and positions the menu in its parent: the panel's right and bottom
  // edges, and the notch's tip, centered at `notch_x`.
  void Place(int panel_right, int panel_bottom, int notch_x);

 private:
  void Choose(size_t row);
  void SetHighlighted(std::optional<size_t> row);
  void MoveHighlight(int step);
  void UpdateHover(const gfx::Point& point);
  std::optional<size_t> RowAt(const gfx::Point& point) const;
  // Opacity and scale toward the notch.
  void Apply(float opacity, float scale);
  SkPath PanelPath() const;
  gfx::Rect PanelBounds() const;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnBlur() override;
  // views::ViewTargeterDelegate: only the panel takes events, not its shadow.
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;
  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  // ui::EventObserver, for presses anywhere in the window:
  using views::View::OnEvent;
  void OnEvent(const ui::Event& event) override;

  const base::RepeatingCallback<void(Item)> select_;
  const base::RepeatingCallback<void(bool)> open_changed_;
  std::vector<raw_ptr<CreateMenuRow>> rows_;
  std::optional<size_t> highlighted_;
  std::optional<size_t> pressed_;
  bool open_ = false;
  bool hold_ = false;
  int notch_x_ = 0;
  float opacity_ = 0;
  float scale_ = 1;
  float from_opacity_ = 0;
  float from_scale_ = 1;
  // Arc highlights New Tab ~100 ms after the menu shows.
  base::OneShotTimer default_highlight_;
  gfx::LinearAnimation animation_{this};
  views::ViewTracker anchor_;
  // Focus to give back on closing.
  views::ViewTracker previous_focus_;
  std::unique_ptr<views::EventMonitor> press_monitor_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace myarc

#endif  // MYARC_CREATE_MENU_H_
