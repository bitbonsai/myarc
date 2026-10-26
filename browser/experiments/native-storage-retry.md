# Native workspace storage retry

## Observed baseline through0022

Fresh marked disposable Profile, real fixture Favorite and known-written metadata. The original file was retained byte-for-byte in an owned backup. Replacing only its metadata filename with an empty directory caused a real atomic-write failure after native Create Space. The new Space remained in memory and an inspected native warning asked the user to fix disk space/permissions and retry saving. A native retry control was absent.

Clean quit/relaunch against the same faulty path retained the original backup and showed the real native page with workspace fallback. Workspace mutation controls were absent. The visible warning asked to retry loading, again without a native retry control. The service already implemented `RetryLoad()`/`RetrySave()`; the baseline UI did not connect them. [Structured baseline](../evidence/workspace-storage-retry.json).

This was deliberate input/I/O fault injection in one synthetic profile, not repair of an acceptance snapshot. No workspace IDs or fields were manufactured. The unsaved memory mutation was not counted as durable. The bounded AX walk omitted the warning label, so its text was inspected in actual captures instead. No complete accessibility claim follows.

Private run `/tmp/myarc-m0-runtime/storage-fault-20261026T024335Z/` retains the exact backup, fault audit, real writer error and write/read captures. Apps/helpers/server closed; faulty directory and backup retained. The candidate repair below exposes state-specific retry actions using the existing Profile-bound service. Automatic recovery and durability remain unaccepted.

## Candidate 0023

MA-007.c bounded repair from parent `7045dda`. Updated canonical [SidebarView](../foundation/sidebar_view.h) and optional [0023](../patches/0023-native-workspace-storage-retry.patch) expose:

- `Retry loading` only for service `kLoadError`, with workspace mutations unavailable and native tabs retained.
- `Retry saving` only for service `kWriteError` with a usable workspace model. Memory remains authoritative until the existing asynchronous writer finishes.

BrowserView posts a weak callback so the pressed button unwinds before notifications rebuild its children. Dispatch rechecks the current state and invokes that adapter's exact-Profile `RetryLoad()` or `RetrySave()`. Pending operations omit the action; completion notifications determine the next display. Repeated failures restore the error/action. No unconditional success message, reset, input repair or native recovery-barrier release was added.

Standard `MdTextButton` controls retain keyboard focusability and their visible accessible names. The unavailable message scrolls independently above its retry button. Its region gets up to 192 DIP, capped at half the remaining sidebar height, preserving native fallback tab space. Ready layout, hover geometry/material and rendering-default-OFF remain unchanged. Focus moves from a disappearing load action to the status scroll view, then returns to the retry action on failure or existing workspace focus on success. Actual native visibility, focus and accessibility still need parent verification.

Storage service, Rust core, schema, adapter and lifecycle sources are byte-identical. Private/ephemeral, loading, write-pending, normal and recovery/binding-only waits do not acquire unsupported retry actions.

## Checks and packaging

Canonical 0001 carries only the two SidebarView file changes; its other 14 owned payload files remain unchanged. Apply 0023 once after that refreshed canonical payload and the unchanged optional stack through 0022. The incremental patch changes only frame `BUILD.gn`, `browser_view.{cc,h}`, `browser_view_browsertest.cc` and `vertical_tab_strip_region_view.cc`. All earlier optional patch artifacts remain byte-identical; the parent baseline evidence is untouched.

- Changed-line Chromium clang-format, GN formatting and four real-context clang syntax checks pass: sidebar, BrowserView, vertical region and browser fixtures. Initial fixture syntax rejected `auto*` deduction over Views' raw-pointer children; using the existing container's reference form fixed it.
- Actual nonzero GN source/header checks pass for `//myarc:sidebar_view` and `//chrome/browser/ui/views/frame:{browser_view,vertical_tab_strip_region_view,browser_tests}`. No browser-test graph was built.
- `autoninja -C out/MyarcM0 -j 8 chrome` completed with terminal exit `0`, 03:02:03Z to 03:04:25Z on 2026-10-26. Final source predates this build; all three affected production objects and `libchrome_dll.dylib` are fresh. Existing launcher/framework shim timestamps remain older. Compiled-artifact hashes are retained privately.
- Bridge/lifecycle helper build and both executables pass. Bridge execution still emits the previously recorded duplicate `ANGLESwapCGLLayer` warning. Cargo fmt, bridge rustfmt, clippy and 32 Rust tests pass. Foundation-payload freshness and document links pass.
- Strict indexed incremental reverse/replay passes twice across the frozen 82-file baseline. A separate fresh full-stack replay from upstream `8d208f20744ac6d4ce00cc3f9de6a14a9b8b926d` matches all 83 installed files, including an additional unmodified toolbar GN input. Of these, seven owned files change and 76 remain byte-identical. Installed canonical16 matches this worktree. Existing independent layout bytes remain unchanged.

Three native browser fixtures are added but **UNRUN**. They cover failed/successful load retry with unchanged bad input and blocked mutations; failed/successful real writer retry with memory and original backup preservation; and recovery-hold/private-workspace omission. Subscriptions check that pending states omit both actions. Source compilation does not establish native acceptance.

Private proof: `/tmp/myarc-m0-runtime/mode23/`, including frozen hashes, adjusted payload script, replay scripts/logs, GN owners, exact syntax commands, terminal build exit, compiled hashes and exact executable-comm preflight. No owned app/helper executable was running at prelink/final checks. No apps, servers or native input were used; no process was killed.

0023 SHA-256: `17365951a2d47f0fe372e733494bed353b921e9ba6bd0868a49e866e74362584`.

## Parent acceptance still required

Parent owns native launches after handoff. Preserve the baseline Profile, backup and fault provenance. In a guarded marked synthetic Profile, check actual failed Retry saving, explicit filesystem repair, then successful Retry saving with the latest in-memory Space. Separately check invalid-input preservation on failed Retry loading, explicit external repair, then successful Retry loading. Confirm native fallback tabs, full warning visibility/scrolling and keyboard focus across each transition. Manual backup restoration must stay distinct from automatic recovery or durability evidence. Baseline JSON remains a failure observation until fresh parent evidence exists.
