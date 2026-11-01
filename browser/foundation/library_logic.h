#ifndef MYARC_LIBRARY_LOGIC_H_
#define MYARC_LIBRARY_LOGIC_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"

namespace myarc::library {

// Measured on Arc's Library at 1440x900 DIP. The page starts at the Library's
// edge: 417 DIP for the lists.
inline constexpr int kRailWidth = 112;
inline constexpr int kListWidth = 417;
inline constexpr int kColumnWidth = 240;
inline constexpr int kColumnGap = 24;
// Rail edge to the first Space column.
inline constexpr int kColumnsLeading = 28;
// Last column to the create button, the button, and the gap to the page.
inline constexpr int kCreateGap = 32;
inline constexpr int kCreateSize = 30;
inline constexpr int kCreateTrailing = 38;

// Marks a Pin's focus key in the Spaces section, apart from Space IDs.
inline constexpr uint64_t kPinKey = uint64_t{1} << 63;

// Width of the Spaces section, rail included, for `spaces` columns. The
// browser layout still caps it to the window.
int SpacesWidth(size_t spaces);

// Case-insensitive substring match; an empty query matches everything.
bool MatchesSearch(std::u16string_view text, std::u16string_view query);

// Arc's day headers in Archived Tabs: "Today", "Yesterday" or "N days ago",
// counted in local calendar days before `now`.
std::u16string DayLabel(base::Time time, base::Time now);

// Rows in the downloads stack above the Library button.
inline constexpr size_t kStackRows = 4;

// The downloads stack's rows from a newest-first list: the most recent
// complete downloads, oldest first, so the newest sits by the button.
template <typename T>
std::vector<T> StackRows(const std::vector<T>& newest_first) {
  std::vector<T> rows;
  for (const T& item : newest_first) {
    if (rows.size() == kStackRows) {
      break;
    }
    if (item.complete) {
      rows.push_back(item);
    }
  }
  std::ranges::reverse(rows);
  return rows;
}

// A download's age in the stack: "Just now" under a minute, then
// "N minutes ago", "N hours ago" or "N days ago".
std::u16string AgeLabel(base::Time time, base::Time now);

}  // namespace myarc::library

#endif  // MYARC_LIBRARY_LOGIC_H_
