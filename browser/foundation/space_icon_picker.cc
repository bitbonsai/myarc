#include "myarc/space_icon_picker.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "myarc/icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;
using CrossAxis = views::BoxLayout::CrossAxisAlignment;
using MainAxis = views::BoxLayout::MainAxisAlignment;

// Measured from Arc's popover: an 8-column grid of 31 DIP cells with emoji
// about 18 DIP across, under a 30 DIP search field.
constexpr int kColumns = 8;
constexpr int kCell = 31;
constexpr int kEmojiSize = 19;
constexpr int kIconSize = 18;
constexpr int kGridRows = 5;
constexpr int kCategoryRowHeight = 34;

// macOS's accent blue, as Arc's segmented control and search ring use it.
constexpr SkColor kAccent = SkColorSetRGB(0x1A, 0x84, 0xFF);

struct Colors {
  SkColor ink;
  SkColor muted;
  SkColor segments;
  SkColor field;
  SkColor ring;
  SkColor hover;
  SkColor separator;
};

// Dark values are sampled from Arc's dark popover.
Colors GetColors(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  const SkColor ink = dark ? SK_ColorWHITE : SK_ColorBLACK;
  return {.ink = ink,
          .muted = SkColorSetA(ink, 0x8C),
          .segments = SkColorSetA(ink, dark ? 0x1C : 0x10),
          .field = dark ? SkColorSetARGB(0x80, 0x10, 0x10, 0x14)
                        : SkColorSetA(SK_ColorBLACK, 0x0A),
          .ring = SkColorSetA(kAccent, 0xB0),
          .hover = SkColorSetA(ink, dark ? 0x1C : 0x14),
          .separator = SkColorSetA(ink, 0x1F)};
}

gfx::FontList Font(int delta, gfx::Font::Weight weight) {
  return gfx::FontList().Derive(delta, gfx::Font::NORMAL, weight);
}

void SetButtonFont(views::LabelButton& button, const gfx::FontList& font) {
  for (views::View* child : button.children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(font);
    }
  }
}

std::unique_ptr<views::View> Row(const gfx::Insets& insets, int spacing) {
  auto row = std::make_unique<views::View>();
  auto* layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kHorizontal, insets, spacing));
  layout->set_cross_axis_alignment(CrossAxis::kCenter);
  return row;
}

}  // namespace

// The picker's grid, painted rather than built from buttons so ~1,900 emoji
// stay cheap. Hover and the arrow keys highlight a cell; a click, Return or
// Space picks it.
class IconGrid final : public views::View {
  METADATA_HEADER(IconGrid, views::View)

 public:
  struct Item {
    SpaceIconChoice choice;
    std::u16string name;
    EmojiCategory category;
  };

  explicit IconGrid(base::RepeatingCallback<void(size_t)> pick)
      : pick_(std::move(pick)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
  }

  void SetItems(std::vector<Item> items, std::u16string accessible_name) {
    items_ = std::move(items);
    images_.assign(items_.size(), {});
    hovered_.reset();
    selected_.reset();
    GetViewAccessibility().SetName(std::move(accessible_name));
    PreferredSizeChanged();
    SchedulePaint();
  }

  void SetColors(SkColor icon, SkColor hover) {
    icon_color_ = icon;
    hover_color_ = hover;
    images_.assign(items_.size(), {});
    SchedulePaint();
  }

  size_t size() const { return items_.size(); }
  const Item& item(size_t index) const { return items_[index]; }
  std::optional<size_t> selected() const { return selected_; }

  std::optional<size_t> FirstOf(EmojiCategory category) const {
    const auto it = std::ranges::find(items_, category, &Item::category);
    if (it == items_.end()) {
      return std::nullopt;
    }
    return static_cast<size_t>(it - items_.begin());
  }

  static int RowTop(size_t index) {
    return static_cast<int>(index / kColumns) * kCell;
  }

