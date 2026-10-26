# Native Chromium regression fixture

[index.html](index.html) is an original local form that writes one invented note to browser localStorage. It implements no myarc workspace rules. The [developer test client](../../../tools/check_browser_foundation.mjs) exercises real browser behavior, separate from production CLI/MCP control.

## Native preview: five-minute manual smoke

From the repository root, on the existing macOS 26+ Apple Silicon build host:

```sh
python3 tools/launch_native_preview.py
```

Requires Python 3 and the already-built optional native stack, currently through
0025, at `$HOME/.cache/myarc-m0/chromium/src/out/MyarcM0/Chromium.app/Contents/MacOS/Chromium`.
The launcher does no build, patching or installation. Missing/nonexecutable builds
fail with that exact path. Ask the build owner to finish verification before use.

Each run creates a new marked, private `native-preview-*` directory beneath
`$HOME/.cache/myarc-m0`, seeds vertical tabs before startup and opens `about:blank`.
It prints the exact profile, browser PID, `preview.pid` and `preview.log` locations.
The log and PID file live inside that directory, outside Git. A started process
alone does not verify a working window. If it exits or shows stock UI, retain the
log and report the build/launch mismatch.

This command explicitly enables the experimental workspace sidebar; ordinary
launches still leave that renderer off. It adds no debugging endpoint, extension,
automation flag or security bypass. Developer preview convenience is separate
from the planned product CLI/MCP browser controls. Keep important data in your
normal browser. Use invented browsing content in this isolated disposable profile.
Leave OS settings and permissions unchanged; cancel unexpected permission prompts
and report them. Account sign-in and AI are unnecessary.

Check these in the preview window, recording failures rather than assuming parity:

1. Press Cmd+T, type `https://example.com`, then Enter. Expect one new real page.
   Press Cmd+L, change its URL to `https://example.org`, then Enter. Expect the same
   tab to navigate, with no duplicate. Hover a result without clicking; input should
   keep focus. Arrow/click selection deliberately activates that result. Escape
   should dismiss an unused palette.
2. Use the footer `+` (Create Space), then switch with its Space dots. A fresh
   Space should show empty material and a neutral header, without the prior URL
   or page. Return to the populated Space and select its row to show the actual page.
3. Right-click a live row. Where exposed, try Pin to Space / Unpin from Space,
   then Add to Profile Favorites / Remove from Profile Favorites. Explicit Remove
   should keep the live page as Temporary. Save again, open another Temporary page,
   then choose Close live page (keep Favorite) on the saved item. Its tile should
   remain; clicking it should reopen its URL. Favorites belong to the Profile and
   can appear across Spaces. Broader last-live-page and multiwindow behavior remain open.
4. Press Cmd+S to hide/show the full sidebar. While hidden, hover at the left
   edge and pause: expect a usable overlay without shifting the page. Move back onto the
   page to dismiss it. Cmd+S should restore persistent visibility. This shortcut
   toggles the sidebar rather than Save Page.
5. Ordinary white pages should remain opaque white; authored dark pages should
   retain their background. `about:blank` and an empty Space use shell material.
   Browser theme alone need not recolor websites. Check with pages you can inspect;
   this smoke does not measure pixel parity, motion timing or accessibility.
6. Focus the preview app, then Cmd+Q to quit its windows. Leave your normal browser
   running. Running the launcher again intentionally creates another fresh profile.
   Persistence testing requires explicitly reusing a marked test profile and the
   parent's verified restore procedure, separately from this launcher.

[Fresh native smoke through0025](../../../browser/evidence/native-preview.json) used this exact command without an endpoint/test extension. Latest run covered actual URL/current-item editing, pointer focus, neutral Empty and Cmd+S. Favorite/Unsave/close-reopen and held sidebar-hover evidence is retained from0022, rather than counted as repeated. Numeric viewport geometry and complete accessibility were not measured in the no-debug run. Provisional global result scope remains separate from current-address literal submission.

