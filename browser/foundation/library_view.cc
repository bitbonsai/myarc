#include "myarc/library_view.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "myarc/icons.h"
#include "myarc/library_logic.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace myarc {

namespace {

using Orientation = views::BoxLayout::Orientation;
using MainAxis = views::BoxLayout::MainAxisAlignment;
using CrossAxis = views::BoxLayout::CrossAxisAlignment;
using Section = LibraryView::Section;

// Same translucent lifts as the sidebar: the rail's selected tile matches the
// Favorites tile, muted text matches the footer icons.
struct Colors {
  SkColor tile;
  SkColor column;
  SkColor muted;
};

// Arc's accent, sampled from its Library: the selected rail icon and the
// create button.
constexpr SkColor kAccent = SkColorSetRGB(0x21, 0x2A, 0xFB);

Colors GetColors(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  return dark ? Colors{SkColorSetA(SK_ColorWHITE, 0x18),
                       SkColorSetA(SK_ColorWHITE, 0x0D),
                       SkColorSetA(SK_ColorWHITE, 0x99)}
              : Colors{SkColorSetA(SK_ColorWHITE, 0x80),
                       SkColorSetA(SK_ColorWHITE, 0x59),
                       SkColorSetA(SK_ColorBLACK, 0x99)};
}

struct SectionInfo {
  Section section;
  // Constant table of static icons.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
  const char16_t* name;
  const char16_t* search;
};

constexpr auto kSections = std::to_array<SectionInfo>({
    {Section::kMedia, &kImageIcon, u"Media", u"Search Media\u2026"},
    {Section::kDownloads, &kCircleArrowDownIcon, u"Downloads",
     u"Search Downloads\u2026"},
    {Section::kEasels, &kPresentationIcon, u"Easels", u"Search Easels\u2026"},
    {Section::kSpaces, &kCopyIcon, u"Spaces", nullptr},
    {Section::kBoosts, &kWandIcon, u"Boosts", u"Search Boosts\u2026"},
    {Section::kArchive, &kArchiveIcon, u"Archived Tabs",
     u"Search Archive\u2026"},
});

const SectionInfo& Info(Section section) {
  for (const auto& info : kSections) {
    if (info.section == section) {
      return info;
    }
  }
  NOTREACHED();
}

std::unique_ptr<views::View> Box(Orientation orientation,
                                 const gfx::Insets& insets,
                                 int spacing) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(orientation, insets, spacing));
  return view;
}

std::unique_ptr<views::Label> MakeLabel(const std::u16string& text,
                                        const gfx::FontList& font,
                                        SkColor color) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(font);
  label->SetEnabledColor(color);
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  return label;
}

// Transparent text button like the sidebar's rows.
std::unique_ptr<views::MdTextButton> TextButton(
    views::Button::PressedCallback callback,
    const std::u16string& text) {
  auto button = std::make_unique<views::MdTextButton>(
      std::move(callback), text, views::style::CONTEXT_BUTTON_MD,
      /*use_text_color_for_icon=*/false);
  button->SetStyle(ui::ButtonStyle::kText);
  button->SetTextSubpixelRenderingEnabled(false);
  button->SetBgColorOverrideDeprecated(SK_ColorTRANSPARENT);
  button->SetEnabledTextColors(ui::kColorSysOnSurface);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetCornerRadius(8);
  button->SetCustomPadding(gfx::Insets::VH(6, 8));
  button->SetMinSize(gfx::Size(0, 32));
  button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  button->SetElideBehavior(gfx::ELIDE_TAIL);
  button->SetImageLabelSpacing(8);
  return button;
}

// A button whose children are plain views; it paints its own hover tile.
class TileButton : public views::Button {
  METADATA_HEADER(TileButton, views::Button)

