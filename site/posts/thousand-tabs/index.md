---
title: "A thousand tabs, and agents that can see"
summary: "Every alpha check is done against the notarized app. Agents can take screenshots, scroll, and wait for pages to load, and myArc opened a thousand tabs in under a second."
date: 2026-11-23
draft: false
redirect_from: [alpha-checklist-and-agent-control]
---

Alpha shipped yesterday, so I ran the checklist against the real signed app instead of trusting unit tests. Build `2026.11.23.1` is what came out of that pass.

## Giving agents eyes

When I first added browser control, agents only got a text outline of the page. That's still how most steps run because it's fast and cheap.

Watching Claude Code drive the packaged app showed what was missing. It filled forms and ignored prompt-injection text on a local test page, then hit a wall whenever it needed to see what actually rendered, scroll to a button below the fold or wait for a page without guessing a sleep time.

This round adds `page.screenshot`, `page.scroll` and `page.wait` to the `myarc` CLI and MCP server. Screenshots come straight from Chromium's compositor and arrive over MCP as image blocks. Scrolling moves the viewport or a specific element. Waiting listens for the real load event with a timeout instead of sleeping. All of it stays inside the Agent Space, behind the same Allow card and Stop button, with your other Spaces and Profiles off limits.

![Side-by-side comparison of the same local test form in Arc on the left with its purple top localhost banner and myArc on the right with a thin blue viewport border and compact port tag in the URL pill](localhost-comparison.png)

*The same local test page in Arc (left) and myArc (right).*

Arc marks local pages with a purple banner across the top. myArc uses a thin blue border around the viewport and a `</> :port` tag in the URL pill instead, so you still see you're on localhost without giving up vertical space.

## A few small fixes

Looking through screenshots all afternoon made a few rough edges hard to ignore.

The Agent Space icon was a thin outline next to filled ones, so I swapped it for the filled robot head I made for [agentbar.fyi](https://agentbar.fyi) in both the Space header and the sidebar footer.

While cleaning that up, I cleared a stray opaque backdrop behind the glass modals and dropped the hover boxes behind pinned extensions in the URL pill so they use the same tooltips as the footer.

![Side-by-side close-up of the myArc sidebar header and bottom footer showing the new filled robot-head icon for the Agent Space](agent-space-icon.png)

*The filled robot-head icon in the Agent Space header and bottom footer.*

## A thousand tabs and no calls home

People who used Arc tend to hoard tabs, so I wanted to see how myArc holds up under real weight. I loaded a fresh profile with ten Spaces and a hundred dormant items each, a thousand in all, and ran it on an Apple M5 Pro.

The first window opened in 797.5 ms, and memory settled at 953.5 MB across 11 processes. Switching Spaces took 75.5 ms at the median (83.1 ms at p95), with workspace saves at 2.08 ms. The command bar stayed quick through all of it.

![Full myArc window with a thousand synthetic items across ten Spaces and the frosted command bar open in the center](thousand-tabs.png)

*A thousand dormant items across ten Spaces, with the command bar open over the window.*

I also ran automated checks on the basics you only notice when they break: native notification prompts, frosted find-in-page, docked DevTools and SQLite history. Then I blocked external DNS and logged every network request from a clean profile. It opened straight into a workspace with no account prompt, pinned a local page to disk and made zero calls to any myArc server.

![myArc showing frosted find-in-page highlighting matches on a local test page](core-smoke-find.png)

*Find-in-page highlighting matches inside the glass bar.*

![myArc showing docked Chromium DevTools inspecting the local test page](core-smoke-devtools.png)

*Docked DevTools inspecting the same page.*

## Moving in

After this pass, I'm switching to myArc as my default browser. Current Chromium underneath, ungoogled-chromium and extra privacy patches on top, and Arc's way of organizing tabs is what I wanted from the start.

I also set up [myarc.slirc.app](https://myarc.slirc.app) for feedback and ideas on what comes next. If you're trying the alpha, drop in and tell me what works or what still feels rough.

If you already have myArc installed, open myArc → Check for Updates… to get `2026.11.23.1`. Fresh installs are on the [download page](https://myarc.dev/download/).
