#include "myarc/blocker.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/supports_user_data.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "myarc/startup_timing.h"
#include "third_party/zlib/google/zip.h"

namespace myarc {
namespace {

// Public half of a key made for myArc's bundled copy; it fixes the extension
// ID. The private half was never kept: the copy is installed unpacked, so
// nothing is signed with it.
constexpr char kBlockerPublicKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtL87iQ9MccBCuLdTN0B0"
    "hJKv7duHtzxgfRhUobK3QjFn3qlv4CRI5OOWIFhTZAdN7lYxe7UcguLQQCWyMTK0"
    "mwJ4zxgj8IjMvCAxFOsx1+RR4H/7UC6jHEza5NhDH0D1y55gA0K44Ed6RC4BBBgL"
    "TqfC+J7hCNcSb17dNLX124Whh3SkaOO8ZWJu6y+Dquc2rTP7kIAzYjxdRbTrLkbH"
    "tlL8mmJ1mBugRCbQHZEqC8gdfteAtwG/P0yLvKLcz19O/zUCJkCQuAj4X/v1MLlb"
    "zPcHKIWgRaxfAGBPGzjlqdLuRMLTs7+U2vGL2drZQe527IH/jEGmjZKrd61OiISe"
    "AQIDAQAB";

// The release zip, copied into the framework's Resources by
// //myarc:blocker_bundle_data.
constexpr base::FilePath::CharType kBlockerZip[] =
    FILE_PATH_LITERAL("ublock_origin_lite.zip");

constexpr char kVisualParitySwitch[] = "myarc-visual-parity";

// How long a navigation may wait for the blocker's rules. Unpacking,
// installing and indexing take about a second on a new Profile.
constexpr base::TimeDelta kMaxRulesWait = base::Seconds(10);
// The check is a map lookup, so a short poll lets the page go as soon as the
// rules are live instead of up to one long poll later.
constexpr base::TimeDelta kRulesPoll = base::Milliseconds(10);

// Per-Profile state for this session, kept on the original Profile.
struct BlockerState : base::SupportsUserData::Data {
  static constexpr char kKey[] = "myarc-blocker-state";

  static BlockerState& For(Profile& profile) {
    auto* state = static_cast<BlockerState*>(profile.GetUserData(kKey));
    if (!state) {
      auto owned = std::make_unique<BlockerState>();
      state = owned.get();
      profile.SetUserData(kKey, std::move(owned));
    }
    return *state;
  }

  bool install_started = false;
  bool installing = false;
  // Navigations stop waiting once the rules were seen live or a wait ran out.
  bool settled = false;
};

// Unzips the bundled release into a new directory inside `profile_dir`, so
// the installer can move it into place on the same volume. Empty on failure.
base::FilePath UnpackBundledBlocker(const base::FilePath& profile_dir) {
  base::FilePath resources;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources)) {
    return {};
  }
  base::FilePath dir;
  if (!base::CreateTemporaryDirInDir(
          profile_dir, FILE_PATH_LITERAL("myarc-blocker-"), &dir)) {
    return {};
  }
  if (!zip::Unzip(resources.Append(kBlockerZip), dir)) {
    LOG(ERROR) << "myArc: could not unpack the bundled blocker from "
               << resources.Append(kBlockerZip);
    base::DeletePathRecursively(dir);
    return {};
  }
  return dir;
}

void OnBlockerInstalled(
    base::WeakPtr<Profile> profile,
    const std::optional<extensions::CrxInstallError>& error) {
  if (profile) {
    BlockerState::For(*profile).installing = false;
  }
  if (error) {
    // The pref stays unset, so the next launch tries again.
    LOG(ERROR) << "myArc: installing the bundled blocker failed: "
               << error->message();
    return;
  }
  if (profile) {
    profile->GetPrefs()->SetBoolean(kBlockerInstalledPref, true);
  }
}

