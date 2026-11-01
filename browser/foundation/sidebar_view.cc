#include "myarc/sidebar_view.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/bridge.rs.h"
#include "myarc/create_menu.h"
#include "myarc/downloads_stack.h"
#include "myarc/footer_button.h"
#include "myarc/icons.h"
#include "myarc/library_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;

constexpr int kTileHeight = 46;
// Arc's row highlight is 36 DIP tall, the URL pill's height.
constexpr int kRowHeight = 36;
// The rows' size and edges; the Library replaces both while it is open.
constexpr gfx::Size kSidebarSize(230, 400);
constexpr gfx::Insets kSidebarInsets = gfx::Insets::TLBR(8, 8, 7, 8);
// Measured from Arc: the edge narrows over ~270 ms while the Library leaves
// 2.2x as fast. The rows replace it ~60% of the way and fade in over ~65 ms.
constexpr base::TimeDelta kLibraryExit = base::Milliseconds(270);
constexpr base::TimeDelta kRowsSwap = base::Milliseconds(55);
constexpr base::TimeDelta kRowsFadeIn = base::Milliseconds(65);
constexpr double kLibraryExitSpeed = 2.2;
// Arc's Space switch slides the rows for ~6 frames at 30 fps, easing out.
constexpr base::TimeDelta kSpaceSlide = base::Milliseconds(200);

// Lucide "circle" filled to the canvas edge: Arc's Space dot is 8 DIP across.
constexpr gfx::PathElement kDotPath[] = {gfx::CANVAS_DIMENSIONS, 24,
                                         gfx::CIRCLE, 12, 12, 12};
constexpr gfx::VectorIconRep kDotRep[] = {{kDotPath}};
constexpr gfx::VectorIcon kDotIcon(kDotRep, std::size(kDotRep), "myarc_dot");

struct Fills {
  SkColor tile;
  SkColor selected_tile;
  SkColor new_tab;
  SkColor current_space;
  SkColor space_dot;
  SkColor title;
};

// Arc lifts sidebar controls with white over the window material, so they read
// lighter than the sidebar. Dark values are sampled from Arc's dark sidebar;
// the current Space's glyph is 0.85 of the footer icons' muted #A9A2AF, other
// Spaces' glyphs the same white at 18%. The URL pill and header toggle chip
// pick light/dark from the same kColorSysSurface.
Fills GetFills(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  return dark ? Fills{SkColorSetA(SK_ColorWHITE, 0x18),
                      SkColorSetA(SK_ColorWHITE, 0x6F),
                      SkColorSetA(SK_ColorWHITE, 0x33),
                      GetCurrentSpaceColor(provider),
                      SkColorSetA(SK_ColorWHITE, 0x2D),
                      SkColorSetA(SK_ColorWHITE, 0x73)}
              : Fills{SkColorSetA(SK_ColorWHITE, 0x80), SK_ColorWHITE,
                      SkColorSetA(SK_ColorWHITE, 0xB3),
                      GetCurrentSpaceColor(provider),
                      SkColorSetA(SK_ColorBLACK, 0x33),
                      SkColorSetA(SK_ColorBLACK, 0x99)};
}

// A Space's footer glyph: its icon, or a dot without one, brighter for the
// current Space. Spaces store no icon yet; Arc's default planet is Space 1's.
ui::ImageModel SpaceGlyph(size_t index, bool current, const Fills& fills) {
  const SkColor color = current ? fills.current_space : fills.space_dot;
  return index == 0 ? ui::ImageModel::FromVectorIcon(kSaturnIcon, color, 16)
                    : ui::ImageModel::FromVectorIcon(kDotIcon, color, 8);
}

// Arc packs the Space glyphs left, 6 DIP after the Library button: the
// footer's 4 DIP spacing and 2 more.
constexpr int kSpacesLead = 2;

enum ItemCommand { kPin = 1, kFavorite, kUnsave, kClose };

// Opens the creation menu once, held open, for background captures.
constexpr char kCaptureCreateMenu[] = "myarc-capture-create-menu";

// The creation menu floats 17 DIP above the Create button, its notch tip 12
// DIP above it and 4 DIP left of its center, and its right edge 5 DIP inside
// the button's.
constexpr int kCreateMenuGap = 17;
constexpr int kCreateMenuRightInset = 5;
constexpr int kCreateMenuNotchShift = 4;

// The row owns its highlight and reserves the close target even when its glyph
// is hidden. Native child enter/exit and focus notifications cover both
// buttons.
class ItemRow final : public views::View, public views::FocusChangeListener {
  METADATA_HEADER(ItemRow, views::View)

 public:
  ItemRow(std::unique_ptr<views::MdTextButton> activate,
          std::unique_ptr<views::MdTextButton> close,
          bool selected)
      : selected_(selected) {
    // The composite row owns hover paint, including the close target.
    views::InkDrop::Get(activate.get())
        ->SetMode(views::InkDropHost::InkDropMode::OFF);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets(), 0));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->SetFlexForView(AddChildView(std::move(activate)), 1);
    if (close) {
      close_ = AddChildView(std::move(close));
      close_->SetImageModel(views::Button::STATE_NORMAL, ui::ImageModel());
    }
    SetNotifyEnterExitOnChild(true);
  }

  void AddedToWidget() override {
    views::View::AddedToWidget();
    focus_observation_.Observe(GetFocusManager());
    UpdateClose();
  }

  void RemovedFromWidget() override {
    focus_observation_.Reset();
    views::View::RemovedFromWidget();
  }

  void WillClearFocusManager() override {
    focus_observation_.Reset();
    views::View::WillClearFocusManager();
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    hovered_ = true;
    UpdateClose();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    hovered_ = false;
    UpdateClose();
  }

  void OnDidChangeFocus(views::View*, views::View*) override { UpdateClose(); }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    UpdateClose();
  }

 private:
  void UpdateClose() {
    auto* manager = GetFocusManager();
    const bool reveal =
        hovered_ || (manager && Contains(manager->GetFocusedView()));
    // Same translucent white as the Favorite tiles: Arc's row highlight
    // (#505060 on the New Tab row) for the selected row, the resting tile
    // fill for hover.
    const Fills fills = GetFills(GetColorProvider());
    SetBackground(selected_ || reveal
                      ? views::CreateRoundedRectBackground(
                            selected_ ? fills.new_tab : fills.tile, 10)
                      : nullptr);
    if (close_) {
      close_->SetImageModel(
          views::Button::STATE_NORMAL,
          reveal ? ui::ImageModel::FromVectorIcon(vector_icons::kCloseIcon,
                                                  ui::kColorSysOnSurface, 20)
                 : ui::ImageModel());
    }
  }

  raw_ptr<views::MdTextButton> close_ = nullptr;
  const bool selected_;
  bool hovered_ = false;
  base::ScopedObservation<views::FocusManager, views::FocusChangeListener>
      focus_observation_{this};
};

