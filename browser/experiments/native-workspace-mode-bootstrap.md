# Runtime workspace mode bootstrap

Slice: MA-007.c. Warm runtime switches passed after a readiness correction; cold horizontal restore still loses identity. Workspace renderer remains default OFF.

## Reproduced defect

The [two-Profile run](../evidence/workspace-native-profiles.json) created a second regular Profile through native Profiles > Add Profile. Its initial mode was horizontal. View > Show tabs vertically changed the real preference in the same window, but left the stock toolbar/vertical strip and never created that Profile's workspace metadata. Closing and reopening the Profile constructed the myarc controls. That workaround is the defect addressed here.

`BrowserView::AddedToWidget()` performed toolbar reparenting and workspace construction only when the initial mode was vertical. The existing `OnVerticalTabStripModeChanged()` callback rebuilt upstream tab-strip views but never performed those myarc steps.

## Narrow repair

[0019-native-workspace-mode-bootstrap.patch](../patches/legacy/0019-native-workspace-mode-bootstrap.patch) adds one guarded `UpdateMyarcTabStripMode()` helper, called after widget initialization and from the existing upstream mode callback, after native strip reset/initialization and before frame notification/layout invalidation. Early notifications defer myarc setup until widget initialization completes. Theme-driven widget reattachment retains the existing initialization guard.

- First vertical enable moves the existing collapse button into the toolbar and the existing toolbar into the vertical header. Horizontal mode transfers both children back. Each `Take` method clears its borrowed pointer before removing the owned child. The header's collapse-button reference remains borrowed and valid in either parent.
- First vertical enable creates one adapter, SidebarView and subscription for an opted-in normal window. A native-ready notification after setup covers late registration; existing pending startup/recovery/restorer counts still block its weak non-nestable completion. Horizontal mode retains them. Further enables reuse the same bindings, workspace/window IDs and subscription. Upstream tab-strip reconstruction still owns its own native views.
- Sidebar toolbar geometry now uses Views' `ApplyLayout` rather than externally changing child visibility. A small FlexLayout subclass exposes that existing protected operation. Upstream requested visibility stays intact, including changes received while controls are hidden. Navigation-button insets return to platform defaults on horizontal exit; the normal layout and toolbar background resume.
- One BrowserView-owned popup anchor exists in both modes. It survives sidebar hide and mode changes. Existing 0016 anchoring and 0018 recovery-choice holds are unchanged.
- Reparenting clears focus/stored focus only when the moved or hidden subtree held it. A weak, non-nestable task waits for the new layout, checks current mode/window activation and avoids replacing another chosen focus target. Hidden toolbar uses existing browser focus restoration.

The helper never writes vertical preferences or adds flags. Without `myarc-visual-parity`, it returns without changing upstream ownership. Visual-only mode reparents controls without creating a workspace adapter. Profile and off-the-record service ownership are unchanged; no new global state, Rust rules, schema or dependency changes were introduced.

Existing workspace commands and canvas behavior remain active after horizontal exit once initialized. This provisionally preserves current cross-Space behavior; this patch defines no new horizontal workspace policy. NTP and upstream AI Mode policy are outside this repair.

## Packaging and checks

Apply 0019 once after the frozen optional stack through 0018, with the current 0001 canonical payload. The handed-off external source already contains 0019; do not apply twice. `upstream.json` still lists only the foundation patch, so optional rendering remains off.

Nine upstream files change, including the direct BrowserView lifecycle dependency in frame/BUILD.gn: BrowserView implementation/header, ToolbarView implementation/header, vertical-header implementation/header, the existing recovery interactive fixture, and its BUILD dependencies. All 16 installed owned files match canonical sources exactly. 0001 and every pre-existing optional patch, including 0012/0016/0017/0018, remain byte-identical. No payload refresh was needed.

Private proof directory: `/tmp/myarc-m0-runtime/mode19/`. It contains the 78-file source baseline, installed and canonical manifests, package script, strict indexed reverse/replay logs, syntax command/log, GN checks, build logs/exits/timestamps and process snapshots. Reverse/replay restores every baseline hash and reproduces every installed hash; 69 unrelated files remain byte-identical in the corrected revision.

The existing `SessionCrashedBubbleMyarcViewTest` gains `RuntimeModesKeepWorkspaceAndRecoveryAnchor` for both initial collapse states. It starts horizontal, enables in the same window with a pending recovery choice, then checks readiness after dismissal, real adapter Space creation/switching, three bidirectional cycles, toolbar ownership/visibility, stable page pointer/bindings/IDs and unchanged allocation counters. The flag-off fixture gains `RuntimeModesLeaveFlagOffToolbarNative`. These native interactive cases are source-checked only, UNRUN.

