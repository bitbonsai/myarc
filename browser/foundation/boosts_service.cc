#include "myarc/boosts_service.h"

#include <array>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace myarc {
namespace {

const char kBoostsServiceKey = 0;

scoped_refptr<base::SequencedTaskRunner> BoostsIoRunner() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

rust::Str PathStr(const std::string& path) {
  return rust::Str(path.data(), path.size());
}

rust::Str ViewStr(std::string_view text) {
  return rust::Str(text.data(), text.size());
}

uint64_t NowSecs() {
  const time_t secs = base::Time::Now().ToTimeT();
  return secs > 0 ? static_cast<uint64_t>(secs) : 0;
}

bool IsEligibleOrigin(const url::Origin& origin) {
  return !origin.opaque() && (origin.scheme() == url::kHttpScheme ||
                              origin.scheme() == url::kHttpsScheme);
}

// Canonicalizes each input origin string via Chromium's GURL + url::Origin
// before crossing into Rust. Returns std::nullopt if any entry is invalid or
// non-HTTP(S).
std::optional<rust::Vec<rust::String>> CanonicalizeOrigins(
    const std::vector<std::string>& raw_origins) {
  const auto limits = foundation::boosts_limits();
  if (raw_origins.empty() || raw_origins.size() > limits.max_origins_per_boost) {
    return std::nullopt;
  }
  rust::Vec<rust::String> out;
  out.reserve(raw_origins.size());
  for (const auto& raw : raw_origins) {
    if (raw.size() > limits.max_origin_bytes || !base::IsStringUTF8(raw)) {
      return std::nullopt;
    }
    const GURL url(raw);
    if (!url.is_valid() || url.has_username() || url.has_password() ||
        !url.SchemeIsHTTPOrHTTPS()) {
      return std::nullopt;
    }
    const url::Origin origin = url::Origin::Create(url);
    if (!IsEligibleOrigin(origin)) {
      return std::nullopt;
    }
    const std::string serialized = origin.Serialize();
    if (serialized.size() > limits.max_origin_bytes) {
      return std::nullopt;
    }
    out.emplace_back(serialized.data(), serialized.size());
  }
  return out;
}

internal::BoostsFileResult ReadSingleBoostsFile(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    if (file.error_details() == base::File::FILE_ERROR_NOT_FOUND) {
      return {};  // Missing / first run.
    }
    return {std::nullopt,
            "Cannot open Boosts file. Check permissions and retry loading.",
            /*damaged=*/false};
  }
  const int64_t size = file.GetLength();
  const size_t max_bytes = foundation::boosts_limits().max_file_bytes;
  if (size < 0 || static_cast<uint64_t>(size) > max_bytes) {
    return {std::nullopt,
            "Boosts file exceeds size limit. File preserved.",
            /*damaged=*/true};
  }
  std::string json(static_cast<size_t>(size), '\0');
  std::array<uint8_t, 1> trailing;
  if (!file.ReadAtCurrentPosAndCheck(base::as_writable_byte_span(json)) ||
      file.ReadAtCurrentPos(trailing) != 0) {
    return {std::nullopt,
            "Boosts file read failed or changed while reading. Retry loading.",
            /*damaged=*/false};
  }
  // Validate with a temporary Rust BoostStore so corrupt or newer-schema files
  // are classified before touching disk.
  auto probe = foundation::new_boost_store();
  const auto check = foundation::boosts_restore_json(
      *probe,
      rust::Slice<const uint8_t>(reinterpret_cast<const uint8_t*>(json.data()),
                                 json.size()));
  if (!check.ok) {
    return {std::nullopt, std::string(check.error), check.damaged};
  }
  return {std::move(json), {}, /*damaged=*/false, /*from_backup=*/false};
}

// Blocking: flushes the saved file and its directory to disk.
void SyncBoostsFile(const base::FilePath& path) {
  const std::string text = path.AsUTF8Unsafe();
  const rust::String error = foundation::sync_workspace_file(PathStr(text));
  LOG_IF(ERROR, !error.empty())
      << "Boosts file flush failed: " << std::string(error)
      << ". The file is saved but a power loss may lose recent changes.";
}

}  // namespace

