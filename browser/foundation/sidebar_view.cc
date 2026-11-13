#include "myarc/sidebar_view.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
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
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
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

// Arc packs the Space glyphs left, 6 DIP after the Library button: the
// footer's 4 DIP spacing and 2 more.
constexpr int kSpacesLead = 2;
// Arc spaces its Space glyphs 34 DIP apart. A short strip closes them up to
// 20 DIP before they scroll.
constexpr int kSpacesPitch = 34;
constexpr int kSpacesMinPitch = 20;
// The other-Profiles group's gap after the glyph before it, and its inset on
// each side.
constexpr int kGroupGap = 4;
constexpr int kGroupInset = 2;

// A light capsule behind the other Profiles' Space glyphs. It hugs the glyph
// row with 4 DIP to spare on each side and fills with the other Spaces' glyph
// color at a quarter of its alpha. Its padding shows the Profiles' names as a
// footer tooltip.
class ProfileGroup final : public views::View {
  METADATA_HEADER(ProfileGroup, views::View)

 public:
  ProfileGroup(base::WeakPtr<FooterTooltip> tooltip,
               std::u16string name,
               std::u16string names)
      : tooltip_(std::move(tooltip)), names_(std::move(names)) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
    GetViewAccessibility().SetName(std::move(name));
  }

  // The strip sizes the group; its glyph buttons split the inner width.
  void Layout(PassKey) override {
    if (children().empty()) {
      return;
    }
    const int pitch =
        (width() - 2 * kGroupInset) / static_cast<int>(children().size());
    for (size_t index = 0; index < children().size(); ++index) {
      children()[index]->SetBounds(
          kGroupInset + static_cast<int>(index) * pitch, 0, pitch, height());
    }
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

// The footer's Space glyphs, packed left after kSpacesLead with the
// other-Profiles group last. When the strip is too short for them at Arc's
// pitch, the pitch closes up so the group still shows whole; only below
// kSpacesMinPitch do the glyphs overflow and scroll.
class SpaceStrip final : public views::View {
  METADATA_HEADER(SpaceStrip, views::View)

 public:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(WidthAt(PitchFor(available_size.width())), 32);
  }

  void Layout(PassKey) override {
    // The ScrollView stretches this view to at least its viewport, so the
    // pitch follows the viewport's width.
    const int pitch = PitchFor(parent() ? views::SizeBound(parent()->width())
                                        : views::SizeBound());
    int x = kSpacesLead;
    for (views::View* child : children()) {
      if (views::IsViewClass<ProfileGroup>(child)) {
        x += kGroupGap;
        const int glyphs = static_cast<int>(child->children().size());
        child->SetBounds(x, 0, 2 * kGroupInset + glyphs * pitch, height());
      } else {
        child->SetBounds(x, 0, pitch, height());
      }
      x = child->bounds().right();
    }
  }

 private:
  // The glyph count and the width the strip needs besides the glyphs.
  std::pair<int, int> Measure() const {
    int glyphs = 0;
    int fixed = kSpacesLead;
    for (const views::View* child : children()) {
      if (views::IsViewClass<ProfileGroup>(child)) {
        glyphs += static_cast<int>(child->children().size());
        fixed += kGroupGap + 2 * kGroupInset;
      } else {
        ++glyphs;
      }
    }
    return {glyphs, fixed};
  }

  int PitchFor(views::SizeBound width) const {
    const auto [glyphs, fixed] = Measure();
    if (!width.is_bounded() || glyphs == 0) {
      return kSpacesPitch;
    }
    return std::clamp((width.value() - fixed) / glyphs, kSpacesMinPitch,
                      kSpacesPitch);
  }

  int WidthAt(int pitch) const {
    const auto [glyphs, fixed] = Measure();
    return fixed + glyphs * pitch;
  }
};

BEGIN_METADATA(SpaceStrip)
END_METADATA

enum ItemCommand {
  kPin = 1,
  kFavorite,
  kUnsave,
  kClose,
  kStopControl,
  kCopyLink,
  kNewFolder,
  kRenameFolder,
  kDeleteFolder,
  kRemoveFromFolder,
  kMoveToFolder,
  kRenameItem,
  kOpenInSplitView,
  kSeparateSplit,
  // Move to Folder's entries: this plus the folder's index in folders_.
  kMoveToFolderFirst = 1000
};

// Arc indents a folder's tabs under its name.
constexpr int kFolderIndent = 14;
// The disclosure arrow at a folder row's right end.
constexpr int kChevronSize = 12;
constexpr int kChevronInset = 10;

// Browser control's accent: a small dot on the agent Space's footer glyph
// while a client is active.
constexpr SkColor kControlAccent = SkColorSetRGB(0x0B, 0x86, 0xFE);

// `glyph` with the control dot at its top-right corner.
class ActiveGlyphSource : public gfx::CanvasImageSource {
 public:
  explicit ActiveGlyphSource(gfx::ImageSkia glyph)
      : gfx::CanvasImageSource(glyph.size()), glyph_(std::move(glyph)) {}

  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawImageInt(glyph_, 0, 0);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kControlAccent);
    canvas->DrawCircle(gfx::PointF(size().width() - 2.5f, 2.5f), 2.5f, flags);
  }

 private:
  const gfx::ImageSkia glyph_;
};

