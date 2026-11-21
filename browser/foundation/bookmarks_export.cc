#include "myarc/bookmarks_export.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "components/constrained_window/constrained_window_views.h"
#include "myarc/workspace_service.h"
#include "ui/base/models/dialog_model.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace myarc {

namespace {

constexpr char16_t kTitle[] = u"Export Pins and Favorites";

void ShowMessage(std::u16string message, gfx::NativeWindow parent) {
  constrained_window::CreateBrowserModalDialogViews(
      views::BubbleDialogModelHost::CreateModal(
          ui::DialogModel::Builder()
              .SetTitle(kTitle)
              .AddParagraph(ui::DialogModelLabel(std::move(message)))
              .AddOkButton(base::DoNothing(),
                           ui::DialogModel::Button::Params().SetLabel(u"OK"))
              .Build(),
          ui::mojom::ModalType::kWindow),
      parent)
      ->Show();
}

void Written(gfx::NativeWindow parent, bool ok) {
  if (!ok) {
    ShowMessage(
        u"myArc couldn’t save the file there. Pick another folder and "
        u"try again.",
        parent);
  }
}

// Owns itself while the save panel is up.
class Picker : public ui::SelectFileDialog::Listener {
 public:
  Picker(Profile& profile, gfx::NativeWindow parent)
      : profile_(profile.GetWeakPtr()), parent_(parent) {}
  Picker(const Picker&) = delete;
  Picker& operator=(const Picker&) = delete;
  ~Picker() override { dialog_->ListenerDestroyed(); }

  void Open() {
    dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(nullptr));
    ui::SelectFileDialog::FileTypeInfo types;
    types.extensions = {{FILE_PATH_LITERAL("html"), FILE_PATH_LITERAL("htm")}};
    const std::string name(foundation::bookmarks_html_file_name());
    dialog_->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE, kTitle,
                        base::GetHomeDir().Append("Downloads").Append(name),
                        &types, 1, FILE_PATH_LITERAL("html"), parent_);
  }

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    // Built only now, so the file holds the workspace as it is when saved.
    std::optional<std::string> html;
    if (profile_) {
      html = WorkspaceService::ForProfile(*profile_).BookmarksHtml();
    }
    if (!html) {
      ShowMessage(
          u"Your Spaces haven’t loaded yet, so there’s nothing to "
          u"export. Try again in a moment.",
          parent_);
    } else {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(
              [](const base::FilePath& path, const std::string& html) {
                return base::WriteFile(path, html);
              },
              file.path(), std::move(*html)),
          base::BindOnce(&Written, parent_));
    }
    delete this;
  }
  void FileSelectionCanceled() override { delete this; }

 private:
  const base::WeakPtr<Profile> profile_;
  const gfx::NativeWindow parent_;
  scoped_refptr<ui::SelectFileDialog> dialog_;
};

}  // namespace

void ExportPinsAndFavorites(Profile& profile, gfx::NativeWindow parent) {
  (new Picker(profile, parent))->Open();
}

}  // namespace myarc
