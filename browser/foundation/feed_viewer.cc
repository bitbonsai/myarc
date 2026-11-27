#include "myarc/feed_viewer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "myarc/glass_mac.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "myarc/bridge.rs.h"
#include "myarc/feeds_service.h"
#include "myarc/local_page_marker.h"
#include "net/base/net_errors.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_decoding_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace myarc {
namespace {

FeedOpenInSplitHandler g_open_in_split_handler = nullptr;
FeedSubscribeHandler g_subscribe_handler = nullptr;

constexpr char kViewerCsp[] =
    "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; "
    "img-src https: http: data:; base-uri 'none'; form-action 'none';";

bool IsCandidateFeedMime(std::string_view mime_type) {
  return base::EqualsCaseInsensitiveASCII(mime_type, "application/rss+xml") ||
         base::EqualsCaseInsensitiveASCII(mime_type, "application/atom+xml") ||
         base::EqualsCaseInsensitiveASCII(mime_type, "application/rdf+xml") ||
         base::EqualsCaseInsensitiveASCII(mime_type, "text/rss+xml") ||
         base::EqualsCaseInsensitiveASCII(mime_type, "text/atom+xml") ||
         base::EqualsCaseInsensitiveASCII(mime_type, "application/feed+json") ||
         base::EqualsCaseInsensitiveASCII(mime_type, "application/xml") ||
         base::EqualsCaseInsensitiveASCII(mime_type, "text/xml");
}

bool IsAttachmentDownload(const network::mojom::URLResponseHead& head) {
  if (!head.headers) {
    return false;
  }
  std::optional<std::string> disposition =
      head.headers->GetNormalizedHeader("content-disposition");
  if (!disposition) {
    return false;
  }
  net::HttpContentDisposition parsed(*disposition, std::string());
  return parsed.is_attachment();
}

// Intercepts a candidate feed response, buffers up to `feed_max_bytes()`,
// delegates parsing/rendering to Rust, and streams either the rendered HTML
// viewer or the untouched original bytes back to Blink.
class FeedViewerURLLoaderThrottle final : public blink::URLLoaderThrottle,
                                          public mojo::DataPipeDrainer::Client,
                                          public network::mojom::URLLoaderClient {
 public:
  explicit FeedViewerURLLoaderThrottle(
      base::RepeatingCallback<content::WebContents*()> wc_getter)
      : wc_getter_(std::move(wc_getter)) {}

  ~FeedViewerURLLoaderThrottle() override = default;

  // blink::URLLoaderThrottle:
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override {
    if (!response_head || !response_url.SchemeIsHTTPOrHTTPS()) {
      return;
    }
    if (response_head->headers && response_head->headers->response_code() != 200) {
      return;
    }
    if (IsAttachmentDownload(*response_head)) {
      return;
    }
    // Chromium's MIME sniffer rewrites application/rss+xml and
    // application/atom+xml to text/plain so Blink displays them instead of
    // downloading; check the raw Content-Type header as well.
    std::string effective_mime = response_head->mime_type;
    if (!IsCandidateFeedMime(effective_mime)) {
      std::string header_mime;
      if (response_head->headers &&
          response_head->headers->GetMimeType(&header_mime) &&
          IsCandidateFeedMime(header_mime)) {
        effective_mime = std::move(header_mime);
      } else {
        return;
      }
    }

    const auto decoding_types = response_head->client_side_content_decoding_types;
    std::optional<network::ContentDecodingInterceptor::DataPipePair>
        decode_pair;
    if (!decoding_types.empty()) {
      decode_pair = network::ContentDecodingInterceptor::CreateDataPipePair(
          network::ContentDecodingInterceptor::ClientType::kURLLoaderThrottle);
      if (!decode_pair) {
        return;
      }
    }

    response_url_ = response_url;
    response_head_ = response_head;
    original_mime_type_ = std::move(effective_mime);
    max_bytes_ = foundation::feed_max_bytes();

    mojo::PendingRemote<network::mojom::URLLoader> dummy_loader;
    std::ignore = dummy_loader.InitWithNewPipeAndPassReceiver();
    mojo::PendingReceiver<network::mojom::URLLoaderClient> new_client_receiver =
        downstream_client_.BindNewPipeAndPassReceiver();

    mojo::ScopedDataPipeConsumerHandle new_consumer;
    if (mojo::CreateDataPipe(nullptr, output_producer_, new_consumer) !=
        MOJO_RESULT_OK) {
      return;
    }

    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        original_client_receiver;
    delegate_->InterceptResponse(std::move(dummy_loader),
                                 std::move(new_client_receiver),
                                 &original_loader_, &original_client_receiver,
                                 &new_consumer);

    // If the response still needs gzip/br/zstd decoding, decode it in the
    // sandboxed network service before buffering.
    if (!decoding_types.empty() && new_consumer.is_valid()) {
      auto endpoints = network::mojom::URLLoaderClientEndpoints::New(
          std::move(original_loader_), std::move(original_client_receiver));
      network::ContentDecodingInterceptor::InterceptOnNetworkService(
          *content::GetNetworkService(), decoding_types, endpoints,
          new_consumer, std::move(*decode_pair));
      original_loader_ = std::move(endpoints->url_loader);
      original_client_receiver = std::move(endpoints->url_loader_client);
      response_head_->client_side_content_decoding_types.clear();
    }

    // `new_consumer` now holds the (decoded) upstream body pipe.
    original_client_receiver_.Bind(std::move(original_client_receiver));
    original_client_receiver_.set_disconnect_handler(
        base::BindOnce(&FeedViewerURLLoaderThrottle::OnUpstreamDisconnect,
                       base::Unretained(this)));

    if (!new_consumer.is_valid()) {
      FinishAndResume();
      return;
    }

    *defer = true;
    drainer_ = std::make_unique<mojo::DataPipeDrainer>(this,
                                                       std::move(new_consumer));
  }

  const char* NameForLoggingWillProcessResponse() override {
    return "MyarcFeedViewer";
  }

  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    if (overflowed_) {
      return;
    }
    if (buffer_.size() + data.size() > max_bytes_) {
      overflowed_ = true;
      buffer_.clear();
      return;
    }
    buffer_.insert(buffer_.end(), data.begin(), data.end());
  }

