#include "myarc/library_view.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/footer_button.h"
#include "myarc/icons.h"
#include "myarc/library_logic.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_paint_util.h"
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
#include "ui/views/controls/menu/menu_runner.h"
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

constexpr int kClearArchive = 1;

// Translucent lifts: `tile` matches the sidebar's Favorites tile and `muted`
// its footer icons. The rail and empty-state values are sampled from Arc's dark
// Library; light mode mirrors them in black.
struct Colors {
  SkColor tile;
  SkColor column;
  SkColor muted;
  SkColor rail_selected;
  SkColor rail_hover;
  SkColor rail_icon;
  SkColor rail_label_hover;
  SkColor rail_label_selected;
  SkColor empty_icon;
  SkColor empty_body;
};

// Arc's accent, sampled from its Library: the create button.
constexpr SkColor kAccent = SkColorSetRGB(0x21, 0x2A, 0xFB);
// The selected rail glyph's outline; icons.h bakes it into the duotone icons.
constexpr SkColor kRailLine = SkColorSetRGB(0xED, 0xED, 0xFD);

Colors GetColors(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  const SkColor ink = dark ? SK_ColorWHITE : SK_ColorBLACK;
  return {.tile = SkColorSetA(SK_ColorWHITE, dark ? 0x18 : 0x80),
          .column = SkColorSetA(SK_ColorWHITE, dark ? 0x06 : 0x59),
          .muted = SkColorSetA(ink, 0x99),
          .rail_selected = SkColorSetA(SK_ColorWHITE, dark ? 0x16 : 0x80),
          .rail_hover = SkColorSetA(SK_ColorWHITE, dark ? 0x0B : 0x59),
          .rail_icon = SkColorSetA(ink, 0xA6),
          .rail_label_hover = SkColorSetA(ink, 0xBF),
          .rail_label_selected = SkColorSetA(ink, 0xE6),
          .empty_icon = SkColorSetA(ink, 0x47),
          .empty_body = SkColorSetA(ink, 0x8C)};
}

struct SectionInfo {
  Section section;
  // Constant table of static icons.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
  // Duotone icon for the selected rail item; null draws `icon` in the
  // selected label color.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* active_icon;
  // Arc's own glyphs draw at 28 DIP, the Lucide ones at 24.
  int icon_size;
  const char16_t* name;
  const char16_t* search;
};

