# Workflow

myArc is built in small loops, one task at a time:

1. The lead picks the next Arc screenshot from `research-private/arc/`, or a logic task such as Space deletion.
2. One agent gets that one task and its acceptance check. For visual work the check is a side-by-side with the Arc screenshot.
3. The lead reviews the side-by-side and the test output, commits to `main` and picks the next task.

Chromium work runs one task at a time in the shared checkout. Rust, CLI/MCP, site and docs work runs in its own worktree alongside it. [AGENTS.md](../AGENTS.md#workflow) has the commands and rules.

Four rules keep the loop honest:

- Every claim needs fresh evidence: a screenshot from this run, and tests that actually ran. A run that selects no tests is not a success. See [VERIFY.md](VERIFY.md).
- Once a behavior passes, its check stays. Don't weaken a baseline to clear a failing change.
- [Behavior contracts](../.plans/BEHAVIOR-CONTRACTS.md) are only for logic rules such as archiving, Peek and Space deletion. For visual work the screenshot is the spec.
- Significant outcomes and failed attempts go in [the development log](../log/DEVLOG.md).

The project first tried an adaptation of the [PhotoCraft repository's](https://github.com/storytold/photocraft/blob/main/AGENTS.md) practices. Its planning ceremony didn't pay off for a browser, so this loop replaced it.
