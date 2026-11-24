#ifndef MYARC_LOCAL_PAGE_MARKER_H_
#define MYARC_LOCAL_PAGE_MARKER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

class GURL;

namespace views {
class Label;
class View;
}  // namespace views

namespace myarc {

// A local development page: http or https on localhost, *.localhost,
// *.local, 127.0.0.0/8, [::1] or 0.0.0.0. The Rust core decides the host.
bool IsLocalPage(const GURL& url);

// Background tint for the sidebar location pill while showing a local page.
SkColor LocalPagePillBackground(bool dark, bool hovered);

// Formats a local page URL for the resting sidebar location pill: displays
// `localhost` for `localhost` and `127.0.0.1`, omits the port (shown in the
// trailing chip), and keeps any non-root path, query or ref.
std::u16string FormatLocalPageDisplayURL(const GURL& url);

// While `url` is local, `card` shows a 2 px system-blue border just inside its
// edge that follows its rounded corners. `card` paints to a layer; the border
// sits above its children, so it draws over the page.
void UpdateLocalPageBorder(views::View& card, const GURL& url);

// Compact port tag for a local page, used at the trailing edge of the location
// pill and on vertical sidebar tab rows.
class LocalPageChip : public views::BoxLayoutView {
  METADATA_HEADER(LocalPageChip, views::BoxLayoutView)

 public:
  enum class Style {
    kLocationBar,
    kSidebarTab,
  };

  static constexpr int kHeight = 18;
  static constexpr int kTabHeight = 16;

  explicit LocalPageChip(Style style = Style::kLocationBar);
  LocalPageChip(const LocalPageChip&) = delete;
  LocalPageChip& operator=(const LocalPageChip&) = delete;
  ~LocalPageChip() override;

  // Shows `url`'s port when `url` is local and has an explicit port. Returns
  // whether the chip has a port to show.
  bool Update(const GURL& url);

  // Whether the chip currently holds a port label (safe to query from layout
  // managers without reading view visibility).
  bool HasPort() const { return has_port_; }

  // views::View:
  void OnThemeChanged() override;

 private:
  void ApplyStyleColors();

  const Style style_;
  bool has_port_ = false;
  raw_ptr<views::Label> port_ = nullptr;
};

}  // namespace myarc

#endif  // MYARC_LOCAL_PAGE_MARKER_H_
