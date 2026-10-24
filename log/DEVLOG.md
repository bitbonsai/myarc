# Development log

Keep significant outcomes, failures and next actions reproducible. Newest entry first. No private exports, page titles/URLs from real browsing, credentials or raw user transcripts.

## 2026-10-24: Self-contained landing page and GitHub Pages

- Followed the requested `claude -p --model fable --effort high` design route. The trace identified `claude-fable-5-1` and invoked `/frontend-design`; the CLI timed out after writing the template and README. Retained its editorial layout, approved brand assets and truthful development-stage copy.
- Added `site/build.py` to produce one 442,041-byte HTML file with embedded font/images and full OFL/ISC notices. No runtime dependencies, analytics or executable JavaScript. Intro movement is finite; footer hover/focus retains the 40-BPM heart.
- Verification: static resource/link/rem guards and a deliberately invalid external-image self-check passed. Chromium rendered offline with no remote resource requests and decoded Outfit/images. Text clipping, 16/14-pixel body/minimum sizes passed at 320/390/768/1440; actual hover/keyboard focus and reduced motion passed. Inspected fresh desktop/mobile captures and a decoded 6.7-second motion/focus/reduced-motion recording; verified browser helper cleanup. Full-page stills taken during recording repeated viewport tiles, so they were rejected and recaptured outside recording. Native browsing remains outside this evidence.
- Published only the artifact to `gh-pages` at `1f9f608cbdb97f16acf3f67604893a562a003304`; GitHub verified author and committer as `bitbonsai`. Pages source is branch root, HTTPS enforced. Creation returned a non-JSON transport error; read-back confirmed success, avoiding duplicate publication. Preserved source font timestamps after finding conversion made otherwise unchanged builds differ; two successive builds now match byte-for-byte. Public HTTP 200 HTML matched the final local file exactly.
- Cloudflare access is blocked by an invalid stored credential and unauthenticated browser session. `myarc.dev` custom-domain binding, DNS writes and domain-certificate verification remain pending. Existing DNS and concurrent browser/reference work were preserved. Naming/asset rights, Safari/Firefox and screen readers remain unverified.

## 2026-10-24: Baseline comparison and live interaction polish

- Reproduced the custom-A regression: stroked chevron feet ended diagonally above the baseline. Replaced the stroke with a closed outline, matching original Outfit A cap height and the m/r baseline. Added `design/brand/alignment.html` with original/custom wordmarks at identical scale, spacing and guides; curved c overshoot remains unchanged. New geometry checks failed on the old asset and passed after correction.
- Standardized all social cards and wallpapers on `myArc, forever yours.` Shortened the lede to local workspace and optional AI. Replaced the footer status line with a GitHub-linked maker credit and an accessible Lucide outline heart; pinned ISC notice and source hash are retained.
- Removed page divider rules and increased spacing. Primary buttons are blue. Text links are blue at 90% alpha, becoming opaque while their underline moves 2 to 5 pixels over 200 ms. The personal blog supplied observed motion behavior, not copied site code. Footer hover/focus runs a single pulse every 1.5 seconds, 40 BPM; reduced motion disables movement and transitions.
- Fixed stale preview images with content-versioned URLs after rendering. Verified the actual internal browser's current DOM and hover state, in addition to the retained headless checks: 90% to opaque link color, 2-to-5-pixel underline, no layout shift and 1.5-second heartbeat. Static checks cover ten HTML pages, exact original-A alignment, icon path provenance and image revisions. Layout/type checks passed at 1440/728/390 widths; source hashes and document links passed. These remain branding checks, not native browser tests.

## 2026-10-24: Outfit, custom A and ivory grain

