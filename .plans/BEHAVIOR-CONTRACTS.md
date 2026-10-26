# Behavior contracts before implementation

Status: initial specification seeds, not verified application behavior. Each implementation task extends the relevant seed and resolves its unknowns before claiming Arc parity. Use original summaries, sources and synthetic examples. Never publish a real user's browser state here.

CLI/MCP browser control is a myarc product requirement, not a claim of Arc feature parity. BC-004 covers that additional behavior.

## Required fields

Contract ID and parent task; source URL/date and reference product/version/platform; evidence kind (`documented`, `observed`, `project requirement`, `proposed`, `unknown`); initial state; user action; expected state; keyboard/focus; persistence/restart; cancellation/error; required semantic and visual proof; unresolved questions.

Source evidence and our product decision must remain distinguishable. If no reference version or timing has been measured, write unknown rather than implying an exact reproduction. References are inventoried in [RESEARCH.md](../docs/RESEARCH.md).

## BC-001: Spaces share a Profile without becoming one Space

Parent: MA-004/007. Evidence: Arc's publicly documented [Profile model](https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing); current behavior across supported Arc versions still needs confirmation.

- Initial state: synthetic Spaces S1/S2 reference Profile P1; Space S3 references Profile P2. S1/S2 have different pins/folders.
- Action: switch among S1, S2 and S3.
- Expected product behavior: S1/S2 keep separate workspace organization while sharing P1 browser data; S3 uses isolated P2 browser data. Switching must never briefly expose another Profile's search suggestions or page content.
- Keyboard/focus: retain a deliberate useful focus target; no unexpected command-bar activation. Exact shortcuts need a platform-specific specification.
- Restart: preserve profile references and workspace organization; runtime browser IDs are not persistent identities.
- Failure: invalid/missing Profile references are reported and recovered without silently merging browser data.
- Proof: pure-core ownership/serialization checks plus browser integration checks for cookies, history, extensions and command suggestions. Core checks alone cannot demonstrate browser isolation.

## BC-002: Import success drives the reveal

Parent: MA-001/006/008. Evidence: project requirement and original [onboarding storyboard](../docs/ONBOARDING.md); historical Arc motion timings are unknown.

- Initial state: synthetic import preview with valid Spaces, folders and pins; nothing is committed and no imported URL has been opened.
- Action: confirm import.
- Expected behavior: validated commit completes before success/reveal. The reveal reads the committed workspace and uses stable IDs; it does not independently construct a decorative substitute workspace.
- Keyboard/focus: Skip remains available; reduced motion exposes equivalent information; handoff focuses a useful actual browser control.
- Restart: after commit, reuse that import receipt and saved workspace rather than importing again. Before commit, preserve the prior workspace.
- Failure: permission denial, malformed input, cancellation or disk failure shows a recoverable error without false success animation.
- Proof: semantic state/receipt checks, interruption cases, and a fresh real-build recording in normal/reduced motion. A prototype satisfies design exploration only.

## BC-003: A pinned tab has explicit navigation and closure rules

Parent: MA-004/007. Evidence: project requirement; detailed Arc reset/close/multiwindow behavior remains unknown. This is a required investigation rather than a completed parity specification.

- Initial state: a synthetic named pin with a canonical URL and a currently navigated page.
- Actions to resolve: activate, navigate, reset, close its live page, remove its pin, move between Spaces, and restart.
- Required product invariants: removing a runtime page must not accidentally delete a persistent pin; changing the canonical URL must be deliberate; Profile boundaries apply to moves; failures must not lose the saved organization.
- Unknowns to settle before implementing: exact reset gesture/shortcut, when navigation persists, focus after closure, duplicate policy, and shared/multiwindow semantics. Record the chosen behavior and its evidence.
- Proof: a command transition table with final canonical/current URL and ownership for each action; persistence checks; browser focus/navigation checks. Do not infer correctness from a pin icon being visible.

## BC-004: An authorized agent acts on a real page

Parent: MA-011/007. Evidence: project requirement and proposed [control contract](../docs/CONTROL-PROTOCOL.md); no implementation exists.

