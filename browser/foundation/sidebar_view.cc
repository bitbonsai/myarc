#include "myarc/sidebar_view.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/bridge.rs.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
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

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;

constexpr int kTileHeight = 46;

// Footer icons from Lucide (ISC, lucide-static 1.61.0): 24 DIP canvas,
// stroke 2. "archive" stands in for Arc's Library box, "plus" for Create.
constexpr gfx::PathElement kArchivePath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::ROUND_RECT, 2, 3, 20, 5, 1,
    gfx::NEW_PATH, gfx::STROKE, 2, gfx::MOVE_TO, 4, 8, gfx::V_LINE_TO, 19,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, 2, gfx::H_LINE_TO, 18,
    gfx::R_ARC_TO, 2, 2, 0, 0, 0, 2, -2, gfx::V_LINE_TO, 8,
    gfx::NEW_PATH, gfx::STROKE, 2, gfx::MOVE_TO, 10, 12, gfx::R_H_LINE_TO, 4};
constexpr gfx::VectorIconRep kArchiveRep[] = {{kArchivePath}};
constexpr gfx::VectorIcon kArchiveIcon(kArchiveRep,
                                       std::size(kArchiveRep),
                                       "myarc_archive");

constexpr gfx::PathElement kPlusPath[] = {
    gfx::CANVAS_DIMENSIONS, 24, gfx::STROKE, 2, gfx::MOVE_TO, 5, 12,
    gfx::H_LINE_TO, 19, gfx::NEW_PATH, gfx::STROKE, 2, gfx::MOVE_TO, 12, 5,
    gfx::V_LINE_TO, 19};
constexpr gfx::VectorIconRep kPlusRep[] = {{kPlusPath}};
constexpr gfx::VectorIcon kPlusIcon(kPlusRep, std::size(kPlusRep), "myarc_plus");

// Lucide "circle" filled: Arc marks every Space with a dot.
constexpr gfx::PathElement kDotPath[] = {gfx::CANVAS_DIMENSIONS, 24,
                                         gfx::CIRCLE, 12, 12, 10};
constexpr gfx::VectorIconRep kDotRep[] = {{kDotPath}};
constexpr gfx::VectorIcon kDotIcon(kDotRep, std::size(kDotRep), "myarc_dot");

struct Fills {
  SkColor tile;
  SkColor selected_tile;
  SkColor new_tab;
  SkColor footer_icon;
};

// Arc lifts sidebar controls with white over the window material, so they read
// lighter than the sidebar. Dark values are sampled from Arc's dark sidebar;
// footer icons are Arc's muted #A9A2AF over it. The URL pill and header toggle
// chip pick light/dark from the same kColorSysSurface.
Fills GetFills(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  return dark ? Fills{SkColorSetA(SK_ColorWHITE, 0x18),
                      SkColorSetA(SK_ColorWHITE, 0x6F),
                      SkColorSetA(SK_ColorWHITE, 0x33),
                      SkColorSetA(SK_ColorWHITE, 0x99)}
              : Fills{SkColorSetA(SK_ColorWHITE, 0x80), SK_ColorWHITE,
                      SkColorSetA(SK_ColorWHITE, 0xB3),
                      SkColorSetA(SK_ColorBLACK, 0x99)};
}

enum ItemCommand {
  kPin = 1,
  kFavorite,
  kUnsave,
  kClose,
  kDownloads,
  kCreateSpace,
  kNewTab,
  kFirstSpace = 100
};

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

