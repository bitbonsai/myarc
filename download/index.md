HTML: https://myarc.dev/download/

# myArc pre-alpha nightly

Pre-alpha nightly, for trying the Arc-like shell. Not for daily or sensitive browsing.

[Download for macOS (Apple silicon)](https://github.com/bitbonsai/myarc/releases/download/nightly-2026.11.13/myArc-Nightly-2026.11.13-arm64.dmg)

- **Version:** 2026.11.13
- **Date:** November 13, 2026
- **Size:** 234.8 MB
- **SHA-256:** `40f51323b612b312e6a69a07dfd36c2dd3909b77abb0c9ef99018d9eb066603a`

## System requirements

- An Apple silicon Mac. Intel Macs are not supported.
- macOS 26 or later. The glass frame requires macOS 26\. It has been tested only on macOS 26.7.

## First launch

1. Check the download against the SHA-256 listed with it. The two values must match.

   ```
   shasum -a 256 ~/Downloads/myArc-Nightly-2026.11.13-arm64.dmg
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
- Cmd\+, opens a native Settings window. General shows the default-browser banner and Advanced links to the full Chromium settings. Profiles, Links and Shortcuts are not available yet.
- Quitting with Cmd\+Q needs no confirmation by default, with a setting to turn the warning on.
- Agents can drive tabs in the Agent Space through the bundled myarc CLI and MCP server. It is off by default. Turn it on in Settings \> Advanced, and myArc asks before each new agent connects.

## What does not work yet

- Importing from Arc. The importer is not implemented.
- Cmd\+D to pin a tab (Cmd\+D still bookmarks), folders, Auto Archive, the Archive and drag-to-pin.
- Peek, Split View and Little Arc. The Peek routing rule exists, but there is no overlay.
- Boosts, the Library, download progress in the sidebar and the Mini Player.
- The animated onboarding.
- Light mode, Reduce Transparency and fullscreen layouts are not verified.

## Known limitations and safety

- Built from a Chromium 158 development revision, not a stable release.
- No automatic updates and no security updates. Install each new nightly by hand.
- From 2026.11.03 on, nightlies are signed with an Apple Developer ID that later nightlies keep. macOS asks once more for your login password so it can read the saved-data key; choose Always Allow, and newer nightlies stop asking.
- The app is signed with a Developer ID but not yet notarized by Apple.
- Recent tabs and navigation can be lost after a crash or force quit.
- Private window, Profile, cookie and credential guarantees are not verified.
- RAR downloads are not scanned by Safe Browsing, because the UnRAR license conflicts with GPL-3.0.
- No Widevine, so Netflix, Spotify and Disney\+ will not play.
- H.264 and AAC are included, so most other web video plays.
- myArc is an independent project, not affiliated with Arc or The Browser Company.

## Other ways to get it

- [GitHub releases](https://github.com/bitbonsai/myarc/releases/tag/nightly-2026.11.13) lists every nightly.
- [Source on GitHub](https://github.com/bitbonsai/myarc), with instructions to [build it from source](https://github.com/bitbonsai/myarc/blob/main/browser/README.md).
- [GitHub issues](https://github.com/bitbonsai/myarc/issues) take bug reports. Include the version, your macOS version and the steps that trigger the problem, and leave out private pages.

## License and notices

myArc is licensed under GPL-3.0, see [LICENSE](https://github.com/bitbonsai/myarc/blob/main/LICENSE). Chromium and other bundled components keep their own licenses. [ATTRIBUTION.md](https://github.com/bitbonsai/myarc/blob/main/ATTRIBUTION.md) records what is known, and the [dependency license inventory](https://github.com/bitbonsai/myarc/blob/main/browser/evidence/dependency-licenses.json) is not yet complete.
