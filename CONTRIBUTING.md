# Contributing

Start with [AGENTS.md](AGENTS.md), the [roadmap](ROADMAP.md), and the [ranked work](docs/GAPS.md). This repository holds plans, synthetic studies and a bounded Rust/native Chromium experiment. A complete myarc browser is still unimplemented.

Choose a task with clear dependencies and a definition of done. Keep changes narrow enough to review, describe the user-visible result, and attach evidence that supports completion. For docs, run `python3 tools/check_docs.py` and review consistency. Rust changes also need the standalone checks and patch-freshness check in [the foundation recipe](browser/README.md). Future application work needs checks appropriate to its data, security, and interaction boundaries.

Behavioral research needs source/date/confidence and an original summary. Do not attach raw user exports, private URLs, personal screenshots, or vendor assets to public issues or pull requests. Use synthetic examples. Report suspected vulnerabilities privately once an actively monitored reporting channel is established; do not disclose exploit details in public issues.

Original contributions use the repository's GPL-3.0 license. Releases and commits before the switch on 29 October 2026 remain available under MIT. Third-party components retain their own licenses; document them before inclusion. No contributor agreement or external account is required to read or build the standalone core.
