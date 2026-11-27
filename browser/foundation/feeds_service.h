#ifndef MYARC_FEEDS_SERVICE_H_
#define MYARC_FEEDS_SERVICE_H_

#include <cstdint>
#include <string>
#include <string_view>

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

 private:
  explicit FeedsService(Profile& profile);

  void SaveNow();

  bool ephemeral_ = false;
  base::FilePath path_;
  scoped_refptr<base::SequencedTaskRunner> io_runner_;
  rust::Box<foundation::FeedStore> store_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace myarc

#endif  // MYARC_FEEDS_SERVICE_H_
