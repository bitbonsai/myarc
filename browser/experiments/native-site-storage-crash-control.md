# Native site-storage crash control

## Scope

Same patched Chromium 158 build, fresh marked disposable Profile, both myarc experiment flags absent. This is a flag-off native-path control, not an independent pristine upstream build. The unmodified localStorage service configures a five-second default commit delay with possible further limiting; that source value is not measured persistence latency.

## Observed runs, 2026-10-26

A clean native session roundtrip first retained the fixture URL with no note. Real fixture form submission then returned the new localStorage value. Actual owned browser SIGKILL, plain relaunch without URLs/restore flag and native Restore followed:

- Warm browser with its native `Crashed` marker already on disk: signal arrived 12.098ms after the returned write acknowledgement. Genuine Restore returned the page; the new note was absent.
- Separate write with a requested seven-second dwell: observed acknowledgement-to-signal interval 7225.205ms, including guards. Genuine Restore returned the exact new note.

These are single observations, not a commit-latency benchmark or reliability bound. They reproduce recent-write loss with the workspace experiment inactive. They do not establish immediate site-storage durability, workspace acknowledgement durability or power-loss behavior. No storage implementation, verifier or OS setting changed.

## Excluded startup attempt

An earlier signal 11.969ms after acknowledgement happened soon after clean relaunch. The on-disk native exit marker still read `Normal`; plain startup offered no recovery prompt. That attempt is excluded from recovery-choice acceptance. A controlled navigation of the existing owned NTP target back to the known fixture found no note; it was not labeled session restoration. Later controls required the actual native `Crashed` marker before the write, without editing it.

A stock omnibox popup also changed the focused native window after Cmd+L. The exact-window input guard stopped before typing; it was not relaxed. The controlled revisit reused the existing target, created no CDP target and was recorded separately.

## Evidence and limits

[Structured process-crash evidence](../evidence/workspace-process-crash.json) retains the workspace failures and bounded persisted-data successes. Raw canaries, Profiles, process audits and UI logs remain private under `/tmp/myarc-m0-runtime/stock-storage-20261026T010219Z/`. All owned app/helper processes and the fixture server were closed. A pristine upstream runtime comparison and early-startup recovery guarantees remain unmeasured. Renderer stays off by default.
