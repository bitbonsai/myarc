#include "myarc/downloads_stack.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/library_logic.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"

namespace myarc {
namespace {

// Measured from Arc's stack (DIP): contiguous 56 tall rows, a 40x40 rounded
// square thumbnail (radius 8) at the sidebar's inset, and text 12 after it.
// The name's cap top sits 13.6 above the row's center and the age's 17 below.
constexpr int kRowHeight = 56;
constexpr int kThumbSize = 40;
constexpr float kThumbRadius = 8.0f;
constexpr int kIconInset = 2;
constexpr int kTextLeft = kThumbSize + 12;
constexpr float kNameCapTop = kRowHeight / 2.f - 13.6f;
constexpr int kAgeCapOffset = 17;
constexpr int kNameSize = 13;
constexpr int kAgeSize = 11;
constexpr int kBadgeExtSize = 8;
constexpr SkAlpha kNameAlpha = 0xE0;
constexpr SkAlpha kAgeAlpha = 0x8A;

constexpr base::TimeDelta kShowDelay = base::Milliseconds(100);
constexpr base::TimeDelta kHideDelay = base::Milliseconds(350);
constexpr base::TimeDelta kShowDuration = base::Milliseconds(150);
constexpr base::TimeDelta kHideDuration = base::Milliseconds(120);
constexpr base::TimeDelta kHoverPoll = base::Milliseconds(100);
// Each row up starts fading this much later, as a fraction of the show.
constexpr float kRowStagger = 0.07f;

gfx::FontList FontOfSize(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.Derive(size - base.GetFontSize(), gfx::Font::NORMAL, weight);
}

std::u16string UpperExtension(std::u16string_view name) {
  const size_t dot = name.rfind(u'.');
  if (dot == std::u16string_view::npos || dot + 1 >= name.size()) {
    return {};
  }
  const std::u16string_view raw = name.substr(dot + 1);
  if (raw.size() > 4) {
    return {};
  }
  std::u16string out;
  out.reserve(raw.size());
  for (char16_t ch : raw) {
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch)) {
      return {};
    }
    out.push_back(base::ToUpperASCII(ch));
  }
  return out;
}

bool IsVideoExtension(std::u16string_view ext) {
  return ext == u"MP4" || ext == u"MOV" || ext == u"WEBM" || ext == u"M4V" ||
         ext == u"MKV" || ext == u"AVI";
}

// Draws a clean video-camera glyph inside `rect` for video file fallbacks.
void DrawVideoGlyph(gfx::Canvas* canvas,
                    const gfx::RectF& rect,
                    SkColor color) {
  cc::PaintFlags stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(cc::PaintFlags::kStroke_Style);
  stroke.setStrokeWidth(1.4f);
  stroke.setStrokeJoin(cc::PaintFlags::kRound_Join);
  stroke.setColor(color);

  const float body_w = rect.width() * 0.64f;
  const float body_h = rect.height() * 0.68f;
  const float body_y = rect.y() + (rect.height() - body_h) / 2.f;
  canvas->DrawRoundRect(gfx::RectF(rect.x(), body_y, body_w, body_h), 2.5f,
                        stroke);

  const float lens_x = rect.x() + body_w + 1.5f;
  const float lens_right = rect.right();
  const float mid_y = rect.CenterPoint().y();
  SkPath lens = SkPathBuilder()
                    .moveTo(lens_x, mid_y - body_h * 0.22f)
                    .lineTo(lens_right, mid_y - body_h * 0.42f)
                    .lineTo(lens_right, mid_y + body_h * 0.42f)
                    .lineTo(lens_x, mid_y + body_h * 0.22f)
                    .close()
                    .detach();
  canvas->DrawPath(lens, stroke);
}

}  // namespace

// One download: its icon, name and age, painted at the stack's opacity.
class DownloadsStack::Row : public views::Button {
  METADATA_HEADER(Row, views::Button)

