# Applying Artcraft's working methods

Reviewed 2026-10-24. This is an original adaptation of public repository practices. It does not reproduce private `craftrules` standards or establish that their delivery speed transfers to a browser.

## Scope of the adaptation

Adopt the public engineering methods that fit myarc: clear orientation, bounded work, shared command paths, explicit failure handling, and evidence attached to completion claims. Do not infer or copy an Artcraft business strategy. The relationship between its open repositories, commercial AI services, and marketing is unverified.

myarc may offer an optional paid AI service, but local browsing, migration, recovery, export, CLI, and MCP control must remain useful without it. Open development is a product and maintenance commitment, not a traffic tactic assumed from another project.

## What the public implementation teaches us

| Practice and primary evidence | Application to myarc | Available now / implementation trigger |
| --- | --- | --- |
| [Readiness assessment](https://github.com/storytold/photocraft/blob/main/docs/target-app-parity.md) distinguishes visible feature coverage from practical replacement quality and records revised estimates | Judge the import → browse → restore journey separately from the number of features present | Existing parity/scorecard plus explicit behavioral contracts; no completion percentage yet |
| [Development guide](https://github.com/storytold/photocraft/blob/main/docs/development.md) separates non-GPU behavior checks from native visual inspection | Exercise the Rust model/importer without Chromium; inspect browser integration and motion on a real supported host | Remove the browser-build prerequisite from standalone core work; integrate the same core later |
| Development guide verifies fresh artifacts and rejects successful commands that selected no tests | Record the tests actually executed and artifacts actually produced by that run | Apply to all implementation completion claims; no stale recording or zero-test success |
| [Scorecard](https://github.com/storytold/photocraft/blob/main/docs/scorecard.md) is generated from structured inputs and distinguishes failing, unrun, and unmeasurable scenarios | Keep evidence and targets separate; derive reports from measured records once those records exist | Documentation remains explicitly manual now; add a generator with the first real measurement harness |
| Scorecard exposes settings that have no effect | Every visible control needs an observable result; unavailable behavior is disabled with an explanation or omitted | Add a no-op-control audit to the browser release gate |
| [Agent instructions](https://github.com/storytold/photocraft/blob/main/AGENTS.md) organize shared commands, dependency rules and regression checks | Keep one command contract across UI/CLI; protect established behaviors as code grows | Document boundaries now; add executable layer/command checks with MA-004 |

## Concrete working rules

### Specify an interaction before implementing it

Use [behavior contracts](../plan/BEHAVIOR-CONTRACTS.md). Record the reference source/version, what is known, what is proposed, initial state, action, expected state, keyboard/focus behavior, persistence, failure path and proof required. Keep unknowns explicit. A help-center feature name alone cannot settle behavior.

Prioritize contracts that constrain multiple systems: Spaces/Profiles, pinned-tab lifecycle, import/reveal, and archive/recovery. The first three seeds are recorded; more contracts are added with the work that needs them.

### Ship a bounded result per work item

For approved scope, the lead agent owns the queue and continues after each verified slice. Delegate isolated implementation where useful, review feedback, integrate and inspect the running native app with fresh captures. Checkpoints report evidence and the next action; they do not wait for another user prompt. Escalate product decisions, authorization boundaries and blockers.

MA-001 through MA-011 are workstreams, not single-agent-sized tickets. Before starting, select a bounded outcome and use a suffix such as `MA-004.a`. Name the files/interfaces involved, dependencies, acceptance evidence and stopping condition. Keep the change small enough to understand in one review. Documentation changes do not require invented application tests.

Useful first slices are: a motion study for the post-import reveal with synthetic state; a reproducible unmodified upstream build; a verified inventory of one actual export version; or a standalone validated workspace round trip. Do not bundle all four into a single completion claim.

### Preserve useful failure knowledge

Record significant work in [the development log](../log/DEVLOG.md): task/outcome, starting revision/environment, sources, actual checks and artifacts, failed attempts and their cause, remaining blocker, next reproducible action. Keep entries concise and exclude personal data, credentials, raw exports and machine-specific secrets. Carry forward the relevant entry when resuming instead of repeating an unsuccessful investigation.

### Turn observations into checks progressively

The documentation check can run now. With the first Rust core, add formatting/linting, semantic model checks, malformed-input cases and command validation. With the importer, add synthetic corpus summaries and round-trip/fault checks. With the browser, add extension/profile integration and actual interaction recordings. Every harness must fail honestly and identify what ran.

Once a behavior has passed, retain a regression check. Keep numerical budgets on comparable machines and workloads; do not weaken a baseline just to clear a failing change. A deliberate product or measurement change needs a dated decision with impact and replacement evidence. Do not turn an aspirational, unmeasured target into an enforced performance claim.

## What we deliberately adapt

The Rust core is reusable; Chromium supplies browser services. WebAssembly parity is not a browser release requirement. Published plans stay committed; private reference material stays outside the public tree. Shared standards remain local to this project until another real consumer justifies extraction. Preserve these differences rather than reproducing an editor's directory tree mechanically.
