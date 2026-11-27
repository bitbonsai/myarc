#include "myarc/little_arc.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "myarc/bridge.rs.h"
#include "myarc/browser_workspace_adapter.h"
#include "myarc/icons.h"
#include "myarc/little_arc_prefs.h"
#include "myarc/space_icon_catalog.h"
#include "myarc/spaces_index.h"
#include "myarc/test_buildflags.h"
#include "myarc/workspace_lifecycle.h"
#include "myarc/workspace_service.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/base/base_window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"

namespace myarc {

namespace {

// Arc's Little Arc is a small window. No capture of a real one exists yet;
// this size fits Arc's Settings illustration of it on a 14-inch screen.
constexpr int kWindowWidth = 760;
constexpr int kWindowHeight = 600;
// Each further Little Arc opens this far down and right of the last.
constexpr int kCascade = 22;

constexpr int kControlHeight = 28;
constexpr int kControlRadius = 10;
constexpr int kChipHeight = 20;
constexpr int kGap = 8;

struct Palette {
  SkColor frame;
  SkColor pill;
  SkColor chip;
  SkColor button;
  SkColor stroke;
  SkColor hover;
  SkColor ink;
  SkColor muted;
  SkColor accent;
};

constexpr Palette kLight = {
    .frame = SkColorSetRGB(0xe4, 0xe4, 0xeb),
    .pill = SkColorSetRGB(0xd6, 0xd6, 0xdf),
    .chip = SkColorSetRGB(0xf6, 0xf6, 0xf8),
    .button = SkColorSetRGB(0xf6, 0xf6, 0xf8),
    .stroke = SkColorSetARGB(0x24, 0x00, 0x00, 0x00),
    .hover = SkColorSetARGB(0x10, 0x00, 0x00, 0x00),
    .ink = SkColorSetRGB(0x3a, 0x3a, 0x44),
    .muted = SkColorSetRGB(0x86, 0x86, 0x92),
    .accent = SkColorSetRGB(0x3b, 0x5f, 0xc8),
};

constexpr Palette kDark = {
    .frame = SkColorSetRGB(0x2a, 0x2a, 0x30),
    .pill = SkColorSetRGB(0x3a, 0x3a, 0x42),
    .chip = SkColorSetRGB(0x4a, 0x4a, 0x53),
    .button = SkColorSetRGB(0x3a, 0x3a, 0x42),
    .stroke = SkColorSetARGB(0x30, 0xff, 0xff, 0xff),
    .hover = SkColorSetARGB(0x14, 0xff, 0xff, 0xff),
    .ink = SkColorSetRGB(0xe8, 0xe8, 0xec),
    .muted = SkColorSetRGB(0x9a, 0x9a, 0xa4),
    .accent = SkColorSetRGB(0x93, 0xab, 0xff),
};

bool IsLittleArcMode() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch("myarc-visual-parity") &&
         command_line.HasSwitch("myarc-workspace-sidebar");
}

gfx::FontList Font(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.Derive(size - base.GetFontSize(), gfx::Font::NORMAL, weight);
}

views::Label* AddLabel(views::View* parent,
                       int size,
                       gfx::Font::Weight weight) {
  auto* label = parent->AddChildView(std::make_unique<views::Label>());
  label->SetFontList(Font(size, weight));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetCanProcessEventsWithinSubtree(false);
  return label;
}

void FillRoundRect(gfx::Canvas* canvas,
                   const gfx::Rect& bounds,
                   int radius,
                   SkColor fill,
                   std::optional<SkColor> stroke = std::nullopt) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(fill);
  canvas->DrawRoundRect(gfx::RectF(bounds), radius, flags);
  if (stroke) {
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(*stroke);
    gfx::RectF edge(bounds);
    edge.Inset(0.5f);
    canvas->DrawRoundRect(edge, radius - 0.5f, flags);
  }
}

// What the header shows about the Space "Open in" moves the page to.
struct TargetSpace {
  bool found = false;
  std::u16string name;
  SpaceIconChoice icon;
};

TargetSpace FindTargetSpace(BrowserWindowInterface& little_arc) {
  TargetSpace target;
  BrowserWindowInterface* window = LittleArcMoveTarget(little_arc);
  auto* adapter =
      window ? BrowserWorkspaceAdapter::ForBrowser(*window) : nullptr;
  if (!adapter) {
    return target;
  }
  const auto sidebar = adapter->Sidebar();
  if (!sidebar.ok) {
    return target;
  }
  target.found = true;
  target.name = base::UTF8ToUTF16(std::string(sidebar.current_space.name));
  switch (sidebar.current_space.icon_kind) {
    case foundation::SpaceIconKind::Emoji:
      target.icon.kind = SpaceIconChoice::Kind::kEmoji;
      break;
    case foundation::SpaceIconKind::Named:
      target.icon.kind = SpaceIconChoice::Kind::kNamed;
      break;
    default:
      break;
  }
  target.icon.value = std::string(sidebar.current_space.icon);
  return target;
}

SpaceIconChoice SpaceIconFromFoundation(foundation::SpaceIconKind kind,
                                        std::string_view value) {
  SpaceIconChoice choice;
  switch (kind) {
    case foundation::SpaceIconKind::Emoji:
      choice.kind = SpaceIconChoice::Kind::kEmoji;
      break;
    case foundation::SpaceIconKind::Named:
      choice.kind = SpaceIconChoice::Kind::kNamed;
      break;
    default:
      return choice;
  }
  choice.value = std::string(value);
  if (!IsBundledSpaceIcon(choice)) {
    return {};
  }
  return choice;
}

SpaceIconChoice SpaceIconFromIndexed(IndexedSpace::IconKind kind,
                                     std::string_view value) {
  SpaceIconChoice choice;
  switch (kind) {
    case IndexedSpace::IconKind::kEmoji:
      choice.kind = SpaceIconChoice::Kind::kEmoji;
      break;
    case IndexedSpace::IconKind::kNamed:
      choice.kind = SpaceIconChoice::Kind::kNamed;
      break;
    default:
      return choice;
  }
  choice.value = std::string(value);
  if (!IsBundledSpaceIcon(choice)) {
    return {};
  }
  return choice;
}

