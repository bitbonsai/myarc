#ifndef MYARC_CREATE_SPACE_PANEL_H_
#define MYARC_CREATE_SPACE_PANEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "myarc/space_icon_catalog.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class BubbleDialogDelegate;
class ImageButton;
class Button;
class ImageView;
class Label;
class LabelButton;
class MenuRunner;
class Widget;
}  // namespace views

namespace myarc {

class SpaceIconPickerView;

// A Profile the new Space can use. `id` is the caller's; the panel only hands
// it back.
struct ProfileChoice {
  std::string id;
  std::u16string name;
};

// Arc's "Create a Space" panel, which takes the sidebar's place: a name field,
// the Profile the Space will use, and Create or Cancel. Return creates and Esc
// cancels. The icon button opens the emoji and icon picker, and the Profile
// pill a menu of Profiles with New Profile…; the theme row is a placeholder
// for now.
class CreateSpacePanel final : public views::View,
                               public views::TextfieldController,
                               public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(CreateSpacePanel, views::View)

 public:
  // `create` gets the trimmed name, the picked icon (Kind::kNone for the
  // default glyph) and the selected Profile's id, and returns why it failed,
  // or nothing once the Space exists. `create` and `cancel` run after the
  // pressed control's handlers unwind and may delete the panel.
  using CreateCallback =
      base::RepeatingCallback<std::u16string(const std::u16string& name,
                                             const SpaceIconChoice& icon,
                                             const std::string& profile_id)>;
  // Reports why creating a Profile failed, or nothing once it exists.
  using ProfileDoneCallback =
      base::OnceCallback<void(const std::u16string& error)>;
  // `create_profile` gets a trimmed name of at most kMaxProfileName
  // characters. On success the caller calls SetProfiles before `done`.
  using CreateProfileCallback =
      base::RepeatingCallback<void(const std::u16string& name,
                                   ProfileDoneCallback done)>;
  static constexpr size_t kMaxProfileName = 64;

  CreateSpacePanel(std::vector<ProfileChoice> profiles,
                   std::string selected_profile_id,
                   CreateCallback create,
                   CreateProfileCallback create_profile,
                   base::RepeatingClosure cancel);
  ~CreateSpacePanel() override;

  void FocusName();
  // Opens the icon picker under the icon button on `tab`. `hold` keeps it open
  // in an inactive window, for background captures.
  void OpenIconPicker(SpaceIconChoice::Kind tab, bool hold = false);
  // Replaces the Profile menu's choices and selects `selected_id`, or the
  // first choice when it isn't listed. Closes the Profile name field.
  void SetProfiles(std::vector<ProfileChoice> profiles,
                   const std::string& selected_id);
  void OpenProfileMenu();
  // Swaps the Profile row for the Profile name field.
  void ShowNewProfile();

 private:
  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  std::u16string TrimmedName() const;
  void UpdateCreateButton();
  void Create();
  void Cancel();
  void RunCreate();
  void PickIcon(SpaceIconChoice icon);
  void OnIconPickerClosed();
  // The picked icon in place of the dashed "+".
  void UpdateIconButton();
  void UpdateProfilePill();
  // Back from the Profile name field to the Profile row.
  void CloseNewProfile();
  void SubmitProfile();
  void OnProfileDone(const std::u16string& error);
  void ShowProfileError(const std::u16string& error);

  CreateCallback create_;
  CreateProfileCallback create_profile_;
  base::RepeatingClosure cancel_;
  std::vector<ProfileChoice> profiles_;
  size_t selected_profile_ = 0;
  bool profile_pending_ = false;
  raw_ptr<views::ImageView> art_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  raw_ptr<views::View> name_row_ = nullptr;
  raw_ptr<views::ImageButton> icon_button_ = nullptr;
  raw_ptr<views::Textfield> name_ = nullptr;
  raw_ptr<views::Label> error_ = nullptr;
  raw_ptr<views::View> profile_row_ = nullptr;
  raw_ptr<views::ImageView> profile_icon_ = nullptr;
  raw_ptr<views::Label> profile_label_ = nullptr;
  raw_ptr<views::Button> profile_pill_ = nullptr;
  raw_ptr<views::Label> profile_name_ = nullptr;
  raw_ptr<views::ImageView> profile_chevron_ = nullptr;
  raw_ptr<views::ImageView> profile_info_ = nullptr;
  raw_ptr<views::View> new_profile_row_ = nullptr;
  raw_ptr<views::ImageView> new_profile_icon_ = nullptr;
  raw_ptr<views::Textfield> new_profile_name_ = nullptr;
  raw_ptr<views::Label> profile_error_ = nullptr;
  raw_ptr<views::View> theme_row_ = nullptr;
  raw_ptr<views::ImageView> theme_icon_ = nullptr;
  raw_ptr<views::Label> theme_label_ = nullptr;
  raw_ptr<views::LabelButton> create_button_ = nullptr;
  raw_ptr<views::LabelButton> cancel_button_ = nullptr;
  SpaceIconChoice icon_;
  bool capture_scheduled_ = false;
  // The picker's delegate outlives its widget; both close with the panel,
  // before the name field they anchor to.
  std::unique_ptr<views::BubbleDialogDelegate> picker_delegate_;
  std::unique_ptr<views::Widget> picker_widget_;
  raw_ptr<SpaceIconPickerView> picker_ = nullptr;
  // The runner goes before its model.
  std::unique_ptr<ui::SimpleMenuModel> profile_menu_model_;
  std::unique_ptr<views::MenuRunner> profile_menu_runner_;
  base::WeakPtrFactory<CreateSpacePanel> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_CREATE_SPACE_PANEL_H_
