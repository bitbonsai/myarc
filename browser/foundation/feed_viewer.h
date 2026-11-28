#ifndef MYARC_FEED_VIEWER_H_
#define MYARC_FEED_VIEWER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace content {
class NavigationHandle;
class NavigationThrottleRegistry;
class WebContents;
}  // namespace content

namespace network {
struct ResourceRequest;
}  // namespace network

namespace myarc {

struct DiscoveredFeed {
  GURL url;
  std::string title;
  std::string mime_type;
};

// Tracks whether a tab is currently displaying the built-in myArc Feed Viewer
// so user-initiated article clicks open in Split View beside the feed, and
// discovers advertised RSS/Atom/JSON Feeds on normal web pages.
class FeedTabHelper final : public content::WebContentsObserver,
                            public content::WebContentsUserData<FeedTabHelper> {
 public:
  ~FeedTabHelper() override;

  // Records that the next committing navigation to `url` was rendered by the
  // built-in feed viewer, snapshots the outgoing page (if any) before the feed
  // commits, starts the X-axis slide transition, and invokes `on_ready`.
  void PreparePendingFeedTransition(const GURL& url,
                                    std::string title,
                                    base::OnceClosure on_ready);

  // Returns the active feed URL if the currently committed primary main-frame
  // document is a rendered myArc Feed Viewer page.
  const std::optional<GURL>& active_feed_url() const {
    return active_feed_url_;
  }
  const std::string& active_feed_title() const { return active_feed_title_; }
  const std::vector<DiscoveredFeed>& discovered_feeds() const {
    return discovered_feeds_;
  }

  // The link and ID of each item of the feed about to commit, recorded when
  // it is a subscription, so opening an item marks it read.
  void SetPendingFeedItems(std::vector<std::pair<GURL, std::string>> items);
  // Marks the item linking to `article_url` in the active feed read.
  void MarkArticleRead(const GURL& article_url);

  static bool IsShowingFeed(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

 private:
  friend class content::WebContentsUserData<FeedTabHelper>;

  explicit FeedTabHelper(content::WebContents* web_contents);
  void OnFeedsDiscovered(const GURL& page_url, base::Value result);

  std::optional<GURL> pending_feed_url_;
  std::string pending_feed_title_;
  std::optional<GURL> active_feed_url_;
  std::string active_feed_title_;
  std::vector<std::pair<GURL, std::string>> pending_feed_items_;
  std::vector<std::pair<GURL, std::string>> active_feed_items_;
  std::vector<DiscoveredFeed> discovered_feeds_;
  base::WeakPtrFactory<FeedTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Returns feeds advertised via <link rel="alternate"> on `web_contents`.
const std::vector<DiscoveredFeed>& GetDiscoveredFeeds(
    content::WebContents* web_contents);

// Returns true if `web_contents` advertises at least one feed that the active
// Profile is already subscribed to.
bool HasSubscribedDiscoveredFeed(content::WebContents* web_contents);

// Invokes the registered subscribe handler (persists subscription + updates
// the current Space's "Feeds" folder).
void TriggerFeedSubscribe(content::WebContents* contents,
                          const GURL& feed_url,
                          const std::string& title,
                          bool subscribe);

// Creates a URLLoaderThrottle that intercepts top-level RSS, Atom, and JSON
// Feed responses and renders them into the themed myArc Feed Viewer HTML.
std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateFeedViewerThrottle(
    const network::ResourceRequest& request,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter);

// Callback registered by BrowserWorkspaceAdapter/BrowserView to open an
// article clicked inside the Feed Viewer in Split View beside the feed tab.
using FeedOpenInSplitHandler = void (*)(content::WebContents* feed_contents,
                                        const GURL& article_url);
void SetFeedOpenInSplitHandler(FeedOpenInSplitHandler handler);

// Callback registered to persist a subscription and file it in the Space's
// "Feeds" folder when the user clicks Subscribe / Subscribed in the viewer.
using FeedSubscribeHandler = void (*)(content::WebContents* feed_contents,
                                      const GURL& feed_url,
                                      const std::string& title,
                                      bool subscribe);
void SetFeedSubscribeHandler(FeedSubscribeHandler handler);

// Adds a NavigationThrottle that routes user-initiated article clicks inside
// the Feed Viewer to Split View via the registered handler.
void MaybeAddFeedSplitThrottle(content::NavigationThrottleRegistry& registry);

}  // namespace myarc

#endif  // MYARC_FEED_VIEWER_H_