BEGIN_METADATA(ItemRow)
END_METADATA

std::unique_ptr<views::View> Box(Orientation orientation, int spacing) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(orientation, gfx::Insets(), spacing));
  return view;
}

template <typename T = views::MdTextButton>
std::unique_ptr<T> Button(views::Button::PressedCallback callback,
                          const std::u16string& text) {
  // Icons keep their explicit colors. Retinting them with the text color bakes
  // in the disabled color whenever colors update in an inactive Mac window.
  auto button = std::make_unique<T>(
      std::move(callback), text, views::style::CONTEXT_BUTTON_MD,
      /*use_text_color_for_icon=*/false);
  button->SetStyle(ui::ButtonStyle::kText);
  button->SetTextSubpixelRenderingEnabled(false);
  // Preserve the parent's material; this API is the available transparent fill.
  button->SetBgColorOverrideDeprecated(SK_ColorTRANSPARENT);
  button->SetEnabledTextColors(ui::kColorSysOnSurface);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetCornerRadius(10);
  button->SetCustomPadding(gfx::Insets::VH(6, 8));
  button->SetMinSize(gfx::Size(0, 32));
  button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  button->SetElideBehavior(gfx::ELIDE_TAIL);
  return button;
}

// A 32 DIP square footer button. Without `icon` the button draws its own art.
template <typename T = views::MdTextButton>
std::unique_ptr<T> IconButton(views::Button::PressedCallback callback,
                              const gfx::VectorIcon* icon,
                              int icon_size = 20,
                              ui::ColorVariant color = ui::kColorSysOnSurface) {
  auto button = Button<T>(std::move(callback), u"");
  button->SetMinSize(gfx::Size(32, 32));
  button->SetMaxSize(gfx::Size(32, 32));
  button->SetPreferredSize(gfx::Size(32, 32));
  button->SetCustomPadding(gfx::Insets());
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->SetImageCentered(true);
  button->SetCornerRadius(8);
  if (icon) {
    button->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*icon, color, icon_size));
  }
  return button;
}

void AddStatus(views::View& parent, const std::u16string& text) {
  auto* label = parent.AddChildView(std::make_unique<views::Label>(text));
  label->SetSubpixelRenderingEnabled(false);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(ui::kColorSysOnSurface);
}

}  // namespace

// The current Space's title and rows. During a Space switch the old Space's
// rows ride one width beside them, outside their bounds, on their own layer.
class SpaceRows final : public views::View {
  METADATA_HEADER(SpaceRows, views::View)

 public:
  SpaceRows() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kVertical,
                                                        gfx::Insets(), 4));
  }

  // `shift` is +1 to place `rows` on the right, -1 on the left.
  void SetOutgoing(std::unique_ptr<SpaceRows> rows, int shift) {
    TakeOutgoing();
    rows->SetProperty(views::kViewIgnoredByLayoutKey, true);
    rows->SetCanProcessEventsWithinSubtree(false);
    rows->GetViewAccessibility().SetIsIgnored(true);
    if (!rows->layer()) {
      rows->SetPaintToLayer();
      rows->layer()->SetFillsBoundsOpaquely(false);
    }
    rows->layer()->SetTransform(gfx::Transform());
    outgoing_shift_ = shift;
    outgoing_ = AddChildView(std::move(rows));
    InvalidateLayout();
  }

  std::unique_ptr<SpaceRows> TakeOutgoing() {
    if (!outgoing_) {
      return nullptr;
    }
    return RemoveChildViewT(std::exchange(outgoing_, nullptr).get());
  }

  void Layout(PassKey) override {
    LayoutSuperclass<views::View>(this);
    if (outgoing_) {
      outgoing_->SetBounds(outgoing_shift_ * width(), 0, width(),
                           outgoing_->GetHeightForWidth(width()));
    }
  }

 private:
  raw_ptr<SpaceRows> outgoing_ = nullptr;
  int outgoing_shift_ = 0;
};

BEGIN_METADATA(SpaceRows)
END_METADATA

SidebarView::SidebarView(Delegate& delegate)
    : views::AnimationDelegateViews(this),
      delegate_(delegate),
      footer_tooltip_(std::make_unique<FooterTooltip>()) {
  library_exit_.SetDuration(kLibraryExit);
  space_slide_.SetDuration(kSpaceSlide);
  SetPreferredSize(kSidebarSize);
  SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kVertical,
                                                      kSidebarInsets, 8));
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Workspace sidebar");
  SetUnavailable(u"Loading workspace\u2026");
}

SidebarView::~SidebarView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  menu_runner_.reset();
}

