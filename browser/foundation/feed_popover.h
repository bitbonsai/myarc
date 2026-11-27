#ifndef MYARC_FEED_POPOVER_H_
#define MYARC_FEED_POPOVER_H_

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace myarc {

// Opens (or closes if already open) the RSS feeds popover anchored to `anchor`
// for the feeds discovered on `web_contents`.
void ShowFeedPopover(views::View* anchor, content::WebContents* web_contents);

}  // namespace myarc

#endif  // MYARC_FEED_POPOVER_H_
