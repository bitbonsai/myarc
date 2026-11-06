#ifndef MYARC_WORKSPACE_SERVICE_H_
#define MYARC_WORKSPACE_SERVICE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
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
// Never redirect OTR windows to GetOriginalProfile(); only Settings edits the
// original Profile's service on purpose. Controls retain subscriptions,
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
  // Runs `done` once loading ends, ready or failed, or now when it has.
  // Dropped if the service dies first.
  void WhenLoaded(base::OnceClosure done);
  void RetryLoad();
  void RetrySave();
  // Queue an owned latest dirty snapshot before ThreadPool shutdown. Leaves
  // normal write replies/retry state intact if quit is cancelled.
  void FlushForShutdown();

  // ok means applied in memory only. Observe storage_state separately. All IDs
  // here are Rust model IDs. Profile 1 is local to each service, not a native
  // ID.
  foundation::MutationResult CreateSpace(std::string_view name);
  // `icon` is one emoji or a bundled icon name, per `icon_kind`; empty for
  // Unset, the default glyph. An unset `theme` keeps the default tint.
  foundation::MutationResult CreateSpace(
      std::string_view name,
      foundation::SpaceIconKind icon_kind,
      std::string_view icon,
      const foundation::SpaceThemeRecord& theme);
  // The same, except that in a workspace still in its untouched default
  // state the new Space replaces the default Space and keeps its ID.
  foundation::MutationResult CreateFirstSpace(
      std::string_view name,
      foundation::SpaceIconKind icon_kind,
      std::string_view icon,
      const foundation::SpaceThemeRecord& theme);
  // The agent Space's ID, created when missing. Leaves every window's
  // selected Space alone.
  foundation::MutationResult EnsureAgentSpace();
  foundation::MutationResult RenameSpace(uint64_t space, std::string_view name);
  // `id` is 1 when the icon changed, else 0. Unset restores the default glyph.
  foundation::MutationResult SetSpaceIcon(uint64_t space,
                                          foundation::SpaceIconKind icon_kind,
                                          std::string_view icon);
  // `id` is 1 when the theme changed, else 0. Unset restores the default tint.
  foundation::MutationResult SetSpaceTheme(
      uint64_t space,
      const foundation::SpaceThemeRecord& theme);
  foundation::MutationResult CreateWindow(uint64_t space);
  foundation::MutationResult CloseWindow(uint64_t window);
  foundation::MutationResult SelectSpace(uint64_t window, uint64_t space);
  foundation::MutationResult AddTemporaryItem(uint64_t space);
  foundation::MutationResult SelectItem(uint64_t window, uint64_t item);
  // Call only when the LAST live projection closes. Saved identity survives.
  foundation::MutationResult LiveTabClosed(uint64_t item);
  // LiveTabClosed that also archives a temporary item with the page's URL and
  // title for the Library. Off-the-record Profiles archive nothing.
  foundation::MutationResult ArchiveClosedTab(uint64_t item,
                                              std::string_view url,
                                              std::string_view title);
  // One Auto Archive pass on the browser-open clock, which advances by the
  // monotonic time since the previous pass: records views of `viewed`
  // (visible tabs), selected and new temporary items, then returns the idle
  // items among `closable` that are due. The caller closes their pages, which
  // archives them. `media` lists items playing or capturing media. Pass empty
  // `closable` to only record views. Off-the-record Profiles get nothing.
  rust::Vec<uint64_t> AutoArchive(base::span<const uint64_t> viewed,
                                  base::span<const uint64_t> closable,
                                  base::span<const uint64_t> media);
  // Unexpired Archived Tabs, newest first. Empty while not loaded.
  rust::Vec<foundation::ArchivedTabRecord> ArchivedTabs() const;
  // Call after the entry has been reopened.
  foundation::MutationResult RemoveArchivedTab(uint64_t id);
  // Clear Archive: drops every Archived Tab and saves.
  foundation::MutationResult ClearArchivedTabs();
  // This Profile's "Archive tabs after" in seconds (12 hours, 24 hours, 7 days
  // or 30 days); 0 while not loaded. Set rejects any other value. The next
  // Auto Archive pass uses the new interval.
  uint64_t ArchiveIntervalSecs() const;
  foundation::MutationResult SetArchiveInterval(uint64_t secs);
  // History deletion: drops Archived Tabs whose URL is in `urls`, and with
  // `range` those closed between its begin and end, inclusive. A null end
  // means no end. Writes only when an entry went.
  void ForgetArchivedTabs(
      base::span<const std::string> urls,
      std::optional<std::pair<base::Time, base::Time>> range);
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
  // Temporary items only. Remembers the page a dormant row reopens when its tab
  // is not restored, with its full URL; URLs outside the saved-URL gate are
  // ignored. Writes when the URL changes, without a notify. Callers skip it
  // when the Profile saves no history.
  foundation::MutationResult SetTemporaryPage(uint64_t item,
                                              std::string_view url,
                                              std::string_view title);
  // A dormant temporary row of `space` remembering the page at `url` and not
  // in `live`, or zero.
  uint64_t DormantTemporaryItem(uint64_t space,
                                std::string_view url,
                                base::span<const uint64_t> live) const;
  foundation::MutationResult RemoveSavedItem(uint64_t item);
  foundation::MutationResult RevealItem(uint64_t window, uint64_t item);
  foundation::MutationResult UnsaveItem(uint64_t item, uint64_t live_space);
  foundation::MutationResult MoveItemBefore(uint64_t item, uint64_t before);
  // Zero before moves the Space to the end.
  foundation::MutationResult MoveSpaceBefore(uint64_t space, uint64_t before);
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
  // Changed, then rewrites this Profile's entry in the Spaces index.
  foundation::MutationResult SpacesChanged(foundation::MutationResult result);
  void UpdateSpacesIndex();
  void StartLoad();
  void OnLoaded(internal::MetadataResult result);
  void ScheduleWrite();
  void OnWritten(bool ok);

  // Copies only, destructor/background work never dereference the dead Profile.
  const bool ephemeral_;
  // The Profile's directory name, its key in the Spaces index.
  const std::string profile_dir_;
  const base::FilePath metadata_path_;
  const scoped_refptr<base::SequencedTaskRunner> io_runner_;
  rust::Box<foundation::Workspace> workspace_;
  StorageState storage_state_;
  std::string storage_error_;
  bool dirty_ = false;
  bool write_in_flight_ = false;
  // When the last Auto Archive pass advanced the clock; null before the first.
  base::TimeTicks last_pass_;
  base::RepeatingClosureList changed_;
  std::vector<base::OnceClosure> when_loaded_;
  SEQUENCE_CHECKER(sequence_checker_);
  // Invalidates replies on destruction. Queued writes own their path/records
  // and block normal shutdown. Ephemeral Profiles queue none.
  base::WeakPtrFactory<WorkspaceService> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_WORKSPACE_SERVICE_H_
