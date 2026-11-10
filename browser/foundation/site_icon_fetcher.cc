#include "myarc/site_icon_fetcher.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "myarc/bridge.rs.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace myarc {
namespace {

const char kUserDataKey[] = "myarc_site_icon_fetcher";
// Sites fetched at once.
constexpr size_t kMaxRunning = 2;
constexpr int kMaxRedirects = 3;
constexpr base::TimeDelta kTimeout = base::Seconds(10);
constexpr size_t kMaxIconBytes = 256 * 1024;
// Decoded icons shrink to fit 128x128 RGBA.
constexpr uint64_t kMaxDecodedBytes = 128 * 128 * 4;
// The frame to pick from a multi-size .ico: a 16 DIP row at 2x.
constexpr int kIconPixels = 32;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("myarc_saved_site_icon", R"(
      semantics {
        sender: "myArc saved site icons"
        description:
          "Fetches the icon of a saved Favorite or Pin from the saved site "
          "itself when this Profile has never stored a favicon for it, for "
          "example after an import. Reads the start of the site's home page "
          "for a declared icon, then the icon or /favicon.ico."
        trigger:
          "The sidebar shows a saved Favorite or Pin whose page has no "
          "favicon in this Profile and whose site was not tried yet since "
          "the browser started."
        data:
          "None beyond the request itself. No cookies, credentials or page "
          "path are sent."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
        setting: "Remove the saved Favorite or Pin."
        policy_exception_justification: "Not implemented."
      })");

bool SameSite(const url::Origin& origin, const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() &&
         net::registry_controlled_domains::SameDomainOrHost(
             origin, url::Origin::Create(url),
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

SiteIconFetcher::Site::Site() = default;
SiteIconFetcher::Site::~Site() = default;

// static
SiteIconFetcher* SiteIconFetcher::ForProfile(Profile& profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (profile.IsOffTheRecord())
    return nullptr;
  auto* fetcher =
      static_cast<SiteIconFetcher*>(profile.GetUserData(kUserDataKey));
  if (!fetcher) {
    auto owned = std::make_unique<SiteIconFetcher>(profile);
    fetcher = owned.get();
    profile.SetUserData(kUserDataKey, std::move(owned));
  }
  return fetcher;
}

SiteIconFetcher::SiteIconFetcher(Profile& profile) : profile_(profile) {}

SiteIconFetcher::~SiteIconFetcher() = default;

base::CallbackListSubscription SiteIconFetcher::AddStoredCallback(
    StoredCallback callback) {
  return stored_.Add(std::move(callback));
}

void SiteIconFetcher::Fetch(std::string_view page_url) {
  const GURL url(page_url);
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      browser_shutdown::HasShutdownStarted()) {
    return;
  }
  const url::Origin origin = url::Origin::Create(url);
  auto [it, inserted] = sites_.try_emplace(origin);
  Site& site = it->second;
  switch (site.state) {
    case State::kFailed:
      return;
    case State::kStored:
      Store(std::string(page_url), site);
      return;
    case State::kQueued:
    case State::kRunning:
      site.pages.emplace(page_url);
      break;
  }
  if (inserted) {
    queue_.push_back(origin);
    Pump();
  }
}

void SiteIconFetcher::Pump() {
  while (running_ < kMaxRunning && !queue_.empty()) {
    const url::Origin origin = queue_.front();
    queue_.pop_front();
    sites_[origin].state = State::kRunning;
    ++running_;
    // The home page, so the saved path never leaves the browser.
    Load(origin, origin.GetURL(), /*page=*/true);
  }
}

void SiteIconFetcher::Load(const url::Origin& origin,
                           const GURL& url,
                           bool page) {
  Site& site = sites_[origin];
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = "GET";
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                             page ? "text/html" : "image/*");
  site.redirects = 0;
  site.loader = network::SimpleURLLoader::Create(std::move(request),
                                                 kTrafficAnnotation);
  site.loader->SetTimeoutDuration(kTimeout);
  // A cut-off page is fine: the icon link sits in its head.
  site.loader->SetAllowPartialResults(page);
  site.loader->SetOnRedirectCallback(base::BindRepeating(
      [](base::WeakPtr<SiteIconFetcher> fetcher, const url::Origin& origin,
         const GURL&, const net::RedirectInfo& info,
         const network::mojom::URLResponseHead&, std::vector<std::string>*) {
        if (fetcher)
          fetcher->OnRedirect(origin, info.new_url);
      },
      weak_factory_.GetWeakPtr(), origin));
  auto factory = profile_->GetDefaultStoragePartition()
                     ->GetURLLoaderFactoryForBrowserProcess();
  if (page) {
    site.loader->DownloadToString(
        factory.get(),
        base::BindOnce(&SiteIconFetcher::OnPage, weak_factory_.GetWeakPtr(),
                       origin),
        foundation::site_icon_scan_bytes());
  } else {
    site.loader->DownloadToString(
        factory.get(),
        base::BindOnce(&SiteIconFetcher::OnIcon, weak_factory_.GetWeakPtr(),
                       origin),
        kMaxIconBytes);
  }
}

void SiteIconFetcher::OnRedirect(const url::Origin& origin,
                                 const GURL& new_url) {
  Site& site = sites_[origin];
  if (++site.redirects > kMaxRedirects || !SameSite(origin, new_url))
    Fail(origin);  // Deletes the loader, which the callback allows.
}

bool SiteIconFetcher::Succeeded(const Site& site) const {
  const auto* info = site.loader->ResponseInfo();
  if (!info || !info->headers)
    return false;
  const int code = info->headers->response_code();
  return code >= 200 && code < 300;
}

void SiteIconFetcher::OnPage(const url::Origin& origin,
                             std::optional<std::string> body) {
  Site& site = sites_[origin];
  const GURL final_url = site.loader->GetFinalURL();
  const auto* info = site.loader->ResponseInfo();
  if (body && Succeeded(site) && info->mime_type == "text/html") {
    const std::string href(foundation::declared_site_icon(
        rust::Slice<const uint8_t>(
            reinterpret_cast<const uint8_t*>(body->data()), body->size())));
    const GURL icon = href.empty() ? GURL() : final_url.Resolve(href);
    if (SameSite(origin, icon)) {
      // Never ask for the same /favicon.ico twice.
      site.tried_fallback = icon.path() == "/favicon.ico";
      Load(origin, icon, /*page=*/false);
      return;
    }
  }
  Fallback(origin, SameSite(origin, final_url) ? final_url : origin.GetURL());
}

void SiteIconFetcher::OnIcon(const url::Origin& origin,
                             std::optional<std::string> body) {
  Site& site = sites_[origin];
  const GURL icon_url = site.loader->GetFinalURL();
  if (!body || body->empty() || !Succeeded(site)) {
    Fallback(origin, origin.GetURL());
    return;
  }
  site.loader.reset();
  data_decoder::DecodeImageIsolated(
      base::as_byte_span(*body), data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, kMaxDecodedBytes,
      gfx::Size(kIconPixels, kIconPixels),
      base::BindOnce(&SiteIconFetcher::OnDecoded, weak_factory_.GetWeakPtr(),
                     origin, icon_url));
}

void SiteIconFetcher::OnDecoded(const url::Origin& origin,
                                const GURL& icon_url,
                                const SkBitmap& bitmap) {
  Site& site = sites_[origin];
  if (bitmap.drawsNothing()) {
    Fallback(origin, origin.GetURL());
    return;
  }
  site.state = State::kStored;
  site.icon_url = icon_url;
  site.image = gfx::Image::CreateFrom1xBitmap(bitmap);
  for (const auto& page : site.pages)
    Store(page, site);
  Finish(site);
}

void SiteIconFetcher::Fallback(const url::Origin& origin,
                               const GURL& site_url) {
  Site& site = sites_[origin];
  const GURL icon = site_url.Resolve("/favicon.ico");
  if (site.tried_fallback || !SameSite(origin, icon)) {
    Fail(origin);
    return;
  }
  site.tried_fallback = true;
  Load(origin, icon, /*page=*/false);
}

void SiteIconFetcher::Fail(const url::Origin& origin) {
  Site& site = sites_[origin];
  site.state = State::kFailed;
  Finish(site);
}

void SiteIconFetcher::Finish(Site& site) {
  DCHECK_GT(running_, 0u);
  --running_;
  site.pages.clear();
  site.loader.reset();
  Pump();
}

void SiteIconFetcher::Store(const std::string& page_url, const Site& site) {
  if (browser_shutdown::HasShutdownStarted())
    return;
  auto* favicons = FaviconServiceFactory::GetForProfile(
      &*profile_, ServiceAccessType::IMPLICIT_ACCESS);
  if (!favicons)
    return;
  // A no-op when the page already has a favicon; a visit replaces this one.
  favicons->SetOnDemandFavicons(
      GURL(page_url), site.icon_url, favicon_base::IconType::kFavicon,
      site.image,
      base::BindOnce(
          [](base::WeakPtr<SiteIconFetcher> fetcher, std::string page_url,
             bool) {
            if (fetcher)
              fetcher->stored_.Notify(page_url);
          },
          weak_factory_.GetWeakPtr(), page_url));
}

}  // namespace myarc
