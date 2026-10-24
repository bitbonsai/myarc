# Current status

Updated: 2026-10-24.

| Item | State |
| --- | --- |
| Product scope and priority | Initial plan documented |
| Rust direction | Dependency-free M0 core and Cxx/native toolbar proof implemented; full workspace model remains open |
| Animated onboarding | [MA-001.a synthetic motion study](../design/onboarding/README.md) implemented and exercised; native integration, performance and user study remain open |
| Arc import | Coverage contract documented; real schema/version evidence still needed |
| Browser foundation | [MA-002.a–c](../browser/README.md): unmodified build/launch, native Rust command and two-revision build/bridge checks verified; foundation acceptance remains open |
| Agent workflow | Contributor instructions and task sequence prepared |
| Artcraft workflow audit | Behavior contracts, bounded tasks, development log, evidence freshness and regression rules added; standalone core dependency corrected |
| Executable checks today | Document links; six storyboard tests and prototype recordings; six Rust core tests; seven C++/Rust checks per pin. Rust/patch workflow added; remote CI execution unverified |
| Browser/importer/AI implementation | Experimental native control only; complete browser, importer and AI remain unimplemented |
| Public landing page | [Self-contained HTML](../site/README.md) deployed to GitHub Pages over HTTPS; offline/layout/motion checks passed. `myarc.dev` DNS and domain HTTPS pending Cloudflare access |
| Brand exploration | Standalone [brand kit](../design/brand/README.md) with supplied raster marks, social cards, wallpapers and lockups; naming/asset rights unverified |
| CLI and MCP browser control | P0 launch requirement; tools, shared control service, scoped grants and MA-011 specified; no executable implementation yet |
| Benchmarks/native application tests | Narrow native control and separate-user-data-dir smoke checks only; production boundaries and release budgets remain unverified |

Next work: retest extension lifecycle in a fresh native-launched profile, complete isolation/recovery and licensing/distribution investigation, and obtain an official export for MA-003. The [326-entry inventory](../browser/evidence/dependency-licenses.json) has eight unresolved metadata/license errors. Signing, maintenance staffing and recurring budget remain unverified; M0 stays open.

Keep this file public and concise. Put local machine state, private source samples and unpublished personal notes outside the committed tree. Do not store user chat transcripts here.