 public:
  explicit TileButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 10);
  }

  void SetSelected(bool selected) {
    selected_ = selected;
    GetViewAccessibility().SetIsSelected(selected);
    Update();
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    Update();
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    Update();
  }

 protected:
  bool selected() const { return selected_; }

  virtual void Update() {
    const bool lifted =
        selected_ || GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    SetBackground(lifted ? views::CreateRoundedRectBackground(
                               GetColors(GetColorProvider()).tile, 10)
                         : nullptr);
  }

 private:
  bool selected_ = false;
};

BEGIN_METADATA(TileButton)
END_METADATA

// Arc's rail entry: icon over a short label, the selected icon in the accent.
class LibraryRailItem final : public TileButton {
  METADATA_HEADER(LibraryRailItem, TileButton)

 public:
  LibraryRailItem(PressedCallback callback, const SectionInfo& info)
      : TileButton(std::move(callback)), icon_(*info.icon) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kVertical, gfx::Insets::VH(12, 4), 6));
    layout->set_main_axis_alignment(MainAxis::kCenter);
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    image_ = AddChildView(std::make_unique<views::ImageView>());
    label_ = AddChildView(
        MakeLabel(info.name,
                  gfx::FontList().Derive(-2, gfx::Font::NORMAL,
                                         gfx::Font::Weight::SEMIBOLD),
                  GetColors(nullptr).muted));
    // Arc's rail tiles are 93x77 DIP, 100 DIP apart.
    SetPreferredSize(gfx::Size(96, 77));
    GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
    GetViewAccessibility().SetName(info.name);
    SetTooltipText(info.name);
  }

 private:
  void Update() override {
    TileButton::Update();
    const Colors colors = GetColors(GetColorProvider());
    image_->SetImage(ui::ImageModel::FromVectorIcon(
        *icon_, selected() ? kAccent : colors.muted, 22));
    label_->SetEnabledColor(
        selected() && GetColorProvider()
            ? GetColorProvider()->GetColor(ui::kColorSysOnSurface)
            : colors.muted);
  }

  const raw_ref<const gfx::VectorIcon> icon_;
  raw_ptr<views::ImageView> image_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
};

BEGIN_METADATA(LibraryRailItem)
END_METADATA

}  // namespace

LibraryView::LibraryView(SidebarView::Delegate& delegate,
                         base::RepeatingClosure close,
                         Section section)
    : delegate_(delegate), close_(std::move(close)), section_(section) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kHorizontal,
                                                      gfx::Insets(), 0));
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Library");

  auto* rail = AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(0, 8, 10, 8), 0));
  rail->SetPreferredSize(gfx::Size(library::kRailWidth, 0));
  auto* rail_layout = static_cast<views::BoxLayout*>(rail->GetLayoutManager());
  auto* items = rail->AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(36, 0, 0, 0), 23));
  items->GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  items->GetViewAccessibility().SetName(u"Library sections");
  auto* items_layout =
      static_cast<views::BoxLayout*>(items->GetLayoutManager());
  items_layout->set_main_axis_alignment(MainAxis::kCenter);
  items_layout->set_cross_axis_alignment(CrossAxis::kCenter);
  rail_layout->SetFlexForView(items, 1);
  // Arc centers the tiles 18 DIP below the rail's middle.
  for (const auto& info : kSections) {
    rail_items_.push_back(items->AddChildView(std::make_unique<LibraryRailItem>(
        base::BindRepeating(&LibraryView::SelectSection, base::Unretained(this),
                            info.section),
        info)));
  }
  auto* footer =
      rail->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 0));
  auto back = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](LibraryView* view) { view->PostAction(view->close_); },
      base::Unretained(this)));
  back->SetImageModel(views::Button::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          kArrowLeftIcon, GetColors(nullptr).muted, 18));
  back->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  back->SetPreferredSize(gfx::Size(32, 32));
  back->SetFocusBehavior(FocusBehavior::ALWAYS);
  back->SetTooltipText(u"Back");
  back->GetViewAccessibility().SetName(u"Back to sidebar");
  views::InstallRoundRectHighlightPathGenerator(back.get(), gfx::Insets(), 8);
  footer->AddChildView(std::move(back));

  pane_ = AddChildView(std::make_unique<views::View>());
  pane_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));
  static_cast<views::BoxLayout*>(GetLayoutManager())->SetFlexForView(pane_, 1);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  Rebuild();
}

