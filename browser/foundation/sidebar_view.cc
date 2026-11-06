#include "myarc/sidebar_view.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "components/vector_icons/vector_icons.h"
#include "myarc/bridge.rs.h"
#include "myarc/create_menu.h"
#include "myarc/create_space_panel.h"
#include "myarc/downloads_stack.h"
#include "myarc/footer_button.h"
#include "myarc/icons.h"
#include "myarc/library_view.h"
#include "myarc/sidebar_tint.h"
#include "myarc/space_icon_catalog.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace myarc {
namespace {

using Orientation = views::BoxLayout::Orientation;

// Arc's tiles are 47 DIP tall; three of them and two 8 DIP gaps fill
// the 212 DIP row.
constexpr int kTileHeight = 47;
// Arc's row highlight is 36 DIP tall, the URL pill's height.
constexpr int kRowHeight = 36;
// The rows' size and edges; the Library replaces both while it is open.
constexpr gfx::Size kSidebarSize(230, 400);
constexpr gfx::Insets kSidebarInsets = gfx::Insets::TLBR(8, 8, 7, 8);
// Measured from Arc: the edge narrows over ~270 ms while the Library leaves
// 2.2x as fast. The rows replace it ~60% of the way and fade in over ~65 ms.
constexpr base::TimeDelta kLibraryExit = base::Milliseconds(270);
constexpr base::TimeDelta kRowsSwap = base::Milliseconds(55);
constexpr base::TimeDelta kRowsFadeIn = base::Milliseconds(65);
constexpr double kLibraryExitSpeed = 2.2;
// Arc's Space switch slides the rows for ~6 frames at 30 fps, easing out.
constexpr base::TimeDelta kSpaceSlide = base::Milliseconds(200);

// Lucide "circle" filled to the canvas edge: Arc's Space dot is 8 DIP across.
constexpr gfx::PathElement kDotPath[] = {gfx::CANVAS_DIMENSIONS, 24,
                                         gfx::CIRCLE, 12, 12, 12};
constexpr gfx::VectorIconRep kDotRep[] = {{kDotPath}};
constexpr gfx::VectorIcon kDotIcon(kDotRep, std::size(kDotRep), "myarc_dot");

struct Fills {
  SkColor tile;
  SkColor selected_tile;
  SkColor new_tab;
  SkColor current_space;
  SkColor space_dot;
  SkColor title;
};

// Arc lifts sidebar controls with white over the window material, so they read
// lighter than the sidebar. Dark values are sampled from Arc's dark sidebar;
// the current Space's glyph is 0.85 of the footer icons' muted #A9A2AF, other
// Spaces' glyphs the same white at 18%. The URL pill and header toggle chip
// pick light/dark from the same kColorSysSurface.
Fills GetFills(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  return dark ? Fills{SkColorSetA(SK_ColorWHITE, 0x18),
                      SkColorSetA(SK_ColorWHITE, 0x6F),
                      SkColorSetA(SK_ColorWHITE, 0x33),
                      GetCurrentSpaceColor(provider),
                      SkColorSetA(SK_ColorWHITE, 0x2D),
                      SkColorSetA(SK_ColorWHITE, 0x73)}
              : Fills{SkColorSetA(SK_ColorWHITE, 0x80), SK_ColorWHITE,
                      SkColorSetA(SK_ColorWHITE, 0xB3),
                      GetCurrentSpaceColor(provider),
                      SkColorSetA(SK_ColorBLACK, 0x33),
                      SkColorSetA(SK_ColorBLACK, 0x99)};
}

// A Space record's icon. Unknown kinds read as none.
std::optional<SpaceTheme> SpaceThemeOf(const foundation::SpaceRecord& space) {
  if (!space.theme.set) {
    return std::nullopt;
  }
  return SpaceTheme{.color = SkColorSetA(space.theme.color, SK_AlphaOPAQUE),
                    .opacity = space.theme.opacity};
}

SpaceIconChoice SpaceIconOf(const foundation::SpaceRecord& space) {
  if (space.icon_kind == foundation::SpaceIconKind::Emoji) {
    return {SpaceIconChoice::Kind::kEmoji, std::string(space.icon)};
  }
  if (space.icon_kind == foundation::SpaceIconKind::Named) {
    return {SpaceIconChoice::Kind::kNamed, std::string(space.icon)};
  }
  return {};
}

// The Space title's icon: the Space's emoji or icon, else Arc's default
// planet, or the bot for the agent Space. 18 DIP, as Arc's title glyph is a
// size up from the rows' 16 DIP favicons.
constexpr int kSpaceIconSize = 18;
ui::ImageModel SpaceIcon(const SpaceIconChoice& icon,
                         SkColor color,
                         bool agent = false) {
  ui::ImageModel image = SpaceIconImage(icon, color, kSpaceIconSize);
  return image.IsEmpty()
             ? ui::ImageModel::FromVectorIcon(agent ? kBotIcon : kSaturnIcon,
                                              color, kSpaceIconSize)
             : image;
}

// Arc centres every row icon 18 DIP into the row, on New Tab's plus, and starts
// every title 36 DIP in.
constexpr int kRowIconCenter = 18;
constexpr int kRowTextStart = 36;

// Leads a row's icon of `icon_size` so it centres on kRowIconCenter and its
// title starts at kRowTextStart.
void AlignRowContents(views::MdTextButton& button, int icon_size) {
  const int lead = kRowIconCenter - icon_size / 2;
  button.SetCustomPadding(gfx::Insets::TLBR(6, lead, 6, 8));
  button.SetImageLabelSpacing(kRowTextStart - lead - icon_size);
}

// Arc fills one tile row edge to edge: one tile spans it, two halve it,
// three or more share it in thirds.
constexpr size_t kTileColumns = 3;

// A Favorite or Pin tile. Arc rings the selected tile in its favicon's colour.
class ItemTile final : public views::MdTextButton {
  METADATA_HEADER(ItemTile, views::MdTextButton)

 public:
  using views::MdTextButton::MdTextButton;

  void SetRing(std::optional<SkColor> ring) {
    if (ring_ != ring) {
      ring_ = ring;
      SchedulePaint();
    }
  }

 private:
  // Arc's ring is ~1.5 DIP inside the tile's 10 DIP corners.
  static constexpr float kRingWidth = 1.5f;
  static constexpr float kCornerRadius = 10;

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (!ring_) {
      return;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kRingWidth);
    flags.setColor(*ring_);
    gfx::RectF bounds(GetLocalBounds());
    bounds.Inset(kRingWidth / 2);
    canvas->DrawRoundRect(bounds, kCornerRadius - kRingWidth / 2, flags);
  }

  std::optional<SkColor> ring_;
};

BEGIN_METADATA(ItemTile)
END_METADATA

// The favicon's dominant colour when it has one; grey, black or white icons
// leave Arc's selected tile untinted.
std::optional<SkColor> FaviconTint(const views::MdTextButton& tile) {
  const gfx::ImageSkia image = tile.GetImage(views::Button::STATE_NORMAL);
  const SkBitmap* bitmap = image.isNull() ? nullptr : image.bitmap();
  if (!bitmap || bitmap->drawsNothing()) {
    return std::nullopt;
  }
  const SkColor color = color_utils::CalculateKMeanColorOfBitmap(*bitmap);
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(color, &hsl);
  if (hsl.s < 0.3 || hsl.l < 0.2 || hsl.l > 0.85) {
    return std::nullopt;
  }
  return color;
}

// STYLE_EMPHASIZED is not allowed for CONTEXT_BUTTON_MD and LabelButton hides
// its label, so reweight the button's Label child directly.
void SetWeight(views::LabelButton& button, gfx::Font::Weight weight) {
  for (views::View* child : button.children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(label->font_list().DeriveWithWeight(weight));
    }
  }
}

// A Space's footer glyph, brighter for the current Space: its emoji or icon,
// else the bot for the agent Space, Arc's default planet for the footer's
// first Space and a dot for the rest. Other Spaces' emoji fade to half.
ui::ImageModel SpaceGlyph(bool first,
                          const SpaceIconChoice& icon,
                          bool current,
                          const Fills& fills,
                          bool agent = false) {
  const SkColor color = current ? fills.current_space : fills.space_dot;
  ui::ImageModel image =
      SpaceIconImage(icon, color, 16, current ? SK_AlphaOPAQUE : 0x80);
  if (!image.IsEmpty()) {
    return image;
  }
  if (agent) {
    return ui::ImageModel::FromVectorIcon(kBotIcon, color, 16);
  }
  return first ? ui::ImageModel::FromVectorIcon(kSaturnIcon, color, 16)
               : ui::ImageModel::FromVectorIcon(kDotIcon, color, 8);
}

// Marks the agent Space's title: "Agent" in a small pill of the title's
// color, filled like the footer's other-Profiles pill.
class AgentBadge final : public views::Label {
  METADATA_HEADER(AgentBadge, views::Label)