constexpr auto kSections = std::to_array<SectionInfo>({
    {Section::kMedia, &kMediaIcon, &kMediaActiveIcon, 28, u"Media",
     u"Search Media..."},
    {Section::kDownloads, &kCircleArrowDownIcon, &kCircleArrowDownActiveIcon,
     24, u"Downloads", u"Search Downloads..."},
    {Section::kSpaces, &kSpacesIcon, &kSpacesActiveIcon, 28, u"Spaces",
     nullptr},
    {Section::kBoosts, &kBoostsIcon, &kBoostsActiveIcon, 28, u"Boosts",
     u"Search Boosts..."},
    {Section::kArchive, &kArchiveIcon, &kArchiveActiveIcon, 24,
     u"Archived Tabs", u"Search Archive..."},
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
template <typename T = views::MdTextButton>
std::unique_ptr<T> TextButton(views::Button::PressedCallback callback,
                              const std::u16string& text) {
  auto button = std::make_unique<T>(
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

// Arc's rail brightens toward its right edge and ends in a hairline, so the
// pane beside it reads a step darker. Light mode darkens the same way.
class RailBackground final : public views::Background {
 public:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    const ui::ColorProvider* provider = view->GetColorProvider();
    const bool dark =
        !provider ||
        color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
    const SkColor ink = dark ? SK_ColorWHITE : SK_ColorBLACK;
    const gfx::Rect bounds = view->GetLocalBounds();
    cc::PaintFlags flags;
    flags.setShader(
        gfx::CreateGradientShader(bounds.origin(), bounds.top_right(),
                                  SkColorSetA(ink, 0), SkColorSetA(ink, 0x0A)));
    canvas->DrawRect(bounds, flags);
    canvas->FillRect(
        gfx::Rect(bounds.right() - 1, bounds.y(), 1, bounds.height()),
        SkColorSetA(ink, 0x12));
  }
};

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

// Arc's rail entry: icon over a short label. Hover lifts the tile half as much
// as selection and brightens only the label; selection swaps in the duotone
// icon. Arc flips these instantly, without a fade.
class LibraryRailItem final : public TileButton {
  METADATA_HEADER(LibraryRailItem, TileButton)

 public:
  LibraryRailItem(PressedCallback callback, const SectionInfo& info)
      : TileButton(std::move(callback)),
        icon_(*info.icon),
        active_icon_(info.active_icon),
        icon_size_(info.icon_size) {
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 12);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kVertical, gfx::Insets::VH(12, 4), 6));
    layout->set_main_axis_alignment(MainAxis::kCenter);
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    image_ = AddChildView(std::make_unique<views::ImageView>());
    // One box for every glyph size keeps the labels in line.
    image_->SetPreferredSize(gfx::Size(28, 28));
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
    const Colors colors = GetColors(GetColorProvider());
    // Arc has no pressed look: a click goes straight to selected.
    const bool hovered = GetState() == STATE_HOVERED;
    SetBackground(
        selected() || hovered
            ? views::CreateRoundedRectBackground(
                  selected() ? colors.rail_selected : colors.rail_hover, 12)
            : nullptr);
    image_->SetImage(
        selected() ? (active_icon_
                          ? ui::ImageModel::FromVectorIcon(
                                *active_icon_, kRailLine, icon_size_)
                          // Single-color glyphs follow the label so they
                          // stay visible on the light selected tile.
                          : ui::ImageModel::FromVectorIcon(
                                *icon_, colors.rail_label_selected, icon_size_))
                   : ui::ImageModel::FromVectorIcon(*icon_, colors.rail_icon,
                                                    icon_size_));
    label_->SetEnabledColor(selected() ? colors.rail_label_selected
                            : hovered  ? colors.rail_label_hover
                                       : colors.muted);
  }

  const raw_ref<const gfx::VectorIcon> icon_;
  RAW_PTR_EXCLUSION const gfx::VectorIcon* const active_icon_;
  const int icon_size_;
  raw_ptr<views::ImageView> image_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
};

BEGIN_METADATA(LibraryRailItem)
END_METADATA

// Arc's Archived Tabs row: favicon, then the title over the page's address.
class ArchiveRow final : public TileButton {
  METADATA_HEADER(ArchiveRow, TileButton)

 public:
  ArchiveRow(PressedCallback callback,
             const SidebarView::ArchivedTabPresentation& tab)
      : TileButton(std::move(callback)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(9, 12), 12));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    // Pages without a cached favicon get Arc's blank rounded square.
    icon_ = AddChildView(std::make_unique<views::ImageView>(tab.icon));
    icon_->SetImageSize(gfx::Size(16, 16));
    icon_->SetPreferredSize(gfx::Size(20, 20));
    blank_icon_ = tab.icon.IsEmpty();
    auto* text = AddChildView(Box(Orientation::kVertical, gfx::Insets(), 2));
    layout->SetFlexForView(text, 1);
    title_ = text->AddChildView(MakeLabel(
        tab.title,
        gfx::FontList().Derive(1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL),
        SK_ColorWHITE));
    url_ = text->AddChildView(
        MakeLabel(tab.url, gfx::FontList(), GetColors(nullptr).muted));
    for (views::Label* label : {title_.get(), url_.get()}) {
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      label->SetElideBehavior(gfx::ELIDE_TAIL);
    }
    std::u16string detail = tab.url;
    if (!tab.space.empty()) {
      detail += u"\nSpace: " + tab.space;
    }
    SetTooltipText(tab.title + u"\n" + detail);
    GetViewAccessibility().SetName(u"Reopen " + tab.title);
    GetViewAccessibility().SetDescription(detail);
  }

 private:
  void Update() override {
    TileButton::Update();
    const Colors colors = GetColors(GetColorProvider());
    if (blank_icon_) {
      icon_->SetBackground(views::CreateRoundedRectBackground(colors.tile, 5));
    }
    if (GetColorProvider()) {
      title_->SetEnabledColor(
          GetColorProvider()->GetColor(ui::kColorSysOnSurface));
    }
    url_->SetEnabledColor(colors.muted);
  }

  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> url_ = nullptr;
  bool blank_icon_ = false;
};

