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

// Settings' "Import from Arc" and the welcome screen's import button. Looks
// for Arc's sidebar file (~/Library/Application Support/Arc/StorableSidebar.json)
// off the UI sequence and opens its preview directly. Only when Arc isn't
// found on this Mac does it offer a fallback dialog to choose an Arc export
// or sidebar file manually. Arc's sidebar file can hold several Arc profiles:
// the main one imports into `profile`, and each other one into the myArc
// Profile named after Arc's profile (or "Arc <its directory>"), which Import
// creates when missing. The preview names each mapping and loads or creates no
// Profile; it reads an unloaded Profile's workspace file without changing it.
// Import writes the Profiles' workspaces, then shows a report of every kind of
// entry it left out or changed, with counts; Cancel changes nothing. Opens no
// tabs and fetches nothing. UI sequence only.
//
// `done`, when given, runs once the import is over: committed when Import
// wrote something, and not committed when the dialog, picker or preview was
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

// Checks off the UI sequence whether Arc's sidebar file exists on this Mac,
// without reading it, and runs `callback` on the UI sequence.
void HasArcData(base::OnceCallback<void(bool)> callback);

// For background captures: skips auto-detection and the file picker, reads
// `path` and shows the preview without taking focus. A non-null `confirmed`
// presses Import after a short delay and gets the export's first Space, or 0
// when nothing was imported. `done` runs as ImportFromArc's does.
void ImportArcFileForCapture(
    Profile& profile,
    gfx::NativeWindow parent,
    const base::FilePath& path,
    base::OnceCallback<void(uint64_t first_space)> confirmed,
    ArcImportDone done = ArcImportDone(),
    base::OnceClosure report_closed = base::OnceClosure());

}  // namespace myarc

#endif  // MYARC_ARC_IMPORT_H_
