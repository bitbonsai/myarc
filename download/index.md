HTML: https://myarc.dev/download/

# myArc Alpha

Arc's workflow on Chromium with ungoogled-chromium privacy patches and Chrome extensions intact.

- No account or sign-in
- Ad blocking on, AI off by default
- Imports your Arc Spaces
- GPL-3.0, data stays on your machine

v2026.11.24.2November 24, 2026

[Download for macOS](https://github.com/bitbonsai/myarc/releases/download/v2026.11.24.2/myArc-2026.11.24.2-arm64.dmg)

184.8 MB · Apple silicon · macOS 26\+ · Notarized

LinuxSoonWindowsSoon

Verify download (SHA-256)

SHA-256

`68f39ab66efc1bccf7fb4df34983a74f6a45a139e03155816da466d677bf867f`

```
shasum -a 256 ~/Downloads/myArc-2026.11.24.2-arm64.dmg
```

Copy

## First launch on macOS

[Install and update guide →](https://myarc.dev/docs/install/)

1. ### Download the DMG

   Apple silicon Mac on macOS 26 or later. Intel Macs will not run it.
2. ### Drag to Applications

   Open the disk image and drop myArc into Applications.
3. ### Open and allow once

   Apple has notarized each build, so macOS asks once. Click Open, then Always Allow if it asks for your login keychain.

## Ready to try

[Browse docs →](https://myarc.dev/docs/)

- **Spaces and Profiles.** Glass sidebar, shared Favorites, pinned tabs and folders.
- **Command bar.** Press `⌘L` or `⌘T` to open sites, run actions or switch Spaces.
- **Peek and Split View.** Open a link in a card over the page, or put two tabs side by side.
- **Little Arc.** Links from other apps open in a small window, or press `⌥⌘N` anywhere.
- **Import from Arc.** Bring over Spaces, folders and pinned tabs in Arc's order.
- **Ad blocking and agents.** uBlock Origin Lite built in, plus opt-in CLI and MCP control.

## Keep in mind

[All limitations →](https://myarc.dev/docs/known-limitations/)

- **Chromium 158 dev.** Checks for new alpha builds daily, with no separate security patches.
- **Media playback.** No Widevine, so Netflix, Spotify and Disney\+ will not play. H.264 and AAC are included, so most other web video plays.
- **Crashes.** Pins, folders and Spaces survive a force quit. Recent tabs can still be lost.
- **Early guarantees.** Profile separation, cookie isolation and saved-password checks are not verified yet.

- [Feedback and chat](https://myarc.slirc.app)

  Join myarc.slirc.app to share what works or feels rough.
- [Report a problem](https://github.com/bitbonsai/myarc/issues)

  Include your myArc and OS versions, and leave out private pages.
- [GitHub releases](https://github.com/bitbonsai/myarc/releases/tag/v2026.11.24.2)

  Every published build.
- [Build from source](https://github.com/bitbonsai/myarc/blob/main/browser/README.md)

  Clone [bitbonsai/myarc](https://github.com/bitbonsai/myarc) and compile it yourself.

GPL-3.0 ([LICENSE](https://github.com/bitbonsai/myarc/blob/main/LICENSE)). Bundled components keep their own licenses ([ATTRIBUTION.md](https://github.com/bitbonsai/myarc/blob/main/ATTRIBUTION.md), [dependency inventory](https://github.com/bitbonsai/myarc/blob/main/browser/evidence/dependency-licenses.json)). Independent project, separate from Arc and The Browser Company.
