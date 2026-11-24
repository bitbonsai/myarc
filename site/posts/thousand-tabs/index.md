---
title: "A thousand tabs, and agents that can see"
summary: "Every alpha check is done against the real notarized app. Agents can now take screenshots, scroll, and wait for pages to load, and myArc handled a thousand tabs without flinching."
date: 2026-11-23
draft: false
redirect_from: [alpha-checklist-and-agent-control]
---

Yesterday alpha went out. That felt great, and it also left a short list of things I wanted to prove against the real signed app instead of trusting memory or unit tests. Could an outside agent really drive a session from scratch? Would the browser stay calm with a thousand tabs sitting in the sidebar? Could you open it offline without an account prompt or a single call home? Nightly `2026.11.23.1` answers all three, and the numbers came back better than I expected.

## Giving agents eyes

When I first wired up browser control, agents worked purely from text outlines of the page. That keeps clicks fast and cheap, and it is still how most steps run.

Still, once I hooked Claude Code up to the real packaged app and watched it fill forms and ignore prompt-injection text on a local test page, three gaps showed up right away. Sometimes an agent genuinely needs to look at what rendered. Sometimes the button it wants sits below the fold. Guessing with fixed sleeps while a page finishes loading is fragile too.

So this nightly adds `page.screenshot`, `page.scroll`, and `page.wait` to both the `myarc` CLI and the MCP server. Screenshots come straight from Chromium's compositor, scaled down to fit cleanly, and arrive over MCP as native image blocks. Scrolling moves the viewport or a target element in any direction. Waiting listens for Chromium's real load event with a hard timeout instead of a timer. Everything stays inside the Agent Space, behind the same Allow card and Stop button, while your own Spaces and Profiles stay out of reach.

![Side-by-side comparison of the same local test form in Arc on the left with its purple top localhost banner and myArc on the right with a thin blue viewport border and compact port tag in the URL pill](localhost-comparison.png)

*The same local test page in Arc (left) and myArc (right).*

You can also spot a small localhost touch in that comparison. Arc puts a heavy purple developer banner across the top of local pages. myArc wraps the viewport in a thin blue border and puts a compact `</> :port` tag right into the URL pill, so you always know you are looking at a local server without losing vertical space.

## Small details that stood out

Spending an afternoon staring at captures has a funny side effect. Tiny visual rough edges jump off the screen.

The Agent Space icon was a thin outline sitting next to heavier filled icons, so I swapped it for the filled robot head I created for [agentbar.fyi](https://agentbar.fyi) in both the Space header and the sidebar footer. While I was in there, I cleared a stray opaque backdrop that was dulling frosted modals, and removed the hover plates behind pinned extension icons in the URL pill so they use the same frosted tooltips as the footer.

![Side-by-side close-up of the myArc sidebar header and bottom footer showing the new filled robot-head icon for the Agent Space](agent-space-icon.png)

*The filled robot-head icon in the Agent Space header and bottom footer.*

## A thousand tabs and no calls home

The test I was most curious about was scale. People who loved Arc tend to hoard tabs, and I wanted real numbers instead of guesses. I seeded a fresh profile with ten Spaces and a hundred dormant items each, a thousand items in all, and measured what happened on an Apple M5 Pro.

The first window showed up in 797.5 ms. Steady-state memory settled at 953.5 MB across 11 processes. Switching Spaces took 75.5 ms at the median and 83.1 ms at p95, with workspace saves finishing in 2.08 ms. The command bar stayed responsive through all of it.

![Full myArc window with a thousand synthetic items across ten Spaces and the frosted command bar open in the center](thousand-tabs.png)

*A thousand dormant items across ten Spaces, with the command bar open over the window.*

I also ran hands-free checks on the everyday pieces that are easy to take for granted: native notification prompts, frosted find-in-page, docked DevTools, and SQLite-backed history. One last run blocked external DNS completely and logged every network request from a brand-new profile. myArc opened straight into a workspace without asking for an account, pinned a local page to disk, and made zero calls to any myArc server.

![myArc showing frosted find-in-page highlighting matches on a local test page](core-smoke-find.png)

*Find-in-page highlighting matches inside the glass bar.*

![myArc showing docked Chromium DevTools inspecting the local test page](core-smoke-devtools.png)

*Docked DevTools inspecting the same page.*

## Moving in for real

After this round, I am ready to make myArc my default browser. Having the latest Chromium underneath, with ungoogled-chromium and extra privacy patches on top, paired with Arc's way of organizing tabs, is the browser I wanted from the start.

I also set up [myarc.slirc.app](https://myarc.slirc.app) to collect feedback and talk through what comes next. If you are trying the nightly, come join and share what works or what still feels rough.

If you already have myArc Nightly installed, open **myArc > Check for Updates…** to pull `2026.11.23.1`. Fresh installs are on the [download page](https://myarc.dev/download/).