Recovery/durability remain unaccepted: recent acknowledged localStorage writes
and recent navigation have failed process-crash probes. This smoke establishes neither recovery nor
release readiness. See [the recovery candidate](../../../browser/experiments/native-recovery-lifecycle.md)
and the parent's [current run report](../../../browser/experiments/overnight-2026-10-25.md).

Cleanup is manual. After the exact preview app and its helpers have exited, inspect
only the printed directory and marker. If no evidence is needed, move that one
`native-preview-*` directory to Trash in Finder. Keep it if process ownership or
exit is uncertain. The launcher never deletes profiles or stops other processes.
Profiles/logs may contain browsing data; keep them out of Git and public reports.

Local launcher checks, using synthetic files and a process double only:

```sh
python3 -m unittest discover -s tools -p 'test_launch_native_preview.py' -v
python3 tools/check_docs.py
```

These checks execute zero native browser launches. Parent native smoke remains
separate from their pass/fail result.

## Run

Requires Node 22+, Python 3 and the compiled Chromium candidate from [the foundation recipe](../../../browser/README.md). Use a fresh, disposable profile and keep an ordinary test tab open. The client checks the candidate version and expected browser PID before navigation. A marker prevents accidental use of an unmarked profile; these privileged checks remain developer tooling, not an authorization system.

From the repository root, with `WORK` and `SRC` from that recipe:

```sh
PROFILE=$(mktemp -d "$WORK/runtime-profile.XXXXXX")
printf 'Owned synthetic M0 browser test profile\n' > "$PROFILE/.myarc-m0-test-profile"
python3 -m http.server 8766 --bind 127.0.0.1 --directory tests/fixtures/browser &
SERVER_PID=$!
"$SRC/out/MyarcM0/Chromium.app/Contents/MacOS/Chromium" \
  --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
  --myarc-foundation --remote-debugging-address=127.0.0.1 \
  --remote-debugging-port=0 http://127.0.0.1:8766/ &
BROWSER_PID=$!
```

Wait for the native window. Install uBlock Origin Lite `2026.1021.1354`, ID `ddkjiahejlhfcafbddmgiahcphecmpfh`, through Chrome Web Store in that test profile. The tested configuration grants its website access. Neither the extension nor its code is bundled here. Keep personal browsing out of this profile.

```sh
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" extension
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" runtime
```

The extension scenario temporarily disables and re-enables the named extension. It visits the public example.com page and fetches an advertising resource without executing it or sending credentials. This negative control requires internet access. Enabled filtering must return an extension-local replacement or a blocked-client error; disabled filtering must produce a response from the expected advertising origin/path. A resolved fetch alone proves neither network access nor blocking.

Quit only the test Chromium instance normally, then repeat its launch command with the same `PROFILE` and flags. Set `BROWSER_PID=$!` to the new process before running:

```sh
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" restart
```

The extension scenario saves its browser PID in the marked profile. The restart scenario rejects the unchanged PID and verifies the enabled state and filtering effect. It leaves the extension installed. Stop the owned fixture server afterward with `kill "$SERVER_PID"`, and quit the test browser to close its developer debugging endpoint.

## Workspace generic Unpin probe

`workspace-pin` exercises the established generic extension Unpin contract through the original [MV3 fixture manifest](workspace-pin-extension/manifest.json) and [worker](workspace-pin-extension/worker.js). It requires the current native stack, including command-safety 0012 and recovery-anchor 0016. The workspace renderer stays off by default; these flags enable it only in this disposable test browser. One real Chromium run passed; see [bounded native evidence](../../../browser/evidence/workspace-saved-identity.json).

Use a fresh marked profile with exactly one ordinary tab, opened on the standard loopback fixture. Seed the real vertical-tabs preference **before startup**; changing it in an already running window does not construct the adapter. Run from the repository root, with no existing process using this new profile:

