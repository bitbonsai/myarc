# Native overnight handoff

Approved window: 2026-10-25 21:38:01 UTC to 2026-10-26 05:38:01 UTC. Run closed 2026-10-26T04:40:48.433979+00:00. This records bounded slices, not eight uninterrupted hours or complete native parity. Workspace renderer remains **off by default**.

## Integrated

- `2a0ef32`: recovery-choice hold preserves workspace identity until Restore/discard.
- `78b303f`, `0dfa2f6`, `7a14927`: runtime-mode readiness and horizontal restore consumers. The intermediate cold-binding failure remains recorded.
- `2b98a0a`: strict two-window checkpoint/readback probe. `c263228`: isolated native preview launcher.
- `5627bb3`: prototype-only unsupported AI Mode entrypoints omitted. Provider modes/network enforcement remain unimplemented.
- `2c1f49d`, `e47b824`, `8b52f25`: same-widget shortcut intent, current-address literal defaults and palette-local hover focus. Explicit global result activation and Cmd+T defaults stay provisional.
- `a8de00d`: real state-specific storage retries. `42208ac`: final native focus/preview acceptance evidence.

Commits are local, author/committer `bitbonsai`. No push, deployment or history rewrite.

## Actual verification

Final Chromium `chrome -j8` build exited 0; real-context syntax/GN and strict 83-file replay passed. Parent checked installed/compiled hashes and canonical 16-file payload. Final standalone rerun passed 32 Rust tests, 20 client API-double tests, 10 launcher tests and 6 storyboard state tests. Formatting/clippy, payload/link checks and bridge/lifecycle helpers passed. Bridge reported seven legacy checks plus workspace/codec checks; duplicate ANGLE warning persists. **Native C++ browser/interactive suites remain UNRUN.**

Parent native evidence:

- [Clean-persisted process-crash Restore/discard](../evidence/workspace-process-crash.json), exact pending metadata and five strict restored samples. Final 0025 control clean-persisted the session after every URL/selection exercise before kill.
- [Two-window saved identity/crash Restore](../evidence/workspace-two-window.json), [horizontal late binding](../evidence/workspace-late-bindings.json), and [two sequential native window reopens](../evidence/workspace-native-late-restore.json).
- Bounded [private lifecycle](../evidence/workspace-native-private.json) and [two regular Profiles](../evidence/workspace-native-profiles.json), separate from complete privacy acceptance.
- [Normal/reduced real pointer, Tab/Enter and mouse selection](../evidence/native-palette-hover-focus.json); [literal/history/default navigation](../evidence/native-palette-address.json).
- [Failed retries preserve bytes/memory; real retries after explicit file repair](../evidence/workspace-storage-retry.json). Manual backup restoration is not automatic recovery.

## Remaining blockers

Recent acknowledged localStorage and navigation can be lost after SIGKILL. One recent-navigation run kept IDs but restored duplicate root URLs; its strict failure remains beside successful persisted-session controls. [Details](native-session-navigation-crash.md). Configured storage timers are not measured flush latency or causal proof. Acknowledgement/OS durability remain unaccepted.

Broader detach/overlap/shutdown, cookies/credentials/Guest/system and complete accessibility/RTL/fullscreen/motion matrices remain open. CLI/MCP is still P0 and unimplemented; developer CDP probes and launcher do not deliver it. Complete browser/importer/native onboarding remain unfinished. AI provider/privacy behavior remains unaccepted. Eight license metadata errors, signing/notarization and update/maintenance ownership still block distribution.

## Tested preview and short checklist

On the existing verified macOS Apple Silicon build host, from repository root:

```sh
python3 tools/launch_native_preview.py
```

[Latest 0025 no-debug smoke](../evidence/native-preview.json) observed real Example Domain navigation/current-item editing, pointer focus, neutral Empty and Cmd+S. Each invocation creates a fresh marked private profile with explicit prototype flags, no debug endpoint/automation/test extension. Saved close/reopen and held sidebar-hover evidence is retained from 0022, not claimed as repeated in this latest no-debug run.

1. Cmd+T → `https://example.com` → Enter. Cmd+L → `https://example.org` → Enter. Current tab should navigate; hovering a result should retain typing focus. Escape cancels.
2. Create/switch to Empty: neutral header/current input, prior page hidden. Return to the populated Space and select its row.
3. Try Favorite save, Close live page (keep Favorite), tile reopen and explicit Remove. Saved identity should persist until deliberate removal. Follow the [full five-minute checklist](../../tests/fixtures/browser/README.md#native-preview-five-minute-manual-smoke).
4. Cmd+S hides/restores sidebar; edge hover should overlay the page. Numeric viewport/exact timing and full accessibility were not measured in the latest no-debug run.

Keep important data in the normal browser. Cmd+Q only the preview; verify its printed PID/helpers exited before moving that one marked directory to Trash. Profiles/logs/captures stay private. Launcher deletes nothing automatically.

## Terminal cleanup and next work

Owned native apps/helpers/debug endpoints and fixture servers are closed. Private evidence/profiles retained; QA/public-pin Profile and Arc untouched. OS appearance/reduced-motion preferences and permissions were preserved. Owned idle-sleep assertion was stopped only after exact executable/arguments/start-time verification. Recurring prompt stopped; terminal process/fixture-port checks passed. No active delegate or build remains. This run closed before the hard deadline with release gates open.

Next investigation: define and verify acknowledged workspace/native-session persistence boundaries without duplicating browser stores or weakening checks. Retain immediate failures separately from clean-persisted controls. Native suites and P0 CLI/MCP remain release work, not completed tasks.
