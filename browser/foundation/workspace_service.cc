#include "myarc/workspace_service.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "myarc/spaces_index.h"
#include "url/gurl.h"

namespace myarc {
namespace {

const char kWorkspaceServiceKey = 0;
// Covers the core maximum (1024 saved URLs and 500 archived URLs of 8192
// bytes), including JSON escaping. JSONReader's input and depth limits bound
// its intermediate tree; per-field/count checks below precede allocation of
// owned bridge records.
constexpr size_t kMaxMetadataBytes = 20 * 1024 * 1024;
constexpr size_t kMaxJsonDepth = 8;
// Auto Archive's default "Archive tabs after", which files leave unwritten.
constexpr uint64_t kDefaultArchiveIntervalSecs = 12 * 60 * 60;

scoped_refptr<base::SequencedTaskRunner> WorkspaceIoRunner() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Serialize unload/final-write/reload even after the old service is gone.
  // Sequence-local ownership also avoids retaining a dead test ThreadPool.
  static base::SequenceLocalStorageSlot<
      scoped_refptr<base::SequencedTaskRunner>>
      slot;
  auto& runner = slot.GetOrCreateValue();
  if (!runner) {
    runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }
  return runner;
}

foundation::MutationResult Failure(std::string_view error) {
  return {false, 0, rust::String(error.data(), error.size())};
}

bool ReadId(const base::DictValue& dict, std::string_view key, uint64_t& id) {
  const auto* text = dict.FindString(key);
  return text && text->size() <= 20 && base::StringToUint64(*text, &id) &&
         base::NumberToString(id) == *text;
}

internal::MetadataResult InvalidMetadata() {
  return {std::nullopt,
          "Workspace metadata is corrupt, unsupported or exceeds limits. "
          "File preserved. Recover it or move it aside, then retry loading."};
}

internal::MetadataResult ReadMetadata(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    if (file.error_details() == base::File::FILE_ERROR_NOT_FOUND) {
      return {};  // First run, not a failed decode.
    }
    return {
        std::nullopt,
        "Cannot open workspace metadata. Check permissions and retry loading."};
  }
  const int64_t size = file.GetLength();
  if (size < 0 || static_cast<uint64_t>(size) > kMaxMetadataBytes) {
    return InvalidMetadata();
  }
  std::string json(static_cast<size_t>(size), '\0');
  std::array<uint8_t, 1> trailing;
  if (!file.ReadAtCurrentPosAndCheck(base::as_writable_byte_span(json)) ||
      file.ReadAtCurrentPos(trailing) != 0) {
    return {std::nullopt,
            "Workspace metadata read failed or changed while "
            "reading. Retry loading."};
  }
  return internal::DecodeWorkspaceMetadata(json);
}

bool WriteMetadata(const base::FilePath& path,
                   foundation::StateSnapshot snapshot) {
  const auto json = internal::EncodeWorkspaceMetadata(snapshot);
  const bool ok =
      json && base::ImportantFileWriter::WriteFileAtomically(path, *json);
  // The service may have died or the UI loop stopped before the reply runs.
  if (!ok) {
    LOG(ERROR) << "Workspace metadata write failed. Check disk space and "
                  "profile-directory permissions, then retry saving.";
  }
  return ok;
}

// GURL is the parser at both untrusted restore and saved-URL mutation
// boundaries.
std::optional<std::string> CanonicalUrl(std::string_view text) {
  if (text.size() > foundation::bridge_limits().url_bytes)
    return std::nullopt;
  const GURL url(text);
  if (!url.is_valid() || url.has_username() || url.has_password() ||
      !(url.SchemeIsHTTPOrHTTPS() || url.spec() == "about:blank") ||
      url.spec().size() > foundation::bridge_limits().url_bytes)
    return std::nullopt;
  return url.spec();
}

bool BoundedText(std::string_view text, size_t limit) {
  return text.size() <= limit && base::IsStringUTF8(text);
}

// Wall-clock seconds for Archived Tabs; a time before 1970 reads as 0 and
// base::Time::Max() as the largest value.
uint64_t Secs(base::Time time) {
  if (time.is_max())
    return std::numeric_limits<uint64_t>::max();
  const time_t secs = time.is_null() ? 0 : time.ToTimeT();
  return secs > 0 ? static_cast<uint64_t>(secs) : 0;
}

uint64_t NowSecs() {
  return Secs(base::Time::Now());
}

}  // namespace

