HTML: https://myarc.dev/download/

# myArc pre-alpha nightly

Pre-alpha nightly, for trying the Arc-like shell. Not for daily or sensitive browsing.

[Download for macOS (Apple silicon)](https://github.com/bitbonsai/myarc/releases/download/nightly-2026.11.19/myArc-Nightly-2026.11.19-arm64.dmg)

- **Version:** 2026.11.19
- **Date:** November 19, 2026
- **Size:** 243.4 MB
- **SHA-256:** `1a0c07efcee77d03535f06dc5e589ee555e1e8c97ff5e8c414a8b67757317b43`

## System requirements

- An Apple silicon Mac. Intel Macs are not supported.
- macOS 26 or later. The glass frame requires macOS 26\. It has been tested only on macOS 26.7.

## First launch

1. Check the download against the SHA-256 listed with it. The two values must match.

   ```
   shasum -a 256 ~/Downloads/myArc-Nightly-2026.11.19-arm64.dmg
   ```
2. Open the DMG and drag myArc Nightly to Applications.
3. Open myArc Nightly. From 2026.11.18 on, Apple has notarized each nightly, so macOS only asks once whether to open an app downloaded from the internet. Choose Open.

## What works today

Each item was seen working in a native run or experiment note. None of it has had release testing.

- Chromium browsing, with Chromium extensions. A Manifest V3 smoke test passed; wider extension coverage is untested.
- A glass sidebar with Favorite tiles, a Space heading, pinned tabs, a New Tab row and temporary tabs below the divider, next to a rounded page card. Only the dark appearance has been compared with Arc.
- Creating, switching and renaming Spaces from the sidebar footer. Favorites are shared across the Spaces of one Profile.
- Closing a pinned tab keeps the pin.
- A command bar: Cmd\+L opens it on the current address, arrows, Enter and Escape work, and new windows open it centered.
- Shortcuts: Cmd\+S hides the sidebar, Cmd\+Shift\+T reopens a closed tab, Cmd\+Shift\+C copies the address and shows a Copied Current URL toast, Cmd\+Shift\+N opens a private window and Ctrl\+Cmd\+N a blank one.
- Cmd\+, opens a native Settings window. General shows the default-browser banner, Profiles sets when tabs are archived, and Advanced holds Import from Arc and a link to the full Chromium settings. Links and Shortcuts are not available yet.
- Quitting with Cmd\+Q needs no confirmation by default, with a setting to turn the warning on.
- Importing an Arc export from Settings \> Advanced \> Import from Arc. A preview shows what it will add, then each Space arrives with its folders and pinned tabs, in Arc's order. Importing the same file again adds nothing.
- Folders for a Space's pinned tabs. Create, rename and delete them from the right-click menu, and open or close them. Drag tabs in and out, and rename a pinned tab from its menu.
- Split View for two tabs side by side. Use Open in Split View in a tab's menu or Ctrl\+Shift\+\=, and Ctrl\+Shift\+- to separate them. The split shows as one sidebar row and comes back after a restart.
- Peek: a link from a pinned tab or Favorite to another site opens in a card over the page. Cmd\+W or Esc closes it, Cmd\+O turns it into a tab, a button splits it beside the pinned page, and Cmd\+Shift\+T reopens the last one closed. Settings \> Links can turn it off.
- Ad and tracker blocking, on by default, with uBlock Origin Lite built in. Turn it off for one site with the Block ads on this site switch in the site info popover, which opens from the "i" in the address field. Private windows block only after you allow the extension there.
- Ctrl\+Tab switches to your last tab, and holding Ctrl goes further back through the five most recent.
- Little Arc: links from other apps open in a small window, and Option-Cmd-N opens one. Open in \<Space\> or Cmd\+O moves the page into your current Space. Settings \> Links turns it off.
- The command bar runs actions: type pin, archive, split or a Space's name, or / to list them. View has Clear Cookies and Refresh and Clear Cache and Refresh for the current site.
- Pins, folders and Spaces survive a force quit. Each change is saved before the sidebar shows it, and a damaged workspace file falls back to the backup.
- Agents can drive tabs in the Agent Space through the bundled myarc CLI and MCP server. It is off by default. Turn it on in Settings \> Advanced, and myArc asks before each new agent connects.

## Known limitations

- No automatic updates and no security updates. Install each new nightly by hand.
- Recent tabs and navigation can be lost after a crash or force quit.
- H.264 and AAC are included, so most other web video plays.

[Known limitations](https://myarc.dev/docs/known-limitations/) lists the rest, with what does not work yet and what to do after a crash.

## Other ways to get it

- [GitHub releases](https://github.com/bitbonsai/myarc/releases/tag/nightly-2026.11.19) lists every nightly.
- [Source on GitHub](https://github.com/bitbonsai/myarc), with instructions to [build it from source](https://github.com/bitbonsai/myarc/blob/main/browser/README.md).
- [GitHub issues](https://github.com/bitbonsai/myarc/issues) take bug reports. Include the version, your macOS version and the steps that trigger the problem, and leave out private pages.

## License and notices

myArc is licensed under GPL-3.0, see [LICENSE](https://github.com/bitbonsai/myarc/blob/main/LICENSE). Chromium and other bundled components keep their own licenses. [ATTRIBUTION.md](https://github.com/bitbonsai/myarc/blob/main/ATTRIBUTION.md) records what is known, and the [dependency license inventory](https://github.com/bitbonsai/myarc/blob/main/browser/evidence/dependency-licenses.json) is not yet complete.