 public:
  AgentBadge() : views::Label(u"Agent") {
    SetFontList(font_list().DeriveWithSizeDelta(-2).DeriveWithWeight(
        gfx::Font::Weight::MEDIUM));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(1, 6)));
    SetSubpixelRenderingEnabled(false);
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 2, 0, 8));
  }

  void OnThemeChanged() override {
    views::Label::OnThemeChanged();
    const Fills fills = GetFills(GetColorProvider());
    SetEnabledColor(fills.title);
    // Radii past half the height clamp, so the ends stay round.
    SetBackground(views::CreateRoundedRectBackground(
        SkColorSetA(fills.space_dot, SkColorGetA(fills.space_dot) / 4), 16));
  }
};

BEGIN_METADATA(AgentBadge)
END_METADATA

// Arc packs the Space glyphs left, 6 DIP after the Library button: the
// footer's 4 DIP spacing and 2 more.
constexpr int kSpacesLead = 2;

// A light capsule behind the other Profiles' Space glyphs, 4 DIP after the
// glyph before it. It hugs the glyph row with 4 DIP to spare on each side and
// fills with the other Spaces' glyph color at a quarter of its alpha. Its
// padding shows the Profiles' names as a footer tooltip.
class ProfileGroup final : public views::View {
  METADATA_HEADER(ProfileGroup, views::View)

 public:
  ProfileGroup(base::WeakPtr<FooterTooltip> tooltip,
               std::u16string name,
               std::u16string names)
      : tooltip_(std::move(tooltip)), names_(std::move(names)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets::VH(0, 2)));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 4, 0, 0));
    GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
    GetViewAccessibility().SetName(std::move(name));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Each glyph sits centered in its button.
    gfx::RectF glyphs;
    for (const views::View* child : children()) {
      const auto* button = views::AsViewClass<views::LabelButton>(child);
      if (!button) {
        continue;
      }
      gfx::Rect image(button->GetImage(views::Button::STATE_NORMAL).size());
      image.Offset(button->bounds().CenterPoint() - image.CenterPoint());
      glyphs.Union(gfx::RectF(image));
    }
    if (glyphs.IsEmpty()) {
      return;
    }
    glyphs.Outset(4);
    const SkColor dot = GetFills(GetColorProvider()).space_dot;
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(dot, SkColorGetA(dot) / 4));
    canvas->DrawRoundRect(glyphs, glyphs.height() / 2, flags);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SchedulePaint();
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    if (tooltip_) {
      tooltip_->Hover(*this, {names_, {}});
    }
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    if (tooltip_) {
      tooltip_->Unhover(*this);
    }
  }

 private:
  base::WeakPtr<FooterTooltip> tooltip_;
  std::u16string names_;
};

BEGIN_METADATA(ProfileGroup)
END_METADATA

enum ItemCommand { kPin = 1, kFavorite, kUnsave, kClose };

// Opens the creation menu once, held open, for background captures.
constexpr char kCaptureCreateMenu[] = "myarc-capture-create-menu";
// Opens one seeded row once the workspace loads, for background captures.
constexpr char kCaptureActivate[] = "myarc-capture-activate";

// The creation menu floats 17 DIP above the Create button, its notch tip 12
// DIP above it and 4 DIP left of its center, and its right edge 5 DIP inside
// the button's.
constexpr int kCreateMenuGap = 17;
constexpr int kCreateMenuRightInset = 5;
constexpr int kCreateMenuNotchShift = 4;

// The row owns its highlight. Its close target shows only with the pointer or
// focus on the row, so a resting title runs to the row's edge as in Arc. Native
// child enter/exit and focus notifications cover both buttons.
class ItemRow final : public views::View, public views::FocusChangeListener {
  METADATA_HEADER(ItemRow, views::View)

 public:
  ItemRow(std::unique_ptr<views::MdTextButton> activate,
          std::unique_ptr<views::MdTextButton> close,
          bool selected)
      : selected_(selected) {
    // The composite row owns hover paint, including the close target.
    views::InkDrop::Get(activate.get())
        ->SetMode(views::InkDropHost::InkDropMode::OFF);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets(), 0));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->SetFlexForView(AddChildView(std::move(activate)), 1);
    close_ = AddChildView(std::move(close));
    close_->SetVisible(false);
    SetNotifyEnterExitOnChild(true);
  }

  void AddedToWidget() override {
    views::View::AddedToWidget();
    focus_observation_.Observe(GetFocusManager());
    UpdateClose();
  }

  void RemovedFromWidget() override {
    focus_observation_.Reset();
    views::View::RemovedFromWidget();
  }

  void WillClearFocusManager() override {
    focus_observation_.Reset();
    views::View::WillClearFocusManager();
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    hovered_ = true;
    UpdateClose();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    hovered_ = false;
    UpdateClose();
  }

  void OnDidChangeFocus(views::View*, views::View*) override { UpdateClose(); }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    UpdateClose();
  }

 private:
  void UpdateClose() {
    auto* manager = GetFocusManager();
    const bool reveal =
        hovered_ || (manager && Contains(manager->GetFocusedView()));
    // Same translucent white as the tiles: Arc's row highlight
    // (#505060 on the New Tab row) for the selected row, the resting tile
    // fill for hover.
    const Fills fills = GetFills(GetColorProvider());
    SetBackground(selected_ || reveal
                      ? views::CreateRoundedRectBackground(
                            selected_ ? fills.new_tab : fills.tile, 10)
                      : nullptr);
    close_->SetVisible(reveal);
  }

  raw_ptr<views::MdTextButton> close_ = nullptr;
  const bool selected_;
  bool hovered_ = false;
  base::ScopedObservation<views::FocusManager, views::FocusChangeListener>
      focus_observation_{this};
};

BEGIN_METADATA(ItemRow)
END_METADATA

std::unique_ptr<views::View> Box(Orientation orientation, int spacing) {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(orientation, gfx::Insets(), spacing));
  return view;
}

template <typename T = views::MdTextButton>
std::unique_ptr<T> Button(views::Button::PressedCallback callback,
                          const std::u16string& text) {
  // Icons keep their explicit colors. Retinting them with the text color bakes
  // in the disabled color whenever colors update in an inactive Mac window.
  auto button = std::make_unique<T>(
      std::move(callback), text, views::style::CONTEXT_BUTTON_MD,
      /*use_text_color_for_icon=*/false);
  button->SetStyle(ui::ButtonStyle::kText);
  button->SetTextSubpixelRenderingEnabled(false);
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

// A 32 DIP square footer button. Without `icon` the button draws its own art.
template <typename T = views::MdTextButton>
std::unique_ptr<T> IconButton(views::Button::PressedCallback callback,
                              const gfx::VectorIcon* icon,
                              int icon_size = 20,
                              ui::ColorVariant color = ui::kColorSysOnSurface) {
  auto button = Button<T>(std::move(callback), u"");
  button->SetMinSize(gfx::Size(32, 32));
  button->SetMaxSize(gfx::Size(32, 32));
  button->SetPreferredSize(gfx::Size(32, 32));
  button->SetCustomPadding(gfx::Insets());
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->SetImageCentered(true);
  button->SetCornerRadius(8);
  if (icon) {
    button->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*icon, color, icon_size));
  }
  return button;
}

void AddStatus(views::View& parent, const std::u16string& text) {
  auto* label = parent.AddChildView(std::make_unique<views::Label>(text));
  label->SetSubpixelRenderingEnabled(false);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(ui::kColorSysOnSurface);
}

}  // namespace

// The current Space's title and rows. During a Space switch the old Space's
// rows ride one width beside them, outside their bounds, on their own layer.
class SpaceRows final : public views::View {
  METADATA_HEADER(SpaceRows, views::View)

 public:
  SpaceRows() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kVertical,
                                                        gfx::Insets(), 4));
  }

  // `shift` is +1 to place `rows` on the right, -1 on the left.
  void SetOutgoing(std::unique_ptr<SpaceRows> rows, int shift) {
    TakeOutgoing();
    rows->SetProperty(views::kViewIgnoredByLayoutKey, true);
    rows->SetCanProcessEventsWithinSubtree(false);
    rows->GetViewAccessibility().SetIsIgnored(true);
    if (!rows->layer()) {
      rows->SetPaintToLayer();
      rows->layer()->SetFillsBoundsOpaquely(false);
    }
    rows->layer()->SetTransform(gfx::Transform());
    outgoing_shift_ = shift;
    outgoing_ = AddChildView(std::move(rows));
    InvalidateLayout();
  }

  std::unique_ptr<SpaceRows> TakeOutgoing() {
    if (!outgoing_) {
      return nullptr;
    }
    return RemoveChildViewT(std::exchange(outgoing_, nullptr).get());
  }

  void Layout(PassKey) override {
    LayoutSuperclass<views::View>(this);
    if (outgoing_) {
      outgoing_->SetBounds(outgoing_shift_ * width(), 0, width(),
                           outgoing_->GetHeightForWidth(width()));
    }
  }

 private:
  raw_ptr<SpaceRows> outgoing_ = nullptr;
  int outgoing_shift_ = 0;
};

BEGIN_METADATA(SpaceRows)
END_METADATA

// Arc's divider above Today is one device pixel thick, so it paints only
// the bottom pixel of its 1 DIP height.
class Hairline final : public views::Separator {
  METADATA_HEADER(Hairline, views::Separator)

