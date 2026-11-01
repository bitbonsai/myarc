#include "myarc/create_menu.h"

#include <array>
#include <iterator>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "myarc/icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_targeter.h"

namespace myarc {
namespace {

// Measured from Arc's menu (DIP): a 206 wide panel with 11 DIP corners, 7 DIP
// above the first row and below the last, 40 DIP rows and a 17 DIP band for
// the separator, inset 17 DIP. The highlight pill is inset 7 DIP with 7.5 DIP
// corners. Icons center 26 DIP from the panel's left, labels start at 49 and
// shortcut glyphs end 17 DIP from its right, about 4 DIP apart. The notch, 8
// DIP wide and 5 tall, points at the Create button.
constexpr int kPanelWidth = 206;
constexpr float kPanelRadius = 11;
constexpr int kPad = 7;
constexpr int kRowHeight = 40;
constexpr int kSeparatorBand = 17;
constexpr int kPanelHeight = 2 * kPad + 3 * kRowHeight + kSeparatorBand;
constexpr int kSeparatorInset = 17;
constexpr int kPillInset = 7;
constexpr float kPillRadius = 7.5f;
constexpr int kIconCenter = 26;
constexpr int kLabelX = 49;
constexpr int kShortcutRight = 17;
constexpr int kGlyphGap = 4;
constexpr int kLabelSize = 13;
constexpr int kShortcutSize = 13;
constexpr int kNotchHalfWidth = 4;
constexpr int kNotchHeight = 5;
// Room around the panel for its soft shadow (black at 30%, blur 16, 4 down).
constexpr gfx::Insets kShadowMargin = gfx::Insets::TLBR(12, 16, 20, 16);

// Arc's menu pops in over 2 frames and leaves over 3, or 2 after a click
// elsewhere, scaling from and toward the notch.
constexpr base::TimeDelta kOpen = base::Milliseconds(65);
constexpr base::TimeDelta kClose = base::Milliseconds(100);
constexpr base::TimeDelta kFastClose = base::Milliseconds(65);
constexpr base::TimeDelta kDefaultHighlight = base::Milliseconds(100);
constexpr float kOpenScale = 0.97f;
constexpr float kCloseScale = 0.94f;

struct MenuColors {
  SkColor panel;
  SkColor border;
  SkColor separator;
  SkColor highlight;
  SkColor label;
  SkColor disabled;
  SkColor shortcut;
  SkColor highlighted_shortcut;
};

// Sampled from Arc's dark menu. Arc has no light menu, so light mode uses a
// white panel with the light footer tooltip's hairline, black ink at Arc's
// relative strengths for text, and the light keycap indigo for the highlight.
constexpr MenuColors kDarkColors = {
    SkColorSetRGB(0x03, 0x04, 0x24), SK_ColorTRANSPARENT,
    SkColorSetRGB(0x1F, 0x1D, 0x58), SkColorSetRGB(0x2E, 0x2C, 0x8D),
    SkColorSetRGB(0xFB, 0xFC, 0xFF), SkColorSetRGB(0x75, 0x77, 0x8B),
    SkColorSetRGB(0x6F, 0x71, 0x89), SkColorSetRGB(0x71, 0x73, 0xA0)};
constexpr MenuColors kLightColors = {SK_ColorWHITE,
                                     SkColorSetA(SK_ColorBLACK, 0x1F),
                                     SkColorSetA(SK_ColorBLACK, 0x14),
                                     SkColorSetRGB(0xE4, 0xE5, 0xFE),
                                     SkColorSetRGB(0x1D, 0x1B, 0x26),
                                     SkColorSetA(SK_ColorBLACK, 0x59),
                                     SkColorSetA(SK_ColorBLACK, 0x73),
                                     SkColorSetA(SK_ColorBLACK, 0x8C)};

const MenuColors& GetColors(const ui::ColorProvider* provider) {
  const bool dark = !provider || color_utils::IsDark(
                                     provider->GetColor(ui::kColorSysSurface));
  return dark ? kDarkColors : kLightColors;
}

struct RowSpec {
  CreateMenu::Item item;
  const char16_t* label;
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
  int icon_size;
  // Plain glyphs, right-aligned, with their ARIA form.
  std::array<const char16_t*, 3> keys;
  const char* key_shortcuts;
  bool enabled;
  const char16_t* description;
  int y;
};

// myArc's own shortcuts: Cmd+T opens the New Tab palette and Chromium binds
// Option+Cmd+N to split view. New Split stays disabled because myArc's
// workspace has no split model yet (docs/GAPS.md, item 10): a split would not
// persist or show as one sidebar row.
constexpr auto kRows = std::to_array<RowSpec>({
    {CreateMenu::Item::kNewSpace, u"New Space", &kNewSpaceIcon, 24, {},
     nullptr, true, nullptr, kPad},
    {CreateMenu::Item::kNewSplit,
     u"New Split",
     &kNewSplitIcon,
     18,
     {u"⌥", u"⌘", u"N"},
     "Alt+Meta+N",
     false,
     u"Split view isn't available in myArc yet",
     kPad + kRowHeight + kSeparatorBand},
    {CreateMenu::Item::kNewTab,
     u"New Tab",
     &kNewTabIcon,
     18,
     {u"⌘", u"T"},
     "Meta+T",
     true,
     nullptr,
     kPad + 2 * kRowHeight + kSeparatorBand},
});

gfx::FontList FontOfSize(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.Derive(size - base.GetFontSize(), gfx::Font::NORMAL, weight);
}

}  // namespace

// One row: its highlight pill, icon, label and shortcut glyphs. The menu
// handles its events; the row carries its accessible menu item.
class CreateMenuRow : public views::View {
  METADATA_HEADER(CreateMenuRow, views::View)