LibraryView::~LibraryView() = default;

void LibraryView::SetSpaces(Spaces spaces, uint64_t current) {
  if (spaces == spaces_ && current == current_space_) {
    return;
  }
  spaces_ = std::move(spaces);
  current_space_ = current;
  if (section_ == Section::kSpaces) {
    Rebuild();
  }
}

void LibraryView::SelectSection(Section section) {
  if (section == section_) {
    return;
  }
  section_ = section;
  query_.clear();
  // Keys from the old section mean nothing in the new one.
  keyed_views_.clear();
  Rebuild();
}

void LibraryView::RefreshDownloads() {
  if (section_ != Section::kDownloads || !list_) {
    return;
  }
  auto downloads = delegate_->GetRecentDownloads();
  // Progress ticks rarely change what the list shows.
  if (downloads == downloads_) {
    return;
  }
  downloads_ = std::move(downloads);
  UpdateDownloads();
}

void LibraryView::FocusSection() {
  for (size_t i = 0; i < std::size(kSections); ++i) {
    if (kSections[i].section == section_) {
      rail_items_[i]->RequestFocus();
    }
  }
}

gfx::Size LibraryView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::View::CalculatePreferredSize(available_size);
  size.set_width(section_ == Section::kSpaces
                     ? library::SpacesWidth(spaces_.size())
                     : library::kListWidth);
  return size;
}

bool LibraryView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // The window offers accelerators before the focused view sees the key, so
  // leave Esc to the omnibox and pages unless focus is in the sidebar.
  const views::FocusManager* manager = GetFocusManager();
  const views::View* focused = manager ? manager->GetFocusedView() : nullptr;
  if (focused && !parent()->Contains(focused)) {
    return false;
  }
  PostAction(close_);
  return true;
}

void LibraryView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // Pane fills are plain colors picked for the current scheme. Rebuild only
  // when they change: inactive-window updates land here too.
  const ui::ColorProvider* provider = GetColorProvider();
  const std::pair colors(provider->GetColor(ui::kColorSysSurface),
                         provider->GetColor(ui::kColorSysOnSurface));
  if (colors == theme_colors_) {
    return;
  }
  theme_colors_ = colors;
  Rebuild();
}

std::optional<uint64_t> LibraryView::FocusedKey() const {
  const views::FocusManager* manager = GetFocusManager();
  const views::View* focused = manager ? manager->GetFocusedView() : nullptr;
  for (const auto& [key, view] : keyed_views_) {
    if (focused && view.get() == focused) {
      return key;
    }
  }
  return std::nullopt;
}

bool LibraryView::RestoreFocus(std::optional<uint64_t> key) {
  if (!key) {
    return false;
  }
  const auto target = std::ranges::find(
      keyed_views_, *key, &decltype(keyed_views_)::value_type::first);
  if (target == keyed_views_.end()) {
    return false;
  }
  target->second->RequestFocus();
  return true;
}

void LibraryView::PostAction(base::OnceClosure action) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<LibraryView> view, base::OnceClosure action) {
            if (view) {
              std::move(action).Run();
            }
          },
          weak_factory_.GetWeakPtr(), std::move(action)));
}

void LibraryView::Rebuild() {
  for (size_t i = 0; i < std::size(kSections); ++i) {
    static_cast<TileButton*>(rail_items_[i].get())
        ->SetSelected(kSections[i].section == section_);
  }
  const bool had_focus = pane_->Contains(
      GetFocusManager() ? GetFocusManager()->GetFocusedView() : nullptr);
  const std::optional<uint64_t> key = FocusedKey();
  search_ = nullptr;
  list_ = nullptr;
  keyed_views_.clear();
  pane_->RemoveAllChildViews();
  if (section_ == Section::kSpaces) {
    BuildSpaces();
  } else {
    BuildList();
  }
  // Keep focus on the same Space or download, else on the section's tab.
  if (had_focus && !RestoreFocus(key)) {
    FocusSection();
  }
  PreferredSizeChanged();
}

