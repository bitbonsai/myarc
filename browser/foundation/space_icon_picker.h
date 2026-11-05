#ifndef MYARC_SPACE_ICON_PICKER_H_
#define MYARC_SPACE_ICON_PICKER_H_

#include <array>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "myarc/space_icon_catalog.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class LabelButton;
class ScrollView;
class Textfield;
}  // namespace views

namespace myarc {

class IconGrid;

// Contents of Arc's Space icon popover: an Emoji | Icon switch, a search
// field, an 8-column grid and, on the Emoji tab, category tabs that scroll the
// grid. Only bundled emoji and icons are offered. Clicking an item, or Return
// on it, runs `pick` once; the owner closes the bubble.
class SpaceIconPickerView final : public views::View,
                                  public views::TextfieldController {
  METADATA_HEADER(SpaceIconPickerView, views::View)

 public:
  using PickCallback = base::OnceCallback<void(SpaceIconChoice)>;

  // Opens on the Icon tab for a named icon, else on Emoji.
  SpaceIconPickerView(const SpaceIconChoice& current, PickCallback pick);
  ~SpaceIconPickerView() override;

  views::Textfield* search() { return search_; }
  // Shows `tab`'s items, keeping the search text.
  void SetTab(SpaceIconChoice::Kind tab);

 private:
  // views::View:
  void OnThemeChanged() override;
  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  void Refilter();
  void ShowCategory(EmojiCategory category);
  void UpdateCategoryTabs();
  void Pick(size_t index);

  PickCallback pick_;
  SpaceIconChoice::Kind tab_ = SpaceIconChoice::Kind::kEmoji;
  raw_ptr<views::View> segments_ = nullptr;
  raw_ptr<views::LabelButton> emoji_tab_ = nullptr;
  raw_ptr<views::LabelButton> icon_tab_ = nullptr;
  raw_ptr<views::View> search_row_ = nullptr;
  raw_ptr<views::ImageView> search_icon_ = nullptr;
  raw_ptr<views::Textfield> search_ = nullptr;
  raw_ptr<views::ScrollView> scroll_ = nullptr;
  raw_ptr<IconGrid> grid_ = nullptr;
  raw_ptr<views::Label> empty_ = nullptr;
  raw_ptr<views::View> separator_ = nullptr;
  raw_ptr<views::View> categories_ = nullptr;
  std::array<raw_ptr<views::ImageButton>, kEmojiCategoryCount> category_tabs_ =
      {};
  EmojiCategory current_category_ = EmojiCategory::kSmileys;
  base::CallbackListSubscription scrolled_;
};

}  // namespace myarc

#endif  // MYARC_SPACE_ICON_PICKER_H_