 public:
  CreateMenuRow(const RowSpec& spec, base::RepeatingClosure choose)
      : spec_(spec),
        choose_(std::move(choose)),
        label_font_(FontOfSize(kLabelSize, gfx::Font::Weight::SEMIBOLD)),
        shortcut_font_(FontOfSize(kShortcutSize, gfx::Font::Weight::NORMAL)) {
    SetCanProcessEventsWithinSubtree(false);
    SetEnabled(spec.enabled);
    GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
    GetViewAccessibility().SetName(spec.label);
    if (spec.key_shortcuts) {
      GetViewAccessibility().SetKeyShortcuts(spec.key_shortcuts);
    }
    if (spec.description) {
      GetViewAccessibility().SetDescription(spec.description);
    }
  }

  void SetHighlighted(bool highlighted) {
    if (highlighted == highlighted_) {
      return;
    }
    highlighted_ = highlighted;
    GetViewAccessibility().SetIsSelected(highlighted);
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const MenuColors& colors = GetColors(GetColorProvider());
    if (highlighted_) {
      gfx::RectF pill(GetLocalBounds());
      pill.Inset(gfx::InsetsF::VH(0.1f, kPillInset));
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(colors.highlight);
      canvas->DrawRoundRect(pill, kPillRadius, flags);
    }
    const SkColor ink = GetEnabled() ? colors.label : colors.disabled;
    {
      gfx::ScopedCanvas scoped(canvas);
      canvas->Translate(gfx::Vector2d(kIconCenter - spec_.icon_size / 2,
                                      (height() - spec_.icon_size) / 2));
      gfx::PaintVectorIcon(canvas, *spec_.icon, spec_.icon_size, ink);
    }
    constexpr int kTextFlags = gfx::Canvas::NO_SUBPIXEL_RENDERING;
    const int label_width = gfx::GetStringWidth(spec_.label, label_font_);
    canvas->DrawStringRectWithFlags(spec_.label, label_font_, ink,
                                    gfx::Rect(kLabelX, 0, label_width, height()),
                                    kTextFlags);
    const SkColor shortcut = !GetEnabled()  ? colors.disabled
                             : highlighted_ ? colors.highlighted_shortcut
                                            : colors.shortcut;
    int right = width() - kShortcutRight;
    for (auto key = spec_.keys.rbegin(); key != spec_.keys.rend(); ++key) {
      if (!*key) {
        continue;
      }
      const int glyph_width = gfx::GetStringWidth(*key, shortcut_font_);
      right -= glyph_width;
      canvas->DrawStringRectWithFlags(
          *key, shortcut_font_, shortcut,
          gfx::Rect(right, 0, glyph_width, height()), kTextFlags);
      right -= kGlyphGap;
    }
  }

  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    if (action_data.action != ax::mojom::Action::kDoDefault || !GetEnabled()) {
      return views::View::HandleAccessibleAction(action_data);
    }
    choose_.Run();
    return true;
  }

