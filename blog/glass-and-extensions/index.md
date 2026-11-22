HTML: https://myarc.dev/blog/glass-and-extensions/

[All posts](https://myarc.dev/blog/)

# Frosted glass, cleaner settings, and extensions in the sidebar

November 22, 2026

[Mauricio Wolff](https://github.com/bitbonsai)4 min read

The last couple of days were about smoothing out the rough edges you only feel when you actually live in a browser. A few big features landed, sure, but most of the work was removing friction: fewer buttons, less clutter, frosted glass that belongs on macOS, and extension icons right where Arc users expect them.

[![myArc Settings Advanced window on macOS frosted glass, showing three grouped cards for Data (Import from Arc and Export Pins and Favorites), Browser control, and More (Chromium Settings and Welcome tour)](https://myarc.dev/blog/glass-and-extensions/settings-advanced.05fb59e7e46f.webp)](https://myarc.dev/blog/glass-and-extensions/settings-advanced.05fb59e7e46f.webp)

Settings \> Advanced, rebuilt as three calm grouped cards on frosted glass.

## Frosted glass everywhere it belongs

The sidebar already blurred the desktop behind it, which made opaque dark panels look stamped on top of the browser. Every myArc-owned surface now shares one macOS glass helper: Settings, the command bar, the Create a Space panel, the theme and icon pickers, coach marks, import and export sheets, and the copy-link toast.

Each surface uses behind-window vibrancy with a subtle specular hairline and calibrated contrast so text stays readable over both white and dark web pages. Turn on Reduce Transparency in macOS System Settings and every one of them falls back cleanly to an opaque card.

[![The frosted glass myArc command bar floating over a split light and dark test page, showing Search Settings highlighted in blue above search suggestions](https://myarc.dev/blog/glass-and-extensions/palette.d18a6a5f2d10.webp)](https://myarc.dev/blog/glass-and-extensions/palette.d18a6a5f2d10.webp)

The command bar over a half-white, half-blue page, staying legible across both.

Sparkle's default update window was the last opaque holdout, so I replaced its standard UI with a custom frosted-glass updater driven by Sparkle's own `SPUUserDriver` protocol. Signature verification, automatic background checks and EdDSA validation stay untouched — it just looks like myArc now.

[![The frosted glass myArc updater window showing the 3D ribbon logo, Ready to Install, Nightly 2026.11.22.1 is verified and ready, and Later and Install and Relaunch buttons](https://myarc.dev/blog/glass-and-extensions/updater-ready.bcb4c87ebf31.webp)](https://myarc.dev/blog/glass-and-extensions/updater-ready.bcb4c87ebf31.webp)

In-app updates now match the rest of the browser.

## Fewer buttons on day one

Both Settings and the welcome screen used to show a separate "Choose a file…" button next to "Import", left over from when myArc only read Arc's Markdown export. That was noise.

Clicking **Import from Arc** now checks your Mac's Arc sidebar file automatically and opens the preview straight away, with your Arc profile names, Favorites, pinned tab names and saved page titles intact. Only if Arc isn't installed on your Mac does Settings offer a fallback dialog to pick an exported file from another machine.

On the welcome screen, **Import from Arc** only shows up if Arc is actually on your Mac — otherwise you just get one clean **Let's go** button. No extra "Found your Arc data" eyebrow, no "Choose a file…" link, and no visible "Skip tour" cluttering the hero (Esc still skips, and every tour card has its own Skip button one click away).

[![The myArc welcome screen with the 3D ribbon A logo above myArc, forever yours., and two centered pill buttons: Import from Arc in blue and Let's go in translucent ivory](https://myarc.dev/blog/glass-and-extensions/welcome.a7913705aeca.webp)](https://myarc.dev/blog/glass-and-extensions/welcome.a7913705aeca.webp)

The decluttered welcome screen when Arc is detected on the Mac.

## Extensions in the URL pill

Installing Bitwarden from the Chrome Web Store this week turned up an obvious gap: the extension installed and stayed on across restarts, yet its icon had nowhere to show up because myArc's sidebar header hid Chromium's extensions container.

Pinned extension icons now sit at the right end of the sidebar's URL pill — invisible at rest so the sidebar stays quiet, revealed on hover next to Copy URL, and kept visible while an extension's popup is open. Pressing ⌘L to type a URL hides them again so you get the full address width.

At the same time, our bundled uBlock Origin Lite stays unpinned by default (including a one-time unpin for existing profiles) and is no longer flagged by Chromium's Safety Check as an unverified off-store extension.

## Quiet speedups and polish

A few smaller changes shipped alongside the visual pass:

- **Lighter startup:** hidden Profile windows now restore with only their active tab scheduled to load, cutting background renderer processes at launch from 30 to 17 on a multi-Profile test setup.
- **Cleaner command bar actions:** when what you type clearly names an action (like "Switch to"), web search suggestions step out of the way.
- **Export everything:** Export Pins and Favorites now covers every regular Profile in one standard bookmarks HTML file.

## Try it

Everything above is in nightly `2026.11.22.1` on the [download page](https://myarc.dev/download/), or via **myArc → Check for Updates…** if you're already on a recent nightly. We're down to the final live checks before calling it Alpha — follow along or file feedback [on GitHub](https://github.com/bitbonsai/myarc).
