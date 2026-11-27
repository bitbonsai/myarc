#ifndef MYARC_PEEK_H_
#define MYARC_PEEK_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "myarc/bridge.rs.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view_observer.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace content {
class NavigationThrottleRegistry;
}

namespace views {
class View;
}

namespace myarc {

class BrowserWorkspaceAdapter;
class PeekView;

// Peek (BC-005): a link from a pinned tab or Favorite to another site opens
// in a rounded card over the page, which dims behind it. The card holds its
// own WebContents, which never becomes a tab or a workspace row unless the
// user expands it. One per browser window with a workspace adapter, owned by
// BrowserView and destroyed before the adapter. UI thread only.
class PeekController final : public TabStripModelObserver,
                             public content::WebContentsDelegate,
                             public views::ViewObserver {
 public:
  // `host` holds the overlay over `area`, the window's page area; both must
  // outlive this.
  PeekController(BrowserWindowInterface& browser,
                 views::View& host,
                 views::View& area,
                 BrowserWorkspaceAdapter& adapter);
  PeekController(const PeekController&) = delete;
  PeekController& operator=(const PeekController&) = delete;
  ~PeekController() override;

  static PeekController* ForBrowser(BrowserWindowInterface& browser);
  // Adds the throttle that sends Peek links here. Set as the Peek hook's
  // factory.
  static void MaybeAddThrottle(content::NavigationThrottleRegistry& registry);

  bool is_open() const { return !!contents_; }
  // Whether Cmd+Shift+T would bring back the Peek closed last.
  bool CanReopen() const;
  // Opens an article clicked in the built-in Feed Viewer in Split View beside
  // `feed_contents` (reusing the other pane if already split).
  static void OpenFeedArticleInSplit(content::WebContents* feed_contents,
                                     const GURL& article_url);
  // Persists a feed subscription and files it in the current Space's "Feeds"
  // folder (or unsubscribes and unpins it from "Feeds").
  static void HandleFeedSubscribe(content::WebContents* feed_contents,
                                  const GURL& feed_url,
                                  const std::string& title,
                                  bool subscribe);
  base::WeakPtr<PeekController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }
  content::WebContents* contents() const { return contents_.get(); }
  // Where Rust routes a link from `tab`, a tab of this window, to `to`:
  // Peek, or a new tab when Settings > Links turns Peek off. Tabs without
  // a sidebar row, and tabs of other windows, stay as they are.
  foundation::PeekRoute Route(content::WebContents& tab, const GURL& to) const;
  // Opens `url` in the card, replacing an open Peek.
  void Open(const GURL& url, const content::Referrer& referrer);
  // Runs what Rust says `trigger` does. Does nothing while closed.
  void Trigger(foundation::PeekTrigger trigger);
  // Cmd+W (IDC_CLOSE_TAB) closes an open Peek and Cmd+O (IDC_OPEN_FILE)
  // expands it, instead of acting on the tab beneath. With no Peek open,
  // Cmd+Shift+T (IDC_RESTORE_TAB) brings back the Peek closed last when Rust
  // says it can. True when handled.
  bool HandleCommand(int command);

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

 private:
  // Ends the Peek. `trigger` is what the user pressed, or nullopt when
  // something else ended it; only a user's close can be reopened.
  void Close(std::optional<foundation::PeekTrigger> trigger = std::nullopt);
  void Expand();
  void SplitBesideSource();
  bool Reopen();
  // Shows `contents` in a new card. `grow_from`, in window coordinates, is
  // where the open animation starts; nullopt scales the card in from its
  // centre.
  void Show(std::unique_ptr<content::WebContents> contents,
            std::optional<gfx::Point> grow_from);
  // Takes the page out of the card and the card off the window, the card
  // shrinking away first when `animate`.
  std::unique_ptr<content::WebContents> Detach(bool animate);
  void RemoveClosingView();
  // Drops the closed Peek, in Rust and its kept page.
  void ForgetClosed();
  // The tab beneath. Its delegate, the Browser, handles the keys and new
  // tabs the card's page passes on.
  content::WebContents* ActiveTab() const;
  void OnOutsideMouseDown();
  void OnWorkspaceChanged();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<views::View> host_;
  raw_ptr<views::View> area_;
  const raw_ptr<BrowserWorkspaceAdapter> adapter_;
  std::unique_ptr<content::WebContents> contents_;
  raw_ptr<PeekView> view_ = nullptr;
  // A card shrinking away after its page left; removed when it finishes.
  raw_ptr<PeekView> closing_view_ = nullptr;
  // The page of the Peek closed last, with its history but no renderer
  // running until it comes back.
  std::unique_ptr<content::WebContents> closed_;
  rust::Box<foundation::PeekReopen> reopen_ = foundation::new_peek_reopen();
  // The sidebar item the open Peek came from.
  uint64_t source_item_ = 0;
  // The Space shown when Peek opened; switching Space closes it.
  uint64_t space_ = 0;
  // Pages beneath ignore input while the card is up; a press on them is a
  // click outside.
  std::vector<content::WebContents::ScopedIgnoreInputEvents> ignored_;
  base::CallbackListSubscription workspace_subscription_;
  base::ScopedObservation<views::View, views::ViewObserver> area_observation_{
      this};
  base::WeakPtrFactory<PeekController> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_PEEK_H_
