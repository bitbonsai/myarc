#include "myarc/local_page_marker.h"

#include <memory>
#include <optional>
#include <string>

#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "myarc/bridge.rs.h"
#include "myarc/icons.h"
#include "myarc/sidebar_tint.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_solid_color.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace myarc {
namespace {

constexpr int kLineHeight = 2;
constexpr int kChipIconSize = 12;
// A Space without a theme: the periwinkle of the default sidebar tint.
constexpr SkColor kDefaultAccent = SkColorSetRGB(0x6E, 0x7B, 0xFF);

// The colour the sidebar tints with for the window's current Space.
SkColor GetSpaceAccent(const views::Widget* widget) {
  const std::optional<SpaceTheme> theme = GetSidebarTheme(widget);
  return theme ? SkColorSetA(theme->color, SK_AlphaOPAQUE) : kDefaultAccent;
}

// The line is a child view so it follows the card's layer and bounds.
class LocalPageLine : public views::View, public views::ViewObserver {
  METADATA_HEADER(LocalPageLine, views::View)

 public:
  explicit LocalPageLine(views::View& card) {
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetIsIgnored(true);
    observation_.Observe(&card);
  }

  void UpdateColor() {
    layer()->AsSolidColor()->SetColor(
        SkColor4f::FromColor(GetSpaceAccent(GetWidget())));
  }

  // views::View:
  void AddedToWidget() override { UpdateColor(); }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* card) override {
    SetBounds(0, 0, card->width(), kLineHeight);
  }

 private:
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

BEGIN_METADATA(LocalPageLine)
END_METADATA

void RefreshIn(views::View* view) {
  if (auto* line = views::AsViewClass<LocalPageLine>(view)) {
    line->UpdateColor();
  } else if (auto* chip = views::AsViewClass<LocalPageChip>(view)) {
    chip->UpdateColors();
  }
  for (views::View* child : view->children()) {
    RefreshIn(child);
  }
}

}  // namespace

bool IsLocalPage(const GURL& url) {
  // Canonical hosts are ASCII, so the Rust string is always valid UTF-8.
  return url.SchemeIsHTTPOrHTTPS() &&
         foundation::is_local_dev_host(std::string(url.host()));
}

void UpdateLocalPageLine(views::View& card, const GURL& url) {
  LocalPageLine* line = nullptr;
  for (views::View* child : card.children()) {
    if (auto* found = views::AsViewClass<LocalPageLine>(child)) {
      line = found;
    }
  }
  if (!IsLocalPage(url)) {
    if (line) {
      card.RemoveChildViewT(line);
    }
    return;
  }
  if (!line) {
    line = card.AddChildView(std::make_unique<LocalPageLine>(card));
    line->OnViewBoundsChanged(&card);
  }
}

void RefreshLocalPageMarkers(views::Widget& widget) {
  if (views::View* root = widget.GetRootView()) {
    RefreshIn(root);
  }
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

void LocalPageChip::UpdateColors() {
  SetBackground(views::CreateRoundedRectBackground(
      SkColorSetA(GetSpaceAccent(GetWidget()), 0x26), kHeight / 2));
}

void LocalPageChip::OnThemeChanged() {
  views::BoxLayoutView::OnThemeChanged();
  const SkColor text = GetColorProvider()->GetColor(ui::kColorLabelForeground);
  icon_->SetImage(ui::ImageModel::FromVectorIcon(kCodeXmlIcon, text,
                                                 kChipIconSize));
  port_->SetEnabledColor(text);
  UpdateColors();
}

BEGIN_METADATA(LocalPageChip)
END_METADATA

}  // namespace myarc
