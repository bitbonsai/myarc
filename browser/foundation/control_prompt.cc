#include "myarc/control_prompt.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "myarc/glass_mac.h"
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
#include "ui/gfx/skia_paint_util.h"
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
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

// Unified frosted-glass card using myarc::dialog tokens.
constexpr int kWidth = 360;
constexpr int kInset = dialog::kPadding;
constexpr int kTextWidth = kWidth - 2 * kInset;
constexpr float kRadius = dialog::kCornerRadius;
constexpr int kButtonHeight = dialog::kButtonHeight;
constexpr int kButtonRadius = static_cast<int>(dialog::kButtonCornerRadius);
constexpr int kButtonMinWidth = dialog::kButtonMinWidth;
constexpr int kButtonPad = 16;
// Spacing beside the sidebar next to the Agent Space footer button.
constexpr int kAnchorGap = 10;
constexpr int kWindowMargin = 14;
constexpr int kTopMargin = 19;
constexpr SkColor kFill = SkColorSetRGB(0x25, 0x25, 0x26);
constexpr SkColor kGlassFill = dialog::kDarkGlassTint;
constexpr SkColor kStroke = SkColorSetRGB(0x3A, 0x3A, 0x3B);
constexpr SkColor kRim =
    SkColorSetA(SK_ColorWHITE, static_cast<U8CPU>(dialog::kDarkRimAlpha * 255));
constexpr SkColor kTitle = SkColorSetRGB(0xF5, 0xF5, 0xF7);
constexpr SkColor kBody = SkColorSetRGB(0xD0, 0xD0, 0xD4);
constexpr SkColor kBlueButton = SkColorSetRGB(0x0A, 0x84, 0xFF);
constexpr SkColor kGrayButton = SkColorSetRGB(0x45, 0x45, 0x46);
constexpr SkColor kGrayButtonGlass = SkColorSetA(SK_ColorWHITE, 0x1A);
constexpr SkColor kButtonText = SkColorSetRGB(0xEC, 0xEC, 0xEF);

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
                              SkColor color,
                              gfx::Font::Weight weight = gfx::Font::Weight::MEDIUM) {
  auto* button = parent->AddChildView(
      std::make_unique<views::LabelButton>(std::move(callback), text));
  button->SetTextSubpixelRenderingEnabled(false);
  for (views::View* child : button->children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(Font(14, weight));
      label->SetSubpixelRenderingEnabled(false);
    }
  }
  button->SetEnabledTextColors(color);
  button->SetTextColor(views::Button::STATE_DISABLED, color);
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->SetBackground(
      views::CreateRoundedRectBackground(fill, kButtonRadius));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kButtonPad)));
  button->SetMinSize(gfx::Size(kButtonMinWidth, kButtonHeight));
  button->SetMaxSize(gfx::Size(0, kButtonHeight));
  return button;
}

class PromptView : public views::BoxLayoutView {
  METADATA_HEADER(PromptView, views::BoxLayoutView)