std::u16string ProfileDisplayName(const ProfileAttributesEntry& entry,
                                  const ProfileManager& manager) {
  if (entry.IsUsingDefaultName() &&
      entry.GetPath() ==
          profiles::GetDefaultProfileDir(manager.user_data_dir())) {
    return u"Default";
  }
  return entry.GetLocalProfileName();
}

base::FilePath ResolveProfilePath(const std::string& profile_dir) {
  ProfileManager* manager = g_browser_process->profile_manager();
  if (!manager || !IsIndexableProfileDir(profile_dir)) {
    return {};
  }
  for (const ProfileAttributesEntry* entry :
       manager->GetProfileAttributesStorage()
           .GetAllProfilesAttributesSortedForDisplay()) {
    if (entry->IsOmitted() || entry->IsEphemeral() ||
        IsProfileDirectoryMarkedForDeletion(entry->GetPath())) {
      continue;
    }
    if (entry->GetPath().BaseName().AsUTF8Unsafe() == profile_dir) {
      return entry->GetPath();
    }
  }
  return {};
}

struct PickerSpaceEntry {
  uint64_t id = 0;
  std::u16string name;
  SpaceIconChoice icon;
};

struct PickerProfileGroup {
  std::string profile_dir;
  std::u16string profile_name;
  bool same_profile = false;
  std::vector<PickerSpaceEntry> spaces;
};

std::vector<PickerSpaceEntry> SpacesForProfile(
    const std::string& profile_dir,
    const std::vector<IndexedSpace>& indexed,
    Profile* loaded_profile) {
  std::vector<PickerSpaceEntry> spaces;
  for (const IndexedSpace& space : indexed) {
    if (space.profile_dir != profile_dir) {
      continue;
    }
    spaces.push_back({.id = space.space_id,
                      .name = base::UTF8ToUTF16(space.name),
                      .icon = SpaceIconFromIndexed(space.icon_kind,
                                                   space.icon)});
  }
  if (!spaces.empty() || !loaded_profile) {
    return spaces;
  }
  const auto snapshot =
      WorkspaceService::ForProfile(*loaded_profile).ExportSnapshot();
  if (!snapshot) {
    return spaces;
  }
  for (const auto& space : snapshot->spaces) {
    spaces.push_back(
        {.id = space.id,
         .name = base::UTF8ToUTF16(std::string(space.name)),
         .icon = SpaceIconFromFoundation(space.icon_kind,
                                         std::string_view(space.icon.data(),
                                                          space.icon.size()))});
  }
  return spaces;
}

std::vector<PickerProfileGroup> CollectPickerGroups(
    BrowserWindowInterface& little_arc,
    uint64_t* current_space_id) {
  if (current_space_id) {
    *current_space_id = 0;
  }
  Profile* own_profile = little_arc.GetProfile();
  const std::string own_dir = own_profile->GetBaseName().AsUTF8Unsafe();
  std::vector<PickerSpaceEntry> live_own_spaces;
  BrowserWindowInterface* window = LittleArcMoveTarget(little_arc);
  auto* adapter =
      window ? BrowserWorkspaceAdapter::ForBrowser(*window) : nullptr;
  if (adapter) {
    const auto sidebar = adapter->Sidebar();
    if (sidebar.ok) {
      if (current_space_id) {
        *current_space_id = sidebar.current_space.id;
      }
      for (const auto& space : sidebar.spaces) {
        live_own_spaces.push_back(
            {.id = space.id,
             .name = base::UTF8ToUTF16(std::string(space.name)),
             .icon = SpaceIconFromFoundation(
                 space.icon_kind,
                 std::string_view(space.icon.data(), space.icon.size()))});
      }
    }
  }

  ProfileManager* manager = g_browser_process->profile_manager();
  PrefService* local_state = g_browser_process->local_state();
  if (!manager) {
    std::vector<PickerProfileGroup> groups;
    if (!live_own_spaces.empty()) {
      groups.push_back({.profile_dir = own_dir,
                        .profile_name = u"Default",
                        .same_profile = true,
                        .spaces = std::move(live_own_spaces)});
    }
    return groups;
  }

  const std::vector<IndexedSpace> indexed =
      local_state ? ReadSpacesIndex(*local_state) : std::vector<IndexedSpace>{};
  const bool allow_other_profiles =
      !own_profile->IsOffTheRecord() && !own_profile->IsGuestSession();

  std::optional<PickerProfileGroup> own_group;
  std::vector<PickerProfileGroup> other_groups;
  for (const ProfileAttributesEntry* entry :
       manager->GetProfileAttributesStorage()
           .GetAllProfilesAttributesSortedForDisplay()) {
    if (entry->IsOmitted() || entry->IsEphemeral() ||
        IsProfileDirectoryMarkedForDeletion(entry->GetPath())) {
      continue;
    }
    const std::string dir = entry->GetPath().BaseName().AsUTF8Unsafe();
    const bool is_own = (dir == own_dir);
    if (!is_own && !allow_other_profiles) {
      continue;
    }
    std::vector<PickerSpaceEntry> spaces =
        is_own && !live_own_spaces.empty()
            ? live_own_spaces
            : SpacesForProfile(dir, indexed,
                               manager->GetProfileByPath(entry->GetPath()));
    if (spaces.empty()) {
      continue;
    }
    PickerProfileGroup group{
        .profile_dir = dir,
        .profile_name = ProfileDisplayName(*entry, *manager),
        .same_profile = is_own,
        .spaces = std::move(spaces),
    };
    if (is_own) {
      own_group = std::move(group);
    } else {
      other_groups.push_back(std::move(group));
    }
  }

  std::vector<PickerProfileGroup> result;
  if (own_group) {
    result.push_back(std::move(*own_group));
  } else if (!live_own_spaces.empty()) {
    result.push_back({.profile_dir = own_dir,
                      .profile_name = u"Default",
                      .same_profile = true,
                      .spaces = std::move(live_own_spaces)});
  }
  for (auto& group : other_groups) {
    result.push_back(std::move(group));
  }
  return result;
}

