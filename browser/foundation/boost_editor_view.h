#ifndef MYARC_BOOST_EDITOR_VIEW_H_
#define MYARC_BOOST_EDITOR_VIEW_H_

#include <cstdint>
#include <memory>

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace views {
class View;
}

namespace myarc {

// Returns whether `contents` is on an eligible HTTP(S) origin for Boosts.
bool CanBoostWebContents(content::WebContents* contents);

// Returns whether `contents` currently has at least one enabled Boost matching
// its committed HTTP(S) origin (and Profile-wide Boosts are enabled).
bool HasActiveBoostForWebContents(content::WebContents* contents);

// Opens or toggles the floating Boost Editor card anchored to the right edge
// of `browser`'s active page card. If `boost_id` is non-zero, edits that
// specific Boost; otherwise opens the first Boost matching the active tab's
// origin, creating "My Boost" for that origin if none exists yet.
// Returns false when the active tab is not an eligible HTTP(S) page or the
// Boosts store is unavailable.
bool ShowBoostEditor(BrowserWindowInterface& browser, uint64_t boost_id = 0);

// Closes any open Boost Editor bubble for `browser`.
void CloseBoostEditor(BrowserWindowInterface& browser);

// Page Info row ("Boost this site" / "Edit Boost…") placed beside the ad
// blocker switch in PageInfoMainView. Returns nullptr on non-HTTP(S) pages.
std::unique_ptr<views::View> CreateBoostPageInfoRow(
    content::WebContents* contents);

}  // namespace myarc

#endif  // MYARC_BOOST_EDITOR_VIEW_H_