namespace internal {

BoostsFileResult ReadBoostsFile(const base::FilePath& path) {
  auto result = ReadSingleBoostsFile(path);
  const bool missing = !result.json && result.error.empty();
  if (!result.damaged && !missing) {
    return result;
  }
  const std::string text = path.AsUTF8Unsafe();
  const base::FilePath backup = base::FilePath::FromUTF8Unsafe(
      std::string(foundation::workspace_backup_path(PathStr(text))));
  auto from_backup = ReadSingleBoostsFile(backup);
  if (!from_backup.json) {
    // First run (both missing), or backup is damaged too: leave files alone.
    return missing ? from_backup : result;
  }
  if (missing) {
    LOG(WARNING) << "Boosts file " << path << " is missing; loaded its backup "
                 << backup << ".";
  } else {
    // Move the damaged file aside before a later save turns it into `.bak`.
    const std::string aside =
        std::string(foundation::set_aside_workspace_file(PathStr(text)));
    if (aside.empty()) {
      LOG(ERROR) << "Boosts file " << path << " is damaged and could not be "
                 << "moved aside; its backup was not loaded.";
      return result;
    }
    LOG(WARNING) << "Boosts file " << path << " is damaged; loaded the last "
                 << "good copy " << backup << " and kept the damaged file as "
                 << aside << ".";
  }
  from_backup.from_backup = true;
  return from_backup;
}

}  // namespace internal

// static
BoostsService& BoostsService::ForProfile(Profile& profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* service =
      static_cast<BoostsService*>(profile.GetUserData(&kBoostsServiceKey));
  if (!service) {
    auto owned = std::unique_ptr<BoostsService>(new BoostsService(profile));
    service = owned.get();
    profile.SetUserData(&kBoostsServiceKey, std::move(owned));
    if (!service->ephemeral_) {
      service->StartLoad();
    }
  }
  return *service;
}

BoostsService::BoostsService(Profile& profile)
    : ephemeral_(profile.IsOffTheRecord() || profile.IsGuestSession() ||
                 profile.IsSystemProfile()),
      file_path_(ephemeral_ ? base::FilePath()
                            : profile.GetPath().AppendASCII(
                                  "myarc-boosts-v1.json")),
      io_runner_(ephemeral_ ? nullptr : BoostsIoRunner()),
      store_(foundation::new_boost_store()),
      storage_state_(ephemeral_ ? StorageState::kEphemeral
                                : StorageState::kLoading) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BoostsService::~BoostsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FlushForShutdown();
}

BoostsService::StorageState BoostsService::storage_state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_state_;
}

std::string BoostsService::storage_error() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_error_;
}

void BoostsService::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void BoostsService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void BoostsService::WhenLoaded(base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_state_ == StorageState::kLoading) {
    when_loaded_.push_back(std::move(done));
  } else {
    std::move(done).Run();
  }
}

void BoostsService::RetryLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_state_ == StorageState::kLoadError) {
    StartLoad();
    NotifyChanged();
  }
}

void BoostsService::RetrySave() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_state_ == StorageState::kWriteError) {
    Save();
    NotifyChanged();
  }
}

void BoostsService::FlushForShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ephemeral_ || !CanMutate() || !dirty_) {
    return;
  }
  Save();
  LOG_IF(ERROR, dirty_) << "Final Boosts file write failed.";
}

bool BoostsService::CanMutate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_state_ != StorageState::kLoading &&
         storage_state_ != StorageState::kLoadError;
}

void BoostsService::NotifyChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnBoostsChanged();
  }
}

void BoostsService::StartLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  storage_state_ = StorageState::kLoading;
  storage_error_.clear();
  if (!io_runner_->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&internal::ReadBoostsFile, file_path_),
          base::BindOnce(&BoostsService::OnLoaded,
                         weak_factory_.GetWeakPtr()))) {
    storage_state_ = StorageState::kLoadError;
    storage_error_ = "Boosts load could not be scheduled. Retry loading.";
  }
}