namespace internal {

bool CanonicalizeUrls(foundation::StateSnapshot& snapshot) {
  const auto limits = foundation::bridge_limits();
  if (snapshot.spaces.size() > limits.spaces ||
      snapshot.items.size() > limits.items ||
      snapshot.windows.size() > limits.windows) {
    return false;
  }
  for (auto& item : snapshot.items) {
    if (!item.has_url) {
      if (!item.canonical_url.empty())
        return false;
      continue;
    }
    auto url = CanonicalUrl(
        std::string_view(item.canonical_url.data(), item.canonical_url.size()));
    if (!url)
      return false;
    item.canonical_url = *url;
  }
  // A bad Archived Tab is dropped rather than failing the snapshot; only the
  // newest entries within the limit stay.
  rust::Vec<foundation::ArchivedTabRecord> archived;
  const size_t skip = snapshot.archived.size() > limits.archived_tabs
                          ? snapshot.archived.size() - limits.archived_tabs
                          : 0;
  for (size_t i = skip; i < snapshot.archived.size(); ++i) {
    auto& tab = snapshot.archived[i];
    auto url = CanonicalUrl(std::string_view(tab.url.data(), tab.url.size()));
    if (!url)
      continue;
    tab.url = *url;
    archived.push_back(std::move(tab));
  }
  snapshot.archived = std::move(archived);
  return true;
}

std::optional<std::string> EncodeWorkspaceMetadata(
    const foundation::StateSnapshot& snapshot) {
  base::DictValue root;
  root.Set("version", static_cast<int>(snapshot.version));
  root.Set("profile_id", base::NumberToString(snapshot.profile_id));
  root.Set("next_space_id", base::NumberToString(snapshot.next_space_id));
  root.Set("next_item_id", base::NumberToString(snapshot.next_item_id));
  root.Set("next_window_id", base::NumberToString(snapshot.next_window_id));
  base::ListValue spaces;
  for (const auto& space : snapshot.spaces) {
    base::DictValue record;
    record.Set("id", base::NumberToString(space.id));
    record.Set("profile_id", base::NumberToString(space.profile_id));
    record.Set("name", std::string(space.name));
    // Builds without Space icons reject the keys, so write them only when set.
    if (space.icon_kind != foundation::SpaceIconKind::Unset) {
      record.Set("icon_kind", static_cast<int>(space.icon_kind));
      record.Set("icon", std::string(space.icon));
    }
    // Likewise for themes.
    if (space.theme.set) {
      record.Set("theme_color", static_cast<int>(space.theme.color));
      record.Set("theme_opacity", static_cast<int>(space.theme.opacity));
    }
    // And for the agent Space's mark.
    if (space.agent) {
      record.Set("agent", true);
    }
    spaces.Append(std::move(record));
  }
  root.Set("spaces", std::move(spaces));
  base::ListValue items;
  for (const auto& item : snapshot.items) {
    base::DictValue record;
    record.Set("id", base::NumberToString(item.id));
    record.Set("placement", static_cast<int>(item.placement));
    record.Set("owner_id", base::NumberToString(item.owner_id));
    record.Set("has_url", item.has_url);
    record.Set("canonical_url", std::string(item.canonical_url));
    // Builds without saved titles reject the key, so write it only when set.
    if (!item.title.empty())
      record.Set("title", std::string(item.title));
    // Builds without renames reject the key too.
    if (!item.name.empty())
      record.Set("name", std::string(item.name));
    // Builds without Auto Archive reject the key; saved items never carry it.
    if (item.last_viewed_secs)
      record.Set("viewed_secs", base::NumberToString(item.last_viewed_secs));
    items.Append(std::move(record));
  }
  root.Set("items", std::move(items));
  base::ListValue windows;
  for (const auto& window : snapshot.windows) {
    base::DictValue record;
    record.Set("id", base::NumberToString(window.id));
    record.Set("space_id", base::NumberToString(window.space_id));
    record.Set("selected_item_id",
               base::NumberToString(window.selected_item_id));
    windows.Append(std::move(record));
  }
  root.Set("windows", std::move(windows));
  // Builds without Archived Tabs reject the key, so write it only when set.
  if (!snapshot.archived.empty()) {
    base::ListValue archived;
    for (const auto& tab : snapshot.archived) {
      base::DictValue record;
      record.Set("id", base::NumberToString(tab.id));
      record.Set("space_id", base::NumberToString(tab.space_id));
      record.Set("url", std::string(tab.url));
      record.Set("title", std::string(tab.title));
      record.Set("archived_secs", base::NumberToString(tab.archived_secs));
      archived.Append(std::move(record));
    }
    root.Set("archived", std::move(archived));
  }
  // Older builds ignore folders, which then show their Pins ungrouped; write
  // the keys only when there are folders.
  if (!snapshot.folders.empty()) {
    base::ListValue folders;
    for (const auto& folder : snapshot.folders) {
      base::DictValue record;
      record.Set("id", base::NumberToString(folder.id));
      record.Set("space_id", base::NumberToString(folder.space_id));
      record.Set("name", std::string(folder.name));
      record.Set("open", folder.open);
      base::ListValue pins;
      for (const uint64_t item : folder.items) {
        pins.Append(base::NumberToString(item));
      }
      record.Set("items", std::move(pins));
      folders.Append(std::move(record));
    }
    root.Set("folders", std::move(folders));
    root.Set("next_folder_id", base::NumberToString(snapshot.next_folder_id));
  }
  // Older builds ignore splits, which then show their tabs as normal rows;
  // write the key only when there are splits.
  if (!snapshot.splits.empty()) {
    base::ListValue splits;
    for (const auto& split : snapshot.splits) {
      base::DictValue record;
      record.Set("left", base::NumberToString(split.left));
      record.Set("right", base::NumberToString(split.right));
      splits.Append(std::move(record));
    }
    root.Set("splits", std::move(splits));
  }
  // Builds without Auto Archive reject the key, so write it only when set.
  if (snapshot.open_secs)
    root.Set("open_secs", base::NumberToString(snapshot.open_secs));
  // Builds without the archive timer reject the key, so write it only when it
  // is not the 12-hour default.
  if (snapshot.archive_interval_secs != kDefaultArchiveIntervalSecs) {
    root.Set("archive_interval_secs",
             base::NumberToString(snapshot.archive_interval_secs));
  }
  auto json = base::WriteJson(root);
  if (json && json->size() <= kMaxMetadataBytes) {
    return json;
  }
  return std::nullopt;
}

MetadataResult DecodeWorkspaceMetadata(std::string_view json) {
  if (json.size() > kMaxMetadataBytes || !base::IsStringUTF8(json)) {
    return InvalidMetadata();
  }
  auto root =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC, kMaxJsonDepth);
  const auto limits = foundation::bridge_limits();
  // Unknown keys are ignored at every level, so a newer build can add
  // optional fields without breaking older ones. A change an older build must
  // not ignore needs a new version.
  if (!root || root->FindInt("version") != static_cast<int>(limits.version)) {
    return InvalidMetadata();
  }
  const auto* spaces = root->FindList("spaces");
  const auto* items = root->FindList("items");
  const auto* windows = root->FindList("windows");
  // Files written before Archived Tabs existed have no "archived" key. A bad
  // archive never fails the file: it is a convenience, the rest is not.
  const auto* archived_value = root->Find("archived");
  const auto* archived = archived_value ? archived_value->GetIfList() : nullptr;
  // Nor does a bad Auto Archive clock (or one from before Auto Archive): it
  // reads as zero, and the next pass restamps every view, so nothing archives
  // early.
  const bool has_open_secs = root->contains("open_secs");
  // A bad archive timer reads as the default; Rust restore does the same for
  // any value that is not one of the four choices.
  const bool has_interval = root->contains("archive_interval_secs");
  if (!spaces || spaces->size() > limits.spaces || !items ||
      items->size() > limits.items || !windows ||
      windows->size() > limits.windows) {
    return InvalidMetadata();
  }
  foundation::StateSnapshot snapshot;
  snapshot.version = limits.version;
  if (!ReadId(*root, "profile_id", snapshot.profile_id) ||
      !ReadId(*root, "next_space_id", snapshot.next_space_id) ||
      !ReadId(*root, "next_item_id", snapshot.next_item_id) ||
      !ReadId(*root, "next_window_id", snapshot.next_window_id)) {
    return InvalidMetadata();
  }
  if (has_open_secs && !ReadId(*root, "open_secs", snapshot.open_secs))
    snapshot.open_secs = 0;
  if (has_interval && !ReadId(*root, "archive_interval_secs",
                              snapshot.archive_interval_secs)) {
    snapshot.archive_interval_secs = 0;
  }
  for (const auto& value : *spaces) {
    const auto* record = value.GetIfDict();
    foundation::SpaceRecord space;
    const auto* name = record ? record->FindString("name") : nullptr;
    if (!record || !name || !BoundedText(*name, limits.name_bytes) ||
        !ReadId(*record, "id", space.id) ||
        !ReadId(*record, "profile_id", space.profile_id)) {
      return InvalidMetadata();
    }
    space.name = *name;
    // Files written before Space icons have neither key. A bad icon reads as
    // none rather than failing the file: it is cosmetic, and Rust restore
    // drops one it rejects the same way.
    const auto icon_kind = record->FindInt("icon_kind");
    const auto* icon = record->FindString("icon");
    space.icon_kind = foundation::SpaceIconKind::Unset;
    if (icon_kind && icon && BoundedText(*icon, limits.icon_bytes) &&
        (*icon_kind == static_cast<int>(foundation::SpaceIconKind::Emoji) ||
         *icon_kind == static_cast<int>(foundation::SpaceIconKind::Named))) {
      space.icon_kind = static_cast<foundation::SpaceIconKind>(*icon_kind);
      space.icon = *icon;
    }
    // Files written before themes have no theme keys; a bad theme reads as
    // none, like a bad icon.
    const auto theme_color = record->FindInt("theme_color");
    const auto theme_opacity = record->FindInt("theme_opacity");
    space.theme = {};
    if (theme_color && theme_opacity && *theme_color >= 0 &&
        *theme_color <= 0xFFFFFF && *theme_opacity >= 0 &&
        *theme_opacity <= 100) {
      space.theme = {.set = true,
                     .color = static_cast<uint32_t>(*theme_color),
                     .opacity = static_cast<uint8_t>(*theme_opacity)};
    }
    // Files written before the agent Space have no "agent" key. Rust restore
    // keeps the first mark per Profile.
    space.agent = record->FindBool("agent").value_or(false);
    snapshot.spaces.push_back(std::move(space));
  }
  for (const auto& value : *items) {
    const auto* record = value.GetIfDict();
    // Files written before saved titles or Auto Archive existed have no
    // "title" or "viewed_secs" key.
    if (!record) {
      return InvalidMetadata();
    }
    const auto kind = record->FindInt("placement");
    const auto has_url = record->FindBool("has_url");
    const auto* url = record->FindString("canonical_url");
    const auto* title = record->FindString("title");
    const bool viewed = record->contains("viewed_secs");
    foundation::ItemRecord item;
    if ((record->contains("title") && !title) ||
        (title && !BoundedText(*title, limits.title_bytes)) || !kind ||
        *kind < static_cast<int>(foundation::PlacementKind::Temporary) ||
        *kind > static_cast<int>(foundation::PlacementKind::Favorite) ||
        !has_url || !url || !BoundedText(*url, limits.url_bytes) ||
        (!*has_url && !url->empty()) || !ReadId(*record, "id", item.id) ||
        !ReadId(*record, "owner_id", item.owner_id)) {
      return InvalidMetadata();
    }
    // A bad view time reads as unrecorded: the next pass restamps it.
    if (viewed && !ReadId(*record, "viewed_secs", item.last_viewed_secs))
      item.last_viewed_secs = 0;
    item.placement = static_cast<foundation::PlacementKind>(*kind);
    item.has_url = *has_url;
    item.canonical_url = *url;
    if (title)
      item.title = *title;
    // A name is cosmetic: a bad one reads as none, as Rust restore does.
    if (const auto* name = record->FindString("name");
        name && BoundedText(*name, limits.title_bytes)) {
      item.name = *name;
    }
    snapshot.items.push_back(std::move(item));
  }
  for (const auto& value : *windows) {
    const auto* record = value.GetIfDict();
    foundation::WindowRecord window;
    if (!record || !ReadId(*record, "id", window.id) ||
        !ReadId(*record, "space_id", window.space_id) ||
        !ReadId(*record, "selected_item_id", window.selected_item_id)) {
      return InvalidMetadata();
    }
    snapshot.windows.push_back(std::move(window));
  }
  if (archived) {
    // Oldest first, so the newest entries within the limit stay.
    const size_t skip = archived->size() > limits.archived_tabs
                            ? archived->size() - limits.archived_tabs
                            : 0;
    for (size_t i = skip; i < archived->size(); ++i) {
      const auto* record = (*archived)[i].GetIfDict();
      const auto* url = record ? record->FindString("url") : nullptr;
      const auto* title = record ? record->FindString("title") : nullptr;
      foundation::ArchivedTabRecord tab;
      if (!record || !url ||
          !BoundedText(*url, limits.url_bytes) || !title ||
          !BoundedText(*title, limits.title_bytes) ||
          !ReadId(*record, "id", tab.id) ||
          !ReadId(*record, "space_id", tab.space_id) ||
          !ReadId(*record, "archived_secs", tab.archived_secs)) {
        continue;
      }
      tab.url = *url;
      tab.title = *title;
      snapshot.archived.push_back(std::move(tab));
    }
  }
  // Files written before folders have none. A bad folder or entry is dropped
  // rather than failing the file, and its Pins show ungrouped; Rust restore
  // drops what this misses.
  snapshot.next_folder_id = 1;
  if (const auto* folders = root->FindList("folders")) {
    if (!ReadId(*root, "next_folder_id", snapshot.next_folder_id))
      snapshot.next_folder_id = 1;
    for (const auto& value : *folders) {
      if (snapshot.folders.size() >= limits.folders)
        break;
      const auto* record = value.GetIfDict();
      const auto* name = record ? record->FindString("name") : nullptr;
      const auto* entries = record ? record->FindList("items") : nullptr;
      foundation::FolderRecord folder;
      if (!record || !name || !BoundedText(*name, limits.name_bytes) ||
          !entries || entries->size() > limits.items ||
          !ReadId(*record, "id", folder.id) ||
          !ReadId(*record, "space_id", folder.space_id)) {
        continue;
      }
      folder.name = *name;
      folder.open = record->FindBool("open").value_or(true);
      for (const auto& entry : *entries) {
        uint64_t item = 0;
        const auto* text = entry.GetIfString();
        if (text && base::StringToUint64(*text, &item) && item)
          folder.items.push_back(item);
      }
      snapshot.folders.push_back(std::move(folder));
    }
  }
  // Files written before Split View have none. A bad split is dropped and its
  // tabs show as normal rows; Rust restore drops what this misses.
  if (const auto* splits = root->FindList("splits")) {
    for (const auto& value : *splits) {
      if (snapshot.splits.size() >= limits.splits)
        break;
      const auto* record = value.GetIfDict();
      foundation::SplitRecord split;
      if (record && ReadId(*record, "left", split.left) &&
          ReadId(*record, "right", split.right)) {
        snapshot.splits.push_back(split);
      }
    }
  }
  if (!CanonicalizeUrls(snapshot))
    return InvalidMetadata();
  return {std::move(snapshot), {}};
}

}  // namespace internal