void LibraryView::BuildSpaces() {
  const Colors colors = GetColors(GetColorProvider());
  auto* layout = static_cast<views::BoxLayout*>(pane_->GetLayoutManager());
  // Columns keep their width: many Spaces or a narrow window scroll sideways,
  // so Create stays reachable.
  auto* scroll = pane_->AddChildView(std::make_unique<views::ScrollView>());
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetDrawOverflowIndicator(false);
  scroll->SetVerticalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetTreatAllScrollEventsAsHorizontal(true);
  scroll->SetUseContentsPreferredSize(true);
  layout->SetFlexForView(scroll, 1);
  // Arc's columns start 28 DIP right of the rail and 48 DIP from the top.
  auto* row =
      scroll->SetContents(Box(Orientation::kHorizontal,
                              gfx::Insets::TLBR(48, library::kColumnsLeading,
                                                49, library::kCreateTrailing),
                              library::kColumnGap));
  scroll->RegisterPostLayoutCallback(
      base::BindRepeating([](views::ScrollView* scroll) {
        // Columns take the viewport's height, and its width when they fit.
        views::View* contents = scroll->contents();
        const gfx::Rect visible = scroll->GetVisibleRect();
        const gfx::Size fill(
            std::max(contents->GetPreferredSize().width(), visible.width()),
            visible.height());
        if (contents->size() != fill) {
          contents->SetSize(fill);
        }
      }));
  auto* row_layout = static_cast<views::BoxLayout*>(row->GetLayoutManager());
  row->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  row->GetViewAccessibility().SetName(u"Spaces");
  const gfx::FontList title_font =
      gfx::FontList().Derive(0, gfx::Font::NORMAL, gfx::Font::Weight::SEMIBOLD);
  for (const auto& [id, name] : spaces_) {
    const bool current = id == current_space_;
    auto* column = row->AddChildView(
        Box(Orientation::kVertical, gfx::Insets::VH(9, 8), 4));
    column->SetBackground(
        views::CreateRoundedRectBackground(colors.column, 10));
    column->SetPreferredSize(gfx::Size(library::kColumnWidth, 0));
    row_layout->SetFlexForView(column, 1);
    auto header =
        TextButton(base::BindRepeating(
                       [](LibraryView* view, uint64_t space) {
                         // Switching rebuilds this column, and a failure can
                         // drop the Library entirely; the action holds nothing
                         // of `view`.
                         view->PostAction(base::BindOnce(
                             [](SidebarView::Delegate* delegate, uint64_t space,
                                const base::RepeatingClosure& close) {
                               delegate->SelectSpace(space);
                               close.Run();
                             },
                             base::Unretained(&view->delegate_.get()), space,
                             view->close_));
                       },
                       base::Unretained(this), id),
                   name);
    for (views::View* child : header->children()) {
      if (auto* label = views::AsViewClass<views::Label>(child)) {
        label->SetFontList(title_font);
      }
    }
    header->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kSaturnIcon, colors.muted, 16));
    if (!current) {
      header->SetEnabledTextColors(colors.muted);
    }
    header->SetTooltipText(
        (current ? u"Current Space: " : u"Switch to Space: ") + name);
    header->GetViewAccessibility().SetName(u"Space: " + name);
    header->GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    header->GetViewAccessibility().SetCheckedState(
        current ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse);
    keyed_views_.emplace_back(id, header.get());
    column->AddChildView(std::move(header));
  }

  auto* create_cell =
      row->AddChildView(Box(Orientation::kVertical, gfx::Insets(), 0));
  static_cast<views::BoxLayout*>(create_cell->GetLayoutManager())
      ->set_main_axis_alignment(MainAxis::kCenter);
  if (!spaces_.empty()) {
    create_cell->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, library::kCreateGap - library::kColumnGap, 0, 0));
  }
  auto create = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](LibraryView* view) {
        view->PostAction(
            base::BindOnce(&SidebarView::Delegate::CreateSpace,
                           base::Unretained(&view->delegate_.get())));
      },
      base::Unretained(this)));
  create->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kPlusIcon, SK_ColorWHITE, 16));
  create->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  create->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  create->SetPreferredSize(
      gfx::Size(library::kCreateSize, library::kCreateSize));
  create->SetBackground(
      views::CreateRoundedRectBackground(kAccent, library::kCreateSize / 2));
  create->SetFocusBehavior(FocusBehavior::ALWAYS);
  create->SetTooltipText(u"Create Space");
  create->GetViewAccessibility().SetName(u"Create Space");
  views::InstallCircleHighlightPathGenerator(create.get());
  keyed_views_.emplace_back(0, create.get());
  create_cell->AddChildView(std::move(create));
}

