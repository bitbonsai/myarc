#include "myarc/library_view.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/footer_button.h"
#include "myarc/icons.h"
#include "myarc/library_logic.h"
#include "myarc/media_lightbox.h"
#include "myarc/sidebar_tint.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace myarc {

namespace {

using Orientation = views::BoxLayout::Orientation;
using MainAxis = views::BoxLayout::MainAxisAlignment;
using CrossAxis = views::BoxLayout::CrossAxisAlignment;
using Section = LibraryView::Section;

enum Command {
  kClearArchive = 1,
  kSwitchSpace,
  kRenameSpace,
  kMoveSpaceLeft,
  kMoveSpaceRight,
  kDeleteSpace,
  kEditTheme,
  kChangeIcon,
  kEditBoost,
  kToggleBoost,
  kDeleteBoost,
  kFilterAllSpaces,
  kFilterDeletedSpaces,
  kFilterTimeAny,
  kFilterTimeToday,
  kFilterTimeLast7Days,
  kFilterTimeLast30Days,
  kFilterReset,
  kHideMedia,
  kMediaOpenPreview,
  kMediaCopy,
  kMediaRevealFinder,
  kFilterSpaceBase = 1000,
};

// The other cards slide aside this fast while a card is dragged.
constexpr base::TimeDelta kCardSlide = base::Milliseconds(150);

// Translucent lifts: `tile` matches the sidebar's Favorites tile and `muted`
// its footer icons. The rail and empty-state values are sampled from Arc's dark
// Library; light mode mirrors them in black.
struct Colors {
  SkColor tile;
  SkColor column;
  SkColor muted;
  SkColor rail_selected;
  SkColor rail_hover;
  SkColor rail_icon;
  SkColor rail_label_hover;
  SkColor rail_label_selected;
  SkColor empty_icon;
  SkColor empty_body;
  // A Space card's edit, move and "..." glyphs, and the first two on hover.
  SkColor card_control;
  SkColor card_control_hover;
};

// The Library accent on the create button: a softer periwinkle than Arc's #212AFB.
constexpr SkColor kAccent = SkColorSetRGB(0x6E, 0x7B, 0xFF);
// The selected rail glyph's outline; icons.h bakes it into the duotone icons.
constexpr SkColor kRailLine = SkColorSetRGB(0xED, 0xED, 0xFD);

Colors GetColors(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  const SkColor ink = dark ? SK_ColorWHITE : SK_ColorBLACK;
  return {.tile = SkColorSetA(SK_ColorWHITE, dark ? 0x18 : 0x80),
          .column = SkColorSetA(SK_ColorWHITE, dark ? 0x06 : 0x59),
          .muted = SkColorSetA(ink, 0x99),
          .rail_selected = SkColorSetA(SK_ColorWHITE, dark ? 0x16 : 0x80),
          .rail_hover = SkColorSetA(SK_ColorWHITE, dark ? 0x0B : 0x59),
          .rail_icon = SkColorSetA(ink, 0xA6),
          .rail_label_hover = SkColorSetA(ink, 0xBF),
          .rail_label_selected = SkColorSetA(ink, 0xE6),
          .empty_icon = SkColorSetA(ink, 0x47),
          .empty_body = SkColorSetA(ink, 0x8C),
          .card_control = SkColorSetA(ink, 0x8C),
          .card_control_hover = SkColorSetA(ink, 0xE6)};
}

struct SectionInfo {
  Section section;
  // Constant table of static icons.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
  // Duotone icon for the selected rail item; null draws `icon` in the
  // selected label color.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* active_icon;
  // Arc's own glyphs draw at 28 DIP, the Lucide ones at 24.
  int icon_size;
  const char16_t* name;
  const char16_t* search;
};

constexpr auto kSections = std::to_array<SectionInfo>({
    {Section::kMedia, &kMediaIcon, &kMediaActiveIcon, 28, u"Media",
     u"Search Media..."},
    {Section::kDownloads, &kCircleArrowDownIcon, &kCircleArrowDownActiveIcon,
     24, u"Downloads", u"Search Downloads..."},
    {Section::kSpaces, &kSpacesIcon, &kSpacesActiveIcon, 28, u"Spaces",
     nullptr},
    {Section::kBoosts, &kBoostsIcon, &kBoostsActiveIcon, 28, u"Boosts",
     u"Search Boosts..."},
    {Section::kArchive, &kArchiveIcon, &kArchiveActiveIcon, 24,
     u"Archived Tabs", u"Search Archive..."},
});

bool IsBuilt(Section section) {
  return true;
}

const SectionInfo& Info(Section section) {
  for (const auto& info : kSections) {
    if (info.section == section) {
      return info;
    }
  }
  NOTREACHED();
}

std::unique_ptr<views::View> Box(Orientation orientation,
                                 const gfx::Insets& insets,
                                 int spacing) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(orientation, insets, spacing));
  return view;
}

std::unique_ptr<views::Label> MakeLabel(const std::u16string& text,
                                        const gfx::FontList& font,
                                        SkColor color) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(font);
  label->SetEnabledColor(color);
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  return label;
}

// Transparent text button like the sidebar's rows.
template <typename T = views::MdTextButton>
std::unique_ptr<T> TextButton(views::Button::PressedCallback callback,
                              const std::u16string& text) {
  auto button = std::make_unique<T>(
      std::move(callback), text, views::style::CONTEXT_BUTTON_MD,
      /*use_text_color_for_icon=*/false);
  button->SetStyle(ui::ButtonStyle::kText);
  button->SetTextSubpixelRenderingEnabled(false);
  button->SetBgColorOverrideDeprecated(SK_ColorTRANSPARENT);
  button->SetEnabledTextColors(ui::kColorSysOnSurface);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetCornerRadius(8);
  button->SetCustomPadding(gfx::Insets::VH(6, 8));
  button->SetMinSize(gfx::Size(0, 32));
  button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  button->SetElideBehavior(gfx::ELIDE_TAIL);
  button->SetImageLabelSpacing(8);
  return button;
}

// A Space card's edit or "..." button: Arc's dim glyph, brighter on hover.
std::unique_ptr<views::ImageButton> CardButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    const Colors& colors,
    const std::u16string& name) {
  auto button = std::make_unique<views::ImageButton>(std::move(callback));
  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(icon, colors.card_control, 18));
  for (auto state :
       {views::Button::STATE_HOVERED, views::Button::STATE_PRESSED}) {
    button->SetImageModel(state, ui::ImageModel::FromVectorIcon(
                                     icon, colors.card_control_hover, 18));
  }
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetPreferredSize(gfx::Size(28, 28));
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetTooltipText(name);
  button->GetViewAccessibility().SetName(name);
  views::InstallRoundRectHighlightPathGenerator(button.get(), gfx::Insets(), 8);
  return button;
}

// Arc's rail brightens toward its right edge and ends in a hairline, so the
// pane beside it reads a step darker. Light mode darkens the same way.
class RailBackground final : public views::Background {
 public:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    const ui::ColorProvider* provider = view->GetColorProvider();
    const bool dark =
        !provider ||
        color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
    const SkColor ink = dark ? SK_ColorWHITE : SK_ColorBLACK;
    const gfx::Rect bounds = view->GetLocalBounds();
    cc::PaintFlags flags;
    flags.setShader(
        gfx::CreateGradientShader(bounds.origin(), bounds.top_right(),
                                  SkColorSetA(ink, 0), SkColorSetA(ink, 0x0A)));
    canvas->DrawRect(bounds, flags);
    canvas->FillRect(
        gfx::Rect(bounds.right() - 1, bounds.y(), 1, bounds.height()),
        SkColorSetA(ink, 0x12));
  }
};

// A button whose children are plain views; it paints its own hover tile.
class TileButton : public views::Button {
  METADATA_HEADER(TileButton, views::Button)

 public:
  explicit TileButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 10);
  }

  void SetSelected(bool selected) {
    selected_ = selected;
    GetViewAccessibility().SetIsSelected(selected);
    Update();
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    Update();
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    Update();
  }

 protected:
  bool selected() const { return selected_; }

  virtual void Update() {
    const bool lifted =
        selected_ || GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    SetBackground(lifted ? views::CreateRoundedRectBackground(
                               GetColors(GetColorProvider()).tile, 10)
                         : nullptr);
  }

 private:
  bool selected_ = false;
};

BEGIN_METADATA(TileButton)
END_METADATA

// Arc's rail entry: icon over a short label. Hover lifts the tile half as much
// as selection and brightens only the label; selection swaps in the duotone
// icon. Arc flips these instantly, without a fade.
class LibraryRailItem final : public TileButton {
  METADATA_HEADER(LibraryRailItem, TileButton)

 public:
  LibraryRailItem(PressedCallback callback, const SectionInfo& info)
      : TileButton(std::move(callback)),
        icon_(*info.icon),
        active_icon_(info.active_icon),
        icon_size_(info.icon_size) {
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 12);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kVertical, gfx::Insets::VH(12, 4), 6));
    layout->set_main_axis_alignment(MainAxis::kCenter);
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    image_ = AddChildView(std::make_unique<views::ImageView>());
    // One box for every glyph size keeps the labels in line.
    image_->SetPreferredSize(gfx::Size(28, 28));
    label_ = AddChildView(
        MakeLabel(info.name,
                  gfx::FontList().Derive(-2, gfx::Font::NORMAL,
                                         gfx::Font::Weight::SEMIBOLD),
                  GetColors(nullptr).muted));
    // Arc's rail tiles are 93x77 DIP, 100 DIP apart.
    SetPreferredSize(gfx::Size(96, 77));
    GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
    GetViewAccessibility().SetName(info.name);
    SetTooltipText(info.name);
  }

 private:
  void Update() override {
    const Colors colors = GetColors(GetColorProvider());
    // Arc has no pressed look: a click goes straight to selected.
    const bool hovered = GetState() == STATE_HOVERED;
    const bool disabled = GetState() == STATE_DISABLED;
    SetBackground(
        selected() || hovered
            ? views::CreateRoundedRectBackground(
                  selected() ? colors.rail_selected : colors.rail_hover, 12)
            : nullptr);
    image_->SetImage(
        selected() ? (active_icon_
                          ? ui::ImageModel::FromVectorIcon(
                                *active_icon_, kRailLine, icon_size_)
                          // Single-color glyphs follow the label so they
                          // stay visible on the light selected tile.
                          : ui::ImageModel::FromVectorIcon(
                                *icon_, colors.rail_label_selected, icon_size_))
                   : ui::ImageModel::FromVectorIcon(
                         *icon_,
                         disabled ? SkColorSetA(colors.rail_icon, 0x59)
                                  : colors.rail_icon,
                         icon_size_));
    label_->SetEnabledColor(selected() ? colors.rail_label_selected
                            : hovered  ? colors.rail_label_hover
                            : disabled ? SkColorSetA(colors.muted, 0x59)
                                       : colors.muted);
  }

  const raw_ref<const gfx::VectorIcon> icon_;
  RAW_PTR_EXCLUSION const gfx::VectorIcon* const active_icon_;
  const int icon_size_;
  raw_ptr<views::ImageView> image_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
};

BEGIN_METADATA(LibraryRailItem)
END_METADATA

// Arc's Archived Tabs row: favicon, then the title over the page's address.
class ArchiveRow final : public TileButton {
  METADATA_HEADER(ArchiveRow, TileButton)

 public:
  ArchiveRow(PressedCallback callback,
             const SidebarView::ArchivedTabPresentation& tab)
      : TileButton(std::move(callback)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(9, 12), 12));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    // Pages without a cached favicon get Arc's blank rounded square.
    icon_ = AddChildView(std::make_unique<views::ImageView>(
        EnsureFaviconContrast(tab.icon, std::nullopt)));
    icon_->SetImageSize(gfx::Size(16, 16));
    icon_->SetPreferredSize(gfx::Size(20, 20));
    blank_icon_ = tab.icon.IsEmpty();
    auto* text = AddChildView(Box(Orientation::kVertical, gfx::Insets(), 2));
    layout->SetFlexForView(text, 1);
    title_ = text->AddChildView(MakeLabel(
        tab.title,
        gfx::FontList().Derive(1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL),
        SK_ColorWHITE));
    url_ = text->AddChildView(
        MakeLabel(tab.url, gfx::FontList(), GetColors(nullptr).muted));
    for (views::Label* label : {title_.get(), url_.get()}) {
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      label->SetElideBehavior(gfx::ELIDE_TAIL);
    }
    std::u16string detail = tab.url;
    if (!tab.space.empty()) {
      detail += u"\nSpace: " + tab.space;
    }
    SetTooltipText(tab.title + u"\n" + detail);
    GetViewAccessibility().SetName(u"Reopen " + tab.title);
    GetViewAccessibility().SetDescription(detail);
  }

 private:
  void Update() override {
    TileButton::Update();
    const Colors colors = GetColors(GetColorProvider());
    if (blank_icon_) {
      icon_->SetBackground(views::CreateRoundedRectBackground(colors.tile, 5));
    }
    if (GetColorProvider()) {
      title_->SetEnabledColor(
          GetColorProvider()->GetColor(ui::kColorSysOnSurface));
    }
    url_->SetEnabledColor(colors.muted);
  }

  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> url_ = nullptr;
  bool blank_icon_ = false;
};

