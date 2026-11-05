#include "myarc/create_space_panel.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "myarc/icons.h"
#include "myarc/space_icon_picker.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;
using CrossAxis = views::BoxLayout::CrossAxisAlignment;

// Measured from Arc's panel in a 230 DIP sidebar: 200 DIP wide controls, 36
// DIP rows 8 apart, and the buttons pinned to the bottom, 22 DIP above the
// Space footer.
constexpr int kSideInset = 7;
constexpr int kRowHeight = 36;
constexpr int kRowRadius = 7;
constexpr int kRowGap = 8;
constexpr int kRowIconSize = 16;
constexpr int kArtSize = 48;

// The switch that opens the panel for captures; "emoji" or "icon" also opens
// the icon picker on that tab and holds it open.
constexpr char kCaptureCreateSpace[] = "myarc-capture-create-space";

// Arc's popover: 14 DIP corners on its dark sidebar navy.
constexpr int kPickerRadius = 14;
constexpr SkColor kPickerDark = SkColorSetRGB(0x25, 0x24, 0x2E);

// Arc's primary blue, sampled from its Create Space button; disabled, the
// button fades into the sidebar.
constexpr SkColor kPrimary = SkColorSetRGB(0x21, 0x2A, 0xFB);
constexpr SkAlpha kPrimaryDisabledAlpha = 0x6B;

struct Colors {
  SkColor ink;
  SkColor subtitle;
  SkColor muted;
  SkColor row;
  SkColor pill;
  SkColor cancel;
  SkColor error;
  bool dark;
};

// Dark values are sampled from Arc's dark sidebar: rows lift it with white at
// about 9%, the Profile pill adds 18% on its row.
Colors GetColors(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  const SkColor ink = dark ? SK_ColorWHITE : SK_ColorBLACK;
  return {.ink = ink,
          .subtitle = SkColorSetA(ink, 0xD9),
          .muted = SkColorSetA(ink, 0x99),
          .row = SkColorSetA(SK_ColorWHITE, dark ? 0x16 : 0x80),
          .pill = SkColorSetA(dark ? SK_ColorWHITE : SK_ColorBLACK,
                              dark ? 0x2E : 0x14),
          // Arc's Cancel is a step dimmer than its labels.
          .cancel = SkColorSetA(ink, 0xB3),
          .error = dark ? SkColorSetRGB(0xFF, 0x8A, 0x8A)
                        : SkColorSetRGB(0xB3, 0x26, 0x1E),
          .dark = dark};
}

std::unique_ptr<views::View> Row(const gfx::Insets& insets, int spacing) {
  auto row = std::make_unique<views::View>();
  auto* layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kHorizontal, insets, spacing));
  layout->set_cross_axis_alignment(CrossAxis::kCenter);
  return row;
}

std::unique_ptr<views::Label> MakeLabel(const std::u16string& text,
                                        const gfx::FontList& font) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(font);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  return label;
}

gfx::FontList Font(int delta, gfx::Font::Weight weight) {
  return gfx::FontList().Derive(delta, gfx::Font::NORMAL, weight);
}

// LabelButton hides its label, so set the font on its Label child.
void SetButtonFont(views::LabelButton& button, const gfx::FontList& font) {
  for (views::View* child : button.children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(font);
    }
  }
}

}  // namespace