void BoostsService::OnLoaded(internal::BoostsFileResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.json) {
    const auto restored = foundation::boosts_restore_json(
        *store_,
        rust::Slice<const uint8_t>(
            reinterpret_cast<const uint8_t*>(result.json->data()),
            result.json->size()));
    if (!restored.ok) {
      result.error = std::string(restored.error);
    } else if (result.from_backup) {
      dirty_ = true;
    }
  }
  storage_error_ = std::move(result.error);
  storage_state_ =
      storage_error_.empty() ? StorageState::kReady : StorageState::kLoadError;
  if (storage_state_ == StorageState::kReady && dirty_) {
    Save();
  }
  NotifyChanged();
  auto weak = weak_factory_.GetWeakPtr();
  for (auto& done : std::exchange(when_loaded_, {})) {
    if (!weak) {
      return;
    }
    std::move(done).Run();
  }
}

void BoostsService::Save() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ephemeral_ || !dirty_) {
    return;
  }
  const rust::String json = foundation::boosts_encode_json(*store_);
  const std::string path = file_path_.AsUTF8Unsafe();
  const rust::String error =
      json.size() <= foundation::boosts_limits().max_file_bytes
          ? foundation::write_workspace_file(
                PathStr(path),
                rust::Slice<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(json.data()), json.size()))
          : rust::String("Boosts data exceeds the file size limit");
  if (!error.empty()) {
    LOG(ERROR) << "Boosts write failed: " << std::string(error);
    storage_state_ = StorageState::kWriteError;
    storage_error_ =
        "Boosts write failed. Changes remain in memory. Check disk "
        "space/permissions and retry saving.";
    return;
  }
  dirty_ = false;
  storage_state_ = StorageState::kWritten;
  storage_error_.clear();
  Sync();
}

void BoostsService::Sync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sync_in_flight_) {
    sync_again_ = true;
    return;
  }
  sync_in_flight_ = true;
  if (!io_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&SyncBoostsFile, file_path_),
          base::BindOnce(&BoostsService::OnSynced,
                         weak_factory_.GetWeakPtr()))) {
    sync_in_flight_ = false;
  }
}

void BoostsService::OnSynced() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_in_flight_ = false;
  if (std::exchange(sync_again_, false)) {
    Sync();
  }
}

foundation::MutationResult BoostsService::Changed(
    foundation::MutationResult result,
    bool mutated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.ok && mutated) {
    if (!ephemeral_) {
      dirty_ = true;
      if (storage_state_ != StorageState::kWriteError) {
        Save();
      }
    }
    NotifyChanged();
  }
  return result;
}

bool BoostsService::GlobalEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return false;
  }
  return foundation::boosts_global_enabled(*store_);
}

bool BoostsService::SetGlobalEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return false;
  }
  if (!foundation::boosts_set_global_enabled(*store_, enabled)) {
    return false;
  }
  if (!ephemeral_) {
    dirty_ = true;
    if (storage_state_ != StorageState::kWriteError) {
      Save();
    }
  }
  NotifyChanged();
  return true;
}

bool BoostsService::ToggleGlobalEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return SetGlobalEnabled(!GlobalEnabled());
}

rust::Vec<foundation::BoostRecord> BoostsService::ListBoosts(
    std::string_view query) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate() || !base::IsStringUTF8(query)) {
    return {};
  }
  return foundation::boosts_list(*store_, ViewStr(query));
}

std::optional<foundation::BoostRecord> BoostsService::GetBoost(
    uint64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return std::nullopt;
  }
  auto list = foundation::boosts_get(*store_, id);
  if (list.empty()) {
    return std::nullopt;
  }
  return std::move(list[0]);
}

rust::Vec<foundation::BoostRecord> BoostsService::MatchingBoosts(
    const url::Origin& origin,
    bool is_subframe) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate() || !IsEligibleOrigin(origin)) {
    return {};
  }
  const std::string serialized = origin.Serialize();
  return foundation::boosts_matching(*store_, ViewStr(serialized), is_subframe);
}

std::string BoostsService::CompileForOrigin(const url::Origin& origin,
                                            bool is_subframe) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate() || !IsEligibleOrigin(origin)) {
    return {};
  }
  const std::string serialized = origin.Serialize();
  return std::string(foundation::boosts_compile_for_origin(
      *store_, ViewStr(serialized), is_subframe));
}