Verification on 2026-10-25:

- Full `autoninja -C out/MyarcM0 -j8 chrome` passed after compiling/relinking the repair. Final focus-adjusted build ran 23:45:54Z to 23:46:15Z, exit 0. The preceding successful build compiled the broader header-dependent closure. No build remains running.
- Existing compiled `workspace_lifecycle_check` passed registration, metadata, startup/restore and recovery-choice checks. Its target was already current; no new helper tests or real Profiles were constructed.
- Final interactive fixture passed the real Chromium clang command with `-fsyntax-only`, including enabled compiler checks. This neither linked nor executed interactive tests. GN generation and dependency checks passed for both the production BrowserView target and interactive fixture target.
- Changed-line clang-format, GN formatting, document links, patch freshness and whitespace checks passed. All 16 canonical files are unchanged; Cargo was not rerun.
- Initial 0019 SHA-256: `099f62a0c22d3a116ab9117c0c782ed13557782df9ad439ab8d23b1fa0f85afe`, superseded by the parent readiness correction. Current SHA-256: `d7bb765e86a2cbee4a70b00ef39885e0510cd242d01f2d95333cbff93933bd5d`.

Initial checks caught a missing `FocusLocation` argument, fixture accessors/private entry-point misuse, and a missing direct GN header dependency. Those were corrected; failed logs remain private. The implementation delegate launched no browser app, desktop automation or fixture server, and modified no actual profile. Parent native attempts are recorded below.

## Parent native results and correction

First actual late enable constructed the toolbar and SidebarView but stayed unavailable, with no workspace file. The late bridge had missed startup completion and retained `native_ready_ = false`. Parent added the native-ready setup notification described above, its direct GN dependency, and `FirstRuntimeModeWithoutRecoveryBecomesReady` in the existing fixture. Counts and native `IsRestoring` checks remain intact. The new native case is source-checked only, UNRUN.

The corrected full app build, native-fixture clang syntax, production GN dependency check, 78-file strict reverse/replay and canonical 16-file match passed. Private corrected proof is `/tmp/myarc-m0-runtime/mode19-r1/`. An initial wrong GN label matched zero targets and was rejected; the corrected label and missing direct dependency were checked before build success.

Fresh native horizontal startup then enabled full workspace controls in the same window. Actual Space creation/switching, three bidirectional cycles, horizontal address focus and hidden-sidebar exit worked. Page target IDs/time origins and persisted IDs/selection/allocation counters stayed exact. The driver corrected its scratch assumption about hidden-state preservation across mode changes rather than changing product code. Original and corrected private runs are `mode19-acceptance-20261025T235214Z` and `mode19-r1-acceptance-20261026T000021Z` under `/tmp/myarc-m0-runtime/`.

A further clean horizontal restart followed by late vertical enable **failed** strict identity verification: original Temporary ID2 disappeared, Favorite ID1 gained a duplicate ID3, and the new Temporary ID4 belonged to the wrong Space. The verifier rejected three persisted items; no metadata repair or baseline change was made. The app-only reduced-motion flag reported true during this failing case, which does not establish full reduced-motion acceptance. [Structured results](../evidence/workspace-mode-bootstrap.json) retain the boundary. Restore bindings must survive independently of late UI activation before this slice can close.

## Parent runtime acceptance

Use a fresh marked disposable profile with actual vertical preference false and explicit experiment flags. Launch remains the parent's action.

1. Open real synthetic pages, then use native View > Show tabs vertically in that same window. Confirm myarc address/toggle and Workspace controls appear, existing pages stay alive, and actual Space creation/switching works.
2. Return horizontal. Confirm usable upstream toolbar, actual pages and persisted workspace identity. Re-enable repeatedly; compare IDs, selection, bindings, page/model/window counts and native keyboard focus.
3. Repeat across another regular Profile and native private browsing. Verify isolation and flag-off behavior. Exercise a pending recovery popup, hidden sidebar, open dialog/palette, rapid toggles and close/shutdown while focus restoration is queued.

Fullscreen/immersive reparenting, native accessibility interaction, recovery Restore, close/shutdown races, Profile/OTR isolation and disk/crash durability are unmeasured by this implementation slice. Compiling and linking cannot establish those outcomes.