BEGIN_METADATA(ArchiveRow)
END_METADATA

// Rounded-square site favicon tile for Boost rows, dimmed with a diagonal slash
// when disabled (matching Arc's Library -> Boosts list).
class BoostIconTile final : public views::View {
  METADATA_HEADER(BoostIconTile, views::View)

 public:
  BoostIconTile(const ui::ImageModel& icon, bool active)
      : icon_(icon), active_(active) {
    SetPreferredSize(gfx::Size(36, 36));
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets(), 0));
    layout->set_main_axis_alignment(MainAxis::kCenter);
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    image_ = AddChildView(std::make_unique<views::ImageView>());
    image_->SetImageSize(gfx::Size(20, 20));
    image_->SetPreferredSize(gfx::Size(20, 20));
    if (!active_) {
      image_->SetPaintToLayer();
      image_->layer()->SetFillsBoundsOpaquely(false);
      image_->layer()->SetOpacity(0.45f);
    }
    UpdateIcon();
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    UpdateIcon();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::RectF rect(GetLocalBounds());
    cc::PaintFlags bg;
    bg.setAntiAlias(true);
    bg.setColor(active_ ? SkColorSetRGB(0xF5, 0xF5, 0xF7)
                        : SkColorSetA(SkColorSetRGB(0xB0, 0xB0, 0xBA), 0x80));
    canvas->DrawRoundRect(rect, 8.f, bg);
    views::View::OnPaint(canvas);
  }

  void PaintChildren(const views::PaintInfo& paint_info) override {
    views::View::PaintChildren(paint_info);
    if (active_) {
      return;
    }
    // Paint diagonal slash over the disabled Boost tile.
    ui::PaintRecorder recorder(paint_info.context(), size());
    gfx::Canvas* canvas = recorder.canvas();
    const gfx::PointF start(4.f, 4.f);
    const gfx::PointF end(width() - 4.f, height() - 4.f);
    cc::PaintFlags outline;
    outline.setAntiAlias(true);
    outline.setStyle(cc::PaintFlags::kStroke_Style);
    outline.setStrokeWidth(4.5f);
    outline.setStrokeCap(cc::PaintFlags::kRound_Cap);
    outline.setColor(SkColorSetA(SK_ColorBLACK, 0x55));
    canvas->DrawLine(start, end, outline);

    cc::PaintFlags slash;
    slash.setAntiAlias(true);
    slash.setStyle(cc::PaintFlags::kStroke_Style);
    slash.setStrokeWidth(2.2f);
    slash.setStrokeCap(cc::PaintFlags::kRound_Cap);
    slash.setColor(SkColorSetRGB(0x7A, 0x4A, 0x55));
    canvas->DrawLine(start, end, slash);
  }

 private:
  void UpdateIcon() {
    if (!icon_.IsEmpty()) {
      image_->SetImage(icon_);
      return;
    }
    image_->SetImage(ui::ImageModel::FromVectorIcon(
        kBoostsIcon,
        active_ ? SkColorSetRGB(0x52, 0x52, 0x62)
                : SkColorSetRGB(0x6E, 0x6E, 0x7A),
        18));
  }

  const ui::ImageModel icon_;
  const bool active_;
  raw_ptr<views::ImageView> image_ = nullptr;
};

BEGIN_METADATA(BoostIconTile)
END_METADATA

// Arc's Boosts row: 36x36 rounded-square favicon tile (slashed when disabled),
// Boost title over origin host subtitle.
class BoostRow final : public TileButton {
  METADATA_HEADER(BoostRow, TileButton)

 public:
  BoostRow(PressedCallback callback,
           const SidebarView::BoostPresentation& boost,
           bool global_enabled)
      : TileButton(std::move(callback)),
        boost_id_(boost.id),
        boost_enabled_(boost.enabled),
        active_(boost.enabled && global_enabled) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(7, 10), 12));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    AddChildView(std::make_unique<BoostIconTile>(boost.icon, active_));
    auto* text = AddChildView(Box(Orientation::kVertical, gfx::Insets(), 2));
    layout->SetFlexForView(text, 1);
    title_ = text->AddChildView(MakeLabel(
        boost.name,
        gfx::FontList().Derive(1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM),
        SK_ColorWHITE));
    host_ = text->AddChildView(
        MakeLabel(boost.host, gfx::FontList(), GetColors(nullptr).muted));
    for (views::Label* label : {title_.get(), host_.get()}) {
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      label->SetElideBehavior(gfx::ELIDE_TAIL);
    }
    std::u16string detail = boost.host;
    if (!boost.enabled) {
      detail += u" (Disabled)";
    } else if (!global_enabled) {
      detail += u" (All Boosts disabled)";
    }
    SetTooltipText(boost.name + u"\n" + detail);
    GetViewAccessibility().SetName(boost.name);
    GetViewAccessibility().SetDescription(detail);
  }

  uint64_t boost_id() const { return boost_id_; }
  bool boost_enabled() const { return boost_enabled_; }

 private:
  void Update() override {
    TileButton::Update();
    const Colors colors = GetColors(GetColorProvider());
    const SkColor on_surface =
        GetColorProvider()
            ? GetColorProvider()->GetColor(ui::kColorSysOnSurface)
            : SK_ColorWHITE;
    title_->SetEnabledColor(active_ ? on_surface
                                    : SkColorSetA(on_surface, 0x73));
    host_->SetEnabledColor(active_ ? colors.muted
                                   : SkColorSetA(colors.muted, 0x73));
  }

  const uint64_t boost_id_;
  const bool boost_enabled_;
  const bool active_;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> host_ = nullptr;
};

BEGIN_METADATA(BoostRow)
END_METADATA

// A Pin on a Space's card: favicon and title, 36 DIP tall like the sidebar's
// rows, with the same hover tile. Arc sets the title in 15pt semibold, 28 DIP
// right of the card's icon column.
class PinRow final : public TileButton {
  METADATA_HEADER(PinRow, TileButton)

 public:
  PinRow(PressedCallback callback, const SidebarView::LibraryPin& pin)
      : TileButton(std::move(callback)),
        icon_(EnsureFaviconContrast(pin.icon, std::nullopt)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(0, 8), 8));
    layout->set_cross_axis_alignment(CrossAxis::kCenter);
    image_ = AddChildView(std::make_unique<views::ImageView>());
    image_->SetImageSize(gfx::Size(16, 16));
    image_->SetPreferredSize(gfx::Size(20, 20));
    title_ = AddChildView(
        MakeLabel(pin.title,
                  gfx::FontList().Derive(2, gfx::Font::NORMAL,
                                         gfx::Font::Weight::SEMIBOLD),
                  SK_ColorWHITE));
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetElideBehavior(gfx::ELIDE_TAIL);
    layout->SetFlexForView(title_, 1);
    SetPreferredSize(gfx::Size(0, 36));
    SetTooltipText(pin.title);
    GetViewAccessibility().SetName(pin.title);
    GetViewAccessibility().SetDescription(u"Space Pin");
  }

 private:
  void Update() override {
    TileButton::Update();
    const ui::ColorProvider* provider = GetColorProvider();
    if (!provider) {
      return;
    }
    const SkColor on_surface = provider->GetColor(ui::kColorSysOnSurface);
    // Pins without a cached favicon get the sidebar's globe.
    image_->SetImage(icon_.IsEmpty()
                         ? ui::ImageModel::FromVectorIcon(
                               vector_icons::kGlobeIcon, on_surface, 16)
                         : icon_);
    title_->SetEnabledColor(on_surface);
  }

  const ui::ImageModel icon_;
  raw_ptr<views::ImageView> image_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
};

BEGIN_METADATA(PinRow)
END_METADATA

// Arc's move handle: dragging it reorders the Space cards. Mouse events go to
// `on_event`, and nullptr when the drag loses capture.
class MoveHandle final : public views::ImageView {
  METADATA_HEADER(MoveHandle, views::ImageView)

 public:
  using OnEvent =
      base::RepeatingCallback<void(views::View*, const ui::MouseEvent*)>;

  MoveHandle(const ui::ImageModel& image, OnEvent on_event)
      : views::ImageView(image), on_event_(std::move(on_event)) {}

  // views::ImageView:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (!event.IsOnlyLeftMouseButton()) {
      return false;
    }
    on_event_.Run(this, &event);
    return true;
  }
  bool OnMouseDragged(const ui::MouseEvent& event) override {
    on_event_.Run(this, &event);
    return true;
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    on_event_.Run(this, &event);
  }
  void OnMouseCaptureLost() override { on_event_.Run(this, nullptr); }
  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kGrab;
  }

 private:
  const OnEvent on_event_;
};

BEGIN_METADATA(MoveHandle)
END_METADATA

// Moves a card sideways, sliding unless the user prefers reduced motion.
void SlideCard(views::View& card, int x) {
  ui::Layer* layer = card.layer();
  if (!layer) {
    return;
  }
  ui::ScopedLayerAnimationSettings slide(layer->GetAnimator());
  slide.SetTransitionDuration(
      gfx::Animation::PrefersReducedMotion() ? base::TimeDelta() : kCardSlide);
  slide.SetTweenType(gfx::Tween::EASE_OUT);
  slide.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer->SetTransform(gfx::Transform::MakeTranslation(x, 0));
}

// Arc's "Show Media" opt-in CTA: a rounded button with a pink-to-indigo
// diagonal gradient (`#E8A6E8` -> `#5B73FF`) and centered semibold white text.
class GradientCtaButton : public views::Button {
  METADATA_HEADER(GradientCtaButton, views::Button)

 public:
  GradientCtaButton(PressedCallback callback, std::u16string text)
      : views::Button(std::move(callback)), text_(std::move(text)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetPreferredSize(gfx::Size(0, 36));
    GetViewAccessibility().SetName(text_);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 8);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::RectF rect(GetLocalBounds());
    constexpr float kRadius = 8.0f;
    SkPoint pts[2] = {SkPoint::Make(0.0f, 0.0f),
                      SkPoint::Make(rect.width(), rect.height())};
    SkColor4f colors[2] = {SkColor4f::FromColor(SkColorSetRGB(0xE8, 0xA6, 0xE8)),
                           SkColor4f::FromColor(SkColorSetRGB(0x5B, 0x73, 0xFF))};
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        pts, colors, nullptr, 2, SkTileMode::kClamp));
    canvas->DrawRoundRect(rect, kRadius, flags);
    if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED) {
      cc::PaintFlags overlay;
      overlay.setAntiAlias(true);
      overlay.setColor(GetState() == STATE_PRESSED
                           ? SkColorSetA(SK_ColorBLACK, 0x28)
                           : SkColorSetA(SK_ColorWHITE, 0x18));
      canvas->DrawRoundRect(rect, kRadius, overlay);
    }
    const gfx::FontList font = gfx::FontList().Derive(
        0, gfx::Font::NORMAL, gfx::Font::Weight::SEMIBOLD);
    canvas->DrawStringRectWithFlags(
        text_, font, SK_ColorWHITE, GetLocalBounds(),
        gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }

 private:
  const std::u16string text_;
};

BEGIN_METADATA(GradientCtaButton)
END_METADATA

// A thumbnail tile in the 2-column Media grid: white-bordered aspect-fit
// preview (or skeleton shimmer while loading), video play overlay, hover pill
// trigger, and drag-out threshold detection for native NSFilePromise drag.
class MediaTile : public views::Button {
  METADATA_HEADER(MediaTile, views::Button)

 public:
  using HoverCallback =
      base::RepeatingCallback<void(views::View*, const std::u16string&, bool)>;
  using DragCallback = base::RepeatingCallback<void(uint64_t)>;

