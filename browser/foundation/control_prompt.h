#ifndef MYARC_CONTROL_PROMPT_H_
#define MYARC_CONTROL_PROMPT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace myarc {

// The card at a browser window's top-right that asks a yes or no question,
// such as whether a control client may use the Agent Space: "<client> wants
// to control your Agent Space", with Allow and Don't allow. It shows without
// activating the window. UI sequence only; `parent` must outlive it or close
// first.
class ControlPrompt : public views::WidgetObserver {
 public:
  enum class Answer { kAllow, kDecline, kClosed };
  using Done = base::OnceCallback<void(Answer)>;
  // The card's text. `accept` answers kAllow and `decline` kDecline.
  struct Copy {
    std::u16string title;
    std::u16string body;
    std::u16string decline;
    std::u16string accept;
  };

  // `client` is already cleaned for display. `done` runs once, with kClosed
  // when the window goes away first, and never after this object is gone.
  ControlPrompt(views::Widget& parent, const std::u16string& client, Done done);
  ControlPrompt(views::Widget& parent, const Copy& copy, Done done);
  ControlPrompt(const ControlPrompt&) = delete;
  ControlPrompt& operator=(const ControlPrompt&) = delete;
  ~ControlPrompt() override;

  // Presses accept (Allow) or decline (Don't allow), for test hooks.
  void PressForTesting(bool allow);

 private:
  void Finish(Answer answer);
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  std::unique_ptr<views::Widget> widget_;
  Done done_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
  base::WeakPtrFactory<ControlPrompt> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_CONTROL_PROMPT_H_
