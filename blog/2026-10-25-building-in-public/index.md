HTML: https://myarc.dev/blog/2026-10-25-building-in-public/

[All posts](https://myarc.dev/blog/)

# Building myArc in public

October 25, 2026

By [bitbonsai](https://github.com/bitbonsai)

[![Mauricio Wolff's Arc card, dated 25 March 2023, with three anniversary stamps](https://myarc.dev/blog/2026-10-25-building-in-public/mauricio-wolff-arc.c739650f7b1d.webp)](https://myarc.dev/blog/2026-10-25-building-in-public/mauricio-wolff-arc.c739650f7b1d.webp)

My Arc card, 2023\.

I started using Arc in 2023\. I appreciate how Spaces and named pinned tabs make its sidebar feel like a workspace. The command bar gives you a quick way back to those tabs.

[Arc is being sunsetted](https://arc.net/). I wanted to keep that way of browsing alive. Why not build an independent, open-source continuation, with privacy as a starting point and AI something people could choose to use? That idea became myArc. For now, it's a Chromium-based prototype.

## A browser should work without an account

Local browsing, migration and export must work without an account or subscription.

I'm committing to no behavioral tracking: myArc will not send browser-usage analytics or create per-install tracking identifiers. Browsing history and workspace data stay local by default. Before release, an audit must check Chromium's defaults and network behavior against those commitments.

I can follow interest through GitHub's aggregate release-download totals, without instrumenting people's browsing. Website and download hosts may still keep operational request logs. Those need separate disclosure.

## Trying Artcraft's way of working

I'm using [Artcraft's public engineering practices](https://github.com/bitbonsai/myarc/blob/main/docs/ARTCRAFT-WORKFLOW.md) as a starting point. PhotoCraft's development guide separates model tests from visual checks in the native app.

Moving navigation controls into the sidebar caused startup and layout failures. Using Chromium's native controls fixed those failures.

The address field lost keyboard focus when it was hidden and shown during layout. Keeping it visible fixed navigation. When those controls move, startup and keyboard navigation both need rechecking in the native app.

Chromium provides myArc's browser services, while Rust handles the workspace logic we own. The agent tasks follow that split.

Each coding agent gets a narrow job and a result I can check. Today's tasks cover the Rust Spaces model and a workspace service owned by a Chromium Profile. Another agent is wiring that state into the native sidebar.

A separate agent reviews the change. Findings go back for another implementation pass. The lead agent brings the Rust and Chromium changes together, builds the browser and checks the native window.

## Keeping context between sessions

I've open-sourced two skills under bitbonsai: [context](https://github.com/bitbonsai/context) and [wrap](https://github.com/bitbonsai/wrap).

Context keeps agent instructions and architecture notes current. A new agent needs to see which parts belong to Chromium and which belong to myArc, and distinguish planned work from working code.

Wrap saves useful gotchas and writes a handover for the next session, so the startup and focus failures can inform the next pass. Private and machine-specific notes stay outside the public repo.

[myArc's code is open source too](https://github.com/bitbonsai/myarc).

## The current build and the target

Page navigation and the centered native command palette already work in the prototype. Today's goal is an Arc-like empty window and a native nightly build people can try without compiling Chromium.

[![Current myArc build loading mauriciowolff.com with the native command palette open](https://myarc.dev/blog/2026-10-25-building-in-public/myarc-current-build.f594a2a31d6a.webp)](https://myarc.dev/blog/2026-10-25-building-in-public/myarc-current-build.f594a2a31d6a.webp)

myArc, with the native command palette open.

[![Arc reference for the window material, sidebar and centered command bar](https://myarc.dev/blog/2026-10-25-building-in-public/arc-reference.3418f314f546.webp)](https://myarc.dev/blog/2026-10-25-building-in-public/arc-reference.3418f314f546.webp)

Arc, our visual reference.