  void OnDataComplete() override {
    drainer_.reset();
    drainer_done_ = true;
    MaybeFinish();
  }

  // network::mojom::URLLoaderClient (from original upstream loader):
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {}
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {}
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    std::move(callback).Run();
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    upstream_status_ = status;
    upstream_complete_ = true;
    MaybeFinish();
  }

 private:
  void OnUpstreamDisconnect() {
    if (!upstream_complete_) {
      upstream_complete_ = true;
      MaybeFinish();
    }
  }

  void MaybeFinish() {
    if (!drainer_done_ || !upstream_complete_) {
      return;
    }
    FinishAndResume();
  }

  void FinishAndResume() {
    std::string payload;
    std::string rendered_title;
    bool rendered_feed = false;

    if (!overflowed_ && upstream_status_.error_code == net::OK &&
        !buffer_.empty()) {
      bool show_subscribe = false;
      bool is_subscribed = false;
      if (wc_getter_) {
        if (content::WebContents* wc = wc_getter_.Run()) {
          if (Profile* profile =
                  Profile::FromBrowserContext(wc->GetBrowserContext())) {
            FeedsService& feeds_service = FeedsService::ForProfile(*profile);
            if (!feeds_service.IsEphemeral()) {
              show_subscribe = true;
              is_subscribed = feeds_service.IsSubscribed(response_url_.spec());
            }
          }
        }
      }
      rust::Slice<const uint8_t> slice(buffer_.data(), buffer_.size());
      foundation::FeedRenderResult result = foundation::render_feed_viewer(
          original_mime_type_, response_url_.spec(), slice, show_subscribe,
          is_subscribed);
      if (result.ok) {
        payload = std::string(result.html);
        rendered_title = std::string(result.title);
        rendered_feed = true;
      }
    }

    if (rendered_feed && response_head_) {
      response_head_->mime_type = "text/html";
      response_head_->charset = "utf-8";
      response_head_->client_side_content_decoding_types.clear();
      if (response_head_->headers) {
        response_head_->headers->RemoveHeader("content-type");
        response_head_->headers->AddHeader("Content-Type",
                                           "text/html; charset=utf-8");
        response_head_->headers->RemoveHeader("content-security-policy");
        response_head_->headers->RemoveHeader(
            "content-security-policy-report-only");
        response_head_->headers->AddHeader("Content-Security-Policy",
                                           kViewerCsp);
        response_head_->headers->RemoveHeader("content-length");
        response_head_->headers->RemoveHeader("content-encoding");
      }
      if (response_head_->parsed_headers) {
        response_head_->parsed_headers->content_security_policy.clear();
      }
    } else if (!overflowed_) {
      payload.assign(reinterpret_cast<const char*>(buffer_.data()),
                     buffer_.size());
    }

    buffer_.clear();
    buffer_.shrink_to_fit();

    network::URLLoaderCompletionStatus status = upstream_status_;
    if (rendered_feed) {
      status = network::URLLoaderCompletionStatus(net::OK);
      status.decoded_body_length = base::ByteSize(payload.size());
    }

    if (output_producer_.is_valid() && !payload.empty()) {
      auto producer =
          std::make_unique<mojo::DataPipeProducer>(std::move(output_producer_));
      auto* producer_ptr = producer.get();
      auto data_source = std::make_unique<mojo::StringDataSource>(
          base::span(payload),
          mojo::StringDataSource::AsyncWritingMode::
              STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION);
      producer_ptr->Write(
          std::move(data_source),
          base::BindOnce(
              [](std::unique_ptr<mojo::DataPipeProducer> producer,
                 mojo::Remote<network::mojom::URLLoaderClient> client,
                 network::URLLoaderCompletionStatus completion,
                 MojoResult) {
                producer.reset();
                if (client.is_bound()) {
                  client->OnComplete(completion);
                }
              },
              std::move(producer), std::move(downstream_client_), status));
    } else {
      output_producer_.reset();
      if (downstream_client_.is_bound()) {
        downstream_client_->OnComplete(status);
      }
    }

    response_head_ = nullptr;
    if (rendered_feed && wc_getter_) {
      if (content::WebContents* wc = wc_getter_.Run()) {
        if (auto* helper = FeedTabHelper::FromWebContents(wc)) {
          helper->PreparePendingFeedTransition(
              response_url_, std::move(rendered_title),
              base::BindOnce(&FeedViewerURLLoaderThrottle::ResumeAfterTransition,
                             weak_factory_.GetWeakPtr()));
          return;
        }
      }
    }
    delegate_->Resume();
  }

  void ResumeAfterTransition() {
    if (delegate_) {
      delegate_->Resume();
    }
  }

  base::RepeatingCallback<content::WebContents*()> wc_getter_;
  GURL response_url_;
  raw_ptr<network::mojom::URLResponseHead> response_head_ = nullptr;
  std::string original_mime_type_;
  size_t max_bytes_ = 0;
  bool overflowed_ = false;
  bool drainer_done_ = false;
  bool upstream_complete_ = false;
  std::vector<uint8_t> buffer_;
  network::URLLoaderCompletionStatus upstream_status_{net::OK};

  mojo::PendingRemote<network::mojom::URLLoader> original_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> original_client_receiver_{
      this};
  mojo::Remote<network::mojom::URLLoaderClient> downstream_client_;
  mojo::ScopedDataPipeProducerHandle output_producer_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  base::WeakPtrFactory<FeedViewerURLLoaderThrottle> weak_factory_{this};
};

