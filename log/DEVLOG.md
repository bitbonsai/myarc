# Development log

Keep significant outcomes, failures and next actions reproducible. Newest entry first. No private exports, page titles/URLs from real browsing, credentials or raw user transcripts.

## 2026-10-24 — myArc copy and readable type

- Source: README branding, `myArc. Arc, forever yours.` Consulted `claude -p --model fable --effort medium`; the response identified `claude-fable-5-1`. Advice was read-only.
- Outcome: removed competing slogans, numbered labels and repeated explanation. Applied myArc casing to the board, preview, six compositions and both raster lockups. Final board headline is `myArc, forever yours.` with one README-based project lede. Original logo files and technical filenames stayed unchanged.
- Verification: regenerated artwork and inspected fresh desktop/mobile screenshots. Actual body/minimum font sizes were 16/14 pixels at 1440/390 widths. Static checks enforce rem font sizes, minimum size, casing and old-copy removal across eight HTML pages. Image dimensions/proportions, local links, documentation links and whitespace checks passed.
- Preview boundary: loopback-only local server for internal preview, no public deployment. Internal tab loaded the correct page title, but reported its pane hidden; internal screenshot capture timed out. Separate agent-browser captures supplied visual inspection. Asset-rights clearance remains outside this change.

## 2026-10-24 — Standalone brand material

- Starting revision: `9f7bec2`; browser implementation remains unstarted.
- Outcome: added `design/brand/` with a responsive identity board, unchanged user-supplied matte/glass PNGs, six editable HTML compositions, two transparent lockups and eight raster mark sizes. Used OFL-licensed Manrope instead of archived reference fonts.
- Evidence: `bash design/brand/export.sh` captured six PNG compositions after font/image decoding and checked the board at 1440/390 pixels for horizontal overflow and distorted image ratios. `python3 design/brand/verify.py` checks eight HTML pages, composition dimensions/content and ten transparent derivatives. Original attachment byte comparisons passed; desktop/mobile previews had no console errors. ZIP integrity and repository documentation links passed. These are branding checks, not application tests.
- Boundaries: no vector redraw, favicon approval, print-production certification, legal clearance or public deployment. Naming and source-logo rights remain open. Existing research edits were left untouched.

## 2026-10-24 — Product principles and Artcraft boundary

- Starting revision: `1ccb972`; no application implementation.
- Outcome: recorded open development, native Chromium plus Rust ownership, Arc-familiar interaction, local workspace ownership, and agent-ready control as design commitments rather than shipped claims. Excluded a browser-inside-browser platform from current scope and added the project-site visual direction.
- Artcraft boundary: retained useful public engineering methods without assuming its open-source work exists to drive traffic or that its repositories have a verified relationship to a commercial AI service. myarc remains useful without paid AI.
- Verification scope: documentation consistency, relative links, and whitespace only. No browser, importer, project site, or AI service was implemented.

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