- Initial state: built-in AI off; synthetic site in tab T1; a different Profile and tab T2 outside the requested scope; no external control grant.
- Action: connect an external MCP client, obtain user approval for T1, inspect, fill/click, wait for a real DOM/state result, inspect/screenshot, then Stop control.
- Expected behavior: only granted context is visible/actionable; the website changes as expected; the browser shows the controller; Stop control rejects queued/new operations immediately. Repeat the same journey through the CLI and compare outcomes.
- Invalidations: stale references, navigation/redirect outside scope, permission denial, cancellation or disconnect produce explicit results and never automatic repeated submissions. Page instructions cannot expand the grant.
- Restart: expired/revoked grants never become silently active after relaunch. Ordinary local browsing remains available.
- Proof: semantic final state plus native browser recording and external-client trace, synthetic data only; record protocol/client/build versions. Listing MCP tools alone is insufficient.

## BC-005: Peek routing

Parent: MA-007. Evidence: documented, Arc's Peek article ([source](https://resources.arc.net/hc/en-us/articles/19335302900887-Peek-Preview-Sites-From-Pinned-Tabs), Internet Archive copy 2024, see the [backlog](2026-10-26-arc-clone-backlog.md) Peek & Split View section) and the [Pinned Tabs article](https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around). Rule implemented and tested in `crates/workspace/src/peek.rs`; the overlay UI does not exist.

- Initial state: a synthetic pinned or favorite item showing a page on site A; Peek enabled.
- Action: a link to site B is activated. The caller passes scheme and registrable domain for source and target.
- Expected behavior, in this order: Cmd-click opens a new tab. Otherwise a Temporary source, a non-http(s) scheme on either side, an unknown (empty) site or the same site keeps the navigation in the current tab. Otherwise a cross-site link from a pinned or favorite item opens in Peek, or in a new tab when the Advanced Peek setting is off.
- Peek actions: "open in tab" (Cmd+O) promotes the Peek page into a new Temporary item in the current Space; close (outside click, X, Cmd+W) dismisses Peek and returns to the source item.
- Keyboard/focus, persistence, error: Peek pages are not workspace items and are never persisted. The core only returns a decision; focus handling is the adapter's job.
- Proof: unit tests in `peek.rs`. Native overlay behavior, focus and reopen need a fresh real-build capture before any parity claim.
- Unknowns (simplest reading chosen): Shift-click opening Peek is not in the sources, so it is not modeled. Option-click (Split View) and expanding Peek into a split are out of scope. Whether the setting also covers links from Temporary tabs is undocumented; Temporary tabs never Peek here. Whether subdomain or cross-scheme moves count as the same site is undocumented; same registrable domain counts as same site, scheme is ignored. Reopening a closed Peek (Cmd+Z/Cmd+Shift+T) is not modeled. What happens to Peek on window close or Space switch is unknown.

## BC-006: Auto Archive moves idle temporary tabs out of the sidebar

Parent: TAB-02, backlog "Tabs lifecycle". Evidence: documented in Arc's [Auto Archive article](https://resources.arc.net/hc/en-us/articles/19228855311127-Auto-Archive-Clean-as-you-go) (12 hours by default, viewing resets the timer, timing per Profile, Little Arc windows after 6 hours; 2024 snapshot), the [Pinned Tabs article](https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around) (pins never archive), the [Folders article](https://resources.arc.net/hc/en-us/articles/19228419623447-Folders-Stash-Similar-Tabs-Together) (folders never archive) and the [2022 release notes](https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes) (media tabs are skipped). Interval choices 24 hours, 7 days, 30 days and never are `proposed`: the saved sources give no option list. Selected-item exclusion and clock handling are `project requirement`.

- Initial state: temporary items with last-viewed times, one Profile policy, `now` from the caller's clock.
- Action: `archive::decide_archive(items, selected, policy, now_secs)` at launch, at each wake-up, and after any change to selection, media state, placement or last-viewed time.
- Expected: archive the item ids that are `Placement::Temporary`, not playing media, not selected in any window, and idle for at least the interval (`now >= last_viewed + interval`). Favorites, Pins and any other placement never archive. The result also names the earliest future deadline so the browser keeps one timer. Items excluded by selection or media add no deadline, which is why those events force a new call.
- Interval: 12 hours by default, per Profile; Little Arc windows use 6 hours whatever the Profile says.
- Cancellation/error: a clock that moved backwards (`now < last_viewed`) means not yet due. Time arithmetic saturates. Empty input and `never` return nothing and no wake-up. The rule never reads a clock, so the caller owns the time source; mixing monotonic and wall values across calls is the caller's bug.
- Persistence/restart: last-viewed times are not persisted by this crate yet. On relaunch the adapter must restore or reset them before the first call; resetting is the safe choice (nothing archives early).
- Proof: unit tests in `crates/workspace/src/archive.rs` cover each rule. Browser timer, archive store, restore and UI are still unproven.
- Unresolved: whether Little Arc overrides a `never` Profile; exact timing source (Arc may count foreground time, not wall time); form-input and Split View exclusions; folder items (the model has no folder placement yet, so no folder case exists to test); the downloads schedule.