BEGIN_METADATA(ArchiveRow)
END_METADATA

// A Pin on a Space's card: favicon and title, 36 DIP tall like the sidebar's
// rows, with the same hover tile. Arc sets the title in 15pt semibold, 28 DIP
// right of the card's icon column.
class PinRow final : public TileButton {
  METADATA_HEADER(PinRow, TileButton)

 public:
  PinRow(PressedCallback callback, const SidebarView::LibraryPin& pin)
      : TileButton(std::move(callback)), icon_(pin.icon) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(0, 8), 8));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    image_ = AddChildView(std::make_unique<views::ImageView>());
    image_->SetImageSize(gfx::Size(16, 16));
    image_->SetPreferredSize(gfx::Size(20, 20));
    title_ = AddChildView(
        MakeLabel(pin.title,
                  gfx::FontList().Derive(2, gfx::Font::NORMAL,
                                         gfx::Font::Weight::SEMIBOLD),
                  SK_ColorWHITE));
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetElideBehavior(gfx::ELIDE_TAIL);
    layout->SetFlexForView(title_, 1);
    SetPreferredSize(gfx::Size(0, 36));
    SetTooltipText(pin.title);
    GetViewAccessibility().SetName(pin.title);
    GetViewAccessibility().SetDescription(u"Space Pin");
  }

 private:
  void Update() override {
    TileButton::Update();
    const ui::ColorProvider* provider = GetColorProvider();
    if (!provider) {
      return;
    }
    const SkColor on_surface = provider->GetColor(ui::kColorSysOnSurface);
    // Pins without a cached favicon get the sidebar's globe.
    image_->SetImage(icon_.IsEmpty()
                         ? ui::ImageModel::FromVectorIcon(
                               vector_icons::kGlobeIcon, on_surface, 16)
                         : icon_);
    title_->SetEnabledColor(on_surface);
  }

  const ui::ImageModel icon_;
  raw_ptr<views::ImageView> image_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
};

BEGIN_METADATA(PinRow)
END_METADATA

}  // namespace

LibraryView::LibraryView(SidebarView::Delegate& delegate,
                         base::RepeatingClosure close,
                         Section section,
                         base::WeakPtr<FooterTooltip> tooltip)
    : delegate_(delegate), close_(std::move(close)), section_(section) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kHorizontal,
                                                      gfx::Insets(), 0));
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Library");

  auto* rail = AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(0, 8, 10, 8), 0));
  rail->SetPreferredSize(gfx::Size(library::kRailWidth, 0));
  rail->SetBackground(std::make_unique<RailBackground>());
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
    // Arc clears the archive from the Archived Tabs item's context menu.
    if (info.section == Section::kArchive) {
      rail_items_.back()->set_context_menu_controller(this);
    }
  }
  auto* footer =
      rail->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 0));
  // Arc's footer is a thin back arrow with its tooltip beside it.
  auto exit = TextButton<FooterButton>(
      base::BindRepeating(
          [](LibraryView* view) { view->PostAction(view->close_); },
          base::Unretained(this)),
      u"");
  exit->SetCustomPadding(gfx::Insets());
  exit->SetMinSize(gfx::Size(32, 32));
  exit->SetMaxSize(gfx::Size(32, 32));
  exit->SetPreferredSize(gfx::Size(32, 32));
  exit->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  exit->SetImageCentered(true);
  exit->SetFooterTooltip(std::move(tooltip),
                         {u"Exit Library", {}, /*beside=*/true});
  exit->GetViewAccessibility().SetName(u"Exit Library");
  exit_ = footer->AddChildView(std::move(exit));

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
  UpdateList(/*keep_scroll=*/true);
}

