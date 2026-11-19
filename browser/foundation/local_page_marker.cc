#include "myarc/local_page_marker.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "myarc/bridge.rs.h"
#include "myarc/icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace myarc {
namespace {

constexpr int kBorderWidth = 2;
constexpr int kChipIconSize = 12;
// macOS system blue, dark variant.
constexpr SkColor kLocalBlue = SkColorSetRGB(0x0A, 0x84, 0xFF);

// The border is a child view so it follows the card's layer and bounds; the
// card's rounded mask clips nothing because the stroke stays inside it.
class LocalPageBorder : public views::View, public views::ViewObserver {
  METADATA_HEADER(LocalPageBorder, views::View)

 public:
  explicit LocalPageBorder(views::View& card) : card_(card) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetIsIgnored(true);
    observation_.Observe(&card);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    gfx::RectF rect(GetLocalBounds());
    rect.Inset(kBorderWidth / 2.f);
    gfx::RoundedCornersF radii = card_->layer()->rounded_corner_radii();
    radii.Set(std::max(radii.upper_left() - kBorderWidth / 2.f, 0.f),
              std::max(radii.upper_right() - kBorderWidth / 2.f, 0.f),
              std::max(radii.lower_right() - kBorderWidth / 2.f, 0.f),
              std::max(radii.lower_left() - kBorderWidth / 2.f, 0.f));
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kBorderWidth);
    flags.setColor(kLocalBlue);
    canvas->sk_canvas()->drawRRect(SkRRect(gfx::RRectF(rect, radii)), flags);
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* card) override {
    SetBoundsRect(card->GetLocalBounds());
    SchedulePaint();
  }

 private:
  const raw_ref<views::View> card_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

BEGIN_METADATA(LocalPageBorder)
END_METADATA

}  // namespace

bool IsLocalPage(const GURL& url) {
  // Canonical hosts are ASCII, so the Rust string is always valid UTF-8.
  return url.SchemeIsHTTPOrHTTPS() &&
         foundation::is_local_dev_host(std::string(url.host()));
}

void UpdateLocalPageBorder(views::View& card, const GURL& url) {
  LocalPageBorder* border = nullptr;
  for (views::View* child : card.children()) {
    if (auto* found = views::AsViewClass<LocalPageBorder>(child)) {
      border = found;
    }
  }
  if (!IsLocalPage(url)) {
    if (border) {
      card.RemoveChildViewT(border);
    }
    return;
  }
  if (!border) {
    border = card.AddChildView(std::make_unique<LocalPageBorder>(card));
  }
  // The layout may have changed the card's radii since the last paint.
  border->OnViewBoundsChanged(&card);
}

LocalPageChip::LocalPageChip() {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetInsideBorderInsets(gfx::Insets::VH(0, 6));
  SetBetweenChildSpacing(3);
  SetCanProcessEventsWithinSubtree(false);
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  port_ = AddChildView(std::make_unique<views::Label>());
  port_->SetFontList(gfx::FontList().DeriveWithSizeDelta(-2));
  port_->SetAutoColorReadabilityEnabled(false);
  port_->SetEnabledColor(SK_ColorWHITE);
  icon_->SetImage(ui::ImageModel::FromVectorIcon(kCodeXmlIcon, SK_ColorWHITE,
                                                 kChipIconSize));
  SetBackground(views::CreateRoundedRectBackground(kLocalBlue, kHeight / 2));
  GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  GetViewAccessibility().SetName(u"Local page");
}

LocalPageChip::~LocalPageChip() = default;

bool LocalPageChip::Update(const GURL& url) {
  if (!IsLocalPage(url)) {
    return false;
  }
  const std::u16string port =
      url.has_port() ? u":" + base::UTF8ToUTF16(url.port()) : u"";
  port_->SetText(port);
  port_->SetVisible(!port.empty());
  GetViewAccessibility().SetName(u"Local page" + port);
  return true;
}

BEGIN_METADATA(LocalPageChip)
END_METADATA

}  // namespace myarc