 private:
  const RowSpec spec_;
  const base::RepeatingClosure choose_;
  const gfx::FontList label_font_;
  const gfx::FontList shortcut_font_;
  bool highlighted_ = false;
};

BEGIN_METADATA(CreateMenuRow)
END_METADATA

CreateMenu::CreateMenu(base::RepeatingCallback<void(Item)> select,
                       base::RepeatingCallback<void(bool)> open_changed)
    : views::AnimationDelegateViews(this),
      select_(std::move(select)),
      open_changed_(std::move(open_changed)) {
  SetVisible(false);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
  GetViewAccessibility().SetName(u"Create");
  for (size_t i = 0; i < kRows.size(); ++i) {
    auto* row = AddChildView(std::make_unique<CreateMenuRow>(
        kRows[i], base::BindRepeating(&CreateMenu::Choose,
                                      base::Unretained(this), i)));
    rows_.push_back(row);
  }
}

CreateMenu::~CreateMenu() = default;

void CreateMenu::Place(int panel_right, int panel_bottom, int notch_x) {
  const int left = panel_right - kPanelWidth - kShadowMargin.left();
  const int top = panel_bottom - kPanelHeight - kShadowMargin.top();
  SetBounds(left, top, kPanelWidth + kShadowMargin.width(),
            kPanelHeight + kShadowMargin.height());
  notch_x_ = notch_x - left;
  for (size_t i = 0; i < rows_.size(); ++i) {
    rows_[i]->SetBounds(kShadowMargin.left(), kShadowMargin.top() + kRows[i].y,
                        kPanelWidth, kRowHeight);
  }
  // The transform's origin follows the notch.
  Apply(opacity_, scale_);
}

gfx::Rect CreateMenu::PanelBounds() const {
  return gfx::Rect(kShadowMargin.left(), kShadowMargin.top(), kPanelWidth,
                   kPanelHeight);
}

SkPath CreateMenu::PanelPath() const {
  const gfx::RectF panel(PanelBounds());
  const SkPath rounded =
      SkPath::RRect(gfx::RectFToSkRect(panel), kPanelRadius, kPanelRadius);
  // The notch overlaps the panel by 1 DIP so the union has no seam.
  const float x = notch_x_;
  const SkPoint notch_points[] = {{x - kNotchHalfWidth, panel.bottom() - 1},
                                  {x, panel.bottom() + kNotchHeight},
                                  {x + kNotchHalfWidth, panel.bottom() - 1}};
  const SkPath notch = SkPath::Polygon(notch_points, /*isClosed=*/true);
  return Op(rounded, notch, kUnion_SkPathOp).value_or(rounded);
}

void CreateMenu::Open(views::View& anchor, bool hold) {
  views::Widget* widget = GetWidget();
  if (open_ || !widget) {
    return;
  }
  open_ = true;
  hold_ = hold;
  anchor_.SetView(&anchor);
  SetHighlighted(std::nullopt);
  pressed_.reset();
  SetVisible(true);
  SetCanProcessEventsWithinSubtree(true);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusManager* focus = GetFocusManager();
  previous_focus_.SetView(focus ? focus->GetFocusedView() : nullptr);
  RequestFocus();
  press_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, widget->GetNativeWindow(), {ui::EventType::kMousePressed});
  widget_observation_.Observe(widget);
  open_changed_.Run(true);
  animation_.Stop();
  if (hold) {
    SetHighlighted(rows_.size() - 1);
  } else {
    default_highlight_.Start(
        FROM_HERE, kDefaultHighlight,
        base::BindOnce(
            [](CreateMenu* menu) {
              if (!menu->highlighted_) {
                menu->SetHighlighted(menu->rows_.size() - 1);
              }
            },
            base::Unretained(this)));
  }
  if (hold || gfx::Animation::PrefersReducedMotion()) {
    Apply(1, 1);
    return;
  }
  from_opacity_ = 0;
  from_scale_ = kOpenScale;
  Apply(from_opacity_, from_scale_);
  animation_.SetDuration(kOpen);
  animation_.Start();
}

