#ifndef MYARC_BOOKMARKS_EXPORT_H_
#define MYARC_BOOKMARKS_EXPORT_H_

#include "ui/gfx/native_ui_types.h"

class Profile;

namespace myarc {

// Settings' and the command bar's "Export Pins and Favorites…". Opens a save
// panel over `parent` named "myArc Bookmarks.html", and once the user picks a
// file writes `profile`'s Favorites and every Space's Pins and folders there
// as a Netscape bookmarks file (import::bookmarks_html). Cancel writes
// nothing. A failed write shows why over `parent`. UI sequence only.
void ExportPinsAndFavorites(Profile& profile, gfx::NativeWindow parent);

}  // namespace myarc

#endif  // MYARC_BOOKMARKS_EXPORT_H_
