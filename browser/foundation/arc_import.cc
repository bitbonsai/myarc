#include "myarc/arc_import.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/common/pref_names.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "crypto/hash.h"
#include "myarc/test_buildflags.h"
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
// The workspace file of a Profile and its bound (workspace_service.cc).
constexpr char kWorkspaceFile[] = "myarc-workspace-v1.json";
constexpr size_t kMaxWorkspaceBytes = 20 * 1024 * 1024;

using Confirmed = base::OnceCallback<void(uint64_t)>;

// Runs the caller's ArcImportDone. A commit runs it at once, before anything
// else, so the caller can save it before a kill. Otherwise it runs once the
// last step holding it lets go: the picker, the preview's buttons or the
// apply. Posted then, since the last release may happen while a dialog
// closes. Also holds the caller's report_closed until the report takes it,
// and runs it if no report ever shows.
class Outcome : public base::RefCounted<Outcome> {
 public:
  Outcome(ArcImportDone done, base::OnceClosure report_closed)
      : done_(std::move(done)), report_closed_(std::move(report_closed)) {}
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

  base::OnceClosure TakeReportClosed() { return std::move(report_closed_); }

 private:
  friend class base::RefCounted<Outcome>;
  ~Outcome() {
    if (done_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(done_), std::move(outcome_)));
    }
    if (report_closed_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(report_closed_));
    }
  }

  ArcImportDone done_;
  base::OnceClosure report_closed_;
  ArcImportOutcome outcome_;
};

struct ReadResult {
  std::string error;
  std::optional<rust::Box<foundation::ArcExport>> export_data;
  std::string content_hash;
  // Arc's sidebar file rather than its Markdown export.
  bool sidebar = false;
};

// Arc's sidebar file, which keeps Favorites apart from Space pins. Read only
// after the user picks it or presses Import under "Found your Arc data".
constexpr char kArcSidebarDir[] = "Library/Application Support/Arc";
constexpr char kArcSidebarName[] = "StorableSidebar.json";

// Arc's Local State, beside the sidebar file in `User Data`. Like Chrome's, it
// keeps each profile's name under `profile.info_cache`.
constexpr char kArcUserDataDir[] = "User Data";
constexpr char kArcLocalStateName[] = "Local State";

// A .json file is Arc's sidebar file; anything else is read as the Markdown
// export.
bool IsSidebarFile(const base::FilePath& path) {
  return path.MatchesExtension(FILE_PATH_LITERAL(".json"));
}

// Worker sequence: reads at most the export limit and parses. The file is
// only read, never written.
ReadResult ReadExport(const base::FilePath& path) {
  const bool sidebar = IsSidebarFile(path);
  const size_t max = sidebar ? foundation::arc_sidebar_max_bytes()
                             : foundation::arc_export_max_bytes();
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
  // The names of Arc's profiles, when a Local State sits next to the sidebar
  // file. Only read; a missing, large or unreadable one leaves them unnamed.
  std::string local_state;
  if (sidebar && !base::ReadFileToStringWithMaxSize(
                     path.DirName()
                         .AppendASCII(kArcUserDataDir)
                         .AppendASCII(kArcLocalStateName),
                     &local_state, foundation::arc_local_state_max_bytes())) {
    local_state.clear();
  }
  return {std::string(),
          WorkspaceService::ParseArcExport(base::as_byte_span(bytes), sidebar,
                                           base::as_byte_span(local_state)),
          base::HexEncodeLower(crypto::hash::Sha256(bytes)), sidebar};
}