 public:
  Hairline() { SetOrientation(Orientation::kHorizontal); }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    const gfx::Rect bounds = GetContentsBounds();
    const int x = std::ceil(bounds.x() * dsf);
    const int r = std::floor(bounds.right() * dsf);
    const int b = std::ceil(bounds.bottom() * dsf);
    canvas->FillRect(gfx::Rect(x, b - 1, std::max(1, r - x), 1),
                     GetForegroundColor());
  }
};

BEGIN_METADATA(Hairline)
END_METADATA

// Arc's divider between the Space title and Today, ending short of Clear at its
// right. It keeps the hairline's place: 6 DIP down a 14 DIP band.
class TodayDivider final : public views::View {
  METADATA_HEADER(TodayDivider, views::View)

 public:
  explicit TodayDivider(std::unique_ptr<views::View> clear) {
    line_ = AddChildView(std::make_unique<Hairline>());
    clear_ = AddChildView(std::move(clear));
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(0, kHeight);
  }

  void Layout(PassKey) override {
    const gfx::Size clear = clear_->GetPreferredSize();
    const int clear_height = std::min(clear.height(), height());
    clear_->SetBounds(width() - clear.width(), (height() - clear_height) / 2,
                      clear.width(), clear_height);
    line_->SetBounds(0, kLineY, std::max(0, clear_->x() - kGap), 1);
  }

 private:
  static constexpr int kHeight = 14;
  static constexpr int kLineY = 6;
  static constexpr int kGap = 6;

  raw_ptr<Hairline> line_ = nullptr;
  raw_ptr<views::View> clear_ = nullptr;
};

BEGIN_METADATA(TodayDivider)
END_METADATA

SidebarView::SidebarView(Delegate& delegate)
    : views::AnimationDelegateViews(this),
      delegate_(delegate),
      footer_tooltip_(std::make_unique<FooterTooltip>()) {
  library_exit_.SetDuration(kLibraryExit);
  space_slide_.SetDuration(kSpaceSlide);
  SetPreferredSize(kSidebarSize);
  SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kVertical,
                                                      kSidebarInsets, 8));
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Workspace sidebar");
  SetUnavailable(u"Loading workspace\u2026");
}

SidebarView::~SidebarView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  menu_runner_.reset();
}

void SidebarView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetWidget() || (menu_runner_ && menu_runner_->IsRunning())) {
    return;
  }
  const auto target =
      std::ranges::find(focus_targets_, source, &FocusTarget::view);
  if (target == focus_targets_.end() || !target->item_menu) {
    return;
  }
  menu_item_id_ = target->id;
  menu_space_id_ = current_space_id_;
  menu_state_ = *target->item_menu;
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  if (!menu_state_.is_pin) {
    menu_model_->AddItem(kPin, menu_state_.is_favorite ? u"Move to Space Pins"
                                                       : u"Pin to Space");
  }
  if (!menu_state_.is_favorite) {
    menu_model_->AddItem(kFavorite, menu_state_.is_pin
                                        ? u"Move to Profile Favorites"
                                        : u"Add to Profile Favorites");
  }
  if (menu_state_.is_pin || menu_state_.is_favorite) {
    menu_model_->AddItem(kUnsave, menu_state_.is_pin
                                      ? u"Unpin from Space"
                                      : u"Remove from Profile Favorites");
  }
  if (menu_state_.is_live) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(
        kClose, menu_state_.is_pin        ? u"Close live page (keep Space Pin)"
                : menu_state_.is_favorite ? u"Close live page (keep Favorite)"
                                          : u"Close live page");
  }
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  // Use the stable widget, never the replaceable row or its button controller.
  // RunMenuAt can enter a native nested loop and even destroy this sidebar.
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void SidebarView::OpenLibrary(LibrarySection section) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (library_exit_.is_animating()) {
    // Reopening while the Library leaves turns it back, keeping its section
    // state and scroll.
    library_exit_.Stop();
    if (library_) {
      delegate_->SetLibraryOpen(true);
      library_->SelectSection(section);
      InvalidateLayout();
      return;
    }
  }
  if (library_) {
    library_->SelectSection(section);
    return;
  }
  if (!current_space_id_) {
    return;
  }
  // Keyboard users keep focus; a click leaves no focus ring, as in Arc.
  const bool had_focus = std::ranges::any_of(
      focus_targets_,
      [](const FocusTarget& target) { return target.view->HasFocus(); });
  menu_item_id_ = 0;
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
  space_slide_.Stop();
  DropRows();
  // The Library sets its own width and edges, as Arc swaps out its sidebar.
  auto* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  layout->set_inside_border_insets(gfx::Insets());
  SetPreferredSize(std::nullopt);
  // The Library posts every close, so its handlers have unwound.
  library_ = AddChildView(std::make_unique<LibraryView>(
      *delegate_,
      base::BindRepeating(&SidebarView::CloseLibrary,
                          weak_factory_.GetWeakPtr()),
      section, footer_tooltip_->GetWeakPtr()));
  layout->SetFlexForView(library_, 1);
  library_->SetSpaces(library_spaces_, current_space_id_);
  delegate_->SetLibraryOpen(true);
  library_keyboard_ = had_focus;
  if (had_focus) {
    library_return_focus_.SetView(nullptr);
    library_->FocusSection();
    return;
  }
  // Esc reaches the Library only while focus is in the sidebar.
  library_return_focus_.SetView(
      GetFocusManager() ? GetFocusManager()->GetFocusedView() : nullptr);
  library_->HoldFocus();
}

void SidebarView::DropRows() {
  focus_targets_.clear();
  footer_targets_.clear();
  footer_ = nullptr;
  downloads_stack_ = nullptr;
  create_menu_ = nullptr;
  create_button_ = nullptr;
  create_menu_open_ = false;
  spaces_scroll_ = nullptr;
  new_tab_ = nullptr;
  space_rows_ = nullptr;
  tiles_.clear();
  create_panel_ = nullptr;
  RemoveAllChildViews();
}

void SidebarView::OpenCreateSpace(std::vector<ProfileChoice> profiles,
                                  ProfileChoice own_profile,
                                  bool profile_menu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (create_panel_) {
    create_panel_->FocusName();
    return;
  }
  if (spaces_.empty()) {
    return;
  }
  const bool from_library = library_ || library_exit_.is_animating();
  const uint64_t space_id = current_space_id_;
  library_exit_.Stop();
  ResetLibrary();
  menu_item_id_ = 0;
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
  space_slide_.Stop();
  // Arc keeps the Space footer under the panel.
  if (footer_ && !from_library) {
    focus_targets_ = footer_targets_;
    new_tab_ = nullptr;
    space_rows_ = nullptr;
    tiles_.clear();
    while (children().front() != footer_) {
      RemoveChildViewT(children().front());
    }
  } else {
    DropRows();
    current_space_id_ = space_id;
    BuildFooter(gfx::PointF(), /*space_changed=*/false);
    footer_targets_ = focus_targets_;
    UpdateFills();
  }
  // SetModel rebuilds the rows from scratch once the panel closes.
  current_space_id_ = 0;
  // The panel posts its callbacks and this view owns it, so none outlives
  // this view, and the delegate outlives both.
  create_panel_ =
      AddChildViewAt(std::make_unique<CreateSpacePanel>(
                         std::move(profiles), std::move(own_profile),
                         base::BindRepeating(&SidebarView::SubmitCreateSpace,
                                             base::Unretained(this)),
                         profile_menu
                             ? base::BindRepeating(
                                   &Delegate::CreateProfile,
                                   base::Unretained(&*delegate_))
                             : CreateSpacePanel::CreateProfileCallback(),
                         base::BindRepeating(&SidebarView::CloseCreateSpace,
                                             base::Unretained(this))),
                     0);
  static_cast<views::BoxLayout*>(GetLayoutManager())
      ->SetFlexForView(create_panel_, 1);
  if (from_library) {
    // The parent narrows back to the rows' width; its model waits for the
    // panel to close.
    auto weak = weak_factory_.GetWeakPtr();
    delegate_->SetLibraryOpen(false);
    if (!weak || !create_panel_) {
      return;
    }
  }
  create_panel_->FocusName();
}

void SidebarView::SetOtherSpaces(std::vector<OtherSpaceEntry> spaces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  other_spaces_ = std::move(spaces);
}

void SidebarView::SetCreateSpaceProfiles(std::vector<ProfileChoice> profiles,
                                         const std::string& selected_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (create_panel_) {
    create_panel_->SetProfiles(std::move(profiles), selected_id);
  }
}

std::u16string SidebarView::SubmitCreateSpace(
    const std::u16string& name,
    const SpaceIconChoice& icon,
    const std::optional<SpaceTheme>& theme,
    const std::string& profile_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto weak = weak_factory_.GetWeakPtr();
  std::u16string error =
      delegate_->CreateNamedSpace(name, icon, theme, profile_id);
  if (weak && error.empty()) {
    CloseCreateSpace();
  }
  return error;
}

