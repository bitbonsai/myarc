# Architecture and foundation decision

## Recommendation, not a completed decision

Use Chromium's complete browser implementation with a deliberately small, maintained patch set for the myarc experience. Preserve browser-layer services rather than assuming a rendering engine also supplies extensions, profiles, downloads, password storage, and updates. Prove this recommendation during M0 before committing the implementation.

**Rust for the myarc-owned core:** workspace model, Arc parsing/normalization, workspace persistence, and command validation. Keep these testable without a running browser. Chromium's rendering engine, browser services, native UI integration, and existing C++ implementation remain upstream code. First-party WebUI may use TypeScript where appropriate; animation must be proven in the actual chosen UI stack.

This adapts Artcraft's Rust-first engineering without pretending a complete Chromium browser can become a pure-Rust application. Chromium documents supported Rust integration and interoperation tools. At the selected revision, follow those tools and GN rules instead of inventing a separate embedding runtime. Sources: [research](RESEARCH.md).

## Native shell, current implementation priority

Target 1:1 Arc visual parity, beginning with the empty/new-tab window and real browsing. The material extends across the sidebar and empty content canvas; address/navigation controls belong inside the sidebar. Ordinary pages retain upstream default canvas handling and authored CSS backgrounds. Match the reference at equal window dimensions before accepting a state.

The [current experiment](../browser/experiments/native-shell.md) keeps Chromium browser services and reuses its real controls in a custom layout, with AppKit behind-window material through Objective-C++. It runs with sidebar address controls and a translucent empty canvas; native keyboard navigation passed. Visual parity remains incomplete. Stock vertical tabs and the built-in glass flag did not establish the required appearance. A separate Swift or Tauri host is an alternative to investigate only if this native boundary blocks fidelity; neither embedding route has been proven here. Import work is deferred while this visible slice is built. Release/security gates remain in force.

The [native New Tab palette](../browser/experiments/native-command-palette.md) now handles user-triggered Cmd/Ctrl+T separately from programmatic tab creation. Its temporary Views state calls Chromium navigation, tab activation and the current Profile\'s existing history service; private windows skip regular-history queries. Appearance uses the browser color provider. The palette is not yet connected to multi-Space workspace state. Full omnibox completion and native acceptance of current-tab editing remain open.

Sidebar hide/show must keep the inner controls at their expanded layout width and translate them behind a clipped viewport. The outer reserved width may animate; shrinking the child layout causes tile and text reflow. Left-edge hover temporarily overlays the hidden sidebar without resizing the page; persistent expansion reserves its width. [The motion investigation](../browser/experiments/native-sidebar-motion.md) records the supplied-video evidence and pending native checks. The optional [saved-page fade](../browser/experiments/native-pinned-site-fade.md) uses the existing native compositor for a short incoming transition, with reduced motion keeping direct switches. Its legacy-pin route has manual native evidence; Rust saved-category routing remains unaccepted.

The optional Rust sidebar now has [manual native evidence](../browser/experiments/native-workspace-command-safety.md) for switching two real pages across Spaces and hiding both in an empty Space. [Fresh native acceptance](../browser/evidence/workspace-empty-header.json) also covers a neutral empty header, background loading and populated-page control restoration. It stays off by default while broader command/lifecycle acceptance remains open. Chromium owns live pages; the shared Rust model owns placement and per-window Space selection.

The [native row/footer slice](../browser/experiments/native-sidebar-row-footer.md) projects Profile Favorite tiles, the Space heading, Space Pins, then New Tab and Temporary rows. Each row owns its full-width background; a separate inset close chip retains a 32-DIP keyboard-accessible target. The Library footer menu calls existing browser Downloads and shared Space actions. It introduces no second workspace policy or storage path. Bounded native pointer/focus, app-only reduced motion and same-profile clean relaunch passed; full native suites and release gates remain open.

