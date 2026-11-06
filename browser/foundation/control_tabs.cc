#include "myarc/control_tabs.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "myarc/browser_workspace_adapter.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/size.h"
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
  record.editable = node.HasState(State::kEditable);
  record.read_only =
      node.GetRestriction() == ax::mojom::Restriction::kReadOnly;
  return record;
}

// What one accessibility snapshot of a tab's page collected.
struct Collected {
  TabStatus status = TabStatus::Done;
  std::string reason;
  // The main document the snapshot is of.
  uint64_t document = 0;
  int32_t root = 0;
  int32_t focus = 0;
  bool source_truncated = false;
  rust::Vec<foundation::AxNodeRecord> nodes;
};

using CollectedCallback = base::OnceCallback<void(Collected)>;

// Takes one accessibility snapshot of a tab's page through
// WebContents::RequestAXTreeSnapshot and runs `done` with it, or with
// timeout or stale. The snapshot runs in the renderer without turning on
// accessibility for the page, opening a debugging port or running page
// script. Owns itself and deletes itself after `done` ran, in a task of its
// own.
//
// Same-origin frames only: a cross-origin frame's content is another site's,
// and the agent's scope is this tab's page. Those frames share the page's
// renderer process, so the combined tree keeps their node IDs, which are
// DOMNodeIds the interaction commands can act on.
class SnapshotTaker final : public content::WebContentsObserver {
 public:
  SnapshotTaker(content::WebContents& page, CollectedCallback done)
      : content::WebContentsObserver(&page), done_callback_(std::move(done)) {
    collected_.document = DocumentOf(page);
  }

  void Start(base::TimeDelta timeout) {
    timer_.Start(FROM_HERE, timeout,
                 base::BindOnce(&SnapshotTaker::Finish, base::Unretained(this),
                                TabStatus::TimedOut,
                                "The page did not answer within the snapshot "
                                "time limit"));
    // The renderer gets most of the time limit, so a slow page still answers
    // with what it had.
    web_contents()->RequestAXTreeSnapshot(
        base::BindOnce(&SnapshotTaker::Received, weak_factory_.GetWeakPtr()),
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
    if (DocumentOf(*web_contents()) != collected_.document) {
      Finish(TabStatus::Stale,
             "The page navigated during the snapshot; run page.snapshot "
             "again");
      return;
    }
    collected_.root = update.root_id;
    collected_.focus = update.has_tree_data ? update.tree_data.focus_id : 0;
    collected_.source_truncated =
        update.nodes.size() >= foundation::control_snapshot_max_nodes();
    for (const ui::AXNodeData& node : update.nodes) {
      collected_.nodes.push_back(RecordOf(node));
    }
    Finish(TabStatus::Done, {});
  }

  void Finish(TabStatus status, std::string reason) {
    if (done_) {
      return;
    }
    done_ = true;
    timer_.Stop();
    collected_.status = status;
    collected_.reason = std::move(reason);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SnapshotTaker::RunAndDelete,
                                  weak_factory_.GetWeakPtr()));
  }

  void WebContentsDestroyed() override {
    Finish(TabStatus::Stale, "The tab closed during the snapshot");
  }

  void RunAndDelete() {
    CollectedCallback done = std::move(done_callback_);
    Collected collected = std::move(collected_);
    delete this;
    std::move(done).Run(std::move(collected));
  }

  CollectedCallback done_callback_;
  Collected collected_;
  bool done_ = false;
  base::OneShotTimer timer_;
  base::WeakPtrFactory<SnapshotTaker> weak_factory_{this};
};

void TakeSnapshot(content::WebContents& page,
                  base::TimeDelta timeout,
                  CollectedCallback done) {
  (new SnapshotTaker(page, std::move(done)))->Start(timeout);
}

// Compares two nodes at the same position of two snapshots. Leaves out
// node IDs, since nodes without a DOM node, such as list markers, get a new
// one in every snapshot; the child counts keep the tree's shape.
bool SameNode(const foundation::AxNodeRecord& a,
              const foundation::AxNodeRecord& b) {
  return a.role == b.role && a.name == b.name && a.value == b.value &&
         a.url == b.url && a.children.size() == b.children.size() &&
         a.level == b.level && a.focusable == b.focusable &&
         a.clickable == b.clickable && a.invisible == b.invisible &&
         a.ignored == b.ignored && a.checked == b.checked &&
         a.expanded == b.expanded && a.disabled == b.disabled &&
         a.selected == b.selected && a.required == b.required &&
         a.is_protected == b.is_protected && a.editable == b.editable &&
         a.read_only == b.read_only;
}