  MediaTile(PressedCallback on_click,
            HoverCallback on_hover,
            DragCallback on_drag,
            SidebarView::MediaItemPresentation item)
      : views::Button(std::move(on_click)),
        on_hover_(std::move(on_hover)),
        on_drag_(std::move(on_drag)),
        item_(std::move(item)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetPreferredSize(gfx::Size(140, 128));
    GetViewAccessibility().SetName(item_.filename);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 10);
  }

  uint64_t media_id() const { return item_.id; }
  const std::u16string& filename() const { return item_.filename; }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    on_hover_.Run(this, item_.filename, true);
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    on_hover_.Run(this, item_.filename, false);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton()) {
      drag_origin_ = event.location();
      drag_started_ = false;
    }
    return views::Button::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (drag_origin_ && !drag_started_ &&
        views::View::ExceededDragThreshold(event.location() - *drag_origin_)) {
      drag_started_ = true;
      on_hover_.Run(this, item_.filename, false);
      on_drag_.Run(item_.id);
      return true;
    }
    return views::Button::OnMouseDragged(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    const bool was_drag = drag_started_;
    drag_origin_.reset();
    drag_started_ = false;
    if (was_drag) {
      SetState(STATE_NORMAL);
      return;
    }
    views::Button::OnMouseReleased(event);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::Rect bounds = GetLocalBounds();
    const gfx::RectF outer(bounds);
    constexpr float kCardRadius = 10.0f;
    constexpr float kCardPadding = 10.0f;
    constexpr float kBorderWidth = 2.0f;
    const ui::ColorProvider* provider = GetColorProvider();
    const bool is_dark =
        !provider ||
        color_utils::IsDark(provider->GetColor(ui::kColorSysSurface));
    const bool hovered = GetState() == STATE_HOVERED;

    // Uniform subtle rounded card cell behind every tile, matching Arc's
    // 2-column Media grid.
    cc::PaintFlags card_bg;
    card_bg.setAntiAlias(true);
    card_bg.setColor(
        is_dark
            ? SkColorSetA(SK_ColorWHITE, hovered ? 0x18 : 0x0D)
            : SkColorSetA(SK_ColorBLACK, hovered ? 0x12 : 0x08));
    canvas->DrawRoundRect(outer, kCardRadius, card_bg);

    const auto draw_play_badge = [canvas](const gfx::Rect& target) {
      const gfx::PointF center(target.CenterPoint());
      const float min_dim =
          static_cast<float>(std::min(target.width(), target.height()));
      const float radius = std::clamp(min_dim * 0.24f, 11.0f, 15.0f);
      cc::PaintFlags badge;
      badge.setAntiAlias(true);
      badge.setColor(SkColorSetA(SK_ColorBLACK, 0x7A));
      canvas->DrawCircle(center, radius, badge);

      const float tri_h = radius * 0.52f;
      const float tri_left = radius * 0.30f;
      const float tri_right = radius * 0.48f;
      SkPath tri = SkPathBuilder()
                       .moveTo(center.x() - tri_left, center.y() - tri_h)
                       .lineTo(center.x() + tri_right, center.y())
                       .lineTo(center.x() - tri_left, center.y() + tri_h)
                       .close()
                       .detach();
      cc::PaintFlags play;
      play.setAntiAlias(true);
      play.setColor(SK_ColorWHITE);
      canvas->DrawPath(tri, play);
    };

    if (item_.thumbnail.IsEmpty()) {
      if (item_.is_video) {
        draw_play_badge(bounds);
      }
      return;
    }

    const gfx::ImageSkia img = item_.thumbnail.Rasterize(provider);
    if (img.isNull()) {
      if (item_.is_video) {
        draw_play_badge(bounds);
      }
      return;
    }

    // Crisp 2 DIP white/light border frame around the aspect-fit thumbnail so
    // dark screenshots stay clearly separated from the dark Library card.
    const gfx::Size img_size = img.size();
    const float avail_w =
        std::max(1.0f, outer.width() - (kCardPadding + kBorderWidth) * 2.0f);
    const float avail_h =
        std::max(1.0f, outer.height() - (kCardPadding + kBorderWidth) * 2.0f);
    const float scale = std::min(avail_w / std::max(1, img_size.width()),
                                 avail_h / std::max(1, img_size.height()));
    const int draw_w = std::max(1, static_cast<int>(img_size.width() * scale));
    const int draw_h = std::max(1, static_cast<int>(img_size.height() * scale));
    const gfx::Rect dst((bounds.width() - draw_w) / 2,
                        (bounds.height() - draw_h) / 2, draw_w, draw_h);
    const gfx::RectF frame(dst.x() - kBorderWidth, dst.y() - kBorderWidth,
                           dst.width() + kBorderWidth * 2.0f,
                           dst.height() + kBorderWidth * 2.0f);

    cc::PaintFlags border_flags;
    border_flags.setAntiAlias(true);
    border_flags.setColor(is_dark ? SkColorSetA(SK_ColorWHITE, 0xF2)
                                  : SK_ColorWHITE);
    canvas->DrawRoundRect(frame, 4.5f, border_flags);

    if (!is_dark) {
      cc::PaintFlags light_outline;
      light_outline.setAntiAlias(true);
      light_outline.setStyle(cc::PaintFlags::kStroke_Style);
      light_outline.setStrokeWidth(1.0f);
      light_outline.setColor(SkColorSetA(SK_ColorBLACK, 0x1A));
      canvas->DrawRoundRect(frame, 4.5f, light_outline);
    }

    canvas->Save();
    SkPath clip =
        SkPathBuilder()
            .addRRect(SkRRect::MakeRectXY(
                SkRect::MakeXYWH(dst.x(), dst.y(), dst.width(), dst.height()),
                2.5f, 2.5f))
            .detach();
    canvas->ClipPath(clip, true);
    canvas->DrawImageInt(img, 0, 0, img_size.width(), img_size.height(),
                         dst.x(), dst.y(), dst.width(), dst.height(), true);
    canvas->Restore();

    if (item_.is_video) {
      draw_play_badge(dst);
    }
  }

 private:
  const HoverCallback on_hover_;
  const DragCallback on_drag_;
  const SidebarView::MediaItemPresentation item_;
  std::optional<gfx::Point> drag_origin_;
  bool drag_started_ = false;
};

BEGIN_METADATA(MediaTile)
END_METADATA

}  // namespace

// Floating non-activating indigo filename pill anchored to the bottom-right of
// the hovered Media tile, matching Arc's `#23297A` capsule badge.
class LibraryView::MediaFilenamePill {
 public:
  MediaFilenamePill() = default;
  ~MediaFilenamePill() { Hide(); }

  void Show(views::Widget* parent,
            const gfx::Rect& tile_screen_bounds,
            const std::u16string& filename) {
    if (!parent) {
      return;
    }
    if (!widget_) {
      widget_ = std::make_unique<views::Widget>();
      views::Widget::InitParams params(
          views::Widget::InitParams::CLIENT_OWNS_WIDGET,
          views::Widget::InitParams::TYPE_POPUP);
      params.parent = parent->GetNativeView();
      params.activatable = views::Widget::InitParams::Activatable::kNo;
      params.accept_events = false;
      params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
      params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
      widget_->Init(std::move(params));

      auto root = std::make_unique<views::View>();
      root->SetLayoutManager(std::make_unique<views::BoxLayout>(
          Orientation::kHorizontal, gfx::Insets::VH(4, 10), 0));
      root->SetBackground(views::CreateRoundedRectBackground(
          SkColorSetRGB(0x23, 0x29, 0x7A), 12));
      auto label = std::make_unique<views::Label>();
      label->SetFontList(gfx::FontList().Derive(
          -1, gfx::Font::NORMAL, gfx::Font::Weight::SEMIBOLD));
      label->SetEnabledColor(SK_ColorWHITE);
      label->SetAutoColorReadabilityEnabled(false);
      label->SetElideBehavior(gfx::ELIDE_MIDDLE);
      label->SetMaximumWidthSingleLine(220);
      label_ = root->AddChildView(std::move(label));
      widget_->SetContentsView(std::move(root));
    }
    label_->SetText(filename);
    const gfx::Size size = widget_->GetContentsView()->GetPreferredSize({});
    const gfx::Point origin(tile_screen_bounds.right() - size.width() + 8,
                            tile_screen_bounds.bottom() - size.height() + 4);
    widget_->SetBounds(gfx::Rect(origin, size));
    widget_->ShowInactive();
  }

  void Hide() {
    if (widget_) {
      widget_->Hide();
    }
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::Label> label_ = nullptr;
};

LibraryView::LibraryView(SidebarView::Delegate& delegate,
                         base::RepeatingClosure close,
                         Section section,
                         base::WeakPtr<FooterTooltip> tooltip)
    : delegate_(delegate),
      close_(std::move(close)),
      section_(IsBuilt(section) ? section : Section::kSpaces) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kHorizontal,
                                                      gfx::Insets(), 0));
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Library");

  auto* rail = AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(0, 8, 10, 8), 0));
  rail->SetPreferredSize(gfx::Size(library::kRailWidth, 0));
  rail->SetBackground(std::make_unique<RailBackground>());
  auto* rail_layout = static_cast<views::BoxLayout*>(rail->GetLayoutManager());
  auto* items = rail->AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(36, 0, 0, 0), 23));
  items->GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  items->GetViewAccessibility().SetName(u"Library sections");
  auto* items_layout =
      static_cast<views::BoxLayout*>(items->GetLayoutManager());
  items_layout->set_main_axis_alignment(MainAxis::kCenter);
  items_layout->set_cross_axis_alignment(CrossAxis::kCenter);
  rail_layout->SetFlexForView(items, 1);
  // Arc centers the tiles 18 DIP below the rail's middle.
  for (const auto& info : kSections) {
    rail_items_.push_back(items->AddChildView(std::make_unique<LibraryRailItem>(
        base::BindRepeating(&LibraryView::SelectSection, base::Unretained(this),
                            info.section),
        info)));
    // Arc clears the archive from the Archived Tabs item's context menu, and
    // hides Media from the Media item's context menu.
    if (info.section == Section::kArchive) {
      rail_items_.back()->set_context_menu_controller(this);
    } else if (info.section == Section::kMedia) {
      rail_items_.back()->set_context_menu_controller(this);
      media_rail_item_ = rail_items_.back();
    }
    if (!IsBuilt(info.section)) {
      rail_items_.back()->SetEnabled(false);
    }
  }
  auto* footer =
      rail->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 0));
  // Arc's footer is a thin back arrow with its tooltip beside it.
  auto exit = TextButton<FooterButton>(
      base::BindRepeating(
          [](LibraryView* view) { view->PostAction(view->close_); },
          base::Unretained(this)),
      u"");
  exit->SetCustomPadding(gfx::Insets());
  exit->SetMinSize(gfx::Size(32, 32));
  exit->SetMaxSize(gfx::Size(32, 32));
  exit->SetPreferredSize(gfx::Size(32, 32));
  exit->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  exit->SetImageCentered(true);
  exit->SetFooterTooltip(std::move(tooltip),
                         {u"Exit Library", {}, /*beside=*/true});
  exit->GetViewAccessibility().SetName(u"Exit Library");
  exit_ = footer->AddChildView(std::move(exit));

  pane_ = AddChildView(std::make_unique<views::View>());
  pane_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));
  static_cast<views::BoxLayout*>(GetLayoutManager())->SetFlexForView(pane_, 1);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  Rebuild();
}

LibraryView::~LibraryView() {
  CloseMediaLightbox();
  HideMediaPill();
  if (media_shown_) {
    media_shown_ = false;
    delegate_->SetMediaLibraryShown(false);
  }
}

void LibraryView::SetSpaces(Spaces spaces, uint64_t current) {
  if (spaces == spaces_ && current == current_space_) {
    return;
  }
  spaces_ = std::move(spaces);
  current_space_ = current;
  // Rebuilding would drop the card under the pointer; the drag's end does it.
  if (drag_) {
    rebuild_after_drag_ = true;
    return;
  }
  if (section_ == Section::kSpaces) {
    Rebuild();
  }
}

void LibraryView::SelectSection(Section section) {
  if (section == section_ || !IsBuilt(section)) {
    return;
  }
  if (section_ == Section::kMedia) {
    CloseMediaLightbox();
    HideMediaPill();
    if (media_shown_) {
      media_shown_ = false;
      delegate_->SetMediaLibraryShown(false);
    }
  }
  section_ = section;
  query_.clear();
  archive_filter_ = {};
  media_card_dismissed_ = false;
  // Keys from the old section mean nothing in the new one.
  keyed_views_.clear();
  Rebuild();
}

void LibraryView::RefreshDownloads() {
  if (section_ != Section::kDownloads || !list_) {
    return;
  }
  auto downloads = delegate_->GetRecentDownloads();
  // Progress ticks rarely change what the list shows.
  if (downloads == downloads_) {
    return;
  }
  downloads_ = std::move(downloads);
  UpdateList(/*keep_scroll=*/true);
}

void LibraryView::RefreshArchive() {
  if (section_ != Section::kArchive || !list_) {
    return;
  }
  auto archived = delegate_->GetArchivedTabs();
  if (archived == archived_) {
    return;
  }
  archived_ = std::move(archived);
  UpdateList(/*keep_scroll=*/true);
}