void SidebarView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetWidget() || (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  const auto target =
      std::ranges::find(focus_targets_, source, &FocusTarget::view);
  if (target == focus_targets_.end() || !target->item_menu) {
    return;
  }
  menu_item_id_ = target->id;
  menu_space_id_ = current_space_id_;
  menu_state_ = *target->item_menu;
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  if (!menu_state_.is_pin) {
    menu_model_->AddItem(kPin, menu_state_.is_favorite ? u"Move to Space Pins"
                                                       : u"Pin to Space");
  }
  if (!menu_state_.is_favorite) {
    menu_model_->AddItem(kFavorite, menu_state_.is_pin
                                        ? u"Move to Profile Favorites"
                                        : u"Add to Profile Favorites");
  }
  if (menu_state_.is_pin || menu_state_.is_favorite) {
    menu_model_->AddItem(kUnsave, menu_state_.is_pin
                                      ? u"Unpin from Space"
                                      : u"Remove from Profile Favorites");
  }
  if (menu_state_.is_live) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(
        kClose, menu_state_.is_pin        ? u"Close live page (keep Space Pin)"
                : menu_state_.is_favorite ? u"Close live page (keep Favorite)"
                                          : u"Close live page");
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  // Use the stable widget, never the replaceable row or its button controller.
  // RunMenuAt can enter a native nested loop and even destroy this sidebar.
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void SidebarView::OpenLibrary(LibrarySection section) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (library_exit_.is_animating()) {
    // Reopening while the Library leaves turns it back, keeping its section
    // state and scroll.
    library_exit_.Stop();
    if (library_) {
      delegate_->SetLibraryOpen(true);
      library_->SelectSection(section);
      InvalidateLayout();
      return;
    }
  }
  if (library_) {
    library_->SelectSection(section);
    return;
  }
  if (!current_space_id_) {
    return;
  }
  // Keyboard users keep focus; a click leaves no focus ring, as in Arc.
  const bool had_focus = std::ranges::any_of(
      focus_targets_,
      [](const FocusTarget& target) { return target.view->HasFocus(); });
  menu_item_id_ = 0;
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
  space_slide_.Stop();
  focus_targets_.clear();
  footer_targets_.clear();
  footer_ = nullptr;
  downloads_stack_ = nullptr;
  create_menu_ = nullptr;
  create_button_ = nullptr;
  create_menu_open_ = false;
  spaces_scroll_ = nullptr;
  new_tab_ = nullptr;
  space_rows_ = nullptr;
  tiles_.clear();
  RemoveAllChildViews();
  // The Library sets its own width and edges, as Arc swaps out its sidebar.
  auto* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  layout->set_inside_border_insets(gfx::Insets());
  SetPreferredSize(std::nullopt);
  // The Library posts every close, so its handlers have unwound.
  library_ = AddChildView(std::make_unique<LibraryView>(
      *delegate_,
      base::BindRepeating(&SidebarView::CloseLibrary,
                          weak_factory_.GetWeakPtr()),
      section, footer_tooltip_->GetWeakPtr()));
  layout->SetFlexForView(library_, 1);
  library_->SetSpaces(spaces_, current_space_id_);
  delegate_->SetLibraryOpen(true);
  if (had_focus) {
    library_->FocusSection();
  }
}

void SidebarView::ResetLibrary() {
  if (!library_) {
    return;
  }
  views::View* library = std::exchange(library_, nullptr);
  RemoveChildViewT(library);
  static_cast<views::BoxLayout*>(GetLayoutManager())
      ->set_inside_border_insets(kSidebarInsets);
  SetPreferredSize(kSidebarSize);
  // SetModel rebuilds the rows from scratch.
  current_space_id_ = 0;
}

void SidebarView::CloseLibrary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!library_ || library_exit_.is_animating()) {
    return;
  }
  // A hidden or zero-width sidebar has nothing to animate.
  if (gfx::Animation::PrefersReducedMotion() || !parent() || !IsDrawn() ||
      parent()->width() <= 0) {
    FinishCloseLibrary(LibraryHasFocus());
    return;
  }
  // The parent eases back to the rows' width while Layout slides the Library
  // out. SetModel keeps it until AnimationProgressed swaps in the rows.
  library_exit_.Start();
  delegate_->SetLibraryOpen(false);
}

bool SidebarView::LibraryHasFocus() {
  // Keyboard users land on the Library button; a click leaves no focus ring.
  return library_ && GetFocusManager() &&
         library_->Contains(GetFocusManager()->GetFocusedView());
}

void SidebarView::FinishCloseLibrary(bool had_focus) {
  ResetLibrary();
  library_closed_ = true;
  auto weak = weak_factory_.GetWeakPtr();
  // After an animated close this repeats only to supply the rows' model.
  delegate_->SetLibraryOpen(false);
  if (!weak || !had_focus) {
    return;
  }
  const auto button = std::ranges::find(focus_targets_, FocusAction::kLibrary,
                                        &FocusTarget::action);
  if (button != focus_targets_.end()) {
    button->view->RequestFocus();
  }
}

void SidebarView::RefreshLibraryDownloads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (library_) {
    library_->RefreshDownloads();
  }
}

void SidebarView::SetCreateMenuOpen(bool open) {
  create_menu_open_ = open;
  if (!create_button_) {
    return;
  }
  create_button_->SetFooterTooltip(
      footer_tooltip_->GetWeakPtr(),
      {open ? u"Close creation menu" : u"Create Space", {}});
  create_button_->SetOpen(open);
  if (open) {
    create_button_->GetViewAccessibility().SetIsExpanded();
  } else {
    create_button_->GetViewAccessibility().SetIsCollapsed();
  }
}

void SidebarView::ShowCreateMenu(const ui::Event& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!create_menu_ || !create_button_) {
    return;
  }
  if (create_menu_->is_open()) {
    create_menu_->Close();
    return;
  }
  menu_item_id_ = 0;
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
  create_menu_->Open(*create_button_);
}

void SidebarView::ChooseCreateItem(int item) {
  // A choice can open a dialog or the palette and rebuild the footer, so it
  // runs after the menu's handlers return.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SidebarView> sidebar, int item) {
                       if (!sidebar) {
                         return;
                       }
                       switch (static_cast<CreateMenu::Item>(item)) {
                         case CreateMenu::Item::kNewSpace:
                           sidebar->delegate_->CreateSpace();
                           return;
                         case CreateMenu::Item::kNewTab:
                           sidebar->delegate_->NewTab();
                           return;
                         case CreateMenu::Item::kNewSplit:
                           return;
                       }
                     },
                     weak_factory_.GetWeakPtr(), item));
}

