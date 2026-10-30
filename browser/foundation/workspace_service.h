#ifndef MYARC_WORKSPACE_SERVICE_H_
#define MYARC_WORKSPACE_SERVICE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "myarc/bridge.rs.h"

class Profile;
namespace base {
class SequencedTaskRunner;
}

namespace myarc {

// Wire-format helpers, shared with bridge_check. Decode bounds owned Cxx
// buffers; Rust restore remains authoritative for version, identities and model
// validation.
namespace internal {
struct MetadataResult {
  std::optional<foundation::StateSnapshot> snapshot;
  std::string error;
};
std::optional<std::string> EncodeWorkspaceMetadata(
    const foundation::StateSnapshot& snapshot);
MetadataResult DecodeWorkspaceMetadata(std::string_view json);
bool CanonicalizeUrls(foundation::StateSnapshot& snapshot);
}  // namespace internal

// UI-sequence only. Exact Profile owns this service and its Rust workspace.
// Never redirect OTR to GetOriginalProfile(). Controls retain subscriptions,
// not independent workspaces; callers must stop using the service before
// Profile dies. Native window/tab pointers and session IDs stay in the browser
// projection layer.
class WorkspaceService final : public base::SupportsUserData::Data {
 public:
  enum class StorageState {
    kLoading,
    kLoadError,  // Read-only until RetryLoad succeeds; file is left untouched.
    kReady,
    kWritePending,
    kWriteError,
    kWritten,  // Atomic write returned success, NOT OS-crash durable evidence.
    kEphemeral,
  };
  static WorkspaceService& ForProfile(Profile& profile);
  ~WorkspaceService() override;

  StorageState storage_state() const;
  std::string storage_error() const;
  base::CallbackListSubscription Subscribe(base::RepeatingClosure callback);
  void RetryLoad();
  void RetrySave();
  // Queue an owned latest dirty snapshot before ThreadPool shutdown. Leaves
  // normal write replies/retry state intact if quit is cancelled.
  void FlushForShutdown();

  // ok means applied in memory only. Observe storage_state separately. All IDs
  // here are Rust model IDs. Profile 1 is local to each service, not a native
  // ID.
  foundation::MutationResult CreateSpace(std::string_view name);
  foundation::MutationResult RenameSpace(uint64_t space, std::string_view name);
  foundation::MutationResult CreateWindow(uint64_t space);
  foundation::MutationResult CloseWindow(uint64_t window);
  foundation::MutationResult SelectSpace(uint64_t window, uint64_t space);
  foundation::MutationResult AddTemporaryItem(uint64_t space);
  foundation::MutationResult SelectItem(uint64_t window, uint64_t item);
  // Call only when the LAST live projection closes. Saved identity survives.
  foundation::MutationResult LiveTabClosed(uint64_t item);
  // has_url=false preserves existing saved URL; first save requires explicit
  // URL.
  foundation::MutationResult SetPlacement(uint64_t item,
                                          foundation::PlacementKind placement,
                                          uint64_t owner,
                                          bool has_url,
                                          std::string_view canonical_url);
  // Saved items only. Pass the live page title while it shows the saved URL;
  // Rust bounds it. Never notifies or writes by itself: a changed title is
  // saved with the next metadata write or the shutdown flush.
  foundation::MutationResult SetTitle(uint64_t item, std::string_view title);
  foundation::MutationResult RemoveSavedItem(uint64_t item);
  foundation::MutationResult RevealItem(uint64_t window, uint64_t item);
  foundation::MutationResult UnsaveItem(uint64_t item, uint64_t live_space);
  foundation::MutationResult MoveItemBefore(uint64_t item, uint64_t before);
  foundation::MutationResult ReconcileLiveItems(
      base::span<const uint64_t> live);
  foundation::SidebarResult Sidebar(uint64_t window) const;
  std::optional<foundation::StateSnapshot> ExportSnapshot() const;
  foundation::MutationResult RestoreSnapshot(
      foundation::StateSnapshot snapshot);

 private:
  explicit WorkspaceService(Profile& profile);
  bool CanMutate() const;
  foundation::MutationResult Changed(foundation::MutationResult result);
  void StartLoad();
  void OnLoaded(internal::MetadataResult result);
  void ScheduleWrite();
  void OnWritten(bool ok);

  // Copies only, destructor/background work never dereference the dead Profile.
  const bool ephemeral_;
  const base::FilePath metadata_path_;
  const scoped_refptr<base::SequencedTaskRunner> io_runner_;
  rust::Box<foundation::Workspace> workspace_;
  StorageState storage_state_;
  std::string storage_error_;
  bool dirty_ = false;
  bool write_in_flight_ = false;
  base::RepeatingClosureList changed_;
  SEQUENCE_CHECKER(sequence_checker_);
  // Invalidates replies on destruction. Queued writes own their path/records
  // and block normal shutdown. Ephemeral Profiles queue none.
  base::WeakPtrFactory<WorkspaceService> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_WORKSPACE_SERVICE_H_
