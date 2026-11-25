#include "myarc/media_library_service.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <set>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "myarc/media_library_mac.h"
#include "myarc/media_prefs.h"

namespace myarc {
namespace {

const void* const kMediaLibraryServiceKey = &kMediaLibraryServiceKey;

constexpr base::TimeDelta kWatcherDebounce = base::Milliseconds(300);
constexpr char kTestMediaDirSwitch[] = "myarc-test-media-dir";

uint64_t ComputeMediaId(const base::FilePath& path, base::Time modified) {
  std::string key = path.AsUTF8Unsafe();
  key.push_back('\0');
  const int64_t us =
      modified.ToDeltaSinceWindowsEpoch().InMicroseconds();
  key.append(reinterpret_cast<const char*>(&us), sizeof(us));
  uint64_t h = base::PersistentHash(key);
  return h ? h : 1u;
}

bool IsInsideDirectory(const base::FilePath& dir, const base::FilePath& child) {
  if (dir.empty() || child.empty()) {
    return false;
  }
  return dir.IsParent(child);
}

}  // namespace

bool IsAllowedMediaExtension(const base::FilePath& path, bool* is_video) {
  const std::string ext = base::ToLowerASCII(path.Extension());
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" ||
      ext == ".gif" || ext == ".heic" || ext == ".heif" || ext == ".tiff" ||
      ext == ".tif" || ext == ".bmp") {
    if (is_video) {
      *is_video = false;
    }
    return true;
  }
  if (ext == ".mov" || ext == ".mp4" || ext == ".m4v" || ext == ".webm") {
    if (is_video) {
      *is_video = true;
    }
    return true;
  }
  return false;
}

std::vector<MediaRecord> ScanMediaDirectories(
    const std::vector<base::FilePath>& dirs,
    bool* permission_denied) {
  if (permission_denied) {
    *permission_denied = false;
  }
  std::vector<base::FilePath> canonical_dirs;
  std::set<base::FilePath> seen_dirs;
  for (const base::FilePath& dir : dirs) {
    if (dir.empty()) {
      continue;
    }
    base::FilePath abs = base::MakeAbsoluteFilePath(dir);
    if (abs.empty()) {
      abs = dir;
    }
    if (seen_dirs.insert(abs).second) {
      canonical_dirs.push_back(std::move(abs));
    }
  }

  std::vector<MediaRecord> results;
  std::set<base::FilePath> seen_files;

  for (const base::FilePath& dir : canonical_dirs) {
    base::FileEnumerator enumerator(
        dir, /*recursive=*/false, base::FileEnumerator::FILES,
        base::FilePath::StringType(),
        base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
        base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
    size_t inspected = 0;
    for (base::FilePath entry = enumerator.Next(); !entry.empty();
         entry = enumerator.Next()) {
      if (inspected++ >= kMaxMediaScanEntries) {
        break;
      }
      const base::FilePath base_name = entry.BaseName();
      const std::string name_utf8 = base_name.AsUTF8Unsafe();
      if (name_utf8.empty() || name_utf8[0] == '.') {
        continue;
      }
      bool is_video = false;
      if (!IsAllowedMediaExtension(entry, &is_video)) {
        continue;
      }
      // Reject symlinks that escape the watched directory.
      if (base::IsLink(entry)) {
        base::FilePath resolved = base::MakeAbsoluteFilePath(entry);
        if (resolved.empty() || !IsInsideDirectory(dir, resolved)) {
          continue;
        }
      }
      const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
      if (info.IsDirectory()) {
        continue;
      }
      if (!seen_files.insert(entry).second) {
        continue;
      }
      MediaRecord record;
      record.path = entry;
      record.filename = base::UTF8ToUTF16(name_utf8);
      record.modified = info.GetLastModifiedTime();
      record.file_size = info.GetSize();
      record.is_video = is_video;
      record.id = ComputeMediaId(entry, record.modified);
      results.push_back(std::move(record));
    }
    const base::File::Error err = enumerator.GetError();
    if (err == base::File::FILE_ERROR_ACCESS_DENIED && permission_denied) {
      *permission_denied = true;
    }
  }

  std::ranges::sort(results, [](const MediaRecord& a, const MediaRecord& b) {
    if (a.modified != b.modified) {
      return a.modified > b.modified;
    }
    return a.filename < b.filename;
  });
  if (results.size() > kMaxMediaItems) {
    results.resize(kMaxMediaItems);
  }
  return results;
}

// static
MediaLibraryService& MediaLibraryService::ForProfile(Profile& profile) {
  auto* service = static_cast<MediaLibraryService*>(
      profile.GetUserData(kMediaLibraryServiceKey));
  if (!service) {
    auto owned =
        std::unique_ptr<MediaLibraryService>(new MediaLibraryService(profile));
    service = owned.get();
    profile.SetUserData(kMediaLibraryServiceKey, std::move(owned));
  }
  return *service;
}

MediaLibraryService::MediaLibraryService(Profile& profile)
    : profile_(&profile),
      off_the_record_(profile.IsOffTheRecord() || profile.IsGuestSession()),
      io_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

MediaLibraryService::~MediaLibraryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopWatchers();
}

