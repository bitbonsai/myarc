# Working on myarc

## Orient before changing anything

Read [README.md](README.md), [ROADMAP.md](ROADMAP.md), [plan/STATUS.md](plan/STATUS.md), [docs/GAPS.md](docs/GAPS.md), and the task in [plan/EXECUTION.md](plan/EXECUTION.md). Read the relevant architecture and behavioral documents. Explicit user instructions control the current scope; these files do not authorize unrelated external actions.

This is currently a planning repository. No browser or importer exists. Do not describe planned commands, crates, tests, animations, or integrations as implemented.

Use [docs/ARTCRAFT-WORKFLOW.md](docs/ARTCRAFT-WORKFLOW.md) for the adapted working method, [plan/BEHAVIOR-CONTRACTS.md](plan/BEHAVIOR-CONTRACTS.md) for cross-system interactions, and relevant recent entries in [log/DEVLOG.md](log/DEVLOG.md) when resuming an investigation.

## Protect the product

- No mandatory account, AI, subscription, or hosted service for local browsing, migration, recovery, and export.
- Animated onboarding is a release requirement. Follow [docs/ONBOARDING.md](docs/ONBOARDING.md); static screenshots cannot close that task.
- Preserve the distinction between Spaces and Profiles. Several Spaces may use one Profile.
- Keep Boosts in scope and Easels out. Imported executable customization stays disabled until deliberate enablement.
- AI must support off/local/user-key/hosted choices, with off as the default. No silent cloud fallback or background page uploads.
- Preserve real Chrome extension/browser services. Do not replace the foundation with a generic webview just to simplify the UI.

## Architecture and style

Use Rust for the myarc-owned workspace, importer, persistence, and command core. Preserve upstream Chromium C++/GN conventions and use its supported Rust integration. Keep the FFI boundary minimal and explicit about ownership, threads, errors and cancellation. Core crates should not depend on UI or provider clients. The UI, command bar, importer and test driver must not implement separate copies of workspace rules.

For first-party JS/TS that is necessary: arrow functions, single quotes, no semicolons, unless generated code or an upstream-required format makes that impossible. Rust follows rustfmt and appropriate clippy rules. Do not force Rust into upstream areas where it creates a second browser platform.

Do not introduce speculative frameworks, empty crates, mock buttons, placeholder tests, or commands that claim success without real work. Add the smallest build/test harness required by the next deliverable. Use `xtask` only once Rust implementation/tooling exists; there is no working `cargo xtask ci` today.

## Reliability and boundaries

Treat web pages, exports, model output, Boosts and automation messages as untrusted. Bound input size, recursion, time, indexing and allocations. Return actionable errors; do not silently drop user data. Do not add panic paths in shipped Rust logic. Isolate and justify unavoidable unsafe FFI; safe core crates forbid it. Never unwind a panic across FFI.

Use upstream browser security/profile machinery; do not bypass sandboxing or duplicate credential stores. Keep secrets out of workspace formats/logs/fixtures. Import from consistent read-only snapshots and commit target state transactionally. Acknowledged workspace mutations must survive the documented crash model.

## Working loop

1. Select an unblocked task or follow the user's concrete request. State assumptions and acceptance criteria.
2. Find its evidence/source and affected boundary. If behavior is unknown, record the question and investigate rather than inventing parity.
3. Implement a complete, narrow journey through model, command and UI where relevant.
4. Verify with checks proportional to the change. Prefer semantic round trips, fault injection and real browser integration for data/security boundaries. Documentation-only edits need link/consistency review, not fabricated application tests.
5. For motion/interaction changes, inspect a running build, record the relevant sequence and verify reduced motion/focus. Use synthetic data for public artifacts.
6. Update status, parity and blockers with evidence, then commit a coherent change. Record what remains unmeasured.

Treat MA-001–MA-010 as workstreams. Select one bounded outcome and use a task suffix for implementation slices, with explicit acceptance evidence and a stopping condition. Record significant outcomes and failed attempts in the development log. Verify that filtered tests actually executed and that visual/output artifacts are fresh from the reported run. A zero-test exit or stale screenshot is not proof.

Retain regression checks for established behaviors; change baselines only with an explained product/measurement decision. Once browser UI exists, audit visible settings and commands for an observable effect. Disable/explain or omit unsupported controls rather than presenting a working-looking no-op.

Do not mark a task complete based on code volume, a screenshot, build success alone, or a simulated backend. Never invent productivity multipliers, implementation percentages, benchmark results, or successful test runs. Propose parallel ownership only when it would help; obey the session's rules about authorizing additional agents.

## Sources and public repository hygiene

Write original behavior specs from public documentation and appropriately permitted observations. Keep provenance and access limitations in [docs/RESEARCH.md](docs/RESEARCH.md). Do not copy proprietary source, brand assets, private standards, or unlicensed reference media. Record any introduced third-party code/assets and their licenses in `ATTRIBUTION.md`.

Commit publishable `plan/` tasks and decisions. Never commit real browser profiles, raw user exports, browsing screenshots, cookies, API keys, machine-specific credentials or private research notes. Synthetic fixtures must use invented content. A public-repo request is not permission to publish unrelated personal context.

## Actual verification available now

Run `python3 tools/check_docs.py` from the repository root for relative document links. It is a local documentation check, not a browser test or security scanner. Read [docs/SCORECARD.md](docs/SCORECARD.md) for the future release evidence.

The documentation workflow runs this same narrow check in GitHub Actions. Adding CI does not enforce branch protection or implement Rust/application gates.