```sh
REPO="$PWD"
WORK="$HOME/.cache/myarc-m0"
SRC="$WORK/chromium/src"
PROFILE=$(mktemp -d "$WORK/workspace-pin-profile.XXXXXX")
printf 'Owned synthetic M0 browser test profile\n' > "$PROFILE/.myarc-m0-test-profile"
mkdir "$PROFILE/Default"
printf '{"vertical_tabs":{"enabled":true}}\n' > "$PROFILE/Default/Preferences"
python3 -m http.server 8766 --bind 127.0.0.1 --directory tests/fixtures/browser &
SERVER_PID=$!
"$SRC/out/MyarcM0/Chromium.app/Contents/MacOS/Chromium" \
  --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
  --myarc-visual-parity --myarc-workspace-sidebar \
  --enable-features=GlassFrame --disable-features=InitialWebUI \
  --load-extension="$REPO/tests/fixtures/browser/workspace-pin-extension" \
  --remote-debugging-address=127.0.0.1 --remote-debugging-port=0 \
  http://127.0.0.1:8766/ &
BROWSER_PID=$!
```

Wait for the native workspace window, then run:

```sh
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" workspace-pin
```

The client verifies candidate version/PID, reuses the single exact fixture target, briefly visits `chrome://version/` to check its actual Profile path and test flags, then returns to the synthetic form. It requires a persisted, selected Temporary item before any extension mutation. Missing renderer, stale saved state, wrong Profile or ambiguous tabs fail the probe.

A single CDP `ServiceWorker.startWorker` request wakes only the known fixture scope, including an idle worker. The client attaches to its exact script target and verifies context, runtime ID and full manifest. Worker attachment lasts for this check and is detached afterward; normal MV3 idle shutdown can resume. It adds no keepalive or background mutation. Worker loading/startup failure is a concrete blocker. Unbranded Chromium supports local `--load-extension`; branded builds, Enhanced Safe Browsing or managed policy may reject it. Keep those protections intact and report the failure rather than add bypass flags.

Through real `chrome.tabs.update`, the worker requests `pinned:true` for the single active, nonprivate fixture tab. The client awaits its actual native pin and the same item ID becoming a persisted Profile Favorite in `Default/myarc-workspace-v1.json` (placement `2`, owner equal to `profile_id`, canonical URL equal to the fixture URL). It then requests `pinned:false`, requiring native pin to remain true and saved identity/placement to remain unchanged. Five actual tab/disk read-backs span queued projection/write turns. Reads are capped at 64 KiB for this fresh single-tab probe; worker/disk readiness checks use a ten-second deadline and individual CDP requests time out after fifteen seconds.

The fixture has only the `tabs` permission, which grants tab metadata access across this disposable Profile. Code restricts its operations to the exact loopback URL and explicit tab ID. It has no host permissions, content scripts, web-accessible resources or message listeners. Its manifest key is public test identity, not a credential. Privileged test CDP invokes its globals directly; this is not a web-message control interface or product authorization system.

The synthetic Favorite is deliberately left saved for the parent's separate native UI Unsave check. A rerun requires another fresh profile. Quit only this owned browser normally and stop the owned server with `kill "$SERVER_PID"` after native follow-up. No extension is disabled or uninstalled by this mode. It proves neither explicit UI Unsave nor restart/crash durability, and does not execute C++ browser tests.

Local checks, without launching a browser:

```sh
node --check tools/check_browser_foundation.mjs
node --check tests/fixtures/browser/workspace-pin-extension/worker.js
node --test tools/check_browser_foundation.test.mjs
python3 tools/check_docs.py
```

The four existing MV3/CLI tests cover manifest identity/permissions, fixture argument/target guards and API forwarding/errors, plus CLI rejection. Their API doubles establish fixture behavior only. The parent ran the native scenario once successfully, then used the actual sidebar menu to remove the Favorite. The same live item became Temporary, its saved URL cleared and actual extension read-back reported native pin false. A further native close/reopen check retained saved identity while creating a new native tab. These observations establish the named journeys, not restart/crash durability or C++ browser-suite execution. A settled unstyled HTTP fixture also exposed an ordinary-page transparency bug; renderer remains off by default.