class FeedSplitNavigationThrottle final : public content::NavigationThrottle {
 public:
  explicit FeedSplitNavigationThrottle(
      content::NavigationThrottleRegistry& registry)
      : content::NavigationThrottle(registry) {}

  ~FeedSplitNavigationThrottle() override = default;

  ThrottleCheckResult WillStartRequest() override {
    content::NavigationHandle* handle = navigation_handle();
    if (!g_open_in_split_handler || !handle || !handle->IsInPrimaryMainFrame() ||
        !handle->HasUserGesture() || !handle->IsRendererInitiated()) {
      return PROCEED;
    }
    const GURL& url = handle->GetURL();
    if (!url.SchemeIsHTTPOrHTTPS()) {
      return PROCEED;
    }
    content::WebContents* wc = handle->GetWebContents();
    auto* helper = wc ? FeedTabHelper::FromWebContents(wc) : nullptr;
    if (!helper || !helper->active_feed_url().has_value()) {
      return PROCEED;
    }
    if (url == *helper->active_feed_url()) {
      return PROCEED;
    }
    g_open_in_split_handler(wc, url);
    return CANCEL_AND_IGNORE;
  }

  const char* GetNameForLogging() override {
    return "MyarcFeedSplitNavigationThrottle";
  }
};

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(FeedTabHelper);