void SidebarView::ExecuteCommand(int command_id, int event_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint64_t id = std::exchange(menu_item_id_, 0);
  if (!id) {
    return;
  }
  // Each call can synchronously rebuild or destroy the view. Touch no members
  // after dispatch. Only the parent/Rust can validate and acknowledge success.
  switch (command_id) {
    case kPin:
      delegate_->PinItem(id);
      return;
    case kFavorite:
      delegate_->FavoriteItem(id);
      return;
    case kUnsave:
      delegate_->UnsaveItem(id);
      return;
    case kClose:
      delegate_->CloseLiveItem(id);
      return;
  }
}

void SidebarView::TrackFocus(views::View* view,
                             FocusAction action,
                             uint64_t id,
                             std::optional<ItemMenuState> item_menu) {
  focus_targets_.push_back({action, id, view, item_menu});
  if (item_menu) {
    view->set_context_menu_controller(this);
    view->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  }
}

std::unique_ptr<views::View> SidebarView::MakeItem(
    const foundation::ItemRecord& item,
    bool tile,
    bool selected) {
  ItemPresentation presentation = delegate_->GetItemPresentation(item);
  // Dormant saved rows show the last title their page had, as Arc does, and
  // fall back to the saved URL until a page has loaded once.
  if (presentation.title.empty() && !item.title.empty()) {
    presentation.title = base::UTF8ToUTF16(std::string(item.title));
  }
  if (presentation.title.empty()) {
    presentation.title =
        item.has_url ? base::UTF8ToUTF16(std::string(item.canonical_url))
                     : u"Untitled tab";
  }
  const auto foreground = ui::kColorSysOnSurface;
  auto activate =
      Button(base::BindRepeating(&Delegate::ActivateItem,
                                 base::Unretained(&delegate_.get()), item.id),
             tile ? std::u16string() : presentation.title);
  if (presentation.icon.IsEmpty()) {
    presentation.icon = ui::ImageModel::FromVectorIcon(
        vector_icons::kGlobeIcon, foreground, tile ? 20 : 16);
  }
  activate->SetImageModel(views::Button::STATE_NORMAL, presentation.icon);
  activate->SetImageLabelSpacing(8);
  activate->SetTooltipText(presentation.title);
  activate->GetViewAccessibility().SetName(presentation.title);
  activate->GetViewAccessibility().SetDescription(
      presentation.is_live ? u"Open tab" : u"Saved site");
  activate->GetViewAccessibility().SetIsSelected(selected);
  activate->SetEnabledTextColors(foreground);
  if (tile) {
    tiles_.emplace_back(activate.get(), selected);
  }
  const ItemMenuState menu_state = {
      item.placement == foundation::PlacementKind::Pin,
      item.placement == foundation::PlacementKind::Favorite,
      presentation.is_live};
  TrackFocus(activate.get(), FocusAction::kActivate, item.id, menu_state);
  if (tile) {
    activate->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    activate->SetImageCentered(true);
    activate->SetMinSize(gfx::Size(0, kTileHeight));
    activate->SetPreferredSize(gfx::Size(0, kTileHeight));
    return activate;
  }
  activate->SetMinSize(gfx::Size(0, kRowHeight));

  std::unique_ptr<views::MdTextButton> close;
  if (presentation.is_live) {
    close = IconButton(
        base::BindRepeating(&Delegate::CloseLiveItem,
                            base::Unretained(&delegate_.get()), item.id),
        &vector_icons::kCloseIcon);
    views::InstallRoundRectHighlightPathGenerator(close.get(), gfx::Insets(4),
                                                  6);
    const std::u16string close_name =
        (menu_state.is_pin        ? u"Close live page, keep Space Pin: "
         : menu_state.is_favorite ? u"Close live page, keep Favorite: "
                                  : u"Close live page: ") +
        presentation.title;
    close->SetTooltipText(close_name);
    close->GetViewAccessibility().SetName(close_name);
    TrackFocus(close.get(), FocusAction::kClose, item.id, menu_state);
  }
  return std::make_unique<ItemRow>(std::move(activate), std::move(close),
                                   selected);
}

views::View* SidebarView::AddStorageRetry(views::View& parent,
                                          StorageRetry retry) {
  auto button =
      Button(base::BindRepeating(&Delegate::RetryWorkspaceStorage,
                                 base::Unretained(&delegate_.get()), retry),
             retry == StorageRetry::kLoad ? u"Retry loading" : u"Retry saving");
  TrackFocus(button.get(), retry == StorageRetry::kLoad
                               ? FocusAction::kRetryLoad
                               : FocusAction::kRetrySave);
  return parent.AddChildView(std::move(button));
}

void SidebarView::SetUnavailable(const std::u16string& reason,
                                 StorageRetry retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool restore_focus = std::ranges::any_of(
      focus_targets_,
      [](const FocusTarget& target) { return target.view->HasFocus(); });
  menu_item_id_ = 0;
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
  library_exit_.Stop();
  space_slide_.Stop();
  ResetLibrary();
  library_closed_ = false;
  focus_targets_.clear();
  footer_targets_.clear();
  footer_ = nullptr;
  downloads_stack_ = nullptr;
  create_menu_ = nullptr;
  create_button_ = nullptr;
  create_menu_open_ = false;
  spaces_scroll_ = nullptr;
  new_tab_ = nullptr;
  space_rows_ = nullptr;
  tiles_.clear();
  current_space_id_ = 0;
  spaces_.clear();
  RemoveAllChildViews();
  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetDrawOverflowIndicator(false);
  scroll->SetFocusBehavior(FocusBehavior::ALWAYS);
  scroll->GetViewAccessibility().SetName(u"Workspace status");
  TrackFocus(scroll, FocusAction::kStatus);
  static_cast<views::BoxLayout*>(GetLayoutManager())->SetFlexForView(scroll, 1);
  auto* body = scroll->SetContents(Box(Orientation::kVertical, 4));
  AddStatus(*body, reason.empty() ? u"Workspace unavailable" : reason);
  // Keep the action visible while a long error scrolls independently.
  auto* focus =
      retry == StorageRetry::kLoad ? AddStorageRetry(*this, retry) : scroll;
  if (restore_focus) {
    focus->RequestFocus();
  }
}