bool HasPickerOptions(BrowserWindowInterface& little_arc,
                      bool current_space_found) {
  const auto groups = CollectPickerGroups(little_arc, nullptr);
  if (groups.size() > 1) {
    return true;
  }
  size_t total_spaces = 0;
  for (const auto& group : groups) {
    total_spaces += group.spaces.size();
  }
  return total_spaces > 1 || (!current_space_found && total_spaces > 0);
}

gfx::Rect InitialBounds(Profile& profile) {
  int open = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        open += IsLittleArc(*browser) && browser->GetProfile() == &profile;
        return true;
      });
  const gfx::Rect area =
      display::Screen::Get()->GetDisplayForNewWindows().work_area();
  const gfx::Size size(std::min(kWindowWidth, area.width()),
                       std::min(kWindowHeight, area.height()));
  const int step = kCascade * (open % 8);
  gfx::Rect bounds(area.x() + (area.width() - size.width()) / 2 + step,
                   area.y() + (area.height() - size.height()) / 4 + step,
                   size.width(), size.height());
  bounds.AdjustToFit(area);
  return bounds;
}

}  // namespace

// The address pill: the target Space's chip at its left and the page's
// security sign and host centred. A press opens the command bar on the
// address.
class LittleArcAddressPill final : public views::Button {
  METADATA_HEADER(LittleArcAddressPill, views::Button)

 public:
  explicit LittleArcAddressPill(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetAccessibleName(u"Address");
    SetTooltipText(u"Edit the address (⌘L)");
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    chip_icon_ = AddChildView(std::make_unique<views::ImageView>());
    chip_icon_->SetCanProcessEventsWithinSubtree(false);
    chip_name_ = AddLabel(this, 11, gfx::Font::Weight::MEDIUM);
    security_ = AddChildView(std::make_unique<views::ImageView>());
    security_->SetCanProcessEventsWithinSubtree(false);
    host_ = AddLabel(this, 13, gfx::Font::Weight::NORMAL);
    host_->SetElideBehavior(gfx::ELIDE_HEAD);
  }

  void Set(const Palette& palette,
           const TargetSpace& space,
           const std::u16string& host,
           bool blank,
           const gfx::VectorIcon* security_icon,
           const std::u16string& security_name) {
    palette_ = palette;
    chip_shown_ = space.found;
    chip_icon_->SetVisible(space.found);
    chip_name_->SetVisible(space.found);
    if (space.found) {
      ui::ImageModel icon = SpaceIconImage(space.icon, palette.ink, 12);
      chip_icon_->SetImage(icon.IsEmpty() ? ui::ImageModel::FromVectorIcon(
                                                kSmileIcon, palette.ink, 12)
                                          : icon);
      chip_name_->SetText(space.name);
      chip_name_->SetEnabledColor(palette.ink);
    }
    host_->SetText(blank ? u"Search or Enter URL…" : host);
    host_->SetEnabledColor(blank ? palette.muted : palette.ink);
    security_->SetVisible(security_icon != nullptr);
    if (security_icon) {
      security_->SetImage(
          ui::ImageModel::FromVectorIcon(*security_icon, palette.muted, 11));
      security_->SetTooltipText(security_name);
    }
    GetViewAccessibility().SetDescription(blank ? std::u16string() : host);
    InvalidateLayout();
    SchedulePaint();
  }

  void Layout(PassKey) override {
    const int chip_y = (height() - kChipHeight) / 2;
    chip_bounds_ = gfx::Rect();
    if (chip_shown_) {
      const int name_width = std::min(chip_name_->GetPreferredSize().width(),
                                      std::max(0, width() / 4));
      chip_bounds_ =
          gfx::Rect(4, chip_y, 6 + 12 + 4 + name_width + 7, kChipHeight);
      chip_icon_->SetBounds(chip_bounds_.x() + 6,
                            chip_y + (kChipHeight - 12) / 2, 12, 12);
      chip_name_->SetBounds(chip_icon_->bounds().right() + 4, chip_y,
                            name_width, kChipHeight);
    }
    // The host centres on the whole pill, as in Arc, unless the chip is in
    // its way.
    const int security_width = security_->GetVisible() ? 11 + 5 : 0;
    const int left = chip_bounds_.IsEmpty() ? 10 : chip_bounds_.right() + 8;
    const int room = std::max(0, width() - left - 10);
    const int host_width =
        std::min(host_->GetPreferredSize().width(), room - security_width);
    const int content = security_width + std::max(0, host_width);
    const int x =
        std::clamp((width() - content) / 2, left, left + room - content);
    if (security_->GetVisible()) {
      security_->SetBounds(x, (height() - 11) / 2, 11, 11);
    }
    host_->SetBounds(x + security_width, 0, std::max(0, host_width), height());
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    FillRoundRect(canvas, GetLocalBounds(), kControlRadius, palette_.pill);
    if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED) {
      FillRoundRect(canvas, GetLocalBounds(), kControlRadius, palette_.hover);
    }
    if (!chip_bounds_.IsEmpty()) {
      FillRoundRect(canvas, chip_bounds_, 6, palette_.chip);
    }
  }

 private:
  Palette palette_ = kLight;
  bool chip_shown_ = false;
  gfx::Rect chip_bounds_;
  raw_ptr<views::ImageView> chip_icon_ = nullptr;
  raw_ptr<views::Label> chip_name_ = nullptr;
  raw_ptr<views::ImageView> security_ = nullptr;
  raw_ptr<views::Label> host_ = nullptr;
};

BEGIN_METADATA(LittleArcAddressPill)
END_METADATA

namespace {

constexpr int kOpenInMenuWidth = 24;

class LittleArcOpenInPrimaryButton final : public views::Button {
  METADATA_HEADER(LittleArcOpenInPrimaryButton, views::Button)

 public:
  explicit LittleArcOpenInPrimaryButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    prefix_ = AddLabel(this, 12, gfx::Font::Weight::NORMAL);
    prefix_->SetText(u"Open in");
    name_ = AddLabel(this, 12, gfx::Font::Weight::MEDIUM);
    key_ = AddLabel(this, 10, gfx::Font::Weight::MEDIUM);
    key_->SetText(u"⌘O");
    key_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }

  void Set(const Palette& palette,
           const TargetSpace& space,
           bool has_menu_segment) {
    palette_ = palette;
    has_menu_segment_ = has_menu_segment;
    SetEnabled(space.found);
    name_->SetText(space.found ? space.name : u"Space");
    const SkColor ink = space.found ? palette.ink : palette.muted;
    prefix_->SetEnabledColor(ink);
    name_->SetEnabledColor(space.found ? palette.accent : palette.muted);
    key_->SetEnabledColor(palette.muted);
    const std::u16string action =
        space.found ? u"Open in " + space.name
                    : u"Open a myArc window to move this page into a Space";
    SetAccessibleName(space.found ? action : u"Open in Space");
    SetTooltipText(space.found ? action + u" (⌘O)" : action);
    PreferredSizeChanged();
    SchedulePaint();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(10 + prefix_->GetPreferredSize().width() + 4 +
                         std::min(name_->GetPreferredSize().width(), 160) + 6 +
                         KeyWidth() + 6,
                     kControlHeight);
  }

  void Layout(PassKey) override {
    int x = 10;
    const int prefix_width = prefix_->GetPreferredSize().width();
    prefix_->SetBounds(x, 0, prefix_width, height());
    x += prefix_width + 4;
    const int name_width =
        std::max(0, std::min(name_->GetPreferredSize().width(),
                             width() - x - 6 - KeyWidth() - 6));
    name_->SetBounds(x, 0, name_width, height());
    key_bounds_ = gfx::Rect(width() - 6 - KeyWidth(), (height() - 18) / 2,
                            KeyWidth(), 18);
    key_->SetBoundsRect(key_bounds_);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (GetEnabled() &&
        (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED)) {
      canvas->Save();
      const int right_inset = has_menu_segment_ ? 0 : 1;
      canvas->ClipRect(
          gfx::Rect(1, 1, std::max(0, width() - 1 - right_inset),
                    std::max(0, height() - 2)));
      const int pill_w =
          has_menu_segment_ ? width() + 1 + kOpenInMenuWidth : width();
      FillRoundRect(canvas, gfx::Rect(1, 1, pill_w - 2, height() - 2),
                    kControlRadius - 1, palette_.hover);
      canvas->Restore();
    }
    FillRoundRect(canvas, key_bounds_, 5, palette_.hover);
  }

 private:
  int KeyWidth() const { return key_->GetPreferredSize().width() + 8; }

  Palette palette_ = kLight;
  bool has_menu_segment_ = false;
  gfx::Rect key_bounds_;
  raw_ptr<views::Label> prefix_ = nullptr;
  raw_ptr<views::Label> name_ = nullptr;
  raw_ptr<views::Label> key_ = nullptr;
};

BEGIN_METADATA(LittleArcOpenInPrimaryButton)
END_METADATA

class LittleArcOpenInMenuButton final : public views::Button {
  METADATA_HEADER(LittleArcOpenInMenuButton, views::Button)

 public:
  explicit LittleArcOpenInMenuButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    SetAccessibleName(u"Choose Space or Profile");
    SetTooltipText(u"Choose Space or Profile (⌥⌘O)");
    chevron_ = AddChildView(std::make_unique<views::ImageView>());
    chevron_->SetCanProcessEventsWithinSubtree(false);
  }

  void Set(const Palette& palette) {
    palette_ = palette;
    chevron_->SetImage(
        ui::ImageModel::FromVectorIcon(kChevronDownIcon, palette.muted, 10));
    SchedulePaint();
  }

  void Layout(PassKey) override {
    chevron_->SetBounds((width() - 10) / 2, (height() - 10) / 2, 10, 10);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (GetEnabled() &&
        (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED)) {
      canvas->Save();
      canvas->ClipRect(
          gfx::Rect(0, 1, std::max(0, width() - 1), std::max(0, height() - 2)));
      const int total_w = parent() ? parent()->width() : width();
      const int offset_x = parent() ? x() : 0;
      FillRoundRect(canvas,
                    gfx::Rect(1 - offset_x, 1, total_w - 2, height() - 2),
                    kControlRadius - 1, palette_.hover);
      canvas->Restore();
    }
  }

 private:
  Palette palette_ = kLight;
  raw_ptr<views::ImageView> chevron_ = nullptr;
};

BEGIN_METADATA(LittleArcOpenInMenuButton)
END_METADATA

}  // namespace

// Split button: "Open in <Space> ⌘O" moves the page into that Space and closes
// the window; the trailing chevron opens a menu of Spaces grouped by Profile.
class LittleArcOpenInButton final : public views::View {
  METADATA_HEADER(LittleArcOpenInButton, views::View)

 public:
  LittleArcOpenInButton(views::Button::PressedCallback move_callback,
                        views::Button::PressedCallback pick_callback) {
    primary_ = AddChildView(
        std::make_unique<LittleArcOpenInPrimaryButton>(
            std::move(move_callback)));
    menu_ = AddChildView(
        std::make_unique<LittleArcOpenInMenuButton>(std::move(pick_callback)));
  }

  void Set(const Palette& palette,
           const TargetSpace& space,
           bool picker_available) {
    palette_ = palette;
    picker_available_ = picker_available;
    primary_->Set(palette, space, picker_available);
    menu_->SetVisible(picker_available);
    menu_->Set(palette);
    PreferredSizeChanged();
    InvalidateLayout();
    SchedulePaint();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int primary_w = primary_->GetPreferredSize().width();
    const int menu_w = picker_available_ ? 1 + kOpenInMenuWidth : 0;
    return gfx::Size(primary_w + menu_w, kControlHeight);
  }

  void Layout(PassKey) override {
    if (picker_available_) {
      const int primary_w = std::max(0, width() - 1 - kOpenInMenuWidth);
      primary_->SetBounds(0, 0, primary_w, height());
      menu_->SetBounds(primary_w + 1, 0, kOpenInMenuWidth, height());
    } else {
      primary_->SetBounds(0, 0, width(), height());
    }
  }

