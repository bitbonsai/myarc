#ifndef MYARC_FEEDS_SERVICE_H_
#define MYARC_FEEDS_SERVICE_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "myarc/bridge.rs.h"
#include "third_party/rust/cxx/v1/cxx.h"

class Profile;
namespace base {
class SequencedTaskRunner;
}

namespace myarc {

// Profile-scoped RSS/Atom/JSON Feed subscription store.
// Never redirects OTR/Guest Profiles to GetOriginalProfile(): private windows
// get an ephemeral, non-persisted store with zero leakage.
class FeedsService final : public base::SupportsUserData::Data {
 public:
  static FeedsService& ForProfile(Profile& profile);
  ~FeedsService() override;

  bool IsEphemeral() const { return ephemeral_; }
  bool IsSubscribed(std::string_view feed_url) const;

  foundation::MutationResult Subscribe(std::string_view feed_url,
                                       std::string_view title,
                                       std::string_view site_url,
                                       uint64_t space_id);
  bool UnsubscribeByUrl(std::string_view feed_url);

  // Unread items of the subscription at `feed_url`, or 0 when there is none.
  uint32_t UnreadCount(std::string_view feed_url) const;
  // Records the fetched `body` of a subscribed feed. Not ok when the feed
  // isn't subscribed or doesn't parse.
  foundation::FeedFetchRecord RecordFetch(std::string_view feed_url,
                                          base::span<const uint8_t> body);
  bool MarkItemRead(std::string_view feed_url, std::string_view item_id);
  // Marks every subscription in `feed_urls` read. Returns whether any
  // unread count changed.
  bool MarkAllRead(const std::vector<std::string>& feed_urls);
  // Runs `callback` after any unread count changes.
  base::CallbackListSubscription AddUnreadObserver(
      base::RepeatingClosure callback);

 private:
  explicit FeedsService(Profile& profile);

  void SaveNow();
  void NotifyUnreadChanged();

  bool ephemeral_ = false;
  base::FilePath path_;
  scoped_refptr<base::SequencedTaskRunner> io_runner_;
  rust::Box<foundation::FeedStore> store_;
  base::RepeatingClosureList unread_observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace myarc

#endif  // MYARC_FEEDS_SERVICE_H_
