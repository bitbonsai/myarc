#ifndef MYARC_BOOSTS_TAB_HELPER_H_
#define MYARC_BOOSTS_TAB_HELPER_H_

#include <optional>
#include <set>
#include <string>

#include "base/scoped_observation.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "myarc/boosts_service.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace myarc {

// Pure-CSS Boost stylesheet injector attached to each tab's WebContents.
//
// Observes the exact Profile's BoostsService (never redirecting OTR/Guest
// Profiles to GetOriginalProfile()) and pushes compiled author stylesheets to
// matching HTTP(S) frames via ChromeRenderFrame::SetBoostStyleSheet. Supports
// ephemeral live-preview overrides without mutating persisted Boosts.
class BoostsTabHelper final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BoostsTabHelper>,
      public BoostsService::Observer {
 public:
  ~BoostsTabHelper() override;

  // Ephemeral live preview for the active Boost editor. While set, frames
  // matching `origin` (canonical `scheme://host[:port]`) receive `css` instead
  // of persisted Boosts without writing to disk.
  void SetPreviewOverride(std::string origin,
                          std::string css,
                          bool apply_to_subframes = false,
                          bool auto_dark = false);
  void ClearPreviewOverride();
  bool HasPreviewOverride() const { return preview_.has_value(); }

  // Returns true when an active Boost (or live preview) for the primary main
  // frame's origin uses ColorMode::Invert, enabling Blink's native Auto Dark.
  bool IsAutoDarkEnabled() const;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // BoostsService::Observer:
  void OnBoostsChanged() override;

 private:
  friend class content::WebContentsUserData<BoostsTabHelper>;

  struct PreviewOverride {
    std::string origin;
    std::string css;
    bool apply_to_subframes = false;
    bool auto_dark = false;
  };

  explicit BoostsTabHelper(content::WebContents* web_contents);

  void SyncAllFrames();
  void SyncFrame(content::RenderFrameHost* rfh);
  void SyncAutoDark();

  base::ScopedObservation<BoostsService, BoostsService::Observer>
      service_observation_{this};
  // Frames that currently have a non-empty Boost stylesheet injected, so we
  // only send removal IPC when clearing a previously styled frame.
  std::set<content::GlobalRenderFrameHostId> styled_frames_;
  std::optional<PreviewOverride> preview_;
  bool auto_dark_active_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace myarc

#endif  // MYARC_BOOSTS_TAB_HELPER_H_
