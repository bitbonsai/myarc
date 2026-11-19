#include "myarc/arc_import.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "components/constrained_window/constrained_window_views.h"
#include "crypto/hash.h"
#include "myarc/workspace_service.h"
#include "ui/base/models/dialog_model.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace myarc {

namespace {

constexpr char16_t kTitle[] = u"Import from Arc";
// Lines of each list the preview shows before "and N more".
constexpr size_t kShownLines = 10;

using Confirmed = base::OnceCallback<void(uint64_t)>;

// Runs the caller's ArcImportDone. A commit runs it at once, before anything
// else, so the caller can save it before a kill. Otherwise it runs once the
// last step holding it lets go: the picker, the preview's buttons or the
// apply. Posted then, since the last release may happen while a dialog
// closes.
class Outcome : public base::RefCounted<Outcome> {
 public:
  explicit Outcome(ArcImportDone done) : done_(std::move(done)) {}
  Outcome(const Outcome&) = delete;
  Outcome& operator=(const Outcome&) = delete;

  void Commit(std::string content_hash, uint64_t first_space) {
    outcome_.committed = true;
    outcome_.content_hash = std::move(content_hash);
    if (first_space) {
      outcome_.space_ids = {first_space};
    }
    if (done_) {
      std::move(done_).Run(outcome_);
    }
  }

 private:
  friend class base::RefCounted<Outcome>;
  ~Outcome() {
    if (done_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(done_), std::move(outcome_)));
    }
  }

  ArcImportDone done_;
  ArcImportOutcome outcome_;
};

struct ReadResult {
  std::string error;
  std::optional<rust::Box<foundation::ArcExport>> export_data;
  std::string content_hash;
};

// Worker sequence: reads at most the export limit and parses. The file is
// only read, never written.
ReadResult ReadExport(const base::FilePath& path) {
  const size_t max = foundation::arc_export_max_bytes();
  const std::optional<int64_t> size = base::GetFileSize(path);
  if (!size) {
    return {"myArc couldn't open this file.", std::nullopt};
  }
  if (*size < 0 || static_cast<uint64_t>(*size) > max) {
    return {base::StrCat({"This file is larger than ",
                          base::NumberToString(max / (1024 * 1024)),
                          " MB, so it isn't an Arc export."}),
            std::nullopt};
  }
  std::string bytes;
  if (!base::ReadFileToStringWithMaxSize(path, &bytes, max)) {
    return {"myArc couldn't read this file.", std::nullopt};
  }
  return {std::string(),
          WorkspaceService::ParseArcExport(base::as_byte_span(bytes)),
          base::HexEncodeLower(crypto::hash::Sha256(bytes))};
}

std::u16string Count(size_t count,
                     std::u16string_view one,
                     std::u16string_view many) {
  return base::StrCat(
      {base::NumberToString16(count), u" ", count == 1 ? one : many});
}

// "a", "a and b" or "a, b and c".
std::u16string Join(const std::vector<std::u16string>& parts) {
  std::u16string text;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) {
      text += i + 1 == parts.size() ? u" and " : u", ";
    }
    text += parts[i];
  }
  return text;
}

std::u16string List(const rust::Vec<rust::String>& lines, size_t total) {
  std::u16string text;
  for (size_t i = 0; i < lines.size() && i < kShownLines; ++i) {
    if (!text.empty()) {
      text += u"\n";
    }
    text +=
        base::UTF8ToUTF16(std::string_view(lines[i].data(), lines[i].size()));
  }
  const size_t shown = std::min(lines.size(), kShownLines);
  if (total > shown) {
    base::StrAppend(
        &text, {u"\nand ", base::NumberToString16(total - shown), u" more"});
  }
  return text;
}

// "4 Spaces, 1 folder and 13 pinned tabs", or empty when nothing is new.
std::u16string Additions(const foundation::ArcImportPreview& preview) {
  std::vector<std::u16string> adds;
  if (preview.new_spaces) {
    adds.push_back(Count(preview.new_spaces, u"Space", u"Spaces"));
  }
  if (preview.new_folders) {
    adds.push_back(Count(preview.new_folders, u"folder", u"folders"));
  }
  if (preview.new_pins) {
    adds.push_back(Count(preview.new_pins, u"pinned tab", u"pinned tabs"));
  }
  return Join(adds);
}

views::Widget* Show(std::unique_ptr<ui::DialogModel> model,
                    gfx::NativeWindow parent,
                    bool inactive) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      views::BubbleDialogModelHost::CreateModal(std::move(model),
                                                ui::mojom::ModalType::kWindow),
      parent);
  if (inactive) {
    widget->ShowInactive();
  } else {
    widget->Show();
  }
  return widget;
}

