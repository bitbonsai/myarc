#ifndef MYARC_SETTINGS_WINDOW_H_
#define MYARC_SETTINGS_WINDOW_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "myarc/bookmarks_export.h"
#include "ui/base/models/image_model.h"

class PrefService;
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
  enum class Section { kGeneral, kProfiles, kLinks, kAdvanced };

  // One Profile on the Profiles page.
  struct ProfileRow {
    std::string id;  // The Profile's directory name.
    std::u16string name;
    ui::ImageModel icon;
    // The Spaces that use it, from the Spaces index, in order.
    std::vector<std::u16string> spaces;
    // False when it has no Spaces, or only the empty Space every new
    // Profile starts with.
    bool in_use = false;
    // The Profile the window was opened from.
    bool current = false;
  };

  // The Profiles page's actions, which the browser implements with
  // Chromium's Profile manager. Each action that can fail returns or reports
  // the reason to show, or an empty string once done.
  struct Profiles {
    // Every listed Profile in Chromium's display order, without ones being
    // deleted.
    base::RepeatingCallback<std::vector<ProfileRow>()> list;
    // Creates a Profile named `name`, as the Create a Space panel's New
    // Profile… does, and reports when it exists.
    base::RepeatingCallback<void(
        const std::u16string& name,
        base::OnceCallback<void(const std::u16string&)>)>
        create;
    base::RepeatingCallback<std::u16string(const std::string& id,
                                           const std::u16string& name)>
        rename;
    // The icons to pick from, with their names; `set_icon` takes an index
    // into this list.
    base::RepeatingCallback<
        std::vector<std::pair<ui::ImageModel, std::u16string>>()>
        icons;
    base::RepeatingCallback<std::u16string(const std::string& id, size_t icon)>
        set_icon;
    // Deletes the Profile and its data. Refuses the last Profile, one that
    // Spaces still use, and one with open windows.
    base::RepeatingCallback<std::u16string(const std::string& id)> remove;
    // Arc's "Archive tabs after" for Profile `id`: `get_archive` returns
    // seconds, or 0 while its workspace isn't loaded or it keeps none
    // (Guest); `set_archive` applies one of the four choices.
    base::RepeatingCallback<uint64_t(const std::string& id)> get_archive;
    base::RepeatingCallback<void(const std::string& id, uint64_t secs)>
        set_archive;
    // Every regular Profile, for Advanced > Export Pins and Favorites….
    BookmarksProfiles bookmarks;
  };

  // `profile` is the original (never off-the-record) Profile the window edits,
  // including Advanced > Import from Arc… and Export Pins and Favorites…;
  // it and `local_state` must outlive this object. `open_more_settings` opens
  // chrome://settings and `open_site` opens myarc.dev, both in the owning
  // browser window. `replay_tour` starts the welcome tour again there, from
  // Advanced > Replay welcome tour.
  SettingsWindow(Profile* profile,
                 PrefService* local_state,
                 base::RepeatingClosure open_more_settings,
                 base::RepeatingClosure open_site,
                 base::RepeatingClosure replay_tour,
                 Profiles profiles);
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
  // Runs Replay welcome tour as if its button was pressed.
  void PressReplayForCapture();
  void Close();

 private:
  void OpenMoreSettings();
  void OpenSite();
  // `found` is Arc's sidebar file for its Import, or empty for the picker.
  void ImportFromArc(const base::FilePath& found);
  void ExportPins();
  void ReplayTour();

  const raw_ptr<Profile> profile_;
  const raw_ptr<PrefService> local_state_;
  base::RepeatingClosure open_more_settings_;
  base::RepeatingClosure open_site_;
  base::RepeatingClosure replay_tour_;
  Profiles profiles_;
  std::unique_ptr<views::WidgetDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  base::WeakPtrFactory<SettingsWindow> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_SETTINGS_WINDOW_H_
