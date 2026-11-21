#include "myarc/startup_timing.h"

#include <cstdint>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace myarc {

namespace {

constexpr char kStartupTimingSwitch[] = "myarc-startup-timing";

bool Enabled() {
  static const bool enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kStartupTimingSwitch);
  return enabled;
}

int64_t SinceProcessStart() {
  const base::Time created = base::Process::Current().CreationTime();
  return created.is_null() ? -1
                           : (base::Time::Now() - created).InMilliseconds();
}

bool g_window_shown = false;
bool g_first_paint = false;
bool g_blocker_hold = false;

// Logs the first paint of any watched page, then stops watching.
class FirstPaintWatcher
    : public content::WebContentsObserver,
      public content::WebContentsUserData<FirstPaintWatcher> {
 public:
  ~FirstPaintWatcher() override = default;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    if (!std::exchange(g_first_paint, true)) {
      LOG(WARNING) << "myarc-startup-timing first_paint_ms="
                   << SinceProcessStart() << " scheme="
                   << web_contents()->GetLastCommittedURL().scheme();
    }
    // Deletes `this`.
    web_contents()->RemoveUserData(UserDataKey());
  }

 private:
  friend class content::WebContentsUserData<FirstPaintWatcher>;

  explicit FirstPaintWatcher(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        content::WebContentsUserData<FirstPaintWatcher>(*contents) {}

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(FirstPaintWatcher);

}  // namespace

void NoteWindowShown() {
  if (Enabled() && !std::exchange(g_window_shown, true)) {
    LOG(WARNING) << "myarc-startup-timing window_shown_ms="
                 << SinceProcessStart();
  }
}

void WatchFirstPaint(content::NavigationHandle& handle) {
  if (!Enabled() || g_first_paint || !handle.IsInPrimaryMainFrame()) {
    return;
  }
  FirstPaintWatcher::CreateForWebContents(handle.GetWebContents());
}

void NoteBlockerHold(base::TimeDelta held) {
  if (Enabled() && !std::exchange(g_blocker_hold, true)) {
    LOG(WARNING) << "myarc-startup-timing blocker_hold_ms="
                 << held.InMilliseconds();
  }
}

}  // namespace myarc
