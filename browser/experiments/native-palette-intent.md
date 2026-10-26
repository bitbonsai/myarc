# Native palette shortcut intent

MA-007.c bounded repair from parent `9426f9e`. Optional [0022-native-palette-intent.patch](../patches/legacy/0022-native-palette-intent.patch) follows the frozen stack through 0021. It addresses the [observed Cmd+T/draft/Cmd+L dismissal](../evidence/native-palette-interruption.json). Source and fixture syntax checks and the full Chrome build pass. Parent bounded desktop acceptance passed below. Browser fixtures remain **UNRUN**. Workspace renderer stays default OFF.

## Behavior and boundary

`BrowserView::ShowMyarcCommandPalette()` now computes effective intent before examining an existing widget. An empty Space forces new-tab intent before reading any current URL. A live palette is reused:

- New-tab to current-tab intent replaces input with `LocationBarModel::GetFormattedFullURL()` and selects it.
- Current-tab to new-tab intent clears input. No tab is created until submission.
- Repeated effective intent retains the draft and selects it, with native widget activation and input focus.

The existing `initial_text_` comparison still chooses unchanged-current-URL reload. Changed current input and history results retain current-tab navigation. New-tab submission retains existing navigation policy. Existing bounds, regular-Profile history lookup and off-the-record history exclusion are unchanged. Cross-Space tab matching remains global within the current native window, provisionally; this slice adds no search-scope policy.

An intent change uses the existing `Rebuild()` path, including bounded results and cancellation of prior history work. Same-intent focus does not rebuild or discard edits. Rust workspace rules, schema, recovery holds from 0018, adapter ownership/readiness from 0019/0020, AI entrypoint gate from 0021 and flags-off routing remain unchanged.

## Close and focus ownership

Upstream `BubbleDialogDelegate::CreateBubble()` installs a synchronous close callback. The existing callback still clears the anchor and defers widget deletion before delegate deletion with `DeleteSoon`. It now carries the owning delegate identity and ignores a callback for a different delegate. This prevents cleanup from reaching replacement members.

Before deferred destruction, the view cancels tracked history tasks and invalidates its weak callbacks. Browser access remains weak. Reuse and initial show retain weak view handles across rebuilding/showing; focus checks widget availability/closed state and weak view validity across native activation. No posted focus task, close/recreate transition or pending-job controller was added. An already closed retained widget is cleaned up before a replacement is created.

The prior owner-close smoke observed both windows closing. It did not observe a pending-history race. These source changes and fixtures do not establish broader shutdown-race safety.

## Native fixtures, UNRUN

Four cases in `chrome/browser/ui/views/frame/browser_view_browsertest.cc`, fixture `BrowserViewPaletteIntentTest`, assert:

- `ReuseIntentDraftsAndCancelThenReopen`: same widget across intent changes; empty/new and displayed-current text; both same-intent drafts selected; no page creation/navigation while editing; Escape then immediate reopen survives deferred old deletion.
- `CurrentReloadEditAndNewTabSubmission`: unchanged current URL reloads; changed current input keeps the same WebContents/count; new intent creates one tab only on submit.
- `EmptySpaceGuardsExistingCurrentIntent`: a real adapter-selected empty Space changes an existing current editor into empty new-tab intent, preserves its later draft, creates a separate submitted page and leaves the hidden URL intact.
- `OwnerCloseWithHistoryWorkQueued`: regular-Profile query text followed by actual owner-browser close and deferred deletion, with a surviving browser window.

The fixture uses production entrypoints and Views text/key dispatch. It does not deliver OS Cmd shortcuts, record desktop focus/motion, force a history callback to overlap close, or inspect reduced motion. All four cases are **UNRUN**. The owning target's real Chromium clang defines, includes and flags pass syntax checking for both production and fixture source. A successful syntax check does not execute assertions.

## Verification and packaging

External Chromium HEAD remains `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`. Changed upstream files:

```text
chrome/browser/ui/views/frame/BUILD.gn
chrome/browser/ui/views/frame/browser_view.cc
chrome/browser/ui/views/frame/browser_view.h
chrome/browser/ui/views/frame/browser_view_browsertest.cc
```

GN generation, source-list checks and header checks pass for nonzero `//chrome/browser/ui/views/frame:browser_view` and `:browser_tests`. The GN delta adds only the fixture's omnibox and existing adapter dependencies. Initial fixture syntax rejected implicit `BubbleAnchor` conversion and private Textfield key-method access; explicit anchor construction and dispatch through the public View interface corrected both.

Strict indexed reverse/replay passes across all 82 captured source files, with two replay/reverse cycles. Exactly four files change; all 78 unrelated files remain byte-identical. All 16 installed canonical payload files match this worktree. Canonical 0001, the earlier optional patch artifacts through 0021 and independent layout bytes remain unchanged; `upstream.json` is unchanged.

Private proof: `/tmp/myarc-m0-runtime/mode22/`. The mode21 snapshot, packaging and syntax scripts were inspected, copied privately and adjusted to this worktree before use. Proof includes before/installed manifests, canonical/prior-patch hashes, replay logs, real-context syntax commands, GN checks and exact executable-comm prelink checks. No running owned executable matched the prelink inventory; no process was killed.

0022 SHA-256: `5b070b3ac63aa266425734016b02f705c1680c8283e2fd35028c458fb02f31c2`.

`autoninja -C out/MyarcM0 -j 8 chrome` completed with terminal exit `0`, 01:49:24Z to 01:49:45Z on 2026-10-26. An earlier successful build overlapped the final focus-guard edit; a forced production-source recompile and relink from frozen final bytes removed that ambiguity. The final source predates this second build and its fresh 419,856-byte `browser_view.o` and 165,341,872-byte `libchrome_dll.dylib`. The unchanged app launcher/framework shim retain older mtimes, as expected. No browser-test graph was built.

Changed-line clang-format, GN formatting, document links, canonical foundation-payload freshness and whitespace checks pass. Both real-context syntax checks were repeated successfully after the build against final source. The lifecycle helper and Rust suites were not rerun because their sources and policy are unchanged.

## Parent bounded native acceptance

Integrated as `2c1f49d`; installed 82-file hashes match. A fresh marked native run retained the same actual palette window through Cmd+T/new draft, repeated Cmd+T, Cmd+L/current URL, repeated current draft and back to empty new-tab mode. Native AX reported input focus and full selection; same-intent drafts survived. Chromium formatted the local URL without scheme/trailing slash, correcting a scratch literal-string expectation.

Escape restored real page focus without reload. Unchanged current Enter reloaded the same target; an unmatched changed URL navigated the same target/count. New intent created exactly one target only on submit. Selecting the empty Space natively closed the existing palette, so the driver stopped when its old window disappeared. Fresh empty Cmd+L exposed no hidden URL, retained its repeated draft and created a separate page while hidden targets/URLs/time origins stayed exact. The forced direct-reuse/empty transition remains a source-only fixture.

Fresh normal and app-only reduced-motion owned-window sequences were recorded and inspected. Source capture initially put a 66x20 same-PID non-key indicator ahead of the window; guards stopped input. The private driver then verified exact AX focused-window bounds/nonmodal state under that bounded overlay, retaining PID/marker/unlocked/foreground checks. Its old post-transition key-release ordering was corrected so cleanup releases the key before checking a changed window. No production guard was relaxed. Exact animation timing and the OS preference matrix remain unmeasured.

The same run also passed the unchanged two-window checkpoint and changed-PID verifier after graceful horizontal quit/restoration and late vertical enable, with app-only reduced motion. IDs, both native Favorite pins and the Temporary projection survived. An earlier outer command timeout produced no verifier result and was excluded. Closing one actual owner window with its palette open removed both while the other window retained saved item `3`, its actual extension-observed native pin and working Cmd+T/Cmd+L focus. Escape/reopen also worked; true pending-history callback overlap and deferred-deletion overlap are not claimed.