WorkspaceService& WorkspaceService::ForProfile(Profile& profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* service = static_cast<WorkspaceService*>(
      profile.GetUserData(&kWorkspaceServiceKey));
  if (!service) {
    auto owned =
        std::unique_ptr<WorkspaceService>(new WorkspaceService(profile));
    service = owned.get();
    profile.SetUserData(&kWorkspaceServiceKey, std::move(owned));
    if (!service->ephemeral_) {
      service->StartLoad();
    }
  }
  return *service;
}

WorkspaceService::WorkspaceService(Profile& profile)
    : ephemeral_(profile.IsOffTheRecord() || profile.IsGuestSession() ||
                 profile.IsSystemProfile()),
      profile_dir_(profile.GetBaseName().AsUTF8Unsafe()),
      metadata_path_(ephemeral_ ? base::FilePath()
                                : profile.GetPath().AppendASCII(
                                      "myarc-workspace-v1.json")),
      io_runner_(ephemeral_ ? nullptr : WorkspaceIoRunner()),
      workspace_(foundation::new_workspace()),
      storage_state_(ephemeral_ ? StorageState::kEphemeral
                                : StorageState::kLoading) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

WorkspaceService::~WorkspaceService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Managed Profiles die in StartTearDown, before ThreadPool::Shutdown.
  FlushForShutdown();
}