void SidebarView::CloseCreateSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!create_panel_) {
    return;
  }
  RemoveChildViewT(std::exchange(create_panel_, nullptr));
  current_space_id_ = 0;
  // Drops the panel's preview; Create's new Space arrives with SetModel.
  if (GetWidget()) {
    SetSidebarTheme(*GetWidget(), space_theme_, /*fade=*/true);
  }
  delegate_->CreateSpacePanelClosed();
}

void SidebarView::ResetLibrary() {
  if (!library_) {
    return;
  }
  views::View* library = std::exchange(library_, nullptr);
  RemoveChildViewT(library);
  static_cast<views::BoxLayout*>(GetLayoutManager())
      ->set_inside_border_insets(kSidebarInsets);
  SetPreferredSize(kSidebarSize);
  // SetModel rebuilds the rows from scratch.
  current_space_id_ = 0;
}

void SidebarView::CloseLibrary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!library_ || library_exit_.is_animating()) {
    return;
  }
  // A hidden or zero-width sidebar has nothing to animate.
  if (gfx::Animation::PrefersReducedMotion() || !parent() || !IsDrawn() ||
      parent()->width() <= 0) {
    FinishCloseLibrary(LibraryHasFocus());
    return;
  }
  // The parent eases back to the rows' width while Layout slides the Library
  // out. SetModel keeps it until AnimationProgressed swaps in the rows.
  library_exit_.Start();
  delegate_->SetLibraryOpen(false);
}

bool SidebarView::LibraryHasFocus() {
  // Keyboard users land on the Library button; a click leaves no focus ring,
  // even after clicking into the Library's search field.
  const views::FocusManager* focus_manager = GetFocusManager();
  const views::View* focused =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;
  return library_ && focused != library_ && library_->Contains(focused) &&
         (library_keyboard_ ||
          focus_manager->focus_change_reason() ==
              views::FocusManager::FocusChangeReason::kFocusTraversal);
}

void SidebarView::FinishCloseLibrary(bool had_focus) {
  // A click-opened Library holds focus only for Esc; hand it back.
  const bool return_focus =
      !had_focus && library_ && GetFocusManager() &&
      library_->Contains(GetFocusManager()->GetFocusedView());
  ResetLibrary();
  library_closed_ = true;
  auto weak = weak_factory_.GetWeakPtr();
  // After an animated close this repeats only to supply the rows' model.
  delegate_->SetLibraryOpen(false);
  if (!weak) {
    return;
  }
  if (return_focus && library_return_focus_.view()) {
    library_return_focus_.view()->RequestFocus();
  }
  library_return_focus_.SetView(nullptr);
  if (!had_focus) {
    return;
  }
  const auto button = std::ranges::find(focus_targets_, FocusAction::kLibrary,
                                        &FocusTarget::action);
  if (button != focus_targets_.end()) {
    button->view->RequestFocus();
  }
}

void SidebarView::RefreshLibraryDownloads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (library_) {
    library_->RefreshDownloads();
  }
}

void SidebarView::SetCreateMenuOpen(bool open) {
  create_menu_open_ = open;
  if (!create_button_) {
    return;
  }
  create_button_->SetFooterTooltip(
      footer_tooltip_->GetWeakPtr(),
      {open ? u"Close creation menu" : u"Create Space", {}});
  create_button_->SetOpen(open);
  if (open) {
    create_button_->GetViewAccessibility().SetIsExpanded();
  } else {
    create_button_->GetViewAccessibility().SetIsCollapsed();
  }
}

void SidebarView::ShowCreateMenu(const ui::Event& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!create_menu_ || !create_button_) {
    return;
  }
  if (create_menu_->is_open()) {
    create_menu_->Close();
    return;
  }
  menu_item_id_ = 0;
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
  create_menu_->Open(*create_button_);
}

void SidebarView::ChooseCreateItem(int item) {
  // A choice can open a dialog or the palette and rebuild the footer, so it
  // runs after the menu's handlers return.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SidebarView> sidebar, int item) {
                       if (!sidebar) {
                         return;
                       }
                       switch (static_cast<CreateMenu::Item>(item)) {
                         case CreateMenu::Item::kNewSpace:
                           sidebar->delegate_->CreateSpace();
                           return;
                         case CreateMenu::Item::kNewTab:
                           sidebar->delegate_->NewTab();
                           return;
                         case CreateMenu::Item::kNewSplit:
                           return;
                       }
                     },
                     weak_factory_.GetWeakPtr(), item));
}

void SidebarView::ExecuteCommand(int command_id, int event_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint64_t id = std::exchange(menu_item_id_, 0);
  if (!id) {
    return;
  }
  // Each call can synchronously rebuild or destroy the view. Touch no members
  // after dispatch. Only the parent/Rust can validate and acknowledge success.
  switch (command_id) {
    case kPin:
      delegate_->PinItem(id);
      return;
    case kFavorite:
      delegate_->FavoriteItem(id);
      return;
    case kUnsave:
      delegate_->UnsaveItem(id);
      return;
    case kClose:
      delegate_->CloseLiveItem(id);
      return;
  }
}

void SidebarView::TrackFocus(views::View* view,
                             FocusAction action,
                             uint64_t id,
                             std::optional<ItemMenuState> item_menu) {
  focus_targets_.push_back({action, id, view, item_menu});
  if (item_menu) {
    view->set_context_menu_controller(this);
    view->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  }
}

SidebarView::ItemPresentation SidebarView::Present(
    const foundation::ItemRecord& item) const {
  ItemPresentation presentation = delegate_->GetItemPresentation(item);
  // Dormant saved rows show the last title their page had, as Arc does, and
  // fall back to the saved URL until a page has loaded once.
  if (presentation.title.empty() && !item.title.empty()) {
    presentation.title = base::UTF8ToUTF16(std::string(item.title));
  }
  if (presentation.title.empty()) {
    presentation.title =
        item.has_url ? base::UTF8ToUTF16(std::string(item.canonical_url))
                     : u"Untitled tab";
  }
  return presentation;
}

std::unique_ptr<views::View> SidebarView::MakeItem(
    const foundation::ItemRecord& item,
    bool tile,
    bool selected) {
  ItemPresentation presentation = Present(item);
  const auto foreground = ui::kColorSysOnSurface;
  auto activate_callback = base::BindRepeating(
      &Delegate::ActivateItem, base::Unretained(&delegate_.get()), item.id);
  std::unique_ptr<views::MdTextButton> activate =
      tile ? Button<ItemTile>(std::move(activate_callback), {})
           : Button(std::move(activate_callback), presentation.title);
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
  if (tile) {
    tiles_.emplace_back(activate.get(), selected);
  }
  const ItemMenuState menu_state = {
      item.placement == foundation::PlacementKind::Pin,
      item.placement == foundation::PlacementKind::Favorite,
      presentation.is_live};
  TrackFocus(activate.get(), FocusAction::kActivate, item.id, menu_state);
  if (tile) {
    activate->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    activate->SetImageCentered(true);
    activate->SetMinSize(gfx::Size(0, kTileHeight));
    activate->SetPreferredSize(gfx::Size(0, kTileHeight));
    return activate;
  }
  activate->SetMinSize(gfx::Size(0, kRowHeight));
  AlignRowContents(*activate, presentation.icon.Size().width());

  // Only Today tabs are rows, and a dormant one closes too.
  auto close = IconButton(
      base::BindRepeating(&Delegate::CloseLiveItem,
                          base::Unretained(&delegate_.get()), item.id),
      &vector_icons::kCloseIcon);
  views::InstallRoundRectHighlightPathGenerator(close.get(), gfx::Insets(4), 6);
  const std::u16string close_name =
      (presentation.is_live ? u"Close live page: " : u"Close tab: ") +
      presentation.title;
  close->SetTooltipText(close_name);
  close->GetViewAccessibility().SetName(close_name);
  TrackFocus(close.get(), FocusAction::kClose, item.id, menu_state);
  return std::make_unique<ItemRow>(std::move(activate), std::move(close),
                                   selected);
}

std::unique_ptr<views::View> SidebarView::MakeTodayDivider(
    std::vector<uint64_t> today) {
  auto clear =
      Button(base::BindRepeating(
                 [](base::WeakPtr<SidebarView> sidebar,
                    const std::vector<uint64_t>& today) {
                   // Every close rebuilds the rows, this button included, and
                   // the last one can close the window. Close after the press
                   // unwinds and stop once the sidebar is gone.
                   base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                       FROM_HERE, base::BindOnce(
                                      [](base::WeakPtr<SidebarView> sidebar,
                                         std::vector<uint64_t> today) {
                                        for (const uint64_t id : today) {
                                          if (!sidebar) {
                                            return;
                                          }
                                          sidebar->delegate_->CloseLiveItem(id);
                                        }
                                      },
                                      sidebar, today));
                 },
                 weak_factory_.GetWeakPtr(), std::move(today)),
             u"Clear");
  // Arc's Clear is 11pt semibold with a thin down arrow, dimmer than the
  // Space title. UpdateFills sets its colours.
  for (views::View* child : clear->children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      label->SetFontList(
          label->font_list().DeriveWithSizeDelta(-2).DeriveWithWeight(
              gfx::Font::Weight::SEMIBOLD));
    }
  }
  clear->SetMinSize(gfx::Size());
  clear->SetCustomPadding(gfx::Insets::VH(0, 2));
  clear->SetCornerRadius(4);
  clear->SetImageLabelSpacing(0);
  clear->SetTooltipText(u"Clear Today tabs");
  clear->GetViewAccessibility().SetName(u"Clear Today tabs");
  TrackFocus(clear.get(), FocusAction::kClearToday);
  auto divider = std::make_unique<TodayDivider>(std::move(clear));
  // The hairline starts 10 DIP inside the rows and Clear's text ends 8 DIP
  // inside them.
  divider->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 10, 0, 6));
  return divider;
}