ui::ImageModel ActiveGlyph(const ui::ImageModel& glyph,
                           const ui::ColorProvider* provider) {
  gfx::ImageSkia image = glyph.Rasterize(provider);
  if (image.isNull()) {
    return glyph;
  }
  return ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<ActiveGlyphSource>(
          std::move(image)));
}

// Opens the creation menu once, held open, for background captures.
constexpr char kCaptureCreateMenu[] = "myarc-capture-create-menu";
// Opens one seeded row once the workspace loads, for background captures.
constexpr char kCaptureActivate[] = "myarc-capture-activate";
// Opens the first tile's context menu once, for background captures.
constexpr char kCaptureItemMenu[] = "myarc-capture-item-menu";
// Drags a row onto a folder row once, for background captures:
// <item ID>:<folder ID>, and :hold to keep it over the folder.
constexpr char kCaptureDrag[] = "myarc-capture-drag";

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
    // Dormant Pins have no live page to close.
    if (close) {
      close_ = AddChildView(std::move(close));
      close_->SetVisible(false);
    }
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
    if (close_) {
      close_->SetVisible(reveal);
    }
  }

  raw_ptr<views::MdTextButton> close_ = nullptr;
  const bool selected_;
  bool hovered_ = false;
  base::ScopedObservation<views::FocusManager, views::FocusChangeListener>
      focus_observation_{this};
};

BEGIN_METADATA(ItemRow)
END_METADATA

// The thin line between a split's panes: 1 DIP wide and 16 DIP tall,
// centred, in the Space dots' colour.
class SplitDivider final : public views::View {
  METADATA_HEADER(SplitDivider, views::View)

 public:
  SplitDivider() { SetPreferredSize(gfx::Size(1, 16)); }

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(gfx::Rect(0, (height() - 16) / 2, 1, 16),
                     GetFills(GetColorProvider()).space_dot);
  }
};

BEGIN_METADATA(SplitDivider)
END_METADATA

// A split's row holds its two panes' rows side by side with the divider
// between them, as Arc draws a split in the sidebar. While either pane shows,
// the row takes the resting tile fill and the focused pane its own highlight.
class SplitRow final : public views::View {
  METADATA_HEADER(SplitRow, views::View)

 public:
  SplitRow(std::unique_ptr<views::View> left,
           std::unique_ptr<views::View> right,
           bool selected)
      : selected_(selected) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets(), 0));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);
    layout->SetFlexForView(AddChildView(std::move(left)), 1);
    AddChildView(std::make_unique<SplitDivider>());
    layout->SetFlexForView(AddChildView(std::move(right)), 1);
    GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
    GetViewAccessibility().SetName(u"Split View");
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SetBackground(selected_ ? views::CreateRoundedRectBackground(
                                  GetFills(GetColorProvider()).tile, 10)
                            : nullptr);
  }

 private:
  const bool selected_;
};

BEGIN_METADATA(SplitRow)
END_METADATA

// A folder's row: its button fills the row and opens or closes it, and the
// row paints the hover fill and the disclosure arrow at its right end.
class FolderRow final : public views::View {
  METADATA_HEADER(FolderRow, views::View)

 public:
  FolderRow(std::unique_ptr<views::MdTextButton> toggle, bool open)
      : open_(open) {
    views::InkDrop::Get(toggle.get())
        ->SetMode(views::InkDropHost::InkDropMode::OFF);
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        Orientation::kHorizontal, gfx::Insets(), 0));
    static_cast<views::BoxLayout*>(GetLayoutManager())
        ->SetFlexForView(AddChildView(std::move(toggle)), 1);
    SetNotifyEnterExitOnChild(true);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    hovered_ = true;
    UpdateFill();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    hovered_ = false;
    UpdateFill();
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    UpdateFill();
  }

  // The button above is transparent, so the arrow shows through it.
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    const gfx::ImageSkia chevron = gfx::CreateVectorIcon(
        open_ ? kChevronDownIcon : kChevronRightIcon, kChevronSize,
        GetFills(GetColorProvider()).title);
    canvas->DrawImageInt(chevron, width() - kChevronInset - kChevronSize,
                         (height() - kChevronSize) / 2);
  }

 private:
  void UpdateFill() {
    SetBackground(hovered_ ? views::CreateRoundedRectBackground(
                                 GetFills(GetColorProvider()).tile, 10)
                           : nullptr);
  }

  const bool open_;
  bool hovered_ = false;
};

BEGIN_METADATA(FolderRow)
END_METADATA

// Hands every event under the sidebar to its row drag before the target sees
// it, so a press on a row's button can turn into a drag.
class RowDragHandler final : public ui::EventHandler {
 public:
  explicit RowDragHandler(base::RepeatingCallback<void(ui::Event&)> on_event)
      : on_event_(std::move(on_event)) {}

  void OnEvent(ui::Event* event) override { on_event_.Run(*event); }

 private:
  base::RepeatingCallback<void(ui::Event&)> on_event_;
};

// Where a dragged row will land: a 2 DIP line between rows or tiles, or an
// outline around the folder row it will go into, in the title's colour.
class DropIndicator final : public views::View {
  METADATA_HEADER(DropIndicator, views::View)

