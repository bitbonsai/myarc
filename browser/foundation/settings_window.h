#ifndef MYARC_SETTINGS_WINDOW_H_
#define MYARC_SETTINGS_WINDOW_H_

#include <cstdint>
#include <memory>

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace views {
class Widget;
class WidgetDelegate;
}  // namespace views

namespace myarc {

// Arc's native Settings window, opened by Cmd+, in myArc mode. Chromium's own
// chrome://settings stays reachable through Advanced > More Settings…. At most
// one window is open per process, bound to one original Profile; the owning
// browser window keeps this object and closes the window when it goes away.
// UI sequence only.
class SettingsWindow {
 public:
  enum class Section { kGeneral, kProfiles, kAdvanced };

  // The bound Profile's timer, Arc's "Archive tabs after". `get` returns its
  // seconds, or 0 while the workspace is not loaded or the Profile keeps no
  // workspace (Guest); `set` applies one of the four choices.
  struct ArchiveTimer {
    std::u16string profile_name;  // Shown on the Profiles page.
    base::RepeatingCallback<uint64_t()> get;
    base::RepeatingCallback<void(uint64_t)> set;
  };

  // `profile` is the original (never off-the-record) Profile the window edits;
  // it must outlive this object. `open_more_settings` opens chrome://settings
  // in the owning browser window.
  SettingsWindow(const Profile* profile,
                 base::RepeatingClosure open_more_settings,
                 ArchiveTimer archive_timer);
  SettingsWindow(const SettingsWindow&) = delete;
  SettingsWindow& operator=(const SettingsWindow&) = delete;
  ~SettingsWindow();

  // Brings back the open Settings window of any browser window of `profile`
  // and refreshes it; `inactive`, or the app being in the background, leaves
  // it where it is. An open window of another Profile closes instead. False
  // when none of `profile` is open.
  static bool ActivateOpen(const Profile* profile, bool inactive);

  // Opens this window on `section`. `inactive` keeps it from taking focus, for
  // background captures; it never takes focus while the app is in the
  // background.
  void Show(Section section, bool inactive);
  bool IsShowing() const;
  // Runs the More Settings… action as if its button was pressed.
  void PressMoreSettingsForCapture();
  void Close();

 private:
  void OpenMoreSettings();

  const raw_ptr<const Profile> profile_;
  base::RepeatingClosure open_more_settings_;
  ArchiveTimer archive_timer_;
  std::unique_ptr<views::WidgetDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  base::WeakPtrFactory<SettingsWindow> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_SETTINGS_WINDOW_H_