void InstallUnpackedBlocker(base::WeakPtr<Profile> profile,
                            base::FilePath dir) {
  if (dir.empty()) {
    if (profile) {
      BlockerState::For(*profile).installing = false;
    }
    return;
  }
  if (!profile) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetDeletePathRecursivelyCallback(dir));
    return;
  }
  scoped_refptr<extensions::CrxInstaller> installer =
      extensions::CrxInstaller::CreateSilent(profile.get());
  // A regular install, as from the store: removable, off in private windows
  // unless allowed there, and granted its host permissions.
  installer->set_install_source(extensions::mojom::ManifestLocation::kInternal);
  // No WAS_INSTALLED_BY_DEFAULT: that runs the unpacker at best-effort
  // priority, which waits for startup to finish, while the first page waits
  // for the blocker.
  installer->set_allow_silent_install(true);
  installer->set_expected_id(kBlockerExtensionId);
  installer->set_install_immediately(true);
  installer->set_delete_source(true);
  installer->AddInstallerCallback(base::BindOnce(&OnBlockerInstalled, profile));
  installer->InstallUnpackedCrx(kBlockerExtensionId, kBlockerPublicKey, dir);
}

void OnExtensionsReady(base::WeakPtr<Profile> profile) {
  if (!profile) {
    return;
  }
  if (extensions::ExtensionRegistry::Get(profile.get())
          ->GetInstalledExtension(kBlockerExtensionId)) {
    profile->GetPrefs()->SetBoolean(kBlockerInstalledPref, true);
    BlockerState::For(*profile).installing = false;
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&UnpackBundledBlocker, profile->GetPath()),
      base::BindOnce(&InstallUnpackedBlocker, profile));
}

// True while `context`'s first page loads should wait: the bundled blocker
// is being installed, or is on but its network rules aren't live yet.
bool RulesPending(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  BlockerState& state = BlockerState::For(*profile->GetOriginalProfile());
  if (state.settled) {
    return false;
  }
  // A private window waits only once the blocker runs there.
  if (state.installing && !profile->IsOffTheRecord()) {
    return true;
  }
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          kBlockerExtensionId);
  if (!extension || !extensions::util::IsIncognitoEnabled(extension, context)) {
    return false;
  }
  auto* rules =
      extensions::declarative_net_request::RulesMonitorService::Get(context);
  return rules &&
         !rules->ruleset_manager()->GetMatcherForExtension(kBlockerExtensionId);
}

// Holds a Profile's web page loads until the blocker's rules are live, so
// the first page a new Profile opens, or a restored tab at launch, is
// filtered too. Gives up after kMaxRulesWait.
class BlockerRulesThrottle final : public content::NavigationThrottle {
 public:
  using content::NavigationThrottle::NavigationThrottle;

  ThrottleCheckResult WillStartRequest() override {
    if (!RulesPending(Context())) {
      return PROCEED;
    }
    started_ = base::TimeTicks::Now();
    poll_.Start(FROM_HERE, kRulesPoll, this, &BlockerRulesThrottle::Poll);
    return DEFER;
  }

  const char* GetNameForLogging() override {
    return "MyarcBlockerRulesThrottle";
  }

 private:
  content::BrowserContext* Context() {
    return navigation_handle()->GetWebContents()->GetBrowserContext();
  }

  void Poll() {
    const bool timed_out = base::TimeTicks::Now() - started_ > kMaxRulesWait;
    if (RulesPending(Context()) && !timed_out) {
      return;
    }
    poll_.Stop();
    NoteBlockerHold(base::TimeTicks::Now() - started_);
    if (timed_out) {
      LOG(WARNING) << "myArc: page load went ahead before the blocker's rules";
    }
    BlockerState::For(
        *Profile::FromBrowserContext(Context())->GetOriginalProfile())
        .settled = true;
    Resume();
  }

  base::TimeTicks started_;
  base::RepeatingTimer poll_;
};

}  // namespace

void RegisterBlockerProfilePrefs(PrefRegistrySimple& registry) {
  registry.RegisterBooleanPref(kBlockerInstalledPref, false);
  registry.RegisterBooleanPref(kBlockerUnpinnedPref, false);
}

void MaybeInstallBlocker(Profile& profile) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kVisualParitySwitch) ||
      !profile.IsRegularProfile() ||
      profile.GetPrefs()->GetBoolean(kBlockerInstalledPref)) {
    return;
  }
  BlockerState& state = BlockerState::For(profile);
  if (state.install_started) {
    return;
  }
  state.install_started = true;
  state.installing = true;
  extensions::ExtensionSystem::Get(&profile)->ready().Post(
      FROM_HERE, base::BindOnce(&OnExtensionsReady, profile.GetWeakPtr()));
}

void MaybeAddBlockerRulesThrottle(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kVisualParitySwitch) ||
      !handle.IsInMainFrame() || !handle.GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  registry.AddThrottle(std::make_unique<BlockerRulesThrottle>(registry));
}

}  // namespace myarc
