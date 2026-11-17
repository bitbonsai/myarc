#ifndef MYARC_LITTLE_ARC_H_
#define MYARC_LITTLE_ARC_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace gfx {
class Point;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace myarc {

class LittleArcAddressPill;
class LittleArcOpenInButton;

// Little Arc (CONV-01): a small window with one page, a slim header and no
// sidebar, for quick lookups and for links other apps open. It is a Chromium
// popup window created with `myarc_little_arc`, so it stays out of the
// workspace, session restore and tab routing. "Open in" moves its page into a
// main window's current Space as a Today tab. UI thread only.

// True for a window OpenLittleArc() created.
bool IsLittleArc(const BrowserWindowInterface& browser);

// Opens a Little Arc of `profile`'s regular Profile showing `url`, or a blank
// page when `url` is empty, where the window opens the command bar. Returns
// null outside myArc's workspace mode or when the Profile can't open windows.
BrowserWindowInterface* OpenLittleArc(Profile& profile, const GURL& url);

// A link another app opened. Rust routes it: with the Settings switch on, a
// web link opens in a new Little Arc, or brings forward the Little Arc that
// already shows it. Returns false when the link should open as a normal tab.
bool OpenLinkFromOtherApp(Profile& profile, const GURL& url);

// Registers Option-Cmd-N system-wide while Settings > Links' Little Arc
// switch is on, so the key opens a new Little Arc on the command bar with
// another app in front. Turning the switch off unregisters it. When another
// app holds the key, registration fails and is logged, and the key works only
// with myArc in front. Idempotent; call once a workspace window exists.
// Test-hook builds take --myarc-test-little-arc-hotkey, which registers
// Ctrl-Option-Cmd-F19 instead, so a test can post a key no app holds.
void StartLittleArcHotKey();

// The main window "Open in" moves `little_arc`'s page to: the most recently
// used one of its Profile with a loaded workspace, or null.
BrowserWindowInterface* LittleArcMoveTarget(BrowserWindowInterface& little_arc);

// Moves the page into the target window's current Space as a Today tab and
// shows that window. The Little Arc closes once its page has gone. Returns
// false, changing nothing, when there is no target.
bool MoveLittleArcToSpace(BrowserWindowInterface& little_arc);

// The Little Arc header, level with the traffic lights: an address pill with
// the target Space and the page's host, and the "Open in <Space> ⌘O" button.
// BrowserView owns it and lays the page card out beneath it.
class LittleArcHeader final : public views::View {
  METADATA_HEADER(LittleArcHeader, views::View)

 public:
  static constexpr int kHeight = 40;
  // The page card's inset from the window edges and its corner radius.
  static constexpr int kCardInset = 6;
  static constexpr int kCardRadius = 8;
  // Room the traffic lights take at the header's left.
  static constexpr int kLightsWidth = 78;

  // `edit_address` opens the command bar on the page's address.
  LittleArcHeader(BrowserWindowInterface& browser,
                  base::RepeatingClosure edit_address);
  LittleArcHeader(const LittleArcHeader&) = delete;
  LittleArcHeader& operator=(const LittleArcHeader&) = delete;
  ~LittleArcHeader() override;

  // Rereads the page and the Space "Open in" would move it to.
  void Update();
  // Points on no control drag the window.
  bool IsDraggableAt(const gfx::Point& point) const;
  // The window colour around the page card.
  SkColor FrameColor() const;

  // views::View:
  void Layout(PassKey) override;
  void OnThemeChanged() override;

 private:
  bool IsDark() const;
  void Move();

  const raw_ref<BrowserWindowInterface> browser_;
  raw_ptr<LittleArcAddressPill> address_ = nullptr;
  raw_ptr<LittleArcOpenInButton> open_in_ = nullptr;
};

}  // namespace myarc

#endif  // MYARC_LITTLE_ARC_H_