void CreateMenu::Close(bool fast) {
  if (!open_) {
    return;
  }
  open_ = false;
  hold_ = false;
  default_highlight_.Stop();
  press_monitor_.reset();
  widget_observation_.Reset();
  anchor_.SetView(nullptr);
  pressed_.reset();
  SetCanProcessEventsWithinSubtree(false);
  if (HasFocus()) {
    views::View* previous = previous_focus_.view();
    if (previous && previous != this && previous->GetWidget() == GetWidget() &&
        previous->IsFocusable()) {
      previous->RequestFocus();
    } else if (GetFocusManager()) {
      GetFocusManager()->ClearFocus();
    }
  }
  previous_focus_.SetView(nullptr);
  SetFocusBehavior(FocusBehavior::NEVER);
  open_changed_.Run(false);
  animation_.Stop();
  if (gfx::Animation::PrefersReducedMotion() || !IsDrawn()) {
    Apply(0, 1);
    SetVisible(false);
    return;
  }
  from_opacity_ = opacity_;
  from_scale_ = scale_;
  animation_.SetDuration(fast ? kFastClose : kClose);
  animation_.Start();
}

void CreateMenu::Choose(size_t row) {
  if (!open_ || row >= rows_.size() || !kRows[row].enabled) {
    return;
  }
  const Item item = kRows[row].item;
  Close();
  select_.Run(item);
}

void CreateMenu::SetHighlighted(std::optional<size_t> row) {
  if (row && !kRows[*row].enabled) {
    row.reset();
  }
  highlighted_ = row;
  for (size_t i = 0; i < rows_.size(); ++i) {
    rows_[i]->SetHighlighted(row == i);
  }
  if (row) {
    GetViewAccessibility().SetActiveDescendant(*rows_[*row]);
  } else {
    GetViewAccessibility().ClearActiveDescendant();
  }
}

void CreateMenu::MoveHighlight(int step) {
  const int count = static_cast<int>(rows_.size());
  int row = highlighted_ ? static_cast<int>(*highlighted_)
                         : (step > 0 ? -1 : count);
  for (int tries = 0; tries < count; ++tries) {
    row = (row + step + count) % count;
    if (kRows[row].enabled) {
      default_highlight_.Stop();
      SetHighlighted(row);
      return;
    }
  }
}

std::optional<size_t> CreateMenu::RowAt(const gfx::Point& point) const {
  for (size_t i = 0; i < rows_.size(); ++i) {
    if (rows_[i]->bounds().Contains(point)) {
      return i;
    }
  }
  return std::nullopt;
}

void CreateMenu::UpdateHover(const gfx::Point& point) {
  // Over a row the highlight snaps to it, or goes away on a disabled one, as
  // in Arc. Padding and the separator leave it where it is.
  const std::optional<size_t> row = RowAt(point);
  if (!row) {
    return;
  }
  default_highlight_.Stop();
  SetHighlighted(row);
}

void CreateMenu::Apply(float opacity, float scale) {
  opacity_ = opacity;
  scale_ = scale;
  layer()->SetOpacity(opacity);
  const gfx::PointF origin(notch_x_,
                           PanelBounds().bottom() + kNotchHeight);
  gfx::Transform transform;
  transform.Translate(origin.x(), origin.y());
  transform.Scale(scale, scale);
  transform.Translate(-origin.x(), -origin.y());
  layer()->SetTransform(transform);
}

void CreateMenu::OnPaint(gfx::Canvas* canvas) {
  const MenuColors& colors = GetColors(GetColorProvider());
  const SkPath path = PanelPath();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(colors.panel);
  flags.setLooper(gfx::CreateShadowDrawLooper({gfx::ShadowValue(
      gfx::Vector2d(0, 4), 16, SkColorSetA(SK_ColorBLACK, 0x4D))}));
  canvas->DrawPath(path, flags);
  if (SkColorGetA(colors.border)) {
    cc::PaintFlags border;
    border.setAntiAlias(true);
    border.setStyle(cc::PaintFlags::kStroke_Style);
    border.setStrokeWidth(1);
    border.setColor(colors.border);
    canvas->DrawPath(path, border);
  }
  const gfx::Rect panel = PanelBounds();
  const int separator_y = panel.y() + kPad + kRowHeight + kSeparatorBand / 2;
  canvas->FillRect(gfx::Rect(panel.x() + kSeparatorInset, separator_y,
                             panel.width() - 2 * kSeparatorInset, 1),
                   colors.separator);
}

