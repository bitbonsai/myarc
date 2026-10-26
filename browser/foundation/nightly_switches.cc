#include "myarc/nightly_switches.h"

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "myarc/nightly_buildflags.h"

namespace myarc {

#if BUILDFLAG(MYARC_NIGHTLY)
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

}  // namespace
#endif  // BUILDFLAG(MYARC_NIGHTLY)

void MaybeEnableNightlyMode(base::CommandLine& command_line) {
#if BUILDFLAG(MYARC_NIGHTLY)
  for (const char* name : {"myarc-visual-parity", "myarc-workspace-sidebar"}) {
    if (!command_line.HasSwitch(name)) {
      command_line.AppendSwitch(name);
    }
  }
  AddFeature(command_line, "enable-features", "GlassFrame");
  AddFeature(command_line, "disable-features", "InitialWebUI");
#endif  // BUILDFLAG(MYARC_NIGHTLY)
}

}  // namespace myarc
