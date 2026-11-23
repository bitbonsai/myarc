# Ranked work and blockers

Updated: 2026-11-23. Statuses follow [ALPHA.md](ALPHA.md), [SCORECARD.md](SCORECARD.md), and [DEVLOG.md](../log/DEVLOG.md) through nightly `2026.11.22.2` and `main` `7a5ed0d`. All **Required for alpha** boxes in [ALPHA.md](ALPHA.md) are closed, and the MA-011 CLI + MCP + Claude Code page journey passed on packaged nightly `2026.11.22.2`.

Current priority: close the remaining core browsing verification gaps (Archive Restore + permissions/history/find/DevTools smoke) and the offline / no-account request audit, then responsiveness/accessibility budgets.

| Rank | Gap | Why it leads | Completion evidence |
| --- | --- | --- | --- |
| 1 | Archive restore live confirmation still open | Permissions, history, find in page and DevTools passed in `tools/check_core_smoke.sh` on `2026.11.22.2`; restoring an archived tab from Library -> Archived Tabs to its original Space is still unconfirmed live | Restore check recorded in ALPHA |
| 2 | Accessibility and responsiveness budgets unmeasured | 1,000-tab frame/responsiveness trace and VoiceOver review are still open onboarding and scorecard gates | Native recordings and traces on a reference Mac |
| 3 | Remaining MA-011 control surface (`page.screenshot`, `scroll`, `wait`, broader client matrix) | Core CLI + MCP + Claude Code journey, Allow/Stop, and isolation passed on `2026.11.22.2`; remaining registry commands still return `unsupported` | Implemented commands + multi-client matrix in [CONTROL-PROTOCOL.md](CONTROL-PROTOCOL.md) |
| 4 | Foundation acceptance residue (MA-002) | Build/native interoperation passed; default-context target-creation assertion and broader extension update/removal coverage remain | MA-002 extension/recovery evidence and accepted upkeep plan |
| 5 | Extension/platform compatibility matrix incomplete | Bitwarden store path and bundled uBOL have live/background evidence; named suite across platforms is not done | Versioned matrix; failures and limits visible |
| 6 | Security/update edge cases still thin | Signed updates and notarization ship; disk-full, power-loss and schema-upgrade durability are unmeasured | SCORECARD rows with dated evidence |
| 7 | Boosts and remaining Arc fidelity | Important after the core is stable; Easels stay out of scope | MA-010 tests and recordings where claimed |
| 8 | Built-in AI/service economics untested | Revenue must support maintenance without delaying the browser or control interfaces | Optional provider pilot only after P0 control and beta gates |

Choose the highest-ranked task whose dependencies and resources are available. Keep MA-001 design progress moving while build/data investigations proceed; this is a sequencing instruction, not authorization to spawn autonomous agents or contact users.

Security, data loss, cross-Profile leakage, an invalid updater, or import corruption outrank feature work. A blocked task produces a concrete blocker record, not pretend completion.
