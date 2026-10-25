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

Checkpoint/verify probe integrated as `5bf05df`; thirteen local tests passed. The first actual process-kill run restored both URLs but failed stable identity: the pending recovery prompt allowed Temporary item pruning before the user chose Restore. [Failure evidence](../evidence/workspace-process-crash.json) records the narrower result. Lifecycle repair is next; full crash recovery remains unaccepted.

## Results and handoff

In progress. Final handoff will include a short change summary with verified commits, actual checks and remaining blockers. It will also provide an exact tested launch command for a fresh disposable preview profile and a short manual test checklist with expected results. Experimental flags, profile protection and known limitations must be explicit. A stopped or interrupted session must remain distinguishable from eight hours of completed work.
