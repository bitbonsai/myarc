# Development log

## 2026-10-26: State-specific storage retry build candidate

From `7045dda`, [0023](../browser/experiments/native-storage-retry.md) wires native load/save retry actions to existing exact-Profile service methods through deferred weak dispatch. Unavailable status scrolls above its action while native tabs retain at least half the remaining region. Canonical SidebarView changes only; service/core/schema and recovery holds stay unchanged. Baseline evidence remains untouched.

Full Chrome build exits `0`; four real-context syntax/GN-owner checks, bridge/lifecycle helpers, Rust fmt/clippy/32 tests, payload freshness and document links pass. Strict fresh full-stack replay matches 83 files, preserving 76 unowned files and all earlier optional patches. Three browser fixtures remain UNRUN. No app launches or native input; parent owns fresh fault/explicit-repair/retry acceptance. Private proof `/tmp/myarc-m0-runtime/mode23/` retains hashes and terminal logs. External source/build ownership returns at handoff with no build running.

## 2026-10-26: Native storage warnings have no retry action

Controlled own-profile metadata-path directory collision retained exact original bytes and triggered a real writer error after Create Space. Native warning reported memory-only changes and requested retry saving; no retry control. Clean relaunch against the same path showed load error/native fallback with mutations unavailable and no retry loading control. [Baseline](../browser/evidence/workspace-storage-retry.json).

Actual captures inspected; bounded AX walk omitted warning text. No input IDs/fields fabricated or recovery/durability pass claimed. Unsaved mutation intentionally stayed unsaved. Fault profile/backup retained privately, owned apps/helpers/server closed. Existing Profile-bound RetryLoad/RetrySave APIs provide the narrow next UI action.

## 2026-10-26: Same-process native late window restoration

Two sequential actual close/Cmd+Shift+T cycles on0022 retained saved ID/owner/URL, both logical window IDs/Spaces/selections and two real native pins. Original closed Temporary2 was removed, then native restoration allocated4/5 with the original Reading owner2. The second restore came from the survivor palette; deactivation closed it and survivor target/URL/time origin/position stayed exact. [Evidence](../browser/evidence/workspace-native-late-restore.json).

An initial checkpoint caught a loading page and failed; settled full checks passed five samples each. Only developer checkpoint artifacts were archived, never product metadata/IDs. Same browser PID, not crash or concurrent-restorer proof. Existing lifecycle hooks/consumer lifetime handled this case without a patch. Apps/helpers/server closed. Native suites, general abort/overlap and durability remain open.

## 2026-10-26: Exact preview command rechecked through0022

Ran `python3 tools/launch_native_preview.py` unchanged. Fresh marked profile had neither debugging/automation/test-extension flags nor DevToolsActivePort. Native Cmd+T and same-item Cmd+L reached actual Example Domain sites; save/pin, last saved-live close/reopen under the same logical ID/counter, explicit Unsave and new empty Space/header passed. Cmd+S and a held edge/inside/exit sequence were recorded/inspected. [Evidence](../browser/evidence/native-preview.json).

An immediate hover snapshot was premature; the held sequence showed overlay/reversal. AX exposed no web-area rectangle, so no numeric viewport remeasurement was counted. OS appearance/preferences unchanged. App/helpers exited; no server/debug endpoint started; synthetic profile retained privately. Broader matcher/accessibility, durability and distribution remain open. Guide now references0022 and an edge pause.

## 2026-10-26: Palette intent native followup

Integrated `2c1f49d`. Same native widget retained both shortcut intents, selected/focused drafts and unchanged-current reload; changed unmatched URL kept the target and new intent added one only on submit. Empty Space selection closed the prior palette; fresh current shortcut exposed no hidden URL. [Evidence](../browser/evidence/native-palette-interruption.json). Normal/app-only reduced sequences inspected, OS prefs unchanged. Capture guards stopped on an owned 66x20 non-key indicator; exact AX focus verification and release-before-postcondition checks fixed the private harness, not product code.

Two-window horizontal clean restoration/late enable passed the unchanged strict verifier with both pins/IDs. Owner-window close with an open palette retained the surviving saved ID and actual native pin; its shortcuts still worked. Saved destination remained stable on live navigation until deliberate Unsave/resave; no metadata repair. Outer-timeout/no-output and wrong harness field/literal-format assumptions were excluded/corrected. Existing global result matching remains unchanged; URI-alias UI route left the prior page, so broad URI semantics are unaccepted. Queued callback/deletion overlap and native browser fixtures remain unrun. Owned apps/helpers/server closed.

## 2026-10-26: Palette intent reuse build candidate

- Starting revision `9426f9e`. [0022](../browser/experiments/native-palette-intent.md) updates the live native palette when shortcut intent changes, preserves same-intent drafts and applies the Empty-Space guard before reuse. Existing reload/navigation and Rust policy remain intact. Close cleanup cancels history callbacks, checks delegate identity and retains anchor clearing/widget-before-delegate deferred deletion. Weak view checks protect focus across native activation.
- Four browser fixtures cover drafts, current reload/edit versus new-tab submission, an adapter-selected empty Space and cancel/reopen/owner-close ownership. All remain UNRUN. Actual nonzero production/fixture GN checks, final real-context clang syntax and full `chrome -j8` terminal build pass, exit `0` at 01:49:45Z after a forced recompile/relink from frozen final source. Initial syntax API errors were corrected. Strict indexed 82-file replay changes four files and preserves 78 unrelated files, canonical16 and all earlier patches through 0021. Document/payload/format/whitespace checks pass.
- No app/helper/server launched. Parent retains actual desktop interruption/focus/reduced-motion/history-overlap acceptance. Lifecycle helper and Rust suites were not rerun because their sources are unchanged. Recovery holds, adapter readiness, AI gate and renderer default OFF remain unchanged. External source/build ownership returns explicitly at handoff.

## 2026-10-26: Existing palette drops current-tab shortcut intent

Real Cmd+T/draft/Cmd+L dismissed the palette on `fdc20a3`; page URL/count/time origin stayed exact. Source closes any existing palette before considering new intent. [Failure](../browser/evidence/native-palette-interruption.json) retained. Routine repair assumption: switch intent in the same live widget, preserve same-intent drafts, keep empty-Space hidden-URL guards. Native owner close closed its palette; outstanding-history race was not directly observed. Apps/helpers/server closed before source transfer.

## 2026-10-26: Native AI Mode omission accepted narrowly

Integrated `5627bb3`, then personally checked fresh NTP, native address menu, regular/private/new-Profile contexts and horizontal/vertical changes. AI Mode button/toggle omitted with both prototype switches; default flag-off positively retained them without a workspace file/UI. [Evidence](../browser/evidence/native-ai-entrypoints-off.json).

Actual same-tab URL/query navigation worked; Google challenged the query, so results remain unaccepted. Native pin/generic Unpin and another clean-seeded actual SIGKILL/plain relaunch/genuine Restore passed strict persisted state/identity/canary checks. Wrong initial checkpoint selection was rejected and corrected through native UI. A native menu delayed graceful cleanup; a harness Cmd+Escape mistake was corrected to plain Escape with release, then Normal exit/app-helper/server cleanup verified. No source/storage guard relaxed. Native suites remain unrun; background AI requests, provider modes, broader focus/durability and product CLI/MCP remain open.

## 2026-10-26: Unsupported AI Mode entrypoint build candidate

- Starting revision `fbdf96e`. Parent native AX found an enabled AI Mode button despite absent built-in enablement/providers. [0021 and run record](../browser/experiments/native-ai-entrypoints-off.md) override Chrome's existing feature/policy eligibility gate only with both prototype switches. Existing Google/non-Google entrypoint and popup/menu callers suppress unsupported controls. Prefs, factory Profile/OTR routing, navigation/search, Rust policy and command guards are untouched.
- Added four switch-combination browser cases for regular/OTR eligibility, menu omission, native policy fallback and non-Google controls. Real-context clang syntax, actual production/fixture GN checks and full `chrome -j8` terminal build pass. Native browser suites remain unrun. The unchanged lifecycle helper executes successfully; strict 82-file replay preserves all 78 unrelated files, canonical16 and every prior patch artifact.
- No apps/helpers/servers launched or actual Profiles/OS settings touched. Parent owns integration and native acceptance. Renderer remains off; provider modes and denied background AI traffic remain unimplemented/unproven. External source/build ownership returns at handoff.

## 2026-10-26: Flag-off native site-storage crash control

Same patched build, fresh marked Profile, both myarc flags absent and no workspace file. Real fixture acknowledgement followed by owned browser SIGKILL after 12.098ms lost the note through plain relaunch/genuine Restore. Another write followed by 7225.205ms retained its note through the same actual recovery journey. One trial per condition, not commit-latency measurement or durability acceptance. [Record](../browser/experiments/native-site-storage-crash-control.md).

