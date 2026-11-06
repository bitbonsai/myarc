#include "myarc/control_host.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "myarc/browser_workspace_adapter.h"
#include "myarc/control_prompt.h"
#include "myarc/test_buildflags.h"
#include "myarc/workspace_service.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

// Epoch of the running host; 0 while stopped.
uint64_t g_epoch = 0;

// Directory of the Profile the running host is pinned to, empty until the
// user approves a client or the first request runs. Kept as a path, since
// the Profile may go away.
base::FilePath& PinnedProfile() {
  static base::NoDestructor<base::FilePath> pinned;
  return *pinned;
}

// Client names the user allowed in this browser session. Names come from
// each client's own hello, so this records consent, not identity.
std::set<std::string>& Approved() {
  static base::NoDestructor<std::set<std::string>> approved;
  return *approved;
}

// The raw name of the client the footer shows as active, or empty.
std::string& Active() {
  static base::NoDestructor<std::string> active;
  return *active;
}

base::RepeatingTimer& ActivityTimer() {
  static base::NoDestructor<base::RepeatingTimer> timer;
  return *timer;
}

PrefChangeRegistrar& Registrar() {
  static base::NoDestructor<PrefChangeRegistrar> registrar;
  return *registrar;
}

// A client name waiting for the user's answer, with every hello under that
// name. The first one in the list shows its prompt.
struct Prompt {
  std::string name;
  std::u16string label;
  std::vector<rust::Box<foundation::ControlReply>> hellos;
  base::FilePath profile;
  std::unique_ptr<ControlPrompt> view;
  base::OneShotTimer timeout;
};

std::list<std::unique_ptr<Prompt>>& Prompts() {
  static base::NoDestructor<std::list<std::unique_ptr<Prompt>>> prompts;
  return *prompts;
}

std::u16string Label(std::string_view text) {
  return base::UTF8ToUTF16(
      std::string(foundation::control_client_label(std::string(text))));
}

std::u16string ClientLabel(std::string_view name) {
  std::u16string label = Label(name);
  return label.empty() ? u"An unnamed tool" : label;
}

// The most recently active normal window that isn't private, of the pinned
// Profile once there is one. Each Profile has its own workspace and numbers
// its tabs from 1, so the host stays in the Profile it first used until it
// stops: a user switching to a window of another Profile mid-task can't move
// the agent onto that Profile's tabs.
struct Target {
  raw_ptr<BrowserWindowInterface> browser = nullptr;
  raw_ptr<BrowserWorkspaceAdapter> adapter = nullptr;
  base::FilePath profile;
  bool other_profile = false;
};

Target FindTarget() {
  const base::FilePath& pinned = PinnedProfile();
  Target target;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        Profile* profile = browser->GetProfile();
        if (profile->IsOffTheRecord()) {
          return true;
        }
        if (!pinned.empty() && profile->GetPath() != pinned) {
          target.other_profile = true;
          return true;
        }
        target.adapter = BrowserWorkspaceAdapter::ForBrowser(*browser);
        if (target.adapter) {
          target.browser = browser;
          target.profile = profile->GetPath();
        }
        return target.adapter == nullptr;
      },
      BrowserCollection::Order::kActivation);
  return target;
}

std::string NoWindowReason(const Target& target) {
  if (target.other_profile) {
    return "Browser control stays in the Profile it first used, " +
           PinnedProfile().BaseName().AsUTF8Unsafe() +
           ", which has no open window; open a window of that Profile and "
           "retry";
  }
  return "No myArc window is open; open one and retry";
}

// Tells every window to redraw its footer and Space title.
void NotifyWindows() {
  GlobalBrowserCollection::GetInstance()->ForEach(
      [](BrowserWindowInterface* browser) {
        if (auto* adapter = BrowserWorkspaceAdapter::ForBrowser(*browser)) {
          adapter->NotifyControlChanged();
        }
        return true;
      });
}

void PollActivity() {
  std::string active(foundation::active_control_client());
  if (active != Active()) {
    Active() = std::move(active);
    NotifyWindows();
  }
}

void ShowNextPrompt();