void ShowMessage(std::u16string message,
                 gfx::NativeWindow parent,
                 bool inactive) {
  Show(ui::DialogModel::Builder()
           .SetTitle(kTitle)
           .AddParagraph(ui::DialogModelLabel(std::move(message)))
           .AddOkButton(base::DoNothing(),
                        ui::DialogModel::Button::Params().SetLabel(u"OK"))
           .Build(),
       parent, inactive);
}

// The done state: what the import added, then every kind of entry it left
// out or changed, with counts from Rust.
void ShowReport(const foundation::ArcImportPreview& result,
                gfx::NativeWindow parent,
                bool inactive) {
  ui::DialogModel::Builder builder;
  builder.SetTitle(kTitle);
  const std::u16string adds = Additions(result);
  builder.AddParagraph(ui::DialogModelLabel(
      adds.empty() ? std::u16string(u"Import finished.")
                   : base::StrCat({u"Import finished. Added ", adds, u"."})));
  if (result.moved_pins) {
    builder.AddParagraph(ui::DialogModelLabel(base::StrCat(
        {u"Moved ", Count(result.moved_pins, u"pinned tab", u"pinned tabs"),
         u" into ", result.moved_pins == 1 ? u"its" : u"their",
         u" Arc folder."})));
  }
  if (result.report.empty()) {
    builder.AddParagraph(
        ui::DialogModelLabel(u"Nothing in the file was left out or changed."));
  } else {
    builder.AddParagraph(
        ui::DialogModelLabel(List(result.report, result.report.size()))
            .set_is_secondary(),
        u"Left out or changed");
  }
  builder.AddParagraph(
      ui::DialogModelLabel(base::UTF8ToUTF16(std::string(result.report_note)))
          .set_is_secondary());
  builder.AddOkButton(base::DoNothing(),
                      ui::DialogModel::Button::Params().SetLabel(u"OK"));
  Show(builder.Build(), parent, inactive);
}

void Apply(base::WeakPtr<Profile> profile,
           gfx::NativeWindow parent,
           bool inactive,
           rust::Box<foundation::ArcExport> export_data,
           const std::string& content_hash,
           scoped_refptr<Outcome> outcome,
           Confirmed confirmed) {
  if (!profile) {
    return;
  }
  auto result =
      WorkspaceService::ForProfile(*profile).ApplyArcImport(*export_data);
  const uint64_t first_space = result.ok ? result.first_space : 0;
  if (result.ok && outcome) {
    outcome->Commit(content_hash, first_space);
  }
  // Post: the preview is still closing.
  if (!result.ok) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ShowMessage,
            base::StrCat({u"Nothing was imported. ",
                          base::UTF8ToUTF16(std::string(result.error))}),
            parent, inactive));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ShowReport, std::move(result), parent, inactive));
  }
  if (confirmed) {
    std::move(confirmed).Run(first_space);
  }
}

