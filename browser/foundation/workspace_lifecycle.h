#ifndef MYARC_WORKSPACE_LIFECYCLE_H_
#define MYARC_WORKSPACE_LIFECYCLE_H_

#include <map>
#include <string>

class BrowserWindowInterface;
class Profile;
namespace content {
class WebContents;
}

namespace myarc {

// UI-sequence, exact-Profile registration owned by that Profile's runtime.
// Chrome sessions/lifetime call this boundary without depending on the adapter.
// Lookup never constructs a service or opts a Profile/window into myarc.
class WorkspaceLifecycle {
 public:
  using ExtraData = std::map<std::string, std::string>;
  enum class Event {
    kStartupStarted,
    kStartupFinished,
    kRecoveryChoiceStarted,
    kRecoveryChoiceFinished,
    kRestoreStarted,
    kSessionRestoreStarted,
    kSessionRestoreFinished,
    kNativeReady,
    kSessionReset,
    kShutdownStarted,
    kShutdownCancelled,
  };

  explicit WorkspaceLifecycle(Profile* profile);
  virtual ~WorkspaceLifecycle();
  WorkspaceLifecycle(const WorkspaceLifecycle&) = delete;
  WorkspaceLifecycle& operator=(const WorkspaceLifecycle&) = delete;

  static WorkspaceLifecycle* Get(Profile* profile);
  static void Notify(Profile* profile, Event event);
  static void NotifyAll(Event event);
  // Covers startup, pending crash recovery choice and native completion
  // observers, even after upstream removes its restorer from IsRestoring().
  // No delegate/service is created.
  static bool HasPendingSessionRestore(Profile* profile);
  // Preserve opaque keys, while current native producers and bounded adapter
  // helpers regenerate the keys they own (including deliberately absent ones).
  static ExtraData PreserveUnknownData(const ExtraData& data);

  virtual void OnLifecycleEvent(Event event) = 0;
  virtual void RestoreTab(BrowserWindowInterface& browser,
                          content::WebContents& contents,
                          const ExtraData& data) = 0;
  virtual void RestoreWindow(BrowserWindowInterface& browser,
                             const ExtraData& data) = 0;
  virtual ExtraData TabData(BrowserWindowInterface& browser,
                            content::WebContents& contents) = 0;
  virtual ExtraData WindowData(BrowserWindowInterface& browser) = 0;
};

}  // namespace myarc

#endif  // MYARC_WORKSPACE_LIFECYCLE_H_
