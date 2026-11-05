#ifndef MYARC_CREATE_SPACE_PANEL_H_
#define MYARC_CREATE_SPACE_PANEL_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class LabelButton;
}  // namespace views

namespace myarc {

// Arc's "Create a Space" panel, which takes the sidebar's place: a name field,
// the Profile the Space will use, and Create or Cancel. Return creates and Esc
// cancels. The icon button and the theme row are placeholders for now.
class CreateSpacePanel final : public views::View,
                               public views::TextfieldController {
  METADATA_HEADER(CreateSpacePanel, views::View)

 public:
  // `create` gets the trimmed name and returns why it failed, or nothing once
  // the Space exists. Both callbacks run after the pressed control's handlers
  // unwind and may delete the panel.
  using CreateCallback =
      base::RepeatingCallback<std::u16string(const std::u16string& name)>;
  CreateSpacePanel(std::u16string profile_name,
                   CreateCallback create,
                   base::RepeatingClosure cancel);
  ~CreateSpacePanel() override;

  void FocusName();

 private:
  // views::View:
  void OnThemeChanged() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  std::u16string TrimmedName() const;
  void UpdateCreateButton();
  void Create();
  void Cancel();
  void RunCreate();

  CreateCallback create_;
  base::RepeatingClosure cancel_;
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
  raw_ptr<views::View> profile_pill_ = nullptr;
  raw_ptr<views::Label> profile_name_ = nullptr;
  raw_ptr<views::ImageView> profile_chevron_ = nullptr;
  raw_ptr<views::ImageView> profile_info_ = nullptr;
  raw_ptr<views::View> theme_row_ = nullptr;
  raw_ptr<views::ImageView> theme_icon_ = nullptr;
  raw_ptr<views::Label> theme_label_ = nullptr;
  raw_ptr<views::LabelButton> create_button_ = nullptr;
  raw_ptr<views::LabelButton> cancel_button_ = nullptr;
  base::WeakPtrFactory<CreateSpacePanel> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_CREATE_SPACE_PANEL_H_