void LibraryView::RefreshBoosts() {
  if (section_ != Section::kBoosts || !list_) {
    return;
  }
  auto boosts = delegate_->GetBoosts();
  const bool global_enabled = delegate_->AllBoostsEnabled();
  if (boosts == boosts_ && global_enabled == boosts_global_enabled_) {
    return;
  }
  boosts_ = std::move(boosts);
  boosts_global_enabled_ = global_enabled;
  UpdateList(/*keep_scroll=*/true);
}

void LibraryView::RefreshMedia() {
  if (section_ != Section::kMedia || !list_) {
    return;
  }
  const bool enabled = delegate_->IsMediaLibraryEnabled();
  const bool denied = delegate_->IsMediaLibraryPermissionDenied();
  auto items = enabled ? delegate_->GetMediaItems()
                       : std::vector<SidebarView::MediaItemPresentation>();
  if (enabled == media_enabled_ && denied == media_permission_denied_ &&
      items == media_) {
    return;
  }
  media_enabled_ = enabled;
  media_permission_denied_ = denied;
  media_ = std::move(items);
  if (search_) {
    search_->SetEnabled(media_enabled_);
  }
  UpdateList(/*keep_scroll=*/true);
}

void LibraryView::ShowExitTooltipNow() {
  exit_->ShowFooterTooltipNow();
}

void LibraryView::HoldFocus() {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  RequestFocus();
}

void LibraryView::FocusSection() {
  for (size_t i = 0; i < std::size(kSections); ++i) {
    if (kSections[i].section == section_) {
      rail_items_[i]->RequestFocus();
    }
  }
}

gfx::Size LibraryView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::View::CalculatePreferredSize(available_size);
  size.set_width(section_ == Section::kSpaces
                     ? library::SpacesWidth(spaces_.size())
                     : library::kListWidth);
  return size;
}

bool LibraryView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (lightbox_) {
    CloseMediaLightbox();
    return true;
  }
  // The window offers accelerators before the focused view sees the key, so
  // leave Esc to the omnibox and pages unless focus is in the sidebar.
  const views::FocusManager* manager = GetFocusManager();
  const views::View* focused = manager ? manager->GetFocusedView() : nullptr;
  if (focused && !parent()->Contains(focused)) {
    return false;
  }
  // Esc drops a dragged card back in its place.
  if (drag_) {
    EndSpaceDrag(/*commit=*/false);
    return true;
  }
  // The first Esc clears a search, the next exits.
  if (search_ && focused == search_ && !search_->GetText().empty()) {
    search_->SetText(std::u16string());
    ContentsChanged(search_, std::u16string());
    return true;
  }
  PostAction(close_);
  return true;
}

void LibraryView::OnBlur() {
  views::View::OnBlur();
  // Deactivating the window clears focus and restores it later, so only a
  // move to another view ends the hold.
  const views::FocusManager* manager = GetFocusManager();
  if (manager && manager->GetFocusedView()) {
    SetFocusBehavior(FocusBehavior::NEVER);
  }
}

void LibraryView::OnThemeChanged() {
  views::View::OnThemeChanged();
  exit_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kArrowLeftIcon,
                                     GetColors(GetColorProvider()).muted, 24));
  // Pane fills are plain colors picked for the current scheme. Rebuild only
  // when they change: inactive-window updates land here too.
  const ui::ColorProvider* provider = GetColorProvider();
  const std::pair colors(provider->GetColor(ui::kColorSysSurface),
                         provider->GetColor(ui::kColorSysOnSurface));
  if (colors == theme_colors_) {
    return;
  }
  theme_colors_ = colors;
  Rebuild();
}

std::optional<uint64_t> LibraryView::FocusedKey() const {
  const views::FocusManager* manager = GetFocusManager();
  const views::View* focused = manager ? manager->GetFocusedView() : nullptr;
  for (const auto& [key, view] : keyed_views_) {
    if (focused && view.get() == focused) {
      return key;
    }
  }
  return std::nullopt;
}

bool LibraryView::RestoreFocus(std::optional<uint64_t> key) {
  if (!key) {
    return false;
  }
  const auto target = std::ranges::find(
      keyed_views_, *key, &decltype(keyed_views_)::value_type::first);
  if (target == keyed_views_.end()) {
    return false;
  }
  target->second->RequestFocus();
  return true;
}

void LibraryView::PostAction(base::OnceClosure action) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<LibraryView> view, base::OnceClosure action) {
            if (view) {
              std::move(action).Run();
            }
          },
          weak_factory_.GetWeakPtr(), std::move(action)));
}

void LibraryView::Rebuild() {
  for (size_t i = 0; i < std::size(kSections); ++i) {
    static_cast<TileButton*>(rail_items_[i].get())
        ->SetSelected(kSections[i].section == section_);
  }
  const bool had_focus = pane_->Contains(
      GetFocusManager() ? GetFocusManager()->GetFocusedView() : nullptr);
  const std::optional<uint64_t> key = FocusedKey();
  // Only a rebuild of the Spaces section has `scrolls_`, so a section switch
  // starts at the top.
  std::map<uint64_t, gfx::PointF> offsets;
  for (const auto& [scroll_key, scroll] : scrolls_) {
    offsets.emplace(scroll_key, scroll->CurrentOffset());
  }
  scrolls_.clear();
  search_ = nullptr;
  filter_button_ = nullptr;
  list_ = nullptr;
  keyed_views_.clear();
  cards_.clear();
  drag_.reset();
  rebuild_after_drag_ = false;
  pane_->RemoveAllChildViews();
  if (section_ == Section::kSpaces) {
    BuildSpaces(offsets);
  } else {
    BuildList();
  }
  // Keep focus on the same Space or download, else on the section's tab.
  if (had_focus && !RestoreFocus(key)) {
    FocusSection();
  }
  PreferredSizeChanged();
}

void LibraryView::BuildSpaces(const std::map<uint64_t, gfx::PointF>& offsets) {
  const Colors colors = GetColors(GetColorProvider());
  auto* layout = static_cast<views::BoxLayout*>(pane_->GetLayoutManager());
  // Columns keep their width: many Spaces or a narrow window scroll sideways,
  // so Create stays reachable.
  auto* scroll = pane_->AddChildView(std::make_unique<views::ScrollView>());
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetDrawOverflowIndicator(false);
  scroll->SetVerticalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetTreatAllScrollEventsAsHorizontal(true);
  scroll->SetUseContentsPreferredSize(true);
  layout->SetFlexForView(scroll, 1);
  // Arc's columns start 28 DIP right of the rail and 48 DIP from the top.
  auto* row =
      scroll->SetContents(Box(Orientation::kHorizontal,
                              gfx::Insets::TLBR(48, library::kColumnsLeading,
                                                49, library::kCreateTrailing),
                              library::kColumnGap));
  // A rebuild puts each scroll back on its first real layout.
  const auto saved = [&offsets](uint64_t key) -> std::optional<gfx::PointF> {
    const auto it = offsets.find(key);
    return it == offsets.end() ? std::nullopt : std::optional(it->second);
  };
  scroll->RegisterPostLayoutCallback(base::BindRepeating(
      [](std::optional<gfx::PointF>& restore, views::ScrollView* scroll) {
        // Columns take the viewport's height, and its width when they fit.
        views::View* contents = scroll->contents();
        const gfx::Rect visible = scroll->GetVisibleRect();
        const gfx::Size fill(
            std::max(contents->GetPreferredSize().width(), visible.width()),
            visible.height());
        if (contents->size() != fill) {
          contents->SetSize(fill);
        }
        if (restore && !visible.IsEmpty()) {
          scroll->ScrollToOffset(*std::exchange(restore, std::nullopt));
        }
      },
      base::OwnedRef(saved(0))));
  scrolls_.emplace_back(0, scroll);
  auto* row_layout = static_cast<views::BoxLayout*>(row->GetLayoutManager());
  row->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  row->GetViewAccessibility().SetName(u"Spaces");
  const gfx::FontList title_font =
      gfx::FontList().Derive(0, gfx::Font::NORMAL, gfx::Font::Weight::SEMIBOLD);
  for (const auto& space : spaces_) {
    const uint64_t id = space.id;
    const std::u16string& name = space.name;
    const bool current = id == current_space_;
    // Arc's footer glyphs sit 22 DIP above the card's bottom edge.
    auto* column = row->AddChildView(
        Box(Orientation::kVertical, gfx::Insets::TLBR(9, 8, 7, 8), 4));
    const size_t index = cards_.size();
    cards_.push_back(column);
    auto* column_layout =
        static_cast<views::BoxLayout*>(column->GetLayoutManager());
    // Arc's header: the name, and the edit brush at the card's right edge.
    auto* header_row =
        column->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 0));
    static_cast<views::BoxLayout*>(header_row->GetLayoutManager())
        ->set_cross_axis_alignment(CrossAxis::kCenter);
    column->SetBackground(
        views::CreateRoundedRectBackground(colors.column, 10));
    column->SetPreferredSize(gfx::Size(library::kColumnWidth, 0));
    row_layout->SetFlexForView(column, 1);
    auto header =
        TextButton(base::BindRepeating(
                       [](LibraryView* view, uint64_t space) {
                         // Switching rebuilds this column, and a failure can
                         // drop the Library entirely; the action holds nothing
                         // of `view`.
                         view->PostAction(base::BindOnce(
                             [](SidebarView::Delegate* delegate, uint64_t space,
                                const base::RepeatingClosure& close) {
                               delegate->SelectSpace(space);
                               close.Run();
                             },
                             base::Unretained(&view->delegate_.get()), space,
                             view->close_));
                       },
                       base::Unretained(this), id),
                   name);
    for (views::View* child : header->children()) {
      if (auto* label = views::AsViewClass<views::Label>(child)) {
        label->SetFontList(title_font);
      }
    }
    header->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kSaturnIcon, colors.muted, 16));
    // Arc lines the name up with the Pin titles below it.
    header->SetImageLabelSpacing(12);
    if (!current) {
      header->SetEnabledTextColors(colors.muted);
    }
    header->SetTooltipText(
        (current ? u"Current Space: " : u"Switch to Space: ") + name);
    header->GetViewAccessibility().SetName(u"Space: " + name);
    header->GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    header->GetViewAccessibility().SetCheckedState(
        current ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse);
    keyed_views_.emplace_back(id, header.get());
    static_cast<views::BoxLayout*>(header_row->GetLayoutManager())
        ->SetFlexForView(header_row->AddChildView(std::move(header)), 1);
    // Arc's brush customizes the Space: Edit Theme… shows the Space in the
    // sidebar, closing the Library, with the theme popover beside its title.
    auto edit =
        CardButton(base::BindRepeating(
                       [](LibraryView* view, uint64_t space) {
                         view->PostAction(base::BindOnce(
                             &SidebarView::Delegate::EditSpaceTheme,
                             base::Unretained(&view->delegate_.get()), space));
                       },
                       base::Unretained(this), id),
                   kSpaceEditIcon, colors, u"Edit Theme: " + name);
    edit->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 4, 0, 2));
    keyed_views_.emplace_back(id | library::kSpaceEditKey, edit.get());
    header_row->AddChildView(std::move(edit));

    // A long list scrolls inside its card, as in Arc, so every card keeps its
    // header and footer in place.
    auto* pins_scroll =
        column->AddChildView(std::make_unique<views::ScrollView>());
    column_layout->SetFlexForView(pins_scroll, 1);
    pins_scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
    pins_scroll->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    pins_scroll->SetBackgroundColor(std::nullopt);
    pins_scroll->SetDrawOverflowIndicator(false);
    if (std::optional<gfx::PointF> offset = saved(id)) {
      pins_scroll->RegisterPostLayoutCallback(base::BindRepeating(
          [](std::optional<gfx::PointF>& restore, views::ScrollView* scroll) {
            if (restore && !scroll->GetVisibleRect().IsEmpty()) {
              scroll->ScrollToOffset(*std::exchange(restore, std::nullopt));
            }
          },
          base::OwnedRef(std::move(offset))));
    }
    scrolls_.emplace_back(id, pins_scroll);
    auto* pins = pins_scroll->SetContents(
        Box(Orientation::kVertical, gfx::Insets::TLBR(1, 0, 0, 0), 2));
    pins->GetViewAccessibility().SetRole(ax::mojom::Role::kList);
    pins->GetViewAccessibility().SetName(u"Pinned tabs in " + name);
    for (const auto& pin : space.pins) {
      auto pin_row = std::make_unique<PinRow>(
          base::BindRepeating(
              [](LibraryView* view, uint64_t item) {
                // Activating switches to the Pin's Space and reopens a dormant
                // one, as its sidebar row does; the action holds nothing of
                // `view`.
                view->PostAction(base::BindOnce(
                    [](SidebarView::Delegate* delegate, uint64_t item,
                       const base::RepeatingClosure& close) {
                      delegate->ActivateItem(item);
                      close.Run();
                    },
                    base::Unretained(&view->delegate_.get()), item,
                    view->close_));
              },
              base::Unretained(this), pin.id),
          pin);
      keyed_views_.emplace_back(pin.id | library::kPinKey, pin_row.get());
      pins->AddChildView(std::move(pin_row));
    }

    // Arc's footer: the move handle and the "..." menu. Keyboard users move
    // a Space from the menu instead of the handle.
    auto* footer =
        column->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 0));
    auto* footer_layout =
        static_cast<views::BoxLayout*>(footer->GetLayoutManager());
    footer_layout->set_cross_axis_alignment(CrossAxis::kCenter);
    auto* move = footer->AddChildView(std::make_unique<MoveHandle>(
        ui::ImageModel::FromVectorIcon(kSpaceMoveIcon, colors.card_control, 18),
        base::BindRepeating(&LibraryView::OnMoveHandleEvent,
                            base::Unretained(this), index)));
    move->SetPreferredSize(gfx::Size(28, 28));
    move->GetViewAccessibility().SetIsIgnored(true);
    footer_layout->SetFlexForView(
        footer->AddChildView(std::make_unique<views::View>()), 1);
    auto more = CardButton(views::Button::PressedCallback(), kSpaceMoreIcon,
                           colors, u"Space options: " + name);
    // The button owns its callback, so the pointer stays valid.
    more->SetCallback(base::BindRepeating(
        [](LibraryView* view, uint64_t space, views::Button* button,
           const ui::Event& event) {
          view->ShowSpaceMenu(space, button,
                              ui::GetMenuSourceTypeForEvent(event));
        },
        base::Unretained(this), id, more.get()));
    keyed_views_.emplace_back(id | library::kSpaceMoreKey, more.get());
    footer->AddChildView(std::move(more));
  }

  auto* create_cell =
      row->AddChildView(Box(Orientation::kVertical, gfx::Insets(), 0));
  static_cast<views::BoxLayout*>(create_cell->GetLayoutManager())
      ->set_main_axis_alignment(MainAxis::kCenter);
  if (!spaces_.empty()) {
    create_cell->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, library::kCreateGap - library::kColumnGap, 0, 0));
  }
  auto create = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](LibraryView* view) {
        view->PostAction(
            base::BindOnce(&SidebarView::Delegate::CreateSpace,
                           base::Unretained(&view->delegate_.get())));
      },
      base::Unretained(this)));
  create->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kPlusIcon, SK_ColorWHITE, 16));
  create->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  create->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  create->SetPreferredSize(
      gfx::Size(library::kCreateSize, library::kCreateSize));
  create->SetBackground(
      views::CreateRoundedRectBackground(kAccent, library::kCreateSize / 2));
  create->SetFocusBehavior(FocusBehavior::ALWAYS);
  create->SetTooltipText(u"Create Space");
  create->GetViewAccessibility().SetName(u"Create Space");
  views::InstallCircleHighlightPathGenerator(create.get());
  keyed_views_.emplace_back(0, create.get());
  create_cell->AddChildView(std::move(create));
}