views::View* SidebarView::AddStorageRetry(views::View& parent,
                                          StorageRetry retry) {
  auto button =
      Button(base::BindRepeating(&Delegate::RetryWorkspaceStorage,
                                 base::Unretained(&delegate_.get()), retry),
             retry == StorageRetry::kLoad ? u"Retry loading" : u"Retry saving");
  TrackFocus(button.get(), retry == StorageRetry::kLoad
                               ? FocusAction::kRetryLoad
                               : FocusAction::kRetrySave);
  return parent.AddChildView(std::move(button));
}

void SidebarView::SetUnavailable(const std::u16string& reason,
                                 StorageRetry retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool restore_focus = std::ranges::any_of(
      focus_targets_,
      [](const FocusTarget& target) { return target.view->HasFocus(); });
  menu_item_id_ = 0;
  if (menu_runner_) {
    menu_runner_->Cancel();
  }
  library_exit_.Stop();
  space_slide_.Stop();
  ResetLibrary();
  library_closed_ = false;
  DropRows();
  current_space_id_ = 0;
  spaces_.clear();
  library_spaces_.clear();
  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetDrawOverflowIndicator(false);
  scroll->SetFocusBehavior(FocusBehavior::ALWAYS);
  scroll->GetViewAccessibility().SetName(u"Workspace status");
  TrackFocus(scroll, FocusAction::kStatus);
  static_cast<views::BoxLayout*>(GetLayoutManager())->SetFlexForView(scroll, 1);
  auto* body = scroll->SetContents(Box(Orientation::kVertical, 4));
  AddStatus(*body, reason.empty() ? u"Workspace unavailable" : reason);
  // Keep the action visible while a long error scrolls independently.
  auto* focus =
      retry == StorageRetry::kLoad ? AddStorageRetry(*this, retry) : scroll;
  if (restore_focus) {
    focus->RequestFocus();
  }
}

void SidebarView::SetModel(const foundation::SidebarResult& model,
                           const std::u16string& status,
                           StorageRetry retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model.ok) {
    SetUnavailable(u"Workspace unavailable: " +
                   base::UTF8ToUTF16(std::string(model.error)));
    return;
  }
  spaces_.clear();
  library_spaces_.clear();
  for (const auto& space : model.spaces) {
    spaces_.push_back({.id = space.id,
                       .name = base::UTF8ToUTF16(std::string(space.name)),
                       .icon = SpaceIconOf(space),
                       .agent = space.agent});
    library_spaces_.push_back({.id = space.id, .name = spaces_.back().name});
  }
  for (const auto& pin : model.space_pins) {
    const auto space =
        std::ranges::find(library_spaces_, pin.owner_id, &LibrarySpace::id);
    if (space != library_spaces_.end()) {
      ItemPresentation presentation = Present(pin);
      space->pins.push_back({.id = pin.id,
                             .title = std::move(presentation.title),
                             .icon = std::move(presentation.icon)});
    }
  }
  // Each Space tints the sidebar with its theme, crossfading on a switch.
  // While the Create a Space panel is open, its preview owns the tint.
  space_theme_ = SpaceThemeOf(model.current_space);
  if (!create_panel_ && GetWidget()) {
    SetSidebarTheme(*GetWidget(), space_theme_, /*fade=*/true);
  }
  if (create_panel_) {
    // The rows rebuild when the panel closes. A Space picked in the footer
    // leaves the panel; Create closes it itself.
    if (footer_space_id_ && model.current_space.id != footer_space_id_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&SidebarView::CloseCreateSpace,
                                    weak_factory_.GetWeakPtr()));
    }
    return;
  }
  if (library_) {
    // The Library shows Spaces and the archive; rows rebuild when it closes.
    current_space_id_ = model.current_space.id;
    library_->SetSpaces(library_spaces_, current_space_id_);
    // Closing a tab or a favicon lookup changes the archive.
    library_->RefreshArchive();
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
    // A usable model always has its ScrollView first.
    if (current_space_id_) {
      offset =
          static_cast<views::ScrollView*>(children().front())->CurrentOffset();
    }
  }
  const gfx::PointF spaces_offset =
      spaces_scroll_ ? spaces_scroll_->CurrentOffset() : gfx::PointF();
  const bool space_changed = current_space_id_ != model.current_space.id;
  // A Space switch slides the old title and rows out as the new ones come
  // in. Row rebuilds mid-slide carry the old rows over.
  std::unique_ptr<SpaceRows> outgoing;
  bool slide = false;
  if (space_rows_ && space_changed) {
    const auto index_of = [this](uint64_t id) {
      return std::ranges::find(spaces_, id, &SpaceEntry::id) - spaces_.begin();
    };
    const auto from = index_of(current_space_id_);
    const auto to = index_of(model.current_space.id);
    space_slide_.Stop();
    if (!gfx::Animation::PrefersReducedMotion() && IsDrawn() &&
        from < std::ssize(spaces_) && to < std::ssize(spaces_)) {
      // A Space to the left enters from the left, pushing the old one right.
      space_slide_shift_ = to < from ? 1 : -1;
      outgoing = space_rows_->parent()->RemoveChildViewT(space_rows_.get());
      slide = true;
    }
  } else if (space_rows_ && space_slide_.is_animating()) {
    outgoing = space_rows_->TakeOutgoing();
  }
  space_rows_ = nullptr;
  current_space_id_ = model.current_space.id;
  focus_targets_.clear();
  new_tab_ = nullptr;
  tiles_.clear();
  // Loading and title ticks rebuild only the rows. The footer stays, with its
  // hover, tooltip and Spaces scroll, until the Spaces or the current one
  // change.
  const bool keep_footer = footer_ && footer_spaces_ == spaces_ &&
                           footer_other_spaces_ == other_spaces_ &&
                           footer_space_id_ == current_space_id_;
  if (keep_footer) {
    while (children().front() != footer_) {
      RemoveChildViewT(children().front());
    }
  } else {
    footer_targets_.clear();
    footer_ = nullptr;
    downloads_stack_ = nullptr;
    create_menu_ = nullptr;
    create_button_ = nullptr;
    create_menu_open_ = false;
    spaces_scroll_ = nullptr;
    RemoveAllChildViews();
  }

  auto* scroll = AddChildViewAt(std::make_unique<views::ScrollView>(), 0);
  // Bounded mode sizes contents to viewport width and full preferred height.
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetBackgroundColor(
      std::nullopt);  // Preserve parent's native material.
  scroll->SetDrawOverflowIndicator(false);
  static_cast<views::BoxLayout*>(GetLayoutManager())->SetFlexForView(scroll, 1);
  auto* body = scroll->SetContents(Box(Orientation::kVertical, 4));
  if (retry == StorageRetry::kSave) {
    AddStorageRetry(*body, retry);
  }
  if (!status.empty()) {
    AddStatus(*body, status);
  }

  // The Profile's Favorites, then the current Space's Pins, share one grid
  // of favicon tiles, as Arc draws both.
  std::vector<const foundation::ItemRecord*> tile_items;
  for (const auto& item : model.favorites) {
    tile_items.push_back(&item);
  }
  for (const auto& item : model.pins) {
    tile_items.push_back(&item);
  }
  views::View* tiles_row = nullptr;
  const size_t columns = std::min(tile_items.size(), kTileColumns);
  for (size_t i = 0; i < tile_items.size(); ++i) {
    if (i % columns == 0) {
      tiles_row = body->AddChildView(Box(Orientation::kHorizontal, 8));
    }
    auto* tile = tiles_row->AddChildView(MakeItem(
        *tile_items[i], true, tile_items[i]->id == model.selected_item_id));
    static_cast<views::BoxLayout*>(tiles_row->GetLayoutManager())
        ->SetFlexForView(tile, 1);
  }
  // Empty layout cells keep a short final row on the grid; never controls.
  for (size_t i = tile_items.size(); tiles_row && i % columns != 0; ++i) {
    auto* cell = tiles_row->AddChildView(std::make_unique<views::View>());
    cell->SetPreferredSize(gfx::Size(0, kTileHeight));
    static_cast<views::BoxLayout*>(tiles_row->GetLayoutManager())
        ->SetFlexForView(cell, 1);
  }

  auto* rows = body->AddChildView(std::make_unique<SpaceRows>());
  space_rows_ = rows;
  const std::u16string name =
      base::UTF8ToUTF16(std::string(model.current_space.name));
  auto heading = Button(base::BindRepeating(&Delegate::RenameSpace,
                                            base::Unretained(&delegate_.get()),
                                            model.current_space.id),
                        name);
  // Arc's Space title is 13pt semibold in white at 45% (#888892 over its dark
  // sidebar), its baseline 31pt below the Favorites tiles.
  SetWeight(*heading, gfx::Font::Weight::SEMIBOLD);
  // Arc leads the title with the Space's icon in the title's color, lined up
  // with the row icons so the name starts where the row titles do.
  const Fills fills = GetFills(GetColorProvider());
  const bool agent = model.current_space.agent;
  heading->SetImageModel(
      views::Button::STATE_NORMAL,
      SpaceIcon(SpaceIconOf(model.current_space), fills.title, agent));
  AlignRowContents(*heading, kSpaceIconSize);
  heading->SetEnabledTextColors(fills.title);
  heading->SetTooltipText(u"Rename Space: " + name);
  heading->GetViewAccessibility().SetName(u"Rename Space: " + name);
  TrackFocus(heading.get(), FocusAction::kRename, model.current_space.id);
  views::View* title = nullptr;
  if (agent) {
    // The agent Space's title keeps its natural width, eliding when long,
    // with the Agent badge right after it.
    auto row = std::make_unique<views::View>();
    row->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
    heading->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred));
    row->AddChildView(std::move(heading));
    row->AddChildView(std::make_unique<AgentBadge>());
    title = rows->AddChildView(std::move(row));
  } else {
    title = rows->AddChildView(std::move(heading));
  }
  if (!tile_items.empty()) {
    title->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(6, 0, 0, 0));
  }

  // Arc divides the Space title from Today only while Today has tabs, and
  // puts Clear at the divider's right end.
  const bool divider = !model.temporary.empty();
  if (divider) {
    std::vector<uint64_t> today;
    for (const auto& item : model.temporary) {
      today.push_back(item.id);
    }
    rows->AddChildView(MakeTodayDivider(std::move(today)));
  }

  auto new_tab = Button(base::BindRepeating(&Delegate::NewTab,
                                            base::Unretained(&delegate_.get())),
                        u"New Tab");
  new_tab->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kAdd2Icon,
                                     ui::kColorSysOnSurface, 16));
  new_tab->SetMinSize(gfx::Size(0, kRowHeight));
  AlignRowContents(*new_tab, 16);
  TrackFocus(new_tab.get(), FocusAction::kNewTab);
  // Arc keeps 41 DIP between New Tab's center and the Space title's or the
  // first Today tab's.
  new_tab->SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(divider ? 0 : 3, 0, 1, 0));
  new_tab_ = rows->AddChildView(std::move(new_tab));
  for (const auto& item : model.temporary) {
    rows->AddChildView(
        MakeItem(item, false, item.id == model.selected_item_id));
  }
  if (outgoing) {
    outgoing->TakeOutgoing();
    rows->SetOutgoing(std::move(outgoing), space_slide_shift_);
    if (slide) {
      space_slide_.Start();
    }
    UpdateSpaceSlide(false);
  }

  // Loading/title ticks rebuild rows, but leave a semantically unchanged menu
  // running. Cancel only if its item/state or Space is no longer available.
  if (menu_item_id_ && menu_runner_ && menu_runner_->IsRunning() &&
      (menu_space_id_ != current_space_id_ ||
       !std::ranges::any_of(focus_targets_, [this](const FocusTarget& target) {
         return target.id == menu_item_id_ && target.item_menu == menu_state_;
       }))) {
    menu_item_id_ = 0;
    menu_runner_->Cancel();
  }

  if (keep_footer) {
    focus_targets_.insert(focus_targets_.end(), footer_targets_.begin(),
                          footer_targets_.end());
  } else {
    const size_t first_footer_target = focus_targets_.size();
    BuildFooter(spaces_offset, space_changed);
    footer_targets_.assign(focus_targets_.begin() + first_footer_target,
                           focus_targets_.end());
  }
  UpdateFills();
  if (!capture_activate_scheduled_ &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(kCaptureActivate)) {
    capture_activate_scheduled_ = true;
    uint64_t id = 0;
    if (base::StringToUint64(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                kCaptureActivate),
            &id)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<SidebarView> sidebar, uint64_t id) {
                if (sidebar) {
                  sidebar->delegate_->ActivateItem(id);
                }
              },
              weak_factory_.GetWeakPtr(), id),
          base::Milliseconds(1500));
    }
  }

  scroll->RegisterPostLayoutCallback(base::BindRepeating(
      [](std::optional<gfx::PointF>& offset, views::ScrollView* scroll) {
        if (auto saved = std::exchange(offset, std::nullopt)) {
          scroll->ScrollToOffset(*saved);
        }
      },
      base::OwnedRef(std::optional(offset))));
  if (focus) {
    const auto target = std::ranges::find_if(
        focus_targets_, [&focus](const FocusTarget& target) {
          return target.action == focus->first && target.id == focus->second;
        });
    const auto current_space =
        std::ranges::find_if(focus_targets_, [this](const FocusTarget& target) {
          return target.action == FocusAction::kSpaces &&
                 target.id == current_space_id_;
        });
    views::View* restore = target != focus_targets_.end() ? target->view.get()
                           : current_space != focus_targets_.end()
                               ? current_space->view.get()
                               : spaces_scroll_->contents();
    restore->RequestFocus();
  }
}

