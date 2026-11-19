#ifndef MYARC_LOCAL_PAGE_MARKER_H_
#define MYARC_LOCAL_PAGE_MARKER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

class GURL;

namespace views {
class ImageView;
class Label;
class View;
}  // namespace views

namespace myarc {

// A local development page: http or https on localhost, *.localhost,
// *.local, 127.0.0.0/8, [::1] or 0.0.0.0. The Rust core decides the host.
bool IsLocalPage(const GURL& url);

// While `url` is local, `card` shows a 2 px system-blue border just inside its
// edge that follows its rounded corners. `card` paints to a layer; the border
// sits above its children, so it draws over the page.
void UpdateLocalPageBorder(views::View& card, const GURL& url);

// The address field's pill for a local page: a white code icon and the port on
// the border's blue.
class LocalPageChip : public views::BoxLayoutView {
  METADATA_HEADER(LocalPageChip, views::BoxLayoutView)

 public:
  static constexpr int kHeight = 18;

  LocalPageChip();
  LocalPageChip(const LocalPageChip&) = delete;
  LocalPageChip& operator=(const LocalPageChip&) = delete;
  ~LocalPageChip() override;

  // Shows `url`'s port, if it has one. Returns whether `url` is local.
  bool Update(const GURL& url);

 private:
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> port_ = nullptr;
};

}  // namespace myarc

#endif  // MYARC_LOCAL_PAGE_MARKER_H_
