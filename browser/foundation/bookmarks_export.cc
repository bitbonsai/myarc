#include "myarc/bookmarks_export.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "components/constrained_window/constrained_window_views.h"
#include "myarc/glass_mac.h"
#include "myarc/workspace_service.h"
#include "ui/base/models/dialog_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace myarc {

namespace {

constexpr char16_t kTitle[] = u"Export Pins and Favorites";
constexpr char kWorkspaceFile[] = "myarc-workspace-v1.json";
// The workspace file's own bound (workspace_service.cc).
constexpr size_t kMaxWorkspaceBytes = 20 * 1024 * 1024;

// A Profile to write: its live snapshot, or the workspace file to read.
struct Source {
  std::u16string name;
  std::optional<foundation::StateSnapshot> snapshot;
  base::FilePath file;
};

struct Result {
  bool written = false;
  // Profiles whose workspace couldn't be read, so they're left out.
  std::vector<std::u16string> skipped;
};

// Worker sequence: reads the files of Profiles that aren't loaded, builds the
// bookmarks file and writes it to `path`. Profile workspace files are only
// read. A Profile without a workspace file has nothing to export.
Result Write(std::vector<Source> sources, const base::FilePath& path) {
  Result result;
  rust::Box<foundation::BookmarksExport> bookmarks =
      foundation::new_bookmarks_export();
  for (Source& source : sources) {
    if (!source.snapshot) {
      if (!base::PathExists(source.file)) {
        continue;
      }
      std::string json;
      if (base::ReadFileToStringWithMaxSize(source.file, &json,
                                            kMaxWorkspaceBytes)) {
        source.snapshot =
            std::move(internal::DecodeWorkspaceMetadata(json).snapshot);
      }
    }
    const std::string name = base::UTF16ToUTF8(source.name);
    if (!source.snapshot || !foundation::add_bookmarks_profile(
                                *bookmarks, rust::Str(name.data(), name.size()),
                                std::move(*source.snapshot))) {
      result.skipped.push_back(std::move(source.name));
    }
  }
  const rust::String html = foundation::bookmarks_export_html(*bookmarks);
  result.written =
      base::WriteFile(path, std::string_view(html.data(), html.size()));
  return result;
}

void ShowMessage(std::u16string message, gfx::NativeWindow parent) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      views::BubbleDialogModelHost::CreateModal(
          ui::DialogModel::Builder()
              .SetTitle(kTitle)
              .AddParagraph(ui::DialogModelLabel(std::move(message)))
              .AddOkButton(base::DoNothing(),
                           ui::DialogModel::Button::Params().SetLabel(u"OK"))
              .Build(),
          ui::mojom::ModalType::kWindow),
      parent);
  const ui::ColorProvider* provider = widget->GetColorProvider();
  const bool dark =
      !provider ||
      color_utils::IsDark(provider->GetColor(ui::kColorDialogBackground));
  ApplyBubbleGlass(
      *widget, GlassMaterial::kSheet, dark,
      dark ? SkColorSetARGB(0xB8, 0x1E, 0x1E, 0x24)
           : SkColorSetARGB(0xD4, 0xFA, 0xFA, 0xFD),
      provider ? provider->GetColor(ui::kColorDialogBackground)
               : (dark ? SkColorSetRGB(0x25, 0x25, 0x28) : SK_ColorWHITE));
  widget->Show();
}

void Written(gfx::NativeWindow parent, Result result) {
  if (!result.written) {
    ShowMessage(
        u"myArc couldn’t save the file there. Pick another folder and "
        u"try again.",
        parent);
    return;
  }
  if (!result.skipped.empty()) {
    std::u16string names;
    for (const std::u16string& name : result.skipped) {
      names = names.empty() ? name : base::StrCat({names, u", ", name});
    }
    ShowMessage(base::StrCat({u"The file is saved, but myArc couldn’t "
                              u"read the Spaces of these Profiles, so they "
                              u"are left out: ",
                              names, u"."}),
                parent);
  }
}

// Owns itself while the save panel is up.
class Picker : public ui::SelectFileDialog::Listener {
 public:
  Picker(BookmarksProfiles profiles, gfx::NativeWindow parent)
      : profiles_(std::move(profiles)), parent_(parent) {}
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
    // Taken only now, so the file holds the workspaces as they are when saved.
    std::vector<Source> sources;
    for (const BookmarksProfile& profile : profiles_.Run()) {
      Source source{.name = profile.name};
      if (profile.profile) {
        source.snapshot =
            WorkspaceService::ForProfile(*profile.profile).ExportSnapshot();
      }
      if (!source.snapshot) {
        source.file = profile.dir.Append(kWorkspaceFile);
      }
      sources.push_back(std::move(source));
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(&Write, std::move(sources), file.path()),
        base::BindOnce(&Written, parent_));
    delete this;
  }
  void FileSelectionCanceled() override { delete this; }

 private:
  const BookmarksProfiles profiles_;
  const gfx::NativeWindow parent_;
  scoped_refptr<ui::SelectFileDialog> dialog_;
};

}  // namespace

void ExportPinsAndFavorites(BookmarksProfiles profiles,
                            gfx::NativeWindow parent) {
  (new Picker(std::move(profiles), parent))->Open();
}

}  // namespace myarc
