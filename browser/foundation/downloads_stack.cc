#include "myarc/downloads_stack.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/library_logic.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"

namespace myarc {
namespace {

// Measured from Arc's stack (DIP): contiguous 56 tall rows, the icon in a
// 40 wide box at the sidebar's inset, the text 12 after it. The name's cap top
// sits 13.6 above the row's center and the age's 17 below the name's.
constexpr int kRowHeight = 56;
constexpr int kIconBox = 40;
constexpr int kIconSize = 36;
constexpr int kTextLeft = kIconBox + 12;
constexpr float kNameCapTop = kRowHeight / 2.f - 13.6f;
constexpr int kAgeCapOffset = 17;
constexpr int kNameSize = 13;
constexpr int kAgeSize = 11;
constexpr SkAlpha kNameAlpha = 0xCC;
constexpr SkAlpha kAgeAlpha = 0x85;

constexpr base::TimeDelta kShowDelay = base::Milliseconds(100);
constexpr base::TimeDelta kHideDelay = base::Milliseconds(350);
constexpr base::TimeDelta kShowDuration = base::Milliseconds(150);
constexpr base::TimeDelta kHideDuration = base::Milliseconds(120);
constexpr base::TimeDelta kHoverPoll = base::Milliseconds(100);
// Each row up starts fading this much later, as a fraction of the show.
constexpr float kRowStagger = 0.07f;

gfx::FontList FontOfSize(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.Derive(size - base.GetFontSize(), gfx::Font::NORMAL, weight);
}

}  // namespace

// One download: its icon, name and age, painted at the stack's opacity.
class DownloadsStack::Row : public views::Button {
  METADATA_HEADER(Row, views::Button)

 public:
  Row(DownloadsStack& stack, const SidebarView::DownloadPresentation& download)
      : views::Button(base::BindRepeating(&DownloadsStack::OpenRow,
                                          base::Unretained(&stack),
                                          download.id)),
        stack_(stack),
        download_(download),
        name_font_(FontOfSize(kNameSize, gfx::Font::Weight::MEDIUM)),
        age_font_(FontOfSize(kAgeSize, gfx::Font::Weight::NORMAL)) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    // Shown only on hover; keyboard users have the Library's Downloads.
    SetFocusBehavior(FocusBehavior::NEVER);
    Update(download);
  }

  uint32_t id() const { return download_.id; }

  // Takes a newer presentation of the same download.
  void Update(const SidebarView::DownloadPresentation& download) {
    DCHECK_EQ(download.id, download_.id);
    download_ = download;
    age_ = library::AgeLabel(download_.time, base::Time::Now());
    GetViewAccessibility().SetName(download_.name + u", " + age_);
    SchedulePaint();
  }

