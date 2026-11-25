#include "myarc/media_lightbox.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "myarc/icons.h"
#include "myarc/library_logic.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;
using MainAxis = views::BoxLayout::MainAxisAlignment;
using CrossAxis = views::BoxLayout::CrossAxisAlignment;

constexpr SkColor kScrimColor = SkColorSetA(SK_ColorBLACK, 0xB8);
constexpr SkColor kHeaderBg = SkColorSetRGB(0x26, 0x26, 0x2B);
constexpr SkColor kHeaderBorder = SkColorSetA(SK_ColorWHITE, 0x1A);
constexpr SkColor kButtonFill = SkColorSetA(SK_ColorWHITE, 0x18);
constexpr SkColor kMutedText = SkColorSetA(SK_ColorWHITE, 0x99);
constexpr int kHeaderHeight = 44;

enum LightboxCommand {
  kCopyFile = 1,
  kRevealInFinder,
  kHideMedia,
};

std::unique_ptr<views::ImageButton> MakeHeaderIconButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    const std::u16string& label,
    int icon_size = 15) {
  auto button = std::make_unique<views::ImageButton>(std::move(callback));
  button->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(icon, SK_ColorWHITE,
                                                       icon_size));
  button->SetImageModel(
      views::Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(icon, SkColorSetA(SK_ColorWHITE, 0x44),
                                     icon_size));
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetPreferredSize(gfx::Size(28, 28));
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetTooltipText(label);
  button->GetViewAccessibility().SetName(label);
  views::InstallRoundRectHighlightPathGenerator(button.get(), gfx::Insets(), 6);
  return button;
}

}  // namespace

class MediaLightbox::CanvasView final : public views::View {
  METADATA_HEADER(CanvasView, views::View)

 public:
  CanvasView(base::RepeatingClosure on_backdrop_click,
             base::RepeatingClosure on_play_click)
      : on_backdrop_click_(std::move(on_backdrop_click)),
        on_play_click_(std::move(on_play_click)) {}

  void SetMedia(gfx::ImageSkia image, bool is_video) {
    image_ = std::move(image);
    is_video_ = is_video;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    image_rect_ = gfx::Rect();
    if (image_.isNull()) {
      return;
    }
    gfx::Rect avail = GetLocalBounds();
    avail.Inset(gfx::Insets(32));
    if (avail.IsEmpty()) {
      return;
    }
    const gfx::Size src = image_.size();
    if (src.IsEmpty()) {
      return;
    }
    const float scale = std::min(
        static_cast<float>(avail.width()) / static_cast<float>(src.width()),
        static_cast<float>(avail.height()) / static_cast<float>(src.height()));
    const int w = std::max(1, static_cast<int>(std::lround(src.width() * scale)));
    const int h = std::max(1, static_cast<int>(std::lround(src.height() * scale)));
    image_rect_ = gfx::Rect(avail.x() + (avail.width() - w) / 2,
                            avail.y() + (avail.height() - h) / 2, w, h);

    // Subtle elevation frame behind the preview image.
    cc::PaintFlags shadow;
    shadow.setAntiAlias(true);
    shadow.setColor(SkColorSetA(SK_ColorBLACK, 0x55));
    gfx::RectF shadow_rect(image_rect_);
    shadow_rect.Offset(0.f, 4.f);
    canvas->DrawRoundRect(shadow_rect, 6.f, shadow);

    canvas->DrawImageInt(image_, 0, 0, src.width(), src.height(),
                         image_rect_.x(), image_rect_.y(), image_rect_.width(),
                         image_rect_.height(), true);

    if (is_video_) {
      const gfx::PointF center(image_rect_.CenterPoint());
      cc::PaintFlags circle;
      circle.setAntiAlias(true);
      circle.setColor(SkColorSetA(SK_ColorBLACK, 0xA8));
      canvas->DrawCircle(center, 28.f, circle);

      SkPath triangle = SkPathBuilder()
                            .moveTo(center.x() - 8.f, center.y() - 12.f)
                            .lineTo(center.x() + 14.f, center.y())
                            .lineTo(center.x() - 8.f, center.y() + 12.f)
                            .close()
                            .detach();
      cc::PaintFlags play;
      play.setAntiAlias(true);
      play.setColor(SK_ColorWHITE);
      canvas->DrawPath(triangle, play);
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (!event.IsOnlyLeftMouseButton()) {
      return false;
    }
    if (is_video_ && image_rect_.Contains(event.location())) {
      on_play_click_.Run();
      return true;
    }
    if (!image_rect_.Contains(event.location())) {
      on_backdrop_click_.Run();
      return true;
    }
    return true;
  }

 private:
  base::RepeatingClosure on_backdrop_click_;
  base::RepeatingClosure on_play_click_;
  gfx::ImageSkia image_;
  bool is_video_ = false;
  gfx::Rect image_rect_;
};

BEGIN_METADATA(MediaLightbox, CanvasView)
END_METADATA

class MediaLightbox::RootView final : public views::View {
  METADATA_HEADER(RootView, views::View)