foundation::MutationResult BoostsService::CreateBoost(
    std::string_view name,
    const std::vector<std::string>& origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  if (!base::IsStringUTF8(name)) {
    return Failure("Boost name must be valid UTF-8.");
  }
  auto canonical = CanonicalizeOrigins(origins);
  if (!canonical) {
    return Failure(
        "Boost origins must be valid http:// or https:// origins without "
        "credentials.");
  }
  auto result = foundation::boosts_create(*store_, ViewStr(name),
                                          std::move(*canonical), NowSecs());
  const bool ok = result.ok;
  return Changed(std::move(result), ok);
}

foundation::MutationResult BoostsService::RenameBoost(uint64_t id,
                                                      std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  if (!base::IsStringUTF8(name)) {
    return Failure("Boost name must be valid UTF-8.");
  }
  auto result =
      foundation::boosts_rename(*store_, id, ViewStr(name), NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::SetBoostEnabled(uint64_t id,
                                                          bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  auto result =
      foundation::boosts_set_enabled(*store_, id, enabled, NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::SetBoostScope(
    uint64_t id,
    const std::vector<std::string>& origins,
    bool include_subdomains,
    bool apply_to_subframes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  auto canonical = CanonicalizeOrigins(origins);
  if (!canonical) {
    return Failure(
        "Boost origins must be valid http:// or https:// origins without "
        "credentials.");
  }
  auto result = foundation::boosts_set_scope(
      *store_, id, std::move(*canonical), include_subdomains,
      apply_to_subframes, NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::SetBoostAppearance(
    uint64_t id,
    const foundation::BoostAppearanceRecord& appearance) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  auto result =
      foundation::boosts_set_appearance(*store_, id, appearance, NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::AddHiddenSelector(
    uint64_t id,
    std::string_view selector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  if (!base::IsStringUTF8(selector)) {
    return Failure("Hidden selector must be valid UTF-8.");
  }
  auto result = foundation::boosts_add_hidden_selector(
      *store_, id, ViewStr(selector), NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::RemoveHiddenSelector(
    uint64_t id,
    std::string_view selector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  if (!base::IsStringUTF8(selector)) {
    return Failure("Hidden selector must be valid UTF-8.");
  }
  auto result = foundation::boosts_remove_hidden_selector(
      *store_, id, ViewStr(selector), NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::ClearHiddenSelectors(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  auto result = foundation::boosts_clear_hidden_selectors(*store_, id,
                                                          NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::SetCustomCss(
    uint64_t id,
    std::string_view custom_css) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  if (!base::IsStringUTF8(custom_css)) {
    return Failure("Custom CSS must be valid UTF-8.");
  }
  auto result = foundation::boosts_set_custom_css(
      *store_, id, ViewStr(custom_css), NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::ResetBoost(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  auto result = foundation::boosts_reset(*store_, id, NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::UndoBoost(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  auto result = foundation::boosts_undo(*store_, id, NowSecs());
  const bool mutated = result.ok && result.id != 0;
  return Changed(std::move(result), mutated);
}

foundation::MutationResult BoostsService::DeleteBoost(uint64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanMutate()) {
    return Failure("Boosts are not loaded yet. Check storage state.");
  }
  auto result = foundation::boosts_delete(*store_, id);
  const bool ok = result.ok;
  return Changed(std::move(result), ok);
}

// static
std::string BoostsService::CompilePreview(
    const foundation::BoostAppearanceRecord& appearance,
    const std::vector<std::string>& hidden_selectors,
    std::string_view custom_css) {
  if (!base::IsStringUTF8(custom_css)) {
    return {};
  }
  rust::Vec<rust::String> selectors;
  selectors.reserve(hidden_selectors.size());
  for (const auto& sel : hidden_selectors) {
    if (base::IsStringUTF8(sel)) {
      selectors.emplace_back(sel.data(), sel.size());
    }
  }
  return std::string(foundation::boosts_compile_preview(
      appearance, std::move(selectors), ViewStr(custom_css)));
}

// static
rust::Vec<foundation::BoostCssFinding> BoostsService::InspectCss(
    std::string_view css) {
  if (!base::IsStringUTF8(css)) {
    return {};
  }
  return foundation::boosts_inspect_css(ViewStr(css));
}

}  // namespace myarc
