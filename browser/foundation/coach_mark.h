#ifndef MYARC_COACH_MARK_H_
#define MYARC_COACH_MARK_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

namespace myarc {

// A welcome tour card that points at a real control: a short title, one line
// of text, an optional key hint, then Back, an optional Next and Skip tour.
// It never takes focus, so the shortcut it teaches still reaches the window,
// and Esc while it shows means Skip tour. VoiceOver hears its text when it
// shows. It grows in from 96% as it fades in, 180 ms with ease-out, from the
// side of its anchor; under Reduce Motion it only fades in, over 150 ms.
// UI sequence only.
class CoachMark : public views::View {
  METADATA_HEADER(CoachMark, views::View)

 public:
  enum class Action { kBack, kNext, kSkipTour };
  using ActionCallback = base::RepeatingCallback<void(Action)>;

  struct Content {
    std::u16string title;
    std::u16string body;
    // Empty for no hint.
    std::u16string hint;
    bool offers_back = false;
    bool offers_next = false;
  };

  CoachMark(const Content& content, ActionCallback action);
  CoachMark(const CoachMark&) = delete;
  CoachMark& operator=(const CoachMark&) = delete;
  ~CoachMark() override;

  // Places the card beside `anchor`, a rect in the parent's coordinates, and
  // inside `bounds`. An empty anchor puts it at the top left of `bounds`
  // with no pointer.
  void Place(const gfx::Rect& anchor, const gfx::Rect& bounds);

  // Fades the card in, growing it unless Reduce Motion is on, and announces
  // its text.
  void Show();

  // Presses a button, as the test hooks do.
  void Press(Action action);

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const ActionCallback action_;
  const std::u16string announcement_;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> body_ = nullptr;
  raw_ptr<views::Label> hint_ = nullptr;
  raw_ptr<views::LabelButton> back_ = nullptr;
  raw_ptr<views::LabelButton> next_ = nullptr;
  raw_ptr<views::LabelButton> skip_ = nullptr;
  // Where the pointer meets the card's left edge, in this view's
  // coordinates, or -1 for none.
  int pointer_y_ = -1;
  bool pressed_ = false;
  bool dark_ = true;
};

}  // namespace myarc

#endif  // MYARC_COACH_MARK_H_