void WorkspaceService::FlushForShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ephemeral_ || !CanMutate() || !dirty_) {
    return;
  }
  // W1 may still be in flight. Queue W2 on the same runner without waiting for
  // its UI reply, which can be cancelled by destruction or normal shutdown.
  if (!io_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&WriteMetadata), metadata_path_,
                         foundation::export_snapshot(*workspace_)))) {
    LOG(ERROR) << "Final workspace metadata write could not be scheduled. "
                  "Keep the profile open and retry saving before closing.";
  }
}

WorkspaceService::StorageState WorkspaceService::storage_state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_state_;
}

std::string WorkspaceService::storage_error() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_error_;
}

base::CallbackListSubscription WorkspaceService::Subscribe(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return changed_.Add(std::move(callback));
}

bool WorkspaceService::CanMutate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_state_ != StorageState::kLoading &&
         storage_state_ != StorageState::kLoadError;
}

void WorkspaceService::StartLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  storage_state_ = StorageState::kLoading;
  storage_error_.clear();
  if (!io_runner_->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ReadMetadata, metadata_path_),
          base::BindOnce(&WorkspaceService::OnLoaded,
                         weak_factory_.GetWeakPtr()))) {
    storage_state_ = StorageState::kLoadError;
    storage_error_ = "Workspace load could not be scheduled. Retry loading.";
  }
}