  void OnPaint(gfx::Canvas* canvas) override {
    FillRoundRect(canvas, GetLocalBounds(), kControlRadius, palette_.button,
                  palette_.stroke);
    if (picker_available_) {
      const int divider_x = primary_->bounds().right();
      cc::PaintFlags flags;
      flags.setColor(palette_.stroke);
      canvas->DrawRect(gfx::RectF(divider_x, 1, 1, height() - 2), flags);
    }
  }

 private:
  Palette palette_ = kLight;
  bool picker_available_ = false;
  raw_ptr<LittleArcOpenInPrimaryButton> primary_ = nullptr;
  raw_ptr<LittleArcOpenInMenuButton> menu_ = nullptr;
};

BEGIN_METADATA(LittleArcOpenInButton)
END_METADATA

bool IsLittleArc(const BrowserWindowInterface& browser) {
  const BrowserInitState* state = BrowserInitState::From(&browser);
  return state && state->create_params().myarc_little_arc;
}

BrowserWindowInterface* OpenLittleArc(Profile& profile, const GURL& url) {
  if (!IsLittleArcMode()) {
    return nullptr;
  }
  // Incognito windows open a Little Arc of their regular Profile; Guest keeps
  // its own.
  Profile& owner =
      profile.IsGuestSession() ? profile : *profile.GetOriginalProfile();
  if (GetBrowserWindowCreationStatusForProfile(owner) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    return nullptr;
  }
  BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_POPUP, owner,
                                   /*from_user_gesture=*/true);
  params.myarc_little_arc = true;
  // Arc archives Little Arcs instead of restoring them; until myArc does, a
  // relaunch leaves them closed.
  params.omit_from_session_restore = true;
  params.initial_bounds = InitialBounds(owner);
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  if (!browser) {
    return nullptr;
  }
  const bool blank = !url.is_valid();
  NavigateParams navigate(
      browser, blank ? GURL(url::kAboutBlankURL) : url,
      blank ? ui::PAGE_TRANSITION_TYPED : ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  // An empty popup takes a new tab, so the page stays in this window.
  navigate.disposition = WindowOpenDisposition::kNewForegroundTab;
  navigate.window_action = NavigateParams::WindowAction::kShowWindow;
  Navigate(&navigate);
  return browser;
}

bool OpenLinkFromOtherApp(Profile& profile, const GURL& url) {
  if (!IsLittleArcMode() || profile.IsOffTheRecord() || !url.is_valid()) {
    return false;
  }
  std::vector<BrowserWindowInterface*> little_arcs;
  rust::Vec<rust::String> shown;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (!IsLittleArc(*browser) || browser->GetProfile() != &profile) {
          return true;
        }
        content::WebContents* contents =
            browser->GetTabStripModel()->GetActiveWebContents();
        little_arcs.push_back(browser);
        shown.push_back(rust::String(contents ? contents->GetVisibleURL().spec()
                                              : std::string()));
        return true;
      });
  const foundation::LittleArcRoute route =
      foundation::route_link_from_other_app(
          IsLittleArcLinksEnabled(g_browser_process->local_state()),
          std::string(url.scheme()), url.spec(), std::move(shown));
  switch (route.kind) {
    case foundation::LittleArcRouteKind::FocusLittleArc:
      if (route.index < little_arcs.size()) {
        little_arcs[route.index]->GetWindow()->Show();
        return true;
      }
      return OpenLittleArc(profile, url) != nullptr;
    case foundation::LittleArcRouteKind::NewLittleArc:
      return OpenLittleArc(profile, url) != nullptr;
    default:
      return false;
  }
}

BrowserWindowInterface* LittleArcMoveTarget(
    BrowserWindowInterface& little_arc) {
  std::vector<BrowserWindowInterface*> windows;
  std::vector<foundation::LittleArcMainWindow> candidates;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
          return true;
        }
        const auto* adapter = BrowserWorkspaceAdapter::ForBrowser(*browser);
        const ui::BaseWindow* window = browser->GetWindow();
        windows.push_back(browser);
        // An ordered-out window of a Profile window group can't show the
        // page; its group's shown window can.
        candidates.push_back(
            {.same_profile = browser->GetProfile() == little_arc.GetProfile(),
             .workspace_ready =
                 adapter && adapter->ready() && window &&
                 (window->IsVisible() || window->IsMinimized())});
        return true;
      });
  const int64_t index = foundation::little_arc_move_target(
      rust::Slice<const foundation::LittleArcMainWindow>(candidates.data(),
                                                         candidates.size()));
  return index >= 0 && static_cast<size_t>(index) < windows.size()
             ? windows[index]
             : nullptr;
}

bool MoveLittleArcToSpace(BrowserWindowInterface& little_arc) {
  return MoveLittleArcToSpace(little_arc, 0);
}

bool MoveLittleArcToSpace(BrowserWindowInterface& little_arc, uint64_t space) {
  if (!IsLittleArc(little_arc)) {
    return false;
  }
  BrowserWindowInterface* target = LittleArcMoveTarget(little_arc);
  auto* adapter =
      target ? BrowserWorkspaceAdapter::ForBrowser(*target) : nullptr;
  TabStripModel* tabs = little_arc.GetTabStripModel();
  const int index = tabs->active_index();
  if (!adapter || index == TabStripModel::kNoTab) {
    return false;
  }
  if (space && space != adapter->Sidebar().current_space.id &&
      !adapter->SelectSpace(space).ok) {
    return false;
  }
  std::unique_ptr<content::WebContents> page =
      tabs->DetachWebContentsAtForInsertion(index);
  if (!page) {
    return false;
  }
  page->SetDelegate(nullptr);
  const foundation::MutationResult result = adapter->AdoptAsTodayTab(page);
  if (!result.ok && page) {
    // The workspace refused a new row. Keep the page as a plain tab of that
    // window rather than lose it; the adapter files it as it does any new
    // tab.
    target->GetTabStripModel()->AppendWebContents(std::move(page),
                                                  /*foreground=*/true);
  }
  target->GetWindow()->Show();
  // The Little Arc has no tabs left and closes itself.
  return true;
}

