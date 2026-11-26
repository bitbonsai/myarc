#include "myarc/boosts_tab_helper.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace myarc {
namespace {

constexpr char kBoostStyleSheetKey[] = "myarc-boosts";

bool IsEligibleHttpOrigin(const url::Origin& origin) {
  return !origin.opaque() && (origin.scheme() == url::kHttpScheme ||
                              origin.scheme() == url::kHttpsScheme);
}

void SendBoostCss(content::RenderFrameHost* rfh, const std::string& css) {
  if (!rfh || !rfh->IsRenderFrameLive()) {
    return;
  }
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> remote;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&remote);
  if (remote.is_bound()) {
    remote->SetBoostStyleSheet(kBoostStyleSheetKey, css);
  }
}

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(BoostsTabHelper);

BoostsTabHelper::BoostsTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BoostsTabHelper>(*web_contents) {
  if (auto* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
    service_observation_.Observe(&BoostsService::ForProfile(*profile));
  }
}

BoostsTabHelper::~BoostsTabHelper() {
  service_observation_.Reset();
}

void BoostsTabHelper::SetPreviewOverride(std::string origin,
                                         std::string css,
                                         bool apply_to_subframes) {
  preview_ = PreviewOverride{
      .origin = std::move(origin),
      .css = std::move(css),
      .apply_to_subframes = apply_to_subframes,
  };
  SyncAllFrames();
}

void BoostsTabHelper::ClearPreviewOverride() {
  if (!preview_) {
    return;
  }
  preview_.reset();
  SyncAllFrames();
}

void BoostsTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }
  // BFCache restores may come back after Boosts were edited or toggled off
  // while the page was cached: re-sync all active frames immediately.
  if (navigation_handle->IsServedFromBackForwardCache()) {
    SyncAllFrames();
    return;
  }
  content::RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();
  if (!rfh) {
    return;
  }
  if (!navigation_handle->IsSameDocument()) {
    // New document: renderer cleared its injected style keys in
    // DidCreateNewDocument(), so drop stale tracking before syncing.
    styled_frames_.erase(rfh->GetGlobalId());
  }
  SyncFrame(rfh);
}

void BoostsTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host) {
    styled_frames_.erase(render_frame_host->GetGlobalId());
  }
}

void BoostsTabHelper::WebContentsDestroyed() {
  service_observation_.Reset();
  styled_frames_.clear();
  preview_.reset();
}

void BoostsTabHelper::OnBoostsChanged() {
  SyncAllFrames();
}

void BoostsTabHelper::SyncAllFrames() {
  if (!web_contents()) {
    return;
  }
  web_contents()->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* rfh) { SyncFrame(rfh); });
}

void BoostsTabHelper::SyncFrame(content::RenderFrameHost* rfh) {
  if (!rfh || !rfh->IsRenderFrameLive() || !rfh->IsActive()) {
    return;
  }
  const content::GlobalRenderFrameHostId frame_id = rfh->GetGlobalId();
  BoostsService* service = service_observation_.GetSource();
  const url::Origin origin = rfh->GetLastCommittedOrigin();
  const bool is_subframe = !rfh->IsInPrimaryMainFrame();

  std::string css;
  // Strict browser-side scheme/origin gate before any renderer IPC.
  if (service && IsEligibleHttpOrigin(origin)) {
    const std::string serialized = origin.Serialize();
    if (preview_ && preview_->origin == serialized &&
        (!is_subframe || preview_->apply_to_subframes)) {
      css = preview_->css;
    } else {
      css = service->CompileForOrigin(origin, is_subframe);
    }
  }

  if (css.empty()) {
    if (styled_frames_.erase(frame_id) > 0) {
      SendBoostCss(rfh, {});
    }
    return;
  }

  SendBoostCss(rfh, css);
  styled_frames_.insert(frame_id);
}

}  // namespace myarc