  void Select(std::optional<size_t> index) {
    if (index && *index >= items_.size()) {
      index.reset();
    }
    selected_ = index;
    SchedulePaint();
    if (selected_) {
      ScrollRectToVisible(CellBounds(*selected_));
      GetViewAccessibility().AnnouncePolitely(items_[*selected_].name);
    }
  }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int rows =
        static_cast<int>((items_.size() + kColumns - 1) / kColumns);
    return gfx::Size(kColumns * kCell, rows * kCell);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    gfx::Rect clip;
    if (!canvas->GetClipBounds(&clip)) {
      return;
    }
    const size_t first =
        static_cast<size_t>(std::max(0, clip.y() / kCell)) * kColumns;
    const size_t last =
        std::min(items_.size(),
                 static_cast<size_t>(clip.bottom() / kCell + 1) * kColumns);
    const std::optional<size_t> highlight =
        hovered_ ? hovered_ : (HasFocus() ? selected_ : std::nullopt);
    for (size_t index = first; index < last; ++index) {
      const gfx::Rect cell = CellBounds(index);
      if (highlight == index) {
        cc::PaintFlags flags;
        flags.setAntiAlias(true);
        flags.setColor(hover_color_);
        canvas->DrawRoundRect(gfx::RectF(cell), 6, flags);
      }
      ui::ImageModel& image = images_[index];
      if (image.IsEmpty()) {
        const bool emoji =
            items_[index].choice.kind == SpaceIconChoice::Kind::kEmoji;
        image = SpaceIconImage(items_[index].choice, icon_color_,
                               emoji ? kEmojiSize : kIconSize);
      }
      const gfx::ImageSkia raster = image.Rasterize(GetColorProvider());
      canvas->DrawImageInt(raster,
                           cell.x() + (cell.width() - raster.width()) / 2,
                           cell.y() + (cell.height() - raster.height()) / 2);
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    pressed_ = IndexAt(event.location());
    return event.IsOnlyLeftMouseButton() && pressed_.has_value();
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    const std::optional<size_t> index = IndexAt(event.location());
    if (index && index == std::exchange(pressed_, std::nullopt)) {
      pick_.Run(*index);
    }
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    SetHovered(IndexAt(event.location()));
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    SetHovered(IndexAt(event.location()));
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    SetHovered(std::nullopt);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (items_.empty()) {
      return false;
    }
    const size_t last = items_.size() - 1;
    const size_t at = selected_.value_or(0);
    switch (event.key_code()) {
      case ui::VKEY_LEFT:
        Select(at == 0 ? 0 : at - 1);
        return true;
      case ui::VKEY_RIGHT:
        Select(std::min(last, at + 1));
        return true;
      case ui::VKEY_UP:
        Select(at < kColumns ? at : at - kColumns);
        return true;
      case ui::VKEY_DOWN:
        Select(std::min(last, at + kColumns));
        return true;
      case ui::VKEY_HOME:
        Select(0);
        return true;
      case ui::VKEY_END:
        Select(last);
        return true;
      case ui::VKEY_RETURN:
      case ui::VKEY_SPACE:
        pick_.Run(at);
        return true;
      default:
        return false;
    }
  }

  void OnFocus() override {
    if (!selected_ && !items_.empty()) {
      Select(0);
    }
    SchedulePaint();
  }

  void OnBlur() override { SchedulePaint(); }

  std::u16string GetRenderedTooltipText(
      const gfx::Point& point) const override {
    const std::optional<size_t> index = IndexAt(point);
    return index ? items_[*index].name : std::u16string();
  }

 private:
  gfx::Rect CellBounds(size_t index) const {
    return gfx::Rect(static_cast<int>(index % kColumns) * kCell, RowTop(index),
                     kCell, kCell);
  }

  std::optional<size_t> IndexAt(const gfx::Point& point) const {
    if (point.x() < 0 || point.y() < 0 || point.x() >= kColumns * kCell) {
      return std::nullopt;
    }
    const size_t index =
        static_cast<size_t>(point.y() / kCell) * kColumns + point.x() / kCell;
    return index < items_.size() ? std::optional(index) : std::nullopt;
  }

  void SetHovered(std::optional<size_t> index) {
    if (hovered_ != index) {
      hovered_ = index;
      SchedulePaint();
    }
  }

  base::RepeatingCallback<void(size_t)> pick_;
  std::vector<Item> items_;
  // Drawn on first paint, dropped when colors change.
  std::vector<ui::ImageModel> images_;
  std::optional<size_t> hovered_;
  std::optional<size_t> selected_;
  std::optional<size_t> pressed_;
  SkColor icon_color_ = SK_ColorWHITE;
  SkColor hover_color_ = SK_ColorTRANSPARENT;
};

BEGIN_METADATA(IconGrid)
END_METADATA