void SidebarView::SetModel(const foundation::SidebarResult& model,
                           const std::u16string& status,
                           StorageRetry retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model.ok) {
    SetUnavailable(u"Workspace unavailable: " +
                   base::UTF8ToUTF16(std::string(model.error)));
    return;
  }
  spaces_.clear();
  for (const auto& space : model.spaces) {
    spaces_.emplace_back(space.id, base::UTF8ToUTF16(std::string(space.name)));
  }
  if (library_) {
    // The Library shows Spaces and the archive; rows rebuild when it closes.
    current_space_id_ = model.current_space.id;
    library_->SetSpaces(spaces_, current_space_id_);
    // Closing a tab or a favicon lookup changes the archive.
    library_->RefreshArchive();
    return;
  }

  std::optional<std::pair<FocusAction, uint64_t>> focus;
  for (const auto& target : focus_targets_) {
    if (target.view->HasFocus()) {
      focus = {target.action, target.id};
      break;
    }
  }
  gfx::PointF offset;
  if (!children().empty()) {
    // A usable model always has its ScrollView first.
    if (current_space_id_) {
      offset =
          static_cast<views::ScrollView*>(children().front())->CurrentOffset();
    }
  }
  const gfx::PointF spaces_offset =
      spaces_scroll_ ? spaces_scroll_->CurrentOffset() : gfx::PointF();
  const bool space_changed = current_space_id_ != model.current_space.id;
  // A Space switch slides the old title and rows out as the new ones come
  // in. Row rebuilds mid-slide carry the old rows over.
  std::unique_ptr<SpaceRows> outgoing;
  bool slide = false;
  if (space_rows_ && space_changed) {
    const auto index_of = [this](uint64_t id) {
      return std::ranges::find(spaces_, id,
                               &std::pair<uint64_t, std::u16string>::first) -
             spaces_.begin();
    };
    const auto from = index_of(current_space_id_);
    const auto to = index_of(model.current_space.id);
    space_slide_.Stop();
    if (!gfx::Animation::PrefersReducedMotion() && IsDrawn() &&
        from < std::ssize(spaces_) && to < std::ssize(spaces_)) {
      // A Space to the left enters from the left, pushing the old one right.
      space_slide_shift_ = to < from ? 1 : -1;
      outgoing = space_rows_->parent()->RemoveChildViewT(space_rows_.get());
      slide = true;
    }
  } else if (space_rows_ && space_slide_.is_animating()) {
    outgoing = space_rows_->TakeOutgoing();
  }
  space_rows_ = nullptr;
  current_space_id_ = model.current_space.id;
  focus_targets_.clear();
  new_tab_ = nullptr;
  tiles_.clear();
  // Loading and title ticks rebuild only the rows. The footer stays, with its
  // hover, tooltip and Spaces scroll, until the Spaces or the current one
  // change.
  const bool keep_footer = footer_ && footer_spaces_ == spaces_ &&
                           footer_space_id_ == current_space_id_;
  if (keep_footer) {
    while (children().front() != footer_) {
      RemoveChildViewT(children().front());
    }
  } else {
    footer_targets_.clear();
    footer_ = nullptr;
    downloads_stack_ = nullptr;
    create_menu_ = nullptr;
    create_button_ = nullptr;
    create_menu_open_ = false;
    spaces_scroll_ = nullptr;
    RemoveAllChildViews();
  }

  auto* scroll = AddChildViewAt(std::make_unique<views::ScrollView>(), 0);
  // Bounded mode sizes contents to viewport width and full preferred height.
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetBackgroundColor(
      std::nullopt);  // Preserve parent's native material.
  scroll->SetDrawOverflowIndicator(false);
  static_cast<views::BoxLayout*>(GetLayoutManager())->SetFlexForView(scroll, 1);
  auto* body = scroll->SetContents(Box(Orientation::kVertical, 4));
  if (retry == StorageRetry::kSave) {
    AddStorageRetry(*body, retry);
  }
  if (!status.empty()) {
    AddStatus(*body, status);
  }

  views::View* favorites_row = nullptr;
  for (size_t i = 0; i < model.favorites.size(); ++i) {
    if (i % 3 == 0) {
      favorites_row = body->AddChildView(Box(Orientation::kHorizontal, 8));
    }
    auto* tile = favorites_row->AddChildView(
        MakeItem(model.favorites[i], true,
                 model.favorites[i].id == model.selected_item_id));
    static_cast<views::BoxLayout*>(favorites_row->GetLayoutManager())
        ->SetFlexForView(tile, 1);
  }
  // Empty layout cells keep the final row at three columns; never controls.
  for (size_t i = model.favorites.size(); favorites_row && i % 3 != 0; ++i) {
    auto* cell = favorites_row->AddChildView(std::make_unique<views::View>());
    cell->SetPreferredSize(gfx::Size(0, kTileHeight));
    static_cast<views::BoxLayout*>(favorites_row->GetLayoutManager())
        ->SetFlexForView(cell, 1);
  }

  auto* rows = body->AddChildView(std::make_unique<SpaceRows>());
  space_rows_ = rows;
  const std::u16string name =
      base::UTF8ToUTF16(std::string(model.current_space.name));
  auto heading = Button(base::BindRepeating(&Delegate::RenameSpace,
                                            base::Unretained(&delegate_.get()),
                                            model.current_space.id),
                        name);
  // Arc's Space title is 13pt semibold in white at 45% (#888892 over its dark
  // sidebar), its baseline 31pt below the Favorites tiles.
  // STYLE_EMPHASIZED is not allowed for CONTEXT_BUTTON_MD and LabelButton hides
  // its label, so reweight the button's Label child directly.
  for (views::View* child : heading->children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(
          label->font_list().DeriveWithWeight(gfx::Font::Weight::SEMIBOLD));
    }
  }
  heading->SetEnabledTextColors(GetFills(GetColorProvider()).title);
  if (!model.favorites.empty()) {
    heading->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(6, 0, 0, 0));
  }
  heading->SetTooltipText(u"Rename Space: " + name);
  heading->GetViewAccessibility().SetName(u"Rename Space: " + name);
  TrackFocus(heading.get(), FocusAction::kRename, model.current_space.id);
  rows->AddChildView(std::move(heading));

  for (const auto& item : model.pins) {
    rows->AddChildView(
        MakeItem(item, false, item.id == model.selected_item_id));
  }
  if (!model.pins.empty()) {
    rows->AddChildView(std::make_unique<views::Separator>());
  }

  auto new_tab = Button(base::BindRepeating(&Delegate::NewTab,
                                            base::Unretained(&delegate_.get())),
                        u"New Tab");
  new_tab->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kAdd2Icon,
                                     ui::kColorSysOnSurface, 16));
  new_tab->SetMinSize(gfx::Size(0, kRowHeight));
  TrackFocus(new_tab.get(), FocusAction::kNewTab);
  // Arc keeps 41 DIP between the Space title and New Tab row centers.
  new_tab->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(3, 0, 0, 0));
  new_tab_ = rows->AddChildView(std::move(new_tab));
  for (const auto& item : model.temporary) {
    rows->AddChildView(
        MakeItem(item, false, item.id == model.selected_item_id));
  }
  if (outgoing) {
    outgoing->TakeOutgoing();
    rows->SetOutgoing(std::move(outgoing), space_slide_shift_);
    if (slide) {
      space_slide_.Start();
    }
    UpdateSpaceSlide(false);
  }

  // Loading/title ticks rebuild rows, but leave a semantically unchanged menu
  // running. Cancel only if its item/state or Space is no longer available.
  if (menu_item_id_ && menu_runner_ && menu_runner_->IsRunning() &&
      (menu_space_id_ != current_space_id_ ||
       !std::ranges::any_of(focus_targets_, [this](const FocusTarget& target) {
         return target.id == menu_item_id_ && target.item_menu == menu_state_;
       }))) {
    menu_item_id_ = 0;
    menu_runner_->Cancel();
  }

  if (keep_footer) {
    focus_targets_.insert(focus_targets_.end(), footer_targets_.begin(),
                          footer_targets_.end());
  } else {
    const size_t first_footer_target = focus_targets_.size();
    BuildFooter(spaces_offset, space_changed);
    footer_targets_.assign(focus_targets_.begin() + first_footer_target,
                           focus_targets_.end());
  }
  UpdateFills();

  scroll->RegisterPostLayoutCallback(base::BindRepeating(
      [](std::optional<gfx::PointF>& offset, views::ScrollView* scroll) {
        if (auto saved = std::exchange(offset, std::nullopt)) {
          scroll->ScrollToOffset(*saved);
        }
      },
      base::OwnedRef(std::optional(offset))));
  if (focus) {
    const auto target = std::ranges::find_if(
        focus_targets_, [&focus](const FocusTarget& target) {
          return target.action == focus->first && target.id == focus->second;
        });
    const auto current_space =
        std::ranges::find_if(focus_targets_, [this](const FocusTarget& target) {
          return target.action == FocusAction::kSpaces &&
                 target.id == current_space_id_;
        });
    views::View* restore = target != focus_targets_.end() ? target->view.get()
                           : current_space != focus_targets_.end()
                               ? current_space->view.get()
                               : spaces_scroll_->contents();
    restore->RequestFocus();
  }
}