// Worker sequence: the workspace of each Profile in `dirs` that isn't loaded,
// read from its file, which stays untouched. None for a Profile without one,
// which holds only the empty default Space.
std::vector<std::optional<foundation::StateSnapshot>> ReadWorkspaces(
    const std::vector<base::FilePath>& dirs) {
  std::vector<std::optional<foundation::StateSnapshot>> snapshots;
  for (const base::FilePath& dir : dirs) {
    std::string json;
    if (base::ReadFileToStringWithMaxSize(dir.AppendASCII(kWorkspaceFile),
                                          &json, kMaxWorkspaceBytes)) {
      snapshots.push_back(
          std::move(internal::DecodeWorkspaceMetadata(json).snapshot));
    } else {
      snapshots.push_back(std::nullopt);
    }
  }
  return snapshots;
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

std::u16string Utf16(const rust::String& text) {
  return base::UTF8ToUTF16(std::string_view(text.data(), text.size()));
}

// The first lines of `lines`, then "and N more" when `total` is larger.
std::u16string List(const std::vector<std::u16string>& lines, size_t total) {
  std::u16string text;
  for (size_t i = 0; i < lines.size() && i < kShownLines; ++i) {
    if (!text.empty()) {
      text += u"\n";
    }
    text += lines[i];
  }
  const size_t shown = std::min(lines.size(), kShownLines);
  if (total > shown) {
    base::StrAppend(
        &text, {u"\nand ", base::NumberToString16(total - shown), u" more"});
  }
  return text;
}

// What one or more parts add, move or keep.
struct Counts {
  size_t new_profiles = 0;
  size_t new_favorites = 0;
  size_t new_spaces = 0;
  size_t new_folders = 0;
  size_t new_pins = 0;
  size_t moved_pins = 0;
  size_t kept_pins = 0;

  void Add(const foundation::ArcImportPreview& preview) {
    new_favorites += preview.new_favorites;
    new_spaces += preview.new_spaces;
    new_folders += preview.new_folders;
    new_pins += preview.new_pins;
    moved_pins += preview.moved_pins;
    kept_pins += preview.kept_pins;
  }

  bool Writes() const {
    return new_favorites || new_spaces || new_folders || new_pins || moved_pins;
  }

  // "4 Spaces, 1 folder and 13 pinned tabs", or empty when nothing is new.
  std::u16string Additions() const {
    std::vector<std::u16string> adds;
    if (new_profiles) {
      adds.push_back(Count(new_profiles, u"Profile", u"Profiles"));
    }
    if (new_favorites) {
      adds.push_back(Count(new_favorites, u"Favorite", u"Favorites"));
    }
    if (new_spaces) {
      adds.push_back(Count(new_spaces, u"Space", u"Spaces"));
    }
    if (new_folders) {
      adds.push_back(Count(new_folders, u"folder", u"folders"));
    }
    if (new_pins) {
      adds.push_back(Count(new_pins, u"pinned tab", u"pinned tabs"));
    }
    return Join(adds);
  }
};

Counts CountsOf(const foundation::ArcImportPreview& preview) {
  Counts counts;
  counts.Add(preview);
  return counts;
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

// The Profile directory of the listed, regular myArc Profile named `name`,
// or empty when there is none.
base::FilePath FindProfile(const std::u16string& name) {
  ProfileManager* manager = g_browser_process->profile_manager();
  if (!manager) {
    return {};
  }
  for (const ProfileAttributesEntry* entry :
       manager->GetProfileAttributesStorage()
           .GetAllProfilesAttributesSortedForDisplay()) {
    if (!entry->IsOmitted() && !entry->IsEphemeral() &&
        entry->GetLocalProfileName() == name &&
        !IsProfileDirectoryMarkedForDeletion(entry->GetPath())) {
      return entry->GetPath();
    }
  }
  return {};
}

// Where one Arc profile's part of the export goes.
struct Target {
  // Arc's name for the profile and the myArc Profile's name; both empty for
  // the main profile, which goes into the Profile running the import.
  std::u16string arc_name;
  std::u16string myarc_name;
  // The existing myArc Profile it goes into, unless it's the main profile's.
  base::FilePath dir;
  // No myArc Profile has the name yet, so Import creates one.
  bool create = false;
  foundation::ArcImportPreview preview;
};

// One import from the moment its file is read: the preview, then Import
// writing each part into its Profile, then the report. Held by the callbacks
// of whichever step runs.
class Session : public base::RefCounted<Session> {
 public:
  Session(base::WeakPtr<Profile> profile,
          gfx::NativeWindow parent,
          bool inactive,
          scoped_refptr<Outcome> outcome,
          Confirmed confirmed,
          ReadResult read)
      : profile_(std::move(profile)),
        parent_(parent),
        inactive_(inactive),
        outcome_(std::move(outcome)),
        confirmed_(std::move(confirmed)),
        export_(std::move(*read.export_data)),
        content_hash_(std::move(read.content_hash)),
        sidebar_(read.sidebar) {}
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  // Finds each part's Profile, reads the workspaces of those not loaded and
  // shows the preview. Loads and creates nothing.
  void Start() {
    if (!profile_) {
      return;
    }
    std::vector<base::FilePath> unloaded;
    ProfileManager* manager = g_browser_process->profile_manager();
    for (const auto& part : foundation::arc_export_profiles(*export_)) {
      Target target{.arc_name = Utf16(part.arc_name),
                    .myarc_name = Utf16(part.myarc_name)};
      if (!target.myarc_name.empty()) {
        target.dir = FindProfile(target.myarc_name);
        // An import from before Arc's names were read made "Arc Profile 1";
        // keep using it rather than adding a second Profile.
        const std::u16string old_name = Utf16(part.old_myarc_name);
        if (target.dir.empty() && !old_name.empty()) {
          target.dir = FindProfile(old_name);
          if (!target.dir.empty()) {
            target.myarc_name = old_name;
          }
        }
        target.create = target.dir.empty();
        if (!target.create &&
            !(manager && manager->GetProfileByPath(target.dir))) {
          unloaded.push_back(target.dir);
        }
      }
      targets_.push_back(std::move(target));
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ReadWorkspaces, unloaded),
        base::BindOnce(&Session::ShowPreview, base::WrapRefCounted(this),
                       unloaded));
  }

 private:
  friend class base::RefCounted<Session>;
  ~Session() = default;

  // The preview of every part, against its Profile's workspace as it is now:
  // the live one, the one read from `unloaded`'s files, or a new Profile's.
  void ShowPreview(
      const std::vector<base::FilePath>& unloaded,
      std::vector<std::optional<foundation::StateSnapshot>> snapshots) {
    if (!profile_) {
      return;
    }
    ProfileManager* manager = g_browser_process->profile_manager();
    for (size_t part = 0; part < targets_.size(); ++part) {
      Target& target = targets_[part];
      Profile* loaded = part == 0 ? profile_.get()
                        : target.create || !manager
                            ? nullptr
                            : manager->GetProfileByPath(target.dir);
      if (loaded) {
        target.preview = WorkspaceService::ForProfile(*loaded).PreviewArcImport(
            *export_, part);
        continue;
      }
      // A new Profile, or one not loaded: its workspace from the file, or a
      // new Profile's.
      rust::Box<foundation::Workspace> workspace = foundation::new_workspace();
      const auto it = std::ranges::find(unloaded, target.dir);
      if (!target.create && it != unloaded.end()) {
        auto& snapshot = snapshots[it - unloaded.begin()];
        if (snapshot) {
          foundation::restore_snapshot(*workspace, std::move(*snapshot));
          snapshot.reset();
        }
      }
      target.preview =
          foundation::preview_arc_import(*workspace, *export_, part);
    }

    ui::DialogModel::Builder builder;
    builder.SetTitle(kTitle);
    Counts total;
    std::u16string error;
    for (const Target& target : targets_) {
      total.Add(target.preview);
      if (!target.preview.ok && error.empty()) {
        error = Utf16(target.preview.error);
      }
    }
    const bool noop = error.empty() && !total.Writes();
    if (!error.empty()) {
      builder.AddParagraph(ui::DialogModelLabel(
          base::StrCat({u"This file can't be imported. ", error})));
    } else if (noop) {
      builder.AddParagraph(ui::DialogModelLabel(
          u"Everything in this file is already in myArc."));
    } else if (targets_.size() == 1) {
      const std::u16string adds = total.Additions();
      if (!adds.empty()) {
        builder.AddParagraph(
            ui::DialogModelLabel(base::StrCat({u"Adds ", adds, u"."})));
      }
    } else {
      // One line per Arc profile, naming the myArc Profile it goes into.
      for (const Target& target : targets_) {
        builder.AddParagraph(ui::DialogModelLabel(Mapping(target)));
      }
    }
    if (error.empty() && !noop) {
      if (total.moved_pins) {
        builder.AddParagraph(ui::DialogModelLabel(base::StrCat(
            {u"Moves ", Count(total.moved_pins, u"pinned tab", u"pinned tabs"),
             u" you already have into the folder Arc keeps ",
             total.moved_pins == 1 ? u"it" : u"them", u" in."})));
      }
      if (total.kept_pins) {
        builder.AddParagraph(ui::DialogModelLabel(base::StrCat(
            {Count(total.kept_pins, u"pinned tab is", u"pinned tabs are"),
             u" already in myArc and stay as they are."})));
      }
      builder.AddParagraph(
          ui::DialogModelLabel(
              sidebar_
                  ? u"Favorites stay Favorites, pinned tabs keep their names, "
                    u"and each Arc folder becomes a folder in its Space. A "
                    u"folder inside a folder joins the outer one, because "
                    u"myArc's folders don't nest yet. Nothing opens until you "
                    u"click it."
                  : u"Each Arc folder becomes a folder in its Space. Favorites "
                    u"become pinned tabs there, because the export doesn't "
                    u"mark them apart. Nothing opens until you click it.")
              .set_is_secondary());
    }
    std::vector<std::u16string> skipped;
    size_t skipped_count = 0;
    for (const Target& target : targets_) {
      skipped_count += target.preview.skipped_count;
      for (const auto& line : target.preview.skipped) {
        skipped.push_back(Utf16(line));
      }
    }
    if (skipped_count) {
      builder.AddParagraph(
          ui::DialogModelLabel(List(skipped, skipped_count))
              .set_is_secondary()
              .set_allow_character_break(),
          base::StrCat({u"Links left out (",
                        base::NumberToString16(skipped_count), u")"}));
    }
    const foundation::ArcImportPreview& main = targets_[0].preview;
    if (main.unrecognized_count) {
      std::vector<std::u16string> lines;
      for (const auto& line : main.unrecognized) {
        lines.push_back(Utf16(line));
      }
      builder.AddParagraph(
          ui::DialogModelLabel(List(lines, main.unrecognized_count))
              .set_is_secondary()
              .set_allow_character_break(),
          base::StrCat({u"Lines myArc couldn't read (",
                        base::NumberToString16(main.unrecognized_count),
                        u")"}));
    }
    const bool can_import = error.empty() && !noop;
    const bool press = can_import && !confirmed_.is_null();
    if (can_import) {
      builder.AddOkButton(
          base::BindOnce(&Session::Apply, base::WrapRefCounted(this)),
          ui::DialogModel::Button::Params().SetLabel(u"Import"));
      builder.AddCancelButton(base::DoNothing());
    } else {
      builder.AddOkButton(base::DoNothing(),
                          ui::DialogModel::Button::Params().SetLabel(u"OK"));
    }
    views::Widget* widget = Show(builder.Build(), parent_, inactive_);
    if (!can_import && confirmed_) {
      std::move(confirmed_).Run(0);
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

  // "Arc profile 'Work' → new myArc Profile 'Work': adds 2 pinned tabs."
  static std::u16string Mapping(const Target& target) {
    std::u16string text =
        target.myarc_name.empty()
            ? std::u16string(u"Main Arc profile → this myArc Profile")
            : base::StrCat(
                  {u"Arc profile '", target.arc_name, u"' → ",
                   target.create ? u"new myArc Profile '" : u"myArc Profile '",
                   target.myarc_name, u"'"});
    const std::u16string adds = CountsOf(target.preview).Additions();
    return base::StrCat({text, adds.empty() ? u": nothing new." : u": adds ",
                         adds, adds.empty() ? u"" : u"."});
  }

  // Import: the main profile's part right away, then each other part once
  // its Profile is loaded or created, one after another.
  void Apply() {
    if (!profile_) {
      return;
    }
    auto result = WorkspaceService::ForProfile(*profile_).ApplyArcImport(
        *export_, 0, false);
    if (!result.ok) {
      // Post: the preview is still closing.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ShowMessage,
                                    base::StrCat({u"Nothing was imported. ",
                                                  Utf16(result.error)}),
                                    parent_, inactive_));
      if (confirmed_) {
        std::move(confirmed_).Run(0);
      }
      return;
    }
    added_.Add(result);
    first_space_ = result.first_space;
    if (outcome_) {
      outcome_->Commit(content_hash_, first_space_);
    }
    ApplyPart(1);
  }

  void ApplyPart(size_t part) {
    // Parts with nothing new need no Profile opened, let alone created.
    while (part < targets_.size() &&
           !CountsOf(targets_[part].preview).Writes()) {
      ++part;
    }
    if (part >= targets_.size()) {
      // Post: the preview may still be closing.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&Session::Finish, base::WrapRefCounted(this)));
      return;
    }
    ProfileManager* manager = g_browser_process->profile_manager();
    const Target& target = targets_[part];
    auto opened =
        base::BindOnce(&Session::Opened, base::WrapRefCounted(this), part);
    if (!manager) {
      std::move(opened).Run(nullptr);
    } else if (target.create) {
      ProfileManager::CreateMultiProfileAsync(
          target.myarc_name,
          manager->GetProfileAttributesStorage()
              .ChooseAvatarIconIndexForNewProfile(),
          /*is_hidden=*/false, std::move(opened));
    } else if (Profile* loaded = manager->GetProfileByPath(target.dir)) {
      std::move(opened).Run(loaded);
    } else if (!manager->LoadProfileByPath(target.dir, /*incognito=*/false,
                                           std::move(opened))) {
      Failed(part);
    }
  }

  void Opened(size_t part, Profile* profile) {
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive =
        profile ? ScopedProfileKeepAlive::TryAcquire(
                      profile, ProfileKeepAliveOrigin::kProfileCreationFlow)
                : nullptr;
    if (!keep_alive) {
      Failed(part);
      return;
    }
    if (targets_[part].create) {
      // Arc has no Profile picker, so launch never shows Chromium's once
      // there are several Profiles, as with New Profile…. A value set
      // before stays.
      PrefService* local_state = g_browser_process->local_state();
      if (local_state && !local_state->HasPrefPath(
                             prefs::kBrowserShowProfilePickerOnStartup)) {
        local_state->SetBoolean(prefs::kBrowserShowProfilePickerOnStartup,
                                false);
      }
    }
    // Waits once if the Profile's workspace is still loading.
    WorkspaceService::ForProfile(*profile).WhenLoaded(
        base::BindOnce(&Session::Loaded, base::WrapRefCounted(this), part,
                       profile->GetWeakPtr(), std::move(keep_alive)));
  }

  void Loaded(size_t part,
              base::WeakPtr<Profile> profile,
              std::unique_ptr<ScopedProfileKeepAlive>) {
    if (!profile) {
      Failed(part);
      return;
    }
    auto result = WorkspaceService::ForProfile(*profile).ApplyArcImport(
        *export_, part, targets_[part].create);
    if (result.ok) {
      added_.Add(result);
      added_.new_profiles += targets_[part].create;
    } else {
      failed_.push_back(targets_[part].myarc_name);
    }
    ApplyPart(part + 1);
  }

  void Failed(size_t part) {
    failed_.push_back(targets_[part].myarc_name);
    ApplyPart(part + 1);
  }

  // The done state: what the import added, then every kind of entry it left
  // out or changed, with counts from Rust.
  void Finish() {
    ui::DialogModel::Builder builder;
    base::OnceClosure closed =
        outcome_ ? outcome_->TakeReportClosed() : base::OnceClosure();
    const bool waited_on = !closed.is_null();
    if (waited_on) {
      builder.SetDialogDestroyingCallback(std::move(closed));
    }
    builder.SetTitle(kTitle);
    const std::u16string adds = added_.Additions();
    builder.AddParagraph(ui::DialogModelLabel(
        adds.empty() ? std::u16string(u"Import finished.")
                     : base::StrCat({u"Import finished. Added ", adds, u"."})));
    if (added_.moved_pins) {
      builder.AddParagraph(ui::DialogModelLabel(base::StrCat(
          {u"Moved ", Count(added_.moved_pins, u"pinned tab", u"pinned tabs"),
           u" into ", added_.moved_pins == 1 ? u"its" : u"their",
           u" Arc folder."})));
    }
    if (!failed_.empty()) {
      std::vector<std::u16string> names;
      for (const std::u16string& name : failed_) {
        names.push_back(base::StrCat({u"'", name, u"'"}));
      }
      builder.AddParagraph(ui::DialogModelLabel(base::StrCat(
          {u"myArc couldn't open or create the Profile",
           failed_.size() == 1 ? u" " : u"s ", Join(names),
           u", so nothing went there. Import the file again to try once "
           u"more."})));
    }
    const auto report = foundation::arc_import_report(*export_);
    std::vector<std::u16string> lines;
    for (const auto& line : report.lines) {
      lines.push_back(Utf16(line));
    }
    if (lines.empty()) {
      builder.AddParagraph(ui::DialogModelLabel(
          u"Nothing in the file was left out or changed."));
    } else {
      builder.AddParagraph(
          ui::DialogModelLabel(List(lines, lines.size())).set_is_secondary(),
          u"Left out or changed");
    }
    builder.AddParagraph(
        ui::DialogModelLabel(Utf16(report.note)).set_is_secondary());
#if BUILDFLAG(MYARC_TEST_HOOKS)
    // Background runs can't read the sheet, so log what it says.
    std::string logged;
    for (const auto& line : report.lines) {
      logged += " [" + std::string(line) + "]";
    }
    LOG(WARNING) << "Test Arc import report: added=\""
                 << base::UTF16ToUTF8(adds) << "\" lines=" << logged
                 << " failed=" << failed_.size() << " note=\""
                 << std::string(report.note) << "\"";
#endif  // BUILDFLAG(MYARC_TEST_HOOKS)
    builder.AddOkButton(base::DoNothing(),
                        ui::DialogModel::Button::Params().SetLabel(u"OK"));
    views::Widget* widget = Show(builder.Build(), parent_, inactive_);
    if (inactive_ && waited_on) {
      // A capture can't press OK, and its caller waits for the report to
      // close.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<views::Widget> widget) {
                if (widget) {
                  widget->Close();
                }
              },
              widget->GetWeakPtr()),
          base::Milliseconds(2000));
    }
    if (confirmed_) {
      std::move(confirmed_).Run(first_space_);
    }
  }

  const base::WeakPtr<Profile> profile_;
  const gfx::NativeWindow parent_;
  const bool inactive_;
  const scoped_refptr<Outcome> outcome_;
  Confirmed confirmed_;
  rust::Box<foundation::ArcExport> export_;
  const std::string content_hash_;
  const bool sidebar_;
  std::vector<Target> targets_;
  // What Import has written so far, and the myArc Profiles it couldn't open.
  Counts added_;
  std::vector<std::u16string> failed_;
  uint64_t first_space_ = 0;
};

