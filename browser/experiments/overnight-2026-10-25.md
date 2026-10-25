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

A bounded checkpoint/verify extension of the retained native developer client is delegated. The lead will prepare the synthetic workspace and own the actual process kill, relaunch and recovery UI. No process-crash acceptance run has completed in this window yet.

## Results and handoff

In progress. This record will contain verified commits, actual checks and remaining blockers when the window closes. A stopped or interrupted session must remain distinguishable from eight hours of completed work.
