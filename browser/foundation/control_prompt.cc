#include "myarc/control_prompt.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace myarc {
namespace {

// The Settings window's dark card, with its row text and pill buttons.
constexpr int kWidth = 320;
constexpr int kInset = 14;
constexpr int kTextWidth = kWidth - 2 * kInset;
constexpr float kRadius = 10;
constexpr int kButtonHeight = 24;
constexpr int kButtonRadius = 5;
constexpr int kButtonPad = 12;
// Offset from the window's top-right corner, as the copied URL toast sits.
constexpr int kTopMargin = 19;
constexpr int kRightMargin = 17;
constexpr SkColor kFill = SkColorSetRGB(0x25, 0x25, 0x26);
constexpr SkColor kStroke = SkColorSetRGB(0x3A, 0x3A, 0x3B);
constexpr SkColor kTitle = SkColorSetRGB(0xF2, 0xF2, 0xF2);
constexpr SkColor kBody = SkColorSetRGB(0xC2, 0xC2, 0xC3);
constexpr SkColor kBlueButton = SkColorSetRGB(0x00, 0x7A, 0xFF);
constexpr SkColor kGrayButton = SkColorSetRGB(0x45, 0x45, 0x46);
constexpr SkColor kButtonText = SkColorSetRGB(0xE3, 0xE3, 0xE3);

gfx::FontList Font(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.Derive(size - base.GetFontSize(), gfx::Font::NORMAL, weight);
}

views::Label* AddText(views::View* parent,
                      const std::u16string& text,
                      int size,
                      gfx::Font::Weight weight,
                      SkColor color) {
  auto* label = parent->AddChildView(std::make_unique<views::Label>(text));
  label->SetFontList(Font(size, weight));
  label->SetEnabledColor(color);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetMultiLine(true);
  label->SetMaximumWidth(kTextWidth);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

views::LabelButton* AddButton(views::View* parent,
                              views::Button::PressedCallback callback,
                              const std::u16string& text,
                              SkColor fill,
                              SkColor color) {
  auto* button = parent->AddChildView(
      std::make_unique<views::LabelButton>(std::move(callback), text));
  button->SetTextSubpixelRenderingEnabled(false);
  button->SetEnabledTextColors(color);
  button->SetTextColor(views::Button::STATE_DISABLED, color);
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->SetBackground(views::CreateRoundedRectBackground(fill, kButtonRadius));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kButtonPad)));
  button->SetMinSize(gfx::Size(0, kButtonHeight));
  button->SetMaxSize(gfx::Size(0, kButtonHeight));
  return button;
}

class PromptView : public views::BoxLayoutView {
  METADATA_HEADER(PromptView, views::BoxLayoutView)

 public:
  PromptView(const std::u16string& client,
             base::RepeatingCallback<void(bool)> answer) {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetInsideBorderInsets(gfx::Insets(kInset));
    SetBetweenChildSpacing(6);
    const std::u16string title =
        client + u" wants to control your Agent Space";
    GetViewAccessibility().SetRole(ax::mojom::Role::kAlertDialog);
    GetViewAccessibility().SetName(title);
    AddText(this, title, 13, gfx::Font::Weight::SEMIBOLD, kTitle);
    AddText(this,
            u"It can open tabs, read pages and click in the Agent Space. It "
            u"can’t see your other Spaces.",
            12, gfx::Font::Weight::NORMAL, kBody);
    auto* buttons = AddChildView(std::make_unique<views::BoxLayoutView>());
    buttons->SetBetweenChildSpacing(8);
    buttons->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
    buttons->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(6, 0, 0, 0));
    decline_ = AddButton(buttons, base::BindRepeating(answer, false),
                         u"Don’t allow", kGrayButton, kButtonText);
    allow_ = AddButton(buttons, base::BindRepeating(answer, true), u"Allow",
                       kBlueButton, SK_ColorWHITE);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  void Press(bool allow) {
    views::LabelButton* button = allow ? allow_ : decline_;
    button->button_controller()->NotifyClick();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kWidth, views::BoxLayoutView::CalculatePreferredSize(
                                 views::SizeBounds(kWidth, {}))
                                 .height());
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kFill);
    const gfx::RectF bounds(GetLocalBounds());
    canvas->DrawRoundRect(bounds, kRadius, flags);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(kStroke);
    gfx::RectF stroke = bounds;
    stroke.Inset(0.5f);
    canvas->DrawRoundRect(stroke, kRadius - 0.5f, flags);
  }

 private:
  raw_ptr<views::LabelButton> allow_ = nullptr;
  raw_ptr<views::LabelButton> decline_ = nullptr;
};

BEGIN_METADATA(PromptView)
END_METADATA

}  // namespace

ControlPrompt::ControlPrompt(views::Widget& parent,
                             const std::u16string& client,
                             Done done)
    : done_(std::move(done)) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.SetParent(&parent);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // Clicks reach the buttons without taking focus from the user's work.
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.name = "MyarcControlPrompt";
  widget_ = std::make_unique<views::Widget>(std::move(params));
  observation_.Observe(widget_.get());
  // The buttons answer after their own press has unwound.
  auto* contents = widget_->SetContentsView(std::make_unique<PromptView>(
      client, base::BindRepeating(
                  [](ControlPrompt* prompt, bool allow) {
                    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                        FROM_HERE,
                        base::BindOnce(&ControlPrompt::Finish,
                                       prompt->weak_factory_.GetWeakPtr(),
                                       allow ? Answer::kAllow
                                             : Answer::kDecline));
                  },
                  base::Unretained(this))));
  const gfx::Rect window = parent.GetClientAreaBoundsInScreen();
  const gfx::Size size = contents->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(window.right() - kRightMargin - size.width(),
                               window.y() + kTopMargin, size.width(),
                               size.height()));
  widget_->ShowInactive();
  contents->GetViewAccessibility().AnnounceText(
      client + u" wants to control your Agent Space");
}

ControlPrompt::~ControlPrompt() {
  observation_.Reset();
  widget_.reset();
}

void ControlPrompt::PressForTesting(bool allow) {
  if (widget_) {
    static_cast<PromptView*>(widget_->GetContentsView())->Press(allow);
  }
}

void ControlPrompt::Finish(Answer answer) {
  if (done_) {
    // May delete this.
    std::move(done_).Run(answer);
  }
}

void ControlPrompt::OnWidgetDestroying(views::Widget* widget) {
  observation_.Reset();
  if (done_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ControlPrompt::Finish,
                                  weak_factory_.GetWeakPtr(), Answer::kClosed));
  }
}

}  // namespace myarc
