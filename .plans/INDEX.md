# Index

Tracker for active work. Release evidence lives in docs/SCORECARD.md; ranked blockers in docs/GAPS.md.

## Active

- [MA-007](EXECUTION.md#ma-007a-native-visual-parity-first-state) native visual parity, Chromium lane. Palette match in progress, then empty address field, empty-window state, Space header, content tint, footer icons.
- [Arc clone backlog](2026-10-26-arc-clone-backlog.md): feature and shortcut checklist from Arc's public docs with myArc status and a Next 15 dispatch list.

## Planned

- Arc import ([MA-003](EXECUTION.md)): importer unimplemented; [fixture](../tests/fixtures/arc-markdown/README.md) only, export version and Favorites ownership unverified.
- Browser foundation acceptance ([MA-002](../browser/README.md)): default-context target-creation assertion still open.
- Onboarding native integration: [synthetic study](../design/onboarding/README.md) done; native, performance and user study open.
- CLI and MCP browser control ([MA-011](EXECUTION.md)): P0 launch scope, specified, not implemented.
- Crash durability: [recent writes and navigation can be lost after kill](../browser/experiments/native-session-navigation-crash.md).
- Private and Profile guarantees: cookies, credentials, extensions, Guest/system and shutdown races open.
- Extension compatibility: MV3 on/off/restart smoke passed; update delivery and broader coverage open.
- License inventory: [eight unresolved metadata/license errors](../browser/evidence/dependency-licenses.json).
- Cloudflare Markdown Worker activation for the [site](../site/README.md).

## Recently shipped

- Per-file Chromium patch exports via `tools/export_patches.py`.
- Scripted native/Arc capture comparison via `tools/capture_*.sh`.
- Slimmer AGENTS.md and docs/VERIFY.md.
- Sidebar rows and footer acceptance ([0026](../browser/experiments/native-sidebar-row-footer.md)).
- Palette [address defaults](../browser/evidence/native-palette-address.json) and hover focus.
