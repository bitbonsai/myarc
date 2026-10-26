# Behavior contracts before implementation

Status: initial specification seeds, not verified application behavior. Each implementation task extends the relevant seed and resolves its unknowns before claiming Arc parity. Use original summaries, sources and synthetic examples. Never publish a real user's browser state here.

CLI/MCP browser control is a myarc product requirement, not a claim of Arc feature parity. BC-004 covers that additional behavior.

## Required fields

Contract ID and parent task; source URL/date and reference product/version/platform; evidence kind (`documented`, `observed`, `project requirement`, `proposed`, `unknown`); initial state; user action; expected state; keyboard/focus; persistence/restart; cancellation/error; required semantic and visual proof; unresolved questions.

Source evidence and our product decision must remain distinguishable. If no reference version or timing has been measured, write unknown rather than implying an exact reproduction. References are inventoried in [RESEARCH.md](../docs/RESEARCH.md).

## BC-001 — Spaces share a Profile without becoming one Space

Parent: MA-004/007. Evidence: Arc's publicly documented [Profile model](https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing); current behavior across supported Arc versions still needs confirmation.

- Initial state: synthetic Spaces S1/S2 reference Profile P1; Space S3 references Profile P2. S1/S2 have different pins/folders.
- Action: switch among S1, S2 and S3.
- Expected product behavior: S1/S2 keep separate workspace organization while sharing P1 browser data; S3 uses isolated P2 browser data. Switching must never briefly expose another Profile's search suggestions or page content.
- Keyboard/focus: retain a deliberate useful focus target; no unexpected command-bar activation. Exact shortcuts need a platform-specific specification.
- Restart: preserve profile references and workspace organization; runtime browser IDs are not persistent identities.
- Failure: invalid/missing Profile references are reported and recovered without silently merging browser data.
- Proof: pure-core ownership/serialization checks plus browser integration checks for cookies, history, extensions and command suggestions. Core checks alone cannot demonstrate browser isolation.

## BC-002 — Import success drives the reveal

Parent: MA-001/006/008. Evidence: project requirement and original [onboarding storyboard](../docs/ONBOARDING.md); historical Arc motion timings are unknown.

- Initial state: synthetic import preview with valid Spaces, folders and pins; nothing is committed and no imported URL has been opened.
- Action: confirm import.
- Expected behavior: validated commit completes before success/reveal. The reveal reads the committed workspace and uses stable IDs; it does not independently construct a decorative substitute workspace.
- Keyboard/focus: Skip remains available; reduced motion exposes equivalent information; handoff focuses a useful actual browser control.
- Restart: after commit, reuse that import receipt and saved workspace rather than importing again. Before commit, preserve the prior workspace.
- Failure: permission denial, malformed input, cancellation or disk failure shows a recoverable error without false success animation.
- Proof: semantic state/receipt checks, interruption cases, and a fresh real-build recording in normal/reduced motion. A prototype satisfies design exploration only.

## BC-003 — A pinned tab has explicit navigation and closure rules

Parent: MA-004/007. Evidence: project requirement; detailed Arc reset/close/multiwindow behavior remains unknown. This is a required investigation rather than a completed parity specification.

- Initial state: a synthetic named pin with a canonical URL and a currently navigated page.
- Actions to resolve: activate, navigate, reset, close its live page, remove its pin, move between Spaces, and restart.
- Required product invariants: removing a runtime page must not accidentally delete a persistent pin; changing the canonical URL must be deliberate; Profile boundaries apply to moves; failures must not lose the saved organization.
- Unknowns to settle before implementing: exact reset gesture/shortcut, when navigation persists, focus after closure, duplicate policy, and shared/multiwindow semantics. Record the chosen behavior and its evidence.
- Proof: a command transition table with final canonical/current URL and ownership for each action; persistence checks; browser focus/navigation checks. Do not infer correctness from a pin icon being visible.

## BC-004 — An authorized agent acts on a real page

Parent: MA-011/007. Evidence: project requirement and proposed [control contract](../docs/CONTROL-PROTOCOL.md); no implementation exists.

- Initial state: built-in AI off; synthetic site in tab T1; a different Profile and tab T2 outside the requested scope; no external control grant.
- Action: connect an external MCP client, obtain user approval for T1, inspect, fill/click, wait for a real DOM/state result, inspect/screenshot, then Stop control.
- Expected behavior: only granted context is visible/actionable; the website changes as expected; the browser shows the controller; Stop control rejects queued/new operations immediately. Repeat the same journey through the CLI and compare outcomes.
- Invalidations: stale references, navigation/redirect outside scope, permission denial, cancellation or disconnect produce explicit results and never automatic repeated submissions. Page instructions cannot expand the grant.
- Restart: expired/revoked grants never become silently active after relaunch. Ordinary local browsing remains available.
- Proof: semantic final state plus native browser recording and external-client trace, synthetic data only; record protocol/client/build versions. Listing MCP tools alone is insufficient.
