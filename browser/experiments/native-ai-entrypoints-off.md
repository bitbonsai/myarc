# Unsupported native AI Mode entrypoints

MA-007.c visible-control slice from parent `fbdf96e`. Optional [0021](../patches/legacy/0021-native-ai-entrypoints-off.patch) suppresses unsupported browser-owned AI Mode entrypoints when both `--myarc-visual-parity` and `--myarc-workspace-sidebar` are present. Full Chrome build passes. Parent bounded desktop/native checks passed; [structured evidence](../evidence/native-ai-entrypoints-off.json) records their limits. Workspace renderer stays default OFF.

## Observed gap and repair

Parent native startup, recovery and new-Profile accessibility checks found an enabled "AI Mode button, press Enter to ask AI Mode" in `LocationBarView`. This prototype has no built-in AI enablement or provider selection. [AI requirements](../../docs/AI.md) require deliberate enablement and action; presenting this control was unsupported.

`ChromeAimEligibilityService` now overrides the existing virtual `IsAimAllowedByFeatureAndPolicy()` gate. Both prototype switches return false; every other combination delegates unchanged to the upstream implementation. This is a prototype availability check, with no enterprise policy, preference writes or new settings.

The actual factory is `chrome/browser/autocomplete/aim_eligibility_service_factory.cc`. It retains its existing regular/OTR own-instance routing and other Profile selections. The change neither removes the service nor shares new state across Profiles.

Existing callers do the UI work:

- `IsAimEligible()` and local eligibility feed the native AI Mode page action, popup, inline context controls and AI starter-pack gates.
- `OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled()` also uses the overridden gate directly for non-Google search providers.
- `ShouldShowAimContextMenuOption()` checks that entrypoint or eligible popup. Both reject the prototype, so its associated "Show AI Mode" menu item is omitted rather than left as an enabled no-op.

The page-action controller owns visibility updates; no `SetVisible` override competes with later focus/tab changes. Ordinary text search/navigation, visiting AI websites, Chrome services/extensions, Rust policy, workspace schema and command blank guards are unchanged.

## Limits

This suppresses entrypoints. It does not deliver off/local/BYOK/hosted provider modes, deny all AI-related network traffic or prove no background requests. In particular, the upstream base service constructor retains its eligibility-request setup before this derived availability gate can be used. Other Chrome AI features and debugging/forced eligibility overrides are outside this slice. No network/privacy enforcement claim follows from an absent button.

## Fixture and build evidence

The existing `chrome_aim_eligibility_service_browsertest.cc` gains `ChromeAimEligibilityServiceMyarcBrowserTest.PrototypeOmitsUnsupportedEntrypoints`, parameterized over all four prototype-switch combinations. It asserts separate regular/OTR service instances, Google eligibility and menu behavior, unchanged unmanaged preferences, existing policy denial/reset, and non-Google entrypoint/menu behavior. The non-Google control uses upstream third-party debug configuration. The normal search template remains intact. It does not navigate a real page.

These four browser cases are **UNRUN**. Real Chromium clang syntax passed with their owning target's defines, includes and flags. Syntax does not establish runtime assertions or native accessibility behavior.

Verification on 2026-10-26 against external Chromium `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`:

- `autoninja -C out/MyarcM0 -j8 chrome workspace_lifecycle_check` completed with terminal exit `0`, 01:15:38Z to 01:15:51Z. The eligibility object and component `libchrome_dll.dylib` have fresh mtimes. The app launcher and unchanged lifecycle helper remain older, as expected.
- `workspace_lifecycle_check` executed and passed. This unchanged helper checks opaque-identity registration/metadata/recovery barriers, not AI behavior.
- GN generation and header checks passed for actual nonzero `//chrome/browser/autocomplete:aim_eligibility_service` and `:browser_tests` targets. Their source lists contain the intended production and fixture files.
- Changed-line clang-format, GN formatting, strict indexed reverse/replay, canonical payload freshness, document links and whitespace checks passed.

No browser-test graph was built. No app, browser/helper process or fixture server was launched. No actual/protected Profile, OS permission/setting, service or singleton lock was touched. Rust checks were not rerun because no owned Rust/foundation source changed.

## Packaging and handoff

Apply 0021 once after canonical16 and the frozen optional stack through 0020. Installed external source already includes it. Exact changed upstream files:

```text
chrome/browser/autocomplete/BUILD.gn
chrome/browser/autocomplete/chrome_aim_eligibility_service.cc
chrome/browser/autocomplete/chrome_aim_eligibility_service.h
chrome/browser/autocomplete/chrome_aim_eligibility_service_browsertest.cc
```

Production changes are one override declaration, its implementation and a command-line include. The GN change adds the fixture's omnibox header dependency. Canonical 0001 and all 19 existing patch artifacts, including 0012/0016/0017/0018/corrected0019/0020, remain byte-identical. Existing independent layout changes are untouched. All 16 installed owned files match this worktree. `upstream.json` is unchanged.

Private proof: `/tmp/myarc-m0-runtime/mode21/`. Packaging and syntax scripts were copied from mode20, checked against mode19-r1 and adjusted to this worktree before use. The directory contains the through0020 baseline, 82-file before/installed manifests, canonical16 and prior-patch hashes, strict reverse/replay logs, GN/syntax commands and results, terminal build exit/timestamps, artifact stats and executable-comm preflight. Replay restores the baseline and reproduces installed bytes; all 78 unrelated captured files remain identical.

0021 SHA-256: `f935d68db5a5d69d35e8947471abdfed0e82938ba9cd1205ab9640596414ec1e`.

## Parent native acceptance

Integrated as `5627bb3`; installed 82-file hashes match. Fresh marked native NTP, horizontal/vertical transitions, private NTP/mode transitions and new regular-Profile horizontal/vertical presentation omitted the AI Mode button. The actual address context menu omitted its associated toggle. Private memory-only notice and recovery NTP captures were inspected. Same other window created a Space without reopening.

Native Cmd+L navigated the same tab to the fixture, submitted an ordinary text query and returned to the fixture. Google redirected the query to a bot challenge; no challenge was interacted with and search results are unaccepted. The single-page native Pin/generic Unpin probe passed. An initial crash-checkpoint attempt rejected the wrong selected page; actual Favorite selection corrected setup without changing the verifier.

A clean changed-process roundtrip seeded and strictly verified the canary. Audited SIGKILL after the real native crash marker persisted, plain relaunch without URLs/restore flag and genuine Restore then retained IDs/owners/selection/canary for five stable samples. Pending metadata stayed exact and its NTP/header showed no AI Mode button.

A separate default flag-off NTP positively showed the native AI Mode button and `Always show AI Mode` menu item, with no workspace file/UI. No eligibility override or preference manipulation was needed. Its active context menu delayed graceful termination; the harness initially sent Cmd+Escape, then corrected that to plain Escape with key release. Native exit was Normal and all owned apps/helpers/servers closed. This was harness cleanup, not a product repair.

Private runs: `/tmp/myarc-m0-runtime/mode21-acceptance-20261026T012158Z/` and `/tmp/myarc-m0-runtime/mode21-control-20261026T012755Z/`. Raw network URLs and OS Services names stay private. Full native suites, other AI features, AI network/privacy enforcement and durability remain unmeasured. Provider modes and product CLI/MCP remain unimplemented; renderer stays off.

External source/build ownership is relinquished at handoff, with no build left running. Parent owns integration, launches and native captures.