The [recovery-anchor fix](../browser/experiments/native-recovery-anchor.md) gives the opt-in shell a noninteractive BrowserView-owned popup anchor at the existing fallback coordinates, independent of sidebar visibility and startup element registration. [Manual native evidence](../browser/evidence/recovery-anchor.json) covers popup startup and hide/resize, with button invocation. [One explicit clean restart](../browser/evidence/workspace-clean-restart.json) retained native pages, Spaces and stable IDs. Crash restoration and durability remain unaccepted. The [two-process developer checkpoint/verify probe](../tests/fixtures/browser/README.md#two-process-workspace-crash-probe) now compares real fixture-page projection with retained on-disk workspace IDs and a form canary. Its local tests use API doubles only; the parent owns actual crash/relaunch/Restore and native acceptance. It neither controls browser lifetime nor exposes a product control endpoint.

## Candidate comparison

| Foundation | Why evaluate it | Main cost / disqualifier | Decision posture |
| --- | --- | --- | --- |
| Chromium browser layer + small patch stack | Preserves the existing browser and extension implementation | Large builds, native UI work, packaging, frequent rebases, service integration | Recommended M0 baseline; no claim of automatic end-to-end compatibility |
| Maintained Chromium derivative, e.g. Helium | Existing privacy choices, patches, packaging, onboarding, updater experience | Extra upstream dependency; inherited product assumptions; Helium-specific GPL-3.0 obligations | Compare effort saved with update lag, patch overlap, and license fit; do not copy before review |
| Electron | Fast UI iteration and familiar tooling | Its official documentation explicitly excludes arbitrary Chrome Web Store extension compatibility | Suitable for a disposable motion study, not the default production foundation |
| CEF or another Chromium embedding | More shell control than a browser fork | Must prove browser services, complete extension lifecycle, installation, and maintenance; embedding is not a compatibility guarantee | Only reconsider if a short working experiment beats the complete browser baseline |
| Gecko / Firefox derivative | Mature alternative engine and existing browser services | Chrome extension compatibility is a separate requirement requiring demonstrated adapters/testing | Alternative only if the product explicitly revises its extension requirement |
| WebKit platform webview | Strong system integration on some platforms | A webview is not the Chrome extension platform | Not selected for this brief |
| Servo or another emerging engine | Interesting longer-term embedding/research direction | Not a demonstrated drop-in Chrome browser/extension platform for this project | Watch; no v1 dependency |

Evidence: [research](RESEARCH.md). “Not selected” is a project judgment, not a claim that a foundation is unsuitable for every browser.

## One-week foundation experiment

Produce a decision record containing:

1. Exact candidate revision, all patches, toolchain, machine/runner specifications, disk/RAM/build times, and repeatable commands. Build/run the unmodified candidate before modifying it.
2. One small sidebar/command integration invoking a real Rust workspace command through a narrow supported bridge, then apply it to a second upstream revision. Record conflict surface, threading/ownership/error handling, and rebase effort.
3. A real Manifest V3 extension installed by the intended user route, used on a page, restarted, updated, disabled, and removed. Include a password-manager/native-messaging case where permitted. Record any dependencies on vendor software or store access.
4. Profile isolation and private browsing demonstrations, a renderer crash/relaunch, a workspace restart, and a packaging/update feasibility check.
5. An upstream/license inventory and a credible maintenance plan with an identified owner, build capacity, and expected recurring cost. Do not infer ownership exists from this document.

If store access, native messaging, codecs, DRM, password-manager integration, or distribution signing is unproven, label it unproven. Do not promise “all Chrome extensions” from the Blink engine alone. Stop the browser launch schedule if this experiment cannot establish an affordable support path.

## Rust integration contract

`myarc-workspace` contains a dependency-free, bounded model for multiple Spaces under one explicit Profile, generated stable IDs and per-window selection. Temporary items, Space pins and Profile favorites have distinct lifetimes. Closing a saved page retains its identity and canonical URL. Explicit unsave reports whether a live page remains; reordering stays within an ownership section. Rust returns owned sidebar snapshots and validates typed snapshot replacement before changing state. The [semantic tests](../crates/workspace/tests/semantics.rs) exercise these rules.

Native session restoration and stable-ID binding must finish before the adapter reconciles unbound temporary items. Zero model windows is valid between browser windows. The browser validates and canonicalizes saved URLs before crossing the Rust boundary; the dependency-free core applies bounded scheme/credential checks rather than a second URL parser. Snapshot acceptance proves in-memory validation, not successful disk persistence.

The [native storage retry candidate](../browser/experiments/native-storage-retry.md) renders state-specific actions through SidebarView's delegate. BrowserView defers dispatch, rechecks the current state and invokes the existing exact-Profile service retry API. Storage, schema and native recovery barriers remain unchanged; native acceptance is pending.

A base-only native lifecycle boundary dispatches restore/session-reset/quit events to an existing, exact-Profile adapter without constructing workspace services for other Profiles. Startup launch, pending native recovery choice and native restorers hold a counted barrier; window creation alone cannot release it. The [recovery choice candidate](../browser/experiments/native-recovery-lifecycle.md) retains its exact-Profile owner across consent and dialog deletion until Restore starts the real restorer, or discard/cancellation releases readiness. Weak, non-nestable completion callbacks reconcile after native constructor/destructor stacks return. Session resets invalidate cached metadata emissions and defer writes until binding completes. The boundary executable checks registration and counter behavior; actual native restore, cancelled quit and storage scheduling remain unaccepted.

The native M0 control still exercises the original rename interface. Profile-owned storage and the new sidebar presentation are integration candidates; actual native Space projection and restart behavior remain unaccepted. Cargo and Chromium GN use the same core source. MA-004 still includes unimplemented folders/archive rules and broader preservation behavior. `myarc-import`, `myarc-commands`, `myarc-store`, the CLI and `xtask` remain proposed until they have real functions. The browser owns actual Profiles and page lifetimes; Rust owns workspace state and validated operations.

Use a minimal, versioned data-oriented C++/Rust interface with explicit ownership, thread affinity, cancellation, errors, and bounded buffers. The M0 experiment uses the pinned revision's supported Cxx/GN route and a Views toolbar control. Ownership and synchronous error handling are exercised; full production interface versioning and asynchronous cancellation remain open. See [the build and interoperation record](../browser/README.md). No Rust panic may unwind across the boundary. Unsafe interoperation must be isolated, documented, reviewed, and exercised by integration checks. Safe core crates forbid unsafe code.

The same core must serve the standalone preservation CLI and browser integration. Document how its dependencies/toolchain are pinned and built under Cargo for standalone work and Chromium GN for the integrated build, without maintaining two versions of the logic. If this becomes prohibitively costly, record the evidence and revise the interface/sequence before changing the Rust-first direction.

## Boundaries

| Layer | Responsibilities | Boundary |
| --- | --- | --- |
| Browser platform | Rendering, sandbox, processes, network, browser profiles, permissions, extensions, downloads | Upstream services remain authoritative; minimize patches |
| Workspace model | Stable IDs; Spaces; profile references; folders; pins; temporary tabs; archive; window and split references | No profile cookies/passwords in workspace JSON |
| Commands | Validated workspace mutations and authorized browser actions | UI and test automation use the same command paths |
| Control service and adapters | CLI, MCP, scoped real-page observation/interaction, session grants, cancellation and Stop control | Browser-side authorization; common commands and errors; native page adapters reuse upstream mechanisms without exposing unrestricted debugging |
| Persistence/import | Versioned snapshots/journal; transactional migration; portable export | Reads user-selected sources; never writes into the Arc profile |
| Presentation | Sidebar, command bar, Peek, splits, animation, accessibility | Renders state; does not independently persist or alter import results |
| Optional providers | AI adapters and hosted convenience clients | Browser startup, local work, export, and recovery do not require them |

Reuse the upstream browser profile, tab, password, and security machinery. Do not build parallel stores that can disagree. Preserve a mapping between workspace IDs and browser tab/profile IDs across restore without assuming runtime tab IDs are stable.

Browser control is a first-class product surface. CLI and MCP adapters sit outside the pure Rust workspace core and call the same control service. Pure core/preservation work stays usable without a running browser; page actions require a live authorized browser adapter. Built-in/hosted AI does not own this boundary or receive extra privileges. Follow [CONTROL-PROTOCOL.md](CONTROL-PROTOCOL.md).

## Workspace schema proposal

`Workspace { schemaVersion, profiles[], spaces[], nodes[], favorites[], archives[], windows[], preferences, importReceipts[] }`

Each Space references one Profile. More than one Space may reference the same Profile. Favorites belong to a Profile; pins/folders belong to a Space. Nodes carry stable IDs, ordering, title, canonical/pinned URL where relevant, and hierarchy. Split references are explicit and must remain within one Profile. Enforce acyclic folders, bounded nesting, valid ownership, safe URL schemes, and referential integrity.

Keep private browsing data out of persistent workspace/archive storage. Account tokens, API keys, and passwords use OS/upstream credential storage, never plaintext workspace exports. Pin/reset behavior, archive eligibility, and multiwindow semantics need explicit behavioral specifications before implementation.

## Durability and performance

Select a persistence mechanism after testing crash boundaries (transactional database or atomic snapshot + journal). Persist successful commands before acknowledging completion; maintain a last-known-good generation; version migrations; provide backup/restore. Reject unsupported newer formats instead of partially overwriting them.

Keep dormant tabs lazy; virtualize long sidebars; build command-bar indexes per Profile; cap parser allocations; avoid a new renderer for every hidden pin. Benchmark relative to the same upstream revision and workload, not unrelated machines or Chrome marketing numbers.

## Repository topology

Keep product docs, Rust core, and myarc patches in this small repository; acquire the large upstream checkout outside it. `browser/` now contains the feasibility candidate/tooling pin, GN arguments and [build/run record](../browser/README.md). `design/onboarding/` is a disposable synthetic HTML/CSS/JS study with storyboard state, not production workspace logic. Its browser harness lives in `tools/check_onboarding.py` and writes ignored local artifacts.

The public [landing page](../site/README.md) is a separate static presentation boundary, with its visual system and preference-driven light/dark themes documented in [design.md](../design.md). `site/build.py` embeds approved brand images, variable Outfit and license notices into the standalone homepage; blog pages use shared content-hashed first-party assets and retain their notices. Post image processing generates bounded WebP/responsive derivatives while keeping original sources out of deployment. `site/discovery.py` exports rendered main content as Markdown and generates `llms.txt`/`llms-full.txt` from published pages only. SEO metadata, schemas and local targets are checked before publication; GitHub Pages supplies negotiated gzip. `site/blog.py` renders approved Markdown posts and copies only their referenced screenshots into generated `site/blog/`; drafts are excluded by default. The first image supplies post social metadata; an optional validated thumbnail override changes only the index image. Image-free posts keep the default brand social preview. Markdown/YAML parsing is build-time only, raw HTML is disabled, and local image paths and sizes are bounded. `site/publish.py` deploys artifacts without source/drafts and replaces the owned blog subtree to remove unpublished content. Image-bearing articles use a small content-hashed first-party script for native-dialog lightboxes; image links still work without it. Adjacent figures form two-column galleries, with captions retained. Dialog UI stays outside exported main content. Generated pages need no client-side router or runtime service. An optional [Markdown edge Worker](../site/edge/README.md) negotiates generated Markdown through `Accept` while GitHub Pages remains the origin. It is active on `myarc.dev/*`; dedicated HTTP checks verify HTML and Markdown bodies against published artifacts. GitHub Pages serves an artifact-only `gh-pages` branch. Hosting and DNS are independent of local browser operation and do not prove product implementation.

`crates/workspace/` is the standalone safe core. `browser/foundation/` holds the GN/Cxx integration; An external Chromium checkout links `src/myarc` to that directory, so there is no second copy of owned logic. Its `myarc` branch holds upstream-file changes; `browser/patches/chromium/` exports them as one patch per file. Standalone Rust formatting/lint/tests and the dependency guard have a GitHub workflow; native Chromium builds remain local evidence. `tests/fixtures/arc-markdown/` contains an invented structural counterpart to a private export sample; it defines no production parsing or Profile rules. `tests/fixtures/browser/` and `tools/check_browser_foundation.mjs` exercise native browser runtime with synthetic local data and a marked, developer-controlled profile. That client is separate from production CLI/MCP authorization. Its [workspace-pin probe](../tests/fixtures/browser/README.md#workspace-generic-unpin-probe) invokes a local MV3 worker through privileged test CDP and checks native pin projection against persisted Rust-owned identity; it introduces no web-message control path. [One real-browser run and native UI follow-up](../browser/evidence/workspace-saved-identity.json) establish generic Unpin preservation, explicit Unsave and saved close/reopen. The [blank-only material fix](../browser/experiments/native-page-background.md) clears shell overrides on ordinary-page navigation and detachment; [native background and saved-journey checks](../browser/evidence/page-background.json) passed. `xtask/` remains proposed.

The developer preview launcher uses the current compiled `out/MyarcM0` binary. Its default mode creates disposable profiles; explicit `--review` reuses one marked review profile and asks Chromium to restore its native session. The launcher preserves existing workspace files and preferences, rejects unmarked/symlinked review directories and refuses a matching live preview rather than forwarding to an older process. It adds no page-control endpoint and is separate from product CLI/MCP.

The two-revision build/bridge proof establishes a narrow integration route. Foundation acceptance still needs extension lifecycle, broader runtime boundaries, complete license/notices review and maintenance ownership. Keep browser services authoritative and replace study fixtures with shared Rust commands during product integration.