void SidebarView::BuildFooter(const gfx::PointF& spaces_offset,
                              bool space_changed) {
  footer_spaces_ = spaces_;
  footer_space_id_ = current_space_id_;
  auto* footer = AddChildView(Box(Orientation::kHorizontal, 4));
  // Arc's footer controls sit 2 DIP higher than the sidebar's bottom inset.
  footer->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 2, 0));
  static_cast<views::BoxLayout*>(footer->GetLayoutManager())
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  auto library = IconButton<LibraryButton>(
      base::BindRepeating(
          [](base::WeakPtr<SidebarView> sidebar) {
            // Arc drops the downloads stack with the view swap, unanimated.
            if (sidebar && sidebar->downloads_stack_) {
              sidebar->downloads_stack_->HideNow();
            }
            // Opening removes this button; let its press handler unwind. A
            // second click queued meanwhile must not reset the section.
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(
                               [](base::WeakPtr<SidebarView> sidebar) {
                                 if (sidebar && !sidebar->library_) {
                                   sidebar->OpenLibrary();
                                 }
                               },
                               sidebar));
          },
          weak_factory_.GetWeakPtr()),
      nullptr);
  // Arc's Shift-Cmd-L would clash with Chromium's View > Collapse Tabs, so the
  // tooltip shows no shortcut.
  library->SetFooterTooltip(footer_tooltip_->GetWeakPtr(),
                            {u"View Library", {}, /*beside=*/true});
  library->SetHoverChangedCallback(base::BindRepeating(
      [](base::WeakPtr<SidebarView> sidebar, bool hovered) {
        if (sidebar && sidebar->downloads_stack_) {
          sidebar->downloads_stack_->SetAnchorHovered(hovered);
        }
      },
      weak_factory_.GetWeakPtr()));
  library->GetViewAccessibility().SetName(u"View Library");
  TrackFocus(library.get(), FocusAction::kLibrary);
  // Right after the Library closes, its glow settles back into the box.
  if (std::exchange(library_closed_, false)) {
    library->SettleFromGlow();
  }
  footer->AddChildView(std::move(library));
  spaces_scroll_ = footer->AddChildView(std::make_unique<views::ScrollView>());
  spaces_scroll_->SetBackgroundColor(std::nullopt);
  spaces_scroll_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  spaces_scroll_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  spaces_scroll_->SetTreatAllScrollEventsAsHorizontal(true);
  spaces_scroll_->SetUseContentsPreferredSize(true);
  // Keep all footer controls on one 32-DIP line while retaining wheel/focus
  // scrolling and the current-Space reveal callback.
  spaces_scroll_->SetPreferredSize(gfx::Size(0, 32));
  // Like Arc, Space glyphs follow the Library and Create sits at the right.
  auto* spaces = spaces_scroll_->SetContents(Box(Orientation::kHorizontal, 0));
  auto* spaces_layout =
      static_cast<views::BoxLayout*>(spaces->GetLayoutManager());
  spaces_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  spaces_layout->set_inside_border_insets(
      gfx::Insets::TLBR(0, kSpacesLead, 0, 0));
  spaces->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  spaces->GetViewAccessibility().SetName(u"Spaces");
  std::optional<size_t> current_space_index;
  for (size_t index = 0; index < spaces_.size(); ++index) {
    const auto& [space_id, space_name] = spaces_[index];
    const bool current = space_id == current_space_id_;
    // Button presses request a switch; only a Rust snapshot changes selection.
    auto choice = IconButton<FooterButton>(
        base::BindRepeating(&Delegate::SelectSpace,
                            base::Unretained(&delegate_.get()), space_id),
        nullptr);
    choice->SetImageModel(
        views::Button::STATE_NORMAL,
        SpaceGlyph(index, current, GetFills(GetColorProvider())));
    // Arc spaces its Space dots 34 DIP apart.
    choice->SetMinSize(gfx::Size(34, 32));
    choice->SetMaxSize(gfx::Size(34, 32));
    choice->SetPreferredSize(gfx::Size(34, 32));
    if (current) {
      current_space_index = spaces->children().size();
    }
    // Ctrl-1 to Ctrl-8 select those Spaces and Ctrl-9 the last one.
    FooterTooltip::Content tooltip = {space_name, {}};
    if (index < 8) {
      tooltip.keys = {u"\u2303", base::NumberToString16(index + 1)};
    } else if (index + 1 == spaces_.size()) {
      tooltip.keys = {u"\u2303", u"9"};
    }
    choice->SetFooterTooltip(footer_tooltip_->GetWeakPtr(), std::move(tooltip));
    choice->GetViewAccessibility().SetName(u"Space: " + space_name);
    choice->GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    choice->GetViewAccessibility().SetCheckedState(
        current ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse);
    TrackFocus(choice.get(), FocusAction::kSpaces, space_id);
    spaces->AddChildView(std::move(choice));
  }
  // Non-layer ScrollViews need an explicit contents size too.
  spaces->SizeToPreferredSize();
  static_cast<views::BoxLayout*>(footer->GetLayoutManager())
      ->SetFlexForView(spaces_scroll_, 1);
  spaces_scroll_->RegisterPostLayoutCallback(base::BindRepeating(
      [](std::optional<gfx::PointF>& offset, bool reveal_current,
         std::optional<size_t> current, views::ScrollView* scroll) {
        if (auto saved = std::exchange(offset, std::nullopt)) {
          scroll->ScrollToOffset(*saved);
          if (reveal_current && current) {
            scroll->contents()->children()[*current]->ScrollViewToVisible();
          }
        }
      },
      base::OwnedRef(std::optional(spaces_offset)), space_changed,
      current_space_index));

  // The button draws its plus and turns it into a cross with the menu.
  auto create = IconButton<CreateButton>(
      base::BindRepeating(&SidebarView::ShowCreateMenu,
                          weak_factory_.GetWeakPtr()),
      nullptr);
  // No shortcut opens the creation menu. SetCreateMenuOpen sets the tooltip.
  create->GetViewAccessibility().SetName(u"Create Space");
  create->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  TrackFocus(create.get(), FocusAction::kCreate);
  create_button_ = footer->AddChildView(std::move(create));
  footer_ = footer;
  // Last, so they paint over the rows.
  downloads_stack_ = AddChildView(std::make_unique<DownloadsStack>(*delegate_));
  downloads_stack_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  create_menu_ = AddChildView(std::make_unique<CreateMenu>(
      base::BindRepeating(
          [](base::WeakPtr<SidebarView> sidebar, CreateMenu::Item item) {
            if (sidebar) {
              sidebar->ChooseCreateItem(static_cast<int>(item));
            }
          },
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SidebarView::SetCreateMenuOpen,
                          weak_factory_.GetWeakPtr())));
  create_menu_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  create_menu_open_ = false;
  if (!capture_menu_scheduled_ &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(kCaptureCreateMenu)) {
    capture_menu_scheduled_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<SidebarView> sidebar) {
              if (sidebar && sidebar->create_menu_ && sidebar->create_button_) {
                sidebar->create_menu_->Open(*sidebar->create_button_,
                                            /*hold=*/true);
              }
            },
            weak_factory_.GetWeakPtr()),
        base::Milliseconds(2000));
  }
}

