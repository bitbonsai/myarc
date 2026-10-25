# Native workspace command safety integration

## 2026-10-25 attempt

Bounded task: install corrected optional 0012 on the existing native stack and compile/link `chrome`. Desktop/runtime acceptance belongs to the parent task.

Imported [0012](../patches/0012-native-workspace-command-safety.patch) unchanged from commit `020a8dc59c7c5e2b063d7a41dd5d7c50fbd2ecb8`.
SHA-256: `028b623228355bdcf6d02bc97fa397972c3080361950fccc51229c5ed834409a`.
Install this complete replacement patch once after its baseline passes verification. Keep old R1/UI/core addon patches out of that application.

External Chromium HEAD matched `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`. All 73 source fingerprints matched the correction handoff's baseline. Existing foundation, optional stack and independent rounding were unchanged.

Process preflight blocked this attempt. Although no executable under `out/MyarcM0` was running, macOS notification services still mapped bundle resource files from that output. The task required stopping if any process used the output. Those processes were left untouched. No external source changes, patch application, GN generation, header checks, compilation or linking occurred. Existing executable predates this attempt and is not corrected-0012 build evidence. Exact process evidence and before/after hashes remain in private scratch outside Git.

Only artifact SHA-256, baseline hashes, process checks, document links and project whitespace were checked here. Earlier correction-handoff syntax/GN proof remains historical evidence, not checks rerun by this task.

## Intended external files

All paths below are relative to `chrome/browser/ui/`; none were edited in this attempt:

- `BUILD.gn`
- `browser_actions.cc`
- `browser_command_controller.cc`
- `browser_command_controller.h`
- `browser_command_controller_browsertest.cc`
- `browser_commands.cc`
- `browser_commands.h`
- `browser_window.h`
- `navigator/browser_navigator.cc`
- `omnibox/chrome_omnibox_client.cc`
- `search_engines/BUILD.gn`
- `search_engines/default_search_extension_controlled_controller.cc`
- `tabs/BUILD.gn`
- `tabs/tab_list_bridge.cc`
- `tabs/tab_menu_model.cc`
- `tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_adapter_impl.cc`
- `tabs/tab_strip_model.cc`
- `views/frame/browser_view.cc`
- `views/frame/browser_view.h`

## Resume and native acceptance

Parent narrowed preflight to executable/code use: the observed notification-service mappings were localization resources, not running browser code. Those system processes were left untouched. All 73 baseline fingerprints still matched. Strict patch check/application succeeded; all 19 result files match the corrected candidate and the other 54 baseline files remain unchanged. Actual GN generation and six target-family header checks pass.

The full native `chrome` build passed with the existing [foundation toolchain](../README.md) and `autoninja -C out/MyarcM0 -j 8 chrome`. The first malformed UI target pattern returned zero matches and was corrected before the six actual checks; zero matches were not counted as proof.

## First actual workspace run

Native startup exposed faults that source checks missed: sidebar naming before a valid accessibility role, unchecked tab-change variant access, unsupported heading-button typography and subpixel text painting through transparent layers. The parent fixed these in the owned adapter/sidebar sources and regenerated the canonical 16-file foundation payload. The final incremental app build passed. A first label-access attempt used a protected method and failed compilation; the public button text-rendering API replaced it. Failed startup profiles were retained privately and fresh marked profiles used for retries, avoiding recovery crash loops.

Vertical tabs must be enabled before this optional sidebar initializes. A fresh-profile run initially used stock horizontal tabs; setting the real Chromium preference after startup did not retroactively construct the adapter. The final acceptance fixture seeded only `vertical_tabs.enabled=true` before launch. Browser and OS appearance settings were left unchanged.

Through actual native controls, the parent created Reading and Empty beside Everyday, opened a second synthetic HTTP page using the command palette, and switched between the two populated Spaces. Page observation confirmed exactly one visible page in each populated Space and both pages hidden in Empty. Empty-to-Reading selected the different native page correctly. Pressing Reload in Empty preserved both hidden pages' URLs and `performance.timeOrigin`. Actual persisted workspace metadata matched three Spaces, two live items and the selected Reading item. Fresh owned-window captures were inspected. This is a manual native smoke journey, not execution of the browser regression suite; see [bounded evidence](../evidence/workspace-native.json).

**Original smoke blocker:** Empty exposed the previous page URL/site-information controls and enabled-looking page buttons. Page pixels were hidden and the observed Reload action was blocked, but visible/accessibility state leaked prior metadata. The follow-up below fixes and manually checks this bounded path. The original run does not establish unmapped action restoration, extension API Unpin behavior or complete command/privacy safety.

Default workspace renderer remains OFF. Automated native browser regressions remain UNRUN. The basic Space-switch/empty-page path below now has the manual evidence above; the broader action/save/consent checks remain open:

1. After successful build and explicit test enablement, use a marked disposable profile. Open distinct synthetic pages in two Spaces, then switch through an empty Space to the different native page. Check action-state recovery and that hidden-page commands cannot affect the other Space.
2. Exercise mixed Save/Unsave. Wait for native pin synchronization; generic Unpin must retain saved identity, while explicit UI Unsave removes membership deliberately.
3. Check blank navigation/focus and search-consent behavior, including the Mac Apple Events hidden-page guard. Confirm existing saved-page fade and empty-window lifecycle behavior remain intact.

Cross-Space shortcut/Tab Search policy, extension `activeTab`, cross-window destinations, partial batches and repeated snapshot costs remain open. This attempt adds no policy, memoization or runtime acceptance claim.

## Empty-header candidate fix, 2026-10-25

The smoke observations and [workspace-native.json](../evidence/workspace-native.json) above describe the pre-fix run. They remain historical evidence. The agent handoff verified compilation only; the parent's later native acceptance is recorded below.

Updated complete replacement [0012](../patches/0012-native-workspace-command-safety.patch), SHA-256 `8f8023789fdb66841128879766afdb89866c7e393453185336eba69745f66e53`. Apply it once to the pre-0012 baseline, rather than over the prior version. The external checkout already contains this update.

Four upstream files changed relative to the integrated candidate:

- `views/frame/browser_view.cc`: refresh the neutral omnibox through the existing null-toolbar update and shared blank guards. Disable Reload after `ChangeMode`, which otherwise enables it again. Retain populated-page toolbar restoration.
- `views/toolbar/toolbar_view.cc`: sidebar layout hides Back, Forward and Reload/Stop while blank, retaining the address field and sidebar toggle. Layout no longer overrides that page-control visibility.
- `views/location_bar/location_bar_view.cc`: report no page while blank, hide site-information and page actions, and let content-setting updates use the missing-page path. URL drag queries use the same page getter; drag-data writing tolerates a Space becoming empty.
- `browser_command_controller_browsertest.cc`: extend `DifferentNativeTabRestoresActionState` with first-page URL, neutral empty address, hidden page controls, rejected page information, repeated toolbar/loading updates and different-page restoration assertions. Both existing command-mode parameters remain.

The patch now contains 21 files, adding toolbar/location-bar changes to the previous 19-file patch. Strict reverse restores all 73 pre-0012 baseline files; strict replay matches all 21 installed results. Seventy other captured external files remain byte-identical, including the parent's startup fixes and hover/fade/caption sources. Foundation-owned sources did not change, so canonical 0001 remains unchanged. Generic Unpin and explicit Unsave behavior are untouched.

Verification actually run:

- `autoninja -C out/MyarcM0 -j8 chrome`: exit 0. The first incremental build compiled all three production translation units and relinked `libchrome_dll.dylib`. A final null-drag guard then recompiled the location bar and relinked successfully.
- Real-header syntax check of the updated browser-test translation unit: exit 0, empty diagnostic log. An initial private toolbar-accessor error was corrected using public `ToolbarButtonProvider`.
- GN header checks for `//chrome/browser/ui/views/location_bar:impl`, `//chrome/browser/ui/views/toolbar:impl` and `//chrome/browser/ui:browser_tests`: all pass.
- Strict patch apply/reverse/replay, changed-line clang-format, document links, foundation payload freshness and whitespace checks pass.

Automated native cases executed: **zero**. No `browser_tests` executable was available; its dependency graph was not built. No app, desktop input, Arc interaction or profile access occurred. Executable `comm` paths were checked before building; resource-only notification-service mappings were left untouched. No build remains running at handoff. Renderer default remains OFF.

## Parent native acceptance, 2026-10-25

A new marked disposable profile repeated Reading -> Everyday -> Empty -> Reading with two actual synthetic HTTP pages. Empty showed an enabled, empty address field with a neutral placeholder. Fresh screenshot and accessibility output contained no prior URL, site-information popup or page-only Back/Forward/Reload/Stop controls. Both actual pages were hidden.

A privileged test-only CDP reload of the retained Workshop page triggered real background loading while Empty remained selected. Its time origin changed as intended; Reading's did not. The header remained neutral with page controls absent. New Tab opened the native palette with empty input; Escape returned without adding a page. Native Cmd+R in Empty then left both hidden URLs and time origins unchanged.

Returning to Reading restored its own URL, site information and page controls. Pressing native Reload changed Reading's time origin while Workshop stayed unchanged. Reading -> Empty -> Reading retained both live pages without another reload. Actual persisted metadata matched three Spaces, two items and the selected Reading item. Fresh Empty/Reading captures were inspected. See [current bounded evidence](../evidence/workspace-empty-header.json).

The palette still lists both live pages as Switch to Tab results, including while Empty is selected. This check establishes empty input and cancellation, not a stronger cross-Space search policy. Automated native browser cases executed remain zero. The existing QA/public-pin profile was untouched. Renderer stays off by default while broader action/save/lifecycle/privacy gates remain open. The next bounded implementation targets the known recovery-bubble anchor crash.