## Two-process workspace crash probe

`workspace-crash-checkpoint` and `workspace-crash-verify` reuse the same real CDP transport. [The recovery helper](../../../tools/browser_workspace_recovery.mjs) observes existing pages and the actual native metadata schema from `WorkspaceService`; it supplies no browser backend. API doubles test these phases independently. [Bounded parent native evidence](../../../browser/evidence/workspace-process-crash.json) records actual persisted-session Restore/discard runs and retained recent-write/navigation failures; broader recovery and acknowledgement durability remain unaccepted.

### Exact setup

The parent owns all browser launches, native UI setup, audited process-specific crash, cleanup, relaunch and Restore actions. The client performs none of those operations. Use the current candidate, one marked synthetic user-data directory and the same loopback fixture server as above. Before the first launch, seed vertical tabs as in the workspace-pin recipe. Renderer remains off by default elsewhere.

Both launches need these explicit flags, in addition to the normal disposable-profile flags:

```sh
--user-data-dir="$PROFILE" --profile-directory=Default \
  --enable-automation --myarc-visual-parity --myarc-workspace-sidebar \
  --remote-debugging-address=127.0.0.1 --remote-debugging-port=0
```

`PROFILE` must be the canonical absolute path, for example `PROFILE=$(cd "$PROFILE" && pwd -P)`. `--enable-automation` permits the existing upstream `Browser.getBrowserCommandLine` inspection. It does not bypass browser privileges. The browser-reported user-data directory must match exactly; the actual default browser context must own every fixture page. Bounded `Local State` inspection additionally requires `profile.last_used` to resolve to `Default` and `profile.info_cache` to contain only `Default`. An absent or empty `last_used` uses upstream's `Default` fallback. Close private contexts. `Default` must not be a symlink. Missing flags, a different Profile, copied port files or a mismatching expected browser PID fail before canary submission.

Through native controls, prepare exactly:

- One regular browser window, two distinctly named synthetic Spaces, such as Everyday and Reading.
- Original `http://127.0.0.1:8766/` page saved as a Profile Favorite, with its canonical URL unchanged.
- One other live Temporary page on [recovery.html](recovery.html), exact URL `http://127.0.0.1:8766/recovery.html`, owned by either existing Space. Open it through normal native navigation, never default-context `Target.createTarget`.
- Select the original Favorite in the desired Space. Its document must be visible and the second page hidden. Close all other page targets, including startup, NTP, version and extension-detail pages. Both fixture pages must remain live in that same native window.

The client also checks persisted `vertical_tabs.enabled` is true; seed it before startup for a reproducible fixture. Runtime enablement now works in the named0020 cases, but this recipe avoids a mode change during the probe. The real `Default/myarc-workspace-v1.json` must already contain that setup: two Spaces, two items and one window selecting the Favorite. The tool reads it and rejects a different setup; it never creates, overwrites or repairs workspace metadata. It does not require or start the MV3 worker. Existing `workspace-pin` behavior is unchanged.

### Checkpoint, parent-owned crash, verify

With the fixture server running and original Favorite selected:

```sh
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" workspace-crash-checkpoint
```

Checkpoint submits a newly invented canary through the original form, then requires actual localStorage and `Saved:` UI acknowledgement. Five matching real metadata/page observations must retain selection and ownership. Only after those observations does it exclusively create `$PROFILE/.myarc-m0-workspace-crash.json` with mode `0600`. That developer file records the old PID, browser build/executable, Profile path, exact expected URLs, canary, observed metadata with stable IDs/owners/placements/canonical URL/Space names/window selection, and visible/hidden projection. An existing checkpoint is preserved and causes failure; the parent must explicitly remove it only when starting a separate journey.

