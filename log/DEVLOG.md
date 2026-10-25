# Development log

Keep significant outcomes, failures and next actions reproducible. Newest entry first. No private exports, page titles/URLs from real browsing, credentials or raw user transcripts.

## 2026-10-25: Command palette history and appearance

- MA-007.b2: combined bounded current-window tab matches with Chromium Profile history, using distinct switch/open actions. History stays local, stale queries are cancelled and private windows skip regular history. Rust was unchanged.
- Fixed mixed light controls and hardcoded dark surfaces. Palette colors now use browser roles, selected labels disable automatic recoloring, and material tint follows effective Profile colors rather than the system theme alone. Restored themed sidebar address backgrounds. Prototype new windows start at `about:blank`.
- Native build, frame/browser-command GN checks and five-source patch reproduction passed. A typed local history title reopened the actual fixture URL in one new tab. Explicit light Profile appearance was inspected on a dark system; a private dark palette showed no normal-history entries. Cmd+N produced one blank target. Captures contain invented data and remain private.
- An initial HoverButton paint failed the opaque-layer subpixel check; disabled subpixel rendering for those labels. Full autocomplete, cached favicons and live appearance switching remain open. Sidebar shortcuts, current-tab palette editing, motion and release recovery are still pending.

## 2026-10-25: Basic native New Tab palette

- MA-007.b1: routed interactive New Tab actions to a centered Views palette while retaining programmatic tab creation. Enter uses Chromium classification/navigation or activates a matching existing tab. Input and tab matching are bounded; Rust workspace rules are unchanged.
- Native build and browser-view GN header checks passed. On macOS, Cmd+T opened the palette without creating a page; submitting a local fixture URL added exactly one tab. Ctrl+T/Escape preserved page IDs and URLs. Selecting an existing fixture tab added no duplicate and restored page focus. Captured the actual window and a native open/close recording outside the repository.
- An attempted parent-view hide violated a layout visibility check. Kept the layout parent visible and hid native hosts instead, preventing clicks on invisible pages. The resulting crash also exposed a missing app-menu anchor in the existing recovery bubble. A fresh disposable profile allowed the palette checks to continue; recovery remains blocked.
- Packaged the seven-file palette delta separately from concurrent rounding work. Current-tab Cmd+L, sidebar hide/show and full autocomplete remain pending. Motion is recorded as a baseline; timing, reduced motion and full visual parity are unverified.

## 2026-10-25: Compact monthly blog rows

- Grouped previews by muted month/year labels. Moved each full date below its title and kept the complete abstract in the same text column beside the thumbnail, with CSS single-line ellipsis.
- Nine unit checks passed, including multi-post month grouping and date/title ordering. Retained browser checks passed across both themes and five blog widths, checking alignment, truncation and month-label contrast. The first light gray measured 4.47:1 and failed; the corrected gray measures 4.88:1 against the plain page surface, with dark mode at 5.99:1.
- Published `6019c32` to the artifact branch. Native work remains separate; Safari/Firefox remain unmeasured.

## 2026-10-25: Image-led post previews and Arc target reference

- Removed the visible blog introduction and retained a visually hidden heading for accessibility. Listings use a small, uncropped WebP thumbnail beside the title, derived from the first Markdown image.
- Post Open Graph/Twitter image URLs, alt text, MIME type and dimensions now follow that first image; structured article data uses the same image. Image-free posts retain the default brand card. Added coverage for first-image ordering, encoded filenames, fallback previews and thumbnail removal when unpublishing.
- Replaced the initial blog screenshot with the specifically approved Arc reference and labelled it as the target appearance, separate from the myArc prototype. Recorded provenance and ownership limits in attribution.
- Eight unit checks and retained homepage/blog browser checks passed, including two themes, mobile widths, metadata/image agreement and non-overlapping thumbnails. Published `0ec3936`; normal TLS read-back matches the index, article, original reference and 5,526-byte thumbnail. Social-platform rendering/cache refresh and Safari/Firefox remain unmeasured.

## 2026-10-25: Markdown progress journal and first public post

- Added build-time CommonMark/YAML posts with explicit publication approval, local validated screenshots, a date-sorted index and per-post canonical/social metadata. Homepage and blog share the existing design/theme shell. Generated output excludes drafts and unreferenced files.
- Added a checked-in artifact-only publisher. It preserves domain/HTTPS settings and unrelated root artifacts, updates without force and replaces the generated blog subtree so unpublished posts and removed images disappear. The current post was explicitly approved for publication.
- Seven real parser/filesystem/publisher unit checks passed, including publish-to-draft removal, unsafe HTML/link handling and image path boundaries. Retained homepage checks passed; blog index/article checks passed at five widths in both themes, with keyboard focus and reduced motion. A first run caught hidden mobile navigation caused by selector specificity; corrected the selector and retained the visibility check.
- Published `5e8d269` to the Pages artifact branch. Normal HTTPS read-back matches the homepage, blog index, first article and screenshot byte-for-byte. Four cited public source URLs returned anonymous HTTP 200. Safari/Firefox, screen readers and social cache refresh remain unmeasured. Native-browser work stayed outside this slice.