void SidebarView::BuildFooter(const gfx::PointF& spaces_offset,
                              bool space_changed) {
  footer_spaces_ = spaces_;
  footer_other_spaces_ = other_spaces_;
  footer_space_id_ = current_space_id_;
  auto* footer = AddChildView(Box(Orientation::kHorizontal, 4));
  // Arc's footer controls sit 2 DIP higher than the sidebar's bottom inset.
  footer->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 2, 0));
  static_cast<views::BoxLayout*>(footer->GetLayoutManager())
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  auto library = IconButton<LibraryButton>(
      base::BindRepeating(
          [](base::WeakPtr<SidebarView> sidebar) {
            // Arc drops the downloads stack with the view swap, unanimated.
            if (sidebar && sidebar->downloads_stack_) {
              sidebar->downloads_stack_->HideNow();
            }
            // Opening removes this button; let its press handler unwind. A
            // second click queued meanwhile must not reset the section.
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(
                               [](base::WeakPtr<SidebarView> sidebar) {
                                 if (sidebar && !sidebar->library_) {
                                   sidebar->OpenLibrary();
                                 }
                               },
                               sidebar));
          },
          weak_factory_.GetWeakPtr()),
      nullptr);
  // Arc's Shift-Cmd-L would clash with Chromium's View > Collapse Tabs, so the
  // tooltip shows no shortcut.
  library->SetFooterTooltip(footer_tooltip_->GetWeakPtr(),
                            {u"View Library", {}, /*beside=*/true});
  library->SetHoverChangedCallback(base::BindRepeating(
      [](base::WeakPtr<SidebarView> sidebar, bool hovered) {
        if (sidebar && sidebar->downloads_stack_) {
          sidebar->downloads_stack_->SetAnchorHovered(hovered);
        }
      },
      weak_factory_.GetWeakPtr()));
  library->GetViewAccessibility().SetName(u"View Library");
  TrackFocus(library.get(), FocusAction::kLibrary);
  // Right after the Library closes, its glow settles back into the box.
  if (std::exchange(library_closed_, false)) {
    library->SettleFromGlow();
  }
  footer->AddChildView(std::move(library));
  spaces_scroll_ = footer->AddChildView(std::make_unique<views::ScrollView>());
  spaces_scroll_->SetBackgroundColor(std::nullopt);
  spaces_scroll_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  spaces_scroll_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  spaces_scroll_->SetTreatAllScrollEventsAsHorizontal(true);
  spaces_scroll_->SetUseContentsPreferredSize(true);
  // Arc draws no hairline where the Spaces overflow the strip.
  spaces_scroll_->SetDrawOverflowIndicator(false);
  // Keep all footer controls on one 32-DIP line while retaining wheel/focus
  // scrolling and the current-Space reveal callback.
  spaces_scroll_->SetPreferredSize(gfx::Size(0, 32));
  // Like Arc, Space glyphs follow the Library and Create sits at the right.
  auto* spaces = spaces_scroll_->SetContents(Box(Orientation::kHorizontal, 0));
  auto* spaces_layout =
      static_cast<views::BoxLayout*>(spaces->GetLayoutManager());
  spaces_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  spaces_layout->set_inside_border_insets(
      gfx::Insets::TLBR(0, kSpacesLead, 0, 0));
  spaces->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  spaces->GetViewAccessibility().SetName(u"Spaces");
  std::optional<size_t> current_space_index;
  // With one Space, Arc's footer shows no Space glyph. Other Profiles'
  // Spaces count too.
  const bool show_spaces = spaces_.size() > 1 || !other_spaces_.empty();
  // The agent Space follows this Profile's other Spaces, wherever it sits in
  // the Spaces' order.
  std::vector<size_t> order;
  for (size_t index = 0; index < spaces_.size(); ++index) {
    if (!spaces_[index].agent) {
      order.push_back(index);
    }
  }
  for (size_t index = 0; index < spaces_.size(); ++index) {
    if (spaces_[index].agent) {
      order.push_back(index);
    }
  }
  for (size_t position = 0; show_spaces && position < order.size();
       ++position) {
    const size_t index = order[position];
    const uint64_t space_id = spaces_[index].id;
    const std::u16string& space_name = spaces_[index].name;
    const bool agent = spaces_[index].agent;
    const bool current = space_id == current_space_id_;
    // Button presses request a switch; only a Rust snapshot changes selection.
    auto choice = IconButton<FooterButton>(
        base::BindRepeating(&Delegate::SelectSpace,
                            base::Unretained(&delegate_.get()), space_id),
        nullptr);
    choice->SetImageModel(
        views::Button::STATE_NORMAL,
        SpaceGlyph(position == 0 && !agent, spaces_[index].icon, current,
                   GetFills(GetColorProvider()), agent));
    // Arc spaces its Space dots 34 DIP apart.
    choice->SetMinSize(gfx::Size(34, 32));
    choice->SetMaxSize(gfx::Size(34, 32));
    choice->SetPreferredSize(gfx::Size(34, 32));
    if (current) {
      current_space_index = spaces->children().size();
    }
    // Ctrl-1 to Ctrl-8 select those Spaces and Ctrl-9 the last one, in the
    // Spaces' order.
    FooterTooltip::Content tooltip = {agent ? u"Agent Space" : space_name, {}};
    if (index < 8) {
      tooltip.keys = {u"\u2303", base::NumberToString16(index + 1)};
    } else if (index + 1 == spaces_.size()) {
      tooltip.keys = {u"\u2303", u"9"};
    }
    choice->SetFooterTooltip(footer_tooltip_->GetWeakPtr(), std::move(tooltip));
    choice->GetViewAccessibility().SetName(
        (agent ? u"Agent Space: " : u"Space: ") + space_name);
    choice->GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    choice->GetViewAccessibility().SetCheckedState(
        current ? ax::mojom::CheckedState::kTrue
                : ax::mojom::CheckedState::kFalse);
    TrackFocus(choice.get(), FocusAction::kSpaces, space_id);
    spaces->AddChildView(std::move(choice));
  }
  // Then other Profiles' Spaces, all in one pill. They aren't numbered.
  ProfileGroup* group = nullptr;
  if (!other_spaces_.empty()) {
    std::u16string names;
    size_t profiles = 0;
    for (size_t index = 0; index < other_spaces_.size(); ++index) {
      if (index == 0 || other_spaces_[index].profile_dir !=
                            other_spaces_[index - 1].profile_dir) {
        names += (profiles++ ? u", " : u"") + other_spaces_[index].profile_name;
      }
    }
    group = spaces->AddChildView(std::make_unique<ProfileGroup>(
        footer_tooltip_->GetWeakPtr(),
        profiles == 1 ? names : u"Other Profiles' Spaces", names));
  }
  for (size_t index = 0; index < other_spaces_.size(); ++index) {
    const OtherSpaceEntry& other = other_spaces_[index];
    auto choice = IconButton<FooterButton>(
        base::BindRepeating(&Delegate::SwitchToSpaceInProfile,
                            base::Unretained(&delegate_.get()),
                            other.profile_dir, other.space_id),
        nullptr);
    choice->SetImageModel(
        views::Button::STATE_NORMAL,
        SpaceGlyph(false, other.icon, false, GetFills(GetColorProvider())));
    choice->SetMinSize(gfx::Size(34, 32));
    choice->SetMaxSize(gfx::Size(34, 32));
    choice->SetPreferredSize(gfx::Size(34, 32));
    choice->SetFooterTooltip(
        footer_tooltip_->GetWeakPtr(),
        {other.name + u" \u00B7 " + other.profile_name, {}});
    choice->GetViewAccessibility().SetName(u"Space: " + other.name +
                                           u", Profile: " + other.profile_name);
    TrackFocus(choice.get(), FocusAction::kOtherSpace, index);
    group->AddChildView(std::move(choice));
  }
  // Non-layer ScrollViews need an explicit contents size too.
  spaces->SizeToPreferredSize();
  static_cast<views::BoxLayout*>(footer->GetLayoutManager())
      ->SetFlexForView(spaces_scroll_, 1);
  spaces_scroll_->RegisterPostLayoutCallback(base::BindRepeating(
      [](std::optional<gfx::PointF>& offset, bool reveal_current,
         std::optional<size_t> current, views::ScrollView* scroll) {
        if (auto saved = std::exchange(offset, std::nullopt)) {
          scroll->ScrollToOffset(*saved);
          if (reveal_current && current) {
            scroll->contents()->children()[*current]->ScrollViewToVisible();
          }
        }
      },
      base::OwnedRef(std::optional(spaces_offset)), space_changed,
      current_space_index));

  // The button draws its plus and turns it into a cross with the menu.
  auto create = IconButton<CreateButton>(
      base::BindRepeating(&SidebarView::ShowCreateMenu,
                          weak_factory_.GetWeakPtr()),
      nullptr);
  // No shortcut opens the creation menu. SetCreateMenuOpen sets the tooltip.
  create->GetViewAccessibility().SetName(u"Create Space");
  create->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  TrackFocus(create.get(), FocusAction::kCreate);
  create_button_ = footer->AddChildView(std::move(create));
  footer_ = footer;
  // Last, so they paint over the rows.
  downloads_stack_ = AddChildView(std::make_unique<DownloadsStack>(*delegate_));
  downloads_stack_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  create_menu_ = AddChildView(std::make_unique<CreateMenu>(
      base::BindRepeating(
          [](base::WeakPtr<SidebarView> sidebar, CreateMenu::Item item) {
            if (sidebar) {
              sidebar->ChooseCreateItem(static_cast<int>(item));
            }
          },
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SidebarView::SetCreateMenuOpen,
                          weak_factory_.GetWeakPtr())));
  create_menu_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  create_menu_open_ = false;
  if (!capture_menu_scheduled_ &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(kCaptureCreateMenu)) {
    capture_menu_scheduled_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<SidebarView> sidebar) {
              if (sidebar && sidebar->create_menu_ && sidebar->create_button_) {
                sidebar->create_menu_->Open(*sidebar->create_button_,
                                            /*hold=*/true);
              }
            },
            weak_factory_.GetWeakPtr()),
        base::Milliseconds(2000));
  }
}

