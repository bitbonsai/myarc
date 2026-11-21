#ifndef MYARC_BOOKMARKS_EXPORT_H_
#define MYARC_BOOKMARKS_EXPORT_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_ui_types.h"

class Profile;

namespace myarc {

// One regular Profile to export: its display name, the Profile when it is
// loaded, and its directory, whose workspace file is read when it is not.
struct BookmarksProfile {
  std::u16string name;
  raw_ptr<Profile> profile = nullptr;
  base::FilePath dir;
};
// Every regular Profile in display order, without Guest, ephemeral Profiles
// and ones being deleted.
using BookmarksProfiles =
    base::RepeatingCallback<std::vector<BookmarksProfile>()>;

// Settings' and the command bar's "Export Pins and Favorites…". Opens a save
// panel over `parent` named "myArc Bookmarks.html", and once the user picks a
// file writes every Profile's Favorites and Spaces there as a Netscape
// bookmarks file (import::bookmarks_html), one folder per Profile when more
// than one has anything to list. A loaded Profile exports its live workspace;
// the others are read from their workspace files, which stay untouched.
// Cancel writes nothing. A failed write, or a Profile that couldn't be read,
// shows why over `parent`. UI sequence only.
void ExportPinsAndFavorites(BookmarksProfiles profiles,
                            gfx::NativeWindow parent);

}  // namespace myarc

#endif  // MYARC_BOOKMARKS_EXPORT_H_