## 2026-10-25: Native header and New Tab control refinement

- Found stock horizontal padding shrinking the custom sidebar header and overlapping its controls. Removed that padding for the prototype, centered the navigation buttons and reused the native dock-to-left glyph. Kept the existing navigation callbacks and security indicators.
- Matched the unfocused address surface to the dark reference palette while retaining high-contrast colors. Exposed the native New Tab label, removed its persistent tile background and placed it above the tab list. A native mouse click added exactly one page; Cmd+L navigation still passed.
- Native build passed after correcting an empty-border API argument. Refreshed the private native capture and optional patch. Full sidebar structure, centered command bar and other window states remain open; visual parity is not accepted.

## 2026-10-25: Native material and sidebar navigation iteration

- Current priority changed to native Arc visual parity, including the empty content canvas. Import investigation and broad test expansion are deferred; release safety gates remain open. [MA-007.a](../plan/EXECUTION.md#ma-007a-native-visual-parity-first-state) defines the first review state.
- Stock vertical tabs and the built-in glass flag did not establish the required appearance. The [optional native patch](../browser/experiments/native-shell.md) uses explicit AppKit behind-window material and moves existing browser navigation/address controls into the sidebar. A real page and a transparent blank canvas now run in the same Chromium candidate. No additional host framework was introduced.
- Native builds passed. Mixed web/native toolbar controls initially caused startup crashes; native controls fixed that configuration. Address-field visibility churn then lost keyboard focus; preserving visibility fixed Cmd+L navigation. Captured a real page and return to the translucent empty canvas. A controlled colored backdrop changed the native material across the window.
- The packaged patch reproduced nine baseline sources exactly. Captures and supplied references remain private. Sidebar contents, centered command bar, complete control styling and other window/accessibility states remain unverified or unimplemented; 1:1 parity is open.

## 2026-10-25: Fable redesign, HTTPS recovery and system themes

- Fable/high (`claude-fable-5-1`, frontend-design Skill) replaced the left-label column layout with headings above content, larger body text and a compact progress panel with dedicated artwork space. Header/footer lockups have 1rem ribbon marks. Parent aligned section/subheading sizes, retained ink navigation without underlines, and verified all six public documentation links plus an actual Roadmap click.
- Domain provisioning stalled despite valid DNS. With explicit permission, removed/re-saved the Pages custom domain. GitHub automatically created Delete/Create CNAME maintenance commits; the branch-change guard caught this side effect, and read-back confirmed unchanged landing HTML. Certificate became approved for apex and www. Normal TLS, exact deployed bytes, enforced HTTPS and the www redirect then passed.
- Added canonical/social metadata, a public 1200 × 630 preview, favicon files, WebSite JSON-LD and crawler files. Website runtime assets remain embedded. These changes are published independently of unrelated native-browser source work.
- Added CSS-only system light/dark preference support, native wordmark source switching, theme-aware browser colors and contrast-adjusted dark links. Added Lucide GitHub icons to both CTAs with the retained ISC notice. [Branding system](../design.md) records actual colors, typography, spacing and motion.
- Retained website checks pass at six widths. Fresh light/dark desktop and dark mobile captures were inspected; sampled dark text/link contrast passed AA. Focus/hover and reduced-motion checks passed. Published `aa78b0b` serves exact generated HTML over HTTPS. Safari/Firefox, screen readers and search/social reindexing remain unmeasured.


## 2026-10-24: Native MV3 and runtime smoke checks

- MA-002.d: user installed uBlock Origin Lite `2026.1021.1354` in a newly marked native Chromium profile; native management reported FROM_STORE. Causal enabled/disabled/re-enabled checks and a normal process restart passed. Corrected a measurement mistake: uBlock redirects this XHR to a local noop resource, so a resolved fetch alone proves neither external network access nor blocking. Final response origin/path and a disabled control now determine the result.
- Renderer crash/reload preserved an acknowledged localStorage value with browser PID unchanged. Off-the-record storage was absent initially, isolated from normal state and absent in a fresh private context after disposal. Native Rust control accessibility/Space-key activation also passed on the rebuilt second pin. These checks establish neither production workspace durability nor full cookie/history/private-data isolation.
- Retained [the developer regression client and fixture](../tests/fixtures/browser/README.md), with version/PID/profile-marker preconditions and a changed-PID restart requirement. Positive scenarios ran against the compiled candidate; wrong PID, unmarked profile and unchanged-PID restart were rejected. Source fixture bytes served by the local server were checked against the repository.
- New failure: default-context `Target.createTarget` aborted the browser on a blocking-disallowed assertion in `Profile::CreateProfile` on the UI thread. Its trigger and upstream-versus-patch origin remain unisolated. Existing-tab tests avoid that path while preserving the failed case; thread checks remain enabled. Extension update/removal, native messaging, duplicate ANGLE class warning and distribution/licensing/ownership gates remain open. [Structured runtime evidence](../browser/evidence/runtime.json) keeps private profiles, screenshots and raw logs outside the repository.

## 2026-10-24: First private Markdown export inventory

- MA-003.a: inspected a user-supplied Markdown sample read-only, with source fingerprint unchanged. Space headings, second-level named groups and ordered bare URLs are present. The file provides no producer/schema version, Profile IDs or tab titles. No URLs were opened.
- Created [an independent shape fixture](../tests/fixtures/arc-markdown/README.md) using invented labels and reserved `.test` URLs. Repeated `Favorites` groups retain an ownership ambiguity; equal URLs cannot establish shared Profiles or justify global deduplication.
- Verification covers relative links, fixture structure and separation from private source values. This is evidence preparation, with no production importer or semantic round trip claimed. Follow-up settings screenshot confirms the built-in export action; route is recorded in the fixture inventory. Producing-build binding, favorite-versus-pin scope and nested-folder representation remain open. Personal source bytes, fingerprint and screenshot stay outside the repository.

## 2026-10-24: Panel spacing, shared grain and custom-domain DNS

- Removed the progress panel's empty second column and paragraph/status width caps. Intro and status text now reach the same right inset as the left padding. Reserved lower artwork space without overlapping text.
- Reused the original grain through one embedded CSS variable on both ivory and navy backgrounds. Browser checks passed at 320/390/768/900/1024/1440, including exact inner-edge alignment, grain reuse and artwork separation. Existing offline assets, typography, focus/hover and reduced-motion checks also passed; inspected fresh desktop/tablet/mobile panel captures.
- Published the revised 442,021-byte artifact and domain CNAME to `gh-pages` at `3c229713f27a85d0f7c4b62d4424485ecc9f4f53`. Bound `myarc.dev` in GitHub Pages. The signed-in Cloudflare browser profile resolved the earlier access blocker; a fresh owned tab was needed, and read-only DOM inspection replaced timed-out accessibility snapshots.
- Imported only four apex A records from GitHub's current documentation and `www CNAME bitbonsai.github.io`, DNS-only with TTL 300. The zone initially had zero records. Authoritative/public DNS resolve correctly and the domain returns HTTP 200; certificate issuance and HTTPS enforcement remain pending. A bounded session monitor checks normal TLS, exact deployed bytes and HTTPS activation before reporting readiness.

## 2026-10-24: Chromium build and bounded Rust/native proof

- MA-002.a: unmodified Chromium `7ae71de816ba32868ff9f974c2716f04ebd9b860` built and launched. The successful resumed segment ran 06:40:26–09:03:03 UTC; full fresh-build time and peak memory/storage remain unmeasured. A synthetic local form and separate-user-data-dir localStorage smoke check passed. This establishes neither full Profile isolation nor private-mode safety.
- MA-002.b: added a dependency-free transient Rust workspace with validated rename, Cxx/GN bridge and opt-in native Views control. Six core tests passed under Cargo and GN; seven bridge checks include 1,000 ownership cycles. Accessibility activation and focused Space key changed Everyday to Research and back. Keyboard access required `FocusBehavior::ALWAYS`. The complete workspace model and persistence remain unimplemented.
- MA-002.c: packaged the source/toolbar patch and rebuilt it at the original pin and immediate child `8d208f20744ac6d4ce00cc3f9de6a14a9b8b926d`, without conflicts. Both runs executed six Rust tests and seven C++/Rust checks. The second sync/hooks passed. This PHP test-dependency roll gives limited update-porting evidence; UI rerun on the second rebuild remains pending. External checkout Git identity is separate from this repository's configuration; recorded commits verify both identities as `bitbonsai`.
- Added pinned standalone Rust tooling, core/patch CI and a payload check. Local format, clippy, six unit tests and dependency guard passed; deliberately stale source was rejected by the payload check. Remote workflow execution remains unverified. Canonical source hashes and scope are in [foundation evidence](../browser/evidence/foundation.json).
- Extension installation through Chrome Web Store was observed for uBlock Origin Lite `2026.1021.1354`. Discarded a driver reconnection that selected stock Chrome 151. A later disposable profile lacked extension files while native management reported ENABLED; its dashboard returned `net::ERR_FILE_NOT_FOUND`. Cause is unresolved. Function/restart/update/remove acceptance remains open pending a fresh native-launched profile.
- Licensing/package findings: inventory contains 326 entries and eight unresolved metadata/license errors; generated development credits are a sample page. The ad-hoc component app fails strict bundle verification. Duplicate `ANGLESwapCGLLayer` warnings remain unexplained. No binary is ready for distribution. Owner, backup capacity and recurring budget remain unspecified.
- Arc UI fixture preparation remains incomplete and produced no verified official export. Private media, browsing captures and local test profiles stay outside the repository. M0 remains open; no production browsing, migration, control-service or security gate was closed by these experiments.

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