std::u16string SidebarView::GetCreateTooltipForTesting() const {
  const auto target = std::ranges::find(focus_targets_, FocusAction::kCreate,
                                        &FocusTarget::action);
  return target == focus_targets_.end()
             ? std::u16string()
             : static_cast<FooterButton*>(target->view.get())
                   ->footer_tooltip()
                   .label;
}

void SidebarView::ShowFooterTooltipForCapture(std::string_view target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // An open Library replaces the footer; its Exit Library tile stands in once
  // the Library's opening motion has settled.
  if (library_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<SidebarView> sidebar) {
              if (sidebar && sidebar->library_) {
                sidebar->library_->ShowExitTooltipNow();
              }
            },
            weak_factory_.GetWeakPtr()),
        base::Milliseconds(500));
    return;
  }
  const FocusAction action = target == "space"    ? FocusAction::kSpaces
                             : target == "create" ? FocusAction::kCreate
                                                  : FocusAction::kLibrary;
  for (const auto& focus : focus_targets_) {
    if (focus.action == action &&
        (action != FocusAction::kSpaces || focus.id == current_space_id_)) {
      if (auto* button = views::AsViewClass<FooterButton>(focus.view.get())) {
        button->ShowFooterTooltipNow();
      }
      return;
    }
  }
}

void SidebarView::RefreshDownloadsStack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (downloads_stack_) {
    downloads_stack_->Refresh();
  }
}

void SidebarView::ShowDownloadsStackForCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (downloads_stack_) {
    downloads_stack_->ShowNow();
  }
}

void SidebarView::AddedToWidget() {
  views::View::AddedToWidget();
  if (!create_panel_) {
    SetSidebarTheme(*GetWidget(), space_theme_, /*fade=*/false);
  }
}

void SidebarView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  if (library_ && library_exit_.is_animating() && parent()) {
    // The Library keeps its width and leads the parent's narrowing edge.
    const int library_width = library_->GetPreferredSize().width();
    const int closed = std::max(0, library_width - parent()->width());
    library_->SetBounds(-base::ClampRound(kLibraryExitSpeed * closed), 0,
                        library_width, height());
  }
  if (downloads_stack_ && footer_) {
    // Bottom on the footer's top, across the sidebar's inner width. A short
    // window clips the oldest rows rather than placing the stack above it.
    const int height =
        std::clamp(downloads_stack_->GetPreferredSize().height(), 0,
                   std::max(footer_->y(), 0));
    downloads_stack_->SetBounds(kSidebarInsets.left(), footer_->y() - height,
                                width() - kSidebarInsets.width(), height);
  }
  if (create_menu_ && footer_) {
    // The Create button is the footer's last control, at its right end.
    const int button_right = width() - kSidebarInsets.right();
    const int button_center = button_right - 16;
    create_menu_->Place(button_right - kCreateMenuRightInset,
                        footer_->y() - kCreateMenuGap,
                        button_center - kCreateMenuNotchShift);
  }
}

