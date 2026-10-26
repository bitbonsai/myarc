# Native overnight work record

Window: 2026-10-25 21:38 UTC through 2026-10-26 05:38 UTC. Status: active.

Baseline: `d82f4f2`. [Background and saved journeys](../evidence/page-background.json) and [one explicit clean restart](../evidence/workspace-clean-restart.json) passed before this window. Those results do not establish crash recovery or OS durability.

## Working decisions and boundaries

- Preserve existing global cross-Space search/switching provisionally. Activation reveals the owning Space. This is a conservative implementation choice, with Arc parity still unverified.
- Use one bounded implementation delegate at a time, followed by lead review, integration and real native checks.
- Prioritize process-crash recovery and multiwindow identity, then interruption/focus/motion and visible-control gaps. Keep the workspace renderer off by default.
- Use marked synthetic profiles only. Preserve the existing QA profile. Locked desktop permits code/standalone work only; permission prompts and lock/focus guards remain intact.
- Keep private captures and profiles outside Git. No pushes, deployments or history rewrites.

## Current work

Checkpoint/verify probe integrated as `5bf05df`; thirteen local tests passed. The first actual process-kill run restored both URLs but failed stable identity: the pending recovery prompt allowed Temporary pruning before Restore. [Failure evidence](../evidence/workspace-process-crash.json) records that baseline.

0018 integrated as `2a0ef32`. Its first actual Restore retained pending metadata and original IDs, but full verification failed on a freshly acknowledged localStorage canary. Unmodified upstream DOMStorage configures a five-second default commit delay, with further rate limiting possible. No durability policy or verifier was weakened.

A separate persisted-data control passed: clean quit/reopen first proved the same canary existed, then audited SIGKILL, plain relaunch and genuine Restore passed all five strict comparisons. A fresh explicit-discard case retained Favorite/Space identity and released readiness; creating a new Space persisted afterward. These bounded results establish neither immediate acknowledgement nor OS/power-loss durability. [Run details](native-recovery-lifecycle.md).

Safe preview launcher integrated as `c263228`; ten local double-based tests passed. Parent ran its exact command against the real app, observed a marked fresh profile without debugging/test-extension flags, navigated to Example Domain through the native palette, created/switched a Space and inspected its neutral empty header/material. App/helpers closed.

Two-window probe integrated as `2b98a0a`; all twenty local tests passed, including the original thirteen. [Actual native journey](../evidence/workspace-two-window.json) retained one logical Favorite across two native copies, close-one/reopen and audited process-kill/plain relaunch/genuine Restore. Five strict samples retained both native windows/pins and stable organization. Window-specific activation changed only its own selection. Apps/helpers and owned server closed. Arbitrary late/overlapping restore remains open. [Native private-window boundary checks](../evidence/workspace-native-private.json) then passed: real Cmd+Shift+N, independent workspace/storage, last-private-window close and fresh private reopen. Regular metadata stayed unchanged; the prior private note and Space were absent afterward. Visible memory-only notice and native Incognito accessibility title were inspected. Apps/helpers/server closed. [Two regular Profiles in one process](../evidence/workspace-native-profiles.json) also retained separate workspace files, Favorites and same-origin localStorage. Native Profiles > Add Profile worked without an account; the duplicate CLI route respected its lock rejection. A native typed query in the fresh other Profile omitted the primary-only tab. Apps/helpers/server closed.

Found a runtime-mode bootstrap gap: a newly created Profile starts horizontal, and its real vertical-tab toggle leaves stock controls without myarc workspace initialization. Reopening its window after the native toggle worked, allowing the isolation checks above. 0019 integrated as `78b303f`. First native enable exposed a missed readiness notification; parent corrected it, rebuilt and verified same-window Space controls, three mode cycles and horizontal focus with unchanged page/model identity. [Further cold horizontal restart failed](../evidence/workspace-mode-bootstrap.json): late activation dropped Temporary identity and duplicated the Favorite. This is a workspace restore-binding blocker, separate from recent site-storage batching. 0020 integrated as `7a14927` and [passed bounded native followups](../evidence/workspace-late-bindings.json): clean horizontal restart before late UI, real crash/recovery with vertical enable while the prompt waited, and two-window horizontal restoration retained IDs/owners/selection and both pins. Three reduced-motion cycles preserved pages/model identity and working horizontal focus; fresh held-state recordings were inspected. Closed-window candidate retention matched the existing contract and later startup retired it. Apps/helpers/servers closed. Fresh post-0020 native private disposal/mode cycling and two regular Profile workspace/note isolation passed. Other Profile initialized horizontally and enabled its sidebar in the same window without reopening. Flag-off horizontal/vertical/horizontal kept its page and created no workspace UI/file. Those apps/helpers/servers closed; cookies/credentials, Guest/system and broader overlap/races remain open. Verifiers stayed strict; recent site-storage acknowledgement and broader durability remain open.

## Tested preview command

```sh
python3 tools/launch_native_preview.py
```

Existing build host only. Each run creates a fresh disposable profile and prints its PID/log paths. Experimental sidebar flags are explicit; ordinary app launches remain unchanged. Keep important data in the normal browser. Follow the [short manual checklist and cleanup instructions](../../tests/fixtures/browser/README.md#native-preview-five-minute-manual-smoke). This developer launcher and acceptance CDP probes do not implement the P0 product CLI/MCP controls.

## Results and handoff

In progress. Final handoff will include a short change summary with verified commits, actual checks and remaining blockers. It will also provide an exact tested launch command for a fresh disposable preview profile and a short manual test checklist with expected results. Experimental flags, profile protection and known limitations must be explicit. A stopped or interrupted session must remain distinguishable from eight hours of completed work.
