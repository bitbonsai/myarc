#include "myarc/workspace_service.h"

#include <array>
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
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace myarc {
namespace {

const char kWorkspaceServiceKey = 0;
// Covers the core maximum (1024 saved URLs of 8192 bytes), including JSON
// escaping. JSONReader's input and depth limits bound its intermediate tree;
// per-field/count checks below precede allocation of owned bridge records.
constexpr size_t kMaxMetadataBytes = 20 * 1024 * 1024;
constexpr size_t kMaxJsonDepth = 8;

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
  if (!root || root->size() != 8 ||
      root->FindInt("version") != static_cast<int>(limits.version)) {
    return InvalidMetadata();
  }
  const auto* spaces = root->FindList("spaces");
  const auto* items = root->FindList("items");
  const auto* windows = root->FindList("windows");
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
  for (const auto& value : *spaces) {
    const auto* record = value.GetIfDict();
    foundation::SpaceRecord space;
    const auto* name = record ? record->FindString("name") : nullptr;
    if (!record || record->size() != 3 || !name ||
        !BoundedText(*name, limits.name_bytes) ||
        !ReadId(*record, "id", space.id) ||
        !ReadId(*record, "profile_id", space.profile_id)) {
      return InvalidMetadata();
    }
    space.name = *name;
    snapshot.spaces.push_back(std::move(space));
  }
  for (const auto& value : *items) {
    const auto* record = value.GetIfDict();
    if (!record || record->size() != 5) {
      return InvalidMetadata();
    }
    const auto kind = record->FindInt("placement");
    const auto has_url = record->FindBool("has_url");
    const auto* url = record->FindString("canonical_url");
    foundation::ItemRecord item;
    if (!kind ||
        *kind < static_cast<int>(foundation::PlacementKind::Temporary) ||
        *kind > static_cast<int>(foundation::PlacementKind::Favorite) ||
        !has_url || !url || !BoundedText(*url, limits.url_bytes) ||
        (!*has_url && !url->empty()) || !ReadId(*record, "id", item.id) ||
        !ReadId(*record, "owner_id", item.owner_id)) {
      return InvalidMetadata();
    }
    item.placement = static_cast<foundation::PlacementKind>(*kind);
    item.has_url = *has_url;
    item.canonical_url = *url;
    snapshot.items.push_back(std::move(item));
  }
  for (const auto& value : *windows) {
    const auto* record = value.GetIfDict();
    foundation::WindowRecord window;
    if (!record || record->size() != 3 || !ReadId(*record, "id", window.id) ||
        !ReadId(*record, "space_id", window.space_id) ||
        !ReadId(*record, "selected_item_id", window.selected_item_id)) {
      return InvalidMetadata();
    }
    snapshot.windows.push_back(std::move(window));
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
    }
  }
  storage_error_ = std::move(result.error);
  storage_state_ =
      storage_error_.empty() ? StorageState::kReady : StorageState::kLoadError;
  changed_.Notify();
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
  return Changed(foundation::create_space(*workspace_,
                                          rust::Str(name.data(), name.size())));
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
  return Changed({result.ok, 0, std::move(result.error)});
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
  return Changed(
      foundation::restore_snapshot(*workspace_, std::move(snapshot)));
}

}  // namespace myarc