CreateSpacePanel::CreateSpacePanel(std::u16string profile_name,
                                   CreateCallback create,
                                   base::RepeatingClosure cancel)
    : create_(std::move(create)), cancel_(std::move(cancel)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets::TLBR(23, kSideInset, 14, kSideInset),
      0));
  layout->set_cross_axis_alignment(CrossAxis::kStretch);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Create a Space");

  // A placeholder for Arc's illustration: the Library's Spaces glyph, large.
  art_ = AddChildView(std::make_unique<views::ImageView>());
  art_->SetPreferredSize(gfx::Size(kArtSize, kArtSize));

  title_ = AddChildView(
      MakeLabel(u"Create a Space", Font(4, gfx::Font::Weight::BOLD)));
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(14, 0, 0, 0));

  subtitle_ = AddChildView(
      MakeLabel(u"Separate your tabs for life, work, projects, and more.",
                Font(0, gfx::Font::Weight::NORMAL)));
  subtitle_->SetMultiLine(true);
  subtitle_->SetLineHeight(18);
  subtitle_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  subtitle_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(8, 4, 0, 4));

  // Name field: the icon button 8 DIP in, the text 10 DIP after it.
  auto* name_row = AddChildView(Row(gfx::Insets::TLBR(0, 8, 0, 10), 8));
  name_row_ = name_row;
  name_row->SetPreferredSize(gfx::Size(0, kRowHeight));
  name_row->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(30, 0, 0, 0));
  auto icon_button = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](CreateSpacePanel* panel) { panel->OpenIconPicker(panel->icon_.kind); },
      base::Unretained(this)));
  icon_button->SetPreferredSize(gfx::Size(20, 20));
  icon_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  icon_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  icon_button->SetTooltipText(u"Choose an icon");
  views::InstallRoundRectHighlightPathGenerator(icon_button.get(),
                                                gfx::Insets(-2), 5);
  icon_button_ = name_row->AddChildView(std::move(icon_button));
  auto name = std::make_unique<views::Textfield>();
  name->SetBackgroundEnabled(false);
  name->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  name->SetFontList(Font(1, gfx::Font::Weight::NORMAL));
  name->SetPlaceholderText(u"Space name…");
  name->GetViewAccessibility().SetName(u"Space name");
  name->set_controller(this);
  name_ = name_row->AddChildView(std::move(name));
  static_cast<views::BoxLayout*>(name_row->GetLayoutManager())
      ->SetFlexForView(name_, 1);

  error_ = AddChildView(MakeLabel({}, Font(-1, gfx::Font::Weight::NORMAL)));
  error_->SetMultiLine(true);
  error_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  error_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(6, 4, 0, 4));
  error_->SetVisible(false);

  // Profile row: which Profile the new Space uses. The pill becomes a menu
  // with New Profile… later; for now it names the current Profile.
  auto* profile_row = AddChildView(Row(gfx::Insets::VH(0, 10), 0));
  profile_row_ = profile_row;
  profile_row->SetPreferredSize(gfx::Size(0, kRowHeight));
  profile_row->SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(kRowGap, 0, 0, 0));
  profile_icon_ =
      profile_row->AddChildView(std::make_unique<views::ImageView>());
  profile_label_ = profile_row->AddChildView(
      MakeLabel(u"Profile", Font(0, gfx::Font::Weight::SEMIBOLD)));
  profile_label_->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, 10, 0, 8));
  auto* pill = profile_row->AddChildView(Row(gfx::Insets::TLBR(2, 8, 2, 6), 3));
  profile_pill_ = pill;
  profile_name_ = pill->AddChildView(
      MakeLabel(profile_name, Font(-1, gfx::Font::Weight::SEMIBOLD)));
  profile_name_->SetElideBehavior(gfx::ELIDE_TAIL);
  // Long names elide so the chevron and the info icon stay in the row.
  profile_name_->SetMaximumWidthSingleLine(44);
  profile_chevron_ = pill->AddChildView(std::make_unique<views::ImageView>());
  pill->GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  pill->GetViewAccessibility().SetName(u"Profile: " + profile_name);
  auto* spacer = profile_row->AddChildView(std::make_unique<views::View>());
  static_cast<views::BoxLayout*>(profile_row->GetLayoutManager())
      ->SetFlexForView(spacer, 1);
  profile_info_ =
      profile_row->AddChildView(std::make_unique<views::ImageView>());
  profile_info_->SetTooltipText(
      u"Spaces that share a Profile share its logins, history and Favorites.");

  // Choose a Theme: a placeholder row until the theme picker exists.
  auto* theme_row = AddChildView(Row(gfx::Insets::VH(0, 10), 10));
  theme_row_ = theme_row;
  theme_row->SetPreferredSize(gfx::Size(0, kRowHeight));
  theme_row->SetProperty(views::kMarginsKey,
                         gfx::Insets::TLBR(kRowGap, 0, 0, 0));
  theme_icon_ = theme_row->AddChildView(std::make_unique<views::ImageView>());
  theme_label_ = theme_row->AddChildView(
      MakeLabel(u"Choose a Theme", Font(0, gfx::Font::Weight::SEMIBOLD)));
  theme_label_->SetCustomTooltipText(u"Space themes are not available yet.");

  auto* fill = AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(fill, 1);

  auto create_button = std::make_unique<views::LabelButton>(
      base::BindRepeating(&CreateSpacePanel::Create, base::Unretained(this)),
      u"Create Space");
  create_button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetButtonFont(*create_button, Font(0, gfx::Font::Weight::SEMIBOLD));
  create_button->SetPreferredSize(gfx::Size(0, kRowHeight));
  create_button->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  views::InstallRoundRectHighlightPathGenerator(create_button.get(),
                                                gfx::Insets(), kRowRadius);
  create_button_ = AddChildView(std::move(create_button));

  auto cancel_button = std::make_unique<views::LabelButton>(
      base::BindRepeating(&CreateSpacePanel::Cancel, base::Unretained(this)),
      u"Cancel");
  cancel_button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetButtonFont(*cancel_button, Font(0, gfx::Font::Weight::MEDIUM));
  cancel_button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 8)));
  views::InstallRoundRectHighlightPathGenerator(cancel_button.get(),
                                                gfx::Insets(), 6);
  // Centred on its own, so only the text takes clicks.
  auto* cancel_row = AddChildView(Row(gfx::Insets(), 0));
  cancel_row->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(10, 0, 0, 0));
  static_cast<views::BoxLayout*>(cancel_row->GetLayoutManager())
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  cancel_button_ = cancel_row->AddChildView(std::move(cancel_button));

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  UpdateCreateButton();
}