std::u16string SidebarView::GetCreateTooltipForTesting() const {
  const auto target = std::ranges::find(focus_targets_, FocusAction::kCreate,
                                        &FocusTarget::action);
  return target == focus_targets_.end()
             ? std::u16string()
             : static_cast<FooterButton*>(target->view.get())
                   ->footer_tooltip()
                   .label;
}

void SidebarView::ShowFooterTooltipForCapture(std::string_view target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // An open Library replaces the footer; its Exit Library tile stands in once
  // the Library's opening motion has settled.
  if (library_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<SidebarView> sidebar) {
              if (sidebar && sidebar->library_) {
                sidebar->library_->ShowExitTooltipNow();
              }
            },
            weak_factory_.GetWeakPtr()),
        base::Milliseconds(500));
    return;
  }
  const FocusAction action = target == "space"    ? FocusAction::kSpaces
                             : target == "create" ? FocusAction::kCreate
                                                  : FocusAction::kLibrary;
  for (const auto& focus : focus_targets_) {
    if (focus.action == action &&
        (action != FocusAction::kSpaces || focus.id == current_space_id_)) {
      if (auto* button = views::AsViewClass<FooterButton>(focus.view.get())) {
        button->ShowFooterTooltipNow();
      }
      return;
    }
  }
}

void SidebarView::RefreshDownloadsStack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (downloads_stack_) {
    downloads_stack_->Refresh();
  }
}

void SidebarView::ShowDownloadsStackForCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (downloads_stack_) {
    downloads_stack_->ShowNow();
  }
}

