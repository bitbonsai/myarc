#ifndef MYARC_COPIED_URL_TOAST_H_
#define MYARC_COPIED_URL_TOAST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace myarc {

// Arc's "Copied Current URL" pill at the window's top-right. Presentation only;
// the caller writes the clipboard. One instance per browser window, on the UI
// sequence; `parent` must outlive it.
class CopiedUrlToast {
 public:
  explicit CopiedUrlToast(views::Widget* parent);
  CopiedUrlToast(const CopiedUrlToast&) = delete;
  CopiedUrlToast& operator=(const CopiedUrlToast&) = delete;
  ~CopiedUrlToast();

  // Shows or restarts the toast. `hold` keeps it up for background captures.
  void Show(bool hold = false);
  bool IsShowing() const;

 private:
  void FadeOut();
  void Hide();

  raw_ptr<views::Widget> parent_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> contents_ = nullptr;
  base::OneShotTimer timer_;
};

}  // namespace myarc

#endif  // MYARC_COPIED_URL_TOAST_H_
