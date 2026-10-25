# Current status

Updated: 2026-10-25.

| Item | State |
| --- | --- |
| Product scope and priority | Native Arc visual parity leads current work, including full-window material and sidebar navigation; export work deferred |
| Rust direction | Dependency-free M0 core and Cxx/native toolbar proof implemented; full workspace model remains open |
| Animated onboarding | [MA-001.a synthetic motion study](../design/onboarding/README.md) implemented and exercised; native integration, performance and user study remain open |
| Arc import | One private Markdown sample inventoried; [synthetic shape fixture](../tests/fixtures/arc-markdown/README.md) added. Built-in export action confirmed; version binding and Favorites ownership remain unverified; importer unimplemented |
| Browser foundation | [MA-002.a–d](../browser/README.md): build/native interoperation plus named MV3 on/off/restart and renderer/private-storage smoke checks passed; target-creation assertion and foundation acceptance remain open |
| Native Spaces prototype | [Command-safety integration and manual native smoke](../browser/experiments/native-workspace-command-safety.md): two populated Spaces switch real pages; Empty hides both. Startup faults and prior-URL header leak fixed; [fresh native header acceptance](../browser/evidence/workspace-empty-header.json) passed. Renderer stays off while broader native cases remain unrun |
| Recovery popup prototype | [Native popup acceptance](../browser/evidence/recovery-anchor.json): forced crashed-exit startup survives, popup remains through sidebar hide/resize, Restore invocation and dismissal do not crash. Actual session recovery/durability remain unaccepted |
| Saved identity prototype | [Real extension Pin/Unpin and native UI Unsave/close/reopen](../browser/evidence/workspace-saved-identity.json) passed in a disposable profile. [Blank-only material and saved-journey rechecks](../browser/evidence/page-background.json) passed. [One explicit clean restart](../browser/evidence/workspace-clean-restart.json) retained tabs/IDs. Multiwindow/crash and broader restart gates remain open |
| Process-crash recovery probe | [0018 native followups](../browser/evidence/workspace-process-crash.json): pending-choice identity loss corrected; actual Restore of previously persisted canary/metadata and separate explicit discard passed. Freshly acknowledged localStorage still loses its canary after kill. Wider lifecycle/write/OS durability remain open; strict verifier retained |
| Agent workflow | One bounded implementation task per agent, followed by lead integration and native verification |
| Artcraft workflow audit | Behavior contracts, bounded tasks, development log, evidence freshness and regression rules added; standalone core dependency corrected |
| Executable checks today | Document links; six storyboard tests and prototype recordings; 32 Rust workspace tests; seven C++/Rust checks per pin; retained manual native-runtime client. Rust/patch workflow added; remote CI execution unverified |
| Browser/importer/AI implementation | [Opt-in native layout/material prototype](../browser/experiments/native-shell.md) runs with sidebar navigation and a [native command palette with local history and light/dark appearance](../browser/experiments/native-command-palette.md); 1:1 visual parity, complete browser, importer and AI remain unimplemented |
| Public website and blog | [Static website and Markdown journal](../site/README.md) deployed to GitHub Pages; first build-in-public post live. Fourteen blog/publisher/discovery tests and local layout/theme/focus checks pass; published HTML/screenshot bytes verified. HTTPS remains enforced; browser product behavior is separate |
| Brand exploration | Standalone [brand kit](../design/brand/README.md) with supplied raster marks, social cards, wallpapers and lockups; naming/asset rights unverified |
| CLI and MCP browser control | P0 launch requirement; tools, shared control service, scoped grants and MA-011 specified; no executable implementation yet |
| Benchmarks/native application tests | Native control on second pin, renderer reload and off-the-record localStorage checks passed; full Profile/privacy/durability guarantees and release budgets remain unverified |

Next work: [MA-007.a](EXECUTION.md#ma-007a-native-visual-parity-first-state), match the Arc empty/new-tab window with real Chromium content, sidebar address controls and full-window translucent material. Import investigation and expanded foundation testing are deferred. Existing crash, isolation and distribution findings remain release blockers. The [326-entry inventory](../browser/evidence/dependency-licenses.json) has eight unresolved metadata/license errors. Signing, maintenance staffing and recurring budget remain unverified; M0 stays open.

Keep this file public and concise. Put local machine state, private source samples and unpublished personal notes outside the committed tree. Do not store user chat transcripts here.