void ShowPreview(base::WeakPtr<Profile> profile,
                 gfx::NativeWindow parent,
                 bool inactive,
                 scoped_refptr<Outcome> outcome,
                 Confirmed confirmed,
                 ReadResult read) {
  if (!profile) {
    return;
  }
  if (!read.export_data) {
    ShowMessage(base::UTF8ToUTF16(read.error), parent, inactive);
    return;
  }
  auto export_data = std::move(*read.export_data);
  const auto preview =
      WorkspaceService::ForProfile(*profile).PreviewArcImport(*export_data);
  ui::DialogModel::Builder builder;
  builder.SetTitle(kTitle);
  const bool noop = preview.ok && !preview.new_spaces && !preview.new_folders &&
                    !preview.new_pins && !preview.moved_pins;
  if (!preview.ok) {
    builder.AddParagraph(ui::DialogModelLabel(
        base::StrCat({u"This file can't be imported. ",
                      base::UTF8ToUTF16(std::string(preview.error))})));
  } else if (noop) {
    builder.AddParagraph(
        ui::DialogModelLabel(u"Everything in this file is already in myArc."));
  } else {
    const std::u16string adds = Additions(preview);
    if (!adds.empty()) {
      builder.AddParagraph(
          ui::DialogModelLabel(base::StrCat({u"Adds ", adds, u"."})));
    }
    if (preview.moved_pins) {
      builder.AddParagraph(ui::DialogModelLabel(base::StrCat(
          {u"Moves ", Count(preview.moved_pins, u"pinned tab", u"pinned tabs"),
           u" you already have into the folder Arc keeps ",
           preview.moved_pins == 1 ? u"it" : u"them", u" in."})));
    }
    if (preview.kept_pins) {
      builder.AddParagraph(ui::DialogModelLabel(base::StrCat(
          {Count(preview.kept_pins, u"pinned tab is", u"pinned tabs are"),
           u" already in myArc and stay as they are."})));
    }
    builder.AddParagraph(
        ui::DialogModelLabel(u"Each Arc folder becomes a folder in its "
                             u"Space. Favorites become pinned tabs there, "
                             u"because the export doesn't mark them apart. "
                             u"Nothing opens until you click it.")
            .set_is_secondary());
  }
  if (preview.skipped_count) {
    builder.AddParagraph(
        ui::DialogModelLabel(List(preview.skipped, preview.skipped_count))
            .set_is_secondary()
            .set_allow_character_break(),
        base::StrCat({u"Links left out (",
                      base::NumberToString16(preview.skipped_count), u")"}));
  }
  if (preview.unrecognized_count) {
    builder.AddParagraph(
        ui::DialogModelLabel(
            List(preview.unrecognized, preview.unrecognized_count))
            .set_is_secondary()
            .set_allow_character_break(),
        base::StrCat({u"Lines myArc couldn't read (",
                      base::NumberToString16(preview.unrecognized_count),
                      u")"}));
  }
  const bool can_import = preview.ok && !noop;
  const bool press = can_import && !confirmed.is_null();
  if (can_import) {
    builder.AddOkButton(
        base::BindOnce(&Apply, profile, parent, inactive,
                       std::move(export_data), read.content_hash, outcome,
                       std::move(confirmed)),
        ui::DialogModel::Button::Params().SetLabel(u"Import"));
    builder.AddCancelButton(base::DoNothing());
  } else {
    builder.AddOkButton(base::DoNothing(),
                        ui::DialogModel::Button::Params().SetLabel(u"OK"));
  }
  views::Widget* widget = Show(builder.Build(), parent, inactive);
  if (!can_import && confirmed) {
    std::move(confirmed).Run(0);
    return;
  }
  if (press) {
    // Captures press Import once the sheet has settled.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<views::Widget> widget) {
              if (widget && widget->widget_delegate()) {
                if (auto* dialog =
                        widget->widget_delegate()->AsDialogDelegate()) {
                  dialog->AcceptDialog();
                }
              }
            },
            widget->GetWeakPtr()),
        base::Milliseconds(1500));
  }
}

void Read(base::WeakPtr<Profile> profile,
          gfx::NativeWindow parent,
          bool inactive,
          const base::FilePath& path,
          scoped_refptr<Outcome> outcome,
          Confirmed confirmed) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadExport, path),
      base::BindOnce(&ShowPreview, profile, parent, inactive,
                     std::move(outcome), std::move(confirmed)));
}

// Owns itself while the file picker is up.
class Picker : public ui::SelectFileDialog::Listener {
 public:
  Picker(Profile& profile,
         gfx::NativeWindow parent,
         scoped_refptr<Outcome> outcome)
      : profile_(profile.GetWeakPtr()),
        parent_(parent),
        outcome_(std::move(outcome)) {}
  Picker(const Picker&) = delete;
  Picker& operator=(const Picker&) = delete;
  ~Picker() override { dialog_->ListenerDestroyed(); }

  void Start() {
    dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(nullptr));
    ui::SelectFileDialog::FileTypeInfo types;
    types.extensions = {{FILE_PATH_LITERAL("md"), FILE_PATH_LITERAL("markdown"),
                         FILE_PATH_LITERAL("txt")}};
    types.include_all_files = true;
    dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, kTitle,
                        base::FilePath(), &types, 1,
                        base::FilePath::StringType(), parent_);
  }

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    Read(profile_, parent_, /*inactive=*/false, file.path(), outcome_,
         Confirmed());
    delete this;
  }
  void FileSelectionCanceled() override { delete this; }

 private:
  const base::WeakPtr<Profile> profile_;
  const gfx::NativeWindow parent_;
  const scoped_refptr<Outcome> outcome_;
  scoped_refptr<ui::SelectFileDialog> dialog_;
};

}  // namespace

void ImportFromArc(Profile& profile,
                   gfx::NativeWindow parent,
                   ArcImportDone done) {
  (new Picker(profile, parent,
              done ? base::MakeRefCounted<Outcome>(std::move(done)) : nullptr))
      ->Start();
}

void ImportArcFileForCapture(Profile& profile,
                             gfx::NativeWindow parent,
                             const base::FilePath& path,
                             Confirmed confirmed,
                             ArcImportDone done) {
  Read(profile.GetWeakPtr(), parent, /*inactive=*/true, path,
       done ? base::MakeRefCounted<Outcome>(std::move(done)) : nullptr,
       std::move(confirmed));
}

}  // namespace myarc