void CreateMenu::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

bool CreateMenu::OnMousePressed(const ui::MouseEvent& event) {
  pressed_ = RowAt(event.location());
  return true;
}

void CreateMenu::OnMouseReleased(const ui::MouseEvent& event) {
  const std::optional<size_t> row = RowAt(event.location());
  if (row && row == std::exchange(pressed_, std::nullopt)) {
    Choose(*row);
  }
}

void CreateMenu::OnMouseMoved(const ui::MouseEvent& event) {
  UpdateHover(event.location());
}

void CreateMenu::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHover(event.location());
}

bool CreateMenu::OnMouseDragged(const ui::MouseEvent& event) {
  UpdateHover(event.location());
  return true;
}

bool CreateMenu::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  switch (event.key_code()) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
    case ui::VKEY_RETURN:
    case ui::VKEY_SPACE:
    case ui::VKEY_ESCAPE:
      return open_;
    default:
      return false;
  }
}

bool CreateMenu::OnKeyPressed(const ui::KeyEvent& event) {
  if (!open_) {
    return false;
  }
  switch (event.key_code()) {
    case ui::VKEY_UP:
      MoveHighlight(-1);
      return true;
    case ui::VKEY_DOWN:
      MoveHighlight(1);
      return true;
    case ui::VKEY_RETURN:
    case ui::VKEY_SPACE:
      if (highlighted_) {
        Choose(*highlighted_);
      }
      return true;
    case ui::VKEY_ESCAPE:
      Close();
      return true;
    default:
      return false;
  }
}

void CreateMenu::OnBlur() {
  views::View::OnBlur();
  // Tab or a click into the page moved focus away.
  if (!hold_) {
    Close(/*fast=*/true);
  }
}

bool CreateMenu::DoesIntersectRect(const views::View* target,
                                   const gfx::Rect& rect) const {
  gfx::Rect panel = PanelBounds();
  panel.Inset(gfx::Insets::TLBR(0, 0, -kNotchHeight, 0));
  return panel.Intersects(rect);
}

void CreateMenu::AnimationProgressed(const gfx::Animation* animation) {
  const double t = animation_.GetCurrentValue();
  if (open_) {
    const double eased = gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, t);
    Apply(gfx::Tween::FloatValueBetween(eased, from_opacity_, 1),
          gfx::Tween::FloatValueBetween(eased, from_scale_, 1));
  } else {
    const double eased = gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, t);
    Apply(gfx::Tween::FloatValueBetween(eased, from_opacity_, 0),
          gfx::Tween::FloatValueBetween(eased, from_scale_, kCloseScale));
  }
}

void CreateMenu::AnimationEnded(const gfx::Animation* animation) {
  if (open_) {
    Apply(1, 1);
    return;
  }
  Apply(0, 1);
  SetVisible(false);
}

void CreateMenu::OnWidgetActivationChanged(views::Widget* widget,
                                           bool active) {
  if (!active && !hold_) {
    Close(/*fast=*/true);
  }
}

void CreateMenu::OnWidgetDestroying(views::Widget* widget) {
  press_monitor_.reset();
  widget_observation_.Reset();
}

void CreateMenu::OnEvent(const ui::Event& event) {
  if (!open_ || hold_) {
    return;
  }
  // Presses on the panel choose; one on the Create button toggles it.
  const gfx::Point point = display::Screen::Get()->GetCursorScreenPoint();
  gfx::Rect panel = PanelBounds();
  panel.Inset(gfx::Insets::TLBR(0, 0, -kNotchHeight, 0));
  views::View::ConvertRectToScreen(this, &panel);
  if (panel.Contains(point) ||
      (anchor_.view() && anchor_.view()->GetBoundsInScreen().Contains(point))) {
    return;
  }
  Close(/*fast=*/true);
}

BEGIN_METADATA(CreateMenu)
END_METADATA

}  // namespace myarc