 public:
  explicit RootView(MediaLightbox* owner) : owner_(owner) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetBackground(views::CreateSolidBackground(kScrimColor));
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kVertical, gfx::Insets(), 0));
    GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().SetName(u"Media preview");
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
    AddAccelerator(ui::Accelerator(ui::VKEY_LEFT, ui::EF_NONE));
    AddAccelerator(ui::Accelerator(ui::VKEY_RIGHT, ui::EF_NONE));
  }

  void ClearOwner() { owner_ = nullptr; }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (!owner_) {
      return false;
    }
    switch (accelerator.key_code()) {
      case ui::VKEY_ESCAPE:
        owner_->Close();
        return true;
      case ui::VKEY_LEFT:
        owner_->Step(-1);
        return true;
      case ui::VKEY_RIGHT:
        owner_->Step(1);
        return true;
      default:
        return false;
    }
  }

 private:
  raw_ptr<MediaLightbox> owner_ = nullptr;
};

BEGIN_METADATA(MediaLightbox, RootView)
END_METADATA

MediaLightbox::MediaLightbox(
    views::Widget& parent,
    SidebarView::Delegate& delegate,
    std::vector<SidebarView::MediaItemPresentation> items,
    size_t index,
    base::OnceClosure on_closed)
    : delegate_(delegate),
      items_(std::move(items)),
      index_(items_.empty() ? 0 : std::min(index, items_.size() - 1)),
      on_closed_(std::move(on_closed)) {
  if (items_.empty()) {
    return;
  }

  auto root = std::make_unique<RootView>(this);
  auto* root_layout = static_cast<views::BoxLayout*>(root->GetLayoutManager());

  // Header bar (44 DIP tall, elevated dark surface).
  auto* header = root->AddChildView(std::make_unique<views::View>());
  header->SetBackground(views::CreateSolidBackground(kHeaderBg));
  header->SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 1, 0), kHeaderBorder));
  header->SetPreferredSize(gfx::Size(0, kHeaderHeight));
  auto* header_layout =
      header->SetLayoutManager(std::make_unique<views::BoxLayout>(
          Orientation::kHorizontal, gfx::Insets::VH(0, 12), 6));
  header_layout->set_cross_axis_alignment(CrossAxis::kCenter);

  header->AddChildView(MakeHeaderIconButton(
      base::BindRepeating(&MediaLightbox::Close, base::Unretained(this)),
      kPeekCloseIcon, u"Close preview", 14));
  prev_button_ = header->AddChildView(MakeHeaderIconButton(
      base::BindRepeating(&MediaLightbox::Step, base::Unretained(this), -1),
      kArrowLeftIcon, u"Previous media", 15));
  next_button_ = header->AddChildView(MakeHeaderIconButton(
      base::BindRepeating(&MediaLightbox::Step, base::Unretained(this), 1),
      kChevronRightIcon, u"Next media", 15));

  auto filename = std::make_unique<views::Label>(
      std::u16string(),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  filename->SetFontList(
      gfx::FontList().Derive(0, gfx::Font::NORMAL, gfx::Font::Weight::SEMIBOLD));
  filename->SetEnabledColor(SK_ColorWHITE);
  filename->SetBackgroundColor(SK_ColorTRANSPARENT);
  filename->SetAutoColorReadabilityEnabled(false);
  filename->SetElideBehavior(gfx::ELIDE_TAIL);
  filename->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  filename->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 8, 0, 4));
  filename_label_ = header->AddChildView(std::move(filename));

  auto age = std::make_unique<views::Label>(std::u16string());
  age->SetFontList(
      gfx::FontList().Derive(-1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  age->SetEnabledColor(kMutedText);
  age->SetBackgroundColor(SK_ColorTRANSPARENT);
  age->SetAutoColorReadabilityEnabled(false);
  age->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  age_label_ = header->AddChildView(std::move(age));

  auto* spacer = header->AddChildView(std::make_unique<views::View>());
  header_layout->SetFlexForView(spacer, 1);

  more_button_ = header->AddChildView(MakeHeaderIconButton(
      base::BindRepeating(
          [](MediaLightbox* self) { self->ShowMoreMenu(self->more_button_); },
          base::Unretained(this)),
      kSpaceMoreIcon, u"More actions", 16));

  auto preview = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&MediaLightbox::OpenCurrentInPreview,
                          base::Unretained(this)),
      u"Open in Preview \u2197");
  preview->SetStyle(ui::ButtonStyle::kText);
  preview->SetBgColorOverrideDeprecated(kButtonFill);
  preview->SetEnabledTextColors(SK_ColorWHITE);
  preview->SetCornerRadius(6);
  preview->SetCustomPadding(gfx::Insets::VH(5, 10));
  preview->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  preview->SetTooltipText(u"Open in Preview");
  preview_button_ = header->AddChildView(std::move(preview));

  canvas_ = root->AddChildView(std::make_unique<CanvasView>(
      base::BindRepeating(&MediaLightbox::Close, base::Unretained(this)),
      base::BindRepeating(&MediaLightbox::OpenCurrentInPreview,
                          base::Unretained(this))));
  root_layout->SetFlexForView(canvas_, 1);

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_CONTROL);
  params.parent = parent.GetNativeView();
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = parent.GetWindowBoundsInScreen();

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  widget_->SetContentsView(std::move(root));
  UpdateCurrent();
  widget_->Show();
  widget_->GetContentsView()->RequestFocus();
}