bool MediaLibraryService::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (off_the_record_) {
    return otr_enabled_;
  }
  const PrefService* prefs = profile_->GetPrefs();
  return prefs && prefs->FindPreference(kMediaLibraryEnabledPref) &&
         prefs->GetBoolean(kMediaLibraryEnabledPref);
}

void MediaLibraryService::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (off_the_record_) {
    otr_enabled_ = enabled;
  } else if (PrefService* prefs = profile_->GetPrefs()) {
    prefs->SetBoolean(kMediaLibraryEnabledPref, enabled);
  }
  ++generation_;
  if (!enabled) {
    StopWatchers();
    items_.clear();
    lru_order_.clear();
    thumbnails_.clear();
    pending_thumbnails_.clear();
    permission_denied_ = false;
    changed_.Notify();
    return;
  }
  if (shown_) {
    StartWatchers();
  }
  StartScan();
  changed_.Notify();
}

void MediaLibraryService::SetShown(bool shown) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (shown_ == shown) {
    return;
  }
  shown_ = shown;
  if (!shown_) {
    StopWatchers();
    return;
  }
  if (IsEnabled()) {
    StartWatchers();
    StartScan();
  }
}

std::vector<MediaRecord> MediaLibraryService::GetItems() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnabled()) {
    return {};
  }
  std::vector<MediaRecord> out;
  out.reserve(items_.size());
  for (const MediaRecord& record : items_) {
    MediaRecord copy = record;
    const CacheKey key{record.path, record.modified, record.file_size};
    copy.thumbnail = LookupThumbnail(key);
    if (copy.thumbnail.isNull()) {
      RequestThumbnail(record);
    }
    out.push_back(std::move(copy));
  }
  return out;
}

std::optional<MediaRecord> MediaLibraryService::FindItem(uint64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const MediaRecord& record : items_) {
    if (record.id == id) {
      MediaRecord copy = record;
      const CacheKey key{record.path, record.modified, record.file_size};
      if (auto it = thumbnails_.find(key); it != thumbnails_.end()) {
        copy.thumbnail = it->second.first;
      }
      return copy;
    }
  }
  return std::nullopt;
}

void MediaLibraryService::RequestPreview(
    uint64_t id,
    base::OnceCallback<void(gfx::ImageSkia)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto item = FindItem(id);
  if (!item) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  DecodeMediaPreviewMac(item->path, item->is_video, kMediaLightboxMaxPx,
                        std::move(callback));
}

void MediaLibraryService::OpenInPreview(uint64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto item = FindItem(id)) {
    OpenMediaInPreviewMac(item->path, item->is_video);
  }
}

void MediaLibraryService::RevealInFinder(uint64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto item = FindItem(id)) {
    RevealMediaInFinderMac(item->path);
  }
}

void MediaLibraryService::CopyFile(uint64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto item = FindItem(id)) {
    CopyMediaFileMac(item->path, item->is_video);
  }
}

base::CallbackListSubscription MediaLibraryService::Subscribe(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return changed_.Add(std::move(callback));
}

std::vector<base::FilePath> MediaLibraryService::ResolveDirectories() const {
  if (const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
      cmd && cmd->HasSwitch(kTestMediaDirSwitch)) {
    base::FilePath dir = cmd->GetSwitchValuePath(kTestMediaDirSwitch);
    if (!dir.empty()) {
      return {dir};
    }
  }
  std::vector<base::FilePath> dirs;
  if (base::FilePath screenshots = GetMacScreenshotsDirectory();
      !screenshots.empty()) {
    dirs.push_back(std::move(screenshots));
  }
  base::FilePath downloads;
  if (!off_the_record_) {
    downloads = DownloadPrefs::FromBrowserContext(profile_)->DownloadPath();
  }
  if (downloads.empty()) {
    downloads = base::GetHomeDir().AppendASCII("Downloads");
  }
  if (!downloads.empty()) {
    dirs.push_back(std::move(downloads));
  }
  return dirs;
}

