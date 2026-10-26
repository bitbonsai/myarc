# Pending native recovery choice

MA-007.c-recovery-lifecycle, 2026-10-25. Build candidate over myarc `2101a464fba1cf64dca0c1fd6bfdedcb4fff7e2c` and external Chromium `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`, with the frozen optional stack through 0017. Renderer remains off by default. Bounded persisted-state Restore and explicit-discard checks passed; recent site-storage acknowledgement and broader durability remain open.

## Cause and bounded fix

The [actual process-kill/Restore failure](../evidence/workspace-process-crash.json) remains unchanged. Startup finished while the recovery prompt waited. Reconciliation pruned Temporary ID 2 and allocated startup NTP ID 3 before genuine Restore could supply its native hints. The restored page then received ID 4. Both URLs returned, but stable identity failed.

The canonical lifecycle boundary now counts pending recovery choices alongside startup launches and native restorers. The adapter handles choice-start as a restore barrier and choice-finish through its existing weak, non-nestable completion path. Existing binding, pruning and persistence gates consume that shared readiness state. No IDs are reconstructed and Temporary lifetime policy is unchanged.

[0018-native-recovery-lifecycle.patch](../patches/legacy/0018-native-recovery-lifecycle.patch) retains the existing browser-removal observer across these paths:

- `ShowIfNotOffTheRecordProfile` creates it before branded consent lookup. The same owner moves through the async reply and successful Show into the dialog delegate, without releasing the count. Failed posting/canceled callbacks, missing browser/tab and unsupported BrowserView release through destruction.
- Restore moves both the stock `CrashedLock` and recovery observer into local ownership before dialog Close deletes the delegate. `RestoreSessionAfterCrash` constructs the native restorer before the local choice owner releases. Its independent restorer count then gates reconciliation through native completion observers.
- Startup-pages moves the observer across dialog deletion and native page opening. Explicit dismissal releases with delegate destruction. Both choices then permit normal reconciliation, which retains saved Favorites and Spaces under existing Rust rules.
- Exact-window removal clears its browser pointer and releases once, including while consent is pending. Exact-Profile observation releases before Profile destruction; it first latches that Profile's existing shutdown gate because teardown may wait for renderers before destroying runtime UserData. Reset observation prevents later callback destruction from notifying a reused Profile address. Stale dialog actions compare the observed browser before dereferencing their native pointer.
- Application quit retains the existing shutdown latch. Canceling quit does not consume a live recovery choice. Multiple choices and restorers retain independent counts for the same Profile; unrelated Profiles are unaffected. Flag-off calls never construct an adapter/service.

Inspected startup infobar creation/first-active-tab callbacks, the ChromeOS SessionService caller, consent task/reply ownership, Show failures, dialog destruction, both actions, browser removal, Profile observer ordering, restore constructor/completion/cancellation, and existing shutdown/cancel callbacks. Stock UI, consent policy, anchor 0016, CHECKs and native recovery backend remain intact.

## Files and packaging

Four canonical files change: `workspace_lifecycle.h`, `.cc`, `workspace_lifecycle_check.cc` and `browser_workspace_adapter.cc`. Their refreshed payload remains in [0001](../patches/legacy/0001-workspace-foundation.patch), with exactly 16 canonical owned files and no core/schema/dependency changes.

0018 contains four upstream deltas under `chrome/browser/ui/views/`: recovery implementation/header, the existing interactive fixture, and its BUILD dependencies. Apply once after the frozen optional stack, using refreshed 0001 for a fresh checkout. The handed-off external checkout already contains both owned and upstream changes; do not apply them twice.

- 0018 SHA-256: `c5da1b27ffbe1fa861e8f2465cbbb508512b423544b7c86172439b9b8dd4abab`.
- All prior optional patches, including 0012/0016/0017, are byte-identical. Only 0001's owned payload changes.
- Captured 75 external source files. Strict indexed reverse/replay of 0018 and a private owned-source delta restores every baseline hash and reproduces all installed hashes. Eight files change; 67 unrelated files remain byte-identical. All 16 installed owned files match canonical sources.

## Actual checks