void SidebarView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  if (library_ && library_exit_.is_animating() && parent()) {
    // The Library keeps its width and leads the parent's narrowing edge.
    const int library_width = library_->GetPreferredSize().width();
    const int closed = std::max(0, library_width - parent()->width());
    library_->SetBounds(-base::ClampRound(kLibraryExitSpeed * closed), 0,
                        library_width, height());
  }
  if (downloads_stack_ && footer_) {
    // Bottom on the footer's top, across the sidebar's inner width. A short
    // window clips the oldest rows rather than placing the stack above it.
    const int height =
        std::clamp(downloads_stack_->GetPreferredSize().height(), 0,
                   std::max(footer_->y(), 0));
    downloads_stack_->SetBounds(kSidebarInsets.left(), footer_->y() - height,
                                width() - kSidebarInsets.width(), height);
  }
  if (create_menu_ && footer_) {
    // The Create button is the footer's last control, at its right end.
    const int button_right = width() - kSidebarInsets.right();
    const int button_center = button_right - 16;
    create_menu_->Place(button_right - kCreateMenuRightInset,
                        footer_->y() - kCreateMenuGap,
                        button_center - kCreateMenuNotchShift);
  }
}

void SidebarView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &space_slide_) {
    UpdateSpaceSlide(false);
    return;
  }
  const base::TimeDelta elapsed =
      kLibraryExit * library_exit_.GetCurrentValue();
  if (library_) {
    if (elapsed < kRowsSwap) {
      InvalidateLayout();
      return;
    }
    // The rows rebuild under a transparent layer and fade in; until the
    // layer goes they take no clicks.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(0);
    SetCanProcessEventsWithinSubtree(false);
    auto weak = weak_factory_.GetWeakPtr();
    FinishCloseLibrary(LibraryHasFocus());
    if (!weak) {
      return;
    }
  }
  if (layer()) {
    layer()->SetOpacity(
        std::clamp((elapsed - kRowsSwap) / kRowsFadeIn, 0.0, 1.0));
  }
}

void SidebarView::AnimationEnded(const gfx::Animation* animation) {
  if (animation == &space_slide_) {
    UpdateSpaceSlide(true);
    return;
  }
  if (library_) {
    auto weak = weak_factory_.GetWeakPtr();
    FinishCloseLibrary(LibraryHasFocus());
    if (!weak) {
      return;
    }
  }
  AnimationCanceled(animation);
}

void SidebarView::AnimationCanceled(const gfx::Animation* animation) {
  if (animation == &space_slide_) {
    UpdateSpaceSlide(true);
    return;
  }
  if (layer()) {
    DestroyLayer();
  }
  SetCanProcessEventsWithinSubtree(true);
}

void SidebarView::UpdateSpaceSlide(bool done) {
  if (!space_rows_) {
    return;
  }
  if (done) {
    space_rows_->TakeOutgoing();
    if (space_rows_->layer()) {
      space_rows_->DestroyLayer();
    }
    return;
  }
  if (!space_rows_->layer()) {
    space_rows_->SetPaintToLayer();
    space_rows_->layer()->SetFillsBoundsOpaquely(false);
  }
  // The rows may not be laid out yet; the sidebar's inner width is theirs.
  const double left = 1.0 - gfx::Tween::CalculateValue(
                                gfx::Tween::EASE_OUT,
                                space_slide_.GetCurrentValue());
  gfx::Transform transform;
  transform.Translate(
      -space_slide_shift_ * (width() - kSidebarInsets.width()) * left, 0);
  space_rows_->layer()->SetTransform(transform);
}

void SidebarView::SetNewTabIntent(bool intent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  new_tab_intent_ = intent;
  UpdateFills();
}

void SidebarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateFills();
}

void SidebarView::UpdateFills() {
  const Fills fills = GetFills(GetColorProvider());
  // With Increase Contrast on, the selected tile also gets a 1px outline.
  const bool more_contrast = GetNativeTheme()->preferred_contrast() ==
                             ui::NativeTheme::PreferredContrast::kMore;
  for (const auto& [tile, selected] : tiles_) {
    tile->SetBgColorOverrideDeprecated(selected ? fills.selected_tile
                                                : fills.tile);
    tile->SetStrokeColorIdOverride(selected && more_contrast
                                       ? std::optional(ui::kColorSysOutline)
                                       : std::nullopt);
  }
  if (new_tab_) {
    new_tab_->SetBgColorOverrideDeprecated(
        new_tab_intent_ ? fills.new_tab : SK_ColorTRANSPARENT);
    // Arc dims the idle New Tab row like the Space title and brightens it
    // under the pointer or while a new tab is pending.
    const ui::ColorVariant idle =
        new_tab_intent_ ? ui::ColorVariant(ui::kColorSysOnSurface)
                        : ui::ColorVariant(fills.title);
    for (const auto state : {views::Button::STATE_NORMAL,
                             views::Button::STATE_HOVERED,
                             views::Button::STATE_PRESSED}) {
      const ui::ColorVariant color = state == views::Button::STATE_NORMAL
                                         ? idle
                                         : ui::kColorSysOnSurface;
      new_tab_->SetTextColor(state, color);
      new_tab_->SetImageModel(
          state, ui::ImageModel::FromVectorIcon(vector_icons::kAdd2Icon, color,
                                                16));
    }
  }
  const auto rename = std::ranges::find(focus_targets_, FocusAction::kRename,
                                        &FocusTarget::action);
  if (rename != focus_targets_.end()) {
    static_cast<views::MdTextButton*>(rename->view.get())
        ->SetEnabledTextColors(fills.title);
  }
  for (const auto& target : focus_targets_) {
    if (target.action == FocusAction::kSpaces) {
      const auto space = std::ranges::find(spaces_, target.id,
                                           [](const auto& entry) {
                                             return entry.first;
                                           });
      static_cast<views::MdTextButton*>(target.view.get())
          ->SetImageModel(
              views::Button::STATE_NORMAL,
              SpaceGlyph(static_cast<size_t>(space - spaces_.begin()),
                         target.id == current_space_id_, fills));
    }
  }
  SetCreateMenuOpen(create_menu_open_);
}

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace myarc