// The user answered the first prompt, or it closed or ran out of time.
void FinishPrompt(ControlPrompt::Answer answer) {
  auto& prompts = Prompts();
  if (prompts.empty()) {
    return;
  }
  std::unique_ptr<Prompt> prompt = std::move(prompts.front());
  prompts.pop_front();
  if (answer == ControlPrompt::Answer::kAllow) {
    Approved().insert(prompt->name);
    if (PinnedProfile().empty()) {
      PinnedProfile() = prompt->profile;
    }
  }
  for (auto& hello : prompt->hellos) {
    switch (answer) {
      case ControlPrompt::Answer::kAllow:
        foundation::decide_control_hello(*hello, true);
        break;
      case ControlPrompt::Answer::kDecline:
        foundation::decide_control_hello(*hello, false);
        break;
      case ControlPrompt::Answer::kClosed:
        foundation::refuse_control_request(
            *hello,
            "the myArc window closed before anyone answered; try again");
        break;
    }
  }
  // The prompt's view may be running this; destroy it after it unwinds.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(prompt));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ShowNextPrompt));
}

// The client already got "denied: waiting for approval timed out".
void TimeOutPrompt() {
  auto& prompts = Prompts();
  if (prompts.empty()) {
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(prompts.front()));
  prompts.pop_front();
  ShowNextPrompt();
}

void ShowNextPrompt() {
  auto& prompts = Prompts();
  while (!prompts.empty()) {
    Prompt& prompt = *prompts.front();
    if (prompt.view) {
      return;
    }
    std::erase_if(prompt.hellos, [](const auto& hello) {
      return !foundation::control_request_live(*hello);
    });
    const Target target = FindTarget();
    views::Widget* widget =
        target.browser ? views::Widget::GetWidgetForNativeWindow(
                             target.browser->GetWindow()->GetNativeWindow())
                       : nullptr;
    if (prompt.hellos.empty() || !widget) {
      for (auto& hello : prompt.hellos) {
        foundation::refuse_control_request(*hello, NoWindowReason(target));
      }
      prompts.pop_front();
      continue;
    }
    prompt.profile = target.profile;
    prompt.view = std::make_unique<ControlPrompt>(
        *widget, prompt.label, base::BindOnce(&FinishPrompt));
    // A little after the client's own deadline, which answers it.
    prompt.timeout.Start(
        FROM_HERE,
        base::Milliseconds(foundation::control_approval_timeout_ms()) +
            base::Seconds(1),
        base::BindOnce(&TimeOutPrompt));
    return;
  }
}

// A client finished its hello. A name the user allowed in this session
// passes at once; any other waits for the prompt.
void AnswerHello(rust::Box<foundation::ControlReply> reply) {
  std::string name(foundation::control_hello_name(*reply));
  if (Approved().contains(name)) {
    foundation::decide_control_hello(*reply, true);
    return;
  }
  auto& prompts = Prompts();
  for (auto& prompt : prompts) {
    if (prompt->name == name) {
      prompt->hellos.push_back(std::move(reply));
      return;
    }
  }
  auto prompt = std::make_unique<Prompt>();
  prompt->label = ClientLabel(name);
  const std::u16string version =
      Label(std::string(foundation::control_hello_version(*reply)));
  if (!version.empty()) {
    prompt->label += u" " + version;
  }
  prompt->name = std::move(name);
  prompt->hellos.push_back(std::move(reply));
  prompts.push_back(std::move(prompt));
  ShowNextPrompt();
}

// Runs one request on the UI thread against the workspace of the target
// window, or hands a hello to the approval prompt.
void AnswerRequest(rust::Box<foundation::ControlReply> reply) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!foundation::control_request_live(*reply)) {
    return;
  }
  if (foundation::is_control_hello(*reply)) {
    AnswerHello(std::move(reply));
    return;
  }
  const Target target = FindTarget();
  if (!target.adapter) {
    foundation::refuse_control_request(*reply, NoWindowReason(target));
    return;
  }
  if (PinnedProfile().empty()) {
    PinnedProfile() = target.profile;
  }
  target.adapter->AnswerControlRequest(std::move(reply));
}

// Background thread: binds the endpoint, then hands requests and hellos to
// the UI thread until the host stops.
void ServeControl(uint64_t epoch,
                  scoped_refptr<base::SingleThreadTaskRunner> ui) {
  base::ScopedBlockingCall blocking(FROM_HERE, base::BlockingType::WILL_BLOCK);
  const rust::String error = foundation::start_control_host(epoch);
  if (!error.empty()) {
    LOG(ERROR) << "Browser control is off: " << std::string(error);
    return;
  }
  while (true) {
    auto reply = foundation::next_control_request(epoch);
    if (!foundation::has_control_request(*reply)) {
      return;
    }
    ui->PostTask(FROM_HERE, base::BindOnce(&AnswerRequest, std::move(reply)));
  }
}

