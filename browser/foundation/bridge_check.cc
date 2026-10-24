#include <iostream>
#include <string>

#include "myarc/bridge.rs.h"

int main() {
  auto workspace = myarc::foundation::new_workspace();
  auto second = myarc::foundation::new_workspace();
  if (std::string(myarc::foundation::current_name(*workspace)) != "Everyday") {
    return 1;
  }
  const auto renamed =
      myarc::foundation::rename_space(*workspace, 1, "Research");
  if (!renamed.ok || std::string(renamed.name) != "Research") {
    return 2;
  }
  const auto invalid = myarc::foundation::rename_space(*workspace, 1, "");
  if (invalid.ok || std::string(invalid.name) != "Research" ||
      invalid.error.empty()) {
    return 3;
  }
  const auto unknown = myarc::foundation::rename_space(*workspace, 2, "Other");
  if (unknown.ok || std::string(unknown.name) != "Research") {
    return 4;
  }
  const auto oversize =
      myarc::foundation::rename_space(*workspace, 1, std::string(129, 'x'));
  if (oversize.ok || std::string(oversize.name) != "Research") {
    return 5;
  }
  if (std::string(myarc::foundation::current_name(*second)) != "Everyday") {
    return 6;
  }
  for (int i = 0; i < 1000; ++i) {
    auto temporary = myarc::foundation::new_workspace();
    if (!myarc::foundation::rename_space(*temporary, 1, "Owned by Rust").ok) {
      return 7;
    }
  }
  std::cout << "7 C++/Rust bridge checks passed, including 1000 ownership "
               "round trips\n";
  return 0;
}