std::unique_ptr<views::MdTextButton> Button(
    views::Button::PressedCallback callback,
    const std::u16string& text) {
  // Icons keep their explicit colors. Retinting them with the text color bakes
  // in the disabled color whenever colors update in an inactive Mac window.
  auto button = std::make_unique<views::MdTextButton>(
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

std::unique_ptr<views::MdTextButton> IconButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int icon_size = 20,
    ui::ColorVariant color = ui::kColorSysOnSurface) {
  auto button = Button(std::move(callback), u"");
  button->SetMinSize(gfx::Size(32, 32));
  button->SetMaxSize(gfx::Size(32, 32));
  button->SetPreferredSize(gfx::Size(32, 32));
  button->SetCustomPadding(gfx::Insets());
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->SetImageCentered(true);
  button->SetCornerRadius(8);
  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(icon, color, icon_size));
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

SidebarView::SidebarView(Delegate& delegate) : delegate_(delegate) {
  SetPreferredSize(gfx::Size(230, 400));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets::TLBR(8, 8, 7, 8), 8));
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

void SidebarView::BuildLibraryMenu() {
  menu_item_id_ = 0;
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItem(kDownloads, u"Downloads");
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  library_space_ids_.clear();
  for (const auto& [id, name] : spaces_) {
    menu_model_->AddItem(
        kFirstSpace + library_space_ids_.size(),
        (id == current_space_id_ ? u"Current Space: " : u"Switch to Space: ") +
            name);
    library_space_ids_.push_back(id);
  }
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(kCreateSpace, u"Create Space");
}

void SidebarView::ShowLibrary(const ui::Event& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetWidget() || (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  const auto target = std::ranges::find(focus_targets_, FocusAction::kLibrary,
                                        &FocusTarget::action);
  if (target == focus_targets_.end()) {
    return;
  }
  const gfx::Rect anchor = target->view->GetBoundsInScreen();
  BuildLibraryMenu();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(
      GetWidget(), nullptr, anchor, views::MenuAnchorPosition::kTopLeft,
      event.IsKeyEvent() ? ui::mojom::MenuSourceType::kKeyboard
                         : ui::mojom::MenuSourceType::kMouse);
}

void SidebarView::BuildCreateMenu() {
  menu_item_id_ = 0;
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItem(kCreateSpace, u"New Space");
  menu_model_->AddItem(kNewTab, u"New Tab");
}

void SidebarView::SetCreateMenuOpen(bool open) {
  create_menu_open_ = open;
  const auto target = std::ranges::find(focus_targets_, FocusAction::kCreate,
                                        &FocusTarget::action);
  if (target == focus_targets_.end()) {
    return;
  }
  auto* button = static_cast<views::MdTextButton*>(target->view.get());
  button->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(
                            open ? vector_icons::kCloseIcon : kPlusIcon,
                            GetFills(GetColorProvider()).footer_icon, 20));
  button->SetTooltipText(open ? u"Close creation menu" : u"Create Space");
  if (open) {
    button->GetViewAccessibility().SetIsExpanded();
  } else {
    button->GetViewAccessibility().SetIsCollapsed();
  }
}

void SidebarView::ShowCreateMenu(const ui::Event& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetWidget()) {
    return;
  }
  if (menu_runner_ && menu_runner_->IsRunning()) {
    if (create_menu_open_) {
      menu_runner_->Cancel();
    }
    return;
  }
  const auto target = std::ranges::find(focus_targets_, FocusAction::kCreate,
                                        &FocusTarget::action);
  if (target == focus_targets_.end()) {
    return;
  }
  const gfx::Rect anchor = target->view->GetBoundsInScreen();
  BuildCreateMenu();
  // Views menus honor the above-footer anchor on macOS; context menus do not.
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::NO_FLAGS,
      base::BindRepeating(&SidebarView::SetCreateMenuOpen,
                          weak_factory_.GetWeakPtr(), false));
  SetCreateMenuOpen(true);
  auto weak = weak_factory_.GetWeakPtr();
  menu_runner_->RunMenuAt(
      GetWidget(), nullptr, anchor, views::MenuAnchorPosition::kBottomCenter,
      event.IsKeyEvent() ? ui::mojom::MenuSourceType::kKeyboard
                         : ui::mojom::MenuSourceType::kMouse);
  // Dispatch can rebuild rows or destroy this view. Resolve the current button.
  if (weak && (!weak->menu_runner_ || !weak->menu_runner_->IsRunning())) {
    weak->SetCreateMenuOpen(false);
  }
}

