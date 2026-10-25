#include "myarc/sidebar_view.h"

#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/bridge.rs.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;

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
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(ui::kColorSysOnSurface);
}

}  // namespace

SidebarView::SidebarView(Delegate& delegate) : delegate_(delegate) {
  SetPreferredSize(gfx::Size(230, 400));
  SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kVertical,
                                                      gfx::Insets(8), 8));
  GetViewAccessibility().SetName(u"Workspace sidebar");
  SetUnavailable(u"Loading workspace\u2026");
}

SidebarView::~SidebarView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SidebarView::TrackFocus(views::View* view,
                             FocusAction action,
                             uint64_t id) {
  focus_targets_.push_back({action, id, view});
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
  TrackFocus(activate.get(), FocusAction::kActivate, item.id);
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
    close->SetTooltipText(u"Close tab: " + presentation.title);
    close->GetViewAccessibility().SetName(u"Close tab: " + presentation.title);
    TrackFocus(close.get(), FocusAction::kClose, item.id);
    row->AddChildView(std::move(close));
  }
  return row;
}

void SidebarView::SetUnavailable(const std::u16string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  focus_targets_.clear();
  RemoveAllChildViews();
  AddStatus(*this, reason.empty() ? u"Workspace unavailable" : reason);
}

void SidebarView::SetModel(const foundation::SidebarResult& model,
                           const std::u16string& status) {
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
    // A usable model always has its ScrollView first; unavailable state has
    // no focus targets and contains only a status label.
    if (!focus_targets_.empty()) {
      offset =
          static_cast<views::ScrollView*>(children().front())->CurrentOffset();
    }
  }
  focus_targets_.clear();
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
  heading->SetLabelStyle(views::style::STYLE_EMPHASIZED);
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

  auto* footer = AddChildView(Box(Orientation::kHorizontal, 4));
  std::vector<ui::SimpleComboboxModel::Item> choices;
  std::vector<uint64_t> ids;
  std::optional<size_t> current;
  for (const auto& space : model.spaces) {
    if (space.id == model.current_space.id) {
      current = choices.size();
    }
    choices.emplace_back(base::UTF8ToUTF16(std::string(space.name)));
    ids.push_back(space.id);
  }
  auto* spaces = footer->AddChildView(std::make_unique<views::Combobox>(
      std::make_unique<ui::SimpleComboboxModel>(std::move(choices))));
  spaces->SetSizeToLargestLabel(false);
  spaces->SetSelectedIndex(current);
  spaces->SetTooltipTextAndAccessibleName(u"Current Space: " + name);
  spaces->SetBackgroundColorId(ui::kColorSysSurfaceContainer);
  spaces->SetForegroundColorId(ui::kColorSysOnSurface);
  spaces->SetFocusBehavior(FocusBehavior::ALWAYS);
  spaces->SetCallback(base::BindRepeating(
      [](Delegate* delegate, views::Combobox* combo,
         const std::vector<uint64_t>& ids, std::optional<size_t> current) {
        const auto index = combo->GetSelectedIndex();
        // Wait for the next Rust snapshot before showing a successful switch.
        combo->SetSelectedIndex(current);
        if (index && *index < ids.size() && index != current) {
          delegate->SelectSpace(ids[*index]);
        }
      },
      base::Unretained(&delegate_.get()), base::Unretained(spaces),
      std::move(ids), current));
  TrackFocus(spaces, FocusAction::kSpaces);
  static_cast<views::BoxLayout*>(footer->GetLayoutManager())
      ->SetFlexForView(spaces, 1);

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
    views::View* restore = spaces;
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