- `gn gen out/MyarcM0` and separate GN header checks for `views:session_crashed_bubble_view` and `views:interactive_ui_tests`: exit 0.
- Built and executed `workspace_lifecycle_check`: exit 0. Four assertion groups cover registration, unknown metadata, startup/restorer counts and recovery-choice ordering. Profile identities are opaque tokens; this executable creates no native Profile. Recovery checks include late registration, pending native-ready notifications, handoff, exact-Profile release, multiple choices and canceled quit.
- Existing native interactive fixture extended with Restore ordering, startup-pages discard, aborted Show and window-close cases. Existing shutdown-action and shell-popup cases also assert release. Final translation unit passes `-fsyntax-only` with its real generated compiler flags and empty diagnostics. These native cases were **not executed**; no browser-test dependency graph was built.
- `PATH+=depot_tools DEPOT_TOOLS_UPDATE=0 autoninja -C out/MyarcM0 -j8 chrome`: exit 0, 22:13:06–22:13:24 UTC. Fresh Ninja entries confirm adapter/recovery compilation and `libchrome_dll.dylib` link. Executable `comm` paths checked before linking; no candidate executable was running.
- Changed-line clang-format, GN formatting, document links, canonical payload freshness and whitespace checks pass. No Rust source changed; Cargo tests were not rerun.

Private proof: `/tmp/myarc-m0-runtime/recovery18/`. Baseline/installed manifests, source copies, `package.py`, private `owned-foundation.delta.patch`, `replay.log`, adjusted payload refresh script, helper logs, final syntax command/log, GN checks, build log/exit/timestamps and process snapshots remain there. The refresh script writes this prepared worktree, never the parent's checkout.

## Parent native checks

Integrated as `2a0ef32`. Three fresh marked synthetic profiles exercised real process-kill recovery:

- First repeat retained exact metadata while the genuine prompt waited, then restored the original IDs and both URLs. Full verification failed on a freshly acknowledged localStorage canary. The verifier stayed strict. Upstream DOMStorage source is unchanged and configures a five-second default commit delay, with further rate limiting possible. This is source configuration, not a measured persistence guarantee or a stock-browser runtime comparison.
- Persisted-data control seeded its checkpoint, quit normally, reopened with explicit session restoration and verified the same canary/metadata. Only then did the parent audit and SIGKILL that live browser. Its subsequent ordinary relaunch supplied neither URLs nor a restore flag; genuine Restore passed the unchanged verifier for five samples, including stable IDs/owners/selection, actual pages and canary. Crash provenance records the intervening clean seed, so this proves previously persisted storage recovery only.
- Separate fresh crash/discard retained exact pending metadata. Closing only the genuine recovery popup released readiness, preserved the saved Favorite and original Spaces, and pruned the discarded Temporary afterward. A real new Space was created and persisted, proving the adapter resumed. Prior page URLs were not restored.

[Structured evidence](../evidence/workspace-process-crash.json) preserves the original failure and these narrower followups. Private artifacts are under `/tmp/myarc-m0-runtime/recovery18-acceptance-20261025T221946Z`, `recovery18-persisted-control-20261025T223056Z` and `recovery18-discard-20261025T223451Z`. Native applications/helpers and owned fixture servers were closed. Exact Profile/PID/window and unlocked/focus guards remained intact; QA and Arc were untouched.

Native selection checks confirmed an already-selected global Favorite may remain selected across a Space switch. An earlier scratch expectation that switching must replace it was rejected against the Rust rule. Explicit row activation selected the correct restored pages. Profile ownership checks waited for the actual native on-disk cache rather than manufacturing it.

## Implementation handoff and limits

The implementation delegate performed no app launch, desktop input, actual-profile access, installs or service kills. No build remains running. External source/build ownership returns to the parent after this candidate handoff.

Use a fresh marked synthetic fixture: the failed fixture already lost ID 2 and this change does not repair overwritten metadata. Before Restore, confirm prior metadata IDs, owners, canonical URL and selection remain unchanged while the prompt waits. Invoke genuine native Restore after a new audited process-kill and normal relaunch without startup URLs or `--restore-last-session`, then run the unchanged strict verifier. Separately dismiss/open startup pages and confirm normal readiness resumes while Favorites/Spaces survive. Exercise pending consent/window-close and canceled-quit paths where available.

Source/helper/build checks are not native runtime proof. Branded async consent and real Profile teardown remain source-audited only. Branded consent/window teardown and canceled-quit native cases, broader multiwindow races, acknowledged-write boundaries and OS/power-loss durability remain unaccepted. Fresh site-storage acknowledgement still fails the process-crash probe.
