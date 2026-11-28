---
title: "Nested folders, built-in RSS, and a 1-second agent CLI"
summary: "Three days after Boosts, myArc adds nested folders, a native RSS reader, frosted-glass dialogs everywhere, Homebrew installs, and a much faster myarc CLI."
date: 2026-11-28
draft: true
---

This morning I asked an agent to use the `myarc` CLI to open my blog, click into the latest post, and save a screenshot to Downloads. It worked, and it felt slow.

So I timed it. Turns out Chromium wasn't the slow part at all. `myarc tabs` took 50 ms. Taking a 1250x929 PNG screenshot of a background Agent Space tab took 60 ms. What made it feel sluggish was everything around those 60 milliseconds: `myarc open` returned while the page was still loading, `click` dumped 12.7 KB of full page text instead of just the interactive refs, and there was no `myarc screenshot <path>` command, so the agent had to pipe base64 JSON through Python. Every extra workaround burned another full LLM turn.

That sent me down a quick CLI cleanup, right on top of a pretty wild three-day stretch since the [Boosts post](https://myarc.dev/blog/boosts/). Build `2026.11.28` bundles all of it.

## Making `myarc` CLI feel like `agent-browser`

I use `agent-browser` a lot, and I wanted people who already live in myArc to have the option to replace it without giving up the Agent Space boundary or the Stop button.

In `2026.11.28`, `myarc` gets the short commands you actually reach for:

- `myarc screenshot [<path>]` writes the PNG or JPEG straight to disk (or a temp file if you skip the path) with zero extra plumbing.
- `myarc scroll <up|down|left|right|top|bottom> [<ref>]` and `myarc wait [<ms>]` work directly from the current tab and revision.
- `myarc open`, `back`, `forward`, and `reload` now wait for the page to finish loading by default (`--no-wait` skips it) before printing the outline.
- `-i` (interactive-only outline) now works on every outline-returning command, not just `snapshot`.

Re-running that exact same 5-step journey (`open -i` -> `click -i` -> `click -i` -> `screenshot` -> `close`) took **1.12 seconds** total. The `/blog` outline shrank from 12.7 KB to 5.9 KB (54% smaller), and the post outline dropped from ~4.5 KB to 927 bytes (79% smaller).

If you want your coding agents to use myArc, you don't even need to install a separate skill file. Running `myarc skill` prints the full agent usage guide (workflow, outline format, error handling, and safety rules) straight from the binary itself, and `myarc --help` points agents right to it. If you still want a standalone skill file in `~/.agents/skills/`, `myarc skill > SKILL.md` stays matched to the exact build you have installed.

## Built-in RSS, right in the sidebar

I still read a lot of personal blogs, and I wanted RSS to feel native instead of bolted on.

When you land on a site that advertises an RSS, Atom, or JSON feed, myArc surfaces it in the address bar. Subscribing drops the feed into your Space's RSS folder, opens articles in a built-in reader view (with a split-view toggle when you want the original page side by side), and lets you unsubscribe straight from the sidebar row or context menu with a quick confirmation.

## Nested folders, finally

Since the first Arc import shipped, nested folders were the biggest structural compromise: any folder inside a folder got flattened into its parent.

Now folders nest up to 8 levels deep across the Rust workspace core, the sidebar UI, and Arc import/export. You can create subfolders, drag folders to reorder or nest them, collapse a parent to hide its whole subtree, and export a Space to HTML/JSON or into a folder in another Space without losing the hierarchy.

## Frosted glass everywhere, smoother splits, and Homebrew

A bunch of daily-use rough edges got smoothed out over the last couple of days too:

- **Unified dark glass**: modal dialogs, permission prompts, bubbles, the new Site Control Center popover in the URL pill, and **About myArc** (with a copyable version pill) now share the same frosted-glass material, rim border, and top-left red close dot.
- **Animated Split View**: opening and closing a split now animates smoothly instead of snapping, and sidebar resizing covers Arc's full 160 to 500 DIP range (double-click the edge to reset).
- **1Password and Homebrew**: macOS native messaging extensions now fall back to Chrome, Arc, and Chromium `NativeMessagingHosts` manifests, so 1Password Touch ID works without manual JSON copying. And if you prefer Homebrew over dragging a DMG, you can now run `brew install --cask bitbonsai/tap/myarc`.

## Try it

Build `2026.11.28` is live on the [download page](https://myarc.dev/download/), on Homebrew, and via **myArc → Check for Updates…**.

<!-- TODO before setting draft: false:
     Add 2-3 synthetic screenshots (each with alt text + italic caption):
     1. Nested folders + RSS folder in the sidebar
     2. Built-in RSS reader / Site Control Center glass popover
     3. Agent Space running the 1.12s myarc CLI flow
-->
