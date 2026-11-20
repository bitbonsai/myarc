# Crash report fixture

`myArc-2026-11-20-150000.ips` is a synthetic macOS crash log in the `.ips` layout: one JSON header line, then the report. Every identifier is zeroed. The crash report prompt reads it through `--myarc-test-crash-logs=<this dir>` in test-hook builds, so captures never read the real `~/Library/Logs/DiagnosticReports`. The identifying keys (`crashReporterKey`, `storeInfo`, `userID`, `incident`) are there to show the prompt drops them.
