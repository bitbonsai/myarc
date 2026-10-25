#include <functional>
#include <iostream>
#include <memory>

#include "myarc/workspace_lifecycle.h"

namespace {

class Probe final : public myarc::WorkspaceLifecycle {
 public:
  explicit Probe(Profile* profile) : WorkspaceLifecycle(profile) {}
  int calls = 0;
  Event last = Event::kRestoreStarted;
  std::function<void()> on_event;
  void OnLifecycleEvent(Event event) override {
    ++calls;
    last = event;
    if (on_event) {
      on_event();
    }
  }
  void RestoreTab(BrowserWindowInterface&,
                  content::WebContents&,
                  const ExtraData&) override {}
  void RestoreWindow(BrowserWindowInterface&, const ExtraData&) override {}
  ExtraData TabData(BrowserWindowInterface&, content::WebContents&) override {
    return {};
  }
  ExtraData WindowData(BrowserWindowInterface&) override { return {}; }
};

bool CheckNativeBarrier() {
  using Lifecycle = myarc::WorkspaceLifecycle;
  int token;
  auto* profile = reinterpret_cast<Profile*>(&token);
  Lifecycle::Notify(profile, Lifecycle::Event::kStartupStarted);
  if (!Lifecycle::HasPendingSessionRestore(profile) ||
      Lifecycle::Get(profile)) {
    return false;
  }
  {
    // A window/delegate can arrive before the restorer is even constructed.
    // Fresh-window readiness must not release the enclosing startup barrier.
    Probe late(profile);
    Lifecycle::Notify(profile, Lifecycle::Event::kNativeReady);
    if (!Lifecycle::HasPendingSessionRestore(profile)) {
      return false;
    }
    Lifecycle::Notify(profile, Lifecycle::Event::kSessionRestoreStarted);
    Lifecycle::Notify(profile, Lifecycle::Event::kStartupFinished);
    if (!Lifecycle::HasPendingSessionRestore(profile)) {
      return false;
    }
    Lifecycle::Notify(profile, Lifecycle::Event::kSessionRestoreStarted);
    Lifecycle::Notify(profile, Lifecycle::Event::kSessionRestoreFinished);
    if (!Lifecycle::HasPendingSessionRestore(profile)) {
      return false;
    }
    Lifecycle::Notify(profile, Lifecycle::Event::kSessionRestoreFinished);
    if (Lifecycle::HasPendingSessionRestore(profile) ||
        late.last != Lifecycle::Event::kSessionRestoreFinished) {
      return false;
    }
  }
  // Fresh launch without any restorer, including nested launches.
  Lifecycle::Notify(profile, Lifecycle::Event::kStartupStarted);
  Lifecycle::Notify(profile, Lifecycle::Event::kStartupStarted);
  Lifecycle::Notify(profile, Lifecycle::Event::kStartupFinished);
  if (!Lifecycle::HasPendingSessionRestore(profile)) {
    return false;
  }
  Lifecycle::Notify(profile, Lifecycle::Event::kStartupFinished);
  return !Lifecycle::Get(profile) &&
         !Lifecycle::HasPendingSessionRestore(profile);
}

bool CheckRecoveryChoice() {
  using Lifecycle = myarc::WorkspaceLifecycle;
  using Event = Lifecycle::Event;
  int token, other_token;
  auto* profile = reinterpret_cast<Profile*>(&token);
  auto* other = reinterpret_cast<Profile*>(&other_token);
  // Consent/show can be pending before the first adapter registers.
  Lifecycle::Notify(profile, Event::kStartupStarted);
  Lifecycle::Notify(profile, Event::kRecoveryChoiceStarted);
  Lifecycle::Notify(profile, Event::kStartupFinished);
  Probe late(profile);
  Lifecycle::Notify(profile, Event::kNativeReady);
  if (!Lifecycle::HasPendingSessionRestore(profile) ||
      Lifecycle::HasPendingSessionRestore(other)) {
    return false;
  }
  // Restore closes/deletes the dialog first. Its moved choice survives until
  // RestoreSessionAfterCrash constructs the real restorer, even in UI loops.
  Lifecycle::Notify(profile, Event::kSessionRestoreStarted);
  Lifecycle::Notify(profile, Event::kNativeReady);
  Lifecycle::Notify(profile, Event::kRecoveryChoiceFinished);
  if (!Lifecycle::HasPendingSessionRestore(profile)) {
    return false;
  }
  Lifecycle::Notify(profile, Event::kSessionRestoreFinished);
  if (Lifecycle::HasPendingSessionRestore(profile)) {
    return false;
  }
  // Discard/cancel use the same finish event, scoped to the exact Profile.
  Lifecycle::Notify(profile, Event::kRecoveryChoiceStarted);
  Lifecycle::Notify(other, Event::kRecoveryChoiceStarted);
  Lifecycle::Notify(profile, Event::kRecoveryChoiceFinished);
  if (Lifecycle::HasPendingSessionRestore(profile) ||
      !Lifecycle::HasPendingSessionRestore(other)) {
    return false;
  }
  Lifecycle::Notify(other, Event::kRecoveryChoiceFinished);
  // Canceling quit preserves a still-pending choice. Multiple prompts retain
  // independent counts and no count survives the final owner's cancellation.
  Lifecycle::Notify(profile, Event::kRecoveryChoiceStarted);
  Lifecycle::Notify(profile, Event::kRecoveryChoiceStarted);
  Lifecycle::NotifyAll(Event::kShutdownStarted);
  Lifecycle::NotifyAll(Event::kShutdownCancelled);
  Lifecycle::Notify(profile, Event::kRecoveryChoiceFinished);
  if (!Lifecycle::HasPendingSessionRestore(profile)) {
    return false;
  }
  Lifecycle::Notify(profile, Event::kRecoveryChoiceFinished);
  return !Lifecycle::HasPendingSessionRestore(profile) &&
         !Lifecycle::HasPendingSessionRestore(other) && !Lifecycle::Get(other);
}

bool CheckMetadata() {
  using Lifecycle = myarc::WorkspaceLifecycle;
  const Lifecycle::ExtraData restored = {
      {"myarc.item.v1", "18446744073709551616"},
      {"glic.panel_open", "true"},
      {"focused_tab_group_id", "closed-group"},
      {"foreign.future.v2", "keep byte-for-byte"},
      {"foreign.current.v2", "historical"}};
  auto unknown = Lifecycle::PreserveUnknownData(restored);
  if (unknown.size() != 2 || restored.size() != 5 ||
      unknown.at("foreign.future.v2") != "keep byte-for-byte") {
    return false;
  }
  Lifecycle::ExtraData current = {{"foreign.current.v2", "current"},
                                  {"myarc.item.v1", "8"}};
  current.merge(unknown);
  return current.at("foreign.current.v2") == "current" &&
         current.at("myarc.item.v1") == "8" &&
         current.at("foreign.future.v2") == "keep byte-for-byte" &&
         !current.contains("glic.panel_open") &&
         !current.contains("focused_tab_group_id");
}

bool CheckRegistration() {
  using Lifecycle = myarc::WorkspaceLifecycle;
  // Only opaque identity tokens: this check never constructs native Profiles.
  int regular_token, otr_token, unconsented_token;
  auto* regular = reinterpret_cast<Profile*>(&regular_token);
  auto* otr = reinterpret_cast<Profile*>(&otr_token);
  auto* unconsented = reinterpret_cast<Profile*>(&unconsented_token);
  if (Lifecycle::Get(regular) || Lifecycle::Get(unconsented)) {
    return false;
  }
  {
    Probe a(regular);
    auto b = std::make_unique<Probe>(otr);
    if (Lifecycle::Get(regular) != &a || Lifecycle::Get(otr) != b.get()) {
      return false;
    }
    Lifecycle::Notify(regular, Lifecycle::Event::kNativeReady);
    if (a.calls != 1 || a.last != Lifecycle::Event::kNativeReady || b->calls) {
      return false;
    }
    Lifecycle::Notify(unconsented, Lifecycle::Event::kSessionReset);
    if (Lifecycle::Get(unconsented)) {
      return false;
    }
    Lifecycle::NotifyAll(Lifecycle::Event::kShutdownStarted);
    if (a.calls != 2 || b->calls != 1) {
      return false;
    }
    Lifecycle::NotifyAll(Lifecycle::Event::kShutdownCancelled);
    if (a.calls != 3 || b->calls != 2 ||
        a.last != Lifecycle::Event::kShutdownCancelled) {
      return false;
    }
    // Registry mutation during notification, in either address order.
    a.on_event = [&b] { b.reset(); };
    Lifecycle::NotifyAll(Lifecycle::Event::kShutdownStarted);
    if (b || Lifecycle::Get(otr)) {
      return false;
    }
  }
  if (Lifecycle::Get(regular)) {
    return false;
  }
  Lifecycle::NotifyAll(Lifecycle::Event::kShutdownCancelled);
  return true;
}

}  // namespace

int main() {
  if (!CheckRegistration() || !CheckMetadata() || !CheckNativeBarrier() ||
      !CheckRecoveryChoice()) {
    std::cerr << "Workspace lifecycle boundary check failed\n";
    return 1;
  }
  std::cout
      << "Workspace lifecycle registration, metadata, startup/restore and "
         "recovery-choice checks passed\n";
}
