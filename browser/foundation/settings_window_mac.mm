#import <Cocoa/Cocoa.h>

#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

namespace myarc {
namespace {

// Arc's Settings window draws its title and tab row on one surface, so the
// content view fills the transparent title bar and only the traffic lights
// stay native.
class SettingsNativeWidgetMac : public views::NativeWidgetMac {
 public:
  explicit SettingsNativeWidgetMac(views::Widget* widget)
      : views::NativeWidgetMac(widget) {}

 protected:
  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override {
    params->style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                         NSWindowStyleMaskMiniaturizable |
                         NSWindowStyleMaskFullSizeContentView;
    params->titlebar_appears_transparent = true;
    params->window_title_hidden = true;
  }
};

}  // namespace

bool IsSettingsAppActive() {
  return NSApp.active;
}

views::NativeWidget* CreateSettingsNativeWidget(views::Widget* widget) {
  return new SettingsNativeWidgetMac(widget);
}

}  // namespace myarc
