#import <Cocoa/Cocoa.h>

#include "myarc/app_activation.h"

namespace myarc {

bool IsAppActive() {
  return NSApp.active;
}

}  // namespace myarc