 public:
  DropIndicator() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
  }

  void Show(const gfx::Rect& mark, bool outline) {
    outline_ = outline;
    SetBoundsRect(mark);
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetFills(GetColorProvider()).title);
    gfx::RectF bounds(GetLocalBounds());
    if (outline_) {
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kStroke);
      bounds.Inset(kStroke / 2);
      canvas->DrawRoundRect(bounds, 10 - kStroke / 2, flags);
      return;
    }
    canvas->DrawRoundRect(bounds, 1, flags);
  }

 private:
  static constexpr float kStroke = 1.5f;
  bool outline_ = false;
};

BEGIN_METADATA(DropIndicator)
END_METADATA

// A row's drop line is this thick and centred on the edge it marks.
constexpr int kDropLine = 2;
// A dragged row fades to this while it is lifted.
constexpr float kLiftedOpacity = 0.4f;

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
  row_drag_handler_ = std::make_unique<RowDragHandler>(base::BindRepeating(
      &SidebarView::OnRowDragEvent, base::Unretained(this)));
  AddPreTargetHandler(row_drag_handler_.get());
  SetUnavailable(u"Loading workspace\u2026");
}

SidebarView::~SidebarView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  menu_runner_.reset();
  RemovePreTargetHandler(row_drag_handler_.get());
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
  const auto agent = std::ranges::find(spaces_, true, &SpaceEntry::agent);
  if (target != focus_targets_.end() &&
      target->action == FocusAction::kSpaces && agent != spaces_.end() &&
      target->id == agent->id && !control_client_.empty()) {
    menu_item_id_ = 0;
    menu_folder_id_ = 0;
    menu_runner_.reset();
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    folder_menu_model_.reset();
    menu_model_->AddItem(kStopControl, u"Stop " + control_client_);
    RunMenu(point, source_type);
    return;
  }
  if (target != focus_targets_.end() &&
      (target->action == FocusAction::kRename ||
       target->action == FocusAction::kFolder)) {
    // The Space title and folder rows offer Arc's folder commands.
    menu_item_id_ = 0;
    menu_folder_id_ = target->action == FocusAction::kFolder ? target->id : 0;
    menu_space_id_ = current_space_id_;
    menu_runner_.reset();
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    folder_menu_model_.reset();
    if (menu_folder_id_) {
      menu_model_->AddItem(kRenameFolder, u"Rename Folder\u2026");
      menu_model_->AddItem(kNewFolder, u"New Folder");
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_model_->AddItem(kDeleteFolder, u"Delete Folder\u2026");
    } else {
      menu_model_->AddItem(kNewFolder, u"New Folder");
    }
    RunMenu(point, source_type);
    return;
  }
  if (target == focus_targets_.end() || !target->item_menu) {
    return;
  }
  menu_item_id_ = target->id;
  menu_folder_id_ = 0;
  menu_space_id_ = current_space_id_;
  menu_state_ = *target->item_menu;
  menu_runner_.reset();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  folder_menu_model_.reset();
  menu_folder_ids_.clear();
  // Arc's order: Copy Link first, the page's own actions next, and removal
  // last behind its own separator.
  if (!menu_state_.url.empty()) {
    menu_model_->AddItem(kCopyLink, u"Copy Link");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  // Arc renames a pinned tab from its menu.
  if (menu_state_.is_pin) {
    menu_model_->AddItem(kRenameItem, u"Rename\u2026");
  }
  if (!menu_state_.is_pin) {
    menu_model_->AddItem(kPin, menu_state_.is_favorite ? u"Move to Space Pins"
                                                       : u"Pin to Space");
  }
  if (!menu_state_.is_favorite) {
    menu_model_->AddItem(kFavorite, menu_state_.is_pin
                                        ? u"Move to Profile Favorites"
                                        : u"Add to Profile Favorites");
  }
  // Pins, and tabs pinned on the way, file into the Space's folders.
  if (!menu_state_.is_favorite) {
    folder_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    for (const FolderEntry& folder : folders_) {
      if (folder.id != menu_state_.folder) {
        folder_menu_model_->AddItem(
            kMoveToFolderFirst + static_cast<int>(menu_folder_ids_.size()),
            folder.name);
        menu_folder_ids_.push_back(folder.id);
      }
    }
    if (!menu_folder_ids_.empty()) {
      menu_model_->AddSubMenu(kMoveToFolder, u"Move to Folder",
                              folder_menu_model_.get());
    }
    if (menu_state_.folder) {
      menu_model_->AddItem(kRemoveFromFolder, u"Remove from Folder");
    }
  }
  // Arc offers Open in Split View on a tab's menu.
  if (menu_state_.can_split) {
    menu_model_->AddItem(kOpenInSplitView, u"Open in Split View");
  }
  if (menu_state_.in_split) {
    menu_model_->AddItem(kSeparateSplit, u"Separate All Tabs");
  }
  if (menu_state_.is_live) {
    menu_model_->AddItem(
        kClose, menu_state_.is_pin        ? u"Close live page (keep Space Pin)"
                : menu_state_.is_favorite ? u"Close live page (keep Favorite)"
                                          : u"Close live page");
  }
  if (menu_state_.is_pin || menu_state_.is_favorite) {
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(kUnsave, menu_state_.is_pin ? u"Unpin from Space"
                                                     : u"Remove Favorite");
  }
  RunMenu(point, source_type);
}

