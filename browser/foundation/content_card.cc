#include "myarc/content_card.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

// Room past the card for the shadow's blur: 3 sigma of blur 12, plus its
// 2 DIP drop.
constexpr int kShadowMargin = 20;

// A child view, so it follows the card's layer and bounds; the card's rounded
// mask clips nothing because the line stays inside it.
class ContentCardEdge : public views::View, public views::ViewObserver {
  METADATA_HEADER(ContentCardEdge, views::View)

 public:
  explicit ContentCardEdge(views::View& card) : card_(card) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetIsIgnored(true);
    observation_.Observe(&card);
    OnViewBoundsChanged(&card);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    const bool dark = color_utils::IsDark(
        GetColorProvider()->GetColor(ui::kColorSysSurface));
    gfx::RectF rect(GetLocalBounds());
    rect.Inset(0.5f);
    gfx::RoundedCornersF radii = card_->layer()->rounded_corner_radii();
    radii.Set(std::max(radii.upper_left() - 0.5f, 0.f),
              std::max(radii.upper_right() - 0.5f, 0.f),
              std::max(radii.lower_right() - 0.5f, 0.f),
              std::max(radii.lower_left() - 0.5f, 0.f));
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(SkColorSetA(dark ? SK_ColorWHITE : SK_ColorBLACK, 15));
    canvas->sk_canvas()->drawRRect(SkRRect(gfx::RRectF(rect, radii)), flags);
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* card) override {
    SetBoundsRect(card->GetLocalBounds());
    SchedulePaint();
  }

 private:
  const raw_ref<views::View> card_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

BEGIN_METADATA(ContentCardEdge)
END_METADATA

}  // namespace

void AddContentCardEdge(views::View& card) {
  card.AddChildView(std::make_unique<ContentCardEdge>(card));
}

ContentCardShadow::ContentCardShadow(views::View& cards,
                                     CardsCallback get_cards)
    : cards_(&cards), get_cards_(std::move(get_cards)) {
  SetCanProcessEventsWithinSubtree(false);
  GetViewAccessibility().SetIsIgnored(true);
  observations_.AddObservation(&cards);
  for (views::View* child : cards.children()) {
    observations_.AddObservation(child);
  }
}

ContentCardShadow::~ContentCardShadow() = default;

void ContentCardShadow::OnPaint(gfx::Canvas* canvas) {
  if (!cards_ || !cards_->GetVisible()) {
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(SK_ColorBLACK);
  flags.setLooper(gfx::CreateShadowDrawLooper({gfx::ShadowValue(
      gfx::Vector2d(0, 2), 12, SkColorSetA(SK_ColorBLACK, 102))}));
  const float dx = cards_->x() - x();
  const float dy = cards_->y() - y();
  for (const gfx::RRectF& card : get_cards_.Run()) {
    SkRRect rrect(card);
    rrect.offset(dx, dy);
    canvas->sk_canvas()->save();
    canvas->sk_canvas()->clipRRect(rrect, SkClipOp::kDifference, true);
    canvas->sk_canvas()->drawRRect(rrect, flags);
    canvas->sk_canvas()->restore();
  }
}

void ContentCardShadow::OnViewBoundsChanged(views::View* view) {
  Follow();
}

void ContentCardShadow::OnViewVisibilityChanged(views::View* view,
                                                views::View* starting_view,
                                                bool visible) {
  Follow();
}

void ContentCardShadow::OnViewIsDeleting(views::View* view) {
  observations_.RemoveObservation(view);
  if (view == cards_) {
    cards_ = nullptr;
  }
}

void ContentCardShadow::Follow() {
  if (!cards_) {
    return;
  }
  gfx::Rect bounds = cards_->bounds();
  bounds.Outset(kShadowMargin);
  SetBoundsRect(bounds);
  SchedulePaint();
}

BEGIN_METADATA(ContentCardShadow)
END_METADATA

}  // namespace myarc