void WorkspaceService::WhenLoaded(base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_state_ == StorageState::kLoading) {
    when_loaded_.push_back(std::move(done));
  } else {
    std::move(done).Run();
  }
}

void WorkspaceService::RetryLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_state_ == StorageState::kLoadError) {
    StartLoad();
    changed_.Notify();
  }
}

void WorkspaceService::OnLoaded(internal::MetadataResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.snapshot) {
    auto restored =
        foundation::restore_snapshot(*workspace_, std::move(*result.snapshot));
    if (!restored.ok) {
      result.error = std::string(restored.error) +
                     ". Metadata preserved. Recover file and retry loading.";
    } else if (foundation::prune_archived_tabs(*workspace_, NowSecs()).id) {
      // Entries that expired while the browser was closed leave the file too.
      dirty_ = true;
    }
  }
  storage_error_ = std::move(result.error);
  storage_state_ =
      storage_error_.empty() ? StorageState::kReady : StorageState::kLoadError;
  if (storage_state_ == StorageState::kReady) {
    ScheduleWrite();
    // Brings the index up to date after edits by an older build or by hand.
    UpdateSpacesIndex();
  }
  changed_.Notify();
  // A callback may destroy this service, so run them from a local list.
  auto weak = weak_factory_.GetWeakPtr();
  for (auto& done : std::exchange(when_loaded_, {})) {
    if (!weak) {
      return;
    }
    std::move(done).Run();
  }
}

foundation::MutationResult WorkspaceService::Changed(
    foundation::MutationResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.ok) {
    if (!ephemeral_) {
      dirty_ = true;
      if (storage_state_ != StorageState::kWriteError) {
        ScheduleWrite();
      }
    }
    changed_.Notify();
  }
  return result;
}

foundation::MutationResult WorkspaceService::SpacesChanged(
    foundation::MutationResult result) {
  result = Changed(std::move(result));
  if (result.ok) {
    UpdateSpacesIndex();
  }
  return result;
}

void WorkspaceService::UpdateSpacesIndex() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (ephemeral_ || !CanMutate() || !local_state) {
    return;
  }
  const auto snapshot = foundation::export_snapshot(*workspace_);
  std::vector<IndexedSpace> spaces;
  spaces.reserve(snapshot.spaces.size());
  for (const auto& space : snapshot.spaces) {
    spaces.push_back(
        {.space_id = space.id,
         .name = std::string(space.name),
         .icon_kind = static_cast<IndexedSpace::IconKind>(space.icon_kind),
         .icon = std::string(space.icon)});
  }
  WriteIndexedSpaces(*local_state, profile_dir_, spaces);
}

void WorkspaceService::ScheduleWrite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ephemeral_ || write_in_flight_ || !dirty_) {
    return;
  }
  dirty_ = false;
  write_in_flight_ = true;
  storage_state_ = StorageState::kWritePending;
  storage_error_.clear();
  if (!io_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&WriteMetadata, metadata_path_,
                         foundation::export_snapshot(*workspace_)),
          base::BindOnce(&WorkspaceService::OnWritten,
                         weak_factory_.GetWeakPtr()))) {
    write_in_flight_ = false;
    dirty_ = true;
    storage_state_ = StorageState::kWriteError;
    storage_error_ = "Workspace write could not be scheduled. Retry saving.";
  }
}

void WorkspaceService::RetrySave() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_state_ == StorageState::kWriteError) {
    ScheduleWrite();
    changed_.Notify();
  }
}

void WorkspaceService::OnWritten(bool ok) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  write_in_flight_ = false;
  if (!ok) {
    dirty_ = true;
    storage_state_ = StorageState::kWriteError;
    storage_error_ =
        "Workspace metadata write failed. Changes remain in "
        "memory. Check disk space/permissions and retry saving.";
  } else if (dirty_) {
    ScheduleWrite();
  } else {
    storage_state_ = StorageState::kWritten;
  }
  changed_.Notify();
}

foundation::MutationResult WorkspaceService::CreateSpace(
    std::string_view name) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!BoundedText(name, foundation::bridge_limits().name_bytes))
    return Failure("Invalid UTF-8 or oversized Space name.");
  return SpacesChanged(foundation::create_space(
      *workspace_, rust::Str(name.data(), name.size())));
}

foundation::MutationResult WorkspaceService::CreateSpace(
    std::string_view name,
    foundation::SpaceIconKind icon_kind,
    std::string_view icon,
    const foundation::SpaceThemeRecord& theme) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!BoundedText(name, foundation::bridge_limits().name_bytes))
    return Failure("Invalid UTF-8 or oversized Space name.");
  if (!BoundedText(icon, foundation::bridge_limits().icon_bytes))
    return Failure("Invalid UTF-8 or oversized Space icon.");
  return SpacesChanged(foundation::create_styled_space(
      *workspace_, rust::Str(name.data(), name.size()), icon_kind,
      rust::Str(icon.data(), icon.size()), theme));
}

