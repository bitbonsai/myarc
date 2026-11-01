#ifndef MYARC_DOWNLOADS_STACK_H_
#define MYARC_DOWNLOADS_STACK_H_

#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "myarc/sidebar_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

namespace myarc {

// Arc's recent downloads: up to 4 complete downloads that fan up out of the
// Library button while the pointer is over it, with no panel behind them. The
// button and the stack are one hover region. The sidebar places it with its
// bottom on the footer's top. On the UI sequence; the delegate outlives it.
class DownloadsStack : public views::View,
                       public views::AnimationDelegateViews {
  METADATA_HEADER(DownloadsStack, views::View)

 public:
  explicit DownloadsStack(SidebarView::Delegate& delegate);
  DownloadsStack(const DownloadsStack&) = delete;
  DownloadsStack& operator=(const DownloadsStack&) = delete;
  ~DownloadsStack() override;

  // The pointer entered or left the Library button.
  void SetAnchorHovered(bool hovered);
  // Hides at once, without the fade, as a press on the Library button does.
  void HideNow();
  // Shows at once and stays up, for captures.
  void ShowNow();
  // Reloads the rows while shown, for example once an icon loads.
  void Refresh();

 private:
  class Row;

  void UpdateHover();
  void Show();
  void Hide();
  // Fills the rows from the delegate. Returns false when there are none.
  bool LoadRows();
  void OpenRow(uint32_t id);
  void DragRow(uint32_t id);
  void LayoutRows();

  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  const raw_ref<SidebarView::Delegate> delegate_;
  bool anchor_hovered_ = false;
  bool hovered_ = false;
  bool hold_ = false;
  // Showing or shown, as opposed to hiding or hidden.
  bool shown_ = false;
  base::OneShotTimer show_delay_;
  base::OneShotTimer hide_delay_;
  // 0 hidden, 1 shown; linear, eased per row in LayoutRows.
  gfx::SlideAnimation animation_{this};
};

}  // namespace myarc

#endif  // MYARC_DOWNLOADS_STACK_H_
