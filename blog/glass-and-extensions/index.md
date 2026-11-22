HTML: https://myarc.dev/blog/glass-and-extensions/

[All posts](https://myarc.dev/blog/)

# Frosted glass, cleaner settings, and extensions in the sidebar

November 22, 2026

[Mauricio Wolff](https://github.com/bitbonsai)3 min read

Most of this round went into fixing things that got in the way when I used myArc day to day: flat dark boxes floating over a blurred sidebar, too many buttons on Settings and the welcome screen, and no place to click a password manager's icon after installing it.

[![myArc Settings Advanced window on macOS frosted glass, showing three grouped cards for Data (Import from Arc and Export Pins and Favorites), Browser control, and More (Chromium Settings and Welcome tour)](https://myarc.dev/blog/glass-and-extensions/settings-advanced.05fb59e7e46f.webp)](https://myarc.dev/blog/glass-and-extensions/settings-advanced.05fb59e7e46f.webp)

Settings \> Advanced, grouped into three cards on frosted glass.

## Frosted glass on the rest of the UI

The sidebar already blurred the desktop behind it, so opaque panels sitting next to it looked out of place. Every myArc window and popup now uses the same macOS vibrancy helper: Settings, the command bar, the Create a Space panel, the theme and icon pickers, tour cards, import and export dialogs, and the copy-link toast.

Each one uses behind-window blur with a thin border and a tint that keeps text readable over white and dark pages alike. If Reduce Transparency is on in macOS System Settings, they switch to solid panels instead.

[![The frosted glass myArc command bar floating over a split light and dark test page, showing Search Settings highlighted in blue above search suggestions](https://myarc.dev/blog/glass-and-extensions/palette.d18a6a5f2d10.webp)](https://myarc.dev/blog/glass-and-extensions/palette.d18a6a5f2d10.webp)

The command bar over a half-white, half-blue page.

Sparkle's default update window was still an opaque gray box, so I replaced its standard UI with a custom `SPUUserDriver` window. Signature checks, automatic background checks and EdDSA validation work the same as before, just in a frosted window that matches myArc.

[![The frosted glass myArc updater window showing the 3D ribbon logo, Ready to Install, Nightly 2026.11.22.1 is verified and ready, and Later and Install and Relaunch buttons](https://myarc.dev/blog/glass-and-extensions/updater-ready.bcb4c87ebf31.webp)](https://myarc.dev/blog/glass-and-extensions/updater-ready.bcb4c87ebf31.webp)

The updater window once an update is verified and ready.

## One-click Arc import and a simpler welcome

Settings and the welcome screen used to show a "Choose a file…" button next to "Import". That was left over from when myArc only read Arc's Markdown export.

Clicking **Import from Arc** now checks `~/Library/Application Support/Arc/StorableSidebar.json` on its own and opens the preview right away, with your Arc profile names, Favorites, pinned tab names and page titles. If Arc isn't on that Mac, Settings shows a dialog that lets you pick an export or backup file from another machine.

On the welcome screen, **Import from Arc** only appears when Arc is installed on the Mac. Otherwise you get one button, **Let's go**. I also dropped the "Found your Arc data" label, the "Choose a file…" link and the "Skip tour" link from the welcome screen. Esc still skips the tour, and every tour card has its own Skip button.

[![The myArc welcome screen with the 3D ribbon A logo above myArc, forever yours., and two centered pill buttons: Import from Arc in blue and Let's go in translucent ivory](https://myarc.dev/blog/glass-and-extensions/welcome.a7913705aeca.webp)](https://myarc.dev/blog/glass-and-extensions/welcome.a7913705aeca.webp)

The welcome screen on a Mac with Arc installed.

## Extension icons in the URL pill

Installing Bitwarden from the Chrome Web Store turned up a real gap: the extension installed and survived restarts, but there was nowhere to click its icon because myArc's sidebar header hid Chromium's extensions container.

Pinned extension icons now sit at the right end of the URL pill. At rest the pill shows only the address. Hovering reveals your pinned icons next to Copy URL, and clicking one opens its popup and keeps the icon visible until the popup closes. Pressing ⌘L to type an address hides the icons again so you get the full width.

The bundled uBlock Origin Lite now stays unpinned by default (including a one-time unpin for existing profiles), since it already has its own switch in the site info popover. Chromium's Safety Check also no longer warns that it can't verify where uBlock Origin Lite came from.

## Smaller fixes

- Hidden Profile windows now restore with only their active tab scheduled to load, dropping background renderers at launch from 30 to 17 in a three-Profile test.
- When you type something in the command bar that names an action (like "Switch to"), web search suggestions no longer mix in below it.
- Export Pins and Favorites now exports every regular Profile into one bookmarks HTML file.

## Try it

Everything above is in nightly `2026.11.22.1` on the [download page](https://myarc.dev/download/), or through **myArc → Check for Updates…** if you're already on a recent nightly. The code is [on GitHub](https://github.com/bitbonsai/myarc).
