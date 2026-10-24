# Product plan

## Audience and job

Serve desktop Arc users who have invested in Spaces, folders, named pinned tabs, and familiar shortcuts, and want that way of working to remain dependable. The first job is to continue an existing browsing life with little relearning or data loss.

**Positioning hypothesis:** familiar spatial browsing, a delightful first minute, and ownership that survives the business behind it. Validate this with actual Arc users before claiming demand. This is narrower and more defensible than a general-purpose AI browser.

## Requirements carried into this plan

The project brief calls for account-free browsing, Chromium extension compatibility, Arc settings import, an onboarding experience with meaningful animation, Boosts, no Easels, entirely optional AI through local models/user keys/a paid hosted service, and browser control through CLI and MCP for AI agents. These are product constraints. The technical choices and dates below are proposals, not prior decisions.

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

Track task completion, import omissions, time to usable workspace, onboarding skips/replays, recovery failures, and return usage. Use local diagnostics or explicit study consent; no background tracking requirement. A small cohort guides design, not a statistically reliable growth forecast.
