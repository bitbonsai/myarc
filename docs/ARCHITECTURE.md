# Architecture and foundation decision

## Recommendation, not a completed decision

Use Chromium's complete browser implementation with a deliberately small, maintained patch set for the myarc experience. Preserve browser-layer services rather than assuming a rendering engine also supplies extensions, profiles, downloads, password storage, and updates. Prove this recommendation during M0 before committing the implementation.

**Rust for the myarc-owned core:** workspace model, Arc parsing/normalization, workspace persistence, and command validation. Keep these testable without a running browser. Chromium's rendering engine, browser services, native UI integration, and existing C++ implementation remain upstream code. First-party WebUI may use TypeScript where appropriate; animation must be proven in the actual chosen UI stack.

This adapts Artcraft's Rust-first engineering without pretending a complete Chromium browser can become a pure-Rust application. Chromium documents supported Rust integration and interoperation tools. At the selected revision, follow those tools and GN rules instead of inventing a separate embedding runtime. Sources: [research](RESEARCH.md).

## Native shell, current implementation priority

Target 1:1 Arc visual parity, beginning with the empty/new-tab window and real browsing. The material extends across the sidebar and empty content canvas; address/navigation controls belong inside the sidebar. Ordinary pages retain their explicit painted backgrounds. Match the reference at equal window dimensions before accepting a state.

The [current experiment](../browser/experiments/native-shell.md) keeps Chromium browser services and reuses its real controls in a custom layout, with AppKit behind-window material through Objective-C++. It runs with sidebar address controls and a translucent empty canvas; native keyboard navigation passed. Visual parity remains incomplete. Stock vertical tabs and the built-in glass flag did not establish the required appearance. A separate Swift or Tauri host is an alternative to investigate only if this native boundary blocks fidelity; neither embedding route has been proven here. Import work is deferred while this visible slice is built. Release/security gates remain in force.

The [native New Tab palette](../browser/experiments/native-command-palette.md) now handles user-triggered Cmd/Ctrl+T separately from programmatic tab creation. Its temporary Views state calls Chromium navigation and tab activation; Rust workspace rules are unchanged. Full omnibox completion and current-tab editing remain open.

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

`myarc-workspace` now contains the bounded M0 experiment: one transient Space and validated rename operation. It is compiled from the same source under Cargo and Chromium GN. The complete workspace model remains MA-004. `myarc-import`, `myarc-commands`, `myarc-store`, the CLI and `xtask` remain proposed until they have real functions. Keep the initial split small; do not create empty crates just to match a diagram. The browser owns page lifetimes and browser Profiles; Rust owns only myarc workspace state and validated operations.

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

The public [landing page](../site/README.md) is a separate static presentation boundary, with its visual system and preference-driven light/dark themes documented in [design.md](../design.md). `site/build.py` embeds approved brand images, variable Outfit and license notices into the homepage and shared blog shell. `site/blog.py` renders approved Markdown posts and copies only their referenced screenshots into generated `site/blog/`; drafts are excluded by default. Markdown/YAML parsing is build-time only, raw HTML is disabled, and local image paths and sizes are bounded. `site/publish.py` deploys artifacts without source/drafts and replaces the owned blog subtree to remove unpublished content. There is no executable JavaScript or runtime service dependency. GitHub Pages serves an artifact-only `gh-pages` branch. Hosting and DNS are independent of local browser operation and do not prove product implementation.

`crates/workspace/` is the standalone safe core. `browser/foundation/` holds the GN/Cxx integration; `browser/patches/` packages it for an external Chromium checkout. The patch-payload check prevents a second, drifting copy of owned logic. Standalone Rust formatting/lint/tests and the dependency guard have a GitHub workflow; native Chromium builds remain local evidence. `tests/fixtures/arc-markdown/` contains an invented structural counterpart to a private export sample; it defines no production parsing or Profile rules. `tests/fixtures/browser/` and `tools/check_browser_foundation.mjs` exercise native browser runtime with synthetic local data and a marked, developer-controlled profile. That client is separate from production CLI/MCP authorization. `xtask/` remains proposed.

The two-revision build/bridge proof establishes a narrow integration route. Foundation acceptance still needs extension lifecycle, broader runtime boundaries, complete license/notices review and maintenance ownership. Keep browser services authoritative and replace study fixtures with shared Rust commands during product integration.