// Whether two snapshots of one document hold the same outline. Layout isn't
// recorded, so an animation alone doesn't count as a change.
bool SameContent(const Collected& a, const Collected& b) {
  return a.document == b.document && a.root == b.root && a.focus == b.focus &&
         a.source_truncated == b.source_truncated &&
         std::ranges::equal(a.nodes, b.nodes, SameNode);
}

// Answers page.snapshot with what `TakeSnapshot` collected for `page`.
void AnswerSnapshot(base::WeakPtr<content::WebContents> page,
                    uint64_t item,
                    rust::Box<foundation::ControlReply> reply,
                    Collected collected) {
  foundation::TabStateRecord tab{};
  tab.item = item;
  if (page) {
    tab = StateOf(item, *page);
  }
  foundation::answer_snapshot_request(
      *reply, collected.status,
      rust::Str(collected.reason.data(), collected.reason.size()),
      std::move(tab), collected.document, collected.root, collected.focus,
      collected.source_truncated, std::move(collected.nodes));
}

// One key press on the page's focused element, as raw key down, an
// optional character and key up, the way a keyboard delivers it.
void PressKey(content::RenderWidgetHost& widget,
              const foundation::KeyRecord& key) {
  const ui::DomCode code =
      ui::KeycodeConverter::CodeStringToDomCode(std::string(key.code));
  const bool named = key.text == 0 || std::string(key.key) == "Enter";
  const ui::DomKey dom_key =
      named ? ui::KeycodeConverter::KeyStringToDomKey(std::string(key.key))
            : ui::DomKey::FromCharacter(key.text);
  const int modifiers = key.shift ? blink::WebInputEvent::kShiftKey : 0;
  auto event_of = [&](blink::WebInputEvent::Type type) {
    input::NativeWebKeyboardEvent event(type, modifiers,
                                        ui::EventTimeForNow());
    event.windows_key_code = key.key_code;
    event.native_key_code = ui::KeycodeConverter::DomCodeToNativeKeycode(code);
    event.dom_code = static_cast<int>(code);
    event.dom_key = static_cast<uint32_t>(dom_key);
    if (key.text) {
      event.text[0] = key.text;
      event.unmodified_text[0] = key.text;
    }
    return event;
  };
  widget.ForwardKeyboardEvent(
      event_of(blink::WebInputEvent::Type::kRawKeyDown));
  if (key.text) {
    widget.ForwardKeyboardEvent(event_of(blink::WebInputEvent::Type::kChar));
  }
  widget.ForwardKeyboardEvent(event_of(blink::WebInputEvent::Type::kKeyUp));
}

// Whether page.snapshot shows `frame`: the main frame, or a frame whose
// chain up to it stays in the main frame's site instance and origin outside
// fenced frames, the frames kSameOriginDirectDescendants collects.
bool SnapshotShows(content::RenderFrameHost& frame,
                   content::RenderFrameHost& main) {
  for (content::RenderFrameHost* current = &frame; current;
       current = current->GetParent()) {
    if (current == &main) {
      return true;
    }
    if (current->GetSiteInstance() != main.GetSiteInstance() ||
        current->IsFencedFrameRoot() ||
        !main.GetLastCommittedOrigin().IsSameOriginWith(
            current->GetLastCommittedOrigin())) {
      return false;
    }
  }
  return false;
}

// How the page's renderer ended, such as "status=killed, exit=9", for
// answers about a crashed page.
std::string CrashDetail(content::WebContents& page) {
  const char* status = "other";
  switch (page.GetCrashedStatus()) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      status = "normal";
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      status = "abnormal";
      break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      status = "killed";
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      status = "crashed";
      break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      status = "launch_failed";
      break;
    case base::TERMINATION_STATUS_OOM:
      status = "oom";
      break;
    default:
      break;
  }
  return base::StrCat(
      {"status=", status, ", code=",
       base::NumberToString(static_cast<int>(page.GetCrashedStatus())),
       ", exit=", base::NumberToString(page.GetCrashedErrorCode())});
}

