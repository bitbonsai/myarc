#include "myarc/library_logic.h"

#include <string>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"

namespace myarc::library {

int SpacesWidth(size_t spaces) {
  // Bounded by the workspace model; clamp so the sum cannot overflow.
  const int columns = static_cast<int>(spaces < 64 ? spaces : 64);
  const int content =
      columns ? columns * kColumnWidth + (columns - 1) * kColumnGap : 0;
  return kRailWidth + kColumnsLeading + content + kCreateGap + kCreateSize +
         kCreateTrailing;
}

bool MatchesSearch(std::u16string_view text, std::u16string_view query) {
  if (query.empty()) {
    return true;
  }
  return base::i18n::FoldCase(text).find(base::i18n::FoldCase(query)) !=
         std::u16string::npos;
}

std::u16string DayLabel(base::Time time, base::Time now) {
  // Round, since a day with a daylight saving change is 23 or 25 hours.
  const int days =
      ((now.LocalMidnight() - time.LocalMidnight()) + base::Hours(12)).InDays();
  if (days <= 0) {
    return u"Today";
  }
  if (days == 1) {
    return u"Yesterday";
  }
  return base::NumberToString16(days) + u" days ago";
}

}  // namespace myarc::library