An earlier near-startup kill left native `Normal` on disk and offered no prompt; excluded from recovery-choice acceptance. A separately recorded existing-target revisit found no note. Exact-window input also stopped before typing when stock omnibox created another window; no guard weakened. Later controls waited for the real native crash marker, without editing it. Apps/helpers/server closed. No storage-source change, pristine upstream comparison, immediate acknowledgement or power-loss guarantee.

## 2026-10-26: Restore consumer lifetime native acceptance

- Integrated `7a14927`. Fresh horizontal setup, clean native restoration before sidebar presentation and late vertical enable passed the unchanged single-window verifier. A subsequent audited SIGKILL/plain relaunch retained metadata through vertical enable during the genuine recovery prompt; actual Restore passed again with the previously persisted canary. [Evidence](../browser/evidence/workspace-late-bindings.json).
- Two real windows restored horizontally with stable logical IDs and both native Favorite pins, then passed the two-window verifier after late presentation. Three app-only reduced-motion cycles preserved page target IDs/time origins, model counters and horizontal address focus. Fresh owned-window held-state recordings were inspected; exact timing remains unmeasured and OS preferences unchanged.
- Closing one window preserved saved identity and a usable survivor. Closed logical rows intentionally remain candidates until startup; complete native restart retired the closed candidate and retained surviving state. A scratch immediate-retirement assertion was discarded against that existing contract. An inactive visibility failure was excluded; exact owned activation preceded successful verification. All owned apps/helpers/servers closed. Fresh post-0020 private mode/disposal, two regular Profile workspace/note isolation and explicit flag-off H/V/H checks subsequently passed. Other Profile now has a core before vertical presentation and enables its sidebar in the same window without reopening. Primary workspace bytes stayed unchanged; flag-off created no workspace file/UI and retained the actual page. Initial native Local State persistence lag was awaited; no metadata repair. One typed-query snapshot is not full async-history proof. Native C++ suites, fresh-write/OS durability, cookies/credentials, Guest/system and broader overlap/races remain open; renderer stays off.

## 2026-10-26: Warm mode repair and cold restore failure

- Integrated 0019 as `78b303f`. Native late enable exposed a missing startup-completion handoff: a new bridge stayed unavailable. Parent added a guarded native-ready notification after setup, direct GN dependency and a normal late-activation fixture. Corrected app build, fixture clang syntax, production GN check and strict 78-file replay passed. Nine files change; 69 unrelated and all 16 owned payload files remain identical. Native C++ suites remain unrun.
- Fresh native same-window Space creation, three mode cycles, horizontal address focus and hidden-sidebar exit retained exact page target IDs/time origins and model IDs/selection/counters. Hidden-state preservation was not inferred from a scratch toggle assumption. [Evidence](../browser/evidence/workspace-mode-bootstrap.json).
- Clean quit with horizontal mode, explicit session restoration, then late vertical enable failed the unchanged strict verifier: Temporary ID2 was pruned, a second Favorite ID3 appeared, and new Temporary ID4 acquired the wrong Space. This is workspace binding loss, separate from site-storage commit delay. App-only reduced motion reported true during that failing case. Failed metadata/profiles stay private; owned apps/helpers/server closed. The next bounded repair must preserve native restore hints independently of UI activation, without guessed IDs or metadata rewrites.

## 2026-10-25: Two regular Profiles and runtime-mode gap

- Native Profiles menu created a second regular Profile without an account. Separate contexts, UI-selected Spaces, persisted Favorite URLs and same-origin notes stayed isolated in one browser process; the primary workspace bytes stayed unchanged. Before visiting the primary site, a real typed palette query in the other Profile omitted its title. [Evidence](../browser/evidence/workspace-native-profiles.json). Numeric IDs remain scoped to native Profile namespaces.
- Direct duplicate CLI launch was rejected by ProcessSingleton; no lock deletion/override was attempted. Native profile creation reset the seeded vertical preference. Enabling vertical tabs in that existing window changed stock layout while myarc toolbar/workspace remained uninitialized. Closing/reopening through native Profiles after the real preference change constructed the correct adapter and allowed the bounded isolation checks. `BrowserView::AddedToWidget` currently owns initialization only for initial vertical mode. Runtime bootstrap is the next narrow fix.
- Native AX/menu returns were followed by actual projection checks; an early uncommitted menu request was not counted as save success. Owned apps/helpers and server closed. No C++ changed in this acceptance slice. Wider privacy, native C++ suites and durability remain open; renderer stays off.

## 2026-10-25: Native private workspace lifecycle

- Fresh marked normal Profile saved a Favorite and named Space. Native Cmd+Shift+N opened a distinct private context with its own workspace. The normal Favorite/Space were absent. A real private Space and acknowledged fixture note stayed separate from the normal note; regular workspace bytes remained unchanged.
- Exact native close of the last private window followed by fresh private reopen discarded the prior note and Space. Normal state remained intact. Inspected the visible memory-only notice and native Incognito accessibility title. [Evidence](../browser/evidence/workspace-native-private.json). No CDP context/target creation or security bypass; raw artifacts stay private.
- A refreshed blank-row AX action returned failure; the actual tree was inspected before retry. Native apps/helpers and owned server were closed. Two regular Profiles, Guest/system, cookies/credentials and broader extension/privacy guarantees remain open. No implementation changed; renderer stays off.

## 2026-10-25: Two-window saved identity and actual Restore

- Integrated `2b98a0a`; twenty local API-double tests passed, retaining all thirteen prior checks. Fresh native setup used two Spaces and native Cmd+N. One logical Favorite had one active/pinned page per window, plus one hidden Temporary. Native saved-menu close of one copy settled before assertion; the surviving copy remained pinned and retained saved identity. Native tile reopen restored both projections. Only the developer checkpoint was archived/recreated; backend metadata stayed untouched.
- Audited SIGKILL, ordinary relaunch without startup URLs/restore flag and genuine Restore passed five strict comparisons. Pending metadata stayed exact; stable logical window/item/Space IDs and both real pins returned. Exact-window Temporary activation changed only its own persisted selection; restoring the original projection passed again. [Evidence](../browser/evidence/workspace-two-window.json). This does not expose direct in-memory Rust bindings or prove arbitrary late/overlapping restoration.
- Identical initial window bounds required foreground/AX-main disambiguation before moving only the owned second window. Exact Profile/PID/window and unlocked/focus checks remained intact. Owned apps/helpers and server closed. No C++ implementation changed; native C++ suites remain unrun. Recent site-storage acknowledgement, wider Profile/private boundaries and OS durability remain open; renderer stays off.

## 2026-10-25: Bounded native recovery and preview launch

- Integrated lifecycle repair as `2a0ef32`. First real crash repeat retained exact pending/restored workspace identity, then failed the strict canary comparison. Unchanged upstream DOMStorage configures delayed batching; no storage policy or verifier was weakened. A separate clean-seeded, verified-persisted canary subsequently survived audited SIGKILL, plain relaunch and genuine Restore with five exact comparisons. Fresh explicit discard retained Favorite/Space identity and resumed real Space creation. [Evidence](../browser/evidence/workspace-process-crash.json) distinguishes these scopes; immediate acknowledgement and OS durability stay open.
- Corrected scratch expectations against actual contracts: global Favorites may retain selection across Spaces, and the persisted no-selection sentinel is string `0`. Native Profile cache readiness was observed rather than written by the driver. Raw failure/control artifacts remain private. Owned apps/helpers and fixture servers were closed.
- Integrated safe launcher `c263228`; ten local process-double tests passed. Parent ran `python3 tools/launch_native_preview.py`, verified its real isolated window and absent debugging/test-extension flags, navigated a public example page, created/switched an empty Space and inspected fresh native material/header. This is preview tooling, separate from P0 product CLI/MCP. Full native C++ suites, broader focus/motion and multiwindow cases remain unrun; renderer stays off by default.

## 2026-10-25: Pending recovery choice build candidate

- Starting revision: `2101a46`. Preserved the actual process-kill failure and strict verifier. [0018 and run record](../browser/experiments/native-recovery-lifecycle.md) add recovery-choice counts to shared lifecycle readiness. The existing exact-browser observer starts before async consent and transfers through Show into the dialog, then into local action ownership before Close deletes the delegate. Restore starts its real restorer before choice release; discard/cancellation use normal reconciliation. Profile teardown releases once under its shutdown gate.
- Extended the lifecycle helper and existing native fixture for pending/Restore/discard/cancel and window-close paths. Actual opaque-identity helper, native source syntax, two GN checks and full `chrome` link pass. Strict upstream-plus-owned reverse/replay checks all 75 captured files; eight change, 67 remain identical. Canonical payload stays exactly 16 files; 0012/0016/0017 and unrelated optional sources are unchanged.
- Native cases executed: zero. No app launch, desktop input, actual-profile access or service kills. External source/build ownership returns to parent; no build remains running. Parent must use fresh synthetic data for actual process-kill/Restore/discard acceptance. Renderer remains off; branded consent/Profile teardown are source-audited only, OS/write-boundary durability remains unproven.

