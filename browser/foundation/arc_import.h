#ifndef MYARC_ARC_IMPORT_H_
#define MYARC_ARC_IMPORT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "ui/gfx/native_ui_types.h"

class Profile;

namespace myarc {

// Settings' "Import from Arc…". Asks for an Arc Markdown export (Arc's
// Settings > Advanced > Export Data), reads it off the UI sequence, then shows
// a preview over `parent` with the Spaces and Pins it adds and the lines it
// could not read. Import writes them into `profile`'s workspace, then shows a
// report of every kind of entry it left out or changed, with counts; Cancel
// changes nothing. Opens no tabs and fetches nothing. UI sequence only.
//
// `done`, when given, runs once the import is over: committed when Import
// wrote something, and not committed when the picker or the preview was
// cancelled or closed, the file was unreadable, or nothing was new. A commit
// runs it right after the workspace is saved, inside the Import press and
// before the report shows, so the caller can save the outcome before a kill
// and should post any UI change.
struct ArcImportOutcome {
  bool committed = false;
  // Lowercase hex SHA-256 of the export file.
  std::string content_hash;
  // The export's first Space, the only one the commit reports.
  std::vector<uint64_t> space_ids;
};
using ArcImportDone = base::OnceCallback<void(const ArcImportOutcome&)>;
void ImportFromArc(Profile& profile,
                   gfx::NativeWindow parent,
                   ArcImportDone done = ArcImportDone());

// For background captures: skips the file picker, reads `path` and shows the
// preview without taking focus. A non-null `confirmed` presses Import after a
// short delay and gets the export's first Space, or 0 when nothing was
// imported. `done` runs as ImportFromArc's does.
void ImportArcFileForCapture(
    Profile& profile,
    gfx::NativeWindow parent,
    const base::FilePath& path,
    base::OnceCallback<void(uint64_t first_space)> confirmed,
    ArcImportDone done = ArcImportDone());

}  // namespace myarc

#endif  // MYARC_ARC_IMPORT_H_
