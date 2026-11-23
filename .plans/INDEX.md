# Index

Tracker for active work. Release evidence lives in docs/SCORECARD.md; ranked blockers in docs/GAPS.md. Updated 2026-11-23 against `main` `7a5ed0d` / nightly `2026.11.22.2`.

## Active

- Core verification still open in [ALPHA.md](../docs/ALPHA.md): Restore from Archived Tabs to the original Space; native smoke for permissions, history, find in page, and DevTools; offline / no-account network request audit.
- [Next steps after alpha](2026-11-23-next-steps.md): beta-readiness verification gaps and housekeeping.

## Planned

- Performance and accessibility budgets: 1,000-tab responsiveness/onboarding frame trace and VoiceOver review ([ONBOARDING.md](../docs/ONBOARDING.md), [SCORECARD.md](../docs/SCORECARD.md)).
- Remaining [CLI and MCP control (MA-011)](EXECUTION.md) surface: `page.screenshot`, `page.scroll`, `page.wait`, and broader MCP client matrix ([CONTROL-PROTOCOL.md](../docs/CONTROL-PROTOCOL.md)).
- Browser foundation acceptance ([MA-002](../browser/README.md)): default-context target-creation assertion still open.
- Extension compatibility matrix beyond the Bitwarden store path and bundled uBOL.
- License inventory: [unresolved metadata/license errors](../browser/evidence/dependency-licenses.json).

## Recently shipped

- MA-011 CLI + MCP + Claude Code (`2.1.330`) page journey, adversarial checks, Allow card, and Stop revocation verified on packaged nightly `2026.11.22.2` via `tools/check_control.sh` ([SCORECARD.md](../docs/SCORECARD.md)).
- Alpha checklist closed in [ALPHA.md](../docs/ALPHA.md) (live pass on `2026.11.22.2`, URL-pill extension icon glass tooltips, modal `ScrollView` glass fix `7a5ed0d`).

- Alpha nightly `2026.11.22.2` and myarc.dev switched to alpha copy ([DEVLOG](../log/DEVLOG.md)).
- One-click Arc sidebar import (`StorableSidebar.json`), Favorites, multi-Profile mapping, and zero-duplicate re-import (Rust + native; live pass still open).
- Welcome tour with Replay (`⌃⌥⌘A`), quick-start page, crash-report card, signed in-app updates.
- Little Arc (links from other apps, promote, setting), Peek, two-pane Split View, Spaces/Profiles Settings.
- CLI `myarc` + `myarc mcp` in the nightly Helpers; Agent Space approval and Stop.
- Per-file Chromium patch exports via `tools/export_patches.py`.
- Scripted native/Arc capture comparison via `tools/capture_*.sh`.