void SidebarView::RunMenu(const gfx::Point& point,
                          ui::mojom::MenuSourceType source_type) {
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
  row_drag_.reset();
  deferred_model_.reset();
  drop_indicator_ = nullptr;
  drop_zones_.clear();
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
                           sidebar->delegate_->NewSplit();
                           return;
                       }
                     },
                     weak_factory_.GetWeakPtr(), item));
}

void SidebarView::OpenCaptureItemMenu(int tries) {
  if (tiles_.empty() || tries <= 0) {
    return;
  }
  // A background app's native menu closes on the user's input elsewhere, so
  // reopen it until the capture has had its chance.
  if (!menu_runner_ || !menu_runner_->IsRunning()) {
    views::View* tile = tiles_.front().first;
    tile->ShowContextMenu(tile->GetBoundsInScreen().CenterPoint(),
                          ui::mojom::MenuSourceType::kMouse);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SidebarView::OpenCaptureItemMenu,
                     weak_factory_.GetWeakPtr(), tries - 1),
      base::Milliseconds(300));
}

void SidebarView::SetControlClient(std::u16string client,
                                   base::RepeatingClosure stop) {
  control_client_ = std::move(client);
  stop_control_ = std::move(stop);
}

void SidebarView::StopControl() {
  // Stopping redraws this view; run it once the press or menu unwinds.
  if (stop_control_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             stop_control_);
  }
}

void SidebarView::ExecuteCommand(int command_id, int event_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (command_id == kStopControl) {
    StopControl();
    return;
  }
  if (command_id == kNewFolder) {
    menu_item_id_ = 0;
    menu_folder_id_ = 0;
    delegate_->CreateFolder();
    return;
  }
  if (command_id == kRenameFolder || command_id == kDeleteFolder) {
    const uint64_t folder = std::exchange(menu_folder_id_, 0);
    if (!folder) {
      return;
    }
    if (command_id == kRenameFolder) {
      delegate_->RenameFolder(folder);
    } else {
      delegate_->DeleteFolder(folder);
    }
    return;
  }
  if (command_id == kCopyLink) {
    menu_item_id_ = 0;
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(menu_state_.url);
    return;
  }
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
    case kRemoveFromFolder:
      delegate_->MovePinToFolder(id, 0);
      return;
    case kRenameItem:
      delegate_->RenameItem(id);
      return;
    case kOpenInSplitView:
      delegate_->OpenInSplitView(id);
      return;
    case kSeparateSplit:
      delegate_->SeparateSplit(id);
      return;
  }
  const size_t index = static_cast<size_t>(command_id - kMoveToFolderFirst);
  if (command_id >= kMoveToFolderFirst && index < menu_folder_ids_.size()) {
    delegate_->MovePinToFolder(id, menu_folder_ids_[index]);
  }
}

void SidebarView::OnRowDragEvent(ui::Event& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event.IsKeyEvent()) {
    // Esc drops a lifted row back where it was.
    if (row_drag_ && row_drag_->lifted &&
        event.type() == ui::EventType::kKeyPressed &&
        event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE) {
      event.StopPropagation();
      EndRowDrag(/*commit=*/false);
    }
    return;
  }
  if (!event.IsMouseEvent()) {
    return;
  }
  auto* target = static_cast<views::View*>(event.target());
  const ui::MouseEvent& mouse = *event.AsMouseEvent();
  gfx::Point point = mouse.location();
  views::View::ConvertPointToTarget(target, this, &point);
  switch (mouse.type()) {
    case ui::EventType::kMousePressed:
      if (row_drag_ && row_drag_->lifted) {
        EndRowDrag(/*commit=*/false);
        return;
      }
      row_drag_.reset();
      if (!mouse.IsOnlyLeftMouseButton()) {
        return;
      }
      // Rows with an item drag: Pin tiles, folders' Pins and Today tabs.
      for (const DropZone& zone : drop_zones_) {
        if (zone.item && zone.view->Contains(target)) {
          row_drag_ = RowDrag{.item = zone.item,
                              .split = zone.split,
                              .source = zone.view,
                              .press = point};
          break;
        }
      }
      return;
    case ui::EventType::kMouseDragged:
      if (!row_drag_) {
        return;
      }
      if (!row_drag_->lifted) {
        if (!views::View::ExceededDragThreshold(point - row_drag_->press)) {
          return;
        }
        row_drag_->lifted = true;
        // The press won't end in a click, so the button lets go of it.
        if (auto* button = views::Button::AsButton(target)) {
          button->SetState(views::Button::STATE_NORMAL);
        }
        views::View& source = *row_drag_->source;
        source.SetPaintToLayer();
        source.layer()->SetFillsBoundsOpaquely(false);
        source.layer()->SetOpacity(kLiftedOpacity);
      }
      event.StopPropagation();
      ShowDrop(DropAt(point));
      return;
    case ui::EventType::kMouseReleased:
      if (!row_drag_) {
        return;
      }
      if (!row_drag_->lifted) {
        row_drag_.reset();
        return;
      }
      event.StopPropagation();
      ShowDrop(DropAt(point));
      EndRowDrag(/*commit=*/true);
      return;
    default:
      return;
  }
}

