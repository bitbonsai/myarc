#ifndef MYARC_CONTENT_CARD_H_
#define MYARC_CONTENT_CARD_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace myarc {

// Gives `card` a 1 px hairline just inside its rounded edge: white at 6% in
// dark mode, black at 6% in light mode. `card` paints to a layer; the line
// sits above its children, so it draws over the page.
void AddContentCardEdge(views::View& card);

// A soft drop shadow (0 2 12, black at 40%) under the page cards, so they
// lift off the sidebar. It paints only outside the cards, so a translucent
// empty card stays clean. Add it to the cards' parent just below `cards`; it
// covers the parent and repaints when `cards` or its children move.
class ContentCardShadow : public views::View, public views::ViewObserver {
  METADATA_HEADER(ContentCardShadow, views::View)

 public:
  // The cards to shadow, in `cards`' coordinates, read at each paint.
  using CardsCallback = base::RepeatingCallback<std::vector<gfx::RRectF>()>;

  ContentCardShadow(views::View& cards, CardsCallback get_cards);
  ContentCardShadow(const ContentCardShadow&) = delete;
  ContentCardShadow& operator=(const ContentCardShadow&) = delete;
  ~ContentCardShadow() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override;
  void OnViewVisibilityChanged(views::View* view,
                               views::View* starting_view,
                               bool visible) override;
  void OnViewIsDeleting(views::View* view) override;

 private:
  void Follow();

  raw_ptr<views::View> cards_;
  CardsCallback get_cards_;
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      observations_{this};
};

}  // namespace myarc

#endif  // MYARC_CONTENT_CARD_H_
