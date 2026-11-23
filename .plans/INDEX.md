# Index

Tracker for active work. Release evidence lives in docs/SCORECARD.md; ranked blockers in docs/GAPS.md. Updated 2026-11-23 against `origin/main` `6059737` / nightly `2026.11.22.2`.

## Active

- Alpha live pass: close the five unchecked **Required for alpha** boxes in [ALPHA.md](../docs/ALPHA.md) via [ALPHA-LIVE-CHECK.md](../docs/ALPHA-LIVE-CHECK.md) (Little Arc `⌥⌘N`, one-click Arc import, welcome tour Back/`⌃2`/Reduce Motion, Chrome Web Store Bitwarden).
- [CLI and MCP control (MA-011)](EXECUTION.md): P0 for full release; verify an external MCP client page journey and control-on launch on a packaged nightly ([CONTROL-PROTOCOL.md](../docs/CONTROL-PROTOCOL.md)).
- [Next steps after alpha](2026-11-23-next-steps.md): beta-readiness verification gaps and housekeeping.

## Planned

- Core verification still open in [ALPHA.md](../docs/ALPHA.md): Restore from Archived Tabs to the original Space; native smoke for permissions, history, find in page, and DevTools; offline / no-account network request audit.
- Performance and accessibility budgets: 1,000-tab responsiveness/onboarding frame trace and VoiceOver review ([ONBOARDING.md](../docs/ONBOARDING.md), [SCORECARD.md](../docs/SCORECARD.md)).
- Browser foundation acceptance ([MA-002](../browser/README.md)): default-context target-creation assertion still open.
- Extension compatibility matrix beyond the Bitwarden store path and bundled uBOL.
- License inventory: [unresolved metadata/license errors](../browser/evidence/dependency-licenses.json).

## Recently shipped

- Alpha nightly `2026.11.22.2` and myarc.dev switched to alpha copy ([DEVLOG](../log/DEVLOG.md)).
- One-click Arc sidebar import (`StorableSidebar.json`), Favorites, multi-Profile mapping, and zero-duplicate re-import (Rust + native; live pass still open).
- Welcome tour with Replay (`⌃⌥⌘A`), quick-start page, crash-report card, signed in-app updates.
- Little Arc (links from other apps, promote, setting), Peek, two-pane Split View, Spaces/Profiles Settings.
- CLI `myarc` + `myarc mcp` in the nightly Helpers; Agent Space approval and Stop.
- Per-file Chromium patch exports via `tools/export_patches.py`.
- Scripted native/Arc capture comparison via `tools/capture_*.sh`.
