HTML: https://myarc.dev/blog/2026-10-25-building-in-public/

[All posts](https://myarc.dev/blog/)

# Building myArc in public

October 25, 2026

By [bitbonsai](https://github.com/bitbonsai) · [Read Markdown](https://myarc.dev/blog/2026-10-25-building-in-public/index.md)

Spaces and named pinned tabs make Arc's sidebar feel like a workspace. The command bar gives you a quick way back to those tabs.

[Arc is being sunsetted](https://arc.net/). I'm building myArc to continue that way of browsing, as an independent, open-source browser based on Chromium.

## A browser should work without an account

I'm positive about this one. Local browsing, migration and export must work without an account or subscription.

The privacy commitment is no behavioral tracking. myArc will not send browser-usage analytics or create per-install tracking identifiers. Browsing history and workspace data stay local by default. A release audit must check Chromium's defaults and network behavior against that commitment.

I can follow interest through GitHub's aggregate release-download totals, without instrumenting people's browsing. Website and download hosts may still keep operational request logs, which need separate disclosure.

## Trying Artcraft's way of working

I'm using [Artcraft's public engineering practices](https://github.com/bitbonsai/myarc/blob/main/docs/ARTCRAFT-WORKFLOW.md) as a starting point. PhotoCraft's development guide separates native visual checks from model tests.

Moving navigation controls into the sidebar caused startup and layout failures. Using Chromium's native controls fixed that path.

The address field had a different problem: hiding and showing it during layout lost keyboard focus. Keeping it visible fixed navigation. Startup and keyboard navigation both need rechecking when those controls move.

In myArc, Chromium provides the browser services and Rust handles the workspace logic we own.

Each coding agent gets a narrow job and a result I can check. Today's tasks cover the Rust Spaces model and a Profile-owned workspace service. Another agent is wiring that state into the native sidebar.

A separate agent reviews the change. Findings go back for another implementation pass. The lead agent brings the Rust and Chromium changes together, builds the browser and checks the native window.

## Keeping context between sessions

I've open-sourced two skills under bitbonsai: [context](https://github.com/bitbonsai/context) and [wrap](https://github.com/bitbonsai/wrap).

Context keeps agent instructions and architecture notes current so a new agent can see which parts belong to Chromium and which belong to myArc. Those notes also need to distinguish planned work from working code.

Wrap saves useful gotchas and writes a handover for the next session. Private and machine-specific notes stay outside the public repo.

[myArc's code is open source too](https://github.com/bitbonsai/myarc).

## The current build and the target

Page navigation and the centered native command palette already work. Today's goal is an Arc-like empty window and a nightly build people can try without compiling Chromium.

[![Current myArc build loading mauriciowolff.com with the native command palette open](https://myarc.dev/blog/2026-10-25-building-in-public/myarc-current-build.f594a2a31d6a.webp)](https://myarc.dev/blog/2026-10-25-building-in-public/myarc-current-build.f594a2a31d6a.webp)

myArc, with the native command palette open.

[![Arc reference for the window material, sidebar and centered command bar](https://myarc.dev/blog/2026-10-25-building-in-public/arc-reference.3418f314f546.webp)](https://myarc.dev/blog/2026-10-25-building-in-public/arc-reference.3418f314f546.webp)

Arc, our visual reference.
