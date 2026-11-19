#ifndef MYARC_WELCOME_VIEW_H_
#define MYARC_WELCOME_VIEW_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "myarc/sidebar_tint.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class BoxLayout;
class LabelButton;
}  // namespace views

namespace myarc {

// The welcome tour's first sheet. The view covers the whole window: it dims
// everything and paints the sheet over the page area, given by
// SetSheetBounds(), and only the sheet takes events. Three actions, live
// from the first frame: Start browsing, Import from Arc… and Skip tour. Tab
// and Shift+Tab cycle them, Return presses the focused one and Esc means Skip
// tour.
//
// On its first paint the sheet plays a one-second intro: colored dots drift
// in from the window's edges and meet in the middle, grow into the sheet, and
// a strip splits off to the sidebar's place while the text sharpens in.
// Under Reduce Motion the finished sheet fades in over 150 ms instead. A
// press ends either at its final frame first. Once an import commits, the
// colour field crossfades to the imported Space's theme. The reveal ends it:
// the sheet slides right as it fades and the dim lifts. UI sequence only.
class WelcomeView : public views::AccessiblePaneView,
                    public views::ViewTargeterDelegate,
                    public gfx::AnimationDelegate {
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

  // The page area, in this view's coordinates.
  void SetSheetBounds(const gfx::Rect& sheet);

  // Jumps the intro, or its reduced fade, to its final frame.
  void FinishIntro();

  // Ends the intro, shows Import from Arc… as done and crossfades the colour
  // field to `tint`, a Space's sidebar tint, in 350 ms, or 150 ms under
  // Reduce Motion. Runs `done` once the field holds the new theme.
  void CrossfadeTo(SidebarTint tint, base::OnceClosure done);

  // Starts the reveal's exit: the sheet slides 64 DIP right and fades, and
  // the dim lifts, in 350 ms, or only fades in 150 ms under Reduce Motion.
  // From here the sheet takes no events. Returns how long the exit takes;
  // the caller removes the sheet after it.
  base::TimeDelta Exit();

  // views::AccessiblePaneView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 protected:
  views::View* GetDefaultFocusableChild() override;

 private:
  void Press(Action action);
  void StartIntro();
  // Sets the text and buttons to the intro at `progress`, 0 to 1.
  void ApplyIntro(double progress);
  // How far the exit has moved the sheet right, and how much of it shows.
  float ExitShift() const;
  float ExitOpacity() const;

  const RadiusCallback radius_;
  const ActionCallback action_;
  raw_ptr<views::LabelButton> start_ = nullptr;
  raw_ptr<views::LabelButton> import_ = nullptr;
  raw_ptr<views::LabelButton> skip_ = nullptr;
  raw_ptr<views::View> title_ = nullptr;
  raw_ptr<views::View> body_ = nullptr;
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  gfx::Rect sheet_;
  // Read once, when the sheet is made.
  const bool reduced_motion_;
  bool intro_started_ = false;
  double progress_ = 0;
  gfx::LinearAnimation intro_;
  // The theme the field crossfades to, and how far, 0 to 1.
  std::optional<SidebarTint> theme_;
  double theme_progress_ = 0;
  gfx::LinearAnimation theme_fade_;
  base::OnceClosure theme_done_;
  // The reveal's exit, and how far it is, 0 to 1.
  bool exiting_ = false;
  double exit_progress_ = 0;
  gfx::LinearAnimation exit_;
  base::WeakPtrFactory<WelcomeView> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_WELCOME_VIEW_H_
