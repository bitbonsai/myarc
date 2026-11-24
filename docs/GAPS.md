# Ranked work and blockers

Updated: 2026-11-24. Statuses follow [ALPHA.md](ALPHA.md), [SCORECARD.md](SCORECARD.md), and [DEVLOG.md](../log/DEVLOG.md) through release `v2026.11.24.2`. All **Required for alpha** boxes in [ALPHA.md](ALPHA.md) are closed, and the MA-011 CLI + MCP + Claude Code page journey passed on packaged `2026.11.22.2`.

Current priority: confirm Restore from Archived Tabs live, then close accessibility review, onboarding reveal frame trace, and upstream memory baseline budgets.

| Rank | Gap | Why it leads | Completion evidence |
| --- | --- | --- | --- |
| 1 | Archive restore live confirmation still open | Permissions, history, find in page and DevTools passed in `tools/check_core_smoke.sh` on `2026.11.22.2`; restoring an archived tab from Library -> Archived Tabs to its original Space is still unconfirmed live | Restore check recorded in ALPHA |
| 2 | Accessibility review and onboarding reveal frame trace open | 1,000-tab Space-switch (p95 83.1 ms), CrBrowserMain trace (p95 45.8 ms) and startup (798 ms) passed in `tools/check_responsiveness.sh` on `2026.11.22.2`; VoiceOver review, onboarding reveal 60 Hz frame trace and upstream memory baseline remain open | VoiceOver notes, reveal frame trace and baseline delta in SCORECARD / ONBOARDING |
| 3 | Broader MA-011 client matrix | Core CLI + MCP + Claude Code journey, Allow/Stop, and isolation passed on `2026.11.22.2`; `page.screenshot`, `page.scroll`, and `page.wait` shipped in `2026.11.23.1`; broader third-party MCP client matrix remains open | Multi-client matrix in [CONTROL-PROTOCOL.md](CONTROL-PROTOCOL.md) |
| 4 | Foundation acceptance residue (MA-002) | Build/native interoperation passed; default-context target-creation assertion and broader extension update/removal coverage remain | MA-002 extension/recovery evidence and accepted upkeep plan |
| 5 | Extension/platform compatibility matrix incomplete | Bitwarden store path and bundled uBOL have live/background evidence; named suite across platforms is not done | Versioned matrix; failures and limits visible |
| 6 | Security/update edge cases still thin | Signed updates and notarization ship; disk-full, power-loss and schema-upgrade durability are unmeasured | SCORECARD rows with dated evidence |
| 7 | Boosts and remaining Arc fidelity | Important after the core is stable; Easels stay out of scope | MA-010 tests and recordings where claimed |
| 8 | Built-in AI/service economics untested | Revenue must support maintenance without delaying the browser or control interfaces | Optional provider pilot only after P0 control and beta gates |

Choose the highest-ranked task whose dependencies and resources are available. Keep MA-001 design progress moving while build/data investigations proceed; this is a sequencing instruction, not authorization to spawn autonomous agents or contact users.

Security, data loss, cross-Profile leakage, an invalid updater, or import corruption outrank feature work. A blocked task produces a concrete blocker record, not pretend completion.