void LibraryView::BuildList() {
  const SectionInfo& info = Info(section_);
  const Colors colors = GetColors(GetColorProvider());
  auto* layout = static_cast<views::BoxLayout*>(pane_->GetLayoutManager());
  // Arc insets the search field 8 DIP from the rail, the top and the page.
  auto* content = pane_->AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(8, 8, 10, 8), 8));
  layout->SetFlexForView(content, 1);
  auto* content_layout =
      static_cast<views::BoxLayout*>(content->GetLayoutManager());

  // Arc's search field: 36 DIP tall on the tile fill, a 14pt placeholder 13
  // DIP right of the magnifier. In Archived Tabs, a Filter button sits beside
  // it.
  views::View* search_parent = content;
  if (section_ == Section::kArchive) {
    search_parent =
        content->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 8));
    static_cast<views::BoxLayout*>(search_parent->GetLayoutManager())
        ->set_cross_axis_alignment(CrossAxis::kCenter);
  }
  auto* field = search_parent->AddChildView(
      Box(Orientation::kHorizontal, gfx::Insets::VH(0, 14), 13));
  if (section_ == Section::kArchive) {
    static_cast<views::BoxLayout*>(search_parent->GetLayoutManager())
        ->SetFlexForView(field, 1);
  }
  field->SetBackground(views::CreateRoundedRectBackground(colors.tile, 10));
  field->SetPreferredSize(gfx::Size(0, 36));
  field->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(kSearchIcon, colors.muted, 16)));
  auto search = std::make_unique<views::Textfield>();
  search->SetBackgroundEnabled(false);
  search->SetBackground(nullptr);
  search->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  search->RemoveHoverEffect();
  search->SetFontList(
      gfx::FontList().Derive(1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  search->SetPlaceholderText(info.search);
  search->GetViewAccessibility().SetName(info.search);
  search->SetText(query_);
  search->set_controller(this);
  search->SetEnabled(section_ == Section::kDownloads ||
                     section_ == Section::kArchive ||
                     section_ == Section::kBoosts ||
                     (section_ == Section::kMedia &&
                      delegate_->IsMediaLibraryEnabled()));
  search_ = field->AddChildView(std::move(search));
  static_cast<views::BoxLayout*>(field->GetLayoutManager())
      ->SetFlexForView(search_, 1);
  static_cast<views::BoxLayout*>(field->GetLayoutManager())
      ->set_cross_axis_alignment(CrossAxis::kCenter);

  if (section_ == Section::kArchive) {
    auto filter = TextButton(
        base::BindRepeating(
            [](LibraryView* view, const ui::Event& event) {
              view->ShowArchiveFilterMenu(
                  view->filter_button_, ui::GetMenuSourceTypeForEvent(event));
            },
            base::Unretained(this)),
        u"Filter");
    filter->SetCornerRadius(10);
    filter->SetCustomPadding(gfx::Insets::VH(0, 12));
    filter->SetMinSize(gfx::Size(78, 36));
    filter->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    filter->SetImageLabelSpacing(6);
    filter->SetTooltipText(u"Filter archived tabs");
    filter->GetViewAccessibility().SetName(u"Filter archived tabs");
    filter->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
    filter_button_ = search_parent->AddChildView(std::move(filter));
    UpdateFilterButton();
  }

  list_ = content->AddChildView(Box(Orientation::kVertical, gfx::Insets(), 0));
  content_layout->SetFlexForView(list_, 1);
  downloads_.clear();
  archived_.clear();
  boosts_.clear();
  media_.clear();
  if (section_ == Section::kDownloads) {
    downloads_ = delegate_->GetRecentDownloads();
  } else if (section_ == Section::kArchive) {
    archived_ = delegate_->GetArchivedTabs();
  } else if (section_ == Section::kBoosts) {
    boosts_ = delegate_->GetBoosts();
    boosts_global_enabled_ = delegate_->AllBoostsEnabled();
  } else if (section_ == Section::kMedia) {
    media_enabled_ = delegate_->IsMediaLibraryEnabled();
    media_permission_denied_ = delegate_->IsMediaLibraryPermissionDenied();
    if (!media_shown_) {
      media_shown_ = true;
      delegate_->SetMediaLibraryShown(true);
    }
    if (media_enabled_) {
      media_ = delegate_->GetMediaItems();
    }
  }
  UpdateList(/*keep_scroll=*/false);
}

void LibraryView::AddBoostRows(views::View& rows) {
  for (const auto& boost : boosts_) {
    if (!library::MatchesSearch(boost.name, query_) &&
        !library::MatchesSearch(boost.host, query_)) {
      continue;
    }
    auto row = std::make_unique<BoostRow>(
        base::BindRepeating(
            [](LibraryView* view, uint64_t id) {
              view->PostAction(base::BindOnce(
                  [](SidebarView::Delegate* delegate, uint64_t id,
                     const base::RepeatingClosure& close) {
                    close.Run();
                    delegate->OpenBoost(id);
                  },
                  base::Unretained(&view->delegate_.get()), id, view->close_));
            },
            base::Unretained(this), boost.id),
        boost, boosts_global_enabled_);
    row->set_context_menu_controller(this);
    keyed_views_.emplace_back(boost.id, row.get());
    rows.AddChildView(std::move(row));
  }
}

void LibraryView::AddArchiveRows(views::View& rows) {
  const Colors colors = GetColors(GetColorProvider());
  const base::Time now = base::Time::Now();
  std::u16string day;
  for (const auto& tab : archived_) {
    if ((!library::MatchesSearch(tab.title, query_) &&
         !library::MatchesSearch(tab.url, query_)) ||
        !library::MatchesArchiveFilter(tab.space_id, tab.archived, now,
                                       archive_filter_.space_id,
                                       archive_filter_.range)) {
      continue;
    }
    // Entries come newest first, so each day's header appears once.
    if (std::u16string label = library::DayLabel(tab.archived, now);
        label != day) {
      day = std::move(label);
      auto* header = rows.AddChildView(
          MakeLabel(day,
                    gfx::FontList().Derive(0, gfx::Font::NORMAL,
                                           gfx::Font::Weight::SEMIBOLD),
                    colors.muted));
      header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      header->SetBorder(
          views::CreateEmptyBorder(gfx::Insets::TLBR(10, 6, 6, 6)));
      header->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    }
    auto row = std::make_unique<ArchiveRow>(
        base::BindRepeating(
            [](LibraryView* view, uint64_t id) {
              // A second click before the first reopen runs is ignored.
              if (!view->reopening_.insert(id).second) {
                return;
              }
              // Reopening switches Space and rebuilds the sidebar, so the
              // action holds nothing of `view`.
              view->PostAction(base::BindOnce(
                  [](SidebarView::Delegate* delegate, uint64_t id,
                     const base::RepeatingClosure& close) {
                    delegate->RestoreArchivedTab(id);
                    close.Run();
                  },
                  base::Unretained(&view->delegate_.get()), id, view->close_));
            },
            base::Unretained(this), tab.id),
        tab);
    keyed_views_.emplace_back(tab.id, row.get());
    rows.AddChildView(std::move(row));
  }
}

void LibraryView::UpdateList(bool keep_scroll) {
  if (!list_) {
    return;
  }
  // A refresh (a closed tab, a favicon) keeps the reader's place; a new query
  // or section starts at the top.
  std::optional<gfx::PointF> offset;
  if (keep_scroll && !list_->children().empty()) {
    if (const auto* scroll =
            views::AsViewClass<views::ScrollView>(list_->children().front())) {
      offset = scroll->CurrentOffset();
    }
  }
  const views::FocusManager* manager = GetFocusManager();
  const bool had_focus =
      list_->Contains(manager ? manager->GetFocusedView() : nullptr);
  const std::optional<uint64_t> key = FocusedKey();
  keyed_views_.clear();
  HideMediaPill();
  list_->RemoveAllChildViews();
  const Colors colors = GetColors(GetColorProvider());
  auto* list_layout = static_cast<views::BoxLayout*>(list_->GetLayoutManager());

  if (section_ == Section::kMedia) {
    visible_media_.clear();
    if (!media_enabled_) {
      BuildMediaOptIn();
      if (had_focus && !RestoreFocus(key)) {
        FocusSection();
      }
      return;
    }
    if (media_permission_denied_) {
      auto* empty = list_->AddChildView(
          Box(Orientation::kVertical, gfx::Insets::VH(0, 16), 8));
      list_layout->SetFlexForView(empty, 1);
      auto* empty_layout =
          static_cast<views::BoxLayout*>(empty->GetLayoutManager());
      empty_layout->set_main_axis_alignment(MainAxis::kCenter);
      empty_layout->set_cross_axis_alignment(CrossAxis::kCenter);
      auto* image = empty->AddChildView(std::make_unique<views::ImageView>(
          ui::ImageModel::FromVectorIcon(kMediaActiveIcon, colors.empty_icon,
                                         44)));
      image->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 8, 0));
      const SkColor on_surface =
          GetColorProvider()
              ? GetColorProvider()->GetColor(ui::kColorSysOnSurface)
              : SK_ColorWHITE;
      empty->AddChildView(
          MakeLabel(u"Folder access needed",
                    gfx::FontList().Derive(2, gfx::Font::NORMAL,
                                           gfx::Font::Weight::SEMIBOLD),
                    on_surface));
      auto* body_label = empty->AddChildView(
          MakeLabel(u"Allow myArc to read Desktop and Downloads in macOS "
                    u"Privacy & Security settings.",
                    gfx::FontList(), colors.empty_body));
      body_label->SetMultiLine(true);
      body_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      body_label->SizeToFit(248);
      auto open_settings = TextButton(
          base::BindRepeating(
              [](LibraryView* view) {
                view->PostAction(base::BindOnce(
                    &SidebarView::Delegate::OpenMediaPrivacySettings,
                    base::Unretained(&view->delegate_.get())));
              },
              base::Unretained(this)),
          u"Open System Settings");
      open_settings->SetBgColorOverrideDeprecated(colors.tile);
      open_settings->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      open_settings->SetCustomPadding(gfx::Insets::VH(6, 14));
      open_settings->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(10, 0, 0, 0));
      empty->AddChildView(std::move(open_settings));
      return;
    }
    for (const auto& item : media_) {
      if (library::MatchesSearch(item.filename, query_)) {
        visible_media_.push_back(item);
      }
    }
    if (!visible_media_.empty()) {
      BuildMediaGrid(visible_media_, std::move(offset));
      if (had_focus && !RestoreFocus(key)) {
        search_->RequestFocus();
      }
      return;
    }
  }

  std::vector<SidebarView::DownloadPresentation> downloads;
  for (const auto& download : downloads_) {
    if (library::MatchesSearch(download.name, query_)) {
      downloads.push_back(download);
    }
  }
  const base::Time now = base::Time::Now();
  const bool any_archived =
      std::ranges::any_of(archived_, [this, now](const auto& tab) {
        return (library::MatchesSearch(tab.title, query_) ||
                library::MatchesSearch(tab.url, query_)) &&
               library::MatchesArchiveFilter(tab.space_id, tab.archived, now,
                                             archive_filter_.space_id,
                                             archive_filter_.range);
      });
  const bool any_boosts =
      std::ranges::any_of(boosts_, [this](const auto& boost) {
        return library::MatchesSearch(boost.name, query_) ||
               library::MatchesSearch(boost.host, query_);
      });

  if (downloads.empty() && !any_archived && !any_boosts) {
    const gfx::VectorIcon* icon = Info(section_).icon;
    std::u16string title = u"Nothing here yet!";
    std::u16string body =
        u"Come back after you\u2019ve found some cool stuff to save.";
    switch (section_) {
      case Section::kDownloads:
        if (!query_.empty()) {
          title = u"No matches";
          body = u"No downloads match your search.";
        }
        break;
      case Section::kArchive:
        body =
            u"Hover your mouse over a tab and press the \u00D7 to archive "
            u"it.";
        if (!query_.empty() || archive_filter_.IsActive()) {
          title = u"No matches";
          body = archive_filter_.IsActive()
                     ? u"No archived tabs match your search or filter."
                     : u"No archived tabs match your search.";
        }
        break;
      case Section::kBoosts:
        body =
            u"Customize any website\u2019s colors, fonts, or layout with a "
            u"Boost.";
        if (!query_.empty()) {
          title = u"No matches";
          body = u"No Boosts match your search.";
        }
        break;
      case Section::kMedia:
        title = query_.empty() ? u"No media found" : u"No matches";
        body = query_.empty()
                   ? u"Screenshots and recent downloads on this Mac will "
                     u"appear here."
                   : u"No media files match your search.";
        break;
      case Section::kSpaces:
        NOTREACHED();
    }
    auto* empty = list_->AddChildView(
        Box(Orientation::kVertical, gfx::Insets::VH(0, 16), 6));
    list_layout->SetFlexForView(empty, 1);
    auto* empty_layout =
        static_cast<views::BoxLayout*>(empty->GetLayoutManager());
    empty_layout->set_main_axis_alignment(MainAxis::kCenter);
    empty_layout->set_cross_axis_alignment(CrossAxis::kCenter);
    auto* image = empty->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(*icon, colors.empty_icon, 48)));
    image->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 12, 0));
    const SkColor on_surface =
        GetColorProvider()
            ? GetColorProvider()->GetColor(ui::kColorSysOnSurface)
            : SK_ColorWHITE;
    empty->AddChildView(
        MakeLabel(title,
                  gfx::FontList().Derive(3, gfx::Font::NORMAL,
                                         gfx::Font::Weight::SEMIBOLD),
                  on_surface));
    auto* body_label = empty->AddChildView(
        MakeLabel(body, gfx::FontList(), colors.empty_body));
    body_label->SetMultiLine(true);
    body_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    // Arc wraps its empty-state text at about 248 DIP.
    body_label->SizeToFit(248);
    if (section_ == Section::kArchive && archive_filter_.IsActive()) {
      auto clear = TextButton(
          base::BindRepeating(
              [](LibraryView* view) {
                view->archive_filter_ = {};
                view->UpdateFilterButton();
                view->UpdateList(/*keep_scroll=*/false);
              },
              base::Unretained(this)),
          u"Clear Filters");
      clear->SetBgColorOverrideDeprecated(colors.tile);
      clear->SetHorizontalAlignment(gfx::ALIGN_CENTER);
      clear->SetCustomPadding(gfx::Insets::VH(6, 14));
      clear->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, 0, 0, 0));
      keyed_views_.emplace_back(library::kArchiveFilterKey, clear.get());
      empty->AddChildView(std::move(clear));
    }
    if (section_ == Section::kBoosts && query_.empty()) {
      auto create_boost = std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              [](LibraryView* view) {
                view->PostAction(base::BindOnce(
                    [](SidebarView::Delegate* delegate,
                       const base::RepeatingClosure& close) {
                      close.Run();
                      delegate->NewBoost();
                    },
                    base::Unretained(&view->delegate_.get()), view->close_));
              },
              base::Unretained(this)),
          u"New Boost");
      create_boost->SetStyle(ui::ButtonStyle::kProminent);
      create_boost->SetBgColorOverrideDeprecated(kAccent);
      create_boost->SetEnabledTextColors(SK_ColorWHITE);
      create_boost->SetImageModel(
          views::Button::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(kPlusIcon, SK_ColorWHITE, 14));
      create_boost->SetImageLabelSpacing(6);
      create_boost->SetCornerRadius(8);
      create_boost->SetCustomPadding(gfx::Insets::VH(8, 16));
      create_boost->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(14, 0, 0, 0));
      create_boost->GetViewAccessibility().SetName(u"New Boost");
      keyed_views_.emplace_back(library::kNewBoostKey, create_boost.get());
      empty->AddChildView(std::move(create_boost));
    }
  } else {
    auto* scroll = list_->AddChildView(std::make_unique<views::ScrollView>());
    scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
    scroll->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    scroll->SetBackgroundColor(std::nullopt);
    scroll->SetDrawOverflowIndicator(false);
    list_layout->SetFlexForView(scroll, 1);
    if (offset) {
      scroll->RegisterPostLayoutCallback(base::BindRepeating(
          [](std::optional<gfx::PointF>& saved, views::ScrollView* scroll) {
            if (auto restore = std::exchange(saved, std::nullopt)) {
              scroll->ScrollToOffset(*restore);
            }
          },
          base::OwnedRef(std::move(offset))));
    }
    auto* rows =
        scroll->SetContents(Box(Orientation::kVertical, gfx::Insets(), 2));
    rows->GetViewAccessibility().SetRole(ax::mojom::Role::kList);
    rows->GetViewAccessibility().SetName(
        section_ == Section::kArchive  ? u"Archived tabs"
        : section_ == Section::kBoosts ? u"Boosts"
                                       : u"Recent downloads");
    AddArchiveRows(*rows);
    AddBoostRows(*rows);
    for (const auto& download : downloads) {
      auto row =
          TextButton(base::BindRepeating(
                         [](LibraryView* view, uint32_t id) {
                           view->PostAction(base::BindOnce(
                               &SidebarView::Delegate::OpenDownload,
                               base::Unretained(&view->delegate_.get()), id));
                         },
                         base::Unretained(this), download.id),
                     download.name);
      row->SetImageModel(views::Button::STATE_NORMAL,
                         ui::ImageModel::FromVectorIcon(kCircleArrowDownIcon,
                                                        colors.muted, 16));
      row->SetMinSize(gfx::Size(0, 36));
      row->SetTooltipText(download.detail.empty()
                              ? download.name
                              : download.name + u"\n" + download.detail);
      row->GetViewAccessibility().SetName(u"Open " + download.name);
      row->GetViewAccessibility().SetDescription(download.detail);
      keyed_views_.emplace_back(download.id, row.get());
      rows->AddChildView(std::move(row));
    }
  }

  if (section_ == Section::kDownloads) {
    // Chrome's full Downloads page keeps every download action reachable.
    auto all = TextButton(base::BindRepeating(
                              [](LibraryView* view) {
                                view->PostAction(base::BindOnce(
                                    &SidebarView::Delegate::OpenDownloads,
                                    base::Unretained(&view->delegate_.get())));
                              },
                              base::Unretained(this)),
                          u"Show All Downloads");
    all->SetEnabledTextColors(colors.muted);
    keyed_views_.emplace_back(0, all.get());
    list_->AddChildView(std::move(all));
  } else if (section_ == Section::kBoosts) {
    auto* footer =
        list_->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 4));
    auto* footer_layout =
        static_cast<views::BoxLayout*>(footer->GetLayoutManager());
    footer_layout->set_cross_axis_alignment(CrossAxis::kCenter);

    // Quick-create Boost button, always reachable in the Boosts panel.
    auto new_boost = TextButton(
        base::BindRepeating(
            [](LibraryView* view) {
              view->PostAction(base::BindOnce(
                  [](SidebarView::Delegate* delegate,
                     const base::RepeatingClosure& close) {
                    close.Run();
                    delegate->NewBoost();
                  },
                  base::Unretained(&view->delegate_.get()), view->close_));
            },
            base::Unretained(this)),
        u"New Boost");
    new_boost->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kPlusIcon, kAccent, 14));
    new_boost->SetEnabledTextColors(kAccent);
    new_boost->SetImageLabelSpacing(6);
    new_boost->GetViewAccessibility().SetName(u"New Boost");
    if (query_.empty() && boosts_.empty()) {
      // Empty state already registers kNewBoostKey on its prominent CTA.
    } else {
      keyed_views_.emplace_back(library::kNewBoostKey, new_boost.get());
    }
    footer->AddChildView(std::move(new_boost));

    // Profile-wide Boosts kill switch.
    const std::u16string label = boosts_global_enabled_
                                     ? u"Disable All Boosts"
                                     : u"Enable All Boosts";
    auto kill_switch = TextButton(
        base::BindRepeating(
            [](LibraryView* view, bool enable) {
              view->PostAction(base::BindOnce(
                  [](LibraryView* view, bool enable) {
                    view->delegate_->SetAllBoostsEnabled(enable);
                    view->RefreshBoosts();
                  },
                  base::Unretained(view), enable));
            },
            base::Unretained(this), !boosts_global_enabled_),
        label);
    kill_switch->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            kBoostsIcon,
            boosts_global_enabled_ ? colors.muted : kAccent, 16));
    kill_switch->SetEnabledTextColors(
        boosts_global_enabled_ ? colors.muted : kAccent);
    kill_switch->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    kill_switch->GetViewAccessibility().SetName(label);
    keyed_views_.emplace_back(0, kill_switch.get());
    footer_layout->SetFlexForView(footer->AddChildView(std::move(kill_switch)),
                                  1);
  }
  // A refresh keeps focus on the same row, else on the search field.
  if (had_focus && !RestoreFocus(key)) {
    search_->RequestFocus();
  }
}

