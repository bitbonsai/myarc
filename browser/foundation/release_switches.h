#ifndef MYARC_RELEASE_SWITCHES_H_
#define MYARC_RELEASE_SWITCHES_H_

namespace base {
class CommandLine;
}

namespace myarc {

// In release builds (GN myarc_release), migrates a legacy "myArc Nightly"
// profile directory to "myArc" if needed and adds the myArc mode switches,
// --no-first-run and features to the browser process command line unless
// already present. Call before the FeatureList and user data dir are
// initialized. Other builds are unchanged.
void MaybeEnableReleaseMode(base::CommandLine& command_line);

}  // namespace myarc

#endif  // MYARC_RELEASE_SWITCHES_H_
