#ifndef MYARC_BOOSTS_SERVICE_H_
#define MYARC_BOOSTS_SERVICE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "myarc/bridge.rs.h"

class Profile;
namespace base {
class SequencedTaskRunner;
}
namespace url {
class Origin;
}

namespace myarc {

namespace internal {
struct BoostsFileResult {
  // Decoded JSON bytes ready for `boosts_restore_json`, or empty on first run
  // / error.
  std::optional<std::string> json;
  std::string error;
  // True when the file failed to decode or exceeded bounds; false for an
  // unsupported newer schema version (which must not be overwritten or
  // replaced by the backup).
  bool damaged = false;
  // True when `json` came from the `.bak` fallback.
  bool from_backup = false;
};

// Blocking. Reads and validates the Boosts file at `path`. When it is damaged
// or missing and its `.bak` backup decodes cleanly, moves the damaged file
// aside as `.corrupt` and returns the backup's JSON. Unsupported newer schema
// versions fail read-only without touching the file.
BoostsFileResult ReadBoostsFile(const base::FilePath& path);
}  // namespace internal

// UI-sequence only. Exact Profile owns this service and its Rust BoostStore.
// Never redirects OTR/Guest Profiles to GetOriginalProfile(): private windows
// get an ephemeral, empty store by default with zero leakage from regular
// Profiles.
class BoostsService final : public base::SupportsUserData::Data {
 public:
  enum class StorageState {
    kLoading,
    kLoadError,  // Read-only until RetryLoad succeeds; file is left untouched.
    kReady,
    kWriteError,
    kWritten,
    kEphemeral,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnBoostsChanged() = 0;
  };

  static BoostsService& ForProfile(Profile& profile);
  ~BoostsService() override;

  bool IsEphemeral() const { return ephemeral_; }
  StorageState storage_state() const;
  std::string storage_error() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void WhenLoaded(base::OnceClosure done);
  void RetryLoad();
  void RetrySave();
  void FlushForShutdown();

  // Global Profile-wide kill switch.
  bool GlobalEnabled() const;
  bool SetGlobalEnabled(bool enabled);
  bool ToggleGlobalEnabled();

  // Queries.
  rust::Vec<foundation::BoostRecord> ListBoosts(
      std::string_view query = "") const;
  std::optional<foundation::BoostRecord> GetBoost(uint64_t id) const;
  rust::Vec<foundation::BoostRecord> MatchingBoosts(
      const url::Origin& origin,
      bool is_subframe = false) const;

  // Compiles one deterministic, pure-CSS stylesheet for all enabled Boosts
  // matching `origin` (returns empty for opaque/non-HTTP(S) origins, when
  // `GlobalEnabled()` is false, or while not loaded).
  std::string CompileForOrigin(const url::Origin& origin,
                               bool is_subframe = false) const;

  // Mutations (synchronous atomic write + off-thread sync + observer notify).
  foundation::MutationResult CreateBoost(
      std::string_view name,
      const std::vector<std::string>& origins);
  foundation::MutationResult RenameBoost(uint64_t id, std::string_view name);
  foundation::MutationResult SetBoostEnabled(uint64_t id, bool enabled);
  foundation::MutationResult SetBoostScope(
      uint64_t id,
      const std::vector<std::string>& origins,
      bool include_subdomains,
      bool apply_to_subframes);
  foundation::MutationResult SetBoostAppearance(
      uint64_t id,
      const foundation::BoostAppearanceRecord& appearance);
  foundation::MutationResult AddHiddenSelector(uint64_t id,
                                               std::string_view selector);
  foundation::MutationResult RemoveHiddenSelector(uint64_t id,
                                                  std::string_view selector);
  foundation::MutationResult ClearHiddenSelectors(uint64_t id);
  foundation::MutationResult SetCustomCss(uint64_t id,
                                          std::string_view custom_css);
  foundation::MutationResult ResetBoost(uint64_t id);
  foundation::MutationResult UndoBoost(uint64_t id);
  foundation::MutationResult DeleteBoost(uint64_t id);

  // Pure helpers for live preview and editor validation (no disk I/O).
  static std::string CompilePreview(
      const foundation::BoostAppearanceRecord& appearance,
      const std::vector<std::string>& hidden_selectors,
      std::string_view custom_css);
  static rust::Vec<foundation::BoostCssFinding> InspectCss(
      std::string_view css);

 private:
  explicit BoostsService(Profile& profile);
  bool CanMutate() const;
  foundation::MutationResult Changed(foundation::MutationResult result,
                                     bool mutated);
  void NotifyChanged();
  void StartLoad();
  void OnLoaded(internal::BoostsFileResult result);
  void Save();
  void Sync();
  void OnSynced();

  const bool ephemeral_;
  const base::FilePath file_path_;
  const scoped_refptr<base::SequencedTaskRunner> io_runner_;
  rust::Box<foundation::BoostStore> store_;
  StorageState storage_state_;
  std::string storage_error_;
  bool dirty_ = false;
  bool sync_in_flight_ = false;
  bool sync_again_ = false;
  base::ObserverList<Observer> observers_;
  std::vector<base::OnceClosure> when_loaded_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BoostsService> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_BOOSTS_SERVICE_H_