 public:
  PromptView(const ControlPrompt::Copy& copy,
             base::RepeatingCallback<void(bool)> answer) {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetInsideBorderInsets(gfx::Insets(kInset));
    SetBetweenChildSpacing(8);
    GetViewAccessibility().SetRole(ax::mojom::Role::kAlertDialog);
    GetViewAccessibility().SetName(copy.title);
    AddText(this, copy.title, 15, gfx::Font::Weight::SEMIBOLD, kTitle);
    AddText(this, copy.body, 12, gfx::Font::Weight::NORMAL, kBody);
    auto* buttons = AddChildView(std::make_unique<views::BoxLayoutView>());
    buttons->SetBetweenChildSpacing(8);
    buttons->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
    buttons->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(10, 0, 0, 0));
    decline_ = AddButton(
        buttons, base::BindRepeating(answer, false), copy.decline,
        ShouldReduceTransparency() ? kGrayButton : kGrayButtonGlass,
        kButtonText);
    allow_ = AddButton(buttons, base::BindRepeating(answer, true), copy.accept,
                       kBlueButton, SK_ColorWHITE, gfx::Font::Weight::SEMIBOLD);
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
    const bool glass = !ShouldReduceTransparency();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(glass ? kGlassFill : kFill);
    const gfx::RectF bounds(GetLocalBounds());
    canvas->DrawRoundRect(bounds, kRadius, flags);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(glass ? kRim : kStroke);
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

views::View* FindViewByClassName(views::View* root, std::string_view name) {
  if (!root) {
    return nullptr;
  }
  if (std::string_view(root->GetClassName()) == name && root->IsDrawn()) {
    return root;
  }
  for (views::View* child : root->children()) {
    if (views::View* found = FindViewByClassName(child, name)) {
      return found;
    }
  }
  return nullptr;
}

views::View* FindAgentFooterButton(views::View* root) {
  if (!root) {
    return nullptr;
  }
  if (std::string_view(root->GetClassName()) == "FooterButton" &&
      root->IsDrawn()) {
    const std::u16string tip = root->GetTooltipText();
    if (tip.find(u"Agent Space") != std::u16string::npos) {
      return root;
    }
  }
  for (views::View* child : root->children()) {
    if (views::View* found = FindAgentFooterButton(child)) {
      return found;
    }
  }
  return nullptr;
}

gfx::Rect ComputePromptBounds(views::Widget& parent, const gfx::Size& size) {
  const gfx::Rect window = parent.GetClientAreaBoundsInScreen();
  int x = window.x() + kWindowMargin;
  int y = window.bottom() - kWindowMargin - size.height();
  if (views::View* sidebar =
          FindViewByClassName(parent.GetRootView(), "SidebarView")) {
    const gfx::Rect sb = sidebar->GetBoundsInScreen();
    if (!sb.IsEmpty()) {
      x = sb.right() + kAnchorGap;
      y = sb.bottom() - kWindowMargin - size.height();
      if (views::View* agent = FindAgentFooterButton(sidebar)) {
        const gfx::Rect ab = agent->GetBoundsInScreen();
        if (!ab.IsEmpty()) {
          y = ab.bottom() - size.height();
        }
      }
    }
  } else if (views::View* bar = FindViewByClassName(parent.GetRootView(),
                                                    "LocationBarView")) {
    const gfx::Rect anchor = bar->GetBoundsInScreen();
    if (!anchor.IsEmpty()) {
      x = anchor.x();
      y = anchor.bottom() + kAnchorGap;
    }
  }
  const int max_x = std::max(window.x() + kWindowMargin,
                             window.right() - kWindowMargin - size.width());
  const int max_y = std::max(window.y() + kTopMargin,
                             window.bottom() - kWindowMargin - size.height());
  x = std::clamp(x, window.x() + kWindowMargin, max_x);
  y = std::clamp(y, window.y() + kTopMargin, max_y);
  return gfx::Rect(x, y, size.width(), size.height());
}

}  // namespace

ControlPrompt::ControlPrompt(views::Widget& parent,
                             const std::u16string& client,
                             Done done)
    : ControlPrompt(
          parent,
          Copy{
              .title = client + u" wants to control your Agent Space",
              .body = u"It can open tabs, read pages and click in the Agent "
                      u"Space. It can’t see your other Spaces.",
              .decline = u"Don’t allow",
              .accept = u"Allow",
          },
          std::move(done)) {}

ControlPrompt::ControlPrompt(views::Widget& parent, const Copy& copy, Done done)
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
      copy, base::BindRepeating(
                [](ControlPrompt* prompt, bool allow) {
                  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                      FROM_HERE,
                      base::BindOnce(
                          &ControlPrompt::Finish,
                          prompt->weak_factory_.GetWeakPtr(),
                          allow ? Answer::kAllow : Answer::kDecline));
                },
                base::Unretained(this))));
  const gfx::Size size = contents->GetPreferredSize();
  widget_->SetBounds(ComputePromptBounds(parent, size));
  ApplyWidgetGlass(*widget_, GlassMaterial::kHud, kRadius, /*dark=*/true);
  widget_->ShowInactive();
  contents->GetViewAccessibility().AnnounceText(copy.title);
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