std::string Describe(const actor::mojom::ActionResult& result) {
  std::string text = base::ToString(result.code);
  if (!result.message.empty()) {
    base::StrAppend(&text, {": ", result.message});
  }
  // Page-influenced; keep it short.
  if (text.size() > 300) {
    text.resize(300);
  }
  return text;
}

// The options of the select whose DOMNodeId is `node` in `content`, or
// false when it isn't there.
bool SelectOptionsOf(const blink::mojom::AIPageContent& content,
                     int32_t node,
                     size_t max_options,
                     rust::Vec<foundation::SelectOptionRecord>& out) {
  std::vector<const blink::mojom::AIPageContentNode*> stack = {
      content.root_node.get()};
  size_t visited = 0;
  while (!stack.empty() && ++visited < 1'000'000) {
    const auto* current = stack.back();
    stack.pop_back();
    if (!current) {
      continue;
    }
    const auto& attributes = current->content_attributes;
    if (attributes && attributes->dom_node_id == node) {
      if (!attributes->form_control_data) {
        return false;
      }
      for (const auto& option :
           attributes->form_control_data->select_options) {
        if (out.size() >= max_options) {
          break;
        }
        foundation::SelectOptionRecord record;
        record.value = rust::String::lossy(option->value.value_or(""));
        record.text = rust::String::lossy(option->text.value_or(""));
        record.disabled = option->disabled;
        out.push_back(std::move(record));
      }
      return true;
    }
    for (const auto& child : current->children_nodes) {
      stack.push_back(child.get());
    }
  }
  return false;
}

// Runs page.click, page.fill, page.select or page.press on an element of the
// latest snapshot, then waits for the page to settle and answers with a new
// snapshot. The element is the DOM node the snapshot showed, acted on
// through the renderer's actor tools (chrome/renderer/actor) on the page's
// main frame: they scroll the element into view, check it is still there,
// enabled and not covered, and send real input events. Owns itself and
// deletes itself after answering.
//
// The agent's tab is usually in the background. While the action runs it
// counts as captured, as Chrome's own actor does, so the renderer treats the
// page as visible and runs its timers on time instead of batching them.
class ActionRun final : public content::WebContentsObserver {
 public:
  ActionRun(content::WebContents& page,
            uint64_t item,
            rust::Box<foundation::ControlReply> reply,
            foundation::ActionStart start)
      : content::WebContentsObserver(&page),
        item_(item),
        reply_(std::move(reply)),
        start_(std::move(start)),
        limits_(foundation::control_action_limits()),
        task_id_(actor::TaskId(++last_task_id_)) {}

  void Start() {
    content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
    if (!frame->IsRenderFrameLive()) {
      Finish(foundation::ActionStatus::Refused,
             "The page is not running; reload it with tab.reload first");
      return;
    }
    frame->GetRemoteAssociatedInterfaces()->GetInterface(&frame_);
    frame_.set_disconnect_handler(base::BindOnce(&ActionRun::OnFrameGone,
                                                 base::Unretained(this)));
    awake_ = web_contents()->IncrementCapturerCount(
        gfx::Size(), /*stay_hidden=*/false, /*stay_awake=*/true,
        /*is_activity=*/true);
    timer_.Start(FROM_HERE, base::Milliseconds(limits_.action_wait_ms),
                 base::BindOnce(&ActionRun::OnActionTimeout,
                                base::Unretained(this)));
    using Kind = foundation::PageActKind;
    switch (start_.kind) {
      case Kind::Click:
        Initialize(Click());
        return;
      case Kind::Fill: {
        auto type = actor::mojom::TypeAction::New();
        type->mode = actor::mojom::TypeAction::Mode::kDeleteExisting;
        type->text = std::string(start_.text);
        type->follow_by_enter = false;
        Initialize(actor::mojom::ToolAction::NewType(std::move(type)));
        return;
      }
      case Kind::Select:
        ReadSelectOptions(*frame);
        return;
      case Kind::Press:
        if (start_.focus_first) {
          Initialize(Click());
        } else {
          Acted();
        }
        return;
      default:
        Finish(foundation::ActionStatus::Refused, "Unknown page action");
        return;
    }
  }