void SidebarView::ExecuteCommand(int command_id, int event_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (command_id == kDownloads) {
    delegate_->OpenDownloads();
    return;
  }
  if (command_id == kCreateSpace) {
    SetCreateMenuOpen(false);
    delegate_->CreateSpace();
    return;
  }
  if (command_id == kNewTab) {
    SetCreateMenuOpen(false);
    delegate_->NewTab();
    return;
  }
  if (command_id >= kFirstSpace) {
    const size_t index = command_id - kFirstSpace;
    if (index < library_space_ids_.size()) {
      delegate_->SelectSpace(library_space_ids_[index]);
    }
    return;
  }
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
  // The bridge has no saved-title field yet. Preserve the actual saved record
  // while dormant instead of substituting a made-up site name.
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

  std::unique_ptr<views::MdTextButton> close;
  if (presentation.is_live) {
    close = IconButton(
        base::BindRepeating(&Delegate::CloseLiveItem,
                            base::Unretained(&delegate_.get()), item.id),
        vector_icons::kCloseIcon);
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
  focus_targets_.clear();
  spaces_scroll_ = nullptr;
  new_tab_ = nullptr;
  tiles_.clear();
  current_space_id_ = 0;
  spaces_.clear();
  library_space_ids_.clear();
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
  current_space_id_ = model.current_space.id;
  focus_targets_.clear();
  spaces_scroll_ = nullptr;
  new_tab_ = nullptr;
  tiles_.clear();
  RemoveAllChildViews();

  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
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

  const std::u16string name =
      base::UTF8ToUTF16(std::string(model.current_space.name));
  auto heading = Button(base::BindRepeating(&Delegate::RenameSpace,
                                            base::Unretained(&delegate_.get()),
                                            model.current_space.id),
                        name);
  heading->SetTooltipText(u"Rename Space: " + name);
  heading->GetViewAccessibility().SetName(u"Rename Space: " + name);
  TrackFocus(heading.get(), FocusAction::kRename, model.current_space.id);
  body->AddChildView(std::move(heading));

  for (const auto& item : model.pins) {
    body->AddChildView(
        MakeItem(item, false, item.id == model.selected_item_id));
  }
  if (!model.pins.empty()) {
    body->AddChildView(std::make_unique<views::Separator>());
  }

  auto new_tab = Button(base::BindRepeating(&Delegate::NewTab,
                                            base::Unretained(&delegate_.get())),
                        u"New Tab");
  new_tab->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kAdd2Icon,
                                     ui::kColorSysOnSurface, 16));
  TrackFocus(new_tab.get(), FocusAction::kNewTab);
  // Arc keeps 41 DIP between the Space title and New Tab row centers.
  new_tab->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(5, 0, 0, 0));
  new_tab_ = body->AddChildView(std::move(new_tab));
  for (const auto& item : model.temporary) {
    body->AddChildView(
        MakeItem(item, false, item.id == model.selected_item_id));
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

  auto* footer = AddChildView(Box(Orientation::kHorizontal, 4));
  static_cast<views::BoxLayout*>(footer->GetLayoutManager())
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  auto library = IconButton(
      base::BindRepeating(&SidebarView::ShowLibrary, base::Unretained(this)),
      kArchiveIcon, 18);
  library->SetTooltipText(u"Library");
  library->GetViewAccessibility().SetName(u"Library");
  library->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  TrackFocus(library.get(), FocusAction::kLibrary);
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
  // Like Arc, Space dots sit centered between Library and Create.
  auto* spaces = spaces_scroll_->SetContents(Box(Orientation::kHorizontal, 0));
  auto* spaces_layout =
      static_cast<views::BoxLayout*>(spaces->GetLayoutManager());
  spaces_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  spaces_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  spaces->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  spaces->GetViewAccessibility().SetName(u"Spaces");
  views::View* current_space = nullptr;
  std::optional<size_t> current_space_index;
  spaces_.clear();
  for (const auto& space : model.spaces) {
    const bool current = space.id == model.current_space.id;
    // Button presses request a switch; only a Rust snapshot changes selection.
    // The current Space's dot is brighter; no chip, as in Arc.
    auto choice = IconButton(
        base::BindRepeating(&Delegate::SelectSpace,
                            base::Unretained(&delegate_.get()), space.id),
        kDotIcon, 8,
        current ? ui::kColorSysOutline : ui::kColorSysStateDisabled);
    choice->SetMinSize(gfx::Size(30, 32));
    choice->SetMaxSize(gfx::Size(30, 32));
    choice->SetPreferredSize(gfx::Size(30, 32));
    if (current) {
      current_space = choice.get();
      current_space_index = spaces->children().size();
    }
    const std::u16string space_name =
        base::UTF8ToUTF16(std::string(space.name));
    spaces_.emplace_back(space.id, space_name);
    choice->SetTooltipText(
        (current ? u"Current Space: " : u"Switch to Space: ") + space_name);
    choice->GetViewAccessibility().SetName(u"Space: " + space_name);
    choice->GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    choice->GetViewAccessibility().SetCheckedState(
        current ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse);
    TrackFocus(choice.get(), FocusAction::kSpaces, space.id);
    spaces->AddChildView(std::move(choice));
  }
  // Non-layer ScrollViews need an explicit contents size too.
  spaces->SizeToPreferredSize();
  static_cast<views::BoxLayout*>(footer->GetLayoutManager())
      ->SetFlexForView(spaces_scroll_, 1);
  spaces_scroll_->RegisterPostLayoutCallback(base::BindRepeating(
      [](std::optional<gfx::PointF>& offset, bool reveal_current,
         std::optional<size_t> current, views::ScrollView* scroll) {
        // Fill the viewport so the centered dots stay centered when they fit.
        views::View* contents = scroll->contents();
        const int fill = std::max(contents->GetPreferredSize().width(),
                                  scroll->GetVisibleRect().width());
        if (contents->width() != fill) {
          contents->SetSize(gfx::Size(fill, contents->height()));
        }
        if (auto saved = std::exchange(offset, std::nullopt)) {
          scroll->ScrollToOffset(*saved);
          if (reveal_current && current) {
            scroll->contents()->children()[*current]->ScrollViewToVisible();
          }
        }
      },
      base::OwnedRef(std::optional(spaces_offset)), space_changed,
      current_space_index));

  auto create = IconButton(
      base::BindRepeating(&SidebarView::ShowCreateMenu,
                          weak_factory_.GetWeakPtr()),
      create_menu_open_ ? vector_icons::kCloseIcon : kPlusIcon);
  create->SetTooltipText(u"Create Space");
  create->GetViewAccessibility().SetName(u"Create Space");
  create->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  TrackFocus(create.get(), FocusAction::kCreate);
  footer->AddChildView(std::move(create));
  UpdateFills();

  scroll->RegisterPostLayoutCallback(base::BindRepeating(
      [](std::optional<gfx::PointF>& offset, views::ScrollView* scroll) {
        if (auto saved = std::exchange(offset, std::nullopt)) {
          scroll->ScrollToOffset(*saved);
        }
      },
      base::OwnedRef(std::optional(offset))));
  if (focus) {
    views::View* restore = current_space ? current_space : spaces;
    for (const auto& target : focus_targets_) {
      if (target.action == focus->first && target.id == focus->second) {
        restore = target.view;
        break;
      }
    }
    restore->RequestFocus();
  }
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
  }
  const auto library = std::ranges::find(focus_targets_, FocusAction::kLibrary,
                                         &FocusTarget::action);
  if (library != focus_targets_.end()) {
    static_cast<views::MdTextButton*>(library->view.get())
        ->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(kArchiveIcon,
                                                       fills.footer_icon, 18));
  }
  SetCreateMenuOpen(create_menu_open_);
}

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace myarc