FeedTabHelper::FeedTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FeedTabHelper>(*web_contents) {}

FeedTabHelper::~FeedTabHelper() = default;

void FeedTabHelper::PreparePendingFeedTransition(const GURL& url,
                                                 std::string title,
                                                 base::OnceClosure on_ready) {
  pending_feed_url_ = url;
  pending_feed_title_ = std::move(title);

  const GURL current_url = web_contents()->GetLastCommittedURL();
  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (current_url.is_empty() || current_url == url || !rwhv) {
    std::move(on_ready).Run();
    return;
  }

  base::WeakPtr<content::WebContents> weak_contents =
      web_contents()->GetWeakPtr();
  rwhv->CopyFromSurface(
      gfx::Rect(), gfx::Size(), base::Milliseconds(250),
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> contents,
             base::OnceClosure ready,
             const content::CopyFromSurfaceResult& result) {
            if (contents && result.has_value()) {
              myarc::AnimateCardShuffle(contents->GetNativeView(),
                                        result->bitmap,
                                        /*reverse=*/false);
            }
            if (ready) {
              std::move(ready).Run();
            }
          },
          weak_contents, std::move(on_ready)));
}

// static
bool FeedTabHelper::IsShowingFeed(content::WebContents* web_contents) {
  auto* helper = web_contents ? FromWebContents(web_contents) : nullptr;
  return helper && helper->active_feed_url().has_value();
}

void FeedTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle || !navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  if (navigation_handle->IsSameDocument()) {
    if (active_feed_url_.has_value()) {
      std::string_view ref = navigation_handle->GetURL().ref();
      if (ref == "myarc-feed-back") {
        base::WeakPtr<content::WebContents> weak_contents =
            web_contents()->GetWeakPtr();
        auto go_back = [](base::WeakPtr<content::WebContents> contents) {
          if (!contents) {
            return;
          }
          content::NavigationController& nav = contents->GetController();
          const int current = nav.GetCurrentEntryIndex();
          if (current >= 2) {
            nav.GoToIndex(current - 2);
          } else if (nav.CanGoBack()) {
            nav.GoBack();
          }
        };
        if (content::RenderWidgetHostView* rwhv =
                web_contents()->GetRenderWidgetHostView()) {
          rwhv->CopyFromSurface(
              gfx::Rect(), gfx::Size(), base::Milliseconds(250),
              base::BindOnce(
                  [](base::WeakPtr<content::WebContents> contents,
                     const content::CopyFromSurfaceResult& result) {
                    if (!contents) {
                      return;
                    }
                    if (result.has_value()) {
                      myarc::AnimateCardShuffle(contents->GetNativeView(),
                                                result->bitmap,
                                                /*reverse=*/true);
                    }
                    content::NavigationController& nav =
                        contents->GetController();
                    const int current = nav.GetCurrentEntryIndex();
                    if (current >= 2) {
                      nav.GoToIndex(current - 2);
                    } else if (nav.CanGoBack()) {
                      nav.GoBack();
                    }
                  },
                  weak_contents));
        } else {
          go_back(weak_contents);
        }
        return;
      }
      if (g_subscribe_handler) {
        if (ref == "myarc-subscribe") {
          g_subscribe_handler(web_contents(), *active_feed_url_,
                              active_feed_title_, true);
        } else if (ref == "myarc-unsubscribe") {
          g_subscribe_handler(web_contents(), *active_feed_url_,
                              active_feed_title_, false);
        }
      }
    }
    return;
  }
  if (pending_feed_url_.has_value() &&
      navigation_handle->GetURL() == *pending_feed_url_) {
    active_feed_url_ = pending_feed_url_;
    active_feed_title_ = std::move(pending_feed_title_);
    pending_feed_url_.reset();
    pending_feed_title_.clear();
  } else {
    pending_feed_url_.reset();
    pending_feed_title_.clear();
    active_feed_url_.reset();
    active_feed_title_.clear();
  }
  if (!discovered_feeds_.empty()) {
    discovered_feeds_.clear();
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_URL);
  }
  if (navigation_handle->IsServedFromBackForwardCache() &&
      !active_feed_url_.has_value()) {
    DocumentOnLoadCompletedInPrimaryMainFrame();
  }
}

void FeedTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (active_feed_url_.has_value()) {
    return;
  }
  const GURL page_url = web_contents()->GetLastCommittedURL();
  if (!page_url.SchemeIsHTTPOrHTTPS() || IsLocalPage(page_url)) {
    return;
  }
  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    return;
  }
  constexpr char16_t kDiscoverFeedsScript[] =
      u"(() => {"
      u"  const out = [];"
      u"  const seen = new Set();"
      u"  const ok = new Set(['application/rss+xml','application/atom+xml',"
      u"    'application/feed+json','application/rdf+xml','text/rss+xml',"
      u"    'text/atom+xml','application/json+feed']);"
      u"  const links = document.querySelectorAll('link[rel][href]');"
      u"  for (const el of links) {"
      u"    if (out.length >= 8) break;"
      u"    const rels = (el.getAttribute('rel') || '').toLowerCase().split(/\\s+/);"
      u"    if (!rels.includes('alternate')) continue;"
      u"    const rawType = (el.getAttribute('type') || '').toLowerCase().split(';')[0].trim();"
      u"    if (!ok.has(rawType)) continue;"
      u"    const href = el.href;"
      u"    if (!href || seen.has(href)) continue;"
      u"    seen.add(href);"
      u"    out.push([href, (el.getAttribute('title') || '').trim(), rawType]);"
      u"  }"
      u"  if (out.length === 0) {"
      u"    const feedRe = /(?:\\/(?:rss|feed|atom|index)\\.xml|\\/feed\\.json|\\/(?:rss|feed|atom)\\/?)$/i;"
      u"    for (const a of document.querySelectorAll('a[href]')) {"
      u"      if (out.length >= 8) break;"
      u"      const href = a.href;"
      u"      if (!href || seen.has(href)) continue;"
      u"      let parsed;"
      u"      try { parsed = new URL(href); } catch { continue; }"
      u"      if (parsed.origin !== window.location.origin) continue;"
      u"      if (!feedRe.test(parsed.pathname)) continue;"
      u"      seen.add(href);"
      u"      const p = parsed.pathname.toLowerCase();"
      u"      const t = p.includes('atom') ? 'application/atom+xml' : (p.endsWith('.json') ? 'application/feed+json' : 'application/rss+xml');"
      u"      out.push([href, (a.textContent || '').trim(), t]);"
      u"    }"
      u"  }"
      u"  return out;"
      u"})()";
  rfh->ExecuteJavaScriptInIsolatedWorld(
      kDiscoverFeedsScript,
      base::BindOnce(&FeedTabHelper::OnFeedsDiscovered,
                     weak_factory_.GetWeakPtr(), page_url),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void FeedTabHelper::OnFeedsDiscovered(const GURL& page_url,
                                      base::Value result) {
  if (active_feed_url_.has_value() ||
      web_contents()->GetLastCommittedURL() != page_url ||
      !result.is_list()) {
    return;
  }
  std::vector<DiscoveredFeed> feeds;
  for (const base::Value& entry : result.GetList()) {
    if (!entry.is_list()) {
      continue;
    }
    const auto& pair = entry.GetList();
    if (pair.size() < 3 || !pair[0].is_string() || !pair[1].is_string() ||
        !pair[2].is_string()) {
      continue;
    }
    GURL feed_url(pair[0].GetString());
    if (!feed_url.is_valid() || !feed_url.SchemeIsHTTPOrHTTPS() ||
        IsLocalPage(feed_url)) {
      continue;
    }
    DiscoveredFeed item;
    item.url = std::move(feed_url);
    item.title = pair[1].GetString();
    item.mime_type = pair[2].GetString();
    feeds.push_back(std::move(item));
    if (feeds.size() >= 8) {
      break;
    }
  }
  if (feeds.empty() && discovered_feeds_.empty()) {
    return;
  }
  discovered_feeds_ = std::move(feeds);
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_URL);
}

