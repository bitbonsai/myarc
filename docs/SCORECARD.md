# Release scorecard

All results: **not measured**. Numbers below are proposed acceptance budgets to calibrate at M0/M2, not performance claims. Record exact build, upstream revision, OS, machine, workload, sample count, p50/p95 where applicable, and trace/log links.

| Area | Proposed gate | Evidence |
| --- | --- | --- |
| Account independence | Fresh install, start-fresh onboarding, local import, browsing and export work without a myarc account; local setup works with network disabled | Recorded journeys and request audit separating updates/site traffic from optional services |
| Animated onboarding | All acceptance criteria in [ONBOARDING.md](ONBOARDING.md) pass | Native recordings, screen-reader review, reduced-motion variant, interaction and frame traces |
| Migration | All supported semantic fields survive fixtures/round trips; every unsupported field is reported; zero source writes | Versioned corpus summaries, source hashes, duplicate/rollback tests |
| Workspace durability | Zero unrecovered state loss in the defined command-acknowledgement/fault-injection suite | Restart, kill, disk-full, corrupt-generation and schema-upgrade evidence |
| Profile isolation | Zero cookie/storage/history/extension/context leakage across test Profiles | Browser integration tests and manual adversarial cases |
| Private browsing | No private titles, URLs, archive entries, or AI history persisted to the workspace | Storage/network inspection after exit/crash |
| Extensions | Named representative suite passes install, use, restart, update, remove on every supported platform | Versioned matrix; failures/limitations visible; no “all extensions” claim |
| Upstream maintenance | Named owner and tested update path; initial target ≤72 h from an applicable urgent upstream security fix to a validated release | Two rehearsals, timestamps, sign/update logs; target reviewed against real capacity |
| Secure distribution | Signed/notarized macOS candidate; authenticated signed update; rejection of tampering/downgrade; recoverable install failure | Clean-machine install/update and negative tests |
| Responsiveness | Command bar interactive ≤100 ms p95; Space switch ≤100 ms p95 excluding page network/render | At least 100 samples on reference machine with 1,000-tab synthetic workspace |
| Startup and memory | Proposed steady workload regression ≤10% vs same upstream build; workspace usable ≤2 s warm/≤5 s cold | Fixed profile/page set; dormant vs loaded tab counts; CPU/RAM and cold-cache method |
| Accessibility | Core journeys work with keyboard, screen reader, zoom, OS contrast/reduced motion | Manual review plus appropriate automation; no broad compliance claim from a single scan |
| Daily use | No unresolved P0 issue after a 14-day consenting cohort trial | Redacted issue log, task outcomes, known limitations |
| AI off / provider failure | No inference/page upload when off; local/BYOK/hosted failure does not affect browsing | Request traces, denied network tests and secret/log review |
| Visible controls | Zero unexplained no-op settings/actions in supported journeys | Command/control inventory with observed effect, enabled condition and semantic check; unsupported controls disabled/explained or omitted |

## Extension test categories

Cover at least: MV3 service worker lifecycle, password manager with permitted native host, content blocker, content script/site styling, tab management, DevTools, storage/permissions, and keyboard commands. Select actual current versions at M0. Check vendor licensing and availability; do not distribute third-party binaries or secrets as fixtures. Include installation access and update delivery, not only unpacked developer loading.

## Reporting

Evidence lives with a dated build record and issue references. Unmeasured, unsupported, failed, and passed are distinct states. A gate may change only through a recorded decision explaining the user impact. Deadlines and attractive demos cannot waive security, privacy, migration integrity, or updater failures.

Record actual executed test names/counts and use fresh output directories for recordings/snapshots. Zero selected tests and artifacts left by an earlier run do not count. When comparable measurements exist, retain passing behavior checks and record before/after results; do not silently lower a regression baseline. Keep aspirational budgets distinct from demonstrated floors.

This scorecard is manually maintained today. With the first measurement harness, introduce structured evidence records containing scenario ID, build/revision, platform, workload/fixture, status, command, run time, measurement and artifact reference. Derive the report and verify freshness in CI. Do not scaffold a generator until it has real records to validate. The current documentation CI validates link paths only.