Private run `/tmp/myarc-m0-runtime/palette22-20261026T015329Z/` retains semantic snapshots, AX fields, native pin observation and fresh media. A saved destination stayed unchanged through actual live-page navigation; deliberate Unsave/resave captured the new committed destination under the same logical ID. AX request return alone was never counted as persisted save acknowledgement. No workspace metadata was edited to make a check pass. All owned apps/helpers/server closed.

[Structured evidence](../evidence/native-palette-interruption.json) retains the failure and this bounded correction. Existing global open-tab/history matching is unchanged. A typed URI alias left its prior page unchanged during setup; its matched-result provenance was not captured, so broad URI-literal editing semantics remain unaccepted. A separately recorded existing-target developer navigation prepared the later saved fixture; it does not accept that UI route or implement product CLI/MCP.

## Current-address default submission defect through0023

A later fresh native two-page run captured the previously unmeasured matching case. With the Reading Temporary active, Cmd+L showed its actual URL. Typing the full URL of the other existing Favorite and pressing Enter, without choosing a result, switched to that old target. The original Temporary kept its prior URL/time origin. Page count stayed two. The visible row explicitly said Switch to Tab; no implicit navigation was counted. [Baseline](../evidence/native-palette-address.json).

The current-address contract requires changed input to navigate that same target. Proposed narrow repair leaves current-intent matches initially unselected, retaining explicit arrow/mouse activation and the existing new-intent default. Matching/ranking and provisional global scope remain unchanged. No URI classifier or Rust policy change is needed.

The same0023 build separately passed a clean persisted seed and audited SIGKILL/plain relaunch/genuine Restore with five unchanged strict samples. Pending metadata stayed byte-exact; no storage retry appeared during the recovery hold. This independent [crash regression](../evidence/workspace-process-crash.json) used a fresh profile without the storage-fault experiment. Owned apps/helpers/server closed.

## Candidate 0024: current-address literal submission

From parent `5f2b76a`, optional [0024-native-current-address-submit.patch](../patches/legacy/0024-native-current-address-submit.patch) follows frozen 0023. Two existing automatic-selection sites now require new-tab intent: synchronous open-tab rebuilding and asynchronous history arrival. Current intent retains `selected_ = -1` until deliberate arrow selection; row mouse callbacks still activate directly. Every text edit resets selection through the existing rebuild path.

Plain changed current-address Enter therefore calls the existing `Submit()` path even when rows match. Unchanged current input still reloads. Explicit result activation retains global cross-Space selection/reveal within the native window. New-intent matching, ranking and default selection remain unchanged provisionally. No classifier, controller or Rust policy was added. Empty-Space guarding before reuse, same-widget drafts/focus, history bounds/cancellation, weak ownership, saved destination policy and flags-off/default-OFF behavior are unchanged.

Four added `BrowserViewPaletteIntentTest` fixtures remain **UNRUN**:

- `MatchedCurrentLiteralKeepsSameContents`: an existing open-tab URL is submitted literally to the original WebContents/count; the other page retains its URL/time origin.
- `ExplicitCurrentResultsRevealOtherSpace`: Down/Enter and a row mouse callback activate a real page in another adapter-created Space, revealing its owner while retaining both URLs/count.
- `NewIntentKeepsDefaultOpenTabMatch`: plain Enter still activates the matched existing page without adding a tab.
- `LateHistoryDoesNotSelectCurrentButDefaultsNew`: waits for real asynchronous history rows; current intent submits the typed URL rather than the longer history URL, while new intent still opens its default history destination.

Existing reload, intent/draft, Empty-Space and owner-close fixtures are retained. These use production entrypoints and Views dispatch, not OS keyboard/mouse input. Real-context syntax checks pass for production and fixture sources. Owning `//chrome/browser/ui/views/frame:browser_view` and `:browser_tests` source lists are nonzero and their GN checks pass. Two history dependencies were added only to the existing fixture target; no browser-test graph was built.

