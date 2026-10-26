#include "myarc/sidebar_view.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/bridge.rs.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;

enum ItemCommand { kPin = 1, kFavorite, kUnsave, kClose };

std::unique_ptr<views::View> Box(Orientation orientation, int spacing) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(orientation, gfx::Insets(), spacing));
  return view;
}

std::unique_ptr<views::MdTextButton> Button(
    views::Button::PressedCallback callback,
    const std::u16string& text) {
  auto button =
      std::make_unique<views::MdTextButton>(std::move(callback), text);
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
  SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kVertical,
                                                      gfx::Insets(8), 8));
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Workspace sidebar");
  SetUnavailable(u"Loading workspace\u2026");
}

SidebarView::~SidebarView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  // The bridge has no saved-title field yet. Preserve the actual saved record
  // while dormant instead of substituting a made-up site name.
  if (presentation.title.empty()) {
    presentation.title =
        item.has_url ? base::UTF8ToUTF16(std::string(item.canonical_url))
                     : u"Untitled tab";
  }
  const auto foreground =
      selected ? ui::kColorSysOnSecondaryContainer : ui::kColorSysOnSurface;
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
  if (selected || tile) {
    activate->SetBgColorOverrideDeprecated(std::nullopt);
    activate->SetBgColorIdOverride(selected ? ui::kColorSysSecondaryContainer
                                            : ui::kColorSysSurfaceContainer);
  }
  const ItemMenuState menu_state = {
      item.placement == foundation::PlacementKind::Pin,
      item.placement == foundation::PlacementKind::Favorite,
      presentation.is_live};
  TrackFocus(activate.get(), FocusAction::kActivate, item.id, menu_state);
  if (tile) {
    activate->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    activate->SetImageCentered(true);
    activate->SetMinSize(gfx::Size(0, 48));
    activate->SetPreferredSize(gfx::Size(0, 48));
    return activate;
  }

  auto row = Box(Orientation::kHorizontal, 0);
  auto* layout = static_cast<views::BoxLayout*>(row->GetLayoutManager());
  layout->SetFlexForView(row->AddChildView(std::move(activate)), 1);
  if (presentation.is_live) {
    auto close =
        Button(base::BindRepeating(&Delegate::CloseLiveItem,
                                   base::Unretained(&delegate_.get()), item.id),
               u"");
    close->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kCloseIcon,
                                       ui::kColorSysOnSurface, 16));
    const std::u16string close_name =
        (menu_state.is_pin        ? u"Close live page, keep Space Pin: "
         : menu_state.is_favorite ? u"Close live page, keep Favorite: "
                                  : u"Close live page: ") +
        presentation.title;
    close->SetTooltipText(close_name);
    close->GetViewAccessibility().SetName(close_name);
    TrackFocus(close.get(), FocusAction::kClose, item.id, menu_state);
    row->AddChildView(std::move(close));
  }
  return row;
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
  current_space_id_ = 0;
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
      favorites_row = body->AddChildView(Box(Orientation::kHorizontal, 6));
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
    cell->SetPreferredSize(gfx::Size(0, 48));
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

  auto new_tab = Button(base::BindRepeating(&Delegate::NewTab,
                                            base::Unretained(&delegate_.get())),
                        u"New Tab");
  new_tab->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kAdd2Icon,
                                     ui::kColorSysOnSurface, 16));
  TrackFocus(new_tab.get(), FocusAction::kNewTab);
  body->AddChildView(std::move(new_tab));
  for (const auto& item : model.pins) {
    body->AddChildView(
        MakeItem(item, false, item.id == model.selected_item_id));
  }
  if (!model.pins.empty() && !model.temporary.empty()) {
    body->AddChildView(std::make_unique<views::Separator>());
  }
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
  spaces_scroll_ = footer->AddChildView(std::make_unique<views::ScrollView>());
  spaces_scroll_->SetBackgroundColor(std::nullopt);
  spaces_scroll_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  spaces_scroll_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kEnabled);
  spaces_scroll_->SetTreatAllScrollEventsAsHorizontal(true);
  spaces_scroll_->SetUseContentsPreferredSize(true);
  // Leave room for the non-overlay horizontal scrollbar as well as 32-DIP dots.
  spaces_scroll_->SetPreferredSize(gfx::Size(0, 48));
  auto* spaces = spaces_scroll_->SetContents(Box(Orientation::kHorizontal, 2));
  spaces->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  spaces->GetViewAccessibility().SetName(u"Spaces");
  views::View* current_space = nullptr;
  std::optional<size_t> current_space_index;
  for (const auto& space : model.spaces) {
    const bool current = space.id == model.current_space.id;
    // Button presses request a switch; only a Rust snapshot changes selection.
    auto choice = Button(
        base::BindRepeating(&Delegate::SelectSpace,
                            base::Unretained(&delegate_.get()), space.id),
        u"\u2022");
    choice->SetMinSize(gfx::Size(32, 32));
    choice->SetPreferredSize(gfx::Size(32, 32));
    choice->SetCustomPadding(gfx::Insets());
    choice->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    choice->SetCornerRadius(16);
    choice->SetEnabledTextColors(current ? ui::kColorSysOnSecondaryContainer
                                         : ui::kColorSysOnSurfaceVariant);
    if (current) {
      choice->SetBgColorOverrideDeprecated(std::nullopt);
      choice->SetBgColorIdOverride(ui::kColorSysSecondaryContainer);
      current_space = choice.get();
      current_space_index = spaces->children().size();
    }
    const std::u16string space_name =
        base::UTF8ToUTF16(std::string(space.name));
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
        if (auto saved = std::exchange(offset, std::nullopt)) {
          scroll->ScrollToOffset(*saved);
          if (reveal_current && current) {
            scroll->contents()->children()[*current]->ScrollViewToVisible();
          }
        }
      },
      base::OwnedRef(std::optional(spaces_offset)), space_changed,
      current_space_index));

  auto create = Button(base::BindRepeating(&Delegate::CreateSpace,
                                           base::Unretained(&delegate_.get())),
                       u"+");
  create->SetTooltipText(u"Create Space");
  create->GetViewAccessibility().SetName(u"Create Space");
  TrackFocus(create.get(), FocusAction::kCreate);
  footer->AddChildView(std::move(create));

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

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace myarc
