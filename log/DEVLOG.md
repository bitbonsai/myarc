# Development log

Keep significant outcomes, failures and next actions reproducible. Newest entry first. No private exports, page titles/URLs from real browsing, credentials or raw user transcripts.

## 2026-10-24 — CLI and MCP become launch scope

- Starting revision: `b799568`; no application implementation.
- Outcome: promoted browser control to P0, specified both adapters over one control service, added MA-011 and BC-004, and separated external-agent control from optional built-in/hosted AI.
- Required proof: actual page observation/actions through both interfaces, one external MCP client, scoped grants, stale/error handling and human Stop control. These remain implementation gates, not completed tests.
- Compatibility boundary: local stdio clients first; cloud MCP hosts require a separate authorized bridge. No unrestricted public debugging endpoint is introduced.
- Verification scope: documentation consistency and relative links. Next implementation work must prove a narrow live-browser control adapter alongside the core, without waiting for a paid AI service.

## 2026-10-24 — Planning workflow audit

- Starting revision: `60c00aa`; application implementation had not started.
- Outcome: compared public Artcraft readiness, development and scorecard practices; added behavior-contract seeds and working rules; separated standalone Rust work from browser-build feasibility; specified evidence freshness, no-op-control checks and regression retention.
- Verification scope: documentation link/consistency checks and workflow configuration review. Browser tests, Rust checks, benchmarks, visual recordings and generated scorecards remain unavailable.
- Source limitation: raw retrieval of some reference documents failed; the public GitHub file interface supplied the development guide and scorecard. No private shared standards were accessed.
- Next action: execute a bounded MA-001, MA-002 or MA-003 slice with actual motion, build or export evidence. For core work, follow the behavior contracts and corrected MA-004 dependencies. Avoid adding another broad planning layer before gathering that evidence.