## BC-007: Clear unpinned closes temporary tabs that are not playing media

Parent: TAB-02, backlog "Tabs lifecycle" and Next 15 item 7. Evidence: documented in Arc's [Keyboard Shortcuts article](https://resources.arc.net/hc/en-us/articles/20595231349911-Keyboard-Shortcuts) (Cmd+Shift+K clears unpinned tabs) and the [2022 release notes](https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes) (media tabs are skipped). Pins and Favorites staying put follows the [Pinned Tabs article](https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around). Rule implemented and tested in `crates/workspace/src/tab_ops.rs`; the shortcut, close path and animation do not exist.

- Initial state: the current Space's items with placement, media state and the window's selection.
- Action: `tab_ops::clear_unpinned(space_id, items)` when the shortcut fires.
- Expected: close the ids that are `Placement::Temporary` in that Space and not playing media, in input order, each once. Favorites, Pins and Temporary items of other Spaces stay. A selected item that plays media stays selected, as does a selected pin or favorite. If the selected item closes, nothing is selected afterwards.
- Cancellation/error: empty input, all pinned or all playing return an empty list. Duplicate ids close once. Work is linear in the input.
- Persistence/restart: the rule returns ids only; the adapter closes or archives them through the usual mutations.
- Proof: unit tests in `tab_ops.rs`. Shortcut, close, animation and undo need a real-build capture before any parity claim.
- Unknowns (simplest reading chosen): what Arc selects after the selected unpinned tab closes is not in the sources. The rule leaves the selection empty and the adapter picks the empty state, matching `clear_hidden_selections`. Whether closed tabs go to the Archive or are discarded is undocumented; the rule is agnostic. Cmd+Z undo is not modeled. Whether Little Arc windows, Split View members or tabs inside folders count is unknown; the model has no folder placement yet. Whether "playing" includes muted or paused media is left to the caller's `media_playing`.

## BC-008: Recent-tab switcher reaches the five most recent tabs

Parent: TAB-02, backlog "Keyboard shortcut behavior" and Next 15 item 13. Evidence: documented in Arc's [Keyboard Shortcuts article](https://resources.arc.net/hc/en-us/articles/20595231349911-Keyboard-Shortcuts) (Ctrl+Tab toggles between the five most recent tabs). Order, wrapping and bounds are `project requirement`. Rule implemented and tested in `crates/workspace/src/tab_ops.rs`; the switcher UI and shortcut do not exist.

- Initial state: one `RecentTabs` per window, filled by `touch(id)` each time the window shows an item.
- Action: each Ctrl+Tab press calls `cycle(1)`, Ctrl+Shift+Tab calls `cycle(-1)`. Releasing Ctrl selects the target and calls `touch(target)`. Closing or archiving an item calls `remove(id)`.
- Expected: the list is newest first and holds each id once. `cycle` moves a cursor through the first five ids and wraps. The first `cycle(1)` after a `touch` returns the previous tab, so one press toggles between the two latest. Older ids are unreachable. With fewer than two ids, `cycle` returns nothing.
- Cancellation/error: `touch` and `remove` end a cycle in progress; `remove` of an unknown id does nothing. Escape cancels by not calling `touch`. The list holds at most `MAX_ITEMS` ids, dropping the oldest. All functions are total, including extreme steps.
- Persistence/restart: not persisted. After restore the adapter rebuilds the list from the selected item, so the first press may find nothing.
- Proof: unit tests in `tab_ops.rs`. The switcher overlay and key release handling need a real-build capture.
- Unknowns (simplest reading chosen): whether Arc's list is per window or per Profile, whether it spans Spaces and whether it includes Favorites is undocumented; one list per window, any placement. Whether the first press shows an overlay or switches at once is unknown. Behavior when a tab closes while the switcher is open is not documented; the cursor resets.
