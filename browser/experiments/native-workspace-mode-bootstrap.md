# Runtime workspace mode bootstrap

Slice: MA-007.c. Candidate repair, native acceptance pending. Workspace renderer remains default OFF.

## Reproduced defect

The [two-Profile run](../evidence/workspace-native-profiles.json) created a second regular Profile through native Profiles > Add Profile. Its initial mode was horizontal. View > Show tabs vertically changed the real preference in the same window, but left the stock toolbar/vertical strip and never created that Profile's workspace metadata. Closing and reopening the Profile constructed the myarc controls. That workaround is the defect addressed here.

`BrowserView::AddedToWidget()` performed toolbar reparenting and workspace construction only when the initial mode was vertical. The existing `OnVerticalTabStripModeChanged()` callback rebuilt upstream tab-strip views but never performed those myarc steps.

## Narrow repair

[0019-native-workspace-mode-bootstrap.patch](../patches/0019-native-workspace-mode-bootstrap.patch) adds one guarded `UpdateMyarcTabStripMode()` helper, called after widget initialization and from the existing upstream mode callback, after native strip reset/initialization and before frame notification/layout invalidation. Early notifications defer myarc setup until widget initialization completes. Theme-driven widget reattachment retains the existing initialization guard.

- First vertical enable moves the existing collapse button into the toolbar and the existing toolbar into the vertical header. Horizontal mode transfers both children back. Each `Take` method clears its borrowed pointer before removing the owned child. The header's collapse-button reference remains borrowed and valid in either parent.
- First vertical enable creates one adapter, SidebarView and subscription for an opted-in normal window. Horizontal mode retains them. Further enables reuse the same bindings, workspace/window IDs and subscription. Upstream tab-strip reconstruction still owns its own native views.
- Sidebar toolbar geometry now uses Views' `ApplyLayout` rather than externally changing child visibility. A small FlexLayout subclass exposes that existing protected operation. Upstream requested visibility stays intact, including changes received while controls are hidden. Navigation-button insets return to platform defaults on horizontal exit; the normal layout and toolbar background resume.
- One BrowserView-owned popup anchor exists in both modes. It survives sidebar hide and mode changes. Existing 0016 anchoring and 0018 recovery-choice holds are unchanged.
- Reparenting clears focus/stored focus only when the moved or hidden subtree held it. A weak, non-nestable task waits for the new layout, checks current mode/window activation and avoids replacing another chosen focus target. Hidden toolbar uses existing browser focus restoration.

The helper never writes vertical preferences or adds flags. Without `myarc-visual-parity`, it returns without changing upstream ownership. Visual-only mode reparents controls without creating a workspace adapter. Profile and off-the-record service ownership are unchanged; no new global state, Rust rules, schema or dependency changes were introduced.

Existing workspace commands and canvas behavior remain active after horizontal exit once initialized. This provisionally preserves current cross-Space behavior; this patch defines no new horizontal workspace policy. NTP and upstream AI Mode policy are outside this repair.

## Packaging and checks

Apply 0019 once after the frozen optional stack through 0018, with the current 0001 canonical payload. The handed-off external source already contains 0019; do not apply twice. `upstream.json` still lists only the foundation patch, so optional rendering remains off.

Eight upstream files change: BrowserView implementation/header, ToolbarView implementation/header, vertical-header implementation/header, the existing recovery interactive fixture, and its BUILD dependencies. All 16 installed owned files match canonical sources exactly. 0001 and every pre-existing optional patch, including 0012/0016/0017/0018, remain byte-identical. No payload refresh was needed.

Private proof directory: `/tmp/myarc-m0-runtime/mode19/`. It contains the 78-file source baseline, installed and canonical manifests, package script, strict indexed reverse/replay logs, syntax command/log, GN checks, build logs/exits/timestamps and process snapshots. Reverse/replay restores every baseline hash and reproduces every installed hash; 70 unrelated files remain byte-identical.

The existing `SessionCrashedBubbleMyarcViewTest` gains `RuntimeModesKeepWorkspaceAndRecoveryAnchor` for both initial collapse states. It starts horizontal, enables in the same window with a pending recovery choice, then checks readiness after dismissal, real adapter Space creation/switching, three bidirectional cycles, toolbar ownership/visibility, stable page pointer/bindings/IDs and unchanged allocation counters. The flag-off fixture gains `RuntimeModesLeaveFlagOffToolbarNative`. These native interactive cases are source-checked only, UNRUN.

Verification on 2026-10-25:

- Full `autoninja -C out/MyarcM0 -j8 chrome` passed after compiling/relinking the repair. Final focus-adjusted build ran 23:45:54Z to 23:46:15Z, exit 0. The preceding successful build compiled the broader header-dependent closure. No build remains running.
- Existing compiled `workspace_lifecycle_check` passed registration, metadata, startup/restore and recovery-choice checks. Its target was already current; no new helper tests or real Profiles were constructed.
- Final interactive fixture passed the real Chromium clang command with `-fsyntax-only`, including enabled compiler checks. This neither linked nor executed interactive tests. GN generation and dependency checks passed for both the production BrowserView target and interactive fixture target.
- Changed-line clang-format, GN formatting, document links, patch freshness and whitespace checks passed. All 16 canonical files are unchanged; Cargo was not rerun.
- 0019 SHA-256: `099f62a0c22d3a116ab9117c0c782ed13557782df9ad439ab8d23b1fa0f85afe`.

Initial checks caught a missing `FocusLocation` argument, fixture accessors/private entry-point misuse, and a missing direct GN header dependency. Those were corrected; failed logs remain private. No browser app, desktop automation, actual profile or fixture server was launched or modified by this slice.

## Parent runtime acceptance

Use a fresh marked disposable profile with actual vertical preference false and explicit experiment flags. Launch remains the parent's action.

1. Open real synthetic pages, then use native View > Show tabs vertically in that same window. Confirm myarc address/toggle and Workspace controls appear, existing pages stay alive, and actual Space creation/switching works.
2. Return horizontal. Confirm usable upstream toolbar, actual pages and persisted workspace identity. Re-enable repeatedly; compare IDs, selection, bindings, page/model/window counts and native keyboard focus.
3. Repeat across another regular Profile and native private browsing. Verify isolation and flag-off behavior. Exercise a pending recovery popup, hidden sidebar, open dialog/palette, rapid toggles and close/shutdown while focus restoration is queued.

Fullscreen/immersive reparenting, native accessibility interaction, recovery Restore, close/shutdown races, Profile/OTR isolation and disk/crash durability are unmeasured by this implementation slice. Compiling and linking cannot establish those outcomes.
