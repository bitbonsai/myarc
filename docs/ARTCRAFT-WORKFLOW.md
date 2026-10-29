# Working method

This started as an adaptation of public practices from the [photocraft repository](https://github.com/storytold/photocraft/blob/main/AGENTS.md). Most of the ceremony didn't pay off for a browser. Four habits stayed:

- One task per agent, with one acceptance check named before it starts. For visual work the check is a side-by-side with an Arc screenshot in `research-private/arc/`.
- Fresh evidence for every claim: a screenshot from this run, and tests that actually ran. A run that selects no tests is not a success. See [VERIFY.md](VERIFY.md).
- Once a behavior passes, its check stays. Don't weaken a baseline to clear a failing change.
- [Behavior contracts](../.plans/BEHAVIOR-CONTRACTS.md) only for logic rules such as archiving, Peek and Space deletion. For visual work the screenshot is the spec.

The day-to-day loop is in [AGENTS.md](../AGENTS.md#workflow). Significant outcomes and failed attempts go in [the development log](../log/DEVLOG.md).
