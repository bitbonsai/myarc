#include "myarc/release_switches.h"

#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "myarc/release_buildflags.h"

namespace myarc {

#if BUILDFLAG(MYARC_RELEASE)
namespace {

// Appends `feature` to the comma-separated list in `switch_name`.
void AddFeature(base::CommandLine& command_line,
                const char* switch_name,
                std::string_view feature) {
  std::string value = command_line.GetSwitchValueASCII(switch_name);
  for (std::string_view name : base::SplitStringPiece(
           value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (name == feature) {
      return;
    }
  }
  value += value.empty() ? "" : ",";
  value += feature;
  command_line.AppendSwitchASCII(switch_name, value);
}

#if BUILDFLAG(IS_MAC)
// One-time migration from ~/Library/Application Support/myArc Nightly to
// ~/Library/Application Support/myArc. Checks for Local State / Default rather
// than directory existence because ~/Library/Application Support/myarc/control/
// may already exist on case-insensitive APFS.
void MaybeMigrateNightlyProfileDir(const base::CommandLine& command_line) {
  if (command_line.HasSwitch("user-data-dir")) {
    return;
  }
  base::FilePath app_support;
  if (!base::PathService::Get(base::DIR_APP_DATA, &app_support)) {
    return;
  }
  const base::FilePath old_dir = app_support.Append("myArc Nightly");
  const base::FilePath new_dir = app_support.Append("myArc");
  if (!base::DirectoryExists(old_dir)) {
    return;
  }
  if (base::PathExists(new_dir.Append("Local State")) ||
      base::DirectoryExists(new_dir.Append("Default"))) {
    return;
  }
  if (!base::PathExists(new_dir)) {
    if (!base::Move(old_dir, new_dir)) {
      LOG(WARNING) << "Failed to move legacy profile dir from " << old_dir
                   << " to " << new_dir;
    }
    return;
  }
  base::FileEnumerator entries(
      old_dir, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath child = entries.Next(); !child.empty();
       child = entries.Next()) {
    const std::string name = child.BaseName().value();
    if (name == "SingletonLock" || name == "SingletonSocket" ||
        name == "SingletonCookie") {
      continue;
    }
    const base::FilePath dst = new_dir.Append(child.BaseName());
    if (!base::PathExists(dst) && !base::Move(child, dst)) {
      LOG(WARNING) << "Failed to migrate " << child << " to " << dst;
    }
  }
  // Remove the legacy directory if it is now empty.
  base::DeleteFile(old_dir);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace
#endif  // BUILDFLAG(MYARC_RELEASE)

void MaybeEnableReleaseMode(base::CommandLine& command_line) {
#if BUILDFLAG(MYARC_RELEASE)
#if BUILDFLAG(IS_MAC)
  MaybeMigrateNightlyProfileDir(command_line);
#endif
  // no-first-run skips Chromium's branded welcome page.
  for (const char* name :
       {"myarc-visual-parity", "myarc-workspace-sidebar", "no-first-run"}) {
    if (!command_line.HasSwitch(name)) {
      command_line.AppendSwitch(name);
    }
  }
  AddFeature(command_line, "enable-features", "GlassFrame");
  AddFeature(command_line, "disable-features", "InitialWebUI");
#endif  // BUILDFLAG(MYARC_RELEASE)
}

}  // namespace myarc
