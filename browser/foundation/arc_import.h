#ifndef MYARC_ARC_IMPORT_H_
#define MYARC_ARC_IMPORT_H_

#include <cstdint>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "ui/gfx/native_ui_types.h"

class Profile;

namespace myarc {

// Settings' "Import from Arc…". Asks for an Arc Markdown export (Arc's
// Settings > Advanced > Export Data), reads it off the UI sequence, then shows
// a preview over `parent` with the Spaces and Pins it adds and the lines it
// could not read. Import writes them into `profile`'s workspace; Cancel
// changes nothing. Opens no tabs and fetches nothing. UI sequence only.
void ImportFromArc(Profile& profile, gfx::NativeWindow parent);

// For background captures: skips the file picker, reads `path` and shows the
// preview without taking focus. A non-null `confirmed` presses Import after a
// short delay and gets the export's first Space, or 0 when nothing was
// imported.
void ImportArcFileForCapture(
    Profile& profile,
    gfx::NativeWindow parent,
    const base::FilePath& path,
    base::OnceCallback<void(uint64_t first_space)> confirmed);

}  // namespace myarc

#endif  // MYARC_ARC_IMPORT_H_
