#ifndef MYARC_NATIVE_BUTTON_H_
#define MYARC_NATIVE_BUTTON_H_

#include <memory>

namespace views {
class View;
}

namespace myarc {
std::unique_ptr<views::View> CreateFoundationButton();
}

#endif  // MYARC_NATIVE_BUTTON_H_