CreateSpacePanel::~CreateSpacePanel() = default;

void CreateSpacePanel::FocusName() {
  name_->RequestFocus();
}

void CreateSpacePanel::OpenIconPicker(SpaceIconChoice::Kind tab, bool hold) {
  if (picker_widget_ && !picker_widget_->IsClosed()) {
    picker_widget_->Activate();
    return;
  }
  if (picker_widget_) {
    OnIconPickerClosed();
  }
  if (!GetWidget()) {
    return;
  }
  // Arc hangs the popover under the whole name field, its left edge on the
  // field's.
  picker_delegate_ = std::make_unique<views::BubbleDialogDelegate>(
      name_row_.get(), views::BubbleBorder::TOP_LEFT);
  auto* delegate = picker_delegate_.get();
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetShowCloseButton(false);
  delegate->SetShowTitle(false);
  delegate->SetTitle(u"Space icon");
  delegate->set_margins(gfx::Insets());
  delegate->set_corner_radius(kPickerRadius);
  delegate->set_close_on_deactivate(!hold);
  const Colors colors = GetColors(GetColorProvider());
  delegate->SetBackgroundColor(colors.dark ? kPickerDark : SK_ColorWHITE);
  SpaceIconChoice current = icon_;
  current.kind = tab;
  picker_ = delegate->SetContentsView(std::make_unique<SpaceIconPickerView>(
      current,
      base::BindOnce(&CreateSpacePanel::PickIcon, weak_factory_.GetWeakPtr())));
  delegate->SetInitiallyFocusedView(picker_->search());
  picker_widget_ = views::BubbleDialogDelegate::CreateBubble(
      delegate,
      base::IgnoreArgs<views::Widget::ClosedReason>(base::BindOnce(
          &CreateSpacePanel::OnIconPickerClosed, weak_factory_.GetWeakPtr())));
  // Arc points a caret at the icon button. macOS bubbles use the OS shadow,
  // which can't be combined with views' visible arrow, so the popover has
  // none.
  if (hold) {
    picker_widget_->ShowInactive();
  } else {
    picker_widget_->Show();
  }
}

void CreateSpacePanel::PickIcon(SpaceIconChoice icon) {
  if (!IsBundledSpaceIcon(icon)) {
    return;
  }
  icon_ = std::move(icon);
  UpdateIconButton();
  if (picker_widget_) {
    picker_widget_->Close();
  }
  name_->RequestFocus();
}

void CreateSpacePanel::OnIconPickerClosed() {
  picker_ = nullptr;
  if (picker_delegate_) {
    picker_delegate_->SetAnchorView(nullptr);
  }
  // The widget is mid-close; free it and its delegate once that unwinds.
  if (picker_widget_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(picker_widget_));
  }
  if (picker_delegate_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(picker_delegate_));
  }
}

void CreateSpacePanel::UpdateIconButton() {
  if (!GetColorProvider()) {
    return;
  }
  const Colors colors = GetColors(GetColorProvider());
  ui::ImageModel image = SpaceIconImage(icon_, colors.ink, 18);
  std::u16string name = u"Space icon";
  if (image.IsEmpty()) {
    image =
        ui::ImageModel::FromVectorIcon(kSquareDashedPlusIcon, colors.muted, 18);
  } else if (icon_.kind == SpaceIconChoice::Kind::kEmoji) {
    const std::string_view keywords = FindBundledEmoji(icon_.value)->keywords;
    name += u": " + base::UTF8ToUTF16(keywords.substr(0, keywords.find('|')));
  } else {
    name += u": " + base::UTF8ToUTF16(icon_.value);
  }
  icon_button_->SetImageModel(views::Button::STATE_NORMAL, image);
  icon_button_->GetViewAccessibility().SetName(name);
}

void CreateSpacePanel::AddedToWidget() {
  views::View::AddedToWidget();
  const std::string capture =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kCaptureCreateSpace);
  if (capture_scheduled_ || (capture != "emoji" && capture != "icon")) {
    return;
  }
  capture_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<CreateSpacePanel> panel, bool icon) {
            if (panel) {
              panel->OpenIconPicker(icon ? SpaceIconChoice::Kind::kNamed
                                         : SpaceIconChoice::Kind::kEmoji,
                                    /*hold=*/true);
            }
          },
          weak_factory_.GetWeakPtr(), capture == "icon"),
      base::Milliseconds(800));
}