Keep significant outcomes, failures and next actions reproducible. Newest entry first. No private exports, page titles/URLs from real browsing, credentials or raw user transcripts.

## 2026-10-25: Actual process crash exposed early recovery pruning

- Integrated `a9b73b9` as `5bf05df`; reran 13 local probe tests. Prepared real two-Space Favorite/Temporary state, acknowledged a form/localStorage canary, and checkpointed observed native metadata. Audited SIGKILL targeted only the marked disposable browser, followed by normal relaunch without startup URLs or a restore launch flag and actual Restore-button invocation.
- Both URLs returned, and Favorite/Space identity survived. [Native verification failed](../browser/evidence/workspace-process-crash.json): Temporary ID 2 had already been pruned while the prompt waited, startup NTP became ID 3, and the restored Temporary was rebound to ID 4. The verifier was kept strict. Failure profile/logs remain private; its test app and owned server were closed.
- Recovery choice is missing from the native readiness barrier. The next bounded source fix must hold pruning/binding/persistence until Restore or deliberate discard resolves, including async prompt/lifetime paths. Default renderer remains off; full crash recovery, write-boundary and OS durability stay unaccepted.

## 2026-10-25: MA-007.c two-process crash probe handoff

- Starting revision: `d82f4f2`. Extended the retained native developer client with `workspace-crash-checkpoint` and `workspace-crash-verify`, plus an original second loopback page. The [recipe](../tests/fixtures/browser/README.md#two-process-workspace-crash-probe) requires two actual Spaces, one selected Profile Favorite, one live Temporary and already-persisted metadata. Schema comes from the owned `WorkspaceService` encoder/decoder; browser ownership uses upstream command-line/default-context inspection plus bounded single-Profile `Local State` read-back, without navigating saved tabs to a version page.
- Checkpoint submits and observes a real form/localStorage canary, retains bounded on-disk metadata and page projection, then exclusively writes a developer file inside the marked directory. Verify requires a changed matching PID/build/Profile and five settled comparisons of both real pages, canary, stable IDs/ownership/canonical URL/Space names/window selection. It rejects duplicates, foreign targets and incomplete restore. Browser lifetime, genuine Restore and crash provenance stay with the parent; no worker, navigation, target creation or metadata mutation is added to these phases.
- Local checks: 13 Node client/fixture tests passed, including API-double lost-canary, persistent-ID rebind and missing-page failures. Syntax, document links and whitespace checked. Native executions: zero. No actual browser profile access, app launch, external source/build change or desktop action. Parent must integrate, review and run the audited SIGKILL/relaunch/Restore journey against synthetic data.
- Limits: checkpoint follows observed disk metadata, so acknowledged-write boundaries, SessionService lag, every late-restore race and OS/power-loss durability remain open. Build identity compares reported version/revision and executable path/stat, not content hashes. Source/API doubles establish local validation only. Existing modes and actual MV3 pin probe remain intact; renderer stays off by default.

## 2026-10-25: Native backgrounds and one clean restart

- Integrated 0017 and checked fresh owned-window pixels across ordinary, blank, authored and dark canvases. Same-target navigation, history back/forward and NTP return preserve the expected background boundary. An Empty Space detached a blank page; privileged hidden HTTP navigation and native Space return restored an ordinary white canvas. No OS appearance or page CSS overrides were used by the driver.
- Real extension Pin/Unpin passed again, followed by explicit UI Unsave and saved close/reopen with stable identity. Native graceful teardown produced no fatal assertion. [Background evidence](../browser/evidence/page-background.json) separates these actual checks from the source-only browser case.
- One normal restart with explicit `--restore-last-session` and no startup URLs restored two native pages, two Spaces and stable IDs/selection under a new PID. Space controls still worked. [Restart evidence](../browser/evidence/workspace-clean-restart.json) establishes clean restoration only; crash/OS durability, default startup policy and broader lifecycle/privacy gates remain open. Renderer stays off; full C++ suites and motion/theme variants were not rerun.

## 2026-10-25: Blank-only page material build candidate

- Starting revision: `7e827d5`. Traced ordinary-page translucency to unconditional transparent page-base and renderer-view backgrounds. [Separate 0017 and run record](../browser/experiments/native-page-background.md) restrict material to blank/NTP contents, reset ordinary pages through Chromium's default/theme hooks, and clear outgoing overrides before unobserved navigation. Primary-page notifications cover same-WebContents navigation and page reactivation; workspace-empty remains detached.
- Added one regression to the existing native fixture for unstyled/authored/dark canvas, navigation/history, detachment, hidden navigation, switching and NTP return. Real-header syntax, two GN header checks, full `chrome` compile/link, formatting, docs/foundation freshness and strict 72-file reverse/replay pass. All 68 unrelated external files and every prior patch remain unchanged, including frozen 0012/0016 and canonical 0001.
- Native cases executed: zero. No app launch, desktop input, profile access or system-service kill. Build terminated successfully; parent owns fresh native color/screenshot/transition and saved-journey acceptance. Renderer default remains off; recovery/durability and broader gates remain open.

## 2026-10-25: Native saved-identity acceptance

- Integrated the one-task extension probe and ran it against a fresh marked profile using normal upstream local-extension loading. Real Pin created a persisted Favorite; generic Unpin retained actual pin and saved ID/canonical URL/owner/placement. Four local argument/fixture tests also pass. The native C++ suites remain unrun.
- Actual sidebar Remove from Profile Favorites converted the same selected live item to Temporary and cleared saved URL/native pin. Native close/reopen with another page retained saved identity but created a new native tab ID. [Evidence](../browser/evidence/workspace-saved-identity.json) records real API, metadata and UI observations. Asynchronous close was checked after settlement rather than counting its first still-open snapshot.
- Fresh resting captures revealed normal unstyled HTTP pages inheriting material. The cause is unconditional transparent base/background setup for visual-parity contents, so the next bounded task restores ordinary page opacity while retaining blank translucency. Existing QA profile stayed untouched. Multiwindow, real recovery and durability remain open; renderer default remains off.

## 2026-10-25: Executable extension generic Unpin probe

- Starting revision: `87ffa08`. Added one `workspace-pin` mode to the retained [native client](../tools/check_browser_foundation.mjs), with an original, tabs-only MV3 fixture. Existing extension/restart/runtime modes retain their paths. The probe reuses one exact loopback tab, checks its native Profile path and worker identity, then awaits real pin projection and on-disk Favorite adoption before requesting generic API Unpin. Stable saved ID/canonical URL/owner/placement and native pin must survive. It leaves the synthetic Favorite for separate explicit UI Unsave.
- [Launch/run instructions](../tests/fixtures/browser/README.md#workspace-generic-unpin-probe) seed vertical tabs before startup and enable the optional shell/workspace renderer only in a fresh marked profile. One supported worker start handles idle MV3 lifecycle; bounded observations and detach replace any keepalive. Normal upstream extension-loading restrictions remain intact.
- Verification: Node syntax for client, fixture worker and test file; four local manifest/argument/fixture tests; document links and whitespace. API doubles test fixture guards/forwarding only. Native executions: zero. No app launch, browser profile access, external source/build edits, desktop input or installs. Renderer default remains off. Parent must run this real-browser probe, then deliberate native UI Unsave; restart/crash durability and native C++ suites remain unaccepted.

## 2026-10-25: Recovery popup native acceptance

- Integrated the single-task 0016 candidate and repeated the fresh forced crashed-exit fixture. Actual recovery popup appears instead of the earlier startup CHECK failure. It stays visible with the sidebar hidden and repositions inside the owned window after resize. Fresh screenshot/AX evidence was inspected.
- Restore invocation dismissed the popup without a crash and kept the synthetic page. A second fresh legacy-shell profile, without the workspace flag, also showed the popup; its close button dismissed the popup without closing the page. The fixture had no prior-session payload, so this establishes popup/actions only. The first Close automation stopped safely on an ambiguous native label; exact recovery-window scoping resolved it.
- Rechecked the two-page/three-Space neutral-header journey after Restore, including background loading, palette cancellation, hidden-page Cmd+R and working populated-page Reload. [Evidence](../browser/evidence/recovery-anchor.json) separates actual observations from build proof. Automated native cases, real session restoration and durability remain unaccepted; renderer default remains off.

## 2026-10-25: Recovery anchor build candidate

- Traced the freshly reproduced startup crash to shell layout hiding the app-menu button before its fallback can register against a visible browser widget. Added a shell-only BrowserView child at the existing fallback position and used it through shared menu anchoring. Upstream recovery UI/actions, stock CHECKs and complete 0012 remain unchanged.
- [Separate 0016 and run record](../browser/experiments/native-recovery-anchor.md) retain a two-parameter regression in the existing recovery fixture. Real-header syntax, three GN header checks, full `chrome` compile/link, formatting, payload freshness and strict 72-file reverse/replay pass; 67 unrelated external files remain identical. Initial test declaration/accessor errors were corrected using existing APIs.
- Native cases executed: zero. No app launch, desktop input or profile access. Renderer remains off. Parent owns forced-exit fixture, screenshot/AX and action acceptance; actual crash recovery and durability remain open.

## 2026-10-25: Empty-Space header native acceptance

- Integrated the one-task header fix and repeated the actual two-page/three-Space journey in a fresh marked profile. Empty now shows an enabled neutral address field, with prior URL/site information and page-only navigation controls absent from pixels and accessibility output. Both actual pages remain hidden.
- A privileged synthetic background-page reload exercised loading updates without re-exposing the header. New Tab opened an empty-input native palette; Escape kept the page count unchanged. Native Cmd+R in Empty preserved both hidden URLs/time origins. Different-page return restored Reading metadata and working native Reload; same-page return retained both live pages.
- Fresh private captures and bounded observation assertions passed. [Evidence](../browser/evidence/workspace-empty-header.json) records this manual acceptance separately from the original leaking-header run. Automated native browser cases remain unrun. Palette still offers other-Space live tabs; broader command, saved identity and lifecycle/privacy acceptance remain open. Renderer default remains off. Next bounded source task targets the existing recovery-bubble anchor crash.

## 2026-10-25: Empty-Space header candidate fix

- Traced the native metadata leak to a skipped blank toolbar update plus sidebar layout restoring location-bar visibility. Reload also enabled itself inside `ChangeMode` after the blank-state disable. Updated optional 0012 to refresh neutral address text, hide site information/page actions and page-only navigation buttons, and disable Reload after mode changes. Existing Chromium paths restore the different populated page. No Rust/workspace policy or canonical foundation source changed.
- Extended the existing two-command-mode transition regression. Its real-header syntax check passes after replacing a private toolbar accessor with the public provider. All three production translation units compiled and the incremental `chrome` target linked; the final null-safe URL-drag follow-up also compiled/linked. Three GN header checks, strict 73-file baseline reverse/replay, formatting, document links, payload freshness and whitespace pass. Seventy captured external files outside this change remain identical.
- [Run record](../browser/experiments/native-workspace-command-safety.md#empty-header-candidate-fix-2026-10-25) separates this build from the historical smoke. Native cases executed: zero. No app launch, desktop input or profile access. Renderer remains off. Parent must repeat populated -> Empty -> different populated with fresh screenshot/AX and hidden-page Reload checks before accepting the fix.

## 2026-10-25: Same-origin blog card navigation

- Local blog cards used absolute production URLs, so index-to-post clicks crossed origins and skipped native View Transitions. Title/thumbnail links now use relative routes; canonical metadata and schemas keep production URLs.
- Previous browser checks rewrote card URLs before clicking, hiding this failure. Removed that rewrite and added actual origin/path assertions. Title and thumbnail journeys now both run unchanged: twelve normal-motion navigations created transitions; six reduced-motion navigations did not. Fourteen unit checks and metadata checks passed. Safari/Firefox remain unchecked.

## 2026-10-25: Explicit lightbox opening order

- The backdrop-only change was deployed and normal live HTML/JS read-back matched, but the user still observed a blink. Replaced the content starting-style rule with an explicit hidden base state. Normal opening now paints the modal backdrop before revealing content on a later animation frame; reduced motion remains immediate.
- Closing cancels a queued reveal and clears visible state. Browser checks sampled 20 entry frames: content began at opacity zero, its opacity rose monotonically, and the backdrop stayed fully blurred. Early-close cancellation, keyboard controls, phone layout and native navigation checks passed; fresh sequence inspected. Fourteen unit checks passed. Safari/Firefox remain unchecked.

## 2026-10-25: Immediate lightbox blur

- Removed the backdrop's transparent starting state and opacity transition, which exposed the clear page before the blur appeared. Kept the content-only 300ms fade/scale entrance and the blur-only background.
- Entry checks now require backdrop opacity 1, zero transition duration and the full blur while content is still animating. Fourteen unit checks and the retained browser suite passed. A reused test-session name hit a reopen/wait timeout; interaction phases now use distinct session names. Safari/Firefox remain unchecked.

## 2026-10-25: Screenshot grid and native lightbox

- Grouped adjacent image/caption pairs into a two-column gallery at desktop and phone widths. Screenshot boxes use contain sizing to preserve the full image; responsive sizes account for the narrower columns.
- Added one small content-hashed first-party script on image-bearing articles. Native dialog supplies modal behavior with a blurred backdrop, full-size image and retained caption. Escape, outside click and close button restore focus. Real image links remain as fallback; modified clicks retain normal behavior. Theme selection and navigation still need no JavaScript.
- Follow-up styling uses a transparent, blur-only backdrop and no solid dialog panel. Captions have pill surfaces; Lucide’s X retains a 44px accessible close target. The content group opens with a 300ms fade/96%-to-100% scale transition, disabled under reduced motion.
- Chrome for Testing 151 hit repeated long-session timeouts and one macOS menu-stack crash during keyboard checks. Browser phases now use fresh sessions with explicit DOM readiness; the final retained run passed. The single-control modal keeps Tab on its close control.
- Fourteen unit checks passed, including gallery grouping, section boundaries, ordinary image links and runtime inclusion. Browser checks passed across existing themes/widths; keyboard opening, both images, native modal focus, Escape, close button and backdrop close were exercised. Fresh grid and phone-dialog captures inspected. Metadata/caption/resource checks passed. Safari/Firefox and screen-reader testing remain open.

## 2026-10-25: Centered blog reading column

- Centered the shared journal container at 68ch, aligning article title/metadata/body and blog index content without centering the text. Kept the wider header/footer and 1.5rem phone gutters.
- Added browser assertions for the column midpoint and matching child widths at every existing viewport/theme. Thirteen unit checks, metadata/caption checks and the retained browser suite passed. Fresh desktop capture inspected; Safari/Firefox remain unchecked.

## 2026-10-25: Project purpose, privacy commitment and visible blur/fade

- Reworked the first post around Arc's announced sunset and the reason to continue its workspace ideas. Added explicit account-free and no-behavioral-tracking commitments, with a release privacy audit still required. Aggregate release-download counts are separate from hosting request logs. Updated the product contract without claiming native privacy compliance or creating tracking infrastructure.
- Reused the approved build screenshot in the closing build/reference section. Both article images have short explicit captions; first-image social metadata still uses the actual myArc build. Publication rejects missing/empty image captions, verified by fault injection. Captions retain the observed reference treatment with readable 1rem resting text.
- Inspected karinwolff.com’s public timing/blur behavior. Replaced the hard-to-see .18s fade with original CSS for a 1s blur/fade and a separately captured header. Actual snapshot animations are checked for 1000ms duration and 32px blur; reduced motion creates none. No runtime JavaScript was added.
- Thirteen unit checks and the retained layout/theme/focus/navigation suite passed. Fresh transition recording inspected. Caption captures now position instantly to avoid sampling an unfinished smooth scroll. Safari/Firefox and native-browser privacy compliance remain unverified.

## 2026-10-25: Native website View Transitions

- Added shared CSS opt-in for same-origin cross-document transitions, with a .18s root fade. Reduced motion opts out; unsupported browsers retain normal navigation. No runtime JavaScript or router was added.
- Retained checks serve the actual generated pages over loopback HTTP. Eight desktop/light and phone/dark navigations created native transitions; four reduced-motion navigations did not. A fresh navigation recording was inspected. The first test attempt clicked during an active transition; the check now waits for rendered frames and finite animations before its next click.
- Thirteen unit tests, static resource/metadata checks and the layout/theme/focus/motion suite passed. Safari/Firefox behavior and precise runtime animation timing remain unmeasured.

## 2026-10-25: Shorter journal prose and figure treatment

- Applied unslop to the project entry: shorter sentences, fewer repeated goals and review explanations, with model identifiers and verified development facts preserved. The previously removed first-image caption stays absent.
- Inspected the public reference blog. Kept the existing underline hover treatment and added semantic figures with explicit captions. Images brighten subtly on hover/focus; captions use normal 1rem quiet text and brighten to main text, preserving resting contrast rather than copying the reference opacity.
- Thirteen unit checks passed, including explicit caption association and ordinary-emphasis preservation. Retained browser checks passed across themes and widths; fresh figure rest/hover captures, focus and reduced-motion checks passed. Sitemap/schema/local-resource checks pass. Safari/Firefox remain unmeasured.

## 2026-10-25: Project-first journal, native capture and optimized delivery

- Rewrote the first entry around browser development, bounded delegation and the public bitbonsai context/wrap skills. Verified native-thread model selections before naming GPT-6.1/high thinking and Sonnet 5.5/high effort. Removed website-adjustment and hosting anecdotes. The Arc-like empty shell and nightly artifact are goals, not reported deliveries.
- Captured the requested public homepage behind the native command palette in a separate marked disposable profile. Verified the browser PID before navigating an existing tab, used a window-only capture and closed the owned instance. The approved current-build image now leads the post and social previews; the Arc target remains a separate reference.
- Completed WebP/responsive post images, content-hashed shared blog assets and automatic Markdown/discovery exports. Preserved the standalone homepage. Added visible author attribution, canonical/social discovery and Blog/BlogPosting/BreadcrumbList data with a pre-publish metadata/resource check. Phone gutters are 1.5rem and the smallest homepage headline is 2.5rem.
- Twelve unit checks, three generated-page SEO checks, document links and retained layout/theme/focus checks passed. Native capture establishes the shown page/palette state only. The earlier delivery pass verified gzip on live HTML and text/Markdown responses; tiny text responses may remain uncompressed. Published `32d8c3b`; index, article, Markdown and both LLM discovery files match local artifacts over normal HTTPS. Search indexing, AI citation behavior and Safari/Firefox remain unmeasured.

## 2026-10-25: First native Rust Spaces journey

- Integrated corrected 0012 from `020a8dc` after the single build agent's `69b4c61` handoff. Its preflight had stopped on macOS notification services mapping localization resources. Parent verified those were resource-only mappings, left system services alone and continued. All 19 upstream results matched the corrected candidate, with the other 54 baseline files unchanged. GN generation, six actual header families and full native app build passed.
- Actual startup exposed four faults absent from source-only evidence: accessibility name before role, wrong tab-change variant extraction, unsupported heading typography and subpixel text on transparent layers. Fixed the shared owned adapter/sidebar code, refreshed the canonical 16-file payload and rebuilt successfully. One protected-label API attempt failed compilation and was replaced with the public setter. Failed marked profiles stayed private; each retry used a fresh one.
- The optional sidebar initializes only with vertical tabs enabled at startup. Runtime preference enablement on an already-created window left the stock body, so the final fresh acceptance fixture seeded that one upstream preference before launch. Existing QA/public-site profile and OS appearance settings were preserved.
- Native UI created three Spaces and opened a second synthetic page through the palette. Page observations verified two live pages, correct visible/hidden transitions, both hidden in Empty, and a return from Empty to the other native page. Reload in Empty left both hidden URLs and time origins unchanged. Actual workspace metadata matched the selected Space/item. Fresh private captures were inspected; automated native browser cases executed: zero.
- Empty still shows the previous URL and enabled-looking page controls. This remains an observable metadata/UI blocker despite hidden page pixels and blocked Reload. Keep the renderer off by default. Next bounded task is clearing and restoring the header/control state across empty transitions; broader lifecycle, extension and durability gates remain open.

## 2026-10-25: Native hover peek and short saved-page fade

- Matched the supplied reference's temporary overlay separately from persistent expansion. Applied 0013, corrected window-relative Cocoa coordinates to screen coordinates, and added a moved-window/inside-hover regression. Full native builds and four current real-header syntax checks pass. Real edge movement opens the hidden legacy sidebar, holds it inside and dismisses it on exit. Actual page observation stays 1280 × 788 through the overlay.
- Native inspection caught stock hover behavior revealing truncated pin labels. Kept parity tiles icon-only and centered, then verified the rebuilt window. Applied 0015 to restore real NSWindow caption visibility/reservation during the peek. Their translation/clipping still needs work; automated native regressions remain unrun.
- Added the conditionally requested 120 ms incoming saved-page fade through the existing page compositor in two production files. Fresh owned-window recordings show opacity intermediates on normal switches and direct cuts with the app-only reduced-motion flag. Returned to normal flags without changing OS preferences. The Rust saved-category route remains unaccepted.
- Updated the first native pin to the user's requested public site and verified its actual favicon and normal-quit restoration. Profiles and recordings stay private. Only the marked browser was quit/relaunched. The optional workspace renderer remains off while combined command safety and lifecycle runtime checks are pending.
- Diagnostic attempts caught non-key browser status/capture overlays in a too-strict input guard, plus retained synthetic Command flags. The private runner now verifies actual AX focus for tiny owned overlays and clears modifier flags on key release/plain mouse input. Fresh plain-click recording was checked after those fixes. Rejected or failed recordings were removed; a failed overwrite was never accepted as a fresh run.

## 2026-10-25: Native workspace lifecycle integration candidate

- Integrated the base-only lifecycle dispatch and optional native hooks from `9c2bd50`. Kept the owned adapter/helper in the canonical 16-file foundation payload; normalized 0011 to upstream hooks only. Strict reverse/apply reproduction matches all 11 affected upstream files byte for byte.
- Independent source review exposed premature startup readiness and reset writes before binding. Added a startup launch counter, deferred weak/non-nestable completion and cache invalidation before guarded reset re-emission. Cancel-resume now ignores an unlatched shutdown. Bound windows retain their existing foreign metadata. Native restorer notification follows its Profile keep-alive construction; global dispatch is checked on its owning thread. A second source review found completed hints could miss persistence if obsolete-window retirement failed; moved that emission before retirement.
- GN generation/header checks, eight actual native object compilations and the boundary executable passed. Recompiled the corrected adapter/restorer/startup objects and reran expanded startup-counter checks. These use opaque Profile identities, not actual browser restoration. An initial object build named the header-only commands target incorrectly; corrected it to the implementation target before compiling. GN formatting also needed the Chromium working directory.
- Full relink, Undo Close, foreign-session hints, metadata retry and actual cancelled quit remain unverified. The workspace body stays disabled. Interactive motion verification remains paused at the locked desktop; no native input or lock bypass occurred.

## 2026-10-25: Fixed-width sidebar motion candidate

- Compared two user-supplied 30 fps recordings. An Arc intermediate frame translates the intact sidebar left; myArc narrows its child layout and stacks three tiles vertically. Recorded the geometry contract in the [motion investigation](../browser/experiments/native-sidebar-motion.md). Reference media stays private.
- Integrated corrected native item bindings and sidebar menus, the optional Rust body, and a fixed-width translated/clipped sidebar. The shared tab-strip layout now reserves both gutters. Full native compilation/linking and frame/tab GN checks pass; standalone Rust still executes 32 tests. Native C++/Rust ownership and metadata checks also pass. The new browser regression remains unexecuted.
- Restored the QA Profile from forced Light to Follow System through Chromium's actual appearance control. Confirmed the native callback, persisted preference and dark shell. Existing native favicon rendering worked; the local pages had no icons. Added invented PNG fixtures, reloaded only those pages and observed all three native icons. A rebuilt resting capture preserves the right gutter.
- Gracefully quit the verified disposable browser before replacing libraries, then restored its session in a changed process. Kept the unaccepted workspace body disabled. An attempted motion recording stopped on a foreground-window guard before toggle input; the rejected recording was deleted. Fresh motion/reversal/reduced-motion evidence remains pending. Caption controls and broader recovery/command/lifecycle gates remain open.

## 2026-10-25: Reviewed workspace rules and native sidebar candidates

- MA-007.c groundwork: integrated the dependency-free Rust model for stable Space/item/window IDs, Profile favorites and Space pins. Independent review exposed unbound temporary rows after restore and dormant saved-item demotion. Added explicit live-binding reconciliation, unsave and within-section reorder commands. Native/palette activation can reveal an item's owning Space through Rust.
- Parent reran formatting, strict clippy and 32 actual Rust tests: six original unit tests and 26 semantic checks. The foundation payload matches the current core. Typed snapshots prove in-memory validation; native session binding and storage durability remain open.
- Corrected split-tile minimum sizing and shared background painting. Combined native compilation passed with the sidebar-toggle and current-tab palette candidates. GN checks pass for frame, tab layout and the corrected omnibox dependency. Packaged the six-source current-tab palette delta separately; exact scratch reproduction passed. The final include-layer correction also passes object-only compilation; running browser binaries were untouched. Full relink and runtime checks remain pending.
- Three synthetic pages were pinned through the native Tab menu. An OS app-data permission prompt interrupted clean captures and remaining interaction checks; the agent left that permission decision to the user. Resume only after verifying the disposable profile, PID and actual foreground window. Sidebar reversal, split hover/selection, current-tab editing and restart are unaccepted.
- Built and executed the native bridge/codec check without replacing running browser binaries. Seven legacy checks, including 1,000 ownership round trips, plus workspace transitions and metadata assertions passed. The first run rejected a test assumption: Chromium accepts a host containing a space and escapes it to `%20`. Replaced that negative fixture with an empty host and retained the measured canonicalization as a positive regression. The codec follows Chromium URL policy, without a separate DNS-name parser. Added explicit raw JSON UTF-8 validation and test-failure diagnostics. The duplicate ANGLE class warning persists.
- Profile-owned storage, a Rust-snapshot sidebar view and the native tab/session adapter are merged source candidates with thirteen-file payload freshness and owned-target GN checks. Native UI hooks and lifecycle corrections remain open. Review confirmed that Profile destruction could discard a newer coalesced snapshot. The fix queues owned dirty state on a shared shutdown-blocking runner; native scheduling scenarios remain unrun. No full Arc parity or release-readiness claim follows from these candidates.

## 2026-10-25: Command palette history and appearance

- MA-007.b2: combined bounded current-window tab matches with Chromium Profile history, using distinct switch/open actions. History stays local, stale queries are cancelled and private windows skip regular history. Rust was unchanged.
- Fixed mixed light controls and hardcoded dark surfaces. Palette colors now use browser roles, selected labels disable automatic recoloring, and material tint follows effective Profile colors rather than the system theme alone. Restored themed sidebar address backgrounds. Prototype new windows start at `about:blank`.
- Native build, frame/browser-command GN checks and five-source patch reproduction passed. A typed local history title reopened the actual fixture URL in one new tab. Explicit light Profile appearance was inspected on a dark system; a private dark palette showed no normal-history entries. Cmd+N produced one blank target. Captures contain invented data and remain private.
- An initial HoverButton paint failed the opaque-layer subpixel check; disabled subpixel rendering for those labels. Full autocomplete, cached favicons and live appearance switching remain open. Sidebar shortcuts, current-tab palette editing, motion and release recovery are still pending.

## 2026-10-25: Basic native New Tab palette

- MA-007.b1: routed interactive New Tab actions to a centered Views palette while retaining programmatic tab creation. Enter uses Chromium classification/navigation or activates a matching existing tab. Input and tab matching are bounded; Rust workspace rules are unchanged.
- Native build and browser-view GN header checks passed. On macOS, Cmd+T opened the palette without creating a page; submitting a local fixture URL added exactly one tab. Ctrl+T/Escape preserved page IDs and URLs. Selecting an existing fixture tab added no duplicate and restored page focus. Captured the actual window and a native open/close recording outside the repository.
- An attempted parent-view hide violated a layout visibility check. Kept the layout parent visible and hid native hosts instead, preventing clicks on invisible pages. The resulting crash also exposed a missing app-menu anchor in the existing recovery bubble. A fresh disposable profile allowed the palette checks to continue; recovery remains blocked.
- Packaged the seven-file palette delta separately from concurrent rounding work. Current-tab Cmd+L, sidebar hide/show and full autocomplete remain pending. Motion is recorded as a baseline; timing, reduced motion and full visual parity are unverified.

## 2026-10-25: Compact monthly blog rows

- Grouped previews by muted month/year labels. Moved each full date below its title and kept the complete abstract in the same text column beside the thumbnail, with CSS single-line ellipsis.
- Nine unit checks passed, including multi-post month grouping and date/title ordering. Retained browser checks passed across both themes and five blog widths, checking alignment, truncation and month-label contrast. The first light gray measured 4.47:1 and failed; the corrected gray measures 4.88:1 against the plain page surface, with dark mode at 5.99:1.
- Published `6019c32` to the artifact branch. Native work remains separate; Safari/Firefox remain unmeasured.

## 2026-10-25: Image-led post previews and Arc target reference

- Removed the visible blog introduction and retained a visually hidden heading for accessibility. Listings use a small, uncropped WebP thumbnail beside the title, derived from the first Markdown image.
- Post Open Graph/Twitter image URLs, alt text, MIME type and dimensions now follow that first image; structured article data uses the same image. Image-free posts retain the default brand card. Added coverage for first-image ordering, encoded filenames, fallback previews and thumbnail removal when unpublishing.
- Replaced the initial blog screenshot with the specifically approved Arc reference and labelled it as the target appearance, separate from the myArc prototype. Recorded provenance and ownership limits in attribution.
- Eight unit checks and retained homepage/blog browser checks passed, including two themes, mobile widths, metadata/image agreement and non-overlapping thumbnails. Published `0ec3936`; normal TLS read-back matches the index, article, original reference and 5,526-byte thumbnail. Social-platform rendering/cache refresh and Safari/Firefox remain unmeasured.

## 2026-10-25: Markdown progress journal and first public post

- Added build-time CommonMark/YAML posts with explicit publication approval, local validated screenshots, a date-sorted index and per-post canonical/social metadata. Homepage and blog share the existing design/theme shell. Generated output excludes drafts and unreferenced files.
- Added a checked-in artifact-only publisher. It preserves domain/HTTPS settings and unrelated root artifacts, updates without force and replaces the generated blog subtree so unpublished posts and removed images disappear. The current post was explicitly approved for publication.
- Seven real parser/filesystem/publisher unit checks passed, including publish-to-draft removal, unsafe HTML/link handling and image path boundaries. Retained homepage checks passed; blog index/article checks passed at five widths in both themes, with keyboard focus and reduced motion. A first run caught hidden mobile navigation caused by selector specificity; corrected the selector and retained the visibility check.
- Published `5e8d269` to the Pages artifact branch. Normal HTTPS read-back matches the homepage, blog index, first article and screenshot byte-for-byte. Four cited public source URLs returned anonymous HTTP 200. Safari/Firefox, screen readers and social cache refresh remain unmeasured. Native-browser work stayed outside this slice.

## 2026-10-25: Native header and New Tab control refinement

- Found stock horizontal padding shrinking the custom sidebar header and overlapping its controls. Removed that padding for the prototype, centered the navigation buttons and reused the native dock-to-left glyph. Kept the existing navigation callbacks and security indicators.
- Matched the unfocused address surface to the dark reference palette while retaining high-contrast colors. Exposed the native New Tab label, removed its persistent tile background and placed it above the tab list. A native mouse click added exactly one page; Cmd+L navigation still passed.
- Native build passed after correcting an empty-border API argument. Refreshed the private native capture and optional patch. Full sidebar structure, centered command bar and other window states remain open; visual parity is not accepted.

## 2026-10-25: Native material and sidebar navigation iteration

- Current priority changed to native Arc visual parity, including the empty content canvas. Import investigation and broad test expansion are deferred; release safety gates remain open. [MA-007.a](../plan/EXECUTION.md#ma-007a-native-visual-parity-first-state) defines the first review state.
- Stock vertical tabs and the built-in glass flag did not establish the required appearance. The [optional native patch](../browser/experiments/native-shell.md) uses explicit AppKit behind-window material and moves existing browser navigation/address controls into the sidebar. A real page and a transparent blank canvas now run in the same Chromium candidate. No additional host framework was introduced.
- Native builds passed. Mixed web/native toolbar controls initially caused startup crashes; native controls fixed that configuration. Address-field visibility churn then lost keyboard focus; preserving visibility fixed Cmd+L navigation. Captured a real page and return to the translucent empty canvas. A controlled colored backdrop changed the native material across the window.
- The packaged patch reproduced nine baseline sources exactly. Captures and supplied references remain private. Sidebar contents, centered command bar, complete control styling and other window/accessibility states remain unverified or unimplemented; 1:1 parity is open.

## 2026-10-25: Fable redesign, HTTPS recovery and system themes

- Fable/high (`claude-fable-5-1`, frontend-design Skill) replaced the left-label column layout with headings above content, larger body text and a compact progress panel with dedicated artwork space. Header/footer lockups have 1rem ribbon marks. Parent aligned section/subheading sizes, retained ink navigation without underlines, and verified all six public documentation links plus an actual Roadmap click.
- Domain provisioning stalled despite valid DNS. With explicit permission, removed/re-saved the Pages custom domain. GitHub automatically created Delete/Create CNAME maintenance commits; the branch-change guard caught this side effect, and read-back confirmed unchanged landing HTML. Certificate became approved for apex and www. Normal TLS, exact deployed bytes, enforced HTTPS and the www redirect then passed.
- Added canonical/social metadata, a public 1200 × 630 preview, favicon files, WebSite JSON-LD and crawler files. Website runtime assets remain embedded. These changes are published independently of unrelated native-browser source work.
- Added CSS-only system light/dark preference support, native wordmark source switching, theme-aware browser colors and contrast-adjusted dark links. Added Lucide GitHub icons to both CTAs with the retained ISC notice. [Branding system](../design.md) records actual colors, typography, spacing and motion.
- Retained website checks pass at six widths. Fresh light/dark desktop and dark mobile captures were inspected; sampled dark text/link contrast passed AA. Focus/hover and reduced-motion checks passed. Published `aa78b0b` serves exact generated HTML over HTTPS. Safari/Firefox, screen readers and search/social reindexing remain unmeasured.


## 2026-10-24: Native MV3 and runtime smoke checks

- MA-002.d: user installed uBlock Origin Lite `2026.1021.1354` in a newly marked native Chromium profile; native management reported FROM_STORE. Causal enabled/disabled/re-enabled checks and a normal process restart passed. Corrected a measurement mistake: uBlock redirects this XHR to a local noop resource, so a resolved fetch alone proves neither external network access nor blocking. Final response origin/path and a disabled control now determine the result.
- Renderer crash/reload preserved an acknowledged localStorage value with browser PID unchanged. Off-the-record storage was absent initially, isolated from normal state and absent in a fresh private context after disposal. Native Rust control accessibility/Space-key activation also passed on the rebuilt second pin. These checks establish neither production workspace durability nor full cookie/history/private-data isolation.
- Retained [the developer regression client and fixture](../tests/fixtures/browser/README.md), with version/PID/profile-marker preconditions and a changed-PID restart requirement. Positive scenarios ran against the compiled candidate; wrong PID, unmarked profile and unchanged-PID restart were rejected. Source fixture bytes served by the local server were checked against the repository.
- New failure: default-context `Target.createTarget` aborted the browser on a blocking-disallowed assertion in `Profile::CreateProfile` on the UI thread. Its trigger and upstream-versus-patch origin remain unisolated. Existing-tab tests avoid that path while preserving the failed case; thread checks remain enabled. Extension update/removal, native messaging, duplicate ANGLE class warning and distribution/licensing/ownership gates remain open. [Structured runtime evidence](../browser/evidence/runtime.json) keeps private profiles, screenshots and raw logs outside the repository.

## 2026-10-24: First private Markdown export inventory

- MA-003.a: inspected a user-supplied Markdown sample read-only, with source fingerprint unchanged. Space headings, second-level named groups and ordered bare URLs are present. The file provides no producer/schema version, Profile IDs or tab titles. No URLs were opened.
- Created [an independent shape fixture](../tests/fixtures/arc-markdown/README.md) using invented labels and reserved `.test` URLs. Repeated `Favorites` groups retain an ownership ambiguity; equal URLs cannot establish shared Profiles or justify global deduplication.
- Verification covers relative links, fixture structure and separation from private source values. This is evidence preparation, with no production importer or semantic round trip claimed. Follow-up settings screenshot confirms the built-in export action; route is recorded in the fixture inventory. Producing-build binding, favorite-versus-pin scope and nested-folder representation remain open. Personal source bytes, fingerprint and screenshot stay outside the repository.

## 2026-10-24: Panel spacing, shared grain and custom-domain DNS

- Removed the progress panel's empty second column and paragraph/status width caps. Intro and status text now reach the same right inset as the left padding. Reserved lower artwork space without overlapping text.
- Reused the original grain through one embedded CSS variable on both ivory and navy backgrounds. Browser checks passed at 320/390/768/900/1024/1440, including exact inner-edge alignment, grain reuse and artwork separation. Existing offline assets, typography, focus/hover and reduced-motion checks also passed; inspected fresh desktop/tablet/mobile panel captures.
- Published the revised 442,021-byte artifact and domain CNAME to `gh-pages` at `3c229713f27a85d0f7c4b62d4424485ecc9f4f53`. Bound `myarc.dev` in GitHub Pages. The signed-in Cloudflare browser profile resolved the earlier access blocker; a fresh owned tab was needed, and read-only DOM inspection replaced timed-out accessibility snapshots.
- Imported only four apex A records from GitHub's current documentation and `www CNAME bitbonsai.github.io`, DNS-only with TTL 300. The zone initially had zero records. Authoritative/public DNS resolve correctly and the domain returns HTTP 200; certificate issuance and HTTPS enforcement remain pending. A bounded session monitor checks normal TLS, exact deployed bytes and HTTPS activation before reporting readiness.

## 2026-10-24: Chromium build and bounded Rust/native proof

- MA-002.a: unmodified Chromium `7ae71de816ba32868ff9f974c2716f04ebd9b860` built and launched. The successful resumed segment ran 06:40:26–09:03:03 UTC; full fresh-build time and peak memory/storage remain unmeasured. A synthetic local form and separate-user-data-dir localStorage smoke check passed. This establishes neither full Profile isolation nor private-mode safety.
- MA-002.b: added a dependency-free transient Rust workspace with validated rename, Cxx/GN bridge and opt-in native Views control. Six core tests passed under Cargo and GN; seven bridge checks include 1,000 ownership cycles. Accessibility activation and focused Space key changed Everyday to Research and back. Keyboard access required `FocusBehavior::ALWAYS`. The complete workspace model and persistence remain unimplemented.
- MA-002.c: packaged the source/toolbar patch and rebuilt it at the original pin and immediate child `8d208f20744ac6d4ce00cc3f9de6a14a9b8b926d`, without conflicts. Both runs executed six Rust tests and seven C++/Rust checks. The second sync/hooks passed. This PHP test-dependency roll gives limited update-porting evidence; UI rerun on the second rebuild remains pending. External checkout Git identity is separate from this repository's configuration; recorded commits verify both identities as `bitbonsai`.
- Added pinned standalone Rust tooling, core/patch CI and a payload check. Local format, clippy, six unit tests and dependency guard passed; deliberately stale source was rejected by the payload check. Remote workflow execution remains unverified. Canonical source hashes and scope are in [foundation evidence](../browser/evidence/foundation.json).
- Extension installation through Chrome Web Store was observed for uBlock Origin Lite `2026.1021.1354`. Discarded a driver reconnection that selected stock Chrome 151. A later disposable profile lacked extension files while native management reported ENABLED; its dashboard returned `net::ERR_FILE_NOT_FOUND`. Cause is unresolved. Function/restart/update/remove acceptance remains open pending a fresh native-launched profile.
- Licensing/package findings: inventory contains 326 entries and eight unresolved metadata/license errors; generated development credits are a sample page. The ad-hoc component app fails strict bundle verification. Duplicate `ANGLESwapCGLLayer` warnings remain unexplained. No binary is ready for distribution. Owner, backup capacity and recurring budget remain unspecified.
- Arc UI fixture preparation remains incomplete and produced no verified official export. Private media, browsing captures and local test profiles stay outside the repository. M0 remains open; no production browsing, migration, control-service or security gate was closed by these experiments.

## 2026-10-24: Self-contained landing page and GitHub Pages

- Followed the requested `claude -p --model fable --effort high` design route. The trace identified `claude-fable-5-1` and invoked `/frontend-design`; the CLI timed out after writing the template and README. Retained its editorial layout, approved brand assets and truthful development-stage copy.
- Added `site/build.py` to produce one 442,041-byte HTML file with embedded font/images and full OFL/ISC notices. No runtime dependencies, analytics or executable JavaScript. Intro movement is finite; footer hover/focus retains the 40-BPM heart.
- Verification: static resource/link/rem guards and a deliberately invalid external-image self-check passed. Chromium rendered offline with no remote resource requests and decoded Outfit/images. Text clipping, 16/14-pixel body/minimum sizes passed at 320/390/768/1440; actual hover/keyboard focus and reduced motion passed. Inspected fresh desktop/mobile captures and a decoded 6.7-second motion/focus/reduced-motion recording; verified browser helper cleanup. Full-page stills taken during recording repeated viewport tiles, so they were rejected and recaptured outside recording. Native browsing remains outside this evidence.
- Published only the artifact to `gh-pages` at `1f9f608cbdb97f16acf3f67604893a562a003304`; GitHub verified author and committer as `bitbonsai`. Pages source is branch root, HTTPS enforced. Creation returned a non-JSON transport error; read-back confirmed success, avoiding duplicate publication. Preserved source font timestamps after finding conversion made otherwise unchanged builds differ; two successive builds now match byte-for-byte. Public HTTP 200 HTML matched the final local file exactly.
- Cloudflare access is blocked by an invalid stored credential and unauthenticated browser session. `myarc.dev` custom-domain binding, DNS writes and domain-certificate verification remain pending. Existing DNS and concurrent browser/reference work were preserved. Naming/asset rights, Safari/Firefox and screen readers remain unverified.

## 2026-10-24: Baseline comparison and live interaction polish

- Reproduced the custom-A regression: stroked chevron feet ended diagonally above the baseline. Replaced the stroke with a closed outline, matching original Outfit A cap height and the m/r baseline. Added `design/brand/alignment.html` with original/custom wordmarks at identical scale, spacing and guides; curved c overshoot remains unchanged. New geometry checks failed on the old asset and passed after correction.
- Standardized all social cards and wallpapers on `myArc, forever yours.` Shortened the lede to local workspace and optional AI. Replaced the footer status line with a GitHub-linked maker credit and an accessible Lucide outline heart; pinned ISC notice and source hash are retained.
- Removed page divider rules and increased spacing. Primary buttons are blue. Text links are blue at 90% alpha, becoming opaque while their underline moves 2 to 5 pixels over 200 ms. The personal blog supplied observed motion behavior, not copied site code. Footer hover/focus runs a single pulse every 1.5 seconds, 40 BPM; reduced motion disables movement and transitions.
- Fixed stale preview images with content-versioned URLs after rendering. Verified the actual internal browser's current DOM and hover state, in addition to the retained headless checks: 90% to opaque link color, 2-to-5-pixel underline, no layout shift and 1.5-second heartbeat. Static checks cover ten HTML pages, exact original-A alignment, icon path provenance and image revisions. Layout/type checks passed at 1440/728/390 widths; source hashes and document links passed. These remain branding checks, not native browser tests.

## 2026-10-24: Outfit, custom A and ivory grain

- Selected Outfit for the main brand page, wordmarks and six artwork compositions. Added a short ethos and four design principles. These express product intent, not implemented browser capabilities.
- Promoted the original barless-A drawing into shared, font-independent vector wordmarks. Other letters outline Outfit ExtraBold 800. Dark/light SVGs, UI marks and raster lockups share the same generated geometry; normal copy retains Outfit's standard A. Supplied ribbon PNGs remain unchanged.
- Inspected tiled noise treatment in archived home/developer pages, then generated a fresh seeded monochrome tile behind warm ivory. No archived texture or blue page background was introduced.
- Verification: regenerated six compositions and two lockups; checked shared five-path SVG geometry, font independence, grain dimensions/alpha/variation, nine HTML pages, original asset hashes and document links. Actual Outfit loading, 16-pixel body, 14-pixel minimum, image proportions and overflow passed at 1440/728/390 widths. Fresh desktop/mobile screenshots were inspected; browser helpers were closed.
- Tooling boundary: CairoSVG was installed but could not resolve its Cairo library. Reused installed fontTools and librsvg (`rsvg-convert`) for outlined wordmark generation/rasterization; no packages were installed. Native browser implementation and branding legal clearance remain outside this work.

## 2026-10-24: Typeface trial

- Added a separate `design/brand/type-study.html` with native radio controls for Outfit, Sora and current Manrope, plus an original SVG barless-A experiment. The supplied wordmark's exact typeface remains unidentified; Outfit ExtraBold is a visual candidate, not a verified match.
- Added unchanged OFL font binaries with license notices, source links and SHA-256 hashes. Main brand page, generated artwork and ZIP were preserved pending selection.
- Verification: all three fonts loaded and switched correctly at 1440/390 widths, with a 14-pixel minimum and no horizontal overflow. Native arrow-key selection passed; fresh desktop/mobile screenshots were inspected. Static branding checks cover nine HTML pages; document links and font hashes passed. No production browser behavior was tested.

## 2026-10-24: M0 motion study and pinned foundation build

- Starting source content: archived-reference baseline `6de61de`; concurrent README/brand work and GitHub attribution corrections were preserved.
- MA-001.a: implemented `design/onboarding/` as an original, dependency-free synthetic motion study. Six scenes include empty/error/1,000-pin and reduced-motion variants, local pin/Space/search practice, Back/Skip and replay. Study state is disposable, with no importer, persistent workspace or live page claimed.
- Evidence: six semantic Node tests and 40 agent-browser checks passed. Fresh source/artifact hashes, normal/reduced recordings and desktop/mobile captures are retained under `design/onboarding/evidence/`. Decoded recordings were inspected. No page errors or lingering browser helpers remained. Native timing, VoiceOver and formative tasks were not measured.
- Failed checks retained as lessons: agent-browser eval needs an async wrapper for awaited animation completion; reloading during capture produced a truncated video, so capture now replays in-place and checks duration/frame count; search input swallowed Escape, fixed with explicit dialog dismissal and focus restoration.
- MA-002.a: pinned Chromium `7ae71de816ba32868ff9f974c2716f04ebd9b860` and depot_tools, fetched outside the repository, and exercised dependency sync, hooks and GN generation. Fixed missing Git LFS, explicit depot_tools Python bootstrap, and a partial LiteRT checkout left by the first sync failure. Mirror LFS returned HTTP 405; scoped skip-smudge restored the exact clean source tree, with no generated build references to unused LiteRT prebuilt libraries. Global Git LFS configuration stayed unchanged.
- Build attempt two completed 1,155 steps before missing Metal Toolchain stopped ANGLE. Installed Apple's component `27A266a` and resumed incremental `chrome` build with eight jobs. Build/launch, extension lifecycle, Rust/native bridge, second-revision patch, isolation/recovery and signed distribution remain unverified. [Foundation recipe/run record](../browser/README.md) documents reproducible commands and boundaries.
- MA-003: no consented official export was supplied or private profile accessed. Coverage remains unknown; study fixtures are not Arc-format evidence. M0 stays open pending build acceptance, export evidence and named maintenance capacity.

## 2026-10-24 — Default-on blocking requirement

- Decision: ad and tracker blocking is P0 browser scope, enabled from the first page load in fresh Profiles, with Profile-scoped per-site exceptions and restore controls.
- Outcome: updated README commitments, product scope and PRV-01 evidence requirements. Added R18 for the Arc security FAQ; its 2024 effective date does not prove current-build defaults.
- Boundary: no blocking engine or extension selected or implemented. Require selected-Chromium compatibility, filter updates/failure handling and synthetic blocked/allowed/site-recovery evidence before release. CLI/MCP headed/headless support remains a proposal pending confirmation.
- Verification: relative documentation links and whitespace checks only; no browser blocking tests exist yet.

## 2026-10-24 — myArc copy and readable type

- Source: README branding, `myArc. Arc, forever yours.` Consulted `claude -p --model fable --effort medium`; the response identified `claude-fable-5-1`. Advice was read-only.
- Outcome: removed competing slogans, numbered labels and repeated explanation. Applied myArc casing to the board, preview, six compositions and both raster lockups. Final board headline is `myArc, forever yours.` with one README-based project lede. Original logo files and technical filenames stayed unchanged.
- Verification: regenerated artwork and inspected fresh desktop/mobile screenshots. Actual body/minimum font sizes were 16/14 pixels at 1440/390 widths. Static checks enforce rem font sizes, minimum size, casing and old-copy removal across eight HTML pages. Image dimensions/proportions, local links, documentation links and whitespace checks passed.
- Preview boundary: loopback-only local server for internal preview, no public deployment. Internal tab loaded the correct page title, but reported its pane hidden; internal screenshot capture timed out. Separate agent-browser captures supplied visual inspection. Asset-rights clearance remains outside this change.

## 2026-10-24 — Standalone brand material

- Starting revision: `9f7bec2`; browser implementation remains unstarted.
- Outcome: added `design/brand/` with a responsive identity board, unchanged user-supplied matte/glass PNGs, six editable HTML compositions, two transparent lockups and eight raster mark sizes. Used OFL-licensed Manrope instead of archived reference fonts.
- Evidence: `bash design/brand/export.sh` captured six PNG compositions after font/image decoding and checked the board at 1440/390 pixels for horizontal overflow and distorted image ratios. `python3 design/brand/verify.py` checks eight HTML pages, composition dimensions/content and ten transparent derivatives. Original attachment byte comparisons passed; desktop/mobile previews had no console errors. ZIP integrity and repository documentation links passed. These are branding checks, not application tests.
- Boundaries: no vector redraw, favicon approval, print-production certification, legal clearance or public deployment. Naming and source-logo rights remain open. Existing research edits were left untouched.

## 2026-10-24 — Product principles and Artcraft boundary

- Starting revision: `1ccb972`; no application implementation.
- Outcome: recorded open development, native Chromium plus Rust ownership, Arc-familiar interaction, local workspace ownership, and agent-ready control as design commitments rather than shipped claims. Excluded a browser-inside-browser platform from current scope and added the project-site visual direction.
- Artcraft boundary: retained useful public engineering methods without assuming its open-source work exists to drive traffic or that its repositories have a verified relationship to a commercial AI service. myarc remains useful without paid AI.
- Verification scope: documentation consistency, relative links, and whitespace only. No browser, importer, project site, or AI service was implemented.

## 2026-10-24 — CLI and MCP become launch scope

- Starting revision: `b799568`; no application implementation.
- Outcome: promoted browser control to P0, specified both adapters over one control service, added MA-011 and BC-004, and separated external-agent control from optional built-in/hosted AI.
- Required proof: actual page observation/actions through both interfaces, one external MCP client, scoped grants, stale/error handling and human Stop control. These remain implementation gates, not completed tests.
- Compatibility boundary: local stdio clients first; cloud MCP hosts require a separate authorized bridge. No unrestricted public debugging endpoint is introduced.
- Verification scope: documentation consistency and relative links. Next implementation work must prove a narrow live-browser control adapter alongside the core, without waiting for a paid AI service.

## 2026-10-24 — Planning workflow audit

- Starting revision: `60c00aa`; application implementation had not started.
- Outcome: compared public Artcraft readiness, development and scorecard practices; added behavior-contract seeds and working rules; separated standalone Rust work from browser-build feasibility; specified evidence freshness, no-op-control checks and regression retention.
- Verification scope: documentation link/consistency checks and workflow configuration review. Browser tests, Rust checks, benchmarks, visual recordings and generated scorecards remain unavailable.
- Source limitation: raw retrieval of some reference documents failed; the public GitHub file interface supplied the development guide and scorecard. No private shared standards were accessed.
- Next action: execute a bounded MA-001, MA-002 or MA-003 slice with actual motion, build or export evidence. For core work, follow the behavior contracts and corrected MA-004 dependencies. Avoid adding another broad planning layer before gathering that evidence.