 public:
  Row(DownloadsStack& stack, const SidebarView::DownloadPresentation& download)
      : views::Button(base::BindRepeating(&DownloadsStack::OpenRow,
                                          base::Unretained(&stack),
                                          download.id)),
        stack_(stack),
        download_(download),
        name_font_(FontOfSize(kNameSize, gfx::Font::Weight::MEDIUM)),
        age_font_(FontOfSize(kAgeSize, gfx::Font::Weight::NORMAL)),
        ext_font_(FontOfSize(kBadgeExtSize, gfx::Font::Weight::MEDIUM)) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    // Shown only on hover; keyboard users have the Library's Downloads.
    SetFocusBehavior(FocusBehavior::NEVER);
    Update(download);
  }

  uint32_t id() const { return download_.id; }
  const std::u16string& name() const { return download_.name; }

  // Takes a newer presentation of the same download.
  void Update(const SidebarView::DownloadPresentation& download) {
    DCHECK_EQ(download.id, download_.id);
    download_ = download;
    age_ = library::AgeLabel(download_.time, base::Time::Now());
    GetViewAccessibility().SetName(download_.name + u", " + age_);
    SchedulePaint();
  }

  void SetMediaThumbnail(ui::ImageModel thumbnail, bool is_video) {
    if (media_thumbnail_ == thumbnail && media_is_video_ == is_video) {
      return;
    }
    media_thumbnail_ = std::move(thumbnail);
    media_is_video_ = is_video;
    SchedulePaint();
  }

  void SetAlpha(float alpha) {
    const SkAlpha value = static_cast<SkAlpha>(alpha * 0xFF + 0.5f);
    if (value != alpha_) {
      alpha_ = value;
      SchedulePaint();
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    press_point_ = event.location();
    dragging_ = false;
    return views::Button::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (!dragging_ && event.IsOnlyLeftMouseButton() &&
        ExceededDragThreshold(event.location() - press_point_)) {
      dragging_ = true;
      // The drag can delete this row.
      stack_->DragRow(download_.id);
      return true;
    }
    return views::Button::OnMouseDragged(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    const bool was_drag = dragging_;
    dragging_ = false;
    if (was_drag) {
      SetState(STATE_NORMAL);
      return;
    }
    views::Button::OnMouseReleased(event);
  }

  void OnMouseCaptureLost() override {
    // A file drag takes the capture, and the pointer may leave without an
    // exit event.
    stack_->RecheckHover();
    views::Button::OnMouseCaptureLost();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (!alpha_) {
      return;
    }
    const ui::ColorProvider* provider = GetColorProvider();
    const bool is_dark =
        !provider ||
        color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
    const SkColor ink = is_dark ? SK_ColorWHITE : SK_ColorBLACK;
    gfx::ScopedCanvas scoped(canvas);
    canvas->SaveLayerAlpha(alpha_);

    const gfx::Rect thumb(0, (height() - kThumbSize) / 2, kThumbSize,
                          kThumbSize);
    const gfx::RectF thumb_f(thumb);
    const std::u16string ext = UpperExtension(download_.name);
    const bool is_video = media_is_video_ || IsVideoExtension(ext);

    // Crisp white/light rounded-square backing so dark thumbnails and file
    // icons pop cleanly against the dark sidebar.
    cc::PaintFlags card_bg;
    card_bg.setAntiAlias(true);
    card_bg.setColor(is_dark ? SkColorSetRGB(0xF5, 0xF5, 0xF7) : SK_ColorWHITE);
    canvas->DrawRoundRect(thumb_f, kThumbRadius, card_bg);

    const gfx::ImageSkia media_image =
        media_thumbnail_.IsEmpty() ? gfx::ImageSkia()
                                   : media_thumbnail_.Rasterize(provider);
    const gfx::ImageSkia icon_image =
        media_image.isNull() && !download_.icon.IsEmpty()
            ? download_.icon.Rasterize(provider)
            : gfx::ImageSkia();

    {
      gfx::ScopedCanvas clip_scope(canvas);
      SkPath clip =
          SkPathBuilder()
              .addRRect(SkRRect::MakeRectXY(
                  SkRect::MakeXYWH(thumb_f.x(), thumb_f.y(), thumb_f.width(),
                                   thumb_f.height()),
                  kThumbRadius, kThumbRadius))
              .detach();
      canvas->ClipPath(clip, true);

      if (!media_image.isNull()) {
        const float scale = std::min(
            static_cast<float>(thumb.width()) / std::max(1, media_image.width()),
            static_cast<float>(thumb.height()) /
                std::max(1, media_image.height()));
        const int draw_w =
            std::max(1, static_cast<int>(media_image.width() * scale + 0.5f));
        const int draw_h =
            std::max(1, static_cast<int>(media_image.height() * scale + 0.5f));
        canvas->DrawImageInt(
            media_image, 0, 0, media_image.width(), media_image.height(),
            thumb.CenterPoint().x() - draw_w / 2,
            thumb.CenterPoint().y() - draw_h / 2, draw_w, draw_h, true);
        if (is_video) {
          const gfx::PointF center(thumb.CenterPoint());
          cc::PaintFlags badge;
          badge.setAntiAlias(true);
          badge.setColor(SkColorSetA(SK_ColorBLACK, 0x7A));
          canvas->DrawCircle(center, 9.5f, badge);
          SkPath tri = SkPathBuilder()
                           .moveTo(center.x() - 2.5f, center.y() - 4.0f)
                           .lineTo(center.x() + 4.5f, center.y())
                           .lineTo(center.x() - 2.5f, center.y() + 4.0f)
                           .close()
                           .detach();
          cc::PaintFlags play;
          play.setAntiAlias(true);
          play.setColor(SK_ColorWHITE);
          canvas->DrawPath(tri, play);
        }
      } else if (!icon_image.isNull()) {
        gfx::Rect fit = thumb;
        fit.Inset(kIconInset);
        const float scale = std::min(
            static_cast<float>(fit.width()) / std::max(1, icon_image.width()),
            static_cast<float>(fit.height()) /
                std::max(1, icon_image.height()));
        const int draw_w =
            std::max(1, static_cast<int>(icon_image.width() * scale + 0.5f));
        const int draw_h =
            std::max(1, static_cast<int>(icon_image.height() * scale + 0.5f));
        canvas->DrawImageInt(icon_image, 0, 0, icon_image.width(),
                             icon_image.height(),
                             fit.CenterPoint().x() - draw_w / 2,
                             fit.CenterPoint().y() - draw_h / 2, draw_w, draw_h,
                             true);
      } else {
        // Fallback file-type icon inside the light rounded-square card.
        const SkColor glyph_color = SkColorSetRGB(0x8E, 0x8E, 0x93);
        const int glyph_bottom = ext.empty() ? thumb.bottom() - 8
                                             : thumb.bottom() - 13;
        if (is_video) {
          DrawVideoGlyph(
              canvas,
              gfx::RectF(thumb.x() + 10.f, thumb.y() + 8.f, 20.f,
                         std::max(12.f, glyph_bottom - (thumb.y() + 8.f))),
              glyph_color);
        } else {
          const gfx::ImageSkia file_icon = gfx::CreateVectorIcon(
              vector_icons::kInsertDriveFileOutlineOldIcon, 20, glyph_color);
          canvas->DrawImageInt(file_icon,
                               thumb.CenterPoint().x() - file_icon.width() / 2,
                               thumb.y() + (ext.empty() ? 10 : 6));
        }
        if (!ext.empty()) {
          canvas->DrawStringRectWithFlags(
              ext, ext_font_, SkColorSetRGB(0x6E, 0x6E, 0x73),
              gfx::Rect(thumb.x() + 2, thumb.bottom() - 13, thumb.width() - 4,
                        11),
              gfx::Canvas::TEXT_ALIGN_CENTER |
                  gfx::Canvas::NO_SUBPIXEL_RENDERING);
        }
      }
    }

    // Subtle hairline border so the rounded-square card reads crisply.
    cc::PaintFlags card_border;
    card_border.setAntiAlias(true);
    card_border.setStyle(cc::PaintFlags::kStroke_Style);
    card_border.setStrokeWidth(1.0f);
    card_border.setColor(is_dark ? SkColorSetA(SK_ColorWHITE, 0x22)
                                 : SkColorSetA(SK_ColorBLACK, 0x16));
    gfx::RectF border_rect = thumb_f;
    border_rect.Inset(0.5f);
    canvas->DrawRoundRect(border_rect, kThumbRadius - 0.5f, card_border);

    constexpr int kTextFlags = gfx::Canvas::NO_SUBPIXEL_RENDERING;
    const int text_width = std::max(0, width() - kTextLeft);
    const int name_baseline =
        static_cast<int>(kNameCapTop + name_font_.GetCapHeight() + 0.5f);
    const int age_baseline = static_cast<int>(kNameCapTop + kAgeCapOffset +
                                              age_font_.GetCapHeight() + 0.5f);
    const std::u16string elided_name = gfx::ElideText(
        download_.name, name_font_, static_cast<float>(text_width),
        gfx::ELIDE_TAIL);
    const std::u16string elided_age = gfx::ElideText(
        age_, age_font_, static_cast<float>(text_width), gfx::ELIDE_TAIL);
    const SkAlpha name_alpha =
        GetState() == STATE_HOVERED ? 0xFF : kNameAlpha;
    canvas->DrawStringRectWithFlags(
        elided_name, name_font_, SkColorSetA(ink, name_alpha),
        gfx::Rect(kTextLeft, name_baseline - name_font_.GetBaseline(),
                  text_width, name_font_.GetHeight()),
        kTextFlags);
    canvas->DrawStringRectWithFlags(
        elided_age, age_font_, SkColorSetA(ink, kAgeAlpha),
        gfx::Rect(kTextLeft, age_baseline - age_font_.GetBaseline(), text_width,
                  age_font_.GetHeight()),
        kTextFlags);
  }

 private:
  const raw_ref<DownloadsStack> stack_;
  SidebarView::DownloadPresentation download_;
  ui::ImageModel media_thumbnail_;
  bool media_is_video_ = false;
  std::u16string age_;
  const gfx::FontList name_font_;
  const gfx::FontList age_font_;
  const gfx::FontList ext_font_;
  SkAlpha alpha_ = 0;
  gfx::Point press_point_;
  bool dragging_ = false;
};

BEGIN_METADATA(DownloadsStack, Row)
END_METADATA

DownloadsStack::DownloadsStack(SidebarView::Delegate& delegate)
    : views::AnimationDelegateViews(this), delegate_(delegate) {
  // A layer keeps the rows above the sidebar's scrolling rows, which have
  // their own.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetNotifyEnterExitOnChild(true);
  SetVisible(false);
  animation_.SetTweenType(gfx::Tween::LINEAR);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Recent downloads");
}

DownloadsStack::~DownloadsStack() = default;

void DownloadsStack::SetAnchorHovered(bool hovered) {
  anchor_hovered_ = hovered;
  UpdateHover();
}

void DownloadsStack::HideNow() {
  show_delay_.Stop();
  hide_delay_.Stop();
  hover_poll_.Stop();
  hold_ = false;
  shown_ = false;
  hovered_ = false;
  animation_.Reset(0);
  SetVisible(false);
  RemoveAllChildViews();
  SetWatching(false);
}

void DownloadsStack::ShowNow() {
  HideNow();
  if (!LoadRows()) {
    return;
  }
  hold_ = true;
  shown_ = true;
  animation_.Reset(1);
  SetVisible(true);
  SetWatching(true);
  LayoutRows();
}

void DownloadsStack::Refresh() {
  if (!shown_) {
    return;
  }
  const auto rows = library::StackRows(delegate_->GetRecentDownloads());
  if (rows.empty()) {
    HideNow();
    return;
  }
  const bool same_downloads = std::ranges::equal(
      rows, children(), {}, &SidebarView::DownloadPresentation::id,
      [](const views::View* row) { return static_cast<const Row*>(row)->id(); });
  if (!same_downloads) {
    SetRows(rows);
    LayoutRows();
    return;
  }
  for (size_t i = 0; i < rows.size(); ++i) {
    static_cast<Row*>(children()[i])->Update(rows[i]);
  }
  ApplyMediaThumbnails();
}

void DownloadsStack::UpdateHover() {
  if (hold_) {
    return;
  }
  if (anchor_hovered_ || hovered_) {
    hide_delay_.Stop();
    if (!shown_ && !show_delay_.IsRunning()) {
      show_delay_.Start(
          FROM_HERE, kShowDelay,
          base::BindOnce(&DownloadsStack::Show, base::Unretained(this)));
    }
    return;
  }
  show_delay_.Stop();
  if (shown_ && !hide_delay_.IsRunning()) {
    hide_delay_.Start(
        FROM_HERE, kHideDelay,
        base::BindOnce(&DownloadsStack::Hide, base::Unretained(this)));
  }
}

void DownloadsStack::RecheckHover() {
  hovered_ = GetVisible() &&
             GetBoundsInScreen().Contains(
                 display::Screen::Get()->GetCursorScreenPoint());
  if (!hovered_) {
    hover_poll_.Stop();
  } else if (!hover_poll_.IsRunning()) {
    hover_poll_.Start(FROM_HERE, kHoverPoll,
                      base::BindRepeating(&DownloadsStack::RecheckHover,
                                          base::Unretained(this)));
  }
  UpdateHover();
}

void DownloadsStack::Show() {
  // Already up while fading out: keep the rows and reverse.
  if (!GetVisible() && !LoadRows()) {
    return;
  }
  shown_ = true;
  SetVisible(true);
  SetWatching(true);
  if (gfx::Animation::PrefersReducedMotion()) {
    animation_.Reset(1);
  } else {
    animation_.SetSlideDuration(kShowDuration);
    animation_.Show();
  }
  LayoutRows();
}

void DownloadsStack::Hide() {
  shown_ = false;
  if (gfx::Animation::PrefersReducedMotion()) {
    HideNow();
    return;
  }
  animation_.SetSlideDuration(kHideDuration);
  animation_.Hide();
}

bool DownloadsStack::LoadRows() {
  SetRows(library::StackRows(delegate_->GetRecentDownloads()));
  return !children().empty();
}

void DownloadsStack::SetRows(
    const std::vector<SidebarView::DownloadPresentation>& rows) {
  RemoveAllChildViews();
  for (const auto& download : rows) {
    AddChildView(std::make_unique<Row>(*this, download));
  }
  ApplyMediaThumbnails();
  PreferredSizeChanged();
}

void DownloadsStack::ApplyMediaThumbnails() {
  if (children().empty() || !delegate_->IsMediaLibraryEnabled()) {
    return;
  }
  std::map<std::u16string, std::pair<ui::ImageModel, bool>> by_name;
  for (const auto& item : delegate_->GetMediaItems()) {
    if (!item.thumbnail.IsEmpty()) {
      by_name.try_emplace(item.filename, item.thumbnail, item.is_video);
    }
  }
  if (by_name.empty()) {
    return;
  }
  for (views::View* child : children()) {
    auto* row = static_cast<Row*>(child);
    if (auto it = by_name.find(row->name()); it != by_name.end()) {
      row->SetMediaThumbnail(it->second.first, it->second.second);
    }
  }
}

void DownloadsStack::SetWatching(bool watching) {
  if (watching_ != watching) {
    watching_ = watching;
    delegate_->SetDownloadsStackShown(watching);
  }
}

void DownloadsStack::OpenRow(uint32_t id) {
  delegate_->OpenDownload(id);
}

void DownloadsStack::DragRow(uint32_t id) {
  const base::WeakPtr<DownloadsStack> weak = weak_factory_.GetWeakPtr();
  delegate_->DragDownload(id);
  if (weak) {
    RecheckHover();
  }
}

void DownloadsStack::LayoutRows() {
  // Rows fan up from the button: each row's center moves from the stack's
  // bottom to its place on the footer's rise curve, the bottom row first.
  static const gfx::CubicBezier rise(0.22, 1, 0.36, 1);
  const float level = animation_.GetCurrentValue();
  const float spread = rise.Solve(level);
  const int count = static_cast<int>(children().size());
  const float last_delay = kRowStagger * std::max(count - 1, 0);
  for (int i = 0; i < count; ++i) {
    const int center = i * kRowHeight + kRowHeight / 2;
    const float y = height() + (center - height()) * spread - kRowHeight / 2.f;
    auto* row = static_cast<Row*>(children()[i]);
    row->SetBounds(0, static_cast<int>(y + 0.5f), width(), kRowHeight);
    const float delay = kRowStagger * (count - 1 - i);
    row->SetAlpha(std::clamp((level - delay) / (1 - last_delay), 0.f, 1.f));
  }
}

void DownloadsStack::Layout(PassKey) {
  LayoutRows();
}

gfx::Size DownloadsStack::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(0, static_cast<int>(children().size()) * kRowHeight);
}

void DownloadsStack::OnMouseEntered(const ui::MouseEvent& event) {
  hovered_ = true;
  UpdateHover();
}

void DownloadsStack::OnMouseExited(const ui::MouseEvent& event) {
  hover_poll_.Stop();
  hovered_ = false;
  UpdateHover();
}

void DownloadsStack::AnimationProgressed(const gfx::Animation* animation) {
  LayoutRows();
}

void DownloadsStack::AnimationEnded(const gfx::Animation* animation) {
  LayoutRows();
  if (!shown_) {
    hover_poll_.Stop();
    SetVisible(false);
    RemoveAllChildViews();
    PreferredSizeChanged();
    SetWatching(false);
  }
}

BEGIN_METADATA(DownloadsStack)
END_METADATA

}  // namespace myarc
