#include "myarc/feeds_service.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"

namespace myarc {
namespace {

const void* const kFeedsServiceKey = &kFeedsServiceKey;
constexpr base::FilePath::CharType kFeedsFileName[] =
    FILE_PATH_LITERAL("myarc-feeds-v1.json");

void WriteFeedsFileBlocking(const base::FilePath& path, std::string json) {
  base::ImportantFileWriter::WriteFileAtomically(path, json);
}

uint64_t NowUnixSecs() {
  const int64_t secs = base::Time::Now().InSecondsFSinceUnixEpoch();
  return secs > 0 ? static_cast<uint64_t>(secs) : 0;
}

}  // namespace

// static
FeedsService& FeedsService::ForProfile(Profile& profile) {
  auto* existing =
      static_cast<FeedsService*>(profile.GetUserData(kFeedsServiceKey));
  if (existing) {
    return *existing;
  }
  auto created = base::WrapUnique(new FeedsService(profile));
  FeedsService* ptr = created.get();
  profile.SetUserData(kFeedsServiceKey, std::move(created));
  return *ptr;
}

FeedsService::FeedsService(Profile& profile)
    : ephemeral_(profile.IsOffTheRecord() || profile.IsGuestSession()),
      store_(foundation::feed_store_new()) {
  if (!ephemeral_) {
    path_ = profile.GetPath().Append(kFeedsFileName);
    io_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    std::string raw;
    if (base::ReadFileToStringWithMaxSize(path_, &raw,
                                          foundation::feed_max_bytes())) {
      rust::Slice<const uint8_t> slice(
          reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
      store_ = foundation::feed_store_load_json(slice);
    }
  }
}

FeedsService::~FeedsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FeedsService::IsSubscribed(std::string_view feed_url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return foundation::feed_store_is_subscribed(*store_, std::string(feed_url));
}

foundation::MutationResult FeedsService::Subscribe(std::string_view feed_url,
                                                   std::string_view title,
                                                   std::string_view site_url,
                                                   uint64_t space_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = foundation::feed_store_subscribe(
      *store_, std::string(feed_url), std::string(title), std::string(site_url),
      space_id, NowUnixSecs());
  if (result.ok) {
    SaveNow();
  }
  return result;
}

bool FeedsService::UnsubscribeByUrl(std::string_view feed_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool removed =
      foundation::feed_store_unsubscribe_url(*store_, std::string(feed_url));
  if (removed) {
    SaveNow();
  }
  return removed;
}

void FeedsService::SaveNow() {
  if (ephemeral_ || !io_runner_) {
    return;
  }
  std::string json(foundation::feed_store_save_json(*store_));
  io_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteFeedsFileBlocking, path_, std::move(json)));
}

}  // namespace myarc
