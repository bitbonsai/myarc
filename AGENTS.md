# Working on myarc

Read [README.md](README.md), [.plans/INDEX.md](.plans/INDEX.md) and [docs/GAPS.md](docs/GAPS.md) first. Current work is MA-007 native visual parity ([.plans/EXECUTION.md](.plans/EXECUTION.md)).

## Product guardrails

- No mandatory account, AI, subscription or hosted service for local browsing, migration, recovery and export.
- Animated onboarding is a release requirement ([docs/ONBOARDING.md](docs/ONBOARDING.md)); static screenshots do not close it.
- Spaces and Profiles stay distinct. Several Spaces may share one Profile.
- Boosts are in scope, Easels are out. Imported executable customization stays disabled until deliberately enabled.
- AI is off by default (off/local/user-key/hosted). No silent cloud fallback or background page uploads.
- CLI and MCP browser control are P0, built on shared scoped commands, not deferred behind AI or treated as test tooling.
- Keep real Chromium extension and browser services; no generic webview shortcut.

## Git identity

Commit as `bitbonsai <44185+bitbonsai@users.noreply.github.com>` (repo-local config). Verify author and committer before committing. No AI attribution lines.

## Architecture

Rust owns workspace, import, persistence and commands. Chromium C++/GN stays upstream style. Keep FFI minimal, no panics across FFI, no unsafe in core crates. First-party JS: arrow functions, single quotes, no semicolons.

Treat pages, exports, model output, Boosts and automation messages as untrusted: bound size, recursion, time and allocations, and return actionable errors instead of dropping user data. Use Chromium's sandbox and profile machinery; never duplicate credential stores or put secrets in workspace formats, logs or fixtures. Acknowledged workspace mutations must survive the documented crash model.

## Workflow

The lead agent owns the queue and gives each agent one small task: goal, files, one acceptance check, stop condition. Then it reviews, runs the native check and commits. Lanes:
- Chromium: one task at a time in the shared checkout `~/.cache/myarc-m0/chromium/src` on branch `myarc`. myarc code is symlinked from `browser/foundation`; export upstream edits per file with `python3 tools/export_patches.py`.
- Observe: scripted Arc/native captures. References stay in `research-private/`.
- Off-Chromium: Rust core, CLI/MCP, site, docs.

Wrap builds and browser launches in `/usr/bin/lockf -k /tmp/myarc-build.lock`. Each task logs 3 lines in [log/DEVLOG.md](log/DEVLOG.md). Update `.plans/INDEX.md` at most daily.

## Commands

`python3 tools/check_docs.py`, `cargo fmt --all -- --check`, `cargo clippy --workspace --all-targets --locked -- -D warnings`, `cargo test --workspace --locked`, `python3 tools/export_patches.py --check`, `node --test design/onboarding/state.test.mjs`. Native runtime checks and evidence rules: [docs/VERIFY.md](docs/VERIFY.md).

## Hygiene

No proprietary source, brand assets, browsing screenshots, real profiles, exports, cookies or keys in the repo. Synthetic fixtures only. Third-party code and assets go in `ATTRIBUTION.md`.
