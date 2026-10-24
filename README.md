# myarc

**Your browsing space, yours to keep.**

An independent browser project for people who love Arc's workflow: a calm sidebar, Spaces, persistent tabs, fast commands, thoughtful motion, and control over their own setup.

**Status: planning repository. No browser, importer, or onboarding animation is implemented yet.** The first milestone proves the browser foundation and specifies the animated first-run experience. This repository does not contain Arc source code or assets and is not affiliated with The Browser Company or Atlassian. `myarc` is a working name; public product naming remains open.

## Product commitments

- Browse without an account. No service outage or subscription expiry may disable local browsing or access to local data.
- Preserve the Arc workflow and make migration a first-class feature.
- Launch with a beautifully animated onboarding experience that teaches by doing.
- Support Chrome extensions through a proven browser implementation, with published compatibility results.
- Make AI optional: off by default, with local models, user API keys, and an optional paid hosted service.
- Include Boosts, beginning with appearance, CSS, and element hiding. No Easels.
- Keep workspace data local and exportable. Cloud services are optional conveniences.

## Start here

| Document | Purpose |
| --- | --- |
| [Product plan](docs/PRODUCT.md) | Audience, advantage, scope, and commercial direction |
| [Roadmap](ROADMAP.md) | Milestones, dates, dependencies, and release gates |
| [Animated onboarding](docs/ONBOARDING.md) | Storyboard, interaction design, motion, and acceptance criteria |
| [Architecture](docs/ARCHITECTURE.md) | Chromium recommendation, alternatives, and foundation experiment |
| [Arc migration](docs/MIGRATION.md) | Import contract, data boundaries, recovery, and portable export |
| [AI and services](docs/AI.md) | Provider choices, privacy, and sustainable hosted billing |
| [Boosts](docs/BOOSTS.md) | Phased scope and safe execution |
| [Parity register](docs/PARITY.md) | What is wanted, what is known, and what is actually implemented |
| [Ranked gaps](docs/GAPS.md) | Work order and launch blockers |
| [Scorecard](docs/SCORECARD.md) | Proposed budgets and evidence needed to ship |
| [Agent workflow](AGENTS.md) | How to contribute without drifting from the product |
| [Execution plan](plan/EXECUTION.md) | First ten implementation tasks with definitions of done |
| [Research](docs/RESEARCH.md) | Primary sources, limitations, and Artcraft lessons |

## What makes this plan different

Migration and animated onboarding lead the work. Browser security, extension support, and data durability decide whether it can ship. AI monetization follows a useful browser rather than determining the browser experience. Feature breadth is tracked separately from demonstrated daily-use reliability.

The current recommendation is a **Rust core for myarc's workspace, importer, persistence, and commands**, integrated through a small patch set over Chromium's complete browser layer, subject to a one-week feasibility gate. Chromium's engine and existing browser services remain C++. This is not a commitment to write a browser engine, use Electron, or rebuild Chrome's extension platform.

## Immediate next step

Follow [plan/EXECUTION.md](plan/EXECUTION.md), starting with task MA-001. Work toward an Arc preservation tool and a safe desktop alpha before attempting full replacement. The December 2026 milestones are conditional targets, not release promises.

## Contributing

Read [AGENTS.md](AGENTS.md) and [CONTRIBUTING.md](CONTRIBUTING.md). Current documents and original tooling are MIT licensed; future upstream code retains its own notices and license obligations. A chosen browser foundation must receive a separate license review before integration or distribution.