void Begin(base::WeakPtr<Profile> profile,
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
  base::MakeRefCounted<Session>(std::move(profile), parent, inactive,
                                std::move(outcome), std::move(confirmed),
                                std::move(read))
      ->Start();
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
      base::BindOnce(&Begin, profile, parent, inactive, std::move(outcome),
                     std::move(confirmed)));
}

scoped_refptr<Outcome> MakeOutcome(ArcImportDone done,
                                   base::OnceClosure report_closed) {
  return done ? base::MakeRefCounted<Outcome>(std::move(done),
                                              std::move(report_closed))
              : nullptr;
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

  // Looks for Arc's sidebar file, without reading it, so the picker can open
  // on it.
  void Start() {
    FindArcData(base::BindOnce(&Picker::Open, base::Unretained(this)));
  }

  void Open(const base::FilePath& sidebar) {
    dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(nullptr));
    ui::SelectFileDialog::FileTypeInfo types;
    types.extensions = {{FILE_PATH_LITERAL("md"), FILE_PATH_LITERAL("markdown"),
                         FILE_PATH_LITERAL("txt")},
                        {FILE_PATH_LITERAL("json")}};
    types.include_all_files = true;
    // The picker opens on Arc's folder with the sidebar file selected; the
    // user still confirms it.
    dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE, kTitle, sidebar,
                        &types, sidebar.empty() ? 1 : 2,
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
                   ArcImportDone done,
                   base::OnceClosure report_closed) {
  (new Picker(profile, parent,
              MakeOutcome(std::move(done), std::move(report_closed))))
      ->Start();
}

void FindArcData(base::OnceCallback<void(const base::FilePath&)> found) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce([] {
        const base::FilePath file =
            base::GetHomeDir().Append(kArcSidebarDir).Append(kArcSidebarName);
        return base::PathExists(file) ? file : base::FilePath();
      }),
      std::move(found));
}

void ImportFoundArcData(Profile& profile,
                        gfx::NativeWindow parent,
                        const base::FilePath& path,
                        ArcImportDone done,
                        base::OnceClosure report_closed) {
  Read(profile.GetWeakPtr(), parent, /*inactive=*/false, path,
       MakeOutcome(std::move(done), std::move(report_closed)), Confirmed());
}

void ImportArcFileForCapture(Profile& profile,
                             gfx::NativeWindow parent,
                             const base::FilePath& path,
                             Confirmed confirmed,
                             ArcImportDone done,
                             base::OnceClosure report_closed) {
  Read(profile.GetWeakPtr(), parent, /*inactive=*/true, path,
       MakeOutcome(std::move(done), std::move(report_closed)),
       std::move(confirmed));
}

}  // namespace myarc
