#include "myarc/control_host.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "myarc/browser_workspace_adapter.h"
#include "myarc/workspace_service.h"

namespace myarc {
namespace {

// Epoch of the running host; 0 while stopped.
uint64_t g_epoch = 0;

// Directory of the Profile the running host is pinned to, empty until its
// first request. Kept as a path, since the Profile may go away.
base::FilePath& PinnedProfile() {
  static base::NoDestructor<base::FilePath> pinned;
  return *pinned;
}

// Runs one request on the UI thread against the workspace of the last active
// normal window of the pinned Profile. Each Profile has its own workspace and
// numbers its tabs from 1, so the host pins itself to the Profile its first
// request reached and stays there until it stops: a user switching to a
// window of another Profile mid-task can't move the agent onto that
// Profile's tabs. Private windows are skipped.
void AnswerRequest(rust::Box<foundation::ControlReply> reply) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!foundation::control_request_live(*reply)) {
    return;
  }
  base::FilePath& pinned = PinnedProfile();
  BrowserWorkspaceAdapter* target = nullptr;
  base::FilePath target_profile;
  bool other_profile = false;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        Profile* profile = browser->GetProfile();
        if (profile->IsOffTheRecord()) {
          return true;
        }
        if (!pinned.empty() && profile->GetPath() != pinned) {
          other_profile = true;
          return true;
        }
        target = BrowserWorkspaceAdapter::ForBrowser(*browser);
        if (target) {
          target_profile = profile->GetPath();
        }
        return target == nullptr;
      },
      BrowserCollection::Order::kActivation);
  if (!target) {
    if (other_profile) {
      foundation::refuse_control_request(
          *reply,
          "Browser control stays in the Profile it first used, " +
              pinned.BaseName().AsUTF8Unsafe() +
              ", which has no open window; open a window of that Profile "
              "and retry");
      return;
    }
    foundation::refuse_control_request(
        *reply, "No myArc window is open; open one and retry");
    return;
  }
  if (pinned.empty()) {
    pinned = target_profile;
  }
  target->AnswerControlRequest(std::move(reply));
}

// Background thread: binds the endpoint, then hands requests to the UI
// thread until the host stops.
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

}  // namespace

void MaybeStartControlHost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_epoch || !base::CommandLine::ForCurrentProcess()->HasSwitch(
                     kEnableControlSwitch)) {
    return;
  }
  g_epoch = foundation::enable_control_host();
  if (!g_epoch) {
    return;
  }
  PinnedProfile().clear();
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
}

}  // namespace myarc