SpaceIconPickerView::SpaceIconPickerView(const SpaceIconChoice& current,
                                         PickCallback pick)
    : pick_(std::move(pick)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets::TLBR(12, 11, 4, 11), 0));
  layout->set_cross_axis_alignment(CrossAxis::kStretch);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Space icon");

  // Emoji | Icon, centred.
  auto* segments_row = AddChildView(Row(gfx::Insets(), 0));
  static_cast<views::BoxLayout*>(segments_row->GetLayoutManager())
      ->set_main_axis_alignment(MainAxis::kCenter);
  segments_ = segments_row->AddChildView(Row(gfx::Insets(2), 0));
  segments_->GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  const auto add_segment = [this](const std::u16string& text,
                                  SpaceIconChoice::Kind kind) {
    auto button = std::make_unique<views::LabelButton>(
        base::BindRepeating(&SpaceIconPickerView::SetTab,
                            base::Unretained(this), kind),
        text);
    button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    button->SetPreferredSize(gfx::Size(52, 24));
    button->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    SetButtonFont(*button, Font(1, gfx::Font::Weight::MEDIUM));
    button->GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
    return segments_->AddChildView(std::move(button));
  };
  emoji_tab_ = add_segment(u"Emoji", SpaceIconChoice::Kind::kEmoji);
  icon_tab_ = add_segment(u"Icon", SpaceIconChoice::Kind::kNamed);

  // Search: the magnifier 9 DIP in, inside a ringed field.
  search_row_ = AddChildView(Row(gfx::Insets::TLBR(0, 9, 0, 8), 6));
  search_row_->SetPreferredSize(gfx::Size(0, 30));
  search_row_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, 0, 0, 0));
  search_icon_ =
      search_row_->AddChildView(std::make_unique<views::ImageView>());
  auto search = std::make_unique<views::Textfield>();
  search->SetBackgroundEnabled(false);
  search->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  search->SetFontList(Font(2, gfx::Font::Weight::NORMAL));
  search->SetPlaceholderText(u"Search");
  search->GetViewAccessibility().SetName(u"Search emoji and icons");
  search->set_controller(this);
  search_ = search_row_->AddChildView(std::move(search));
  static_cast<views::BoxLayout*>(search_row_->GetLayoutManager())
      ->SetFlexForView(search_, 1);

  scroll_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(9, 0, 0, 0));
  scroll_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_->SetBackgroundColor(std::nullopt);
  scroll_->SetDrawOverflowIndicator(false);
  grid_ = scroll_->SetContents(std::make_unique<IconGrid>(
      base::BindRepeating(&SpaceIconPickerView::Pick, base::Unretained(this))));
  scrolled_ = scroll_->AddContentsScrolledCallback(base::BindRepeating(
      &SpaceIconPickerView::UpdateCategoryTabs, base::Unretained(this)));

  empty_ = AddChildView(std::make_unique<views::Label>(u"No results"));
  empty_->SetFontList(Font(0, gfx::Font::Weight::NORMAL));
  empty_->SetAutoColorReadabilityEnabled(false);
  empty_->SetBackgroundColor(SK_ColorTRANSPARENT);
  empty_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(9, 0, 0, 0));
  empty_->SetVisible(false);

  separator_ = AddChildView(std::make_unique<views::View>());
  separator_->SetPreferredSize(gfx::Size(0, 1));

  categories_ = AddChildView(Row(gfx::Insets(), 0));
  categories_->SetPreferredSize(gfx::Size(0, kCategoryRowHeight));
  categories_->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  categories_->GetViewAccessibility().SetName(u"Emoji categories");
  for (size_t index = 0; index < kEmojiCategoryCount; ++index) {
    const auto category = static_cast<EmojiCategory>(index);
    auto button = std::make_unique<views::ImageButton>(base::BindRepeating(
        &SpaceIconPickerView::ShowCategory, base::Unretained(this), category));
    button->SetPreferredSize(gfx::Size(kCell, kCategoryRowHeight));
    button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    const std::u16string name(EmojiCategoryName(category));
    button->SetTooltipText(name);
    button->GetViewAccessibility().SetName(name);
    category_tabs_[index] = categories_->AddChildView(std::move(button));
  }

  SetTab(current.kind == SpaceIconChoice::Kind::kNamed
             ? SpaceIconChoice::Kind::kNamed
             : SpaceIconChoice::Kind::kEmoji);
}

SpaceIconPickerView::~SpaceIconPickerView() = default;

void SpaceIconPickerView::SetTab(SpaceIconChoice::Kind tab) {
  tab_ = tab;
  const bool emoji = tab == SpaceIconChoice::Kind::kEmoji;
  separator_->SetVisible(emoji);
  categories_->SetVisible(emoji);
  // The Icon tab gives the category row's height to the grid, so the popover
  // keeps its size.
  const int height =
      kGridRows * kCell - 5 + (emoji ? 0 : kCategoryRowHeight + 1);
  scroll_->ClipHeightTo(height, height);
  empty_->SetPreferredSize(gfx::Size(0, height));
  emoji_tab_->GetViewAccessibility().SetIsSelected(emoji);
  icon_tab_->GetViewAccessibility().SetIsSelected(!emoji);
  if (GetColorProvider()) {
    OnThemeChanged();
  }
  Refilter();
}