void LibraryView::BuildMediaOptIn() {
  const Colors colors = GetColors(GetColorProvider());
  auto* list_layout = static_cast<views::BoxLayout*>(list_->GetLayoutManager());
  const SkColor on_surface =
      GetColorProvider() ? GetColorProvider()->GetColor(ui::kColorSysOnSurface)
                         : SK_ColorWHITE;

  if (media_card_dismissed_) {
    auto* minimal = list_->AddChildView(
        Box(Orientation::kVertical, gfx::Insets::VH(0, 20), 10));
    list_layout->SetFlexForView(minimal, 1);
    auto* minimal_layout =
        static_cast<views::BoxLayout*>(minimal->GetLayoutManager());
    minimal_layout->set_main_axis_alignment(MainAxis::kCenter);
    minimal_layout->set_cross_axis_alignment(CrossAxis::kCenter);
    minimal->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(kMediaActiveIcon, colors.empty_icon,
                                       36)));
    auto cta = std::make_unique<GradientCtaButton>(
        base::BindRepeating(
            [](LibraryView* view) {
              view->media_card_dismissed_ = false;
              view->delegate_->SetMediaLibraryEnabled(true);
              view->RefreshMedia();
            },
            base::Unretained(this)),
        u"Show Media");
    cta->SetPreferredSize(gfx::Size(160, 36));
    minimal->AddChildView(std::move(cta));
    return;
  }

  // Stacked FillLayout: subtle skeleton placeholder rows behind the centered
  // floating dark opt-in card (matching Arc's Media empty-state screenshot).
  auto* stack = list_->AddChildView(std::make_unique<views::View>());
  stack->SetLayoutManager(std::make_unique<views::FillLayout>());
  list_layout->SetFlexForView(stack, 1);

  auto* skeleton = stack->AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(8, 2, 0, 2), 10));
  const std::array<std::u16string, 3> buckets = {
      u"Today", u"Yesterday", u"Earlier This Week"};
  for (const auto& label_text : buckets) {
    auto* heading = skeleton->AddChildView(MakeLabel(
        label_text,
        gfx::FontList().Derive(0, gfx::Font::NORMAL,
                               gfx::Font::Weight::SEMIBOLD),
        SkColorSetA(colors.muted, 0x66)));
    heading->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    auto* pair = skeleton->AddChildView(
        Box(Orientation::kHorizontal, gfx::Insets(), 8));
    auto* pair_layout = static_cast<views::BoxLayout*>(pair->GetLayoutManager());
    for (int col = 0; col < 2; ++col) {
      auto* ph = pair->AddChildView(std::make_unique<views::View>());
      ph->SetPreferredSize(gfx::Size(136, 92));
      ph->SetBackground(views::CreateRoundedRectBackground(
          SkColorSetA(SK_ColorWHITE, 0x0E), 8));
      pair_layout->SetFlexForView(ph, 1);
    }
  }

  auto* overlay = stack->AddChildView(
      Box(Orientation::kVertical, gfx::Insets::VH(0, 14), 0));
  auto* overlay_layout =
      static_cast<views::BoxLayout*>(overlay->GetLayoutManager());
  overlay_layout->set_main_axis_alignment(MainAxis::kCenter);
  overlay_layout->set_cross_axis_alignment(CrossAxis::kStretch);

  auto* card = overlay->AddChildView(
      Box(Orientation::kVertical, gfx::Insets::TLBR(14, 18, 18, 18), 10));
  auto* card_layout = static_cast<views::BoxLayout*>(card->GetLayoutManager());
  card_layout->set_cross_axis_alignment(CrossAxis::kStretch);
  card->SetBackground(views::CreateRoundedRectBackground(
      SkColorSetRGB(0x28, 0x29, 0x36), 14));
  card->SetBorder(views::CreateRoundedRectBorder(
      1, 14, SkColorSetA(SK_ColorWHITE, 0x1A)));

  auto* top_row =
      card->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 0));
  auto* top_layout =
      static_cast<views::BoxLayout*>(top_row->GetLayoutManager());
  top_layout->SetFlexForView(
      top_row->AddChildView(std::make_unique<views::View>()), 1);
  auto dismiss = std::make_unique<views::ImageButton>(base::BindRepeating(
      [](LibraryView* view) {
        view->media_card_dismissed_ = true;
        view->UpdateList(/*keep_scroll=*/false);
      },
      base::Unretained(this)));
  dismiss->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseIcon, colors.muted,
                                     14));
  dismiss->SetPreferredSize(gfx::Size(22, 22));
  dismiss->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  dismiss->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  dismiss->SetTooltipText(u"Dismiss");
  dismiss->GetViewAccessibility().SetName(u"Dismiss");
  top_row->AddChildView(std::move(dismiss));

  auto* icon_row =
      card->AddChildView(Box(Orientation::kHorizontal, gfx::Insets(), 0));
  static_cast<views::BoxLayout*>(icon_row->GetLayoutManager())
      ->set_main_axis_alignment(MainAxis::kCenter);
  icon_row->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(kMediaActiveIcon,
                                     SkColorSetRGB(0xD8, 0x9C, 0xE8), 36)));

  auto* title = card->AddChildView(MakeLabel(
      u"Your Media in the Library",
      gfx::FontList().Derive(2, gfx::Font::NORMAL, gfx::Font::Weight::SEMIBOLD),
      on_surface));
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  auto* body = card->AddChildView(
      MakeLabel(u"Quickly find, preview, and drag recent screenshots and "
                u"downloads into any page.",
                gfx::FontList(), colors.empty_body));
  body->SetMultiLine(true);
  body->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  body->SizeToFit(212);

  auto cta = std::make_unique<GradientCtaButton>(
      base::BindRepeating(
          [](LibraryView* view) {
            view->delegate_->SetMediaLibraryEnabled(true);
            view->RefreshMedia();
          },
          base::Unretained(this)),
      u"Show Media");
  cta->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(4, 0, 2, 0));
  card->AddChildView(std::move(cta));

  auto* privacy = card->AddChildView(
      Box(Orientation::kHorizontal, gfx::Insets(), 5));
  auto* privacy_layout =
      static_cast<views::BoxLayout*>(privacy->GetLayoutManager());
  privacy_layout->set_main_axis_alignment(MainAxis::kCenter);
  privacy_layout->set_cross_axis_alignment(CrossAxis::kCenter);
  privacy->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(vector_icons::kLockIcon, colors.muted,
                                     11)));
  privacy->AddChildView(MakeLabel(
      u"Files never leave your computer.",
      gfx::FontList().Derive(-2, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL),
      colors.muted));
}

