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
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "myarc/browser_workspace_adapter.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
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

// Numbers each main document page.snapshot looks at. A cross-document
// navigation gives the tab a new document and so a new number, which makes
// the next snapshot's revision differ and the old references unusable.
class SnapshotDocument final
    : public content::DocumentUserData<SnapshotDocument> {
 public:
  ~SnapshotDocument() override = default;

  uint64_t id() const { return id_; }

 private:
  friend content::DocumentUserData<SnapshotDocument>;

  explicit SnapshotDocument(content::RenderFrameHost* frame)
      : DocumentUserData(frame), id_(++last_id_) {}

  static inline uint64_t last_id_ = 0;
  const uint64_t id_;

  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(SnapshotDocument);

uint64_t DocumentOf(content::WebContents& page) {
  return SnapshotDocument::GetOrCreateForCurrentDocument(
             page.GetPrimaryMainFrame())
      ->id();
}

foundation::AxNodeRecord RecordOf(const ui::AXNodeData& node) {
  using ax::mojom::BoolAttribute;
  using ax::mojom::State;
  using ax::mojom::StringAttribute;
  foundation::AxNodeRecord record;
  record.id = node.id;
  record.role = rust::String::lossy(ui::ToString(node.role));
  record.name =
      rust::String::lossy(node.GetStringAttribute(StringAttribute::kName));
  record.value =
      rust::String::lossy(node.GetStringAttribute(StringAttribute::kValue));
  record.url =
      rust::String::lossy(node.GetStringAttribute(StringAttribute::kUrl));
  for (const int32_t child : node.child_ids) {
    record.children.push_back(child);
  }
  const int level =
      node.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
  record.level = level > 0 ? static_cast<uint32_t>(level) : 0;
  record.focusable = node.HasState(State::kFocusable);
  record.clickable = node.IsClickable();
  record.invisible = node.IsInvisible();
  record.ignored = node.IsIgnored();
  switch (node.GetCheckedState()) {
    case ax::mojom::CheckedState::kTrue:
      record.checked = 1;
      break;
    case ax::mojom::CheckedState::kMixed:
      record.checked = 2;
      break;
    default:
      record.checked = 0;
  }
  record.expanded = node.HasState(State::kExpanded)    ? 1
                    : node.HasState(State::kCollapsed) ? 2
                                                       : 0;
  record.disabled =
      node.GetRestriction() == ax::mojom::Restriction::kDisabled;
  record.selected = node.GetBoolAttribute(BoolAttribute::kSelected);
  record.required = node.HasState(State::kRequired);
  record.is_protected = node.HasState(State::kProtected);
  return record;
}

// Takes one accessibility snapshot of a tab's page through
// WebContents::RequestAXTreeSnapshot and answers page.snapshot with it, or
// with timeout or stale. The snapshot runs in the renderer without turning
// on accessibility for the page, opening a debugging port or running page
// script. Owns itself and deletes itself after answering, in a task of its
// own.
class SnapshotWait final : public content::WebContentsObserver {
 public:
  SnapshotWait(content::WebContents& page,
               uint64_t item,
               rust::Box<foundation::ControlReply> reply)
      : content::WebContentsObserver(&page),
        item_(item),
        document_(DocumentOf(page)),
        reply_(std::move(reply)) {}

  void Start() {
    const auto timeout =
        base::Milliseconds(foundation::control_snapshot_timeout_ms());
    timer_.Start(FROM_HERE, timeout,
                 base::BindOnce(&SnapshotWait::Finish, base::Unretained(this),
                                TabStatus::TimedOut,
                                "The page did not answer within the snapshot "
                                "time limit"));
    // Same-origin frames only: a cross-origin frame's content is another
    // site's, and the agent's scope is this tab's page. The renderer gets
    // most of the time limit, so a slow page still answers with what it had.
    web_contents()->RequestAXTreeSnapshot(
        base::BindOnce(&SnapshotWait::Received, weak_factory_.GetWeakPtr()),
        ui::kAXModeWebContentsOnly, foundation::control_snapshot_max_nodes(),
        timeout * 3 / 4,
        content::WebContents::AXTreeSnapshotPolicy::
            kSameOriginDirectDescendants);
  }

 private:
  void Received(ui::AXTreeUpdate& update) {
    if (done_ || !web_contents()) {
      return;
    }
    if (DocumentOf(*web_contents()) != document_) {
      Finish(TabStatus::Stale,
             "The page navigated during the snapshot; run page.snapshot "
             "again");
      return;
    }
    root_ = update.root_id;
    focus_ = update.has_tree_data ? update.tree_data.focus_id : 0;
    source_truncated_ =
        update.nodes.size() >= foundation::control_snapshot_max_nodes();
    for (const ui::AXNodeData& node : update.nodes) {
      nodes_.push_back(RecordOf(node));
    }
    Finish(TabStatus::Done, {});
  }

  void Finish(TabStatus status, std::string reason) {
    if (done_) {
      return;
    }
    done_ = true;
    timer_.Stop();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SnapshotWait::AnswerAndDelete,
                       weak_factory_.GetWeakPtr(), status, std::move(reason)));
  }

  void WebContentsDestroyed() override {
    Finish(TabStatus::Stale, "The tab closed during the snapshot");
  }

  void AnswerAndDelete(TabStatus status, std::string reason) {
    foundation::TabStateRecord tab{};
    tab.item = item_;
    if (auto* page = web_contents()) {
      tab = StateOf(item_, *page);
    }
    foundation::answer_snapshot_request(
        *reply_, status, rust::Str(reason.data(), reason.size()),
        std::move(tab), document_, root_, focus_, source_truncated_,
        std::move(nodes_));
    delete this;
  }

  const uint64_t item_;
  const uint64_t document_;
  rust::Box<foundation::ControlReply> reply_;
  int32_t root_ = 0;
  int32_t focus_ = 0;
  bool source_truncated_ = false;
  rust::Vec<foundation::AxNodeRecord> nodes_;
  bool done_ = false;
  base::OneShotTimer timer_;
  base::WeakPtrFactory<SnapshotWait> weak_factory_{this};
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
    case TabActionKind::Snapshot:
      if (page->IsCrashed()) {
        Answer(*reply, TabStatus::Invalid,
               "The page crashed; reload it with tab.reload first");
        return;
      }
      (new SnapshotWait(*page, plan.item, std::move(reply)))->Start();
      return;
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
