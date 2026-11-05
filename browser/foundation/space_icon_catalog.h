#ifndef MYARC_SPACE_ICON_CATALOG_H_
#define MYARC_SPACE_ICON_CATALOG_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace myarc {

// The Space icon picker's bundled sets: emoji from Unicode with CLDR search
// keywords, and Lucide icons. A Space may only carry an icon from these, so
// text the core accepts but the picker never offers draws the default glyph.

// The Emoji tab's categories, in Arc's tab order.
enum class EmojiCategory : uint8_t {
  kSmileys,
  kNature,
  kFood,
  kActivities,
  kTravel,
  kObjects,
  kSymbols,
  kFlags,
};
inline constexpr size_t kEmojiCategoryCount = 8;

struct EmojiEntry {
  // UTF-8.
  const char* emoji;
  // Lowercase CLDR name, then keywords, joined by '|'.
  const char* keywords;
  EmojiCategory category;
};

struct NamedSpaceIcon {
  // Lucide's name, which the Space stores.
  const char* name;
  // A constexpr table of static icons.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon;
};

// A Space's icon as the picker and the sidebar see it.
struct SpaceIconChoice {
  enum class Kind : uint8_t { kNone, kEmoji, kNamed };
  Kind kind = Kind::kNone;
  std::string value;

  bool operator==(const SpaceIconChoice&) const = default;
};

// Generated in space_icon_data.cc.
base::span<const EmojiEntry> BundledEmoji();
base::span<const NamedSpaceIcon> BundledSpaceIcons();
const gfx::VectorIcon& EmojiCategoryIcon(EmojiCategory category);
std::u16string_view EmojiCategoryName(EmojiCategory category);

const EmojiEntry* FindBundledEmoji(std::string_view emoji);
const NamedSpaceIcon* FindBundledSpaceIcon(std::string_view name);
// True for no icon, or an emoji or name from the bundled sets.
bool IsBundledSpaceIcon(const SpaceIconChoice& choice);

// Whether every space-separated word of `query` (lowercase ASCII) appears in
// `keywords`. An empty query matches.
bool MatchesQuery(std::string_view keywords, std::string_view query);

// The icon drawn `size` DIP square: an emoji as color text, a named icon in
// `color`. Empty for no icon or one outside the bundled sets, so callers keep
// their default glyph. `alpha` fades an emoji, which ignores `color`.
ui::ImageModel SpaceIconImage(const SpaceIconChoice& choice,
                              SkColor color,
                              int size,
                              SkAlpha alpha = SK_AlphaOPAQUE);

}  // namespace myarc

#endif  // MYARC_SPACE_ICON_CATALOG_H_
