#include "myarc/native_button.h"

#include <string>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "myarc/bridge.rs.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/md_text_button.h"

namespace myarc {
namespace {

class FoundationButton : public views::MdTextButton {
  METADATA_HEADER(FoundationButton, views::MdTextButton)

 public:
  FoundationButton()
      : views::MdTextButton(base::BindRepeating(&FoundationButton::Rename,
                                                base::Unretained(this))),
        workspace_(foundation::new_workspace()) {
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    UpdateName(foundation::current_name(*workspace_));
    SetTooltipText(
        u"M0 test: rename transient Space through Rust. Resets on close.");
  }

  ~FoundationButton() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  void UpdateName(const rust::String& name) {
    SetText(base::UTF8ToUTF16("Test Space: " + std::string(name)));
  }

  void Rename() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const auto current = foundation::current_name(*workspace_);
    const std::string next =
        std::string(current) == "Everyday" ? "Research" : "Everyday";
    auto result = foundation::rename_space(*workspace_, 1, next);
    if (!result.ok) {
      SetText(u"Test Space: rename failed");
      SetTooltipText(base::UTF8ToUTF16(std::string(result.error)));
      return;
    }
    UpdateName(result.name);
  }

  rust::Box<foundation::Workspace> workspace_;
  SEQUENCE_CHECKER(sequence_checker_);
};

BEGIN_METADATA(FoundationButton)
END_METADATA

}  // namespace

std::unique_ptr<views::View> CreateFoundationButton() {
  return std::make_unique<FoundationButton>();
}

}  // namespace myarc