Strict indexed incremental reverse/replay passes twice. A fresh full stack from original upstream `8d208f20744ac6d4ce00cc3f9de6a14a9b8b926d` matches all 83 installed files at external HEAD `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`. Only `browser_view.cc`, `browser_view_browsertest.cc` and frame `BUILD.gn` change; the other 80 files, canonical16, 0001 and every prior optional patch remain byte-identical. The frozen independent0003 residual is empty; existing layout bytes are already in the prior stack. An initial replay-script assertion incorrectly required a nonempty residual; the fresh retry skips it and verifies every installed hash without copying over replay output. Baseline evidence above remains unchanged.

`autoninja -C out/MyarcM0 -j 8 chrome` completed with terminal exit `0`, 03:33:16Z to 03:33:37Z on 2026-10-26. Frozen source hashes match before/after build; the 421,608-byte `browser_view.o` and 165,343,424-byte `libchrome_dll.dylib` are newer than final production source. Exact executable-comm prelink checks found no owned app process; none was killed. Private proof `/tmp/myarc-m0-runtime/mode24/` contains source/build manifests, commands and terminal logs.

0024 SHA-256: `757c67756796d4377021c909aa3b0a81c78784ee2da529a5d98059e061163f33`.

Changed-line Chromium clang-format, GN formatting, lifecycle helper, Rust formatting/clippy/32 tests, canonical foundation-payload freshness, document links and whitespace checks pass. Unchanged native-runtime client, launcher, storyboard and legacy bridge/codec suites reuse prior checks; they were not rerun here. Native browser/interactive suites remain UNRUN.

Native acceptance for 0024 remains with the parent: repeat the actual two-target matched-current submission, then deliberate global result activation and unchanged new-intent behavior. Recheck unchanged-current reload and saved destination retention. The prior 0023 crash regression above does not accept this build. No native app/input/desktop check or server was launched by this implementation task.

## Parent bounded native0024 followups

Integrated as `e47b824`; all83 source/artifact hashes match and canonical16 stayed unchanged. Actual normal-mode matched current input navigated the original target while the other target/time origin stayed exact. Deliberate Down/Enter and row mouse activation revealed the Reading owner without navigation/extra pages. Cmd+T default matching still activated the existing result. An actual history-only row arrived: current intent used the literal URL without its history query suffix; new intent retained the history default and created one real page. Closing that Temporary retained the original two-item saved model.

Reduced mode observed actual reduced motion and passed matched-current same-target navigation. Two rapid-reopen repeats then stopped with the result row focused and the input unchanged. Repeated native Cmd+L refocused it. The second driver added a specific input-focus precondition and stopped before typing. Full reduced-repeat acceptance is withheld. A separate [normal native hover probe](native-palette-hover-focus.md) reproduced focus moving to a result on pointer entry without clicking, with draft/page untouched.

A post-seed recent-navigation crash failed on duplicate restored URLs; an independent no-post-seed-navigation clean-persisted control passed. [Durability evidence](native-session-navigation-crash.md) retains both. Private address run `/tmp/myarc-m0-runtime/address24-20261026T034121Z/` holds AX/page/history captures and failed logs. Apps/helpers/servers closed. Native C++ fixtures and broader focus/durability remain unaccepted.

## Historical 0022 delegate handoff

The following remaining-acceptance and ownership notes are retained from the 0022 source-only handoff. Later parent acceptance above supersedes its named desktop cases; they are not 0024 build acceptance.

Use a marked disposable profile and exact foreground owned palette. Reproduce native Cmd+T, draft, Cmd+L and verify selected displayed URL; repeat both same-intent shortcuts without losing edits; switch back with Cmd+T and verify no page until submit. Check current-tab reload/edit, Empty-Space hidden-page protection, Escape/immediate reopen and owner-window close. Inspect normal/reduced-motion focus and a real history response across interruption where feasible. Compilation cannot accept these journeys.

No app/browser/helper or fixture server was launched. Protected/real Profiles, OS permissions/settings, singleton locks and external services were untouched. Parent owns desktop launches and recordings. External source/build ownership remains exclusive during this task and will be explicitly relinquished at handoff.
