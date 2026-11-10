#ifndef MYARC_SITE_ICON_FETCHER_H_
#define MYARC_SITE_ICON_FETCHER_H_

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;
class SkBitmap;

namespace network {
class SimpleURLLoader;
}

namespace myarc {

// Fetches the icon of a saved Favorite's or Pin's site when the Profile's
// favicon database has none for it, and stores it there as an on-demand
// favicon, which the page's own favicon replaces once it is visited. Requests
// go only to the saved URL's registrable domain, through the Profile's network
// context and without cookies or credentials. Images decode in the data
// decoder service. Each origin gets one attempt per browser session, failures
// included. UI sequence only; never exists for off-the-record Profiles.
class SiteIconFetcher : public base::SupportsUserData::Data {
 public:
  using StoredCallback = base::RepeatingCallback<void(const std::string&)>;

  // Null for off-the-record Profiles.
  static SiteIconFetcher* ForProfile(Profile& profile);

  explicit SiteIconFetcher(Profile& profile);
  SiteIconFetcher(const SiteIconFetcher&) = delete;
  SiteIconFetcher& operator=(const SiteIconFetcher&) = delete;
  ~SiteIconFetcher() override;

  // Fetches `page_url`'s site icon unless its origin was tried this session.
  // Ignores URLs that are not http or https.
  void Fetch(std::string_view page_url);

  // `callback` gets each page URL, as passed to Fetch, once its icon write
  // finished; look the page up in the favicon database again.
  base::CallbackListSubscription AddStoredCallback(StoredCallback callback);

 private:
  enum class State { kQueued, kRunning, kStored, kFailed };
  struct Site {
    Site();
    ~Site();
    State state = State::kQueued;
    std::set<std::string> pages;  // Waiting for this site's icon.
    std::unique_ptr<network::SimpleURLLoader> loader;
    int redirects = 0;
    bool tried_fallback = false;
    GURL icon_url;
    gfx::Image image;
  };

  void Pump();
  void Load(const url::Origin& origin, const GURL& url, bool page);
  void OnRedirect(const url::Origin& origin, const GURL& new_url);
  void OnPage(const url::Origin& origin, std::optional<std::string> body);
  void OnIcon(const url::Origin& origin, std::optional<std::string> body);
  void OnDecoded(const url::Origin& origin,
                 const GURL& icon_url,
                 const SkBitmap& bitmap);
  // Tries `/favicon.ico` once, then gives up on the site.
  void Fallback(const url::Origin& origin, const GURL& site_url);
  void Fail(const url::Origin& origin);
  void Finish(Site& site);
  void Store(const std::string& page_url, const Site& site);
  // A successful response from `site`'s current loader, or false.
  bool Succeeded(const Site& site) const;

  const raw_ref<Profile> profile_;
  std::map<url::Origin, Site> sites_;
  std::deque<url::Origin> queue_;
  size_t running_ = 0;
  base::RepeatingCallbackList<void(const std::string&)> stored_;
  base::WeakPtrFactory<SiteIconFetcher> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_SITE_ICON_FETCHER_H_
