# Index

Tracker for active work. Release evidence lives in docs/SCORECARD.md; ranked blockers in docs/GAPS.md. Updated 2026-11-24 against `main` `abca90c` / release `v2026.11.24.2`.

## Active

- Core verification still open in [ALPHA.md](../docs/ALPHA.md): Restore from Archived Tabs to the original Space.
- [Next steps after alpha](2026-11-23-next-steps.md): beta-readiness verification gaps and housekeeping.

## Planned

- Performance and accessibility budgets: VoiceOver review, onboarding reveal 60 Hz frame trace and upstream memory baseline ([ONBOARDING.md](../docs/ONBOARDING.md), [SCORECARD.md](../docs/SCORECARD.md)).
- Broader [CLI and MCP control (MA-011)](EXECUTION.md) third-party MCP client matrix and remaining workspace/session commands ([CONTROL-PROTOCOL.md](../docs/CONTROL-PROTOCOL.md)).
- Browser foundation acceptance ([MA-002](../browser/README.md)): default-context target-creation assertion still open.
- Extension compatibility matrix beyond the Bitwarden store path and bundled uBOL.
- License inventory: [unresolved metadata/license errors](../browser/evidence/dependency-licenses.json).

## Recently shipped

- Release `v2026.11.24.2`: retired Nightly naming (`myArc.app`, `dev.myarc.browser`, first-launch profile + Keychain migration), Arc-aligned tab context menu (`Mute`, `Duplicate`, unified `Move to`, `Archive Tabs Below`), empty-sidebar Space menu, URL-pill icon background suppression, neutral sidebar port chips, and myArc + Chromium versions in the Sparkle up-to-date sheet ([DEVLOG](../log/DEVLOG.md)).
- `page.screenshot`, `page.scroll`, and `page.wait` control commands across `crates/control`, `browser/foundation`, and `myarc mcp` (`2026.11.23.1`).
- Native core smoke (`tools/check_core_smoke.sh`: permissions, history, find in page, DevTools), offline / no-account NetLog audit (`tools/check_offline_audit.sh`), and 1,000-tab responsiveness benchmark (`tools/check_responsiveness.sh`).
- MA-011 CLI + MCP + Claude Code (`2.1.330`) page journey, adversarial checks, Allow card, and Stop revocation verified on packaged `2026.11.22.2` via `tools/check_control.sh` ([SCORECARD.md](../docs/SCORECARD.md)).
- Alpha checklist closed in [ALPHA.md](../docs/ALPHA.md) (live pass on `2026.11.22.2`, URL-pill extension icon glass tooltips, modal `ScrollView` glass fix `7a5ed0d`).
- One-click Arc sidebar import (`StorableSidebar.json`), Favorites, multi-Profile mapping, and zero-duplicate re-import.
- Welcome tour with Replay (`⌃⌥⌘A`), quick-start page, crash-report card, signed in-app updates.
- Little Arc (links from other apps, promote, setting), Peek, two-pane Split View, Spaces/Profiles Settings.
- CLI `myarc` + `myarc mcp` in the release Helpers; Agent Space approval and Stop.
- Per-file Chromium patch exports via `tools/export_patches.py`.
- Scripted native/Arc capture comparison via `tools/capture_*.sh`.
