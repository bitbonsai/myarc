# Product plan

## Audience and job

Serve desktop Arc users who have invested in Spaces, folders, named pinned tabs, and familiar shortcuts, and want that way of working to remain dependable. The first job is to continue an existing browsing life with little relearning or data loss.

**Positioning hypothesis:** familiar spatial browsing, a delightful first minute, and ownership that survives the business behind it. Validate this with actual Arc users before claiming demand. This is narrower and more defensible than a general-purpose AI browser.

## Requirements carried into this plan

The project brief calls for account-free browsing, ad and tracker blocking enabled by default, Chromium extension compatibility, Arc settings import, an onboarding experience with meaningful animation, Boosts, no Easels, entirely optional AI through local models/user keys/a paid hosted service, and browser control through CLI and MCP for AI agents. These are product constraints. The technical choices and dates below are proposals, not prior decisions.

## Design commitments

These commitments guide product and design tradeoffs. They do not describe shipped features.

- Open source. Publish code, build instructions, and license information. Keep myarc-owned work permissively licensed without implying that Chromium or other upstream components use the same license.
- Native desktop browser. Build on Chromium's complete browser layer with a Rust-owned workspace core. Use Chromium WebUI where it fits; do not use Electron as the production shell or describe the full browser as pure Rust.
- Familiar from day one. Preserve Arc's useful layouts, shortcuts, Spaces, and workspace habits. Use onboarding to explain differences rather than ask people to relearn browsing.
- Your workspace, your machine. Keep workspace state local, exportable, and usable without an account. Do not upload pages or workspace data to AI services in the background. Normal website traffic still uses the network.
- No behavioral tracking. Do not send browser-usage analytics or create per-install tracking identifiers. Aggregate release-host download totals may measure interest without instrumenting browsing. Keep operational website/download-host logs separately disclosed. Audit Chromium defaults and network behavior before claiming release compliance.
- Default-on blocking. Block ads and trackers from the first page load, with Profile-scoped per-site exceptions and a clear way to restore protection. Verify engine/filter compatibility and update behavior on the selected Chromium version; the implementation is not chosen yet.
- Agent-ready. Expose CLI and MCP as supported product interfaces over the same command path as the UI. Scope each grant and give the user a visible way to stop control.

For the project site, use a restrained grid, clear typography, and small blue accents. The browser should keep that clarity while allowing warmer, more personal Spaces and motion.

Do not add an "also in your browser" version to the current scope. A local workspace or export viewer may be useful later, but it does not justify maintaining another platform now.

## Why someone switches

1. Their spaces, hierarchy, and named tabs arrive in a recognizable workspace, with an honest report of anything missing.
2. Onboarding makes that continuity visible through a polished reveal and direct interaction, rather than lengthy explanations.
3. The browser feels familiar in daily use: sidebar, fast command bar, persistent and temporary tabs, Peek and Split View.
4. Local browsing and exports keep working without an account, billing server, or mandatory AI.
5. Compatibility and security maintenance are visible, tested commitments.

Dia already advertises optional AI and Arc migration. Those are baseline competitive requirements, not sufficient differentiation. The claim to validate is the combination of Arc-style workflow fidelity, account independence, tasteful motion, Boosts, open development, and a credible maintenance record.

## Scope

| Priority | Experience | Required outcome |
| --- | --- | --- |
| P0 | Animated first run | Original motion design; skip/resume/replay; reduced motion; user reaches a usable workspace without registration |
| P0 | Spaces and Profiles | Multiple Spaces can share one Profile; distinct Profiles isolate browser data |
| P0 | Sidebar and tabs | Profile favorites, Space pins/folders, temporary tabs, ordering, renaming, shortcuts, archive/restore |
| P0 | Command bar | Open URL/search, find existing tab, dispatch common actions, remain inside the active Profile |
| P0 | Migration and export | Preview, selection, recoverable commit, truthful coverage report, portable workspace |
| P0 | Browser essentials | Extensions, permissions, downloads, history, find, private browsing, context menus, DevTools, recovery, updates |
| P0 | Default-on ad and tracker blocking | Fresh Profiles start protected; per-site exceptions remain Profile-scoped; engine/filter updates and site recovery are tested on the selected Chromium version |
| P0 | CLI and MCP browser control | An approved agent can inspect and act on real pages, manage tabs/Spaces, capture screenshots and stop/release control through a shared interface; works independently of built-in AI |
| P1, beta target | Peek and two-pane Split View | Correct focus, navigation, dismissal, persistence; no cross-Profile split |
| P1, beta target | Boosts | Appearance controls, CSS, element hiding; inspectable and reversible |
| P2 | Built-in AI providers | Explicit invocation and scope with local/BYOK/hosted providers; the P0 external-agent control interface does not wait for this |
| Later | Convenience | Link routing, richer customization, sync, additional platforms, richer Boosts |
| Excluded | Easels | No implementation or migration work beyond reporting unsupported items |

## Platforms

Proposed first supported target: macOS on Apple Silicon, based on the desktop Arc experience and the need for focus. This is an execution assumption to revisit at M0, not a user-imposed requirement. Windows and Linux stay architectural targets, but receive no release dates until build, packaging, update, and QA resources exist. No mobile commitment for v1.

## Business model

Keep core local browsing and local/BYOK AI useful without a subscription. Earn revenue through an optional managed AI service and, later, genuinely useful managed services such as sync. Hosted billing may need an account; the browser must not. A cancelled plan only affects the paid service.

Validate willingness to pay with a small opted-in cohort after the browser is useful. Price from measured model, infrastructure, payment, and support costs; avoid unlimited AI promises. Do not sell browsing history, inject sponsored results, or use data access as the cost of a free browser.

## Learning and adoption

Start with five consenting Arc users using representative synthetic or privately held exports. Watch them import, recognize their workspace, open a pinned tab, switch a Space, and use the command bar. Note failures and assistance needed. Follow with a two-week daily-use cohort if the security gates pass. Recruitment and outreach require a separate authorized action; this plan sends no invitations.

Use aggregate release-download totals to follow adoption. For consented studies, observe task completion, import omissions, time to usable workspace, onboarding skips/replays, recovery failures and return usage. Keep diagnostics local or collect them through explicit study consent; do not add background behavioral tracking. A small cohort guides design, not a statistically reliable growth forecast.
