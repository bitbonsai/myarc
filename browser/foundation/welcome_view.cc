#include "myarc/welcome_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

constexpr char16_t kTitle[] = u"Welcome to myArc";
constexpr char16_t kBody[] =
    u"Everything stays on this Mac, and there's no account to make. Bring "
    u"your Spaces over from Arc, or start with a clean sidebar.";
constexpr char16_t kStart[] = u"Start browsing";
constexpr char16_t kImport[] = u"Import from Arc…";
constexpr char16_t kImported[] = u"Imported from Arc";
constexpr char16_t kSkip[] = u"Skip tour";

constexpr int kColumnWidth = 360;
constexpr int kButtonWidth = 240;
constexpr int kButtonHeight = 36;
constexpr int kButtonRadius = 8;
constexpr SkColor kPrimary = SkColorSetRGB(0x3D, 0x4B, 0xF2);

// A field of two quiet tints, light or dark; step 4 animates it.
constexpr SkColor kFieldLightTop = SkColorSetRGB(0xF1, 0xF0, 0xFF);
constexpr SkColor kFieldLightBottom = SkColorSetRGB(0xFB, 0xF1, 0xF6);
constexpr SkColor kFieldDarkTop = SkColorSetRGB(0x1F, 0x20, 0x33);
constexpr SkColor kFieldDarkBottom = SkColorSetRGB(0x2A, 0x20, 0x33);

gfx::FontList Font(int delta, gfx::Font::Weight weight) {
  return gfx::FontList().Derive(delta, gfx::Font::NORMAL, weight);
}

// LabelButton hides its label, so set the font on its Label child.
void SetButtonFont(views::LabelButton& button, const gfx::FontList& font) {
  for (views::View* child : button.children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(font);
      // The sheet paints to a translucent layer.
      label->SetSubpixelRenderingEnabled(false);
    }
  }
}

// macOS views buttons ignore Return and take focus only with Full Keyboard
// Access; the sheet needs both.
class SheetButton : public views::LabelButton {
  METADATA_HEADER(SheetButton, views::LabelButton)

 public:
  SheetButton(PressedCallback callback, const std::u16string& text)
      : views::LabelButton(std::move(callback), text) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kButtonRadius);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_RETURN) {
      if (GetEnabled() && !event.is_repeat()) {
        NotifyClick(event);
      }
      return true;
    }
    return views::LabelButton::OnKeyPressed(event);
  }
  // Keeps Return from reaching the window's accelerators first.
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override {
    return event.key_code() == ui::VKEY_RETURN ||
           views::LabelButton::SkipDefaultKeyEventProcessing(event);
  }
};

BEGIN_METADATA(SheetButton)
END_METADATA

bool IsDark(const views::View& view) {
  const ui::ColorProvider* provider = view.GetColorProvider();
  return provider &&
         color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
}

}  // namespace

WelcomeView::WelcomeView(bool offers_import,
                         RadiusCallback radius,
                         ActionCallback action)
    : radius_(std::move(radius)), action_(std::move(action)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(24), 0));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(std::u16string(kTitle));
  GetViewAccessibility().SetDescription(std::u16string(kBody));

  auto title = std::make_unique<views::Label>(kTitle);
  title->SetFontList(Font(14, gfx::Font::Weight::BOLD));
  title->SetAutoColorReadabilityEnabled(false);
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetSubpixelRenderingEnabled(false);
  title->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
  title_ = AddChildView(std::move(title));

  auto body = std::make_unique<views::Label>(kBody);
  body->SetFontList(Font(2, gfx::Font::Weight::NORMAL));
  body->SetMultiLine(true);
  body->SetLineHeight(22);
  body->SetMaximumWidth(kColumnWidth);
  body->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  body->SetAutoColorReadabilityEnabled(false);
  body->SetBackgroundColor(SK_ColorTRANSPARENT);
  body->SetSubpixelRenderingEnabled(false);
  body->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, 0, 28, 0));
  body_ = AddChildView(std::move(body));

  const auto pressed = [](WelcomeView* view, Action action) {
    view->Press(action);
  };
  start_ = AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this),
                          Action::kStartBrowsing),
      kStart));
  import_ = AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this), Action::kImport),
      offers_import ? kImport : kImported));
  import_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(10, 0, 0, 0));
  import_->SetEnabled(offers_import);
  skip_ = AddChildView(std::make_unique<SheetButton>(
      base::BindRepeating(pressed, base::Unretained(this), Action::kSkipTour),
      kSkip));
  skip_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(16, 0, 0, 0));
  skip_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 10)));
  for (views::LabelButton* button : {start_.get(), import_.get()}) {
    button->SetPreferredSize(gfx::Size(kButtonWidth, kButtonHeight));
    button->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    SetButtonFont(*button, Font(1, gfx::Font::Weight::SEMIBOLD));
  }
  SetButtonFont(*skip_, Font(0, gfx::Font::Weight::MEDIUM));
}

WelcomeView::~WelcomeView() = default;

void WelcomeView::TakeFocus() {
  SetPaneFocusAndFocusDefault();
  GetViewAccessibility().AnnounceText(std::u16string(kTitle) + u". " +
                                      std::u16string(kBody));
}

bool WelcomeView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    Press(Action::kSkipTour);
    return true;
  }
  return views::AccessiblePaneView::AcceleratorPressed(accelerator);
}

views::View* WelcomeView::GetDefaultFocusableChild() {
  return start_;
}

void WelcomeView::OnPaint(gfx::Canvas* canvas) {
  const bool dark = IsDark(*this);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(
      gfx::CreateGradientShader(gfx::Point(), gfx::Point(0, height()),
                                dark ? kFieldDarkTop : kFieldLightTop,
                                dark ? kFieldDarkBottom : kFieldLightBottom));
  canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()),
                        radius_ ? radius_.Run() : 0, flags);
}

void WelcomeView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  const bool dark = IsDark(*this);
  const SkColor ink = dark ? SK_ColorWHITE : SkColorSetRGB(0x1A, 0x1A, 0x22);
  views::AsViewClass<views::Label>(title_.get())->SetEnabledColor(ink);
  views::AsViewClass<views::Label>(body_.get())
      ->SetEnabledColor(SkColorSetA(ink, 0xB8));
  start_->SetBackground(
      views::CreateRoundedRectBackground(kPrimary, kButtonRadius));
  start_->SetEnabledTextColors(SK_ColorWHITE);
  import_->SetBackground(views::CreateRoundedRectBackground(
      SkColorSetA(ink, dark ? 0x1F : 0x12), kButtonRadius));
  import_->SetEnabledTextColors(ink);
  import_->SetTextColor(views::Button::STATE_DISABLED, SkColorSetA(ink, 0x80));
  skip_->SetEnabledTextColors(SkColorSetA(ink, 0x99));
  SchedulePaint();
}

void WelcomeView::Press(Action action) {
  // A copy: the action may close the sheet and delete this view.
  auto run = action_;
  run.Run(action);
}

BEGIN_METADATA(WelcomeView)
END_METADATA

}  // namespace myarc