void SidebarView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &space_slide_) {
    UpdateSpaceSlide(false);
    return;
  }
  const base::TimeDelta elapsed =
      kLibraryExit * library_exit_.GetCurrentValue();
  if (library_) {
    if (elapsed < kRowsSwap) {
      InvalidateLayout();
      return;
    }
    // The rows rebuild under a transparent layer and fade in; until the
    // layer goes they take no clicks.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(0);
    SetCanProcessEventsWithinSubtree(false);
    auto weak = weak_factory_.GetWeakPtr();
    FinishCloseLibrary(LibraryHasFocus());
    if (!weak) {
      return;
    }
  }
  if (layer()) {
    layer()->SetOpacity(
        std::clamp((elapsed - kRowsSwap) / kRowsFadeIn, 0.0, 1.0));
  }
}

void SidebarView::AnimationEnded(const gfx::Animation* animation) {
  if (animation == &space_slide_) {
    UpdateSpaceSlide(true);
    return;
  }
  if (library_) {
    auto weak = weak_factory_.GetWeakPtr();
    FinishCloseLibrary(LibraryHasFocus());
    if (!weak) {
      return;
    }
  }
  AnimationCanceled(animation);
}

void SidebarView::AnimationCanceled(const gfx::Animation* animation) {
  if (animation == &space_slide_) {
    UpdateSpaceSlide(true);
    return;
  }
  if (layer()) {
    DestroyLayer();
  }
  SetCanProcessEventsWithinSubtree(true);
}

void SidebarView::UpdateSpaceSlide(bool done) {
  if (!space_rows_) {
    return;
  }
  if (done) {
    space_rows_->TakeOutgoing();
    if (space_rows_->layer()) {
      space_rows_->DestroyLayer();
    }
    return;
  }
  if (!space_rows_->layer()) {
    space_rows_->SetPaintToLayer();
    space_rows_->layer()->SetFillsBoundsOpaquely(false);
  }
  // The rows may not be laid out yet; the sidebar's inner width is theirs.
  const double left = 1.0 - gfx::Tween::CalculateValue(
                                gfx::Tween::EASE_OUT,
                                space_slide_.GetCurrentValue());
  gfx::Transform transform;
  transform.Translate(
      -space_slide_shift_ * (width() - kSidebarInsets.width()) * left, 0);
  space_rows_->layer()->SetTransform(transform);
}

gfx::Rect SidebarView::GetSpaceRowsBoundsInWidget() const {
  if (!space_rows_ || !space_rows_->GetWidget() || library_ || create_panel_) {
    return gfx::Rect();
  }
  return space_rows_->ConvertRectToWidget(space_rows_->GetVisibleBounds());
}

void SidebarView::SlideInSpace(const gfx::ImageSkia& outgoing,
                               const gfx::Point& origin,
                               int shift) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!space_rows_ || outgoing.isNull() ||
      gfx::Animation::PrefersReducedMotion() || !IsDrawn()) {
    return;
  }
  // The picture starts where it showed in the other window, which has this
  // window's frame, however far each sidebar is scrolled. Whatever showed
  // above these rows' top is cut, as the rows' layer is.
  int top = origin.y() - space_rows_->ConvertRectToWidget(gfx::Rect()).y();
  gfx::ImageSkia picture = outgoing;
  if (top < 0) {
    if (-top >= outgoing.height()) {
      return;
    }
    picture = gfx::ImageSkiaOperations::ExtractSubset(
        outgoing,
        gfx::Rect(0, -top, outgoing.width(), outgoing.height() + top));
    top = 0;
  }
  // The picture takes the outgoing rows' place, so the slide is the same.
  auto rows = std::make_unique<SpaceRows>();
  auto* image = rows->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromImageSkia(picture)));
  image->SetHorizontalAlignment(views::ImageViewBase::Alignment::kLeading);
  image->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(top, 0, 0, 0)));
  space_slide_.Stop();
  space_slide_shift_ = shift;
  space_rows_->SetOutgoing(std::move(rows), shift);
  space_slide_.Start();
  UpdateSpaceSlide(false);
}

void SidebarView::SetNewTabIntent(bool intent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  new_tab_intent_ = intent;
  UpdateFills();
}

void SidebarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateFills();
}

void SidebarView::UpdateFills() {
  const Fills fills = GetFills(GetColorProvider());
  // With Increase Contrast on, the selected tile also gets a 1px outline.
  const bool more_contrast = GetNativeTheme()->preferred_contrast() ==
                             ui::NativeTheme::PreferredContrast::kMore;
  for (const auto& [tile, selected] : tiles_) {
    // Arc tints a selected tile's fill and rings it in its favicon's colour.
    const std::optional<SkColor> tint =
        selected ? FaviconTint(*tile) : std::nullopt;
    SkColor fill = selected ? fills.selected_tile : fills.tile;
    if (tint) {
      fill = SkColorSetA(color_utils::AlphaBlend(
                             *tint, SkColorSetA(fill, SK_AlphaOPAQUE), 0.12f),
                         std::max<SkAlpha>(SkColorGetA(fill), 0x80));
    }
    tile->SetBgColorOverrideDeprecated(fill);
    if (auto* item_tile = views::AsViewClass<ItemTile>(tile.get())) {
      item_tile->SetRing(
          tint ? std::optional(SkColorSetA(
                     color_utils::AlphaBlend(*tint, SK_ColorWHITE, 0.5f), 0xD9))
               : std::nullopt);
    }
    tile->SetStrokeColorIdOverride(selected && more_contrast
                                       ? std::optional(ui::kColorSysOutline)
                                       : std::nullopt);
  }
  if (new_tab_) {
    new_tab_->SetBgColorOverrideDeprecated(
        new_tab_intent_ ? fills.new_tab : SK_ColorTRANSPARENT);
    // Arc dims the idle New Tab row like the Space title and brightens it
    // under the pointer or while a new tab is pending.
    const ui::ColorVariant idle =
        new_tab_intent_ ? ui::ColorVariant(ui::kColorSysOnSurface)
                        : ui::ColorVariant(fills.title);
    for (const auto state : {views::Button::STATE_NORMAL,
                             views::Button::STATE_HOVERED,
                             views::Button::STATE_PRESSED}) {
      const ui::ColorVariant color = state == views::Button::STATE_NORMAL
                                         ? idle
                                         : ui::kColorSysOnSurface;
      new_tab_->SetTextColor(state, color);
      new_tab_->SetImageModel(
          state, ui::ImageModel::FromVectorIcon(vector_icons::kAdd2Icon, color,
                                                16));
    }
  }
  const auto clear = std::ranges::find(focus_targets_, FocusAction::kClearToday,
                                       &FocusTarget::action);
  if (clear != focus_targets_.end()) {
    auto* button = static_cast<views::MdTextButton*>(clear->view.get());
    const SkColor dim =
        SkColorSetA(fills.title, SkColorGetA(fills.title) * 3 / 4);
    for (const auto state : {views::Button::STATE_NORMAL,
                             views::Button::STATE_HOVERED,
                             views::Button::STATE_PRESSED}) {
      const ui::ColorVariant color = state == views::Button::STATE_NORMAL
                                         ? ui::ColorVariant(dim)
                                         : ui::ColorVariant(fills.title);
      button->SetTextColor(state, color);
      button->SetImageModel(
          state, ui::ImageModel::FromVectorIcon(kArrowDownIcon, color, 12));
    }
  }
  const auto rename = std::ranges::find(focus_targets_, FocusAction::kRename,
                                        &FocusTarget::action);
  if (rename != focus_targets_.end()) {
    auto* heading = static_cast<views::MdTextButton*>(rename->view.get());
    heading->SetEnabledTextColors(fills.title);
    const auto space =
        std::ranges::find(spaces_, current_space_id_, &SpaceEntry::id);
    heading->SetImageModel(
        views::Button::STATE_NORMAL,
        SpaceIcon(space != spaces_.end() ? space->icon : SpaceIconChoice(),
                  fills.title, space != spaces_.end() && space->agent));
  }
  for (const auto& target : focus_targets_) {
    if (target.action == FocusAction::kOtherSpace &&
        target.id < footer_other_spaces_.size()) {
      const OtherSpaceEntry& other = footer_other_spaces_[target.id];
      static_cast<views::MdTextButton*>(target.view.get())
          ->SetImageModel(views::Button::STATE_NORMAL,
                          SpaceGlyph(false, other.icon, false, fills));
    }
    if (target.action == FocusAction::kSpaces) {
      const auto space = std::ranges::find(spaces_, target.id, &SpaceEntry::id);
      if (space == spaces_.end()) {
        continue;
      }
      const auto first =
          std::ranges::find(spaces_, false, &SpaceEntry::agent);
      static_cast<views::MdTextButton*>(target.view.get())
          ->SetImageModel(views::Button::STATE_NORMAL,
                          SpaceGlyph(space == first, space->icon,
                                     target.id == footer_space_id_, fills,
                                     space->agent));
    }
  }
  SetCreateMenuOpen(create_menu_open_);
}

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace myarc