namespace {

void FinishOpenLittleArcInWindow(
    base::WeakPtr<BrowserWindowInterface> little_arc,
    base::WeakPtr<BrowserWindowInterface> target,
    uint64_t space,
    GURL url,
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive,
    int remaining_retries) {
  if (!target) {
    return;
  }
  auto* adapter = BrowserWorkspaceAdapter::ForBrowser(*target);
  const bool has_url = url.is_valid() && !url.is_empty() && !url.IsAboutBlank();
  if (adapter && adapter->ready()) {
    const auto sidebar = adapter->Sidebar();
    const uint64_t target_space =
        space ? space : (sidebar.ok ? sidebar.current_space.id : 0);
    if (has_url) {
      const foundation::MutationResult result =
          target_space ? adapter->OpenInSpace(url, target_space, nullptr)
                       : adapter->OpenInCurrentSpace(url);
      if (!result.ok) {
        if (target_space && sidebar.ok &&
            target_space != sidebar.current_space.id) {
          adapter->SelectSpace(target_space);
        }
        NavigateParams navigate(target.get(), url, ui::PAGE_TRANSITION_LINK);
        navigate.disposition = WindowOpenDisposition::kNewForegroundTab;
        navigate.window_action = NavigateParams::WindowAction::kShowWindow;
        Navigate(&navigate);
      }
    } else if (target_space && sidebar.ok &&
               target_space != sidebar.current_space.id) {
      adapter->SelectSpace(target_space);
    }
    target->GetWindow()->Show();
    if (little_arc && IsLittleArc(*little_arc)) {
      little_arc->GetWindow()->Close();
    }
    return;
  }
  if (remaining_retries > 0) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FinishOpenLittleArcInWindow, std::move(little_arc),
                       std::move(target), space, std::move(url),
                       std::move(keep_alive), remaining_retries - 1),
        base::Milliseconds(50));
    return;
  }
  if (has_url) {
    NavigateParams navigate(target.get(), url, ui::PAGE_TRANSITION_LINK);
    navigate.disposition = WindowOpenDisposition::kNewForegroundTab;
    navigate.window_action = NavigateParams::WindowAction::kShowWindow;
    Navigate(&navigate);
  } else {
    target->GetWindow()->Show();
  }
  if (little_arc && IsLittleArc(*little_arc)) {
    little_arc->GetWindow()->Close();
  }
}

void OpenLittleArcInLoadedProfile(
    base::WeakPtr<BrowserWindowInterface> little_arc,
    base::WeakPtr<Profile> target_profile,
    uint64_t space,
    GURL url,
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive) {
  if (!target_profile) {
    return;
  }
  BrowserWindowInterface* visible_target = nullptr;
  BrowserWindowInterface* hidden_target = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
            browser->GetProfile() != target_profile.get()) {
          return true;
        }
        const auto* adapter = BrowserWorkspaceAdapter::ForBrowser(*browser);
        const ui::BaseWindow* window = browser->GetWindow();
        if (!adapter || !adapter->ready() || !window) {
          return true;
        }
        if (window->IsVisible() || window->IsMinimized()) {
          if (!visible_target) {
            visible_target = browser;
          }
        } else if (!hidden_target) {
          hidden_target = browser;
        }
        return !visible_target;
      });
  if (BrowserWindowInterface* target =
          visible_target ? visible_target : hidden_target) {
    FinishOpenLittleArcInWindow(std::move(little_arc), target->GetWeakPtr(),
                                space, std::move(url), std::move(keep_alive),
                                /*remaining_retries=*/0);
    return;
  }
  if (GetBrowserWindowCreationStatusForProfile(*target_profile) !=
      BrowserWindowInterface::CreationStatus::kOk) {
    return;
  }
  BrowserWindowCreateParams params(*target_profile, /*from_user_gesture=*/true);
  params.should_trigger_session_restore = false;
  BrowserWindowInterface* created = CreateBrowserWindow(std::move(params));
  if (!created) {
    return;
  }
  WorkspaceLifecycle::Notify(target_profile.get(),
                             WorkspaceLifecycle::Event::kNativeReady);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&FinishOpenLittleArcInWindow, std::move(little_arc),
                     created->GetWeakPtr(), space, std::move(url),
                     std::move(keep_alive), /*remaining_retries=*/20));
}

}  // namespace

bool MoveLittleArcToSpaceInProfile(BrowserWindowInterface& little_arc,
                                   const std::string& profile_dir,
                                   uint64_t space) {
  if (!IsLittleArc(little_arc)) {
    return false;
  }
  Profile* own_profile = little_arc.GetProfile();
  const std::string own_dir = own_profile->GetBaseName().AsUTF8Unsafe();
  const bool same_profile = profile_dir.empty() || profile_dir == own_dir;
  if (same_profile && LittleArcMoveTarget(little_arc)) {
    return MoveLittleArcToSpace(little_arc, space);
  }
  if (!same_profile &&
      (own_profile->IsOffTheRecord() || own_profile->IsGuestSession())) {
    return false;
  }
  ProfileManager* manager = g_browser_process->profile_manager();
  const base::FilePath path =
      same_profile ? own_profile->GetPath() : ResolveProfilePath(profile_dir);
  if (!manager || path.empty()) {
    return false;
  }
  content::WebContents* contents =
      little_arc.GetTabStripModel()->GetActiveWebContents();
  const GURL url = contents ? contents->GetVisibleURL() : GURL();
  auto on_loaded = base::BindOnce(
      [](base::WeakPtr<BrowserWindowInterface> little_arc_weak, uint64_t space,
         GURL url, Profile* profile) {
        if (!profile || IsProfileDirectoryMarkedForDeletion(profile->GetPath())) {
          return;
        }
        auto keep_alive = ScopedProfileKeepAlive::TryAcquire(
            profile, ProfileKeepAliveOrigin::kSessionRestore);
        if (!keep_alive) {
          return;
        }
        WorkspaceService::ForProfile(*profile).WhenLoaded(base::BindOnce(
            &OpenLittleArcInLoadedProfile, std::move(little_arc_weak),
            profile->GetWeakPtr(), space, std::move(url),
            std::move(keep_alive)));
      },
      little_arc.GetWeakPtr(), space, url);
  if (Profile* loaded = manager->GetProfileByPath(path)) {
    std::move(on_loaded).Run(loaded);
    return true;
  }
  return manager->LoadProfileByPath(path, /*incognito=*/false,
                                    std::move(on_loaded));
}