foundation::MutationResult WorkspaceService::CreateFirstSpace(
    std::string_view name,
    foundation::SpaceIconKind icon_kind,
    std::string_view icon,
    const foundation::SpaceThemeRecord& theme) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!BoundedText(name, foundation::bridge_limits().name_bytes))
    return Failure("Invalid UTF-8 or oversized Space name.");
  if (!BoundedText(icon, foundation::bridge_limits().icon_bytes))
    return Failure("Invalid UTF-8 or oversized Space icon.");
  return SpacesChanged(foundation::create_first_space(
      *workspace_, rust::Str(name.data(), name.size()), icon_kind,
      rust::Str(icon.data(), icon.size()), theme));
}

foundation::ControlPlan WorkspaceService::AnswerControlRequest(
    uint64_t window,
    foundation::ControlReply& reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    foundation::refuse_control_request(
        reply, "Workspace is not loaded yet; retry shortly");
    return {};
  }
  auto plan = foundation::answer_control_request(reply, *workspace_, window,
                                                 profile_dir_);
  if (plan.changed)
    SpacesChanged(foundation::MutationResult{true, 0, {}});
  return plan;
}

foundation::MutationResult WorkspaceService::EnsureAgentSpace() {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return SpacesChanged(foundation::ensure_agent_space(*workspace_));
}

foundation::MutationResult WorkspaceService::RenameSpace(
    uint64_t space,
    std::string_view name) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!BoundedText(name, foundation::bridge_limits().name_bytes))
    return Failure("Invalid UTF-8 or oversized Space name.");
  auto result = foundation::rename_space(*workspace_, space,
                                         rust::Str(name.data(), name.size()));
  return SpacesChanged({result.ok, 0, std::move(result.error)});
}

foundation::MutationResult WorkspaceService::SetSpaceIcon(
    uint64_t space,
    foundation::SpaceIconKind icon_kind,
    std::string_view icon) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!BoundedText(icon, foundation::bridge_limits().icon_bytes))
    return Failure("Invalid UTF-8 or oversized Space icon.");
  return SpacesChanged(foundation::set_space_icon(
      *workspace_, space, icon_kind, rust::Str(icon.data(), icon.size())));
}

foundation::MutationResult WorkspaceService::SetSpaceTheme(
    uint64_t space,
    const foundation::SpaceThemeRecord& theme) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return SpacesChanged(foundation::set_space_theme(*workspace_, space, theme));
}

foundation::MutationResult WorkspaceService::CreateWindow(uint64_t space) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::create_window(*workspace_, space));
}

foundation::MutationResult WorkspaceService::CloseWindow(uint64_t window) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::close_window(*workspace_, window));
}

foundation::MutationResult WorkspaceService::SelectSpace(uint64_t window,
                                                         uint64_t space) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::select_space(*workspace_, window, space));
}

foundation::MutationResult WorkspaceService::AddTemporaryItem(uint64_t space) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::add_temporary_item(*workspace_, space));
}

foundation::MutationResult WorkspaceService::SelectItem(uint64_t window,
                                                        uint64_t item) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::select_item(*workspace_, window, item));
}

foundation::MutationResult WorkspaceService::LiveTabClosed(uint64_t item) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::live_tab_closed(*workspace_, item));
}

foundation::MutationResult WorkspaceService::ArchiveClosedTab(
    uint64_t item,
    std::string_view url,
    std::string_view title) {
  // Private browsing keeps no history, so it archives nothing.
  if (ephemeral_)
    return LiveTabClosed(item);
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  // An unarchivable URL (a Chrome page, say) still ends the item in Rust.
  const std::string canonical = CanonicalUrl(url).value_or(std::string());
  const std::string text = base::IsStringUTF8(title) ? std::string(title) : "";
  return Changed(foundation::archive_closed_tab(
      *workspace_, item, rust::Str(canonical.data(), canonical.size()),
      rust::Str(text.data(), text.size()), NowSecs()));
}

rust::Vec<uint64_t> WorkspaceService::AutoArchive(
    base::span<const uint64_t> viewed,
    base::span<const uint64_t> closable,
    base::span<const uint64_t> media) {
  // Private browsing keeps no history, so it archives nothing.
  if (ephemeral_ || !CanMutate())
    return {};
  // Whole monotonic seconds since the previous pass; the remainder carries
  // over. Rust caps the gap, so time asleep does not count either way.
  const base::TimeTicks now = base::TimeTicks::Now();
  if (last_pass_.is_null())
    last_pass_ = now;
  const int64_t elapsed = std::max<int64_t>(0, (now - last_pass_).InSeconds());
  last_pass_ += base::Seconds(elapsed);
  auto pass = foundation::auto_archive(
      *workspace_, rust::Slice<const uint64_t>(viewed.data(), viewed.size()),
      rust::Slice<const uint64_t>(closable.data(), closable.size()),
      rust::Slice<const uint64_t>(media.data(), media.size()),
      static_cast<uint64_t>(elapsed));
  // Like titles, the clock and view times touch no bindings: save them with
  // the next metadata write or the shutdown flush, without a notify.
  if (pass.changed)
    dirty_ = true;
  return std::move(pass.due);
}

rust::Vec<foundation::ArchivedTabRecord> WorkspaceService::ArchivedTabs()
    const {
  if (!CanMutate())
    return {};
  return foundation::archived_tabs(*workspace_, NowSecs());
}