void CreateSpacePanel::OnThemeChanged() {
  views::View::OnThemeChanged();
  const Colors colors = GetColors(GetColorProvider());
  art_->SetImage(
      ui::ImageModel::FromVectorIcon(kSpacesIcon, colors.subtitle, kArtSize));
  title_->SetEnabledColor(colors.ink);
  subtitle_->SetEnabledColor(colors.subtitle);
  name_row_->SetBackground(
      views::CreateRoundedRectBackground(colors.row, kRowRadius));
  UpdateIconButton();
  name_->SetColor(colors.ink);
  error_->SetEnabledColor(colors.error);
  profile_row_->SetBackground(
      views::CreateRoundedRectBackground(colors.row, kRowRadius));
  profile_icon_->SetImage(
      ui::ImageModel::FromVectorIcon(kSmileIcon, colors.muted, kRowIconSize));
  profile_label_->SetEnabledColor(colors.ink);
  profile_pill_->SetBackground(
      views::CreateRoundedRectBackground(colors.pill, 10));
  profile_name_->SetEnabledColor(colors.ink);
  profile_chevron_->SetImage(
      ui::ImageModel::FromVectorIcon(kChevronDownIcon, colors.ink, 12));
  profile_info_->SetImage(
      ui::ImageModel::FromVectorIcon(kInfoIcon, colors.muted, 15));
  theme_row_->SetBackground(
      views::CreateRoundedRectBackground(colors.row, kRowRadius));
  theme_icon_->SetImage(
      ui::ImageModel::FromVectorIcon(kBrushIcon, colors.muted, kRowIconSize));
  theme_label_->SetEnabledColor(colors.ink);
  cancel_button_->SetEnabledTextColors(colors.cancel);
  UpdateCreateButton();
}

bool CreateSpacePanel::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // The window offers accelerators before the focused view sees the key, so
  // leave Esc to the omnibox and pages unless focus is in the sidebar.
  const views::FocusManager* manager = GetFocusManager();
  const views::View* focused = manager ? manager->GetFocusedView() : nullptr;
  if (focused && parent() && !parent()->Contains(focused)) {
    return false;
  }
  Cancel();
  return true;
}

void CreateSpacePanel::ContentsChanged(views::Textfield* sender,
                                       const std::u16string& new_contents) {
  error_->SetVisible(false);
  name_->SetInvalid(false);
  UpdateCreateButton();
}

bool CreateSpacePanel::HandleKeyEvent(views::Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }
  switch (key_event.key_code()) {
    case ui::VKEY_RETURN:
      Create();
      return true;
    case ui::VKEY_ESCAPE:
      Cancel();
      return true;
    default:
      return false;
  }
}

std::u16string CreateSpacePanel::TrimmedName() const {
  std::u16string name;
  base::TrimWhitespace(name_->GetText(), base::TRIM_ALL, &name);
  return name;
}

void CreateSpacePanel::UpdateCreateButton() {
  const bool enabled = !TrimmedName().empty();
  create_button_->SetEnabled(enabled);
  create_button_->SetBackground(views::CreateRoundedRectBackground(
      enabled ? kPrimary : SkColorSetA(kPrimary, kPrimaryDisabledAlpha),
      kRowRadius));
  create_button_->SetEnabledTextColors(SK_ColorWHITE);
  create_button_->SetTextColor(views::Button::STATE_DISABLED,
                               SkColorSetA(SK_ColorWHITE, 0x40));
}

void CreateSpacePanel::Create() {
  if (TrimmedName().empty()) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateSpacePanel::RunCreate, weak_factory_.GetWeakPtr()));
}

void CreateSpacePanel::Cancel() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<CreateSpacePanel> panel) {
                       if (panel) {
                         // A copy, since closing deletes the panel.
                         auto cancel = panel->cancel_;
                         cancel.Run();
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

void CreateSpacePanel::RunCreate() {
  const std::u16string name = TrimmedName();
  if (name.empty()) {
    return;
  }
  auto weak = weak_factory_.GetWeakPtr();
  // A copy, since success deletes the panel.
  auto create = create_;
  const std::u16string error = create.Run(name, icon_);
  if (!weak || error.empty()) {
    return;
  }
  name_->SetInvalid(true);
  error_->SetText(error);
  error_->SetVisible(true);
  error_->GetViewAccessibility().AnnounceText(error);
}

BEGIN_METADATA(CreateSpacePanel)
END_METADATA

}  // namespace myarc
