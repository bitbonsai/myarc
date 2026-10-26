# Native palette shortcut intent

MA-007.c bounded repair from parent `9426f9e`. Optional [0022-native-palette-intent.patch](../patches/0022-native-palette-intent.patch) follows the frozen stack through 0021. It addresses the [observed Cmd+T/draft/Cmd+L dismissal](../evidence/native-palette-interruption.json). Source and fixture syntax checks and the full Chrome build pass. Desktop acceptance and browser fixtures are **UNRUN**. Workspace renderer stays default OFF.

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

## Parent acceptance still required

Use a marked disposable profile and exact foreground owned palette. Reproduce native Cmd+T, draft, Cmd+L and verify selected displayed URL; repeat both same-intent shortcuts without losing edits; switch back with Cmd+T and verify no page until submit. Check current-tab reload/edit, Empty-Space hidden-page protection, Escape/immediate reopen and owner-window close. Inspect normal/reduced-motion focus and a real history response across interruption where feasible. Compilation cannot accept these journeys.

No app/browser/helper or fixture server was launched. Protected/real Profiles, OS permissions/settings, singleton locks and external services were untouched. Parent owns desktop launches and recordings. External source/build ownership remains exclusive during this task and will be explicitly relinquished at handoff.
