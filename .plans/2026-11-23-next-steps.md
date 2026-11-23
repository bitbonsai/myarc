# Next steps after alpha nightly `2026.11.22.2`

Captured: 2026-11-23 at `origin/main` `6059737` (Chromium `myarc` `c79fde785c`).

## Baseline status

- `origin/main` is at `6059737` (*site: update blog post title to Alpha is here*), right after shipping `nightly-2026.11.22.2` (`71e04fc`).
- `python3 tools/check_docs.py` passes (508 links across 97 files).
- `python3 tools/export_patches.py --check` passes (150 patches match `myarc`).
- Shared Chromium checkout (`~/.cache/myarc-m0/chromium/src`) is clean on branch `myarc` at `c79fde785c`.
- No open GitHub PRs or issues.

## 1. Finish the Alpha live pass (manual, ~10 min)

Run [ALPHA-LIVE-CHECK.md](../docs/ALPHA-LIVE-CHECK.md) on `2026.11.22.2` or newer to close the last 5 unchecked **Required for alpha** items in [ALPHA.md](../docs/ALPHA.md):

1. **Little Arc (`⌥⌘N`)**: trigger from another app and promote with `⌘O`.
2. **One-click Arc import**: run against real Arc data, verify profile mapping and zero-duplicate re-import.
3. **Welcome tour**: replay with `⌃⌥⌘A`, click **Back** on a coach mark, complete `⌃2` handoff, and verify Reduce Motion fade.
4. **Chrome Web Store extension install**: install Bitwarden via **Add to Chrome**, confirm dialog anchoring, check URL pill hover icon, quit and relaunch.

## 2. P0 and beta-readiness gaps

Tracked in [ALPHA.md](../docs/ALPHA.md) and [SCORECARD.md](../docs/SCORECARD.md):

- **CLI and MCP control (MA-011, P0 for full release)**: Partial check 2026-11-23 on installed `2026.11.22.2`: `myarc` binary lists the full registry; control endpoint is live (`control.json` + sock) with the packaged nightly already running control-on. `myarc run space.list` reached the Allow card and timed out (`denied: waiting for approval timed out`). Still needed: click Allow, then an end-to-end CLI + external MCP page journey ([CONTROL-PROTOCOL.md](../docs/CONTROL-PROTOCOL.md)).
- **Core verification gaps**:
  - **Restore** from Archived Tabs: native path `BrowserWorkspaceAdapter::RestoreArchivedTab` reopens in `tab->space_id` when that Space still exists, else the current Space. Live confirm still open.
  - Add a native smoke test for permissions, history, find in page, and DevTools.
  - Run the offline / no-account network request audit.
- **Performance and accessibility budgets**: record the 1,000-tab responsiveness/onboarding frame trace and a VoiceOver review.

## 3. Housekeeping

- **Refresh stale trackers**: Done 2026-11-23 in this worktree. [INDEX.md](INDEX.md) and [GAPS.md](../docs/GAPS.md) now track the live pass, MA-011, and post-alpha verification instead of pre-alpha "import unimplemented / MA-007.a active".
- **Check other worktrees** (2026-11-23 audit vs `origin/main` `6059737`):

  | Worktree / branch | Verdict |
  | --- | --- |
  | `t3/site-content-latest-changes`, `t3/beta-readiness-gaps`, `t3/ping-check`, `t3/blog-alpha-is-here`, `t3/find-settings-page-prototypes`, `t3/pull-latest-review` | Ancestors of `origin/main` → safe to prune |
  | `t3/myarc-browser-skill`, `t3/myarc-shipped-since-agents-post` | Tips not on main, but skill + post already exist on main under later commits → likely prune after confirming no unique edits |
  | `docs/sync-with-main` (`93c5775`) | **Land candidate**: user-docs sync (install, import, shortcuts, spaces, known-limitations, your-data) not on main |
  | `ribbon-3d-hero` (`a26b40a`, bit/myarc) | **Land or drop**: WebGL homepage ribbon, +750/−18 vs main |
  | `t3/native-favorite-tile-drag`, `welcome-page`, older experiment branches | Behind main; check individually before prune |

## Computer use for §1

Local IDE agents cannot drive Computer Use. A My Machines worker with `--computer-use` is running as `myarc-live-pass` (`workerId=482fdb20-0147-41a6-a452-cea0b6c78061`). Open [cursor.com/agents for this worker](https://cursor.com/agents#workerId=482fdb20-0147-41a6-a452-cea0b6c78061), confirm Accessibility + Screen Recording for **Cursor Computer Use**, and run [ALPHA-LIVE-CHECK.md](../docs/ALPHA-LIVE-CHECK.md) on `/Applications/myArc Nightly.app` (`2026.11.22.2`) with a disposable Profile for import/extensions.