std::optional<SidebarView::Drop> SidebarView::DropAt(
    const gfx::Point& point) const {
  if (point.x() < 0 || point.x() >= width()) {
    return std::nullopt;
  }
  for (const DropZone& zone : drop_zones_) {
    const gfx::Rect bounds = views::View::ConvertRectToTarget(
        zone.view, this, zone.view->GetLocalBounds());
    if (!bounds.Contains(point)) {
      continue;
    }
    // A split's row moves within Today only, until a split can be pinned.
    if (row_drag_ && row_drag_->split &&
        zone.kind != foundation::DropTargetKind::Today) {
      return std::nullopt;
    }
    Drop drop = {.kind = zone.kind, .folder = zone.folder};
    if (!zone.item) {
      drop.before = zone.next;
      if (zone.kind == foundation::DropTargetKind::Folder) {
        drop.mark = bounds;
        drop.outline = true;
      } else {
        // Above the Space title, or below Today's divider and New Tab.
        const int y = zone.kind == foundation::DropTargetKind::Pins
                          ? bounds.y()
                          : bounds.bottom();
        drop.mark =
            gfx::Rect(bounds.x(), y - kDropLine / 2, bounds.width(), kDropLine);
      }
      return drop;
    }
    const bool before_row = zone.tile ? point.x() < bounds.CenterPoint().x()
                                      : point.y() < bounds.CenterPoint().y();
    drop.before = before_row ? zone.item : zone.next;
    if (zone.tile) {
      // In the middle of the 8 DIP gap beside the tile.
      const int x = before_row ? bounds.x() - 4 : bounds.right() + 4;
      drop.mark =
          gfx::Rect(x - kDropLine / 2, bounds.y(), kDropLine, bounds.height());
    } else {
      const int y = before_row ? bounds.y() : bounds.bottom();
      drop.mark =
          gfx::Rect(bounds.x(), y - kDropLine / 2, bounds.width(), kDropLine);
    }
    return drop;
  }
  // Gaps between rows keep the last drop, so the line doesn't flicker.
  return row_drag_ ? row_drag_->drop : std::nullopt;
}

void SidebarView::ShowDrop(const std::optional<Drop>& drop) {
  if (row_drag_) {
    row_drag_->drop = drop;
  }
  if (!drop) {
    if (drop_indicator_) {
      RemoveChildViewT(drop_indicator_.ExtractAsDangling());
    }
    return;
  }
  if (!drop_indicator_) {
    drop_indicator_ = AddChildView(std::make_unique<DropIndicator>());
  }
  static_cast<DropIndicator*>(drop_indicator_.get())
      ->Show(drop->mark, drop->outline);
}

void SidebarView::EndRowDrag(bool commit) {
  if (!row_drag_) {
    return;
  }
  const RowDrag drag = *std::exchange(row_drag_, std::nullopt);
  ShowDrop(std::nullopt);
  if (drag.lifted && drag.source->layer()) {
    drag.source->DestroyLayer();
  }
  std::unique_ptr<foundation::SidebarResult> deferred =
      std::move(deferred_model_);
  if (commit && drag.lifted && drag.drop) {
    // The drop's own model replaces any that arrived mid-drag. Dropping can
    // rebuild or delete this view.
    auto weak = weak_factory_.GetWeakPtr();
    const bool dropped = delegate_->DropItem(
        drag.item, drag.drop->kind, drag.drop->folder, drag.drop->before);
    if (!weak || dropped) {
      return;
    }
  }
  if (deferred) {
    SetModel(*deferred, deferred_status_, deferred_retry_);
  }
}

