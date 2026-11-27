#ifndef MYARC_SITE_CONTROL_CENTER_VIEW_H_
#define MYARC_SITE_CONTROL_CENTER_VIEW_H_

class BrowserWindowInterface;

namespace views {
class View;
}

namespace myarc {

// Opens or toggles Arc's frosted-glass Site Control Center popover anchored
// below `anchor` (the URL bar's site-controls button). Returns false when the
// active tab has no committed or visible page.
bool ToggleSiteControlCenter(BrowserWindowInterface& browser,
                             views::View* anchor);

// Closes any open Site Control Center popover for `browser`.
void CloseSiteControlCenter(BrowserWindowInterface& browser);

// Returns whether the Site Control Center popover is currently open for
// `browser`.
bool IsSiteControlCenterShowing(BrowserWindowInterface& browser);

}  // namespace myarc

#endif  // MYARC_SITE_CONTROL_CENTER_VIEW_H_
