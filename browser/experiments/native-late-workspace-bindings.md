# Native restore bindings before sidebar presentation

MA-007.c bounded follow-up to [0019](native-workspace-mode-bootstrap.md), based on parent `0dfa2f6`. Optional [0020](../patches/0020-native-late-workspace-bindings.patch) changes consumer lifetime. Full build and source checks pass. Actual clean native restart acceptance remains pending with the parent; workspace renderer stays default OFF.

## Failure and boundary

The [parent's cold failure](../evidence/workspace-mode-bootstrap.json) followed a real horizontal choice, graceful quit and changed-PID `--restore-last-session` launch without URLs. Later enabling vertical tabs retained both actual pages but lost workspace organization: Favorite 1 remained dormant alongside duplicate Favorite 3, Temporary 2 disappeared, and replacement Temporary 4 belonged to Everyday instead of Reading. The strict verifier rejected three persisted items. The failed profile remains preserved; this change does not reconstruct its identities. This failure is separate from recent localStorage batching under process kill.

Previously `BrowserView::UpdateMyarcTabStripMode()` created its first `BrowserWorkspaceAdapter` only while vertical. Native `SessionRestoreImpl::CreateRestoredBrowser()` delivers window metadata after `CreateBrowserWindow()` returns. `chrome::CreateRestoredTab()` delivers tab metadata before insertion. Both use `WorkspaceLifecycle::Get(profile)`; its Profile-owned bridge then finds the exact window's adapter. Horizontal startup had no consumer, even if another window had registered the Profile bridge. Native hints were dropped. Later reconciliation saw unbound pages, pruned the original Temporary, then migrated the native pin as a new Favorite and allocated another Temporary.

## Narrow repair and ownership

`BrowserView` now constructs the adapter and its weak subscription at the end of widget initialization for each normal window with both experimental switches, regardless of tab-strip presentation. Construction completes before the window returns to native restore callers. The existing Profile-owned bridge can consume the native window/item/Space hints immediately. Validated tab bindings stay on their actual WebContents; window hints stay on that adapter. There is no new cache, URL matching or identity inference.

Toolbar reparenting and first `SidebarView` creation remain vertical-only. Before any sidebar exists, adapter notifications update the existing workspace canvas/command state. This preserves the provisional global cross-Space behavior while horizontal. Once created, the sidebar and adapter survive repeated presentation switches. BrowserView teardown still invalidates weak callbacks, removes the subscription/sidebar and destroys the adapter. The Profile owns the shared runtime/service; OTR and other ephemeral storage rules are unchanged.

The explicit native-ready notification from corrected 0019 remains after adapter setup. Startup, recovery-choice and restorer counts, actual `SessionRestore::IsRestoring()` guards, weak non-nestable readiness, shutdown/cancellation and reconciliation ordering are untouched. Constructing a consumer does not release these barriers or authorize pruning before hints arrive. Native preferences, ordinary flag-off windows and visual-only mode retain their existing gates. No Rust, schema, dependency version or core URL policy changes.

## Regression coverage

The existing `SessionCrashedBubbleMyarcViewTest` gains `HorizontalRestoreHintsSurviveLateVerticalPresentation`, parameterized by both initial collapse states. It captures real adapter-generated metadata for two Spaces, a Favorite and a Temporary, closes their source window under a counted restore hold, creates a horizontal replacement, and passes the captured hints through `RestoreWindow` and native `chrome::AddRestoredTab`. Both pages deliberately use `about:blank`; identity must come from the hints.

The case checks bindings before vertical presentation, rejects an early native-ready notification while the hold remains, and compares the entire encoded workspace checkpoint after activation across three bidirectional cycles. The comparison includes original IDs, owners, placements, canonical URL, window selection and allocation counters. Page pointers, native metadata and navigation entries are also checked. The counted hold models insertion; it is not an actual disk/PID session restore.

Existing initial-horizontal assertions now require the adapter to exist and preserve its bindings across first activation. Existing warm page/binding/mode/recovery checks remain. The flag-off fixture is unchanged. All these native interactive cases remain **UNRUN**; only their real-header syntax and GN dependencies were checked here.

## Packaging and checks

Apply 0020 once after canonical16 and the frozen optional stack through corrected 0019. The installed external source already includes it. Existing 0001 and optional patches, including 0012/0016/0017/0018/corrected0019, remain byte-identical. All 16 installed owned payload files match this worktree, so no foundation refresh was needed. `upstream.json` remains unchanged.

Only three upstream files change: `frame/browser_view.cc`, the existing recovery interactive fixture and its `views/BUILD.gn` dependencies on native tab restore/sessions. No additional browser-test graph or framework was introduced.

Private proof: `/tmp/myarc-m0-runtime/mode20/`. It contains the verified corrected0019 baseline, 78-file installed/baseline manifests, canonical16 manifest, prior patch hashes, inspected packaging/syntax scripts, strict indexed reverse/replay, GN results, build logs/exits/timestamps, artifact mtimes and executable-comm snapshots. Replay restores all baseline hashes and reproduces installed hashes; 75 unrelated files stay identical.

Verification on 2026-10-26:

- `autoninja -C out/MyarcM0 -j8 chrome workspace_lifecycle_check` passed, 00:17:42Z to 00:18:03Z. BrowserView object and component `libchrome_dll.dylib` have fresh mtimes. The app launcher and already-current helper remain older, as expected for this component build.
- `out/MyarcM0/workspace_lifecycle_check` executed and passed registration, metadata and startup/restore/recovery-choice checks. It uses opaque tokens, not actual Profiles.
- Interactive fixture passed its real Chromium clang command with `-fsyntax-only`; syntax log is empty. This neither linked nor executed interactive tests.
- GN generation and dependency checks passed for the actual production BrowserView and interactive fixture targets; source-list checks confirmed each target owned the intended file.
- Standalone Rust formatting, bridge rustfmt, strict workspace clippy and `cargo test --workspace --locked` passed: 6 unit and 26 semantic tests executed. No Rust source changed; these tests do not exercise native consumer lifetime.
- Changed-line clang-format, GN formatting, strict patch replay, foundation payload freshness, document links and whitespace checks passed.

0020 SHA-256: `52e86645175697aadbdb9351525eb8058fc6c7890ed1efd332bd75a577487636`.

## Parent handoff

External source/build: `/Users/mwolff/.cache/myarc-m0/chromium/src`, output `out/MyarcM0`. Executable comm paths were checked before linking. No browser app, desktop automation or server was launched. Actual profiles, OS settings, permissions, services and singleton locks were untouched. Source/build ownership is relinquished at handoff, with no build left running.

Use a fresh explicitly experimental disposable profile. Create a two-Space Favorite/Temporary checkpoint, choose horizontal through native controls, quit gracefully, then perform actual native restore with no URL arguments under a changed PID. Enable vertical tabs in that same window. Compare strict unchanged metadata, including IDs/owners/placement/canonical URL/selection, and confirm actual pages remain. Repeat warm switches, pending recovery and regular-Profile/OTR/flag-off checks. Preserve the failed profile and its original checkpoint.

Compilation cannot establish native acceptance. Actual clean/crash restart, Profile/OTR isolation, reduced motion, shutdown races and durability remain with the parent. The failed cold run's app-only reduced-motion flag was true; this implementation launched no motion test and changes no OS preference.
