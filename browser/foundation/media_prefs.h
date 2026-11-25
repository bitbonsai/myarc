#ifndef MYARC_MEDIA_PREFS_H_
#define MYARC_MEDIA_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace myarc {

// Per-Profile boolean pref behind Library > Media opt-in ("Show Media").
// Off by default; never inherited by off-the-record Profiles.
inline constexpr char kMediaLibraryEnabledPref[] =
    "myarc.library.media_enabled";

void RegisterMediaProfilePrefs(user_prefs::PrefRegistrySyncable& registry);

}  // namespace myarc

#endif  // MYARC_MEDIA_PREFS_H_
