# Ranked work and blockers

Updated: 2026-10-24. Browser release blockers remain open. Synthetic onboarding and a two-revision Rust/native build experiment now supply bounded evidence.

| Rank | Gap | Why it leads | Completion evidence |
| --- | --- | --- | --- |
| 1 | Foundation acceptance and maintenance capacity unresolved | Build/native interoperation passed; extension lifecycle, runtime boundaries and ownership remain unverified | MA-002 extension/recovery evidence, complete notices and accepted upkeep/signing/budget plan |
| 2 | Arc export provenance and semantic coverage incomplete | One Markdown shape is inventoried; producer/version, Favorites scope and nested-folder behavior remain unverified | MA-003 confirmed export route, field mapping and expanded synthetic corpus |
| 3 | Native animated onboarding remains unverified | First-run quality is an explicit product requirement; [web study](../design/onboarding/README.md) now runs with synthetic fixtures | Review study; integrate shared state and gather native motion, accessibility, performance and user-task evidence |
| 4 | Workspace semantics and persistence unimplemented | Import, sidebar, onboarding and recovery share this contract | MA-004/005 model, commands, round trip and interruption checks |
| 5 | No safe import transaction or preservation utility | Users need a usable escape hatch before the complete browser | MA-006 dry run, omissions report, rollback/source-integrity proof |
| 6 | No complete browsing slice | An attractive shell is insufficient | MA-007 integrated import → pin → Space → command → restart journey |
| 7 | Security/update/profile/private-mode guarantees unproven | Blocks any public daily-use promise | MA-009 and [security plan](SECURITY-PLAN.md) evidence |
| 8 | No extension/platform compatibility matrix | Chromium heritage is not compatibility evidence | MA-002/009 named test cases, versions and outcomes |
| 9 | No usable CLI/MCP browser control | P0 user requirement; AI agents must observe and act on real pages | MA-011 both-interface and external-client journeys, grants, isolation and Stop control |
| 10 | Peek/splits and initial Boosts unimplemented | Important workflow fidelity after the core is stable | MA-010 tests, user tasks and recordings |
| 11 | Built-in AI/service economics and demand untested | Revenue must support maintenance, without delaying the browser or core control interfaces | Optional provider pilot, cost ledger and limit/billing evidence |

Choose the highest-ranked task whose dependencies and resources are available. Keep MA-001 design progress moving while build/data investigations proceed; this is a sequencing instruction, not authorization to spawn autonomous agents or contact users.

The standalone Rust model and preservation path may progress while the browser foundation is blocked; MA-004 no longer treats a completed Chromium build as a prerequisite. Use [behavior contracts](../plan/BEHAVIOR-CONTRACTS.md) to settle cross-system rules and retain useful failed-attempt context in [the development log](../log/DEVLOG.md).

Security, data loss, cross-Profile leakage, an invalid updater, or import corruption outrank feature work. A blocked task produces a concrete blocker record, not pretend completion.
