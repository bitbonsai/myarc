#ifndef MYARC_BLOCKER_H_
#define MYARC_BLOCKER_H_

class PrefRegistrySimple;
class Profile;

namespace content {
class NavigationThrottleRegistry;
}

namespace myarc {

// myArc's default ad and tracker blocker is uBlock Origin Lite, bundled in
// browser/foundation/blocker/ (see its README). Each Profile gets it once, as
// a regular extension the user can turn off or remove; a removed blocker is
// not put back.

// ID of the bundled copy, derived from the public key in blocker.cc.
inline constexpr char kBlockerExtensionId[] =
    "aefnaoagnnooajnoebfnnnpdngplgemc";

// Profile pref: the bundled blocker has been installed into this Profile.
inline constexpr char kBlockerInstalledPref[] = "myarc.blocker.installed";
// Profile pref: one-time unpin migration for Profiles where an older build
// auto-pinned the bundled blocker to the toolbar.
inline constexpr char kBlockerUnpinnedPref[] = "myarc.blocker.unpinned_v1";

void RegisterBlockerProfilePrefs(PrefRegistrySimple& registry);

// Installs the bundled blocker into `profile` once its extension system is
// ready, unless the Profile already had it. Only under --myarc-visual-parity,
// and only for regular Profiles.
void MaybeInstallBlocker(Profile& profile);

// Adds a throttle to main-frame web navigations that holds them while the
// blocker is installing or its rules are still loading, at most 10 seconds
// and only until the first time they are live in a session.
void MaybeAddBlockerRulesThrottle(
    content::NavigationThrottleRegistry& registry);

}  // namespace myarc

#endif  // MYARC_BLOCKER_H_
