#ifndef MYARC_SPACES_INDEX_H_
#define MYARC_SPACES_INDEX_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/values.h"

class PrefRegistrySimple;
class PrefService;

namespace myarc {

// Every Profile's Spaces in one Local State dictionary, so any window can list
// them without loading another Profile. Keys are Profile directory names, and
// each value lists that Profile's Spaces in order:
//   {"Profile 1": [{"id": "2", "name": "Client", "icon_kind": 1,
//                   "icon": "🚀"}]}
// Each Profile's WorkspaceService rewrites only its own entry. Readers treat
// the dictionary as untrusted: Local State is a file on disk.
inline constexpr char kSpacesIndexPref[] = "myarc.spaces_index";
inline constexpr size_t kMaxIndexedProfiles = 32;
// Match the workspace core's limits.
inline constexpr size_t kMaxIndexedSpaces = 64;
inline constexpr size_t kMaxIndexedNameBytes = 128;
inline constexpr size_t kMaxIndexedIconBytes = 64;
inline constexpr size_t kMaxProfileDirBytes = 255;

struct IndexedSpace {
  // The same values as foundation::SpaceIconKind.
  enum class IconKind : int { kUnset = 0, kEmoji = 1, kNamed = 2 };

  std::string profile_dir;
  uint64_t space_id = 0;
  std::string name;
  IconKind icon_kind = IconKind::kUnset;
  std::string icon;
  // Position among its Profile's Spaces, from 0.
  size_t order = 0;
  bool operator==(const IndexedSpace&) const = default;
};

void RegisterSpacesIndexPref(PrefRegistrySimple& registry);

// A plain directory name: valid UTF-8, no separators, not "." or "..".
bool IsIndexableProfileDir(std::string_view dir);

// Replaces `profile_dir`'s entry in `index` with `spaces` (their profile_dir
// and order are ignored), dropping it when `spaces` is empty. Skips invalid
// Spaces and keeps the first kMaxIndexedSpaces. A new Profile isn't added once
// the index holds kMaxIndexedProfiles. Returns whether `index` changed.
bool MergeIndexedSpaces(base::DictValue& index,
                        std::string_view profile_dir,
                        base::span<const IndexedSpace> spaces);

// Every valid Space in `index`, grouped by Profile directory in key order and
// in order within each. Skips bad Profiles and Spaces, and reads at most
// kMaxIndexedProfiles Profiles of kMaxIndexedSpaces Spaces each.
std::vector<IndexedSpace> DecodeSpacesIndex(const base::DictValue& index);

// Drops every entry whose Profile directory `keep` rejects. Returns whether
// `index` changed.
bool PruneSpacesIndex(base::DictValue& index,
                      base::FunctionRef<bool(std::string_view)> keep);

// Whether the index may keep a Profile directory's entry: the Profile is
// listed in ProfileAttributesStorage and isn't marked for deletion. The
// browser sets it once, since this target can't depend on the Profile
// manager. Until then nothing is dropped.
using IndexedProfileFilter = bool (*)(std::string_view profile_dir);
void SetIndexedProfileFilter(IndexedProfileFilter filter);

// The same, on Local State. UI sequence only. Each write first drops the
// entries the filter rejects, so deleted Profiles never count toward
// kMaxIndexedProfiles, and writes nothing for a rejected `profile_dir`.
void WriteIndexedSpaces(PrefService& local_state,
                        std::string_view profile_dir,
                        base::span<const IndexedSpace> spaces);
// Drops the entries the filter rejects, as when a Profile is removed.
void PruneSpacesIndex(PrefService& local_state);
std::vector<IndexedSpace> ReadSpacesIndex(const PrefService& local_state);

}  // namespace myarc

#endif  // MYARC_SPACES_INDEX_H_