void MediaLibraryService::StartScan() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnabled()) {
    return;
  }
  const uint64_t gen = ++generation_;
  std::vector<base::FilePath> dirs = ResolveDirectories();
  struct ScanResult {
    std::vector<MediaRecord> items;
    bool permission_denied = false;
  };
  io_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<base::FilePath> dirs) {
            ScanResult out;
            out.items = ScanMediaDirectories(dirs, &out.permission_denied);
            return out;
          },
          std::move(dirs)),
      base::BindOnce(
          [](base::WeakPtr<MediaLibraryService> self, uint64_t gen,
             ScanResult result) {
            if (self) {
              self->OnScanComplete(gen, std::move(result.items),
                                   result.permission_denied);
            }
          },
          weak_factory_.GetWeakPtr(), gen));
}

void MediaLibraryService::OnScanComplete(uint64_t generation,
                                         std::vector<MediaRecord> items,
                                         bool permission_denied) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (generation != generation_ || !IsEnabled()) {
    return;
  }
  items_ = std::move(items);
  permission_denied_ = permission_denied;
  changed_.Notify();
}

class MediaLibraryService::DirectoryWatchers {
 public:
  DirectoryWatchers(std::vector<base::FilePath> dirs,
                    scoped_refptr<base::SequencedTaskRunner> ui_runner,
                    base::WeakPtr<MediaLibraryService> owner) {
    for (const base::FilePath& dir : dirs) {
      auto watcher = std::make_unique<base::FilePathWatcher>();
      if (watcher->Watch(
              dir, base::FilePathWatcher::Type::kNonRecursive,
              base::BindRepeating(
                  [](scoped_refptr<base::SequencedTaskRunner> runner,
                     base::WeakPtr<MediaLibraryService> target,
                     const base::FilePath& path, bool error) {
                    runner->PostTask(
                        FROM_HERE,
                        base::BindOnce(&MediaLibraryService::OnDirectoryChanged,
                                       target, path, error));
                  },
                  ui_runner, owner))) {
        watchers_.push_back(std::move(watcher));
      }
    }
  }
  ~DirectoryWatchers() = default;

 private:
  std::vector<std::unique_ptr<base::FilePathWatcher>> watchers_;
};

void MediaLibraryService::StartWatchers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopWatchers();
  std::vector<base::FilePath> dirs = ResolveDirectories();
  if (dirs.empty()) {
    return;
  }
  watchers_.emplace(io_runner_, std::move(dirs),
                    base::SequencedTaskRunner::GetCurrentDefault(),
                    weak_factory_.GetWeakPtr());
}

void MediaLibraryService::StopWatchers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  debounce_timer_.Stop();
  watchers_.Reset();
}

void MediaLibraryService::OnDirectoryChanged(const base::FilePath& path,
                                             bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!shown_ || !IsEnabled()) {
    return;
  }
  debounce_timer_.Start(FROM_HERE, kWatcherDebounce,
                        base::BindOnce(&MediaLibraryService::StartScan,
                                       weak_factory_.GetWeakPtr()));
}

void MediaLibraryService::RequestThumbnail(const MediaRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const CacheKey key{record.path, record.modified, record.file_size};
  if (thumbnails_.contains(key) || !pending_thumbnails_.insert(key).second) {
    return;
  }
  const uint64_t gen = generation_;
  GenerateMediaThumbnailMac(
      record.path, kMediaThumbnailMaxPx,
      base::BindOnce(&MediaLibraryService::OnThumbnailReady,
                     weak_factory_.GetWeakPtr(), gen, key));
}

void MediaLibraryService::OnThumbnailReady(uint64_t generation,
                                           CacheKey key,
                                           gfx::ImageSkia image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_thumbnails_.erase(key);
  if (generation != generation_ || !IsEnabled() || image.isNull()) {
    return;
  }
  StoreThumbnail(key, std::move(image));
  changed_.Notify();
}

gfx::ImageSkia MediaLibraryService::LookupThumbnail(const CacheKey& key) {
  auto it = thumbnails_.find(key);
  if (it == thumbnails_.end()) {
    return gfx::ImageSkia();
  }
  lru_order_.splice(lru_order_.begin(), lru_order_, it->second.second);
  return it->second.first;
}

void MediaLibraryService::StoreThumbnail(const CacheKey& key,
                                         gfx::ImageSkia image) {
  if (auto it = thumbnails_.find(key); it != thumbnails_.end()) {
    it->second.first = std::move(image);
    lru_order_.splice(lru_order_.begin(), lru_order_, it->second.second);
    return;
  }
  if (thumbnails_.size() >= kMaxCachedMediaThumbnails && !lru_order_.empty()) {
    const CacheKey oldest = lru_order_.back();
    lru_order_.pop_back();
    thumbnails_.erase(oldest);
  }
  lru_order_.push_front(key);
  thumbnails_.emplace(key, std::make_pair(std::move(image), lru_order_.begin()));
}

}  // namespace myarc