uint64_t WorkspaceService::ArchiveIntervalSecs() const {
  if (!CanMutate())
    return 0;
  return foundation::archive_interval_secs(*workspace_);
}

foundation::MutationResult WorkspaceService::SetArchiveInterval(
    uint64_t secs) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::set_archive_interval(*workspace_, secs));
}

foundation::MutationResult WorkspaceService::ClearArchivedTabs() {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::clear_archived_tabs(*workspace_));
}

foundation::MutationResult WorkspaceService::RemoveArchivedTab(uint64_t id) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::remove_archived_tab(*workspace_, id));
}

void WorkspaceService::ForgetArchivedTabs(
    base::span<const std::string> urls,
    std::optional<std::pair<base::Time, base::Time>> range) {
  if (!CanMutate())
    return;
  rust::Vec<rust::String> owned;
  owned.reserve(urls.size());
  for (const auto& url : urls)
    owned.emplace_back(url.data(), url.size());
  // A null end means "until now", the same as no end.
  const uint64_t end =
      range && !range->second.is_null() ? Secs(range->second)
                                        : std::numeric_limits<uint64_t>::max();
  auto result = foundation::forget_archived_tabs(
      *workspace_, std::move(owned), range.has_value(),
      range ? Secs(range->first) : 0, end);
  if (result.id)
    Changed(std::move(result));
}

foundation::MutationResult WorkspaceService::SetPlacement(
    uint64_t item,
    foundation::PlacementKind placement,
    uint64_t owner,
    bool has_url,
    std::string_view canonical_url) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!BoundedText(canonical_url, foundation::bridge_limits().url_bytes))
    return Failure("Invalid UTF-8 or oversized saved URL.");
  auto url = has_url ? CanonicalUrl(canonical_url)
                     : std::optional<std::string>(std::string(canonical_url));
  if (!url)
    return Failure(
        "Use a valid HTTP(S) saved URL without credentials, or about:blank.");
  return Changed(
      foundation::set_placement(*workspace_, item, placement, owner, has_url,
                                rust::Str(url->data(), url->size())));
}

foundation::MutationResult WorkspaceService::SetTitle(uint64_t item,
                                                      std::string_view title) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!base::IsStringUTF8(title))
    return Failure("Invalid UTF-8 page title.");
  auto result = foundation::set_title(*workspace_, item,
                                      rust::Str(title.data(), title.size()));
  // Titles touch no bindings, and live rows show the page's own title, so
  // skip the notify and its rebuilds. The next write or shutdown flush saves it.
  if (result.ok && result.id && !ephemeral_)
    dirty_ = true;
  return result;
}

foundation::MutationResult WorkspaceService::RenameItem(uint64_t item,
                                                        std::string_view name) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!base::IsStringUTF8(name))
    return Failure("Invalid UTF-8 tab name.");
  return Changed(foundation::rename_item(*workspace_, item,
                                         rust::Str(name.data(), name.size())));
}

foundation::MutationResult WorkspaceService::DropItem(
    uint64_t window,
    uint64_t item,
    foundation::DropTargetKind target,
    uint64_t folder,
    uint64_t before,
    std::optional<std::string_view> page_url) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  std::optional<std::string> url;
  if (page_url) {
    url = CanonicalUrl(*page_url);
    if (!url)
      return Failure(
          "Pin a page with a valid HTTP(S) URL without credentials, or "
          "about:blank.");
  }
  const std::string_view text = url ? std::string_view(*url) : "";
  return Changed(foundation::drop_item(*workspace_, window, item, target,
                                       folder, before, url.has_value(),
                                       rust::Str(text.data(), text.size())));
}

foundation::MutationResult WorkspaceService::SetTemporaryPage(
    uint64_t item,
    std::string_view url,
    std::string_view title) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  const auto canonical = CanonicalUrl(url);
  if (!canonical || !base::IsStringUTF8(title))
    return {true, 0, rust::String()};
  auto result = foundation::set_temporary_page(
      *workspace_, item, rust::Str(canonical->data(), canonical->size()),
      rust::Str(title.data(), title.size()));
  // Like titles, a page touches no bindings, so skip the notify. It is written
  // now so a crash keeps the row reopenable.
  if (result.ok && result.id && !ephemeral_) {
    dirty_ = true;
    if (storage_state_ != StorageState::kWriteError)
      ScheduleWrite();
  }
  return result;
}

uint64_t WorkspaceService::DormantTemporaryItem(
    uint64_t space,
    std::string_view url,
    base::span<const uint64_t> live) const {
  const auto canonical = CanonicalUrl(url);
  if (!CanMutate() || !canonical)
    return 0;
  return foundation::dormant_temporary_item(
      *workspace_, space, rust::Str(canonical->data(), canonical->size()),
      rust::Slice<const uint64_t>(live.data(), live.size()));
}

foundation::MutationResult WorkspaceService::RemoveSavedItem(uint64_t item) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::remove_saved_item(*workspace_, item));
}

foundation::MutationResult WorkspaceService::RevealItem(uint64_t window,
                                                        uint64_t item) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::reveal_item(*workspace_, window, item));
}

foundation::MutationResult WorkspaceService::SelectItemInItsSpace(
    uint64_t window,
    uint64_t item) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(
      foundation::select_item_in_its_space(*workspace_, window, item));
}

foundation::MutationResult WorkspaceService::UnsaveItem(uint64_t item,
                                                        uint64_t live_space) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::unsave_item(*workspace_, item, live_space));
}

