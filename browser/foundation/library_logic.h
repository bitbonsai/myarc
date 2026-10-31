#ifndef MYARC_LIBRARY_LOGIC_H_
#define MYARC_LIBRARY_LOGIC_H_

#include <cstddef>
#include <string_view>

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

// Width of the Spaces section, rail included, for `spaces` columns. The
// browser layout still caps it to the window.
int SpacesWidth(size_t spaces);

// Case-insensitive substring match; an empty query matches everything.
bool MatchesSearch(std::u16string_view text, std::u16string_view query);

}  // namespace myarc::library

#endif  // MYARC_LIBRARY_LOGIC_H_