void SpaceIconPickerView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const Colors colors = GetColors(GetColorProvider());
  const bool emoji = tab_ == SpaceIconChoice::Kind::kEmoji;
  segments_->SetBackground(
      views::CreateRoundedRectBackground(colors.segments, 8));
  for (views::LabelButton* tab : {emoji_tab_.get(), icon_tab_.get()}) {
    const bool selected = (tab == emoji_tab_) == emoji;
    tab->SetBackground(selected ? views::CreateRoundedRectBackground(kAccent, 6)
                                : nullptr);
    tab->SetEnabledTextColors(selected ? SK_ColorWHITE
                                       : SkColorSetA(colors.ink, 0xD9));
  }
  search_row_->SetBackground(
      views::CreateRoundedRectBackground(colors.field, 9));
  search_row_->SetBorder(views::CreateRoundedRectBorder(2, 9, colors.ring));
  search_icon_->SetImage(
      ui::ImageModel::FromVectorIcon(kSearchIcon, colors.muted, 16));
  search_->SetColor(colors.ink);
  grid_->SetColors(SkColorSetA(colors.ink, 0xE6), colors.hover);
  empty_->SetEnabledColor(colors.muted);
  separator_->SetBackground(views::CreateSolidBackground(colors.separator));
  UpdateCategoryTabs();
}

void SpaceIconPickerView::ContentsChanged(views::Textfield* sender,
                                          const std::u16string& new_contents) {
  Refilter();
}

bool SpaceIconPickerView::HandleKeyEvent(views::Textfield* sender,
                                         const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::kKeyPressed || !grid_->size()) {
    return false;
  }
  switch (key_event.key_code()) {
    case ui::VKEY_RETURN:
      Pick(0);
      return true;
    case ui::VKEY_DOWN:
      grid_->RequestFocus();
      grid_->Select(0);
      return true;
    default:
      return false;
  }
}

void SpaceIconPickerView::Refilter() {
  const std::string query =
      base::ToLowerASCII(base::UTF16ToUTF8(search_->GetText()));
  std::vector<IconGrid::Item> items;
  if (tab_ == SpaceIconChoice::Kind::kEmoji) {
    for (const EmojiEntry& entry : BundledEmoji()) {
      const std::string_view keywords = entry.keywords;
      if (MatchesQuery(keywords, query)) {
        // The CLDR name leads the keywords.
        items.push_back(
            {.choice = {SpaceIconChoice::Kind::kEmoji, entry.emoji},
             .name = base::UTF8ToUTF16(keywords.substr(0, keywords.find('|'))),
             .category = entry.category});
      }
    }
  } else {
    for (const NamedSpaceIcon& icon : BundledSpaceIcons()) {
      std::string name = icon.name;
      if (MatchesQuery(name, query)) {
        std::u16string label = base::UTF8ToUTF16(name);
        std::ranges::replace(label, u'-', u' ');
        items.push_back({.choice = {SpaceIconChoice::Kind::kNamed, name},
                         .name = std::move(label),
                         .category = EmojiCategory::kSymbols});
      }
    }
  }
  const bool any = !items.empty();
  grid_->SetItems(std::move(items),
                  tab_ == SpaceIconChoice::Kind::kEmoji ? u"Emoji" : u"Icons");
  scroll_->SetVisible(any);
  empty_->SetVisible(!any);
  scroll_->ScrollToOffset(gfx::PointF());
  UpdateCategoryTabs();
}

void SpaceIconPickerView::ShowCategory(EmojiCategory category) {
  if (!search_->GetText().empty()) {
    search_->SetText({});
    Refilter();
  }
  if (const std::optional<size_t> first = grid_->FirstOf(category)) {
    current_category_ = category;
    scroll_->ScrollToOffset(gfx::PointF(0, IconGrid::RowTop(*first)));
    UpdateCategoryTabs();
  }
}

void SpaceIconPickerView::UpdateCategoryTabs() {
  if (grid_->size()) {
    // The category of the first fully visible row.
    const int top = scroll_->GetVisibleRect().y();
    const size_t index =
        std::min(grid_->size() - 1,
                 static_cast<size_t>((top + kCell / 2) / kCell) * kColumns);
    current_category_ = grid_->item(index).category;
  }
  if (!GetColorProvider()) {
    return;
  }
  const Colors colors = GetColors(GetColorProvider());
  for (size_t index = 0; index < kEmojiCategoryCount; ++index) {
    const auto category = static_cast<EmojiCategory>(index);
    const SkColor color =
        category == current_category_ ? colors.ink : colors.muted;
    category_tabs_[index]->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(EmojiCategoryIcon(category), color, 17));
  }
}

void SpaceIconPickerView::Pick(size_t index) {
  if (index >= grid_->size() || !pick_) {
    return;
  }
  std::move(pick_).Run(grid_->item(index).choice);
}

BEGIN_METADATA(SpaceIconPickerView)
END_METADATA

}  // namespace myarc