After checkpoint succeeds, the parent must audit the exact disposable browser PID and command line, record the actual SIGKILL of only that process, and own all helper cleanup. Keep the same Profile, executable/build, flags and fixture server. Relaunch without startup URLs. If upstream presents recovery UI, invoke its genuine **Restore** action and wait for both original pages to load. Record whether restoration was automatic, used Restore UI or used an explicit session-restore launch flag. Do not edit session files, rewrite workspace metadata, suppress recovery protection or open replacement fixture pages to satisfy verification. Close an extra startup/NTP tab through native controls if needed. A successful changed-PID check alone cannot establish that the prior exit was a crash.

Set `BROWSER_PID` to the new, independently audited browser PID, then run:

```sh
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" workspace-crash-verify
```

Verify reads the checkpoint and requires a changed PID with the same reported product/revision/runtime and executable path/size/mtime, the same owned Profile, both real restored fixture pages without duplicate URLs, acknowledged canary in localStorage, and matching persisted IDs/owners/placements/canonical URL/Space names/window projection. It checks the original page is actually visible, the other hidden, and both belong to one actual native window. Native target, context and browser-window IDs may change; those are not persistent identity comparisons. The checkpoint remains untouched for diagnosis. Failures identify missing Restore/pages, canary loss or workspace loss/rebinding. Scope violations abort immediately.

Recovery observation has a 20-second total budget after shared transport/PID setup, with individual CDP requests capped at three seconds. Settlement gets at most ten seconds and needs five matching samples separated by 200 ms. The shared connection setup retains its five-second HTTP/WebSocket bounds and fifteen-second initial PID request. File reads reject symlinks/nonregular files and cap marker/port data at 256 bytes, workspace/checkpoint JSON at 64 KiB and `Local State`/`Default/Preferences` at 1 MiB. Native target lists are capped at 32, including exactly two page targets for success. Missing or changing state fails rather than extending the wait indefinitely.

Local verification remains separate from native evidence:

```sh
node --check tools/browser_workspace_recovery.mjs
node --test tools/check_browser_foundation.test.mjs
python3 tools/check_docs.py
```

Thirteen local tests cover existing MV3 behavior, CLI guards, actual-schema validation, bounded file reads and API-double recovery outcomes. They execute zero native browser journeys. The checkpoint is taken **after observed on-disk metadata**, not at every acknowledged workspace write boundary. This cannot establish SessionService flush timing, every late-restore race, OS/power-loss durability or product CLI/MCP authorization. Disk identity plus native page projection is observed; the client exposes no in-memory Rust inspection API. Build comparison uses reported versions and executable stat identity, not a binary-content hash. The parent must retain actual crash provenance and native output separately before making any process-crash recovery claim.

## Two-window disk-checkpoint probe

`workspace-two-window-checkpoint` and `workspace-two-window-verify` add a separate,
read-only page journey through [the two-window helper](../../../tools/browser_workspace_two_window.mjs).
They reuse the crash probe's bounded JSON/schema parser, ownership/build checks
and CDP transport. Existing modes keep their contracts. This is developer
acceptance tooling, separate from product P0 CLI/MCP.

The parent owns native setup and every close/reopen, crash, launch and Restore.
Use a fresh canonical marked Profile with only `Default`, vertical tabs seeded
before startup, the current native stack through 0018, and the same explicit
flags and loopback server as the two-process recipe. Also load the existing
`workspace-pin-extension` fixture through its supported `--load-extension` route.
The helper wakes only that MV3 worker using the existing `ServiceWorker` lifecycle
API, verifies its identity/manifest/context, and reads `chrome.tabs.query({})`.
It never invokes the fixture's Pin/Unpin functions. Missing worker support fails;
keep browser protections intact.

Prepare through native controls:

- Two distinctly named Spaces sharing one regular Profile. Save the original
  `http://127.0.0.1:8766/` page (served by `index.html`) as one Profile Favorite.
  Keep exactly one Temporary on `http://127.0.0.1:8766/recovery.html`.
- Create window two with native Cmd+N, then activate the same saved Favorite tile
  there. This intentionally creates two native projections of one logical saved
  item. Parent separately verifies close-one/reopen retains its saved ID and the
  surviving projection, then returns to this final two-window setup.