const std::vector<DiscoveredFeed>& GetDiscoveredFeeds(
    content::WebContents* web_contents) {
  static const base::NoDestructor<std::vector<DiscoveredFeed>> kEmpty;
  if (!web_contents) {
    return *kEmpty;
  }
  FeedTabHelper* helper = FeedTabHelper::FromWebContents(web_contents);
  return helper ? helper->discovered_feeds() : *kEmpty;
}

bool HasSubscribedDiscoveredFeed(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  const auto& feeds = GetDiscoveredFeeds(web_contents);
  if (feeds.empty()) {
    return false;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return false;
  }
  FeedsService& service = FeedsService::ForProfile(*profile);
  for (const auto& feed : feeds) {
    if (service.IsSubscribed(feed.url.spec())) {
      return true;
    }
  }
  return false;
}

void TriggerFeedSubscribe(content::WebContents* contents,
                          const GURL& feed_url,
                          const std::string& title,
                          bool subscribe) {
  if (g_subscribe_handler && contents) {
    g_subscribe_handler(contents, feed_url, title, subscribe);
  }
}

std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateFeedViewerThrottle(
    const network::ResourceRequest& request,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter) {
  if (request.destination != network::mojom::RequestDestination::kDocument ||
      !request.url.SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }
  return std::make_unique<FeedViewerURLLoaderThrottle>(wc_getter);
}

void SetFeedOpenInSplitHandler(FeedOpenInSplitHandler handler) {
  g_open_in_split_handler = handler;
}

void SetFeedSubscribeHandler(FeedSubscribeHandler handler) {
  g_subscribe_handler = handler;
}

void MaybeAddFeedSplitThrottle(content::NavigationThrottleRegistry& registry) {
  if (!g_open_in_split_handler) {
    return;
  }
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame()) {
    return;
  }
  if (!FeedTabHelper::IsShowingFeed(handle.GetWebContents())) {
    return;
  }
  registry.AddThrottle(std::make_unique<FeedSplitNavigationThrottle>(registry));
}

}  // namespace myarc
