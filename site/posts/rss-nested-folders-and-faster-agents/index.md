---
title: "Nested folders, built-in RSS, and a 1-second agent CLI"
summary: "Three days after Boosts, myArc adds nested folders, a native RSS reader, frosted-glass dialogs everywhere, Homebrew installs, and a much faster myarc CLI."
date: 2026-11-28
draft: true
---

This morning I asked an agent to use the `myarc` CLI to open my blog, click into the latest post, and save a screenshot to Downloads.

It worked. And it felt slow.

So I timed it. Turns out Chromium wasn't the slow part at all. `myarc tabs` took **50 ms**. Capturing a 1250x929 PNG screenshot of a background Agent Space tab took **60 ms**.

> The browser was already taking screenshots in 60 milliseconds. The slowness was every workaround the agent had to do around the CLI.

`myarc open` returned while the page was still loading, so the agent burned a second turn waiting. `myarc click` dumped **12.7 KB** of full page prose instead of just the interactive refs. And there was no `myarc screenshot <path>` command, so the agent had to pipe base64 JSON through Python. Every extra workaround meant another full LLM round-trip.

That turned into a quick CLI cleanup, right on top of a busy three-day stretch since the [Boosts post](https://myarc.dev/blog/boosts/). Build `2026.11.28` brings all of it together.

## Making `myarc` CLI feel like `agent-browser`

I use `agent-browser` a lot. I wanted people who already live in myArc to be able to replace it if they want to, without giving up the Agent Space isolation or the visible Stop button.

In `2026.11.28`, `myarc` gets the short commands you actually reach for:

```sh
myarc open -i https://mauriciowolff.com   # waits for load, prints compact refs
myarc click -i @e1                        # interactive-only outline on actions too
myarc scroll down [@e4]                   # up, down, left, right, top, bottom
myarc wait [10000]                        # defaults to 10s
myarc screenshot ~/Downloads/post.png     # writes PNG/JPEG straight to disk
```

A few small changes made a big difference:

- **Auto-wait on navigation**: `open`, `back`, `forward`, and `reload` wait for the page to finish loading before printing the outline (`--no-wait` skips it).
- **`-i` everywhere**: compact interactive outlines now work on every outline-returning command, not just `snapshot`.
- **Direct screenshots**: `myarc screenshot [<path>]` decodes the image straight to disk (or a temp file if you omit the path), with zero external dependencies.

Re-running that exact same 5-step journey (`open -i` -> `click -i` -> `click -i` -> `screenshot` -> `close`) took **1.12 seconds** total. The `/blog` outline shrank from **12.7 KB to 5.9 KB** (54% smaller), and the post outline dropped from **~4.5 KB to 927 bytes** (79% smaller).

## Putting the skill inside the CLI

Instead of asking people to install and maintain a separate skill package, the agent guide now lives right inside the `myarc` binary:

```sh
myarc skill
```

`myarc --help` points agents straight to it. Because it's compiled into the binary, it works offline, works inside network-restricted sandboxes, and always matches the exact version of myArc on disk.

If you want your coding agents to pick `myarc` automatically (even when you don't name it in your prompt), one line in your global `AGENTS.md` or `CLAUDE.md` is enough:

```markdown
Use `myarc` for browser tasks; run `myarc skill` first.
```

Or keep using `agent-browser` if you prefer. Everything is opt-in.

## Built-in RSS, right in the sidebar

I still read a lot of personal blogs, and I wanted RSS to feel like part of the browser instead of another web app tab.

When a site advertises an RSS, Atom, or JSON feed, myArc surfaces it in the address bar. Subscribing adds the feed to your Space's **RSS** folder and opens articles in a built-in reader view, with a split-view toggle when you want the original page side by side. Unsubscribing sits right on the sidebar row and context menu with a quick confirmation.

## Nested folders, finally

Since the first Arc import shipped, nested folders were the biggest structural compromise: any folder inside a folder got flattened into its parent.

Folders now nest up to **8 levels deep** across the Rust workspace core, the sidebar UI, and Arc import/export. You can create subfolders, drag folders to reorder or nest them, collapse a parent to hide its whole subtree, and export a Space to HTML/JSON or into a folder in another Space without losing the tree.

## Glass dialogs, smoother splits, and Homebrew

A bunch of daily-use rough edges landed over the last few days too:

- **Unified dark frosted glass**: modal dialogs, permission prompts, bubbles, the new Site Control Center popover in the URL pill, and **About myArc** (with a copyable version pill) now share the same frosted material, rim border, and top-left red close dot.
- **Animated Split View & sidebar resize**: opening and closing splits now animates smoothly, and dragging the sidebar edge covers Arc's full **160 to 500 DIP** range (double-click the edge to reset).
- **1Password Touch ID & Homebrew**: macOS native messaging extensions now fall back to Chrome, Arc, and Chromium `NativeMessagingHosts` manifests, so 1Password works without copying JSON files by hand. And if you prefer Homebrew over dragging a DMG:

```sh
brew install --cask bitbonsai/tap/myarc
```

## Try it

Build `2026.11.28` is live on the [download page](https://myarc.dev/download/), on Homebrew, and via **myArc → Check for Updates…**.

<!-- TODO before setting draft: false:
     Add 2-3 synthetic screenshots (each with alt text + italic caption):
     1. Nested folders + RSS folder in the sidebar
     2. Built-in RSS reader / Site Control Center glass popover
     3. Agent Space running the 1.12s myarc CLI flow
-->
