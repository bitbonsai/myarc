---
title: "Agents in the browser, and RSS"
summary: "Why driving your real browser beats headless automation, putting the agent skill inside the CLI, and built-in RSS with nothing else to install."
date: 2026-11-28
draft: true
---

This morning I asked an agent to use the `myarc` CLI to open my blog, click into the latest post, and save a screenshot to Downloads. It worked, just slowly enough that I got annoyed watching it.

So I timed it. Turns out Chromium wasn't the slow part at all. `myarc tabs` took 50 ms, and taking a 1250x929 PNG screenshot of a background Agent Space tab took 60 ms. The delay was everything the agent had to do around the CLI: `open` returned while the page was still loading, `click` dumped 12.7 KB of full page prose instead of just the interactive refs, and there was no `myarc screenshot <path>` command, so the agent had to pipe base64 JSON through Python. Every missing shortcut cost another full LLM round-trip.

Fixing those small things got me thinking about why I care so much about browser control living inside myArc in the first place, and about the other thing that landed this week: built-in RSS.

## Agents in the browser you already use

Most browser automation tools spin up a separate, blank headless Chromium. That's great for CI, and annoying for real work.

The moment you want an agent to check something behind a login, test a local app with your extensions running, or inspect a page you actually care about, headless turns into friction. You're either pasting cookies around, fighting auth walls, or staring at a terminal wondering what the browser is actually doing because you can't see it.

With `myarc`, the agent works inside the browser you're already using, in its own isolated **Agent Space**. You can log in to a site normally and let the agent take over from there. If you want to watch what it's doing, click the bot icon in the sidebar footer and see the page live. If you don't want to watch, leave it running in the background while you stay in your own Space, completely out of its reach. And if it starts doing something you didn't ask for, hit **Stop** and revoke access on the spot.

In build `2026.11.28`, the CLI catches up to `agent-browser` on the day-to-day commands:

```sh
myarc open -i https://mauriciowolff.com   # waits for load, prints compact refs
myarc click -i @e1                        # -i works on actions, not just snapshot
myarc scroll down [@e4]                   # up, down, left, right, top, bottom
myarc wait [10000]                        # defaults to 10s
myarc screenshot ~/Downloads/post.png     # writes PNG/JPEG straight to disk
```

Re-running that same 5-step journey (`open -i`, `click -i`, `click -i`, `screenshot`, `close`) took **1.12 seconds** total. The `/blog` outline dropped from 12.7 KB to 5.9 KB, and the post outline went from roughly 4.5 KB down to 927 bytes.

## No separate skill to maintain

After that, shipping a separate skill file for `myarc` felt backwards. External skill files go stale the moment you change a flag, and coding agents often run in sandboxes that can't fetch docs from GitHub anyway.

So the agent guide now lives inside the `myarc` binary itself (on `main`, landing in the next build):

```sh
myarc skill
```

`myarc --help` points agents straight to it. It works offline, runs in a couple of milliseconds, and always matches the build on disk. If you want your agents to reach for `myarc` automatically instead of `agent-browser`, one line in your global `AGENTS.md` or `CLAUDE.md` is enough:

```markdown
Use `myarc` for browser tasks; run `myarc skill` first.
```

## RSS without another app, extension, or account

I still read a lot of personal blogs, and every few months I try to get back into RSS. Every time, the same thing stops me: I have to pick an RSS app, or install an extension, or sign up for a sync service, and then remember to actually open that separate app. Within two weeks I stop opening it.

RSS works much better when it's just part of the browser. There's nothing extra to download, no account to create, and no third-party service to depend on.

When you land on a site that has an RSS, Atom, or JSON feed, myArc makes it easy to spot right in the address bar. One click subscribes and drops it into your Space's **RSS** folder. Clicking a feed opens its articles in a clean built-in reader, with a toggle to put the original page beside it in Split View when you want the real layout. When you're done with a feed, unsubscribing sits right on the sidebar row and context menu.

## Also in this week's builds

A few other things landed alongside CLI and RSS since the [Boosts post](https://myarc.dev/blog/boosts/): folders now nest up to 8 levels deep (including Arc import and export), modal dialogs and popovers share the same dark frosted glass and red close dot, Split View animates open and closed, 1Password Touch ID works out of the box via macOS native messaging fallback, and you can now install or update via Homebrew (`brew install --cask bitbonsai/tap/myarc`).

Build `2026.11.28` is live on the [download page](https://myarc.dev/download/), on Homebrew, and via **myArc → Check for Updates…**.

<!-- TODO before setting draft: false:
     Add 2-3 synthetic screenshots (each with alt text + italic caption):
     1. Agent Space running the 1.12s myarc CLI flow (watching it live vs background)
     2. Address bar RSS feed discovery + built-in reader with Split View toggle
     3. Space RSS folder in the sidebar
-->
