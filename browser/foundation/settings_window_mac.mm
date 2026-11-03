#import <Cocoa/Cocoa.h>

#include <string>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
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

std::u16string MyarcVersionLabel() {
  // Written into Info.plist when the nightly is packaged; dev builds lack it.
  NSString* version = base::apple::ObjCCast<NSString>(
      [base::apple::OuterBundle() objectForInfoDictionaryKey:@"MyarcVersion"]);
  return version.length ? u"Nightly " + base::SysNSStringToUTF16(version)
                        : u"Development build";
}

ui::ImageModel AppIconImage() {
  // The bundle's ribbon icon. NSApp's icon can come back as the system's
  // tinted variant.
  NSString* path = [base::apple::OuterBundle() pathForResource:@"app"
                                                        ofType:@"icns"];
  NSImage* icon = path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
  return icon ? ui::ImageModel::FromImage(gfx::Image(icon)) : ui::ImageModel();
}

views::NativeWidget* CreateSettingsNativeWidget(views::Widget* widget) {
  return new SettingsNativeWidgetMac(widget);
}

}  // namespace myarc
