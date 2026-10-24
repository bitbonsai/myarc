# First implementation sequence

All tasks are **ready for specification or blocked by stated dependencies**, not implemented. Each task should become a small set of reviewable changes with evidence. Dates and priorities are in [ROADMAP.md](../ROADMAP.md).

## MA-001 — Design the animated first minute

Dependencies: product brief; final integration waits for MA-004/007. Begin immediately.

Deliver an original storyboard and motion study for welcome, source selection, import preview, workspace reveal, guided practice, and handoff. Use synthetic data. Capture the interaction qualities worth learning from permissible Arc references without copying assets. Include empty/error/large-import and reduced-motion variants.

Done: reviewable recording/prototype, state transitions, motion tokens, accessibility/focus behavior, original-asset provenance, and identified integration points. Final acceptance still requires the actual browser build under MA-008.

## MA-002 — Prove Chromium + Rust foundation

Dependencies: macOS build capacity, signing/update feasibility investigation, maintenance owner.

Build the unmodified complete Chromium candidate at an exact revision. Exercise a real extension and its lifecycle using the intended install route. Integrate one Rust-owned workspace command and a minimal native UI trigger through an officially supported bridge. Reapply the patch on another upstream revision. Compare a maintained derivative only if it demonstrably reduces total work.

Done: reproducible instructions, machine/build costs and times, extension evidence, interop error/thread/ownership checks, isolation/recovery smoke checks, patch/rebase record, dependency/license inventory, and accepted foundation decision. Unknowns remain explicit. If no affordable maintenance route exists, pause browser release targets and continue preservation/design work.

## MA-003 — Establish real export coverage

Dependencies: consented, privately held Arc export samples and relevant version/platform metadata.

Inspect official exports first. Inventory format signatures, schema versions, fields, hierarchy, profile relationships, and unsupported data. Create wholly synthetic counterparts. If a read-only local-profile adapter is necessary, specify the snapshot/permission workflow before accessing a profile.

Done: source/version matrix, field mapping, sample-independent synthetic fixtures, documented limits, and a route to preserve known fields. Do not claim cookie/password/session/extension-data transfer from the presence of URLs or extension IDs.

## MA-004 — Implement the Rust workspace and commands

Dependencies: MA-002 integration boundary; MA-003 findings inform schema. Schema/specification can begin earlier.

Implement stable IDs, Profile references, Spaces, favorites, pins/folders, temporary tabs, archives and import receipts. Add validation and the initial command registry. Keep runtime browser IDs separate from persistent IDs. Share the exact same model between preservation CLI and browser integration.

Done: meaningful semantic tests for many-Spaces-to-one-Profile, invalid ownership/cycles, move/reorder/rename, unsupported versions and round trips. The minimal command API is usable without a UI and matches [control contract](../docs/CONTROL-PROTOCOL.md).

## MA-005 — Make workspace storage recoverable

Dependencies: MA-004.

Choose a transactional or atomic-generation persistence mechanism based on failure behavior. Implement version migration, last-known-good recovery, portable export and explicit errors. Keep secrets/browser credentials and private-mode data out of workspace storage.

Done: process-kill and disk-full tests at relevant write boundaries; supported export/import round trip; known-good recovery; rejected newer format; no acknowledged command lost under the documented fault model.

## MA-006 — Release a local preservation slice

Dependencies: MA-003/004/005. Independent of completed browser UI.

Read selected supported Arc data, normalize it, show a preview/report, write portable myarc workspace and a lower-fidelity bookmark HTML export where feasible. Add idempotency/duplicate handling and target rollback. Do not open imported tabs or fetch favicons while converting.

Done: end-to-end local use, source integrity hashes, explicit omitted fields, cancelled/failed import recovery, deterministic synthetic-corpus outputs, and accurate public limitations. This is a preservation utility, not a browser replacement.

## MA-007 — Complete one real browsing journey

Dependencies: MA-002/004/005/006.

Wire the sidebar, browser Profiles and tabs, Space switching, pins/folders, archive/restore, and command bar to shared commands. Retain upstream permissions, downloads, private browsing, history, DevTools and extension UI access. Account-free startup is the only default.

Done: import → recognize workspace → open pin → switch Space → find tab/action with Cmd+T → close/relaunch and recover, plus a separate-Profile/private-mode journey. A simulated webpage or screenshot cannot satisfy this task.

## MA-008 — Integrate and finish onboarding animation

Dependencies: MA-001/004/006/007.

Use the real persisted onboarding/import states, shared components, stable item IDs and actual browser controls. Implement skip/resume/replay, keyboard/screen-reader behavior, and reduced motion. Separate real import progress from the post-commit reveal.

Done: every [onboarding acceptance criterion](../docs/ONBOARDING.md) passes, with native-build recordings, a large synthetic dataset trace, restart/interruption evidence and formative task results. This is a release gate even if all browser plumbing works.

## MA-009 — Establish trustworthy distribution and daily use

Dependencies: MA-002/005/007; public release also needs MA-008.

Implement/rehearse signed distribution and authenticated updates. Publish the extension/OS compatibility matrix and security maintenance plan. Exercise profile isolation, untrusted imports/automation, renderer recovery, and safe rollback/recovery from update failure. Run the daily-use cohort only after these boundaries pass.

Done: [scorecard](../docs/SCORECARD.md) has build-specific evidence and a named maintainer/capacity plan. No P0 data-loss, isolation, private-data or updater blocker remains. Do not waive these because of Arc's deadline.

## MA-010 — Close the most valuable workflow gaps

Dependencies: MA-007; prioritize after safety blockers.

Add two-pane Split View, Peek and B1 Boosts with explicit focus, profile, navigation and reversibility rules. Complete recordings and daily-use tasks. Keep full JavaScript sharing and AI services out of this critical path.

Done: working journeys, persistence/restart behavior where applicable, safe scope/permission checks, no core-gate regression, and updated parity rows. Afterward select optional AI adapter and commercial pilot work from demonstrated demand.

## Implementation handoff

Read [AGENTS.md](../AGENTS.md), then start with MA-001 design work and MA-002/003 feasibility/data investigations as resources allow. First report the available platform/toolchain and data sources. Do not start by scaffolding a broad UI, a paid AI backend, or a replacement browser engine. Close tasks only with the evidence above.