  void SetAlpha(float alpha) {
    const SkAlpha value = static_cast<SkAlpha>(alpha * 0xFF + 0.5f);
    if (value != alpha_) {
      alpha_ = value;
      SchedulePaint();
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    press_point_ = event.location();
    dragging_ = false;
    return views::Button::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (!dragging_ && event.IsOnlyLeftMouseButton() &&
        ExceededDragThreshold(event.location() - press_point_)) {
      dragging_ = true;
      // The drag can delete this row.
      stack_->DragRow(download_.id);
      return true;
    }
    return views::Button::OnMouseDragged(event);
  }

  void OnMouseCaptureLost() override {
    // A file drag takes the capture, and the pointer may leave without an
    // exit event.
    stack_->RecheckHover();
    views::Button::OnMouseCaptureLost();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (!alpha_) {
      return;
    }
    const ui::ColorProvider* provider = GetColorProvider();
    const SkColor ink =
        !provider ||
                color_utils::IsDark(provider->GetColor(ui::kColorSysSurface))
            ? SK_ColorWHITE
            : SK_ColorBLACK;
    gfx::ScopedCanvas scoped(canvas);
    canvas->SaveLayerAlpha(alpha_);

    const gfx::Rect icon((kIconBox - kIconSize) / 2, (height() - kIconSize) / 2,
                         kIconSize, kIconSize);
    const gfx::ImageSkia image =
        download_.icon.IsEmpty()
            ? gfx::CreateVectorIcon(vector_icons::kInsertDriveFileOutlineOldIcon,
                                    28, SkColorSetA(ink, kAgeAlpha))
            : download_.icon.Rasterize(provider);
    if (!image.isNull()) {
      // Aspect-fit into the icon box.
      const float scale =
          std::min(static_cast<float>(icon.width()) / image.width(),
                   static_cast<float>(icon.height()) / image.height());
      const gfx::Size size(image.width() * scale, image.height() * scale);
      canvas->DrawImageInt(image, 0, 0, image.width(), image.height(),
                           icon.CenterPoint().x() - size.width() / 2,
                           icon.CenterPoint().y() - size.height() / 2,
                           size.width(), size.height(), true);
    }

    constexpr int kTextFlags = gfx::Canvas::NO_SUBPIXEL_RENDERING;
    const int text_width = std::max(0, width() - kTextLeft);
    const int name_baseline =
        static_cast<int>(kNameCapTop + name_font_.GetCapHeight() + 0.5f);
    const int age_baseline = static_cast<int>(kNameCapTop + kAgeCapOffset +
                                              age_font_.GetCapHeight() + 0.5f);
    canvas->DrawStringRectWithFlags(
        download_.name, name_font_, SkColorSetA(ink, kNameAlpha),
        gfx::Rect(kTextLeft, name_baseline - name_font_.GetBaseline(),
                  text_width, name_font_.GetHeight()),
        kTextFlags);
    canvas->DrawStringRectWithFlags(
        age_, age_font_, SkColorSetA(ink, kAgeAlpha),
        gfx::Rect(kTextLeft, age_baseline - age_font_.GetBaseline(), text_width,
                  age_font_.GetHeight()),
        kTextFlags);
  }

 private:
  const raw_ref<DownloadsStack> stack_;
  SidebarView::DownloadPresentation download_;
  std::u16string age_;
  const gfx::FontList name_font_;
  const gfx::FontList age_font_;
  SkAlpha alpha_ = 0;
  gfx::Point press_point_;
  bool dragging_ = false;
};

BEGIN_METADATA(DownloadsStack, Row)
END_METADATA

DownloadsStack::DownloadsStack(SidebarView::Delegate& delegate)
    : views::AnimationDelegateViews(this), delegate_(delegate) {
  // A layer keeps the rows above the sidebar's scrolling rows, which have
  // their own.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetNotifyEnterExitOnChild(true);
  SetVisible(false);
  animation_.SetTweenType(gfx::Tween::LINEAR);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Recent downloads");
}

DownloadsStack::~DownloadsStack() = default;

void DownloadsStack::SetAnchorHovered(bool hovered) {
  anchor_hovered_ = hovered;
  UpdateHover();
}

void DownloadsStack::HideNow() {
  show_delay_.Stop();
  hide_delay_.Stop();
  hover_poll_.Stop();
  hold_ = false;
  shown_ = false;
  hovered_ = false;
  animation_.Reset(0);
  SetVisible(false);
  RemoveAllChildViews();
  SetWatching(false);
}

void DownloadsStack::ShowNow() {
  HideNow();
  if (!LoadRows()) {
    return;
  }
  hold_ = true;
  shown_ = true;
  animation_.Reset(1);
  SetVisible(true);
  SetWatching(true);
  LayoutRows();
}

void DownloadsStack::Refresh() {
  if (!shown_) {
    return;
  }
  const auto rows = library::StackRows(delegate_->GetRecentDownloads());
  if (rows.empty()) {
    HideNow();
    return;
  }
  const bool same_downloads = std::ranges::equal(
      rows, children(), {}, &SidebarView::DownloadPresentation::id,
      [](const views::View* row) { return static_cast<const Row*>(row)->id(); });
  if (!same_downloads) {
    SetRows(rows);
    LayoutRows();
    return;
  }
  for (size_t i = 0; i < rows.size(); ++i) {
    static_cast<Row*>(children()[i])->Update(rows[i]);
  }
}

void DownloadsStack::UpdateHover() {
  if (hold_) {
    return;
  }
  if (anchor_hovered_ || hovered_) {
    hide_delay_.Stop();
    if (!shown_ && !show_delay_.IsRunning()) {
      show_delay_.Start(
          FROM_HERE, kShowDelay,
          base::BindOnce(&DownloadsStack::Show, base::Unretained(this)));
    }
    return;
  }
  show_delay_.Stop();
  if (shown_ && !hide_delay_.IsRunning()) {
    hide_delay_.Start(
        FROM_HERE, kHideDelay,
        base::BindOnce(&DownloadsStack::Hide, base::Unretained(this)));
  }
}

void DownloadsStack::RecheckHover() {
  hovered_ = GetVisible() &&
             GetBoundsInScreen().Contains(
                 display::Screen::Get()->GetCursorScreenPoint());
  if (!hovered_) {
    hover_poll_.Stop();
  } else if (!hover_poll_.IsRunning()) {
    hover_poll_.Start(FROM_HERE, kHoverPoll,
                      base::BindRepeating(&DownloadsStack::RecheckHover,
                                          base::Unretained(this)));
  }
  UpdateHover();
}

void DownloadsStack::Show() {
  // Already up while fading out: keep the rows and reverse.
  if (!GetVisible() && !LoadRows()) {
    return;
  }
  shown_ = true;
  SetVisible(true);
  SetWatching(true);
  if (gfx::Animation::PrefersReducedMotion()) {
    animation_.Reset(1);
  } else {
    animation_.SetSlideDuration(kShowDuration);
    animation_.Show();
  }
  LayoutRows();
}

void DownloadsStack::Hide() {
  shown_ = false;
  if (gfx::Animation::PrefersReducedMotion()) {
    HideNow();
    return;
  }
  animation_.SetSlideDuration(kHideDuration);
  animation_.Hide();
}

bool DownloadsStack::LoadRows() {
  SetRows(library::StackRows(delegate_->GetRecentDownloads()));
  return !children().empty();
}

void DownloadsStack::SetRows(
    const std::vector<SidebarView::DownloadPresentation>& rows) {
  RemoveAllChildViews();
  for (const auto& download : rows) {
    AddChildView(std::make_unique<Row>(*this, download));
  }
  PreferredSizeChanged();
}

void DownloadsStack::SetWatching(bool watching) {
  if (watching_ != watching) {
    watching_ = watching;
    delegate_->SetDownloadsStackShown(watching);
  }
}

void DownloadsStack::OpenRow(uint32_t id) {
  delegate_->OpenDownload(id);
}

void DownloadsStack::DragRow(uint32_t id) {
  const base::WeakPtr<DownloadsStack> weak = weak_factory_.GetWeakPtr();
  delegate_->DragDownload(id);
  if (weak) {
    RecheckHover();
  }
}

void DownloadsStack::LayoutRows() {
  // Rows fan up from the button: each row's center moves from the stack's
  // bottom to its place on the footer's rise curve, the bottom row first.
  static const gfx::CubicBezier rise(0.22, 1, 0.36, 1);
  const float level = animation_.GetCurrentValue();
  const float spread = rise.Solve(level);
  const int count = static_cast<int>(children().size());
  const float last_delay = kRowStagger * std::max(count - 1, 0);
  for (int i = 0; i < count; ++i) {
    const int center = i * kRowHeight + kRowHeight / 2;
    const float y = height() + (center - height()) * spread - kRowHeight / 2.f;
    auto* row = static_cast<Row*>(children()[i]);
    row->SetBounds(0, static_cast<int>(y + 0.5f), width(), kRowHeight);
    const float delay = kRowStagger * (count - 1 - i);
    row->SetAlpha(std::clamp((level - delay) / (1 - last_delay), 0.f, 1.f));
  }
}

void DownloadsStack::Layout(PassKey) {
  LayoutRows();
}

gfx::Size DownloadsStack::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(0, static_cast<int>(children().size()) * kRowHeight);
}

void DownloadsStack::OnMouseEntered(const ui::MouseEvent& event) {
  hovered_ = true;
  UpdateHover();
}

void DownloadsStack::OnMouseExited(const ui::MouseEvent& event) {
  hover_poll_.Stop();
  hovered_ = false;
  UpdateHover();
}

void DownloadsStack::AnimationProgressed(const gfx::Animation* animation) {
  LayoutRows();
}

void DownloadsStack::AnimationEnded(const gfx::Animation* animation) {
  LayoutRows();
  if (!shown_) {
    hover_poll_.Stop();
    SetVisible(false);
    RemoveAllChildViews();
    PreferredSizeChanged();
    SetWatching(false);
  }
}

BEGIN_METADATA(DownloadsStack)
END_METADATA

}  // namespace myarc