LittleArcHeader::LittleArcHeader(BrowserWindowInterface& browser,
                                 base::RepeatingClosure edit_address)
    : browser_(browser) {
  address_ = AddChildView(
      std::make_unique<LittleArcAddressPill>(std::move(edit_address)));
  open_in_ = AddChildView(std::make_unique<LittleArcOpenInButton>(
      base::BindRepeating(&LittleArcHeader::Move, base::Unretained(this)),
      base::BindRepeating(&LittleArcHeader::ShowSpacePickerWithSource,
                          base::Unretained(this),
                          ui::mojom::MenuSourceType::kMouse)));
  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(u"Little Arc");
#if BUILDFLAG(MYARC_TEST_HOOKS)
  size_t pick = 0;
  if (base::StringToSizeT(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              "myarc-test-little-arc-pick"),
          &pick)) {
    auto runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    runner->PostDelayedTask(FROM_HERE,
                            base::BindOnce(&LittleArcHeader::ShowSpacePicker,
                                           weak_factory_.GetWeakPtr()),
                            base::Seconds(2));
    runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LittleArcHeader::PickSpaceForTesting,
                       weak_factory_.GetWeakPtr(), pick),
        base::Seconds(4));
  }
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
}

LittleArcHeader::~LittleArcHeader() = default;

bool LittleArcHeader::IsDark() const {
  const ui::ColorProvider* colors = GetColorProvider();
  return colors && color_utils::IsDark(colors->GetColor(ui::kColorSysSurface));
}

SkColor LittleArcHeader::FrameColor() const {
  return (IsDark() ? kDark : kLight).frame;
}

void LittleArcHeader::Update() {
  const bool dark = IsDark();
  const Palette& palette = dark ? kDark : kLight;
  const TargetSpace space = FindTargetSpace(*browser_);
  const bool picker_available = HasPickerOptions(*browser_, space.found);
  content::WebContents* contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  const GURL url = contents ? contents->GetVisibleURL() : GURL();
  const bool blank = url.is_empty() || url.IsAboutBlank();
  std::u16string host;
  if (!blank) {
    host = url.has_host() ? base::UTF8ToUTF16(url_formatter::StripWWW(
                                std::string(url.host())))
                          : base::UTF8ToUTF16(url.spec().substr(0, 256));
  }
  const gfx::VectorIcon* security_icon = nullptr;
  std::u16string security_name;
  if (!blank && contents) {
    const security_state::SecurityLevel level =
        chrome_security_state::GetSecurityLevel(contents);
    const bool web = url.SchemeIsHTTPOrHTTPS();
    // Avoid flashing a warning triangle on HTTPS URLs while TLS is connecting
    // (security_state::NONE before the first response commits).
    const bool secure =
        level == security_state::SECURE ||
        (level == security_state::NONE && url.SchemeIsCryptographic());
    security_icon =
        secure ? &kPeekLockIcon : (web ? &kPeekNotSecureIcon : &kInfoIcon);
    security_name = secure ? u"Connection is secure"
                           : (web ? u"Not secure" : u"Browser page");
  }
  if (has_rendered_ && last_dark_ == dark &&
      last_space_found_ == space.found &&
      last_picker_available_ == picker_available &&
      last_space_name_ == space.name &&
      last_space_icon_ == space.icon.value && last_host_ == host &&
      last_blank_ == blank && last_security_icon_ == security_icon) {
    return;
  }
  has_rendered_ = true;
  last_dark_ = dark;
  last_space_found_ = space.found;
  last_picker_available_ = picker_available;
  last_space_name_ = space.name;
  last_space_icon_ = space.icon.value;
  last_host_ = host;
  last_blank_ = blank;
  last_security_icon_ = security_icon;

  address_->Set(palette, space, host, blank, security_icon, security_name);
  open_in_->Set(palette, space, picker_available);
  InvalidateLayout();
}

bool LittleArcHeader::IsDraggableAt(const gfx::Point& point) const {
  for (const views::View* child : {static_cast<const views::View*>(address_),
                                   static_cast<const views::View*>(open_in_)}) {
    if (child->GetVisible() && child->bounds().Contains(point)) {
      return false;
    }
  }
  return true;
}

void LittleArcHeader::Layout(PassKey) {
  const int y = (height() - kControlHeight) / 2;
  const int button_width =
      std::min(open_in_->GetPreferredSize().width(), width() / 3 + 24);
  open_in_->SetBounds(width() - kCardInset - button_width, y, button_width,
                      kControlHeight);
  const int left = kLightsWidth;
  address_->SetBounds(left, y, std::max(0, open_in_->x() - kGap - left),
                      kControlHeight);
}

void LittleArcHeader::OnThemeChanged() {
  views::View::OnThemeChanged();
  Update();
}

void LittleArcHeader::Move() {
  MoveLittleArcToSpace(*browser_);
}

void LittleArcHeader::ShowSpacePicker() {
  ShowSpacePickerWithSource(ui::mojom::MenuSourceType::kKeyboard);
}

