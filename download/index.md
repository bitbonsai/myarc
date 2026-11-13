HTML: https://myarc.dev/download/

# myArc pre-alpha nightly

Pre-alpha nightly, for trying the Arc-like shell. Not for daily or sensitive browsing.

[Download for macOS (Apple silicon)](https://github.com/bitbonsai/myarc/releases/download/nightly-2026.11.13.3/myArc-Nightly-2026.11.13.3-arm64.dmg)

- **Version:** 2026.11.13.3
- **Date:** November 13, 2026
- **Size:** 234.1 MB
- **SHA-256:** `bf626f87635632d127d01fe31a3b726210b46033f88a4d752319c7b1ff3a53f1`

## System requirements

- An Apple silicon Mac. Intel Macs are not supported.
- macOS 26 or later. The glass frame requires macOS 26\. It has been tested only on macOS 26.7.

## First launch

1. Check the download against the SHA-256 listed with it. The two values must match.

   ```
   shasum -a 256 ~/Downloads/myArc-Nightly-2026.11.13.3-arm64.dmg
   ```
2. Open the DMG and drag myArc Nightly to Applications.
3. macOS blocks the first launch because the app is not notarized. Right-click myArc Nightly, choose Open, then Open again. If macOS only offers Done or Move to Trash, which macOS 15 and later usually does, open System Settings \> Privacy \& Security, scroll to the myArc Nightly message and choose Open Anyway.
4. Or remove the quarantine flag in Terminal and open the app normally.

   ```
   xattr -dr com.apple.quarantine "/Applications/myArc Nightly.app"
   ```

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
- Agents can drive tabs in the Agent Space through the bundled myarc CLI and MCP server. It is off by default. Turn it on in Settings \> Advanced, and myArc asks before each new agent connects.

## Known limitations

- No automatic updates and no security updates. Install each new nightly by hand.
- Not yet notarized by Apple, so macOS blocks the first launch until you allow it.
- Recent tabs and navigation can be lost after a crash or force quit.
- H.264 and AAC are included, so most other web video plays.

[Known limitations](https://myarc.dev/docs/known-limitations/) lists the rest, with what does not work yet and what to do after a crash.

## Other ways to get it

- [GitHub releases](https://github.com/bitbonsai/myarc/releases/tag/nightly-2026.11.13.3) lists every nightly.
- [Source on GitHub](https://github.com/bitbonsai/myarc), with instructions to [build it from source](https://github.com/bitbonsai/myarc/blob/main/browser/README.md).
- [GitHub issues](https://github.com/bitbonsai/myarc/issues) take bug reports. Include the version, your macOS version and the steps that trigger the problem, and leave out private pages.

## License and notices

myArc is licensed under GPL-3.0, see [LICENSE](https://github.com/bitbonsai/myarc/blob/main/LICENSE). Chromium and other bundled components keep their own licenses. [ATTRIBUTION.md](https://github.com/bitbonsai/myarc/blob/main/ATTRIBUTION.md) records what is known, and the [dependency license inventory](https://github.com/bitbonsai/myarc/blob/main/browser/evidence/dependency-licenses.json) is not yet complete.
