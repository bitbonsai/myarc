#include "myarc/space_icon_catalog.h"

#include <array>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_constants.h"

namespace myarc {
namespace {

// Draws one emoji centred in a square.
class EmojiImageSource final : public gfx::CanvasImageSource {
 public:
  EmojiImageSource(std::u16string emoji, int size, SkAlpha alpha)
      : gfx::CanvasImageSource(gfx::Size(size, size)),
        emoji_(std::move(emoji)),
        alpha_(alpha) {}

  void Draw(gfx::Canvas* canvas) override {
    const int size = this->size().width();
    // Apple Color Emoji's glyphs are a little under 1 em, so a font the
    // square's size nearly fills it.
    const gfx::FontList base;
    const gfx::FontList font =
        base.DeriveWithSizeDelta(size - base.GetFontSize());
    if (alpha_ != SK_AlphaOPAQUE) {
      canvas->SaveLayerAlpha(alpha_);
    }
    // Slack on every side so the advance and line box never elide or clip
    // the glyph; the canvas clips to the square.
    canvas->DrawStringRectWithFlags(
        emoji_, font, SK_ColorBLACK,
        gfx::Rect(-size / 2, -size / 2, size * 2, size * 2),
        gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_ELLIPSIS |
            gfx::Canvas::NO_SUBPIXEL_RENDERING);
    if (alpha_ != SK_AlphaOPAQUE) {
      canvas->Restore();
    }
  }

 private:
  const std::u16string emoji_;
  const SkAlpha alpha_;
};

}  // namespace

std::u16string_view EmojiCategoryName(EmojiCategory category) {
  static constexpr std::array<std::u16string_view, kEmojiCategoryCount> kNames =
      {u"Smileys & People", u"Animals & Nature", u"Food & Drink", u"Activity",
       u"Travel & Places",  u"Objects",          u"Symbols",      u"Flags"};
  return kNames[static_cast<size_t>(category)];
}

const EmojiEntry* FindBundledEmoji(std::string_view emoji) {
  if (emoji.empty()) {
    return nullptr;
  }
  for (const EmojiEntry& entry : BundledEmoji()) {
    if (emoji == entry.emoji) {
      return &entry;
    }
  }
  return nullptr;
}

const NamedSpaceIcon* FindBundledSpaceIcon(std::string_view name) {
  for (const NamedSpaceIcon& icon : BundledSpaceIcons()) {
    if (name == icon.name) {
      return &icon;
    }
  }
  return nullptr;
}

bool IsBundledSpaceIcon(const SpaceIconChoice& choice) {
  switch (choice.kind) {
    case SpaceIconChoice::Kind::kNone:
      return choice.value.empty();
    case SpaceIconChoice::Kind::kEmoji:
      return FindBundledEmoji(choice.value);
    case SpaceIconChoice::Kind::kNamed:
      return FindBundledSpaceIcon(choice.value);
  }
  return false;
}

bool MatchesQuery(std::string_view keywords, std::string_view query) {
  for (std::string_view word : base::SplitStringPiece(
           query, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (keywords.find(word) == std::string_view::npos) {
      return false;
    }
  }
  return true;
}

ui::ImageModel SpaceIconImage(const SpaceIconChoice& choice,
                              SkColor color,
                              int size,
                              SkAlpha alpha) {
  switch (choice.kind) {
    case SpaceIconChoice::Kind::kNone:
      return {};
    case SpaceIconChoice::Kind::kEmoji:
      if (!FindBundledEmoji(choice.value)) {
        return {};
      }
      return ui::ImageModel::FromImageSkia(
          gfx::CanvasImageSource::MakeImageSkia<EmojiImageSource>(
              base::UTF8ToUTF16(choice.value), size, alpha));
    case SpaceIconChoice::Kind::kNamed:
      if (const NamedSpaceIcon* icon = FindBundledSpaceIcon(choice.value)) {
        return ui::ImageModel::FromVectorIcon(*icon->icon, color, size);
      }
      return {};
  }
  return {};
}

}  // namespace myarc
