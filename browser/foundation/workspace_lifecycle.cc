#include "myarc/workspace_lifecycle.h"

#include <vector>

#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"

namespace myarc {
namespace {

struct Registry {
  std::map<Profile*, WorkspaceLifecycle*> registrations;
  // Each startup launch and SessionRestoreImpl owns one count. Native
  // completion observers may start another restore for the same Profile.
  std::map<Profile*, size_t> pending;
  THREAD_CHECKER(thread_checker);
};

Registry& GetRegistry() {
  static base::NoDestructor<Registry> registry;
  DCHECK_CALLED_ON_VALID_THREAD(registry->thread_checker);
  return *registry;
}

std::map<Profile*, WorkspaceLifecycle*>& Registrations() {
  return GetRegistry().registrations;
}

std::map<Profile*, size_t>& PendingRestores() {
  return GetRegistry().pending;
}

}  // namespace

WorkspaceLifecycle::WorkspaceLifecycle(Profile* profile) {
  const bool inserted = Registrations().emplace(profile, this).second;
  DCHECK(inserted);
}

WorkspaceLifecycle::~WorkspaceLifecycle() {
  std::erase_if(Registrations(),
                [this](const auto& entry) { return entry.second == this; });
}

WorkspaceLifecycle* WorkspaceLifecycle::Get(Profile* profile) {
  auto it = Registrations().find(profile);
  return it == Registrations().end() ? nullptr : it->second;
}

bool WorkspaceLifecycle::HasPendingSessionRestore(Profile* profile) {
  return PendingRestores().contains(profile);
}

void WorkspaceLifecycle::Notify(Profile* profile, Event event) {
  if (event == Event::kStartupStarted ||
      event == Event::kSessionRestoreStarted) {
    ++PendingRestores()[profile];
  } else if (event == Event::kStartupFinished ||
             event == Event::kSessionRestoreFinished) {
    auto it = PendingRestores().find(profile);
    if (it != PendingRestores().end() && --it->second == 0) {
      PendingRestores().erase(it);
    }
  }
  if (auto* lifecycle = Get(profile)) {
    lifecycle->OnLifecycleEvent(event);
  }
}

void WorkspaceLifecycle::NotifyAll(Event event) {
  // A callback may tear down another Profile in a nested UI loop.
  const std::vector<std::pair<Profile*, WorkspaceLifecycle*>> registrations(
      Registrations().begin(), Registrations().end());
  for (const auto& [profile, lifecycle] : registrations) {
    if (Get(profile) == lifecycle) {
      lifecycle->OnLifecycleEvent(event);
    }
  }
}

WorkspaceLifecycle::ExtraData WorkspaceLifecycle::PreserveUnknownData(
    const ExtraData& data) {
  ExtraData unknown = data;
  // shortcut: private upstream keys require auditing on each Chromium rebase.
  // Mirrors browser_tabrestore, BrowserLiveTabContext and the adapter.
  for (const char* key :
       {"myarc.item.v1", "myarc.window.v1", "myarc.space.v1",
        "glic.instance_id", "glic.conversation_id", "glic.panel_open",
        "glic.pinned_instances", "contextual_tasks.task_id",
        "contextual_tasks.thread_url", "send_tab_to_self.entry_guid",
        "vertical_tab_strip_collapsed", "vertical_tab_strip_uncollapsed_width",
        "focused_tab_group_id"}) {
    unknown.erase(key);
  }
  return unknown;
}

}  // namespace myarc