 private:
  enum class Phase { kActing, kSettling, kSnapshot };

  static actor::mojom::ToolActionPtr Click() {
    auto click = actor::mojom::ClickAction::New();
    click->type = actor::mojom::ClickType::kLeft;
    click->count = actor::mojom::ClickCount::kSingle;
    return actor::mojom::ToolAction::NewClick(std::move(click));
  }

  // Checks the target and scrolls it into view, then runs the action.
  void Initialize(actor::mojom::ToolActionPtr action) {
    auto invocation = actor::mojom::ToolInvocation::New();
    invocation->task_id = task_id_;
    invocation->action = std::move(action);
    invocation->target =
        actor::mojom::ToolTarget::NewDomNodeId(start_.node);
    frame_->InitializeTool(std::move(invocation),
                           base::BindOnce(&ActionRun::OnInitialized,
                                          weak_factory_.GetWeakPtr()));
  }

  void OnInitialized(actor::mojom::InitializeToolResultPtr result) {
    if (phase_ != Phase::kActing || done_) {
      return;
    }
    if (result->is_error_result()) {
      Finish(foundation::ActionStatus::Refused,
             base::StrCat({"The page refused the action (",
                           Describe(*result->get_error_result()),
                           "); take a new page.snapshot"}));
      return;
    }
    frame_->ExecuteTool(task_id_, base::BindOnce(&ActionRun::OnExecuted,
                                                 weak_factory_.GetWeakPtr()));
  }

  void OnExecuted(actor::mojom::ActionResultPtr result) {
    if (phase_ != Phase::kActing || done_) {
      return;
    }
    if (result->code != actor::mojom::ActionResultCode::kOk) {
      Finish(foundation::ActionStatus::Unknown,
             base::StrCat({"The action stopped partway (", Describe(*result),
                           "); take page.snapshot before retrying"}));
      return;
    }
    Acted();
  }

  // The element action is done; press the key, if any, and settle. The key
  // goes to the focused frame, so it must be one the snapshot shows; a
  // cross-origin frame gets no keys from the agent.
  void Acted() {
    if (start_.kind == foundation::PageActKind::Press) {
      content::RenderFrameHost* main = web_contents()->GetPrimaryMainFrame();
      content::RenderFrameHost* focused = web_contents()->GetFocusedFrame();
      if (focused && !SnapshotShows(*focused, *main)) {
        Finish(start_.focus_first ? foundation::ActionStatus::Unknown
                                  : foundation::ActionStatus::Refused,
               "Focus is in a frame the agent can't see; click a ref first");
        return;
      }
      content::RenderWidgetHost* widget = main->GetRenderWidgetHost();
      if (!widget) {
        Finish(foundation::ActionStatus::Unknown,
               "The page went away before the key press");
        return;
      }
      PressKey(*widget, start_.key);
    }
    Settle();
  }

  void ReadSelectOptions(content::RenderFrameHost& frame) {
    reading_options_ = true;
    frame.GetRemoteInterfaces()->GetInterface(
        agent_.BindNewPipeAndPassReceiver());
    agent_.set_disconnect_handler(
        base::BindOnce(&ActionRun::Finish, base::Unretained(this),
                       foundation::ActionStatus::Refused,
                       "The page did not report the select's options; take "
                       "a new page.snapshot"));
    auto options = blink::mojom::AIPageContentOptions::New();
    options->mode = blink::mojom::AIPageContentMode::kDefault;
    options->on_critical_path = true;
    agent_->GetAIPageContent(std::move(options),
                             base::BindOnce(&ActionRun::OnPageContent,
                                            weak_factory_.GetWeakPtr()));
  }

