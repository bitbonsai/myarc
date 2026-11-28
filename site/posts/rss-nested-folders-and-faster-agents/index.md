---
title: "Nested folders, built-in RSS, and a 1-second agent CLI"
summary: "Why the myarc CLI felt slow even though screenshots took 60 ms, plus built-in RSS, nested folders, frosted glass dialogs, and Homebrew."
date: 2026-11-28
draft: true
---

This morning I asked an agent to use the `myarc` CLI to open my blog, click into the latest post, and save a screenshot to Downloads. It worked, just slowly enough that I got annoyed watching it.

So I timed it. Turns out Chromium wasn't the slow part at all. `myarc tabs` took 50 ms, and taking a 1250x929 PNG screenshot of a background Agent Space tab took 60 ms.

The real bottleneck was everything the agent had to do around the CLI. `myarc open` returned while the page was still loading, forcing a second turn to wait and snapshot. `myarc click` dumped 12.7 KB of full page prose instead of just the interactive refs. And because there was no `myarc screenshot <path>` command, the agent had to pipe base64 JSON through Python and decode it by hand. Every missing convenience meant another full LLM round-trip.

## Making `myarc` feel like `agent-browser`

I use `agent-browser` a lot, and I wanted people who already live in myArc to be able to swap it out if they want, while keeping the Agent Space boundary and the Stop button.

In build `2026.11.28`, `myarc` gets the short commands you actually reach for:

```sh
myarc open -i https://mauriciowolff.com   # waits for load, prints compact refs
myarc click -i @e1                        # -i now works on actions, not just snapshot
myarc scroll down [@e4]                   # up, down, left, right, top, bottom
myarc wait [10000]                        # defaults to 10s
myarc screenshot ~/Downloads/post.png     # decodes PNG/JPEG straight to disk
```

`open`, `back`, `forward`, and `reload` now wait for the page to finish loading before printing the outline (`--no-wait` skips the wait).

I ran the exact same 5-step journey again (`open -i`, `click -i`, `click -i`, `screenshot`, `close`). All five commands finished in **1.12 seconds** total. The `/blog` outline dropped from 12.7 KB to 5.9 KB, and the post outline went from roughly 4.5 KB down to 927 bytes.

## Putting the skill inside the CLI

After that, I realized shipping a separate skill file for `myarc` felt backwards. External skill files go stale the moment you change a flag, and half the time coding agents run in sandboxes that can't fetch docs from GitHub anyway.

So I embedded the agent guide straight into the `myarc` binary (on `main`, landing in the next build):

```sh
myarc skill
```

`myarc --help` points agents right to it. It works offline, runs in a couple of milliseconds, and always matches the build on disk. If you want your agents to reach for `myarc` automatically without naming it in every prompt, one line in your global `AGENTS.md` or `CLAUDE.md` does the job:

```markdown
Use `myarc` for browser tasks; run `myarc skill` first.
```

## Built-in RSS, right in the sidebar

I still read a lot of personal blogs, and I wanted RSS inside the browser instead of another web app tab I forget to check.

When a site advertises an RSS, Atom, or JSON feed, myArc shows a feed icon in the address bar. Subscribing puts it in your Space's **RSS** folder and opens articles in a built-in reader view, with a toggle to put the original page beside it in Split View. Unsubscribing sits right on the sidebar row and context menu with a confirmation dialog.

## Nested folders, finally

When I first wired up Arc import, any folder inside another folder got flattened into its parent. That bugged me every time I looked at my sidebar.

Folders now nest up to 8 levels deep across the Rust workspace, the sidebar UI, and Arc import and export. You can create subfolders, drag folders to nest or reorder them, collapse a parent to hide its subtree, and export a Space to HTML, JSON, or a folder in another Space without flattening it again.

## Glass dialogs, smoother splits, and Homebrew

A few other things got cleaned up since the [Boosts post](https://myarc.dev/blog/boosts/). Modal dialogs, permission prompts, the new Site Control Center popover in the URL pill, and **About myArc** (now with a copyable version pill) all share the same dark frosted glass, rim border, and top-left red close dot. Opening and closing Split View animates instead of snapping, and dragging the sidebar edge now covers Arc's full 160 to 500 DIP range, with a double-click to reset.

On macOS, native messaging extensions also fall back to Chrome, Arc, and Chromium `NativeMessagingHosts` manifests, so 1Password Touch ID works without copying JSON files by hand. And if you prefer Homebrew over dragging a DMG:

```sh
brew install --cask bitbonsai/tap/myarc
```

Build `2026.11.28` is on the [download page](https://myarc.dev/download/), on Homebrew, and via **myArc → Check for Updates…**.

<!-- TODO before setting draft: false:
     Add 2-3 synthetic screenshots (each with alt text + italic caption):
     1. Nested folders + RSS folder in the sidebar
     2. Built-in RSS reader / Site Control Center glass popover
     3. Agent Space running the 1.12s myarc CLI flow
-->