MediaLightbox::~MediaLightbox() {
  closing_ = true;
  menu_runner_.reset();
  menu_model_.reset();
  if (widget_) {
    if (auto* root = static_cast<RootView*>(widget_->GetContentsView())) {
      root->ClearOwner();
    }
    if (!widget_->IsClosed()) {
      widget_->CloseNow();
    }
  }
}

void MediaLightbox::Close() {
  if (closing_) {
    return;
  }
  closing_ = true;
  menu_runner_.reset();
  if (widget_ && !widget_->IsClosed()) {
    widget_->Close();
  }
}

void MediaLightbox::Step(int delta) {
  if (items_.size() <= 1) {
    return;
  }
  const int next = static_cast<int>(index_) + delta;
  if (next < 0 || next >= static_cast<int>(items_.size())) {
    return;
  }
  index_ = static_cast<size_t>(next);
  UpdateCurrent();
}

void MediaLightbox::ShowMoreMenu(views::View* anchor) {
  if (!widget_ || !anchor || (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItem(kCopyFile, u"Copy File");
  menu_model_->AddItem(kRevealInFinder, u"Reveal in Finder");
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(kHideMedia, u"Hide Media in Library");
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  const gfx::Rect bounds = anchor->GetBoundsInScreen();
  menu_runner_->RunMenuAt(widget_.get(), nullptr,
                          gfx::Rect(bounds.bottom_left(), gfx::Size()),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kMouse);
}

void MediaLightbox::OpenCurrentInPreview() {
  if (index_ < items_.size()) {
    delegate_->OpenMediaInPreview(items_[index_].id);
  }
}

void MediaLightbox::OnWidgetClosing() {
  if (on_closed_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_closed_));
  }
}

void MediaLightbox::ExecuteCommand(int command_id, int event_flags) {
  if (index_ >= items_.size()) {
    return;
  }
  const uint64_t id = items_[index_].id;
  switch (command_id) {
    case kCopyFile:
      delegate_->CopyMediaFile(id);
      break;
    case kRevealInFinder:
      delegate_->RevealMediaInFinder(id);
      break;
    case kHideMedia:
      delegate_->SetMediaLibraryEnabled(false);
      Close();
      break;
  }
}

void MediaLightbox::UpdateCurrent() {
  if (index_ >= items_.size()) {
    return;
  }
  const auto& item = items_[index_];
  filename_label_->SetText(item.filename);
  age_label_->SetText(library::AgeLabel(item.modified, base::Time::Now()));
  prev_button_->SetEnabled(index_ > 0);
  next_button_->SetEnabled(index_ + 1 < items_.size());

  if (auto it = previews_.find(item.id); it != previews_.end()) {
    canvas_->SetMedia(it->second, item.is_video);
  } else {
    gfx::ImageSkia thumb;
    if (item.thumbnail.IsImage()) {
      thumb = item.thumbnail.GetImage().AsImageSkia();
    }
    canvas_->SetMedia(thumb, item.is_video);
    RequestPreviewForIndex(index_, /*apply_if_current=*/true);
  }

  // Pre-decode immediate neighbors for instant < > stepping.
  if (index_ > 0) {
    RequestPreviewForIndex(index_ - 1, /*apply_if_current=*/false);
  }
  if (index_ + 1 < items_.size()) {
    RequestPreviewForIndex(index_ + 1, /*apply_if_current=*/false);
  }
}

void MediaLightbox::RequestPreviewForIndex(size_t index,
                                           bool apply_if_current) {
  if (index >= items_.size()) {
    return;
  }
  const uint64_t id = items_[index].id;
  if (previews_.contains(id)) {
    return;
  }
  delegate_->RequestMediaPreview(
      id, base::BindOnce(&MediaLightbox::OnPreviewDecoded,
                         weak_factory_.GetWeakPtr(), id, apply_if_current));
}

void MediaLightbox::OnPreviewDecoded(uint64_t id,
                                     bool apply_if_current,
                                     gfx::ImageSkia image) {
  if (image.isNull()) {
    return;
  }
  previews_[id] = image;
  if (apply_if_current && index_ < items_.size() && items_[index_].id == id &&
      canvas_) {
    canvas_->SetMedia(image, items_[index_].is_video);
  }
}

}  // namespace myarc
