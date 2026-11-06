#include "myarc/control_tabs.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "myarc/browser_workspace_adapter.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace myarc {
namespace {

using foundation::TabActionKind;
using foundation::TabStatus;

foundation::TabStateRecord StateOf(uint64_t item, content::WebContents& page) {
  foundation::TabStateRecord state;
  state.item = item;
  GURL url = page.GetLastCommittedURL();
  if (url.is_empty()) {
    url = page.GetVisibleURL();
  }
  state.url = url.possibly_invalid_spec();
  state.title = base::UTF16ToUTF8(page.GetTitle());
  state.loading = page.IsLoading();
  auto& controller = page.GetController();
  state.can_go_back = controller.CanGoBack();
  state.can_go_forward = controller.CanGoForward();
  return state;
}

void Answer(foundation::ControlReply& reply,
            TabStatus status,
            std::string_view reason,
            rust::Vec<foundation::TabStateRecord> tabs = {}) {
  foundation::answer_tab_request(
      reply, status, rust::Str(reason.data(), reason.size()), std::move(tabs));
}

void AnswerWith(foundation::ControlReply& reply,
                uint64_t item,
                content::WebContents& page) {
  rust::Vec<foundation::TabStateRecord> tabs;
  tabs.push_back(StateOf(item, page));
  Answer(reply, TabStatus::Done, {}, std::move(tabs));
}

// Why the browser won't load `url`, or empty when it may. The core checked
// the text; this checks what the browser parsed from it.
std::string Refusal(const GURL& url) {
  if (!url.is_valid()) {
    return "The URL is not valid";
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return "Only http and https URLs are allowed";
  }
  if (url.has_username() || url.has_password()) {
    return "URLs with a user name or password are not allowed";
  }
  return {};
}

// Answers a navigating command once the tab's primary main frame commits a
// navigation, or with not-committed when that navigation ends without
// committing or the wait runs out. Owns itself and deletes itself after
// answering, which happens in a task of its own, never inside an observer
// call.
class CommitWait final : public content::WebContentsObserver {
 public:
  // With `any`, the first primary main frame navigation to finish counts,
  // for a page whose navigation started before the wait began. Otherwise
  // the first one to start from now on does.
  CommitWait(content::WebContents& page,
             uint64_t item,
             rust::Box<foundation::ControlReply> reply,
             bool any)
      : content::WebContentsObserver(&page),
        item_(item),
        any_(any),
        reply_(std::move(reply)) {}

  // Call once the navigation was started. `id` names it when known.
  void Arm(int64_t id) {
    if (id) {
      id_ = id;
    }
    if (done_) {
      return;
    }
    timer_.Start(FROM_HERE,
                 base::Milliseconds(foundation::control_navigation_wait_ms()),
                 base::BindOnce(&CommitWait::Finish, base::Unretained(this),
                                TabStatus::NotCommitted,
                                "The navigation did not commit in time; check "
                                "the tab state before retrying"));
  }

  void Finish(TabStatus status, std::string reason) {
    if (done_) {
      return;
    }
    done_ = true;
    timer_.Stop();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&CommitWait::AnswerAndDelete, weak_factory_.GetWeakPtr(),
                       status, std::move(reason)));
  }

 private:
  void DidStartNavigation(content::NavigationHandle* handle) override {
    if (!any_ && !id_ && handle->IsInPrimaryMainFrame()) {
      id_ = handle->GetNavigationId();
    }
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (!handle->IsInPrimaryMainFrame() ||
        (id_ && handle->GetNavigationId() != id_) || (!id_ && !any_)) {
      return;
    }
    if (handle->HasCommitted()) {
      Finish(TabStatus::Done, {});
    } else {
      Finish(TabStatus::NotCommitted,
             "The navigation ended without committing, for example because "
             "it was cancelled, blocked or became a download");
    }
  }

  void WebContentsDestroyed() override {
    Finish(TabStatus::Stale, "The tab closed before its navigation committed");
  }

  void AnswerAndDelete(TabStatus status, std::string reason) {
    rust::Vec<foundation::TabStateRecord> tabs;
    if (auto* page = web_contents()) {
      tabs.push_back(StateOf(item_, *page));
    }
    Answer(*reply_, status, reason, std::move(tabs));
    delete this;
  }

  const uint64_t item_;
  const bool any_;
  rust::Box<foundation::ControlReply> reply_;
  int64_t id_ = 0;
  bool done_ = false;
  base::OneShotTimer timer_;
  base::WeakPtrFactory<CommitWait> weak_factory_{this};
};

}  // namespace

