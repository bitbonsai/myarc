# Ranked work and blockers

Updated: 2026-11-23. Statuses follow [ALPHA.md](ALPHA.md), [SCORECARD.md](SCORECARD.md), and [DEVLOG.md](../log/DEVLOG.md) through nightly `2026.11.22.2` (`origin/main` `6059737`). Alpha daily-use shape is largely in the packaged nightly; the open work below is what still blocks calling alpha "closed" or a full release.

Current priority: finish the [alpha live pass](ALPHA-LIVE-CHECK.md), then MA-011 CLI/MCP evidence on a packaged nightly. Visual parity polish continues in the Chromium lane when a slice is assigned; it no longer outranks those gates.

| Rank | Gap | Why it leads | Completion evidence |
| --- | --- | --- | --- |
| 1 | Alpha live pass unfinished | Five **Required for alpha** boxes still unchecked: Little Arc `⌥⌘N` from another app, one-click Arc import on real data, welcome tour Back/`⌃2`/Reduce Motion, Chrome Web Store Bitwarden install | Checked boxes in [ALPHA.md](ALPHA.md) from a run of [ALPHA-LIVE-CHECK.md](ALPHA-LIVE-CHECK.md) on `2026.11.22.2` or newer |
| 2 | No usable CLI/MCP journey on a packaged nightly (MA-011) | P0 for full release; CLI and MCP ship in the nightly, but the log has no external MCP client page journey and no control-on packaged launch | Both interfaces complete the same real page journey; Allow/Stop; [CONTROL-PROTOCOL.md](CONTROL-PROTOCOL.md) |
| 3 | Core browsing smoke and Restore still open | Archive restore to the original Space is unconfirmed; permissions, history, find in page and DevTools lack a native smoke | Restore check + native smoke recorded in ALPHA / SCORECARD |
| 4 | Offline / no-account network audit missing | Account independence gate needs a fresh install with the network off and a request audit | Recorded journey separating updates/site traffic from optional services |
| 5 | Accessibility and responsiveness budgets unmeasured | 1,000-tab frame/responsiveness trace and VoiceOver review are still open onboarding and scorecard gates | Native recordings and traces on a reference Mac |
| 6 | Foundation acceptance residue (MA-002) | Build/native interoperation passed; default-context target-creation assertion and broader extension update/removal coverage remain | MA-002 extension/recovery evidence and accepted upkeep plan |
| 7 | Extension/platform compatibility matrix incomplete | Bitwarden store path and bundled uBOL have background evidence; named suite across platforms is not done | Versioned matrix; failures and limits visible |
| 8 | Security/update edge cases still thin | Signed updates and notarization ship; disk-full, power-loss and schema-upgrade durability are unmeasured | SCORECARD rows with dated evidence |
| 9 | Boosts and remaining Arc fidelity | Important after the core is stable; Easels stay out of scope | MA-010 tests and recordings where claimed |
| 10 | Built-in AI/service economics untested | Revenue must support maintenance without delaying the browser or control interfaces | Optional provider pilot only after P0 control and alpha close |

Choose the highest-ranked task whose dependencies and resources are available. Keep MA-001 design progress moving while build/data investigations proceed; this is a sequencing instruction, not authorization to spawn autonomous agents or contact users.

Security, data loss, cross-Profile leakage, an invalid updater, or import corruption outrank feature work. A blocked task produces a concrete blocker record, not pretend completion.