void LibraryView::BuildMediaGrid(
    const std::vector<SidebarView::MediaItemPresentation>& items,
    std::optional<gfx::PointF> offset) {
  const Colors colors = GetColors(GetColorProvider());
  auto* list_layout = static_cast<views::BoxLayout*>(list_->GetLayoutManager());
  auto* scroll = list_->AddChildView(std::make_unique<views::ScrollView>());
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetDrawOverflowIndicator(false);
  list_layout->SetFlexForView(scroll, 1);

  if (offset) {
    scroll->RegisterPostLayoutCallback(base::BindRepeating(
        [](std::optional<gfx::PointF>& saved, views::ScrollView* scroll) {
          if (auto restore = std::exchange(saved, std::nullopt)) {
            scroll->ScrollToOffset(*restore);
          }
        },
        base::OwnedRef(std::move(offset))));
  }

  auto* container = scroll->SetContents(
      Box(Orientation::kVertical, gfx::Insets::TLBR(2, 0, 10, 0), 8));
  const base::Time now = base::Time::Now();
  std::u16string current_group;
  views::View* current_row = nullptr;
  int col_in_row = 0;

  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];
    const std::u16string group = library::DayLabel(item.modified, now);
    if (group != current_group) {
      current_group = group;
      current_row = nullptr;
      col_in_row = 0;
      auto* header = container->AddChildView(MakeLabel(
          group,
          gfx::FontList().Derive(0, gfx::Font::NORMAL,
                                 gfx::Font::Weight::SEMIBOLD),
          colors.muted));
      header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      header->SetBorder(
          views::CreateEmptyBorder(gfx::Insets::TLBR(8, 4, 2, 4)));
      header->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    }

    if (!current_row || col_in_row == 2) {
      current_row = container->AddChildView(
          Box(Orientation::kHorizontal, gfx::Insets(), 8));
      col_in_row = 0;
    }

    auto tile = std::make_unique<MediaTile>(
        base::BindRepeating(&LibraryView::OpenMediaLightbox,
                            base::Unretained(this), i),
        base::BindRepeating(
            [](LibraryView* view, views::View* tile,
               const std::u16string& filename, bool hovered) {
              if (hovered) {
                view->ShowMediaPill(tile, filename);
              } else {
                view->HideMediaPill();
              }
            },
            base::Unretained(this)),
        base::BindRepeating(
            [](LibraryView* view, uint64_t id) {
              view->delegate_->DragMediaItem(id);
            },
            base::Unretained(this)),
        item);
    tile->set_context_menu_controller(this);
    keyed_views_.emplace_back(item.id, tile.get());
    auto* row_layout =
        static_cast<views::BoxLayout*>(current_row->GetLayoutManager());
    row_layout->SetFlexForView(current_row->AddChildView(std::move(tile)), 1);
    ++col_in_row;
  }

  // Pad trailing single-item row so the tile keeps 50% width.
  if (current_row && col_in_row == 1) {
    auto* spacer = current_row->AddChildView(std::make_unique<views::View>());
    static_cast<views::BoxLayout*>(current_row->GetLayoutManager())
        ->SetFlexForView(spacer, 1);
  }
}

void LibraryView::ShowMediaPill(views::View* tile,
                                const std::u16string& filename) {
  if (!tile || !GetWidget() || lightbox_) {
    return;
  }
  if (!media_pill_) {
    media_pill_ = std::make_unique<MediaFilenamePill>();
  }
  media_pill_->Show(GetWidget(), tile->GetBoundsInScreen(), filename);
}

void LibraryView::HideMediaPill() {
  if (media_pill_) {
    media_pill_->Hide();
  }
}

void LibraryView::OpenMediaLightbox(size_t index) {
  if (index >= visible_media_.size() || !GetWidget()) {
    return;
  }
  HideMediaPill();
  lightbox_return_key_ = visible_media_[index].id;
  lightbox_ = std::make_unique<MediaLightbox>(
      *GetWidget(), delegate_.get(), visible_media_, index,
      base::BindOnce(&LibraryView::CloseMediaLightbox,
                     weak_factory_.GetWeakPtr()));
}

void LibraryView::CloseMediaLightbox() {
  if (!lightbox_) {
    return;
  }
  lightbox_.reset();
  if (lightbox_return_key_ != 0) {
    RestoreFocus(lightbox_return_key_);
    lightbox_return_key_ = 0;
  }
}