void SidebarView::RunCaptureDrag(uint64_t item,
                                 uint64_t folder,
                                 uint64_t row,
                                 bool hold) {
  const auto source = std::ranges::find(drop_zones_, item, &DropZone::item);
  const auto target =
      std::ranges::find_if(drop_zones_, [folder, row](const DropZone& zone) {
        return row ? zone.item == row
                   : zone.kind == foundation::DropTargetKind::Folder &&
                         zone.folder == folder && !zone.item;
      });
  if (!GetWidget() || source == drop_zones_.end() ||
      target == drop_zones_.end()) {
    LOG(ERROR) << "Capture drag found no row " << item << ", folder "
               << folder << " or target row " << row;
    return;
  }
  gfx::Point from = source->view->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToWidget(source->view, &from);
  // A row target takes the drop above it, a folder in its middle.
  const gfx::Rect target_bounds = target->view->GetLocalBounds();
  gfx::Point to = target_bounds.CenterPoint();
  if (row) {
    to.set_y(target_bounds.y() + target_bounds.height() / 4);
  }
  views::View::ConvertPointToWidget(target->view, &to);
  // Through the root view, as the window would send them, but without the
  // pointer, so the capture never takes focus or moves the mouse.
  views::View* root = GetWidget()->GetRootView();
  auto weak = weak_factory_.GetWeakPtr();
  const auto event = [](ui::EventType type, const gfx::Point& at) {
    return ui::MouseEvent(type, at, at, ui::EventTimeForNow(),
                          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  };
  root->OnMousePressed(event(ui::EventType::kMousePressed, from));
  constexpr int kSteps = 8;
  for (int step = 1; step <= kSteps && weak; ++step) {
    const gfx::Point at(from.x() + (to.x() - from.x()) * step / kSteps,
                        from.y() + (to.y() - from.y()) * step / kSteps);
    root->OnMouseDragged(event(ui::EventType::kMouseDragged, at));
  }
  if (weak && !hold) {
    root->OnMouseReleased(event(ui::EventType::kMouseReleased, to));
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
  // The user's name for a Pin wins over its page's title, live or stored, as
  // in Arc, until it is cleared.
  if (!item.name.empty()) {
    presentation.title = base::UTF8ToUTF16(std::string(item.name));
  }
  // Dormant saved rows show the last title their page had, as Arc does, and
  // fall back to a short label of the saved URL until a page has loaded once.
  if (presentation.title.empty() && !item.title.empty()) {
    presentation.title = base::UTF8ToUTF16(std::string(item.title));
  }
  if (presentation.title.empty()) {
    presentation.title =
        item.has_url ? base::UTF8ToUTF16(std::string(
                           foundation::url_label(item.canonical_url)))
                     : u"Untitled tab";
  }
  return presentation;
}

std::unique_ptr<views::View> SidebarView::MakeItem(
    const foundation::ItemRecord& item,
    bool tile,
    bool selected,
    uint64_t folder,
    bool in_split) {
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
      presentation.is_live,
      item.has_url ? base::UTF8ToUTF16(std::string(item.canonical_url))
                   : std::u16string(),
      folder,
      item.placement == foundation::PlacementKind::Temporary &&
          presentation.is_live && !in_split,
      in_split};
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

  // Today tabs close even while dormant. A folder's Pin only closes its live
  // page.
  if (item.placement != foundation::PlacementKind::Temporary &&
      !presentation.is_live) {
    return std::make_unique<ItemRow>(std::move(activate), nullptr, selected);
  }
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

std::unique_ptr<views::View> SidebarView::MakeSplit(
    const foundation::ItemRecord& left,
    const foundation::ItemRecord& right,
    uint64_t selected) {
  return std::make_unique<SplitRow>(
      MakeItem(left, false, left.id == selected, 0, /*in_split=*/true),
      MakeItem(right, false, right.id == selected, 0, /*in_split=*/true),
      left.id == selected || right.id == selected);
}

std::unique_ptr<views::View> SidebarView::MakeFolder(
    const foundation::FolderRecord& folder) {
  const std::u16string name = base::UTF8ToUTF16(std::string(folder.name));
  auto toggle = Button(base::BindRepeating(&Delegate::SetFolderOpen,
                                           base::Unretained(&delegate_.get()),
                                           folder.id, !folder.open),
                       name);
  toggle->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          folder.open ? kFolderOpenIcon : kFolderClosedIcon,
          ui::kColorSysOnSurface, 16));
  toggle->SetMinSize(gfx::Size(0, kRowHeight));
  AlignRowContents(*toggle, 16);
  // The title stops short of the disclosure arrow.
  toggle->SetCustomPadding(gfx::Insets::TLBR(
      6, kRowIconCenter - 8, 6, kChevronInset + kChevronSize + 6));
  toggle->SetTooltipText(name);
  toggle->GetViewAccessibility().SetName(u"Folder: " + name);
  if (folder.open) {
    toggle->GetViewAccessibility().SetIsExpanded();
  } else {
    toggle->GetViewAccessibility().SetIsCollapsed();
  }
  TrackFocus(toggle.get(), FocusAction::kFolder, folder.id);
  toggle->set_context_menu_controller(this);
  toggle->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  return std::make_unique<FolderRow>(std::move(toggle), folder.open);
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
  // Rebuilding would drop the row under the pointer; the drag's end applies
  // the latest model.
  if (row_drag_ && row_drag_->lifted) {
    deferred_model_ = std::make_unique<foundation::SidebarResult>(model);
    deferred_status_ = status;
    deferred_retry_ = retry;
    return;
  }
  row_drag_.reset();
  if (drop_indicator_) {
    RemoveChildViewT(drop_indicator_.ExtractAsDangling());
  }
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
  drop_zones_.clear();
  new_tab_ = nullptr;
  tiles_.clear();
  // Loading and title ticks rebuild only the rows. The footer stays, with its
  // hover, tooltip and Spaces scroll, until the Spaces or the current one
  // change.
  const bool keep_footer = footer_ && footer_spaces_ == spaces_ &&
                           footer_other_spaces_ == other_spaces_ &&
                           footer_space_id_ == current_space_id_ &&
                           footer_control_client_ == control_client_;
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
  // Pins in folders show as rows under their folder instead.
  std::set<uint64_t> foldered;
  folders_.clear();
  for (const auto& folder : model.folders) {
    folders_.push_back(
        {folder.id, base::UTF8ToUTF16(std::string(folder.name))});
    foldered.insert(folder.items.begin(), folder.items.end());
  }
  std::vector<const foundation::ItemRecord*> tile_items;
  for (const auto& item : model.favorites) {
    tile_items.push_back(&item);
  }
  for (const auto& item : model.pins) {
    if (!foldered.contains(item.id)) {
      tile_items.push_back(&item);
    }
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
    // Pin tiles drag and take drops; Favorites stay put.
    if (tile_items[i]->placement == foundation::PlacementKind::Pin) {
      const auto next = std::find_if(
          tile_items.begin() + i + 1, tile_items.end(), [](const auto* item) {
            return item->placement == foundation::PlacementKind::Pin;
          });
      drop_zones_.push_back({.view = tile,
                             .kind = foundation::DropTargetKind::Pins,
                             .item = tile_items[i]->id,
                             .next = next == tile_items.end() ? 0 : (*next)->id,
                             .tile = true});
    }
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
  if (!tile_items.empty()) {
    heading->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(6, 0, 0, 0));
  }
  // The bot, or the user's icon, and this tooltip mark the agent Space.
  heading->SetTooltipText((agent ? u"Agent Space: " : u"Rename Space: ") +
                          name);
  heading->GetViewAccessibility().SetName(
      (agent ? u"Rename Agent Space: " : u"Rename Space: ") + name);
  TrackFocus(heading.get(), FocusAction::kRename, model.current_space.id);
  // Dropping on the title adds the row to the end of the ungrouped Pins,
  // which is how a Pin leaves a folder when no Pin tiles show.
  drop_zones_.push_back(
      {.view = heading.get(), .kind = foundation::DropTargetKind::Pins});
  // Its menu makes a folder.
  heading->set_context_menu_controller(this);
  if (agent && !control_client_.empty()) {
    // While a control client is active, a Stop button follows the title.
    auto header = Box(Orientation::kHorizontal, 4);
    auto* layout = static_cast<views::BoxLayout*>(header->GetLayoutManager());
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    // Stop's right edge lines up with Clear's below it.
    header->SetProperty(
        views::kMarginsKey,
        (heading->GetProperty(views::kMarginsKey)
             ? *heading->GetProperty(views::kMarginsKey)
             : gfx::Insets()) +
            gfx::Insets::TLBR(0, 0, 0, 6));
    heading->ClearProperty(views::kMarginsKey);
    layout->SetFlexForView(header->AddChildView(std::move(heading)), 1);
    auto stop = Button(base::BindRepeating(&SidebarView::StopControl,
                                           base::Unretained(this)),
                       u"Stop");
    // Clear's 11pt semibold on a 24 DIP pill, so the title row keeps its
    // height and the fill stays clear of the divider.
    for (views::View* child : stop->children()) {
      if (auto* label = views::AsViewClass<views::Label>(child)) {
        label->SetFontList(
            label->font_list().DeriveWithSizeDelta(-2).DeriveWithWeight(
                gfx::Font::Weight::SEMIBOLD));
      }
    }
    stop->SetMinSize(gfx::Size());
    stop->SetCustomPadding(gfx::Insets::VH(4, 8));
    stop->SetEnabledTextColors(fills.title);
    stop->SetBgColorOverrideDeprecated(fills.tile);
    stop->SetCornerRadius(6);
    stop->SetTooltipText(u"Stop " + control_client_);
    stop->GetViewAccessibility().SetName(u"Stop " + control_client_);
    header->AddChildView(std::move(stop));
    rows->AddChildView(std::move(header));
  } else {
    rows->AddChildView(std::move(heading));
  }

  // The Space's folders follow its title, each with its Pins beneath it
  // while open. A closed folder still shows its selected Pin, so the page
  // on screen keeps its row.
  for (const auto& folder : model.folders) {
    // A row dropped on the folder's own row goes in at its end.
    drop_zones_.push_back({.view = rows->AddChildView(MakeFolder(folder)),
                           .kind = foundation::DropTargetKind::Folder,
                           .folder = folder.id});
    for (size_t i = 0; i < folder.items.size(); ++i) {
      const uint64_t id = folder.items[i];
      const auto item = std::ranges::find(model.pins, id,
                                          &foundation::ItemRecord::id);
      const bool selected = id == model.selected_item_id;
      if (item == model.pins.end() || (!folder.open && !selected)) {
        continue;
      }
      auto row = MakeItem(*item, false, selected, folder.id);
      row->SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(0, kFolderIndent, 0, 0));
      drop_zones_.push_back(
          {.view = rows->AddChildView(std::move(row)),
           .kind = foundation::DropTargetKind::Folder,
           .folder = folder.id,
           .item = id,
           .next = i + 1 < folder.items.size() ? folder.items[i + 1] : 0});
    }
  }

  // Arc divides the Space title from Today only while Today has tabs, and
  // puts Clear at the divider's right end.
  const bool divider = !model.temporary.empty();
  if (divider) {
    std::vector<uint64_t> today;
    for (const auto& item : model.temporary) {
      today.push_back(item.id);
    }
    // Today's divider and New Tab row take drops at Today's top.
    drop_zones_.push_back(
        {.view = rows->AddChildView(MakeTodayDivider(std::move(today))),
         .kind = foundation::DropTargetKind::Today,
         .next = model.temporary.front().id});
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
  drop_zones_.push_back(
      {.view = new_tab_.get(),
       .kind = foundation::DropTargetKind::Today,
       .next = model.temporary.empty() ? 0 : model.temporary.front().id});
  // A split shows as one row where its left tab sits; its right tab has no
  // row of its own. The row drags as one item and stands for its left tab.
  std::map<uint64_t, uint64_t> split_right;
  std::set<uint64_t> split_rights;
  for (const auto& split : model.splits) {
    split_right[split.left] = split.right;
    split_rights.insert(split.right);
  }
  for (size_t i = 0; i < model.temporary.size(); ++i) {
    const auto& item = model.temporary[i];
    if (split_rights.contains(item.id)) {
      continue;
    }
    if (const auto pair = split_right.find(item.id);
        pair != split_right.end()) {
      const auto right = std::ranges::find(model.temporary, pair->second,
                                           &foundation::ItemRecord::id);
      if (right != model.temporary.end()) {
        const auto after = std::max(model.temporary.begin() + i, right) + 1;
        drop_zones_.push_back(
            {.view = rows->AddChildView(
                 MakeSplit(item, *right, model.selected_item_id)),
             .kind = foundation::DropTargetKind::Today,
             .item = item.id,
             .next = after != model.temporary.end() ? after->id : 0,
             .split = true});
        continue;
      }
    }
    drop_zones_.push_back({.view = rows->AddChildView(MakeItem(
                               item, false, item.id == model.selected_item_id)),
                           .kind = foundation::DropTargetKind::Today,
                           .item = item.id,
                           .next = i + 1 < model.temporary.size()
                                       ? model.temporary[i + 1].id
                                       : 0});
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
  if (menu_folder_id_ && menu_runner_ && menu_runner_->IsRunning() &&
      (menu_space_id_ != current_space_id_ ||
       !std::ranges::any_of(folders_, [this](const FolderEntry& folder) {
         return folder.id == menu_folder_id_;
       }))) {
    menu_folder_id_ = 0;
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
  if (!capture_item_menu_scheduled_ && !tiles_.empty() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(kCaptureItemMenu)) {
    capture_item_menu_scheduled_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SidebarView::OpenCaptureItemMenu,
                       weak_factory_.GetWeakPtr(), 30),
        base::Milliseconds(2000));
  }
  if (!capture_drag_scheduled_ &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(kCaptureDrag)) {
    capture_drag_scheduled_ = true;
    const std::vector<std::string> parts = base::SplitString(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kCaptureDrag),
        ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    uint64_t item = 0;
    uint64_t folder = 0;
    uint64_t row = 0;
    // "r<item ID>" drops above that row instead of on a folder.
    const bool to_row = parts.size() >= 2 && parts[1].starts_with('r') &&
                        base::StringToUint64(parts[1].substr(1), &row);
    if (parts.size() >= 2 && base::StringToUint64(parts[0], &item) &&
        (to_row || base::StringToUint64(parts[1], &folder))) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&SidebarView::RunCaptureDrag,
                         weak_factory_.GetWeakPtr(), item, folder, row,
                         parts.size() > 2 && parts[2] == "hold"),
          base::Milliseconds(2000));
    }
  }
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
  footer_control_client_ = control_client_;
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
  auto* spaces = spaces_scroll_->SetContents(std::make_unique<SpaceStrip>());
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
  const size_t keyboard_spaces =
      std::ranges::count(spaces_, false, &SpaceEntry::agent);
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
    const bool controlled = agent && !control_client_.empty();
    ui::ImageModel glyph =
        SpaceGlyph(position == 0 && !agent, spaces_[index].icon, current,
                   GetFills(GetColorProvider()), agent);
    choice->SetImageModel(views::Button::STATE_NORMAL,
                          controlled ? ActiveGlyph(glyph, GetColorProvider())
                                     : glyph);
    // Arc spaces its Space dots 34 DIP apart.
    choice->SetMinSize(gfx::Size(34, 32));
    choice->SetMaxSize(gfx::Size(34, 32));
    choice->SetPreferredSize(gfx::Size(34, 32));
    if (current) {
      current_space_index = spaces->children().size();
    }
    // Ctrl-1 to Ctrl-8 select those Spaces and Ctrl-9 the last one, in the
    // footer's order. The agent Space takes no number.
    FooterTooltip::Content tooltip = {
        controlled ? control_client_ + u" is controlling the Agent Space"
        : agent    ? u"Agent Space"
                   : space_name,
        {}};
    if (!agent && position < 8) {
      tooltip.keys = {u"\u2303", base::NumberToString16(position + 1)};
    } else if (!agent && position + 1 == keyboard_spaces) {
      tooltip.keys = {u"\u2303", u"9"};
    }
    choice->SetFooterTooltip(footer_tooltip_->GetWeakPtr(), std::move(tooltip));
    choice->GetViewAccessibility().SetName(
        (agent ? u"Agent Space: " : u"Space: ") + space_name +
        (controlled ? u", " + control_client_ + u" is controlling it" : u""));
    if (controlled) {
      // Right-click offers Stop; a click still selects the Space.
      choice->set_context_menu_controller(this);
      choice->GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
    }
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
      ui::ImageModel glyph =
          SpaceGlyph(space == first, space->icon,
                     target.id == footer_space_id_, fills, space->agent);
      if (space->agent && !footer_control_client_.empty()) {
        glyph = ActiveGlyph(glyph, GetColorProvider());
      }
      static_cast<views::MdTextButton*>(target.view.get())
          ->SetImageModel(views::Button::STATE_NORMAL, glyph);
    }
  }
  SetCreateMenuOpen(create_menu_open_);
}

BEGIN_METADATA(SidebarView)
END_METADATA

}  // namespace myarc
