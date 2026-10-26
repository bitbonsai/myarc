#ifndef MYARC_NIGHTLY_SWITCHES_H_
#define MYARC_NIGHTLY_SWITCHES_H_

namespace base {
class CommandLine;
}

namespace myarc {

// In nightly builds (GN myarc_nightly), adds the myArc mode switches and
// features to the browser process command line unless already present.
// Call before the FeatureList is created. Other builds are unchanged.
void MaybeEnableNightlyMode(base::CommandLine& command_line);

}  // namespace myarc

#endif  // MYARC_NIGHTLY_SWITCHES_H_