- Selected Outfit for the main brand page, wordmarks and six artwork compositions. Added a short ethos and four design principles. These express product intent, not implemented browser capabilities.
- Promoted the original barless-A drawing into shared, font-independent vector wordmarks. Other letters outline Outfit ExtraBold 800. Dark/light SVGs, UI marks and raster lockups share the same generated geometry; normal copy retains Outfit's standard A. Supplied ribbon PNGs remain unchanged.
- Inspected tiled noise treatment in archived home/developer pages, then generated a fresh seeded monochrome tile behind warm ivory. No archived texture or blue page background was introduced.
- Verification: regenerated six compositions and two lockups; checked shared five-path SVG geometry, font independence, grain dimensions/alpha/variation, nine HTML pages, original asset hashes and document links. Actual Outfit loading, 16-pixel body, 14-pixel minimum, image proportions and overflow passed at 1440/728/390 widths. Fresh desktop/mobile screenshots were inspected; browser helpers were closed.
- Tooling boundary: CairoSVG was installed but could not resolve its Cairo library. Reused installed fontTools and librsvg (`rsvg-convert`) for outlined wordmark generation/rasterization; no packages were installed. Native browser implementation and branding legal clearance remain outside this work.

## 2026-10-24: Typeface trial

- Added a separate `design/brand/type-study.html` with native radio controls for Outfit, Sora and current Manrope, plus an original SVG barless-A experiment. The supplied wordmark's exact typeface remains unidentified; Outfit ExtraBold is a visual candidate, not a verified match.
- Added unchanged OFL font binaries with license notices, source links and SHA-256 hashes. Main brand page, generated artwork and ZIP were preserved pending selection.
- Verification: all three fonts loaded and switched correctly at 1440/390 widths, with a 14-pixel minimum and no horizontal overflow. Native arrow-key selection passed; fresh desktop/mobile screenshots were inspected. Static branding checks cover nine HTML pages; document links and font hashes passed. No production browser behavior was tested.

## 2026-10-24: M0 motion study and pinned foundation build

- Starting source content: archived-reference baseline `6de61de`; concurrent README/brand work and GitHub attribution corrections were preserved.
- MA-001.a: implemented `design/onboarding/` as an original, dependency-free synthetic motion study. Six scenes include empty/error/1,000-pin and reduced-motion variants, local pin/Space/search practice, Back/Skip and replay. Study state is disposable, with no importer, persistent workspace or live page claimed.
- Evidence: six semantic Node tests and 40 agent-browser checks passed. Fresh source/artifact hashes, normal/reduced recordings and desktop/mobile captures are retained under `design/onboarding/evidence/`. Decoded recordings were inspected. No page errors or lingering browser helpers remained. Native timing, VoiceOver and formative tasks were not measured.
- Failed checks retained as lessons: agent-browser eval needs an async wrapper for awaited animation completion; reloading during capture produced a truncated video, so capture now replays in-place and checks duration/frame count; search input swallowed Escape, fixed with explicit dialog dismissal and focus restoration.
- MA-002.a: pinned Chromium `7ae71de816ba32868ff9f974c2716f04ebd9b860` and depot_tools, fetched outside the repository, and exercised dependency sync, hooks and GN generation. Fixed missing Git LFS, explicit depot_tools Python bootstrap, and a partial LiteRT checkout left by the first sync failure. Mirror LFS returned HTTP 405; scoped skip-smudge restored the exact clean source tree, with no generated build references to unused LiteRT prebuilt libraries. Global Git LFS configuration stayed unchanged.
- Build attempt two completed 1,155 steps before missing Metal Toolchain stopped ANGLE. Installed Apple's component `27A266a` and resumed incremental `chrome` build with eight jobs. Build/launch, extension lifecycle, Rust/native bridge, second-revision patch, isolation/recovery and signed distribution remain unverified. [Foundation recipe/run record](../browser/README.md) documents reproducible commands and boundaries.
- MA-003: no consented official export was supplied or private profile accessed. Coverage remains unknown; study fixtures are not Arc-format evidence. M0 stays open pending build acceptance, export evidence and named maintenance capacity.

## 2026-10-24 — Default-on blocking requirement

- Decision: ad and tracker blocking is P0 browser scope, enabled from the first page load in fresh Profiles, with Profile-scoped per-site exceptions and restore controls.
- Outcome: updated README commitments, product scope and PRV-01 evidence requirements. Added R18 for the Arc security FAQ; its 2024 effective date does not prove current-build defaults.
- Boundary: no blocking engine or extension selected or implemented. Require selected-Chromium compatibility, filter updates/failure handling and synthetic blocked/allowed/site-recovery evidence before release. CLI/MCP headed/headless support remains a proposal pending confirmation.
- Verification: relative documentation links and whitespace checks only; no browser blocking tests exist yet.

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