void OnControlPrefChanged() {
  if (IsControlEnabled(g_browser_process->local_state())) {
    if (!browser_shutdown::IsTryingToQuit()) {
      MaybeStartControlHost();
    }
    return;
  }
  StopControlHost();
  // Turning control off forgets this session's approvals.
  Approved().clear();
  PinnedProfile().clear();
}

#if BUILDFLAG(MYARC_TEST_HOOKS)
// Test hooks: a test writes one word to $MYARC_CONTROL_DIR/test-hook and the
// browser runs it and removes the file. "allow" and "decline" press the
// prompt's buttons, "stop" stops the active client, and "control-on" and
// "control-off" flip the Settings switch's pref.
std::string TakeTestHook(const base::FilePath& path) {
  std::string text;
  if (!base::ReadFileToStringWithMaxSize(path, &text, 64)) {
    return {};
  }
  base::DeleteFile(path);
  return std::string(base::TrimWhitespaceASCII(text, base::TRIM_ALL));
}

void RunTestHook(const std::string& hook) {
  if (hook.empty()) {
    return;
  }
  LOG(WARNING) << "myArc control test hook: " << hook;
  if (hook == "allow" || hook == "decline") {
    auto& prompts = Prompts();
    if (!prompts.empty() && prompts.front()->view) {
      prompts.front()->view->PressForTesting(hook == "allow");
    }
  } else if (hook == "stop") {
    StopControlClient();
  } else if (hook == "control-on" || hook == "control-off") {
    g_browser_process->local_state()->SetBoolean(kControlEnabledPref,
                                                 hook == "control-on");
  }
}

void StartTestHooks() {
  const std::optional<std::string> dir =
      base::Environment::Create()->GetVar("MYARC_CONTROL_DIR");
  if (!dir || dir->empty()) {
    return;
  }
  static base::NoDestructor<base::RepeatingTimer> poll;
  const base::FilePath path = base::FilePath(*dir).Append("test-hook");
  poll->Start(FROM_HERE, base::Milliseconds(300),
              base::BindRepeating(
                  [](const base::FilePath& path) {
                    base::ThreadPool::PostTaskAndReplyWithResult(
                        FROM_HERE, {base::MayBlock()},
                        base::BindOnce(&TakeTestHook, path),
                        base::BindOnce(&RunTestHook));
                  },
                  path));
}
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)

}  // namespace

void MaybeStartControlHost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state && !Registrar().prefs()) {
    Registrar().Init(local_state);
    Registrar().Add(kControlEnabledPref,
                    base::BindRepeating(&OnControlPrefChanged));
#if BUILDFLAG(MYARC_TEST_HOOKS)
    StartTestHooks();
#endif
  }
  if (g_epoch || !IsControlEnabled(local_state)) {
    return;
  }
  g_epoch = foundation::enable_control_host();
  if (!g_epoch) {
    return;
  }
  PinnedProfile().clear();
  ActivityTimer().Start(FROM_HERE, base::Seconds(1),
                        base::BindRepeating(&PollActivity));
  base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(FROM_HERE, base::BindOnce(&ServeControl, g_epoch,
                                           content::GetUIThreadTaskRunner({})));
}

void StopControlHost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!g_epoch) {
    return;
  }
  g_epoch = 0;
  foundation::stop_control_host();
  // The stopped host answered every waiting hello already.
  Prompts().clear();
  ActivityTimer().Stop();
  if (!Active().empty()) {
    Active().clear();
    NotifyWindows();
  }
}

std::u16string ActiveControlClient(const Profile& profile) {
  if (Active().empty() || profile.IsOffTheRecord() ||
      profile.GetPath() != PinnedProfile()) {
    return {};
  }
  return ClientLabel(Active());
}

void StopControlClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const std::string name = Active();
  if (name.empty()) {
    return;
  }
  Approved().erase(name);
  foundation::revoke_control_client(name);
  Active().clear();
  NotifyWindows();
}

}  // namespace myarc
