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
// Settings > Advanced > Export Data) or Arc's sidebar file, reads it off the
// UI sequence, then shows a preview over `parent` with what it adds and the
// lines it could not read. Arc's sidebar file can hold several Arc profiles:
// the main one imports into `profile`, and each other one into the myArc
// Profile named "Arc <its directory>", which the import creates when it is
// missing. The preview names each mapping and loads or creates no Profile;
// it reads an unloaded Profile's workspace file without changing it. Import
// writes the Profiles' workspaces, then shows a report of every kind of entry
// it left out or changed, with counts; Cancel changes nothing. Opens no tabs
// and fetches nothing. UI sequence only.
//
// `done`, when given, runs once the import is over: committed when Import
// wrote something, and not committed when the picker or the preview was
// cancelled or closed, the file was unreadable, or nothing was new. A commit
// runs it right after `profile`'s workspace is saved, inside the Import press
// and before other Profiles import and the report shows, so the caller can
// save the outcome before a kill and should post any UI change.
// `report_closed`, when given with `done`, runs once the report after a
// commit closes, or once the import is over when no report shows. In
// captures the report closes itself after 2 s.
struct ArcImportOutcome {
  bool committed = false;
  // Lowercase hex SHA-256 of the export file.
  std::string content_hash;
  // The first Space the export put in `profile`, the only one the commit
  // reports. Empty when it put none there.
  std::vector<uint64_t> space_ids;
};
using ArcImportDone = base::OnceCallback<void(const ArcImportOutcome&)>;
void ImportFromArc(Profile& profile,
                   gfx::NativeWindow parent,
                   ArcImportDone done = ArcImportDone(),
                   base::OnceClosure report_closed = base::OnceClosure());

// Looks for Arc's sidebar file,
// ~/Library/Application Support/Arc/StorableSidebar.json, off the UI sequence
// without reading it, and runs `found` on the UI sequence with its path, or an
// empty path when there is none.
void FindArcData(base::OnceCallback<void(const base::FilePath&)> found);

// "Found your Arc data" > Import: like ImportFromArc with `path`, from
// FindArcData, already picked, so the preview opens without the file picker.
void ImportFoundArcData(Profile& profile,
                        gfx::NativeWindow parent,
                        const base::FilePath& path,
                        ArcImportDone done = ArcImportDone(),
                        base::OnceClosure report_closed = base::OnceClosure());

// For background captures: skips the file picker, reads `path` and shows the
// preview without taking focus. A non-null `confirmed` presses Import after a
// short delay and gets the export's first Space, or 0 when nothing was
// imported. `done` runs as ImportFromArc's does.
void ImportArcFileForCapture(
    Profile& profile,
    gfx::NativeWindow parent,
    const base::FilePath& path,
    base::OnceCallback<void(uint64_t first_space)> confirmed,
    ArcImportDone done = ArcImportDone(),
    base::OnceClosure report_closed = base::OnceClosure());

}  // namespace myarc

#endif  // MYARC_ARC_IMPORT_H_