  void OnPageContent(blink::mojom::AIPageContentPtr content) {
    agent_.reset();
    reading_options_ = false;
    if (done_) {
      return;
    }
    rust::Vec<foundation::SelectOptionRecord> options;
    if (!content || !SelectOptionsOf(*content, start_.node,
                                     limits_.max_select_options, options)) {
      Finish(foundation::ActionStatus::Refused,
             "The select is no longer on the page; take a new page.snapshot");
      return;
    }
    const foundation::ChoiceResult choice =
        foundation::choose_select_option(*reply_, std::move(options));
    if (!choice.ok) {
      // Answered already.
      done_ = true;
      base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                    this);
      return;
    }
    auto select = actor::mojom::SelectAction::New();
    select->value = std::string(choice.value);
    Initialize(actor::mojom::ToolAction::NewSelect(std::move(select)));
  }

  void OnActionTimeout() {
    if (reading_options_) {
      // Nothing ran yet, so the snapshot's references stay usable.
      Finish(foundation::ActionStatus::TimedOut, {});
      return;
    }
    if (frame_.is_bound()) {
      frame_->CancelTool(task_id_);
    }
    Finish(foundation::ActionStatus::Unknown,
           "The page did not finish the action in time; take page.snapshot "
           "before retrying");
  }

  void OnFrameGone() {
    frame_.reset();
    if (phase_ != Phase::kActing) {
      return;
    }
    // A link the click followed replaces the frame.
    if (navigating_ || navigated_) {
      Settle();
      return;
    }
    Finish(foundation::ActionStatus::Unknown,
           "The page went away during the action; take page.snapshot");
  }

  // Waits for the page to settle: for a started navigation to finish and
  // the new page to load, or else for the page to go quiet. Quiet means two
  // snapshots `kQuietWindow` apart hold the same outline while the tab
  // isn't navigating or loading; the first is taken right after the action.
  // The window is longer than the short timers pages use to show a result.
  // Gives up after the cap and snapshots what is there.
  void Settle() {
    if (phase_ != Phase::kActing || done_) {
      return;
    }
    phase_ = Phase::kSettling;
    timer_.Start(FROM_HERE, base::Milliseconds(limits_.settle_cap_ms),
                 base::BindOnce(&ActionRun::OnSettleTimeout,
                                base::Unretained(this)));
    MaybeSnapshot();
  }

  static constexpr base::TimeDelta kQuietWindow = base::Milliseconds(250);

  void Probe() {
    if (phase_ != Phase::kSettling || done_ || navigating_ || new_document_ ||
        probing_) {
      return;
    }
    probing_ = true;
    TakeSnapshot(*web_contents(),
                 base::Milliseconds(limits_.snapshot_timeout_ms),
                 base::BindOnce(&ActionRun::OnProbe,
                                weak_factory_.GetWeakPtr()));
  }

  void OnProbe(Collected collected) {
    probing_ = false;
    if (phase_ != Phase::kSettling || done_ || navigating_ || new_document_) {
      // A navigation took over; it settles through MaybeSnapshot.
      MaybeSnapshot();
      return;
    }
    if (collected.status == TabStatus::Done && probe_ &&
        SameContent(*probe_, collected) && !web_contents()->IsLoading()) {
      phase_ = Phase::kSnapshot;
      timer_.Stop();
      OnSnapshot(std::move(collected));
      return;
    }
    if (collected.status == TabStatus::Done) {
      probe_ = std::move(collected);
    }
    quiet_timer_.Start(FROM_HERE, kQuietWindow,
                       base::BindOnce(&ActionRun::Probe,
                                      base::Unretained(this)));
  }

  void OnSettleTimeout() {
    settled_ = false;
    Snapshot();
  }

  void MaybeSnapshot() {
    if (phase_ != Phase::kSettling || done_ || navigating_) {
      return;
    }
    if (new_document_) {
      if (loaded_) {
        Snapshot();
      }
    } else if (!quiet_timer_.IsRunning()) {
      Probe();
    }
  }

  void Snapshot() {
    if (phase_ != Phase::kSettling || done_) {
      return;
    }
    phase_ = Phase::kSnapshot;
    timer_.Stop();
    quiet_timer_.Stop();
    if (web_contents()->IsCrashed()) {
      Finish(foundation::ActionStatus::Unknown,
             base::StrCat({"The page crashed after the action (",
                           CrashDetail(*web_contents()),
                           "); reload it and take page.snapshot"}));
      return;
    }
    TakeSnapshot(*web_contents(),
                 base::Milliseconds(limits_.snapshot_timeout_ms),
                 base::BindOnce(&ActionRun::OnSnapshot,
                                weak_factory_.GetWeakPtr()));
  }

  void OnSnapshot(Collected collected) {
    if (done_) {
      return;
    }
    if (collected.status != TabStatus::Done || !web_contents()) {
      Finish(foundation::ActionStatus::Unknown,
             base::StrCat({"The action ran, but the snapshot after it failed (",
                           collected.reason, "); take page.snapshot"}));
      return;
    }
    done_ = true;
    foundation::answer_action_request(
        *reply_, foundation::ActionStatus::Done, {},
        StateOf(item_, *web_contents()), navigated_, settled_,
        collected.document, collected.root, collected.focus,
        collected.source_truncated, std::move(collected.nodes));
    delete this;
  }

  void DidStartNavigation(content::NavigationHandle* handle) override {
    if (handle->IsInPrimaryMainFrame() && !handle->IsSameDocument()) {
      navigating_ = true;
    }
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (!handle->IsInPrimaryMainFrame()) {
      return;
    }
    if (handle->HasCommitted()) {
      navigated_ = true;
      if (!handle->IsSameDocument()) {
        new_document_ = true;
        loaded_ = !web_contents()->IsLoading();
      }
    }
    if (!handle->IsSameDocument()) {
      navigating_ = false;
    }
    MaybeSnapshot();
  }

  void DidStopLoading() override {
    loaded_ = true;
    MaybeSnapshot();
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    const std::string detail = CrashDetail(*web_contents());
    LOG(WARNING) << "Control action page crashed: " << detail;
    Finish(foundation::ActionStatus::Unknown,
           base::StrCat({"The page crashed during the action (", detail,
                         "); reload it and take page.snapshot"}));
  }

  void WebContentsDestroyed() override {
    Finish(foundation::ActionStatus::Unknown,
           "The tab closed during the action");
  }

  void Finish(foundation::ActionStatus status, std::string reason) {
    if (done_) {
      return;
    }
    done_ = true;
    timer_.Stop();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ActionRun::AnswerAndDelete, weak_factory_.GetWeakPtr(),
                       status, std::move(reason)));
  }

  void AnswerAndDelete(foundation::ActionStatus status, std::string reason) {
    foundation::TabStateRecord tab{};
    tab.item = item_;
    if (auto* page = web_contents()) {
      tab = StateOf(item_, *page);
    }
    foundation::answer_action_request(
        *reply_, status, rust::Str(reason.data(), reason.size()),
        std::move(tab), navigated_, settled_, 0, 0, 0, false, {});
    delete this;
  }

  static inline int32_t last_task_id_ = 0;

  const uint64_t item_;
  rust::Box<foundation::ControlReply> reply_;
  const foundation::ActionStart start_;
  const foundation::ActionLimits limits_;
  const actor::TaskId task_id_;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> frame_;
  // Keeps the page treated as visible until the answer.
  base::ScopedClosureRunner awake_;
  mojo::Remote<blink::mojom::AIPageContentAgent> agent_;
  Phase phase_ = Phase::kActing;
  bool navigating_ = false;
  bool navigated_ = false;
  bool new_document_ = false;
  bool loaded_ = false;
  bool settled_ = true;
  bool done_ = false;
  bool probing_ = false;
  // page.select is reading the select's options; nothing ran yet.
  bool reading_options_ = false;
  // The latest quiet-window snapshot.
  std::optional<Collected> probe_;
  base::OneShotTimer timer_;
  base::OneShotTimer quiet_timer_;
  base::WeakPtrFactory<ActionRun> weak_factory_{this};
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
               base::StrCat({"The page crashed (", CrashDetail(*page),
                             "); reload it with tab.reload first"}));
        return;
      }
      TakeSnapshot(
          *page,
          base::Milliseconds(foundation::control_snapshot_timeout_ms()),
          base::BindOnce(&AnswerSnapshot, page->GetWeakPtr(), plan.item,
                         std::move(reply)));
      return;
    case TabActionKind::Act: {
      foundation::ActionStart start =
          foundation::begin_page_action(*reply, DocumentOf(*page));
      if (!start.ok) {
        return;
      }
      (new ActionRun(*page, plan.item, std::move(reply), std::move(start)))
          ->Start();
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