void LibraryView::BuildList() {
  const SectionInfo& info = Info(section_);
  const Colors colors = GetColors(GetColorProvider());
  auto* layout = static_cast<views::BoxLayout*>(pane_->GetLayoutManager());
  auto* content = pane_->AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(10, 4, 10, 10), 8));
  layout->SetFlexForView(content, 1);
  auto* content_layout =
      static_cast<views::BoxLayout*>(content->GetLayoutManager());

  // Arc's search field: 35 DIP tall on the tile fill, 10 DIP from the top.
  auto* field = content->AddChildView(
      Box(Orientation::kHorizontal, gfx::Insets::VH(0, 14), 10));
  field->SetBackground(views::CreateRoundedRectBackground(colors.tile, 10));
  field->SetPreferredSize(gfx::Size(0, 35));
  field->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(kSearchIcon, colors.muted, 16)));
  auto search = std::make_unique<views::Textfield>();
  search->SetBackgroundEnabled(false);
  search->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  search->SetPlaceholderText(info.search);
  search->GetViewAccessibility().SetName(info.search);
  search->SetText(query_);
  search->set_controller(this);
  // Only Downloads has anything to search yet.
  search->SetEnabled(section_ == Section::kDownloads);
  search_ = field->AddChildView(std::move(search));
  static_cast<views::BoxLayout*>(field->GetLayoutManager())
      ->SetFlexForView(search_, 1);
  static_cast<views::BoxLayout*>(field->GetLayoutManager())
      ->set_cross_axis_alignment(CrossAxis::kCenter);

  list_ = content->AddChildView(Box(Orientation::kVertical, gfx::Insets(), 0));
  content_layout->SetFlexForView(list_, 1);
  downloads_.clear();
  if (section_ == Section::kDownloads) {
    downloads_ = delegate_->GetRecentDownloads();
  }
  UpdateDownloads();
}