foundation::MutationResult WorkspaceService::MoveItemBefore(uint64_t item,
                                                            uint64_t before) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::move_item_before(*workspace_, item, before));
}

foundation::MutationResult WorkspaceService::CreateFolder(
    uint64_t space,
    std::string_view name) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!BoundedText(name, foundation::bridge_limits().name_bytes))
    return Failure("Invalid UTF-8 or oversized folder name.");
  return Changed(foundation::create_folder(
      *workspace_, space, rust::Str(name.data(), name.size())));
}

foundation::MutationResult WorkspaceService::RenameFolder(
    uint64_t folder,
    std::string_view name) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!BoundedText(name, foundation::bridge_limits().name_bytes))
    return Failure("Invalid UTF-8 or oversized folder name.");
  return Changed(foundation::rename_folder(
      *workspace_, folder, rust::Str(name.data(), name.size())));
}

foundation::MutationResult WorkspaceService::SetFolderOpen(uint64_t folder,
                                                           bool open) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  auto result = foundation::set_folder_open(*workspace_, folder, open);
  return result.ok && !result.id ? result : Changed(std::move(result));
}

foundation::MutationResult WorkspaceService::MoveFolderBefore(uint64_t folder,
                                                              uint64_t before) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::move_folder_before(*workspace_, folder, before));
}

foundation::MutationResult WorkspaceService::MovePinToFolder(uint64_t item,
                                                             uint64_t folder,
                                                             uint64_t before) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(
      foundation::move_pin_to_folder(*workspace_, item, folder, before));
}

foundation::MutationResult WorkspaceService::DeleteFolder(uint64_t folder,
                                                          bool archive) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  // Private browsing keeps no history, so it archives nothing.
  return Changed(foundation::delete_folder(*workspace_, folder,
                                           archive && !ephemeral_, NowSecs()));
}

foundation::MutationResult WorkspaceService::CreateSplit(uint64_t left,
                                                         uint64_t right) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  auto result = foundation::create_split(*workspace_, left, right);
  return result.ok && !result.id ? result : Changed(std::move(result));
}

foundation::MutationResult WorkspaceService::SeparateSplit(uint64_t item) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  auto result = foundation::separate_split(*workspace_, item);
  return result.ok && !result.id ? result : Changed(std::move(result));
}

foundation::MutationResult WorkspaceService::MoveSpaceBefore(uint64_t space,
                                                             uint64_t before) {
  if (!CanMutate()) {
    return Failure("Workspace is not loaded. Check storage state.");
  }
  return SpacesChanged(
      foundation::move_space_before(*workspace_, space, before));
}

foundation::MutationResult WorkspaceService::ReconcileLiveItems(
    base::span<const uint64_t> live) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  return Changed(foundation::reconcile_live_items(
      *workspace_, rust::Slice<const uint64_t>(live.data(), live.size())));
}

foundation::SidebarResult WorkspaceService::Sidebar(uint64_t window) const {
  if (!CanMutate()) {
    foundation::SidebarResult result;
    result.ok = false;
    result.error = "Workspace is not loaded. Check storage state.";
    return result;
  }
  return foundation::sidebar_snapshot(*workspace_, window);
}

std::optional<foundation::StateSnapshot> WorkspaceService::ExportSnapshot()
    const {
  if (!CanMutate())
    return std::nullopt;
  return foundation::export_snapshot(*workspace_);
}

foundation::MutationResult WorkspaceService::RestoreSnapshot(
    foundation::StateSnapshot snapshot) {
  if (!CanMutate())
    return Failure("Workspace is not loaded. Check storage state.");
  if (!internal::CanonicalizeUrls(snapshot))
    return Failure("Workspace snapshot exceeds limits or has an invalid URL.");
  auto result = foundation::restore_snapshot(*workspace_, std::move(snapshot));
  if (result.ok)
    foundation::prune_archived_tabs(*workspace_, NowSecs());
  return SpacesChanged(std::move(result));
}

// static
rust::Box<foundation::ArcExport> WorkspaceService::ParseArcExport(
    base::span<const uint8_t> bytes) {
  auto export_data =
      foundation::parse_arc_export(rust::Slice<const uint8_t>(bytes.data(),
                                                              bytes.size()));
  rust::Vec<rust::String> canonical;
  for (const auto& url : foundation::arc_export_urls(*export_data)) {
    const auto spec = CanonicalUrl(std::string_view(url.data(), url.size()));
    // about:blank passes the saved-URL gate but is no Arc tab.
    canonical.push_back(spec && *spec != "about:blank"
                            ? rust::String(*spec)
                            : rust::String());
  }
  foundation::set_arc_export_canonical_urls(*export_data, std::move(canonical));
  return export_data;
}

foundation::ArcImportPreview WorkspaceService::PreviewArcImport(
    const foundation::ArcExport& export_data) const {
  if (!CanMutate()) {
    auto preview = foundation::preview_arc_import(*workspace_, export_data);
    preview.ok = false;
    preview.error = "Workspace is not loaded. Check storage state.";
    return preview;
  }
  return foundation::preview_arc_import(*workspace_, export_data);
}

foundation::ArcImportPreview WorkspaceService::ApplyArcImport(
    const foundation::ArcExport& export_data) {
  if (!CanMutate())
    return PreviewArcImport(export_data);
  auto result = foundation::apply_arc_import(*workspace_, export_data);
  if (result.ok && (result.new_spaces || result.new_pins ||
                    result.new_folders || result.moved_pins)) {
    SpacesChanged({true, 0, rust::String()});
  }
  return result;
}

}  // namespace myarc
