#ifndef MYARC_WELCOME_VIEW_H_
#define MYARC_WELCOME_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"

namespace views {
class LabelButton;
}  // namespace views

namespace myarc {

// The welcome tour's first sheet, over a window's page area. Three actions,
// live from the first frame: Start browsing, Import from Arc… and Skip tour.
// Tab and Shift+Tab cycle them, Return presses the focused one and Esc means
// Skip tour. No motion yet. UI sequence only.
class WelcomeView : public views::AccessiblePaneView {
  METADATA_HEADER(WelcomeView, views::AccessiblePaneView)

 public:
  enum class Action { kStartBrowsing, kImport, kSkipTour };
  using ActionCallback = base::RepeatingCallback<void(Action)>;
  using RadiusCallback = base::RepeatingCallback<float()>;

  // `offers_import` false shows Import from Arc… as already done. `radius`
  // gives the page area's corner radius, so the sheet rounds like it.
  WelcomeView(bool offers_import, RadiusCallback radius, ActionCallback action);
  WelcomeView(const WelcomeView&) = delete;
  WelcomeView& operator=(const WelcomeView&) = delete;
  ~WelcomeView() override;

  // Puts focus on Start browsing, keeps Tab inside the sheet and says what
  // the sheet is.
  void TakeFocus();

  // views::AccessiblePaneView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 protected:
  views::View* GetDefaultFocusableChild() override;

 private:
  void Press(Action action);

  const RadiusCallback radius_;
  const ActionCallback action_;
  raw_ptr<views::LabelButton> start_ = nullptr;
  raw_ptr<views::LabelButton> import_ = nullptr;
  raw_ptr<views::LabelButton> skip_ = nullptr;
  raw_ptr<views::View> title_ = nullptr;
  raw_ptr<views::View> body_ = nullptr;
};

}  // namespace myarc

#endif  // MYARC_WELCOME_VIEW_H_
