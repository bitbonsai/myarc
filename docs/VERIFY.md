# Verification

## Evidence rules

- Build success alone proves nothing. Do not close a task on it, or on code volume, a screenshot or a simulated backend.
- Screenshots and recordings must be fresh from the run being reported.
- A filtered test run that executes zero tests is a failure, not a pass.
- Once a behavior passes, keep a regression check. Change a baseline only with a dated product or measurement decision.
- Every visible control needs an observable effect. Disable it with an explanation or omit it.
- Never report invented percentages, benchmarks or test runs. Record what remains unmeasured.
- Documentation-only edits need link review (`python3 tools/check_docs.py`), not made-up application tests.

## Native runtime checks

- Use a marked disposable profile and confirm the expected browser PID.
- Follow [tests/fixtures/browser/README.md](../tests/fixtures/browser/README.md). The checks crash a renderer on purpose and temporarily disable the test extension.
- Do not create default-context CDP targets until the recorded profile-initialization assertion is isolated. Reuse the existing test tab.
- The external Chromium build has its own [run record](../browser/README.md).

## Native evidence gotcha

A clean session seed goes stale after later URL or selection exercises. Re-prove the final session before a persisted-state crash control, and keep immediate-acknowledgement failures separate. See [native navigation loss](../browser/experiments/native-session-navigation-crash.md). A clean-seeded pass does not close acknowledgement durability.

## Onboarding browser check

`python3 tools/check_onboarding.py` checks prototype interactions, focus and recordings. It needs agent-browser, Chrome and ffprobe, and closes agent-browser and Chrome for Testing helpers when done. Read the [study instructions](../design/onboarding/README.md) first.

## Release evidence

Standalone and study checks do not validate production browsing, import or security. Native and browser CI is not available yet. See [docs/SCORECARD.md](SCORECARD.md) for the evidence still required for release.
