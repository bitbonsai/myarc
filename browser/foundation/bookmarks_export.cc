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
#include "base/memory/weak_ptr.h"
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
constexpr char16_t kSpaceTitle[] = u"Export Space";
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

void ShowMessage(std::u16string title,
                 std::u16string message,
                 gfx::NativeWindow parent) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      views::BubbleDialogModelHost::CreateModal(
          ui::DialogModel::Builder()
              .SetTitle(std::move(title))
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
      dark ? SkColorSetARGB(0xA1, 0x1E, 0x1E, 0x24)
           : SkColorSetARGB(0xD4, 0xFA, 0xFA, 0xFD),
      provider ? provider->GetColor(ui::kColorDialogBackground)
               : (dark ? SkColorSetRGB(0x25, 0x25, 0x28) : SK_ColorWHITE));
  widget->Show();
}

void Written(gfx::NativeWindow parent, Result result) {
  if (!result.written) {
    ShowMessage(
        kTitle,
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
    ShowMessage(kTitle,
                base::StrCat({u"The file is saved, but myArc couldn’t "
                              u"read the Spaces of these Profiles, so they "
                              u"are left out: ",
                              names, u"."}),
                parent);
  }
}

bool WriteSpace(foundation::StateSnapshot snapshot,
                uint64_t space_id,
                bool as_json,
                const base::FilePath& path) {
  const rust::String payload = foundation::export_space_bookmarks(
      std::move(snapshot), space_id, as_json);
  if (payload.empty()) {
    return false;
  }
  return base::WriteFile(path,
                         std::string_view(payload.data(), payload.size()));
}

void SpaceWritten(gfx::NativeWindow parent, bool written) {
  if (!written) {
    ShowMessage(
        kSpaceTitle,
        u"myArc couldn’t save the file there. Pick another folder and "
        u"try again.",
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

// Owns itself while the single-Space save panel is up.
class SpacePicker : public ui::SelectFileDialog::Listener {
 public:
  SpacePicker(base::WeakPtr<Profile> profile,
              uint64_t space_id,
              std::u16string space_name,
              gfx::NativeWindow parent)
      : profile_(std::move(profile)),
        space_id_(space_id),
        space_name_(std::move(space_name)),
        parent_(parent) {}
  SpacePicker(const SpacePicker&) = delete;
  SpacePicker& operator=(const SpacePicker&) = delete;
  ~SpacePicker() override { dialog_->ListenerDestroyed(); }

  void Open() {
    dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(nullptr));
    ui::SelectFileDialog::FileTypeInfo types;
    types.extensions = {{FILE_PATH_LITERAL("html"), FILE_PATH_LITERAL("htm")},
                        {FILE_PATH_LITERAL("json")}};
    types.extension_description_overrides = {u"Netscape Bookmarks HTML",
                                             u"JSON"};
    const std::string utf8_name = base::UTF16ToUTF8(space_name_);
    const std::string file_name(foundation::space_export_file_name(
        rust::Str(utf8_name.data(), utf8_name.size())));
    dialog_->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE, kSpaceTitle,
                        base::GetHomeDir().Append("Downloads").Append(file_name),
                        &types, 1, FILE_PATH_LITERAL("html"), parent_);
  }

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    if (!profile_) {
      delete this;
      return;
    }
    std::optional<foundation::StateSnapshot> snapshot =
        WorkspaceService::ForProfile(*profile_).ExportSnapshot();
    if (!snapshot) {
      SpaceWritten(parent_, false);
      delete this;
      return;
    }
    const bool as_json =
        file.path().MatchesExtension(FILE_PATH_LITERAL(".json")) || index == 2;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(&WriteSpace, std::move(*snapshot), space_id_, as_json,
                       file.path()),
        base::BindOnce(&SpaceWritten, parent_));
    delete this;
  }
  void FileSelectionCanceled() override { delete this; }

 private:
  const base::WeakPtr<Profile> profile_;
  const uint64_t space_id_;
  const std::u16string space_name_;
  const gfx::NativeWindow parent_;
  scoped_refptr<ui::SelectFileDialog> dialog_;
};

}  // namespace

void ExportPinsAndFavorites(BookmarksProfiles profiles,
                            gfx::NativeWindow parent) {
  (new Picker(std::move(profiles), parent))->Open();
}

void ExportSpaceBookmarks(Profile& profile,
                          uint64_t space_id,
                          std::u16string space_name,
                          gfx::NativeWindow parent) {
  (new SpacePicker(profile.GetWeakPtr(), space_id, std::move(space_name),
                   parent))
      ->Open();
}

}  // namespace myarc
