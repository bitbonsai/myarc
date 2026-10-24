# Behavioral parity register

Updated: 2026-10-24. **Every implementation row is currently not started.** This is a scope/evidence register, not a claim of product completeness. Each future implementation needs commands, behavioral checks, and end-to-end evidence. Screenshots alone cannot prove behavior.

Legend: `DOC` = public documentation/index evidence; `BRIEF` = project requirement; `UNKNOWN` = needs source/sample/observation. Historical docs may be stale; see [research](RESEARCH.md).

| ID | Behavior to specify | Priority | Reference basis | Evidence still needed |
| --- | --- | --- | --- | --- |
| UX-01 | Animated welcome, workspace reveal, guided practice, handoff | P0 | BRIEF; original [storyboard](ONBOARDING.md) | Native recordings, reduced motion, focus, frame traces, user tasks |
| WS-01 | Several Spaces can share a Profile; Profiles isolate data | P0 | DOC: Profiles | Shared/isolation browser tests and import mappings |
| WS-02 | Switch, reorder, rename, remove and restore Spaces | P0 | DOC: Spaces index; details UNKNOWN | Commands, deletion safeguards, keyboard behavior |
| TAB-01 | Profile favorites and Space pins/folders retain order/names | P0 | DOC: organization index; details UNKNOWN | Canonical pinned URL, reset, close, move, cross-Space behavior |
| TAB-02 | Temporary tabs, archive eligibility, timing and restoration | P0 | DOC: organization index; details UNKNOWN | Time model, active/media/form exclusions, recovery guarantees |
| CMD-01 | URL/search/tab/action command bar | P0 | BRIEF; details UNKNOWN | Keyboard navigation, input cases, active-Profile scoping |
| IMP-01 | Arc export preview and transaction; re-import/rollback | P0 | BRIEF; format UNKNOWN | Versioned adapters, synthetic corpus, interruption evidence |
| EXT-01 | Extension install/use/restart/update/remove | P0 | Chromium requirement; compatibility UNKNOWN | Supported platform matrix and real store route |
| PRV-01 | Ad and tracker blocking enabled by default | P0 | BRIEF; R18 documents Arc bundling uBlock Origin, not current-build behavior | Fresh-Profile first-load blocking; synthetic blocked/allowed requests; per-site exceptions and restore; Profile isolation; engine/filter compatibility, update failures and site recovery; not implemented |
| CORE-01 | Permissions, downloads, history, private browsing, DevTools | P0 | Inherit platform; integration UNKNOWN | Native integration smoke tests and private-data non-persistence |
| REL-01 | Crash recovery, versioned export, secure updates | P0 | BRIEF; project release standard | Fault injection and signed-update rehearsal |
| AUTO-01 | CLI and MCP for scoped tab/Space and real-page control | P0 | BRIEF; [control contract](CONTROL-PROTOCOL.md) | Both-interface journey and external client evidence; grants, stale references, cancellation, Stop control; not implemented |
| VIEW-01 | Peek and two-pane Split View | P1 | DOC: Split View index; details UNKNOWN | Navigation/focus, reopen, drag, dismiss, persistence |
| BST-01 | Appearance/CSS/hiding Boosts | P1 | DOC: Boosts index; BRIEF | Scope, reversibility, SPA/iframe rules, unsafe input checks |
| BST-02 | JavaScript Boosts and safe sharing | P2 | DOC: disable/release notes; BRIEF | Threat review, disabled imports, revocation |
| AI-01 | Off/local/user-key/hosted provider choices | P2 | BRIEF | Scope UI, secret handling, provider failure isolation |
| CONV-01 | Air Traffic Control, Little Arc, rich customization | Later | DOC: organization/release-note index | User value and exact behavior before scheduling |
| NO-01 | Easels | Excluded | BRIEF | Report omissions only |

## How to measure later

For each feature, track these independently: specified, implemented, exercised by automated checks, verified in an end-to-end journey, and observed in daily use. Report breadth and reliability separately. An implemented button does not establish workflow parity.

Do not publish a weighted overall score until the denominator, weights, evidence links, supported platforms, and unsupported behavior are documented. Initially report counts of verified journeys and open blockers. The current state is **zero demonstrated browser journeys**, not an invented percentage.
