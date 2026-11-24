HTML: https://myarc.dev/blog/thousand-tabs/

[All posts](https://myarc.dev/blog/)

# A thousand tabs, and agents that can see

November 23, 2026

[Mauricio Wolff](https://github.com/bitbonsai)4 min read

Alpha went out yesterday, and I wanted to put the signed app through a real test to see what breaks. I let an agent drive it and stuffed a thousand tabs into the sidebar. Then I cut the internet to see if it ever called home.

Build `2026.11.23.1` is what came out of that.

## Giving agents eyes

When I first wired up browser control, agents only got a text outline of the page. Most of the time that's plenty, because it's fast and doesn't burn tokens on images.

Then I pointed Claude Code at the packaged app on a local test page. It filled out the form and ignored the prompt-injection text I'd hidden there, which was nice to see. Still, a text outline only goes so far when an agent needs to see how something rendered or reach a button off-screen.

So I added `page.screenshot`, `page.scroll` and `page.wait` to the `myarc` CLI and MCP server. Screenshots come straight from the compositor as MCP image blocks, and `page.wait` listens for the real load event with a timeout so the agent stops guessing with sleep timers. Everything stays locked inside the Agent Space behind the Allow card and Stop button. Your own Spaces and Profiles stay untouched.

## Localhost without the purple bar

[![Side-by-side comparison of the same local test form in Arc on the left with its purple top localhost banner and myArc on the right with a thin blue viewport border and compact port tag in the URL pill](https://myarc.dev/blog/thousand-tabs/localhost-comparison.6025eeb90a9b.webp)](https://myarc.dev/blog/thousand-tabs/localhost-comparison.6025eeb90a9b.webp)

The same local test page in Arc (left) and myArc (right).

Arc puts a thick purple bar across the top of local pages. myArc draws a thin blue border around the viewport and shows a `</> :port` tag in the URL pill instead, so you keep your vertical space.

## A new icon for the Agent Space

Looking through screenshots all afternoon, the Agent Space icon started to bother me. It was a thin outline next to filled ones, and once I noticed it I couldn't unsee it. I swapped it for the robot head I made for [agentbar.fyi](https://agentbar.fyi).

[![Side-by-side close-up of the myArc sidebar header and bottom footer showing the new filled robot-head icon for the Agent Space](https://myarc.dev/blog/thousand-tabs/agent-space-icon.418b87b88871.webp)](https://myarc.dev/blog/thousand-tabs/agent-space-icon.418b87b88871.webp)

The new icon in the Space header and sidebar footer.

## A thousand tabs and no calls home

Arc people tend to hoard tabs, so I wanted to see what happens when myArc has to carry that weight. I loaded a fresh profile with ten Spaces and a hundred dormant items each, a thousand in all, on an Apple M5 Pro.

It held up better than I expected. The first window opened in `797.5ms`, and memory settled at `953.5MB` across 11 processes. Switching Spaces took `75.5ms` at the median (`83.1ms` at p95), with workspace saves at `2.08ms`. The command bar stayed snappy the whole time.

[![Full myArc window with a thousand synthetic items across ten Spaces and the frosted command bar open in the center](https://myarc.dev/blog/thousand-tabs/thousand-tabs.0b0fae3e3284.webp)](https://myarc.dev/blog/thousand-tabs/thousand-tabs.0b0fae3e3284.webp)

A thousand dormant items across ten Spaces, with the command bar open over the window.

After that I ran automated checks on the everyday stuff you only think about when it breaks: native notification prompts, frosted find-in-page, docked DevTools and SQLite history. Then I blocked external DNS and watched every network request from a clean profile. It opened straight into a workspace with no account prompt and pinned a local page to disk. Not a single request went to a myArc server.

[![myArc showing frosted find-in-page highlighting matches on a local test page](https://myarc.dev/blog/thousand-tabs/core-smoke-find.5cbc653dfcd8.webp)](https://myarc.dev/blog/thousand-tabs/core-smoke-find.5cbc653dfcd8.webp)

Find-in-page highlighting matches inside the glass bar.

[![myArc showing docked Chromium DevTools inspecting the local test page](https://myarc.dev/blog/thousand-tabs/core-smoke-devtools.8491e57988cc.webp)](https://myarc.dev/blog/thousand-tabs/core-smoke-devtools.8491e57988cc.webp)

Docked DevTools inspecting the same page.

We also tested macOS's Reduce Motion setting. I forgot I'd left it on, and when every animation in myArc stopped I got annoyed thinking we'd broken something in the code. Neon had to remind me I was the one who flipped the switch.

## Moving in

I started this round wanting to put myArc through a real test and see what broke. Nothing major did, and that was enough. **I'm making myArc my default browser**. Current Chromium with ungoogled-chromium and extra privacy patches, plus Arc's way of organizing tabs, is what I've been trying to get back to from the start.

I also spun up [myarc.slirc.app](https://myarc.slirc.app) so there's a place to share feedback and talk through what comes next. If you're trying the alpha, come say hi and tell me what works or what still feels rough.

Already using it? Open myArc → Check for Updates… to get `2026.11.23.1`. Fresh installs are on the [download page](https://myarc.dev/download/).