void LibraryView::RefreshArchive() {
  if (section_ != Section::kArchive || !list_) {
    return;
  }
  auto archived = delegate_->GetArchivedTabs();
  if (archived == archived_) {
    return;
  }
  archived_ = std::move(archived);
  UpdateList(/*keep_scroll=*/true);
}

void LibraryView::ShowExitTooltipNow() {
  exit_->ShowFooterTooltipNow();
}

void LibraryView::HoldFocus() {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  RequestFocus();
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
  // The first Esc clears a search, the next exits.
  if (search_ && focused == search_ && !search_->GetText().empty()) {
    search_->SetText(std::u16string());
    ContentsChanged(search_, std::u16string());
    return true;
  }
  PostAction(close_);
  return true;
}

void LibraryView::OnBlur() {
  views::View::OnBlur();
  // Deactivating the window clears focus and restores it later, so only a
  // move to another view ends the hold.
  const views::FocusManager* manager = GetFocusManager();
  if (manager && manager->GetFocusedView()) {
    SetFocusBehavior(FocusBehavior::NEVER);
  }
}

void LibraryView::OnThemeChanged() {
  views::View::OnThemeChanged();
  exit_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kArrowLeftIcon,
                                     GetColors(GetColorProvider()).muted, 24));
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
  for (const auto& space : spaces_) {
    const uint64_t id = space.id;
    const std::u16string& name = space.name;
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
    // Arc lines the name up with the Pin titles below it.
    header->SetImageLabelSpacing(12);
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
    if (space.pins.empty()) {
      continue;
    }
    // A long list scrolls inside its card, as in Arc, so every card keeps its
    // header in place.
    auto* pins_scroll =
        column->AddChildView(std::make_unique<views::ScrollView>());
    pins_scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
    pins_scroll->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    pins_scroll->SetBackgroundColor(std::nullopt);
    pins_scroll->SetDrawOverflowIndicator(false);
    static_cast<views::BoxLayout*>(column->GetLayoutManager())
        ->SetFlexForView(pins_scroll, 1);
    auto* pins = pins_scroll->SetContents(
        Box(Orientation::kVertical, gfx::Insets::TLBR(1, 0, 0, 0), 2));
    pins->GetViewAccessibility().SetRole(ax::mojom::Role::kList);
    pins->GetViewAccessibility().SetName(u"Pinned tabs in " + name);
    for (const auto& pin : space.pins) {
      auto pin_row = std::make_unique<PinRow>(
          base::BindRepeating(
              [](LibraryView* view, uint64_t item) {
                // Activating switches to the Pin's Space and reopens a dormant
                // one, as its sidebar row does; the action holds nothing of
                // `view`.
                view->PostAction(base::BindOnce(
                    [](SidebarView::Delegate* delegate, uint64_t item,
                       const base::RepeatingClosure& close) {
                      delegate->ActivateItem(item);
                      close.Run();
                    },
                    base::Unretained(&view->delegate_.get()), item,
                    view->close_));
              },
              base::Unretained(this), pin.id),
          pin);
      keyed_views_.emplace_back(pin.id | library::kPinKey, pin_row.get());
      pins->AddChildView(std::move(pin_row));
    }
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
  // Arc insets the search field 8 DIP from the rail, the top and the page.
  auto* content = pane_->AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(8, 8, 10, 8), 8));
  layout->SetFlexForView(content, 1);
  auto* content_layout =
      static_cast<views::BoxLayout*>(content->GetLayoutManager());

  // Arc's search field: 36 DIP tall on the tile fill, a 14pt placeholder 13
  // DIP right of the magnifier.
  auto* field = content->AddChildView(
      Box(Orientation::kHorizontal, gfx::Insets::VH(0, 14), 13));
  field->SetBackground(views::CreateRoundedRectBackground(colors.tile, 10));
  field->SetPreferredSize(gfx::Size(0, 36));
  field->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(kSearchIcon, colors.muted, 16)));
  auto search = std::make_unique<views::Textfield>();
  search->SetBackgroundEnabled(false);
  search->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  search->SetFontList(
      gfx::FontList().Derive(1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  search->SetPlaceholderText(info.search);
  search->GetViewAccessibility().SetName(info.search);
  search->SetText(query_);
  search->set_controller(this);
  // Only Downloads and Archived Tabs have anything to search yet.
  search->SetEnabled(section_ == Section::kDownloads ||
                     section_ == Section::kArchive);
  search_ = field->AddChildView(std::move(search));
  static_cast<views::BoxLayout*>(field->GetLayoutManager())
      ->SetFlexForView(search_, 1);
  static_cast<views::BoxLayout*>(field->GetLayoutManager())
      ->set_cross_axis_alignment(CrossAxis::kCenter);

  list_ = content->AddChildView(Box(Orientation::kVertical, gfx::Insets(), 0));
  content_layout->SetFlexForView(list_, 1);
  downloads_.clear();
  archived_.clear();
  if (section_ == Section::kDownloads) {
    downloads_ = delegate_->GetRecentDownloads();
  } else if (section_ == Section::kArchive) {
    archived_ = delegate_->GetArchivedTabs();
  }
  UpdateList(/*keep_scroll=*/false);
}

void LibraryView::AddArchiveRows(views::View& rows) {
  const Colors colors = GetColors(GetColorProvider());
  const base::Time now = base::Time::Now();
  std::u16string day;
  for (const auto& tab : archived_) {
    if (!library::MatchesSearch(tab.title, query_) &&
        !library::MatchesSearch(tab.url, query_)) {
      continue;
    }
    // Entries come newest first, so each day's header appears once.
    if (std::u16string label = library::DayLabel(tab.archived, now);
        label != day) {
      day = std::move(label);
      auto* header = rows.AddChildView(
          MakeLabel(day,
                    gfx::FontList().Derive(0, gfx::Font::NORMAL,
                                           gfx::Font::Weight::SEMIBOLD),
                    colors.muted));
      header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      header->SetBorder(
          views::CreateEmptyBorder(gfx::Insets::TLBR(10, 6, 6, 6)));
      header->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    }
    auto row = std::make_unique<ArchiveRow>(
        base::BindRepeating(
            [](LibraryView* view, uint64_t id) {
              // A second click before the first reopen runs is ignored.
              if (!view->reopening_.insert(id).second) {
                return;
              }
              // Reopening switches Space and rebuilds the sidebar, so the
              // action holds nothing of `view`.
              view->PostAction(base::BindOnce(
                  [](SidebarView::Delegate* delegate, uint64_t id,
                     const base::RepeatingClosure& close) {
                    delegate->RestoreArchivedTab(id);
                    close.Run();
                  },
                  base::Unretained(&view->delegate_.get()), id, view->close_));
            },
            base::Unretained(this), tab.id),
        tab);
    keyed_views_.emplace_back(tab.id, row.get());
    rows.AddChildView(std::move(row));
  }
}

void LibraryView::UpdateList(bool keep_scroll) {
  if (!list_) {
    return;
  }
  // A refresh (a closed tab, a favicon) keeps the reader's place; a new query
  // or section starts at the top.
  std::optional<gfx::PointF> offset;
  if (keep_scroll && !list_->children().empty()) {
    if (const auto* scroll =
            views::AsViewClass<views::ScrollView>(list_->children().front())) {
      offset = scroll->CurrentOffset();
    }
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
  const bool any_archived =
      std::ranges::any_of(archived_, [this](const auto& tab) {
        return library::MatchesSearch(tab.title, query_) ||
               library::MatchesSearch(tab.url, query_);
      });

  if (downloads.empty() && !any_archived) {
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
        body =
            u"Hover your mouse over a tab and press the \u00D7 to archive "
            u"it.";
        if (!query_.empty()) {
          title = u"No matches";
          body = u"No archived tabs match your search.";
        }
        break;
      case Section::kMedia:
        title = u"Not available yet";
        body = u"Media from your pages will show up here in a later version.";
        break;
      case Section::kBoosts:
        title = u"Not available yet";
        body = u"Boosts are coming in a later version.";
        break;
      case Section::kSpaces:
        NOTREACHED();
    }
    auto* empty = list_->AddChildView(
        Box(Orientation::kVertical, gfx::Insets::VH(0, 16), 6));
    list_layout->SetFlexForView(empty, 1);
    auto* empty_layout =
        static_cast<views::BoxLayout*>(empty->GetLayoutManager());
    empty_layout->set_main_axis_alignment(MainAxis::kCenter);
    empty_layout->set_cross_axis_alignment(CrossAxis::kCenter);
    auto* image = empty->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(*icon, colors.empty_icon, 48)));
    image->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 12, 0));
    const SkColor on_surface =
        GetColorProvider()
            ? GetColorProvider()->GetColor(ui::kColorSysOnSurface)
            : SK_ColorWHITE;
    empty->AddChildView(
        MakeLabel(title,
                  gfx::FontList().Derive(3, gfx::Font::NORMAL,
                                         gfx::Font::Weight::SEMIBOLD),
                  on_surface));
    auto* body_label = empty->AddChildView(
        MakeLabel(body, gfx::FontList(), colors.empty_body));
    body_label->SetMultiLine(true);
    body_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    // Arc wraps its empty-state text at about 248 DIP.
    body_label->SizeToFit(248);
  } else {
    auto* scroll = list_->AddChildView(std::make_unique<views::ScrollView>());
    scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
    scroll->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    scroll->SetBackgroundColor(std::nullopt);
    scroll->SetDrawOverflowIndicator(false);
    list_layout->SetFlexForView(scroll, 1);
    if (offset) {
      scroll->RegisterPostLayoutCallback(base::BindRepeating(
          [](std::optional<gfx::PointF>& saved, views::ScrollView* scroll) {
            if (auto restore = std::exchange(saved, std::nullopt)) {
              scroll->ScrollToOffset(*restore);
            }
          },
          base::OwnedRef(std::move(offset))));
    }
    auto* rows =
        scroll->SetContents(Box(Orientation::kVertical, gfx::Insets(), 2));
    rows->GetViewAccessibility().SetRole(ax::mojom::Role::kList);
    rows->GetViewAccessibility().SetName(
        section_ == Section::kArchive ? u"Archived tabs" : u"Recent downloads");
    AddArchiveRows(*rows);
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

void LibraryView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (!GetWidget() || (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItem(kClearArchive, u"Clear Archive");
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void LibraryView::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == kClearArchive) {
    // The confirmation and the refresh it triggers run after the menu closes.
    PostAction(base::BindOnce(&SidebarView::Delegate::ClearArchive,
                              base::Unretained(&delegate_.get())));
  }
}

void LibraryView::ContentsChanged(views::Textfield* sender,
                                  const std::u16string& new_contents) {
  query_ = new_contents;
  UpdateList(/*keep_scroll=*/false);
}

BEGIN_METADATA(LibraryView)
END_METADATA

}  // namespace myarc
