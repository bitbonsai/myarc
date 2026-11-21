#ifndef MYARC_WELCOME_VIEW_H_
#define MYARC_WELCOME_VIEW_H_

#include <array>
#include <cstdint>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "myarc/sidebar_tint.h"
#include "myarc/welcome_ribbon.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/accessible_pane_view.h"

namespace views {
class ImageView;
class Label;
class LabelButton;
}  // namespace views

namespace myarc {

// The welcome tour's first screen: it covers the whole browser window, so
// the sidebar and toolbar stay hidden until the reveal. Its background is
// myarc.dev's midnight with large blurred fields of the ribbon logo's
// colours and the site blue, drifting on 20 to 30 s loops.
//
// Play() starts the arrival on welcome_timeline.h's times, which the sound
// sets. At 1.7 s the ribbon logo rises 20 DIP while it fades in over 1.1 s
// on the site's ease, as on the myarc.dev hero, drifts once in 0.8 s (up
// 14 DIP and back), then moves up over 0.5 s to the top of a centred stack
// above the headline and the choice, where it stays. At 4.3 s
// "myArc, forever yours." slides in from the left while its blur clears,
// and 0.4 s later the choice fades in under it: Import from Arc…, the primary
// button, Start fresh and Skip tour. When Arc's sidebar file is there, the
// primary button reads Import under "Found your Arc data", and Choose a
// file… sits beside Skip tour. Return presses the focused button, which
// starts on the primary one, Tab and Shift+Tab cycle them and Esc means Skip
// tour. Under Reduce Motion nothing moves: Play() fades the logo, the
// headline and the choice in over 150 ms.
//
// Every change is a layer animation, so the compositor runs it at the
// display's rate. Once an import commits, the background crossfades to the
// imported Space's theme. The reveal ends it: the content slides right as
// the whole screen fades. UI sequence only.
class WelcomeView : public views::AccessiblePaneView {
  METADATA_HEADER(WelcomeView, views::AccessiblePaneView)

 public:
  enum class Action { kStartBrowsing, kImport, kChooseFile, kSkipTour };
  using ActionCallback = base::RepeatingCallback<void(Action)>;

  // `offers_import` false shows Import from Arc… as already done, and Start
  // fresh becomes the primary button.
  WelcomeView(bool offers_import, ActionCallback action);
  WelcomeView(const WelcomeView&) = delete;
  WelcomeView& operator=(const WelcomeView&) = delete;
  ~WelcomeView() override;

  // Puts focus on the primary button, keeps Tab inside the window and says
  // what it shows.
  void TakeFocus();

  // The background's base, myarc.dev's midnight or its dark page, as
  // 0xRRGGBB.
  uint32_t BaseColor() const;

  // Starts the arrival and, once the window shows, the background's drift,
  // on welcome_timeline.h's times from `start`, its t = 0.
  void Play(base::TimeTicks start);

  // Jumps the arrival to its end, with the headline and the choice showing.
  void FinishIntro();

  // Arc's sidebar file is there: "Found your Arc data" shows over the primary
  // button, which reads Import and imports that file without the file
  // picker, and Choose a file… shows beside Skip tour for the picker. Does
  // nothing once the import is done.
  void ShowFoundArcData();

  // Ends the arrival, shows Import from Arc… as done and crossfades the
  // background to `tint`, a Space's sidebar tint, in 350 ms, or 150 ms under
  // Reduce Motion. Runs `done` once it holds the new theme.
  void CrossfadeTo(SidebarTint tint, base::OnceClosure done);

  // Starts the reveal's exit: the content slides 64 DIP right and the whole
  // screen fades, in 350 ms, or only fades in 150 ms under Reduce Motion.
  // From here it takes no events. Returns how long the exit takes; the
  // caller removes the view after it.
  base::TimeDelta Exit();

  // A click anywhere then means Skip tour, for the intro's replay.
  void SetClosesOnClick() { closes_on_click_ = true; }

  // views::AccessiblePaneView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout(PassKey) override;

 protected:
  views::View* GetDefaultFocusableChild() override;

 private:
  void Press(Action action);
  // Starts the fields' drift once the view has a size and is playing.
  void StartDrift();
  // The ribbon video's frames, one per tick, from its start.
  void StartRibbon();
  void NextRibbonFrame();
  // The logo's move from the centre to the stack's top, from the layout.
  gfx::Transform MarkDocked() const;

  const ActionCallback action_;
  // The background's blurred fields, and the imported theme over them.
  std::array<raw_ptr<views::View>, 3> fields_ = {};
  raw_ptr<views::View> theme_ = nullptr;
  // Everything the exit slides: the logo, the headline and its blurred copy,
  // which fades out as the headline sharpens, and the choice.
  raw_ptr<views::View> content_ = nullptr;
  raw_ptr<views::ImageView> mark_ = nullptr;
  raw_ptr<views::Label> headline_ = nullptr;
  raw_ptr<views::Label> headline_blur_ = nullptr;
  raw_ptr<views::Label> found_ = nullptr;
  raw_ptr<views::LabelButton> import_ = nullptr;
  raw_ptr<views::LabelButton> start_ = nullptr;
  raw_ptr<views::LabelButton> choose_ = nullptr;
  raw_ptr<views::LabelButton> skip_ = nullptr;
  // Read once, when the view is made.
  const bool reduced_motion_;
  bool playing_ = false;
  bool drifting_ = false;
  // The logo has been sent to its spot above the headline, or starts there
  // under Reduce Motion.
  bool mark_docked_ = false;
  // The top of the centred stack of logo, headline and choice.
  int mark_top_ = 0;
  // The logo's view: kMarkSize for the PNG, larger for the ribbon's frames.
  int mark_view_size_ = 0;
  // The 3D ribbon while it plays, then null; null without the video.
  std::unique_ptr<WelcomeRibbon> ribbon_;
  base::OneShotTimer ribbon_start_timer_;
  base::RepeatingTimer ribbon_timer_;
  bool exiting_ = false;
  bool closes_on_click_ = false;
  // Starts the drift once the window shows.
  base::OneShotTimer drift_timer_;
  // Runs CrossfadeTo's `done` once the theme holds.
  base::OneShotTimer theme_timer_;
};

}  // namespace myarc

#endif  // MYARC_WELCOME_VIEW_H_