void RunTabAction(BrowserWorkspaceAdapter& target,
                  rust::Box<foundation::ControlReply> reply,
                  const foundation::ControlPlan& plan) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (plan.action == TabActionKind::List) {
    rust::Vec<foundation::TabStateRecord> tabs;
    for (const uint64_t item : plan.items) {
      BrowserWorkspaceAdapter* owner = nullptr;
      if (auto* page = target.FindPage(item, &owner)) {
        tabs.push_back(StateOf(item, *page));
      }
    }
    Answer(*reply, TabStatus::Done, {}, std::move(tabs));
    return;
  }
  if (plan.action == TabActionKind::Open) {
    const GURL url{std::string(plan.url)};
    if (const std::string refusal = Refusal(url); !refusal.empty()) {
      Answer(*reply, TabStatus::Denied, refusal);
      return;
    }
    const auto result = target.OpenWithoutSwitching(url, plan.space);
    BrowserWorkspaceAdapter* owner = nullptr;
    auto* page = result.ok ? target.FindPage(result.id, &owner) : nullptr;
    if (!page) {
      Answer(*reply, TabStatus::Denied,
             result.ok ? std::string("The tab closed as it opened")
                       : std::string(result.error));
      return;
    }
    // Navigate started the page's first navigation already.
    (new CommitWait(*page, result.id, std::move(reply), /*any=*/true))->Arm(0);
    return;
  }

  BrowserWorkspaceAdapter* owner = nullptr;
  auto* page = target.FindPage(plan.item, &owner);
  if (!page) {
    Answer(*reply, TabStatus::Stale,
           "The tab has no open page; open it again with tab.open");
    return;
  }
  auto& controller = page->GetController();
  switch (plan.action) {
    case TabActionKind::Activate:
    case TabActionKind::Close: {
      if (!owner) {
        Answer(*reply, TabStatus::Invalid,
               "The tab is moving between windows; retry shortly");
        return;
      }
      if (plan.action == TabActionKind::Activate) {
        const auto result = owner->ActivateInItsSpace(plan.item);
        if (!result.ok) {
          Answer(*reply, TabStatus::Invalid, std::string(result.error));
          return;
        }
        AnswerWith(*reply, plan.item, *page);
        return;
      }
      // Report the tab as it was; closing may destroy it at once.
      rust::Vec<foundation::TabStateRecord> tabs;
      tabs.push_back(StateOf(plan.item, *page));
      const auto result = owner->CloseItem(plan.item);
      if (!result.ok) {
        Answer(*reply, TabStatus::Invalid, std::string(result.error));
        return;
      }
      Answer(*reply, TabStatus::Done, {}, std::move(tabs));
      return;
    }
    case TabActionKind::Back:
    case TabActionKind::Forward: {
      const bool back = plan.action == TabActionKind::Back;
      if (back ? !controller.CanGoBack() : !controller.CanGoForward()) {
        Answer(*reply, TabStatus::Invalid,
               back ? "The tab has no earlier page; check can_go_back"
                    : "The tab has no later page; check can_go_forward");
        return;
      }
      auto* wait = new CommitWait(*page, plan.item, std::move(reply), false);
      if (back) {
        controller.GoBack();
      } else {
        controller.GoForward();
      }
      wait->Arm(0);
      return;
    }
    case TabActionKind::Reload: {
      auto* wait = new CommitWait(*page, plan.item, std::move(reply), false);
      controller.Reload(content::ReloadType::NORMAL,
                        /*check_for_repost=*/true);
      wait->Arm(0);
      return;
    }
    case TabActionKind::Navigate: {
      const GURL url{std::string(plan.url)};
      if (const std::string refusal = Refusal(url); !refusal.empty()) {
        Answer(*reply, TabStatus::Denied, refusal);
        return;
      }
      auto* wait = new CommitWait(*page, plan.item, std::move(reply), false);
      content::NavigationController::LoadURLParams params(url);
      params.transition_type = ui::PAGE_TRANSITION_TYPED;
      const auto handle = controller.LoadURLWithParams(params);
      if (!handle) {
        wait->Finish(TabStatus::NotCommitted,
                     "The navigation did not start; check the tab state");
        return;
      }
      wait->Arm(handle->GetNavigationId());
      return;
    }
    default:
      Answer(*reply, TabStatus::Invalid, "Unknown tab action");
      return;
  }
}

}  // namespace myarc
