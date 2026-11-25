#ifndef MYARC_MEDIA_LIBRARY_SERVICE_H_
#define MYARC_MEDIA_LIBRARY_SERVICE_H_

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace myarc {

// Hard bounds for local media discovery and thumbnail caching.
inline constexpr size_t kMaxMediaScanEntries = 2000;
inline constexpr size_t kMaxMediaItems = 120;
inline constexpr size_t kMaxCachedMediaThumbnails = 120;

struct MediaRecord {
  uint64_t id = 0;
  std::u16string filename;
  base::FilePath path;
  base::Time modified;
  int64_t file_size = 0;
  bool is_video = false;
  gfx::ImageSkia thumbnail;
};

// Pure helpers, also exercised by bridge_check.
bool IsAllowedMediaExtension(const base::FilePath& path,
                             bool* is_video = nullptr);

// Blocking. Shallow, symlink-safe scan of `dirs` bounded by
// kMaxMediaScanEntries per directory and kMaxMediaItems total, sorted by
// modified time descending. Sets `permission_denied` if any directory failed
// with EPERM/EACCES.
std::vector<MediaRecord> ScanMediaDirectories(
    const std::vector<base::FilePath>& dirs,
    bool* permission_denied = nullptr);

// UI-sequence only. Exact Profile owns this service. Scans local screenshot
// and download folders only after explicit per-Profile opt-in, caches bounded
// thumbnails off the UI thread, and watches directories while the Library
// Media section is open.
class MediaLibraryService final : public base::SupportsUserData::Data {
 public:
  static MediaLibraryService& ForProfile(Profile& profile);
  ~MediaLibraryService() override;

  bool IsEnabled() const;
  void SetEnabled(bool enabled);
  bool PermissionDenied() const { return permission_denied_; }

  // Starts/stops directory watchers and triggers a refresh while shown.
  void SetShown(bool shown);

  // Current items, newest first, with any cached thumbnails attached. Kicks
  // off async thumbnail generation for missing thumbnails.
  std::vector<MediaRecord> GetItems();
  std::optional<MediaRecord> FindItem(uint64_t id) const;

  void RequestPreview(uint64_t id,
                      base::OnceCallback<void(gfx::ImageSkia)> callback);
  void OpenInPreview(uint64_t id) const;
  void RevealInFinder(uint64_t id) const;
  void CopyFile(uint64_t id) const;

  base::CallbackListSubscription Subscribe(base::RepeatingClosure callback);

 private:
  struct CacheKey {
    base::FilePath path;
    base::Time modified;
    int64_t file_size = 0;
    auto operator<=>(const CacheKey&) const = default;
  };

  explicit MediaLibraryService(Profile& profile);

  std::vector<base::FilePath> ResolveDirectories() const;
  void StartScan();
  void OnScanComplete(uint64_t generation,
                      std::vector<MediaRecord> items,
                      bool permission_denied);
  void StartWatchers();
  void StopWatchers();
  void OnDirectoryChanged(const base::FilePath& path, bool error);
  void RequestThumbnail(const MediaRecord& record);
  void OnThumbnailReady(uint64_t generation,
                        CacheKey key,
                        gfx::ImageSkia image);
  gfx::ImageSkia LookupThumbnail(const CacheKey& key);
  void StoreThumbnail(const CacheKey& key, gfx::ImageSkia image);

  const raw_ptr<Profile> profile_;
  const bool off_the_record_;
  // Off-the-record Profiles never read or write the regular Profile's pref.
  bool otr_enabled_ = false;
  bool shown_ = false;
  bool permission_denied_ = false;
  uint64_t generation_ = 0;

  std::vector<MediaRecord> items_;
  // Bounded LRU thumbnail cache keyed by {path, modified, file_size}.
  std::list<CacheKey> lru_order_;
  std::map<CacheKey,
           std::pair<gfx::ImageSkia, std::list<CacheKey>::iterator>>
      thumbnails_;
  std::set<CacheKey> pending_thumbnails_;

  class DirectoryWatchers;

  scoped_refptr<base::SequencedTaskRunner> io_runner_;
  base::SequenceBound<DirectoryWatchers> watchers_;
  base::OneShotTimer debounce_timer_;
  base::RepeatingClosureList changed_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MediaLibraryService> weak_factory_{this};
};

}  // namespace myarc

#endif  // MYARC_MEDIA_LIBRARY_SERVICE_H_