void LibraryView::ShowSpaceMenu(uint64_t space,
                                views::View* anchor,
                                ui::mojom::MenuSourceType source_type) {
  if (!anchor || !GetWidget() || (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  // Theme, icon, rename, switching and Delete are the Space actions the
  // sidebar has too.
  menu_space_ = space;
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  if (space != current_space_) {
    menu_model_->AddItem(kSwitchSpace, u"Switch to Space");
  }
  menu_model_->AddItem(kEditTheme, u"Edit Theme\u2026");
  menu_model_->AddItem(kChangeIcon, u"Change Icon\u2026");
  menu_model_->AddItem(kRenameSpace, u"Rename Space\u2026");
  // The handle's drag, for the keyboard.
  const auto it =
      std::ranges::find(spaces_, space, &SidebarView::LibrarySpace::id);
  if (it != spaces_.begin() && it != spaces_.end()) {
    menu_model_->AddItem(kMoveSpaceLeft, u"Move Left");
  }
  if (it != spaces_.end() && std::next(it) != spaces_.end()) {
    menu_model_->AddItem(kMoveSpaceRight, u"Move Right");
  }
  // As in the sidebar, the last Space can't be deleted.
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(kDeleteSpace, u"Delete Space\u2026");
  menu_model_->SetEnabledAt(menu_model_->GetItemCount() - 1,
                            spaces_.size() > 1);
  menu_runner_ =
      std::make_unique<views::MenuRunner>(menu_model_.get(), /*run_types=*/0);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, anchor->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight, source_type);
}

void LibraryView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (!GetWidget() || (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  if (auto* boost_row = views::AsViewClass<BoostRow>(source)) {
    menu_boost_ = boost_row->boost_id();
    menu_boost_enabled_ = boost_row->boost_enabled();
    menu_model_->AddItem(kEditBoost, u"Edit Boost\u2026");
    menu_model_->AddItem(kToggleBoost, menu_boost_enabled_ ? u"Disable Boost"
                                                           : u"Enable Boost");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kDeleteBoost, u"Delete Boost");
  } else if (auto* media_tile = views::AsViewClass<MediaTile>(source)) {
    HideMediaPill();
    menu_media_ = media_tile->media_id();
    menu_model_->AddItem(kMediaOpenPreview, u"Open in Preview");
    menu_model_->AddItem(kMediaCopy, u"Copy");
    menu_model_->AddItem(kMediaRevealFinder, u"Reveal in Finder");
  } else if (source == media_rail_item_) {
    menu_model_->AddItem(kHideMedia, u"Hide Media in Library");
    menu_model_->SetEnabledAt(0, delegate_->IsMediaLibraryEnabled());
  } else {
    menu_model_->AddItem(kClearArchive, u"Clear Archive");
    // An empty archive has nothing to clear.
    menu_model_->SetEnabledAt(0, !delegate_->GetArchivedTabs().empty());
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void LibraryView::UpdateFilterButton() {
  if (!filter_button_) {
    return;
  }
  const Colors colors = GetColors(GetColorProvider());
  const bool active = archive_filter_.IsActive();
  const SkColor on_surface =
      GetColorProvider() ? GetColorProvider()->GetColor(ui::kColorSysOnSurface)
                         : SK_ColorWHITE;
  filter_button_->SetBgColorOverrideDeprecated(
      active ? SkColorSetA(kAccent, 0x38) : colors.tile);
  filter_button_->SetEnabledTextColors(active ? on_surface : colors.muted);
  filter_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kFilterIcon,
                                     active ? on_surface : colors.muted, 15));
}

void LibraryView::ShowArchiveFilterMenu(views::View* anchor,
                                        ui::mojom::MenuSourceType source_type) {
  if (!GetWidget() || !anchor ||
      (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  filter_space_ids_.clear();

  menu_model_->AddCheckItem(kFilterAllSpaces, u"All Spaces");
  for (size_t i = 0; i < spaces_.size(); ++i) {
    filter_space_ids_.push_back(spaces_[i].id);
    menu_model_->AddCheckItem(kFilterSpaceBase + static_cast<int>(i),
                              spaces_[i].name);
  }
  const bool has_deleted_space = std::ranges::any_of(
      archived_, [](const auto& tab) { return tab.space_id == 0; });
  if (has_deleted_space) {
    menu_model_->AddCheckItem(kFilterDeletedSpaces, u"Other / Deleted Spaces");
  }
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddCheckItem(kFilterTimeAny, u"Any Time");
  menu_model_->AddCheckItem(kFilterTimeToday, u"Today");
  menu_model_->AddCheckItem(kFilterTimeLast7Days, u"Last 7 Days");
  menu_model_->AddCheckItem(kFilterTimeLast30Days, u"Last 30 Days");
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(kFilterReset, u"Reset Filters");

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  const gfx::Rect bounds = anchor->GetBoundsInScreen();
  menu_runner_->RunMenuAt(GetWidget(), nullptr,
                          gfx::Rect(bounds.bottom_left(), gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

bool LibraryView::IsCommandIdChecked(int command_id) const {
  if (command_id == kFilterAllSpaces) {
    return !archive_filter_.space_id.has_value();
  }
  if (command_id == kFilterDeletedSpaces) {
    return archive_filter_.space_id == 0u;
  }
  if (command_id >= kFilterSpaceBase) {
    const size_t idx = static_cast<size_t>(command_id - kFilterSpaceBase);
    return idx < filter_space_ids_.size() &&
           archive_filter_.space_id == filter_space_ids_[idx];
  }
  switch (command_id) {
    case kFilterTimeAny:
      return archive_filter_.range == library::ArchiveTimeRange::kAny;
    case kFilterTimeToday:
      return archive_filter_.range == library::ArchiveTimeRange::kToday;
    case kFilterTimeLast7Days:
      return archive_filter_.range == library::ArchiveTimeRange::kLast7Days;
    case kFilterTimeLast30Days:
      return archive_filter_.range == library::ArchiveTimeRange::kLast30Days;
    default:
      return false;
  }
}

bool LibraryView::IsCommandIdEnabled(int command_id) const {
  if (command_id == kFilterReset) {
    return archive_filter_.IsActive();
  }
  return ui::SimpleMenuModel::Delegate::IsCommandIdEnabled(command_id);
}

void LibraryView::ExecuteCommand(int command_id, int event_flags) {
  // Each action, its dialog and the refresh it triggers run after the menu
  // closes.
  switch (command_id) {
    case kClearArchive:
      PostAction(base::BindOnce(&SidebarView::Delegate::ClearArchive,
                                base::Unretained(&delegate_.get())));
      break;
    case kSwitchSpace:
      // As the header does: switching can drop the Library, so the action
      // holds nothing of this view.
      PostAction(base::BindOnce(
          [](SidebarView::Delegate* delegate, uint64_t space,
             const base::RepeatingClosure& close) {
            delegate->SelectSpace(space);
            close.Run();
          },
          base::Unretained(&delegate_.get()), menu_space_, close_));
      break;
    case kRenameSpace:
      PostAction(base::BindOnce(&SidebarView::Delegate::RenameSpace,
                                base::Unretained(&delegate_.get()),
                                menu_space_));
      break;
    case kEditTheme:
    case kChangeIcon:
      // The parent shows the Space in the sidebar, closing the Library, and
      // opens the picker beside its title.
      PostAction(base::BindOnce(
          command_id == kEditTheme ? &SidebarView::Delegate::EditSpaceTheme
                                   : &SidebarView::Delegate::ChangeSpaceIcon,
          base::Unretained(&delegate_.get()), menu_space_));
      break;
    case kDeleteSpace:
      // The confirmation runs first; the Library stays open and rebuilds
      // its cards once the Space is gone.
      PostAction(base::BindOnce(&SidebarView::Delegate::DeleteSpace,
                                base::Unretained(&delegate_.get()),
                                menu_space_));
      break;
    case kMoveSpaceLeft:
    case kMoveSpaceRight: {
      const std::vector<uint64_t> ids = SpaceIds();
      const auto it = std::ranges::find(ids, menu_space_);
      if (it == ids.end()) {
        break;
      }
      const size_t source = static_cast<size_t>(it - ids.begin());
      const bool left = command_id == kMoveSpaceLeft;
      if ((left && source == 0) || (!left && source + 1 == ids.size())) {
        break;
      }
      MoveSpace(menu_space_, library::SpaceMoveBefore(
                                 ids, source, left ? source - 1 : source + 1));
      break;
    }
    case kEditBoost:
      PostAction(base::BindOnce(
          [](SidebarView::Delegate* delegate, uint64_t id,
             const base::RepeatingClosure& close) {
            close.Run();
            delegate->OpenBoost(id);
          },
          base::Unretained(&delegate_.get()), menu_boost_, close_));
      break;
    case kToggleBoost:
      PostAction(base::BindOnce(
          [](LibraryView* view, uint64_t id, bool enable) {
            view->delegate_->SetBoostEnabled(id, enable);
            view->RefreshBoosts();
          },
          base::Unretained(this), menu_boost_, !menu_boost_enabled_));
      break;
    case kDeleteBoost:
      PostAction(base::BindOnce(
          [](LibraryView* view, uint64_t id) {
            view->delegate_->DeleteBoost(id);
            view->RefreshBoosts();
          },
          base::Unretained(this), menu_boost_));
      break;
    case kFilterAllSpaces:
      archive_filter_.space_id = std::nullopt;
      UpdateFilterButton();
      UpdateList(/*keep_scroll=*/false);
      break;
    case kFilterDeletedSpaces:
      archive_filter_.space_id = 0u;
      UpdateFilterButton();
      UpdateList(/*keep_scroll=*/false);
      break;
    case kFilterTimeAny:
      archive_filter_.range = library::ArchiveTimeRange::kAny;
      UpdateFilterButton();
      UpdateList(/*keep_scroll=*/false);
      break;
    case kFilterTimeToday:
      archive_filter_.range = library::ArchiveTimeRange::kToday;
      UpdateFilterButton();
      UpdateList(/*keep_scroll=*/false);
      break;
    case kFilterTimeLast7Days:
      archive_filter_.range = library::ArchiveTimeRange::kLast7Days;
      UpdateFilterButton();
      UpdateList(/*keep_scroll=*/false);
      break;
    case kFilterTimeLast30Days:
      archive_filter_.range = library::ArchiveTimeRange::kLast30Days;
      UpdateFilterButton();
      UpdateList(/*keep_scroll=*/false);
      break;
    case kFilterReset:
      archive_filter_ = {};
      UpdateFilterButton();
      UpdateList(/*keep_scroll=*/false);
      break;
    case kHideMedia:
      PostAction(base::BindOnce(
          [](LibraryView* view) {
            view->CloseMediaLightbox();
            view->HideMediaPill();
            view->media_card_dismissed_ = false;
            view->delegate_->SetMediaLibraryEnabled(false);
            view->RefreshMedia();
          },
          base::Unretained(this)));
      break;
    case kMediaOpenPreview:
      PostAction(base::BindOnce(&SidebarView::Delegate::OpenMediaInPreview,
                                base::Unretained(&delegate_.get()),
                                menu_media_));
      break;
    case kMediaCopy:
      PostAction(base::BindOnce(&SidebarView::Delegate::CopyMediaFile,
                                base::Unretained(&delegate_.get()),
                                menu_media_));
      break;
    case kMediaRevealFinder:
      PostAction(base::BindOnce(&SidebarView::Delegate::RevealMediaInFinder,
                                base::Unretained(&delegate_.get()),
                                menu_media_));
      break;
    default:
      if (command_id >= kFilterSpaceBase) {
        const size_t idx = static_cast<size_t>(command_id - kFilterSpaceBase);
        if (idx < filter_space_ids_.size()) {
          archive_filter_.space_id = filter_space_ids_[idx];
          UpdateFilterButton();
          UpdateList(/*keep_scroll=*/false);
        }
      }
      break;
  }
}

std::vector<uint64_t> LibraryView::SpaceIds() const {
  std::vector<uint64_t> ids;
  for (const auto& space : spaces_) {
    ids.push_back(space.id);
  }
  return ids;
}

void LibraryView::OnMoveHandleEvent(size_t index,
                                    views::View* handle,
                                    const ui::MouseEvent* event) {
  if (!event) {
    EndSpaceDrag(/*commit=*/false);
    return;
  }
  if (index >= cards_.size() || index >= spaces_.size()) {
    return;
  }
  views::View* row = cards_[index]->parent();
  // Row coordinates follow the pointer whatever the dragged card's offset.
  gfx::Point point = event->location();
  views::View::ConvertPointToTarget(handle, row, &point);
  switch (event->type()) {
    case ui::EventType::kMousePressed:
      drag_ = SpaceDrag{
          .ids = SpaceIds(), .source = index, .target = index, .press = point};
      return;
    case ui::EventType::kMouseDragged:
      DragCard(point);
      return;
    case ui::EventType::kMouseReleased:
      EndSpaceDrag(/*commit=*/true);
      return;
    default:
      return;
  }
}

void LibraryView::DragCard(const gfx::Point& point) {
  if (!drag_) {
    return;
  }
  views::View& card = *cards_[drag_->source];
  if (!drag_->lifted) {
    if (!views::View::ExceededDragThreshold(point - drag_->press)) {
      return;
    }
    // Arc lifts the card over the others, which slide aside under it.
    drag_->lifted = true;
    for (auto& each : cards_) {
      each->SetPaintToLayer();
      each->layer()->SetFillsBoundsOpaquely(false);
    }
    if (ui::Layer* parent = card.layer()->parent()) {
      parent->StackAtTop(card.layer());
    }
  }
  // The card stays within the row of cards.
  const int offset =
      std::clamp(point.x() - drag_->press.x(), cards_.front()->x() - card.x(),
                 cards_.back()->x() - card.x());
  card.SetTransform(gfx::Transform::MakeTranslation(offset, 0));
  const int step = cards_.size() > 1 ? cards_[1]->x() - cards_[0]->x() : 1;
  const size_t target =
      library::SpaceDropSlot(drag_->source, cards_.size(), offset, step);
  if (target == drag_->target) {
    return;
  }
  drag_->target = target;
  const size_t source = drag_->source;
  for (size_t i = 0; i < cards_.size(); ++i) {
    if (i == source) {
      continue;
    }
    int shift = 0;
    if (source < target && i > source && i <= target) {
      shift = -step;
    } else if (target < source && i >= target && i < source) {
      shift = step;
    }
    SlideCard(*cards_[i], shift);
  }
}

void LibraryView::EndSpaceDrag(bool commit) {
  if (!drag_) {
    return;
  }
  const SpaceDrag drag = *std::exchange(drag_, std::nullopt);
  const bool rebuild = std::exchange(rebuild_after_drag_, false);
  if (commit && drag.lifted && drag.target != drag.source) {
    MoveSpace(drag.ids[drag.source],
              library::SpaceMoveBefore(drag.ids, drag.source, drag.target));
    return;
  }
  for (auto& card : cards_) {
    SlideCard(*card, 0);
  }
  if (rebuild) {
    // The handle's own event is still running.
    PostAction(base::BindOnce(&LibraryView::Rebuild, base::Unretained(this)));
  }
}

void LibraryView::MoveSpace(uint64_t space, uint64_t before) {
  // Shows the new order at once, after the caller's handler unwinds. The
  // workspace's refresh keeps it, or puts the old order back if it refused.
  PostAction(base::BindOnce(
      [](LibraryView* view, uint64_t space, uint64_t before) {
        auto& spaces = view->spaces_;
        const auto moved =
            std::ranges::find(spaces, space, &SidebarView::LibrarySpace::id);
        if (moved != spaces.end() && space != before) {
          SidebarView::LibrarySpace card = std::move(*moved);
          spaces.erase(moved);
          const auto at =
              std::ranges::find(spaces, before, &SidebarView::LibrarySpace::id);
          spaces.insert(at, std::move(card));
        }
        view->Rebuild();
        // Last: the refresh it posts may replace this view.
        view->delegate_->MoveSpace(space, before);
      },
      base::Unretained(this), space, before));
}

void LibraryView::ContentsChanged(views::Textfield* sender,
                                  const std::u16string& new_contents) {
  query_ = new_contents;
  UpdateList(/*keep_scroll=*/false);
}

BEGIN_METADATA(LibraryView)
END_METADATA

}  // namespace myarc