- Select a different Space in each window and select the global Favorite in both.
  Both Favorite documents must be visible, natively pinned and active in their
  own windows. The Temporary must be hidden and unpinned in one of those windows.
  Parent checks its actual window/Space placement. Close all other tabs/windows.

Wait for native on-disk metadata to contain exactly two Spaces, one Favorite,
one Temporary and two distinct stable Rust window IDs, both selecting that same
Favorite, with one window per Space. Never manufacture or repair that file.
Global Favorite selection across Space switches remains the provisional existing
policy; the probe does not impose a single global live URL.

```sh
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" workspace-two-window-checkpoint
# Parent records actual owned-process crash, ordinary relaunch and genuine Restore.
# Set BROWSER_PID to the independently audited new browser PID afterward.
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" workspace-two-window-verify
```

Checkpoint exclusively creates `$PROFILE/.myarc-m0-workspace-two-window.json`
with mode `0600` after five matching disk/page/native-pin samples. It submits no
form, writes no fresh canary and never touches workspace/session metadata or
Profile exit state. An existing checkpoint is preserved. Verify requires changed
PID, unchanged build/executable identity and exact logical IDs, owners, saved
canonical URL/placement, Space names and both Rust window selections/Spaces.
Native tab/context/session-window IDs can change or be reused. Actual restored
pages must still occupy exactly two distinct native windows, one Favorite per
window and one Temporary total, with both Favorite pins read back independently.

Checkpoint allows zero startup tabs. Verify tolerates at most one exact
`chrome://newtab/` only while waiting; it must disappear before any successful
sample. Foreign contexts, unknown URLs or excess duplicate projections abort.
Verify never accepts a subset. Existing 20-second observation, three-second CDP
request and ten-second settle budgets apply. Each successful run needs five
samples separated by 200 ms; file/target bounds match the crash probe. The helper
creates/closes/moves/navigates no tab or window, launches no browser/server and
performs no process control.

Local checks, API doubles only, zero native launches:

```sh
node --check tools/browser_workspace_two_window.mjs
node --test tools/check_browser_foundation.test.mjs tools/browser_workspace_two_window.test.mjs
python3 tools/check_docs.py
```

The original 13 tests remain; seven added tests cover two-window schema, native
pins and target scope, checkpoint/changed-PID verification, missing restoration,
valid identity/owner/window-selection rebinding and malformed/oversized metadata.
The client observes disk Rust organization and real native projections separately;
it cannot directly inspect each native page/window's in-memory Rust binding.
Parent native UI/readbacks remain necessary for that association and close-one
provenance. Changed PID alone proves no crash. This covers an observed on-disk
checkpoint only, without site-storage acknowledgement, OS durability or release
claims. Previously persisted canary/metadata Restore and separate discard passed
in [0018's bounded journey](../../../browser/experiments/native-recovery-lifecycle.md);
fresh acknowledged localStorage still failed. Upstream's five-second default
commit delay is source configuration, not measured durability. Renderer stays
off by default.

## Scope and limits

- Renderer check deliberately crashes the selected renderer, reloads it and checks acknowledged localStorage, with the browser PID unchanged. It does not prove automatic workspace recovery.
- Off-the-record checks verify localStorage separation from the normal context and absence in a newly created private context after disposal. Cookie/history isolation, disk forensics and production workspace private-data handling remain untested.
- Native toolbar activation is a separate macOS accessibility/keyboard check. The Node client covers neither full UI accessibility nor product CLI/MCP grants.
- The candidate aborted once when default-context `Target.createTarget` entered synchronous Profile creation on the browser UI thread. The client reuses an existing ordinary tab and creates additional targets only inside explicit off-the-record contexts. The default-profile failure remains unresolved; upstream-versus-patch origin has not been isolated.
- Extension version-changing updates, removal and native messaging remain untested. These checks run manually with the external browser build; GitHub's standalone Rust workflow does not run them.