void LittleArcHeader::ShowSpacePickerWithSource(
    ui::mojom::MenuSourceType source_type) {
  if (!GetWidget() || (picker_runner_ && picker_runner_->IsRunning())) {
    return;
  }
  uint64_t current_space_id = 0;
  const std::vector<PickerProfileGroup> groups =
      CollectPickerGroups(*browser_, &current_space_id);
  if (groups.empty()) {
    return;
  }
  const Palette& palette = IsDark() ? kDark : kLight;
  const bool multi_profile = groups.size() > 1;
  picker_targets_.clear();
  picker_current_ = current_space_id;
  picker_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  for (size_t i = 0; i < groups.size(); ++i) {
    const PickerProfileGroup& group = groups[i];
    if (multi_profile) {
      if (i > 0) {
        picker_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      }
      picker_model_->AddTitle(group.profile_name);
    }
    for (const PickerSpaceEntry& space : group.spaces) {
      const int command_id = static_cast<int>(picker_targets_.size());
      picker_model_->AddCheckItem(command_id, space.name);
      ui::ImageModel icon = SpaceIconImage(space.icon, palette.ink, 14);
      if (!icon.IsEmpty()) {
        picker_model_->SetIconForCommandId(command_id, icon);
      }
      picker_targets_.push_back({.profile_dir = group.profile_dir,
                                 .space_id = space.id,
                                 .same_profile = group.same_profile});
    }
  }
  if (picker_targets_.empty()) {
    return;
  }
  picker_runner_ = std::make_unique<views::MenuRunner>(picker_model_.get(),
                                                       /*run_types=*/0);
  picker_runner_->RunMenuAt(GetWidget(), nullptr, open_in_->GetBoundsInScreen(),
                            views::MenuAnchorPosition::kTopRight, source_type);
}

void LittleArcHeader::PickSpaceForTesting(size_t index) {
  if (index < picker_targets_.size()) {
    ExecuteCommand(static_cast<int>(index), 0);
  }
  if (picker_runner_) {
    picker_runner_->Cancel();
  }
}

void LittleArcHeader::ExecuteCommand(int command_id, int event_flags) {
  if (command_id < 0 ||
      static_cast<size_t>(command_id) >= picker_targets_.size()) {
    return;
  }
  const PickerTarget target = picker_targets_[command_id];
  // The move closes this window, so it runs once the menu has unwound.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<BrowserWindowInterface> browser,
             std::string profile_dir, uint64_t space) {
            if (browser) {
              MoveLittleArcToSpaceInProfile(*browser, profile_dir, space);
            }
          },
          browser_->GetWeakPtr(), target.profile_dir, target.space_id));
}

bool LittleArcHeader::IsCommandIdChecked(int command_id) const {
  if (command_id < 0 ||
      static_cast<size_t>(command_id) >= picker_targets_.size()) {
    return false;
  }
  const PickerTarget& target = picker_targets_[command_id];
  return target.same_profile && picker_current_ != 0 &&
         target.space_id == picker_current_;
}

namespace {
LittleArcHeader* FindHeader(views::View* view) {
  if (!view) {
    return nullptr;
  }
  if (auto* header = views::AsViewClass<LittleArcHeader>(view)) {
    return header;
  }
  for (views::View* child : view->children()) {
    if (auto* header = FindHeader(child)) {
      return header;
    }
  }
  return nullptr;
}
}  // namespace

void ShowLittleArcSpacePicker(BrowserWindowInterface& little_arc) {
  if (!IsLittleArc(little_arc)) {
    return;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      little_arc.GetWindow()->GetNativeWindow());
  if (LittleArcHeader* header =
          widget ? FindHeader(widget->GetRootView()) : nullptr) {
    header->ShowSpacePicker();
  }
}

BEGIN_METADATA(LittleArcHeader)
END_METADATA

namespace {

// Option-Cmd-N from any app. The listener is Chromium's, the one extension
// commands use; on macOS it registers a Carbon hot key.
class LittleArcHotKey final : public ui::GlobalAcceleratorListener::Observer {
 public:
  static LittleArcHotKey& Get() {
    static base::NoDestructor<LittleArcHotKey> hot_key;
    return *hot_key;
  }

  void Start() {
    PrefService* local_state = g_browser_process->local_state();
    if (started_ || !local_state || !IsLittleArcMode()) {
      return;
    }
    started_ = true;
    registrar_.Init(local_state);
    registrar_.Add(
        kLittleArcLinksPref,
        base::BindRepeating(&LittleArcHotKey::Update, base::Unretained(this)));
    Update();
  }

 private:
  friend class base::NoDestructor<LittleArcHotKey>;
  LittleArcHotKey() = default;
  ~LittleArcHotKey() override = default;

  static ui::Accelerator Key() {
#if BUILDFLAG(MYARC_TEST_HOOKS)
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            "myarc-test-little-arc-hotkey")) {
      return ui::Accelerator(ui::VKEY_F19, ui::EF_CONTROL_DOWN |
                                               ui::EF_ALT_DOWN |
                                               ui::EF_COMMAND_DOWN);
    }
#endif
    return ui::Accelerator(ui::VKEY_N, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  }

  void Update() {
    auto* listener = ui::GlobalAcceleratorListener::GetInstance();
    if (!listener || listener->IsShortcutHandlingSuspended()) {
      return;
    }
    const bool wanted =
        IsLittleArcLinksEnabled(g_browser_process->local_state());
    if (wanted == registered_) {
      return;
    }
    if (!wanted) {
      listener->UnregisterAccelerator(Key(), this);
      registered_ = false;
      LOG(WARNING) << "myarc-little-arc-hotkey unregistered";
      return;
    }
    // Another app holding the key wins; the key then works only with myArc
    // in front, through the View menu.
    registered_ = listener->RegisterAccelerator(Key(), this);
    LOG(WARNING) << "myarc-little-arc-hotkey "
                 << (registered_ ? "registered " : "taken by another app: ")
                 << Key().GetShortcutText();
  }

  // ui::GlobalAcceleratorListener::Observer:
  void OnKeyPressed(const ui::Accelerator& accelerator) override {
    // The Little Arc takes the Profile of the window used last.
    BrowserWindowInterface* last = nullptr;
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&last](BrowserWindowInterface* browser) {
          last = browser;
          return false;
        });
    const bool opened = last && OpenLittleArc(*last->GetProfile(), GURL());
    LOG(WARNING) << "myarc-little-arc-hotkey pressed opened=" << opened;
  }
  void ExecuteCommand(const std::string& accelerator_group_id,
                      const std::string& command_id) override {}

  PrefChangeRegistrar registrar_;
  bool started_ = false;
  bool registered_ = false;
};

}  // namespace

void StartLittleArcHotKey() {
  LittleArcHotKey::Get().Start();
}

}  // namespace myarc
