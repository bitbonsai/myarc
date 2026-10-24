# Roadmap

Updated: 2026-10-24. Stage: M0 feasibility. Synthetic onboarding study runs locally; unmodified Chromium build is being exercised. Browser implementation and foundation acceptance remain pending.

## Delivery strategy

Run two coordinated tracks: **preserve people's setups** and **build a dependable browser**. An export/conversion tool can be useful before the browser is ready; it must never be marketed as a browser replacement. Animated onboarding is in the launch scope, with its design explored immediately and its final quality validated in the real browser.

CLI and MCP browser control are P0 launch scope. Expose useful page observation/actions and tab/Space management to authorized AI agents independently of built-in AI providers. Design the shared command/grant model during M0/M1; integrate both interfaces in M2 and harden them through M3/M4. See [MA-011](plan/EXECUTION.md) and the [control contract](docs/CONTROL-PROTOCOL.md). This added launch scope requires re-estimating the target windows after the foundation experiment; dates are not expanded capacity.

Arc's official site says desktop access ends on **2026-12-22**. That creates urgency for preservation, not permission to skip release gates. Source and limits: [research](docs/RESEARCH.md).

## Milestones

Dates assume work starts October 24, access to a suitable Mac/build runner, and a funded maintenance owner. They are target windows to review after the foundation experiment; no staffing or capacity is presumed to exist.

| Milestone | Target window | Deliverable | Exit gate / response if missed |
| --- | --- | --- | --- |
| M0: Prove the foundation | Oct 24–30 | Exact upstream revision; clean local build; real extension install; one patch/rebase exercise; import sample inventory; animated onboarding storyboard and clickable motion study | Select foundation only with reproducible build, extension evidence, estimated upkeep, licensing, runner cost, and maintenance owner. If unavailable, continue preservation and motion prototype; do not announce a browser launch. |
| M1: Preserve and welcome | Oct 31–Nov 13 | Local export reader/converter; versioned portable workspace; preview/report; synthetic fixtures; animated onboarding prototype using the real workspace state model | Proven no source modification; repeatable conversion and round-trip export; unsupported data reported; welcome and workspace reveal reviewed in normal and reduced motion. |
| M2: Desktop alpha | Nov 14–27 | macOS Apple Silicon alpha with sidebar, Spaces/Profiles, tabs, command bar, recovery, initial import, animated onboarding and scoped CLI/MCP page control | Core journeys and the same synthetic-site control journey through both interfaces pass; profile isolation, crash recovery and extension smoke tests pass; signed internal distribution works. Distribution remains limited and experimental. |
| M3: Daily-use beta | Nov 28–Dec 11 | Durability hardening; two-pane Split View and Peek; CSS/appearance Boosts; accessibility; updater; measured extension matrix | Complete [scorecard](docs/SCORECARD.md) on supported hardware. No data-loss, isolation, or updater blocker. Optional features can slip. |
| M4: Release decision | Dec 12–18 | Candidate build plus compatibility, migration, accessibility, update, recovery and external-agent control evidence | Ship only after all P0 gates pass, including animated onboarding and MA-011 CLI/MCP control. If gates fail, publish preservation tooling and an honest progress report, not a replacement claim. |
| M5: Sustain and expand | After launch gates | Security cadence; verified Windows/Linux builds; opt-in AI providers; paid hosted service pilot | Each platform and service earns its own support promise; desktop account-free operation stays independent. |

## Critical path

Foundation proof → workspace/profile integration → import transaction and durable storage → core browsing → secure updates and daily-use evaluation → release. Onboarding design starts during M0; its integration depends on workspace/import contracts. It is a separate release gate, not spare-time polish.

The CLI/MCP control path is a release gate alongside the browsing and onboarding journeys. Built-in AI providers and cloud sync do not block browser launch; connecting an external agent is independently optional for each user. Easels are excluded. Full JavaScript Boost sharing, complex splits, Live Folders, mobile, and multi-device sync follow measured demand.

## Scope fallback

If the eight-week browser target is unrealistic, preserve an M1 release: local Arc export conversion, portable workspace, import coverage report, and a local preview. It should help users keep their data and, where formats permit, export ordinary bookmark HTML for another browser. No claim that passwords, cookies, or extensions have been migrated without evidence.

## Review cadence

At each weekly review, update the evidence register, open blockers, upstream version lag, available capacity, build/service costs, and target windows. Change scope or dates explicitly. Never replace missing measurements with guessed parity percentages or agent-hour multipliers.
