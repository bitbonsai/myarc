# Working method

This is an original adaptation of public practices from the [photocraft repository](https://github.com/storytold/photocraft/blob/main/AGENTS.md). It does not reproduce private standards or assume another project's delivery speed transfers to a browser.

## Rules

- Keep tasks small and bounded. Name the files, one acceptance check and a stopping condition before starting. MA-001 through MA-011 are workstreams; pick a slice such as `MA-004.a`.
- Attach fresh evidence to every claim. Say which tests ran and which artifacts this run produced. See [VERIFY.md](VERIFY.md).
- Harnesses fail honestly. A run that selects no tests is not a success, and a check must say what it covered.
- Once a behavior passes, keep a regression check for it. Don't weaken a baseline to clear a failing change.

Interactions are specified first in [behavior contracts](../.plans/BEHAVIOR-CONTRACTS.md). Significant outcomes and failed attempts go in [the development log](../log/DEVLOG.md).