void LibraryView::UpdateDownloads() {
  if (!list_) {
    return;
  }
  const views::FocusManager* manager = GetFocusManager();
  const bool had_focus =
      list_->Contains(manager ? manager->GetFocusedView() : nullptr);
  const std::optional<uint64_t> key = FocusedKey();
  keyed_views_.clear();
  list_->RemoveAllChildViews();
  const Colors colors = GetColors(GetColorProvider());
  auto* list_layout = static_cast<views::BoxLayout*>(list_->GetLayoutManager());

  std::vector<SidebarView::DownloadPresentation> downloads;
  for (const auto& download : downloads_) {
    if (library::MatchesSearch(download.name, query_)) {
      downloads.push_back(download);
    }
  }

  if (downloads.empty()) {
    const gfx::VectorIcon* icon = Info(section_).icon;
    std::u16string title = u"Nothing here yet!";
    std::u16string body =
        u"Come back after you\u2019ve found some cool stuff to save.";
    switch (section_) {
      case Section::kDownloads:
        if (!query_.empty()) {
          title = u"No matches";
          body = u"No downloads match your search.";
        }
        break;
      case Section::kArchive:
        body = u"Tabs you archive will show up here.";
        break;
      case Section::kMedia:
        title = u"Not available yet";
        body = u"Media from your pages will show up here in a later version.";
        break;
      case Section::kEasels:
        title = u"Not part of myArc";
        body = u"myArc doesn\u2019t include Easels.";
        break;
      case Section::kBoosts:
        title = u"Not available yet";
        body = u"Boosts are coming in a later version.";
        break;
      case Section::kSpaces:
        NOTREACHED();
    }
    auto* empty = list_->AddChildView(
        Box(Orientation::kVertical, gfx::Insets::VH(0, 16), 8));
    list_layout->SetFlexForView(empty, 1);
    auto* empty_layout =
        static_cast<views::BoxLayout*>(empty->GetLayoutManager());
    empty_layout->set_main_axis_alignment(MainAxis::kCenter);
    empty_layout->set_cross_axis_alignment(CrossAxis::kCenter);
    auto* image = empty->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(*icon, colors.muted, 48)));
    image->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 8, 0));
    const SkColor on_surface =
        GetColorProvider()
            ? GetColorProvider()->GetColor(ui::kColorSysOnSurface)
            : SK_ColorWHITE;
    empty->AddChildView(MakeLabel(
        title,
        gfx::FontList().Derive(3, gfx::Font::NORMAL, gfx::Font::Weight::BOLD),
        on_surface));
    auto* body_label =
        empty->AddChildView(MakeLabel(body, gfx::FontList(), colors.muted));
    body_label->SetMultiLine(true);
    body_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    // Arc wraps its empty-state text at about 240 DIP.
    body_label->SizeToFit(240);
  } else {
    auto* scroll = list_->AddChildView(std::make_unique<views::ScrollView>());
    scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
    scroll->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    scroll->SetBackgroundColor(std::nullopt);
    scroll->SetDrawOverflowIndicator(false);
    list_layout->SetFlexForView(scroll, 1);
    auto* rows =
        scroll->SetContents(Box(Orientation::kVertical, gfx::Insets(), 2));
    rows->GetViewAccessibility().SetRole(ax::mojom::Role::kList);
    rows->GetViewAccessibility().SetName(u"Recent downloads");
    for (const auto& download : downloads) {
      auto row =
          TextButton(base::BindRepeating(
                         [](LibraryView* view, uint32_t id) {
                           view->PostAction(base::BindOnce(
                               &SidebarView::Delegate::OpenDownload,
                               base::Unretained(&view->delegate_.get()), id));
                         },
                         base::Unretained(this), download.id),
                     download.name);
      row->SetImageModel(views::Button::STATE_NORMAL,
                         ui::ImageModel::FromVectorIcon(kCircleArrowDownIcon,
                                                        colors.muted, 16));
      row->SetMinSize(gfx::Size(0, 36));
      row->SetTooltipText(download.detail.empty()
                              ? download.name
                              : download.name + u"\n" + download.detail);
      row->GetViewAccessibility().SetName(u"Open " + download.name);
      row->GetViewAccessibility().SetDescription(download.detail);
      keyed_views_.emplace_back(download.id, row.get());
      rows->AddChildView(std::move(row));
    }
  }

  if (section_ == Section::kDownloads) {
    // Chrome's full Downloads page keeps every download action reachable.
    auto all = TextButton(base::BindRepeating(
                              [](LibraryView* view) {
                                view->PostAction(base::BindOnce(
                                    &SidebarView::Delegate::OpenDownloads,
                                    base::Unretained(&view->delegate_.get())));
                              },
                              base::Unretained(this)),
                          u"Show All Downloads");
    all->SetEnabledTextColors(colors.muted);
    keyed_views_.emplace_back(0, all.get());
    list_->AddChildView(std::move(all));
  }
  // A refresh keeps focus on the same row, else on the search field.
  if (had_focus && !RestoreFocus(key)) {
    search_->RequestFocus();
  }
}

void LibraryView::ContentsChanged(views::Textfield* sender,
                                  const std::u16string& new_contents) {
  query_ = new_contents;
  UpdateDownloads();
}

BEGIN_METADATA(LibraryView)
END_METADATA

}  // namespace myarc
