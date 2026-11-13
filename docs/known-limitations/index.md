HTML: https://myarc.dev/docs/known-limitations/

[All docs](https://myarc.dev/docs/)

# Known limitations

myArc is a pre-alpha. Use it to try the Arc-style sidebar, and keep daily and sensitive browsing in another browser for now.

## Safety and updates

- Each nightly is built from a Chromium 158 development revision, before Chromium ships it as a stable release.
- There are no automatic updates and no security updates. Install each new nightly by hand, as [Install myArc](https://myarc.dev/docs/install/) describes.
- The app is signed with an Apple Developer ID but not yet notarized by Apple, so macOS blocks the first launch until you allow it.
- The protections around private windows, Profile separation, cookies and saved passwords aren't verified yet.
- Safe Browsing doesn't scan RAR downloads, because the UnRAR license conflicts with GPL-3.0, the license myArc uses.
- myArc is an independent project with no ties to Arc or The Browser Company.

## Video and media

- There is no Widevine, the copy protection that Netflix, Spotify and Disney\+ need, so those services won't play.
- H.264 and AAC are included, so most other web video plays.

## What doesn't work yet

- Arc Favorites arrive as pinned tabs in their Space, because the export doesn't mark them.
- Folders inside folders, and dragging a tab up to pin it or into a folder.
- Peek, Split View and Little Arc.
- Boosts and the Mini Player.
- The animated onboarding.
- The Links and Shortcuts tabs in Settings.
- Only the dark appearance has been compared with Arc. Light mode, Reduce Transparency and full-screen layouts haven't been checked.

## After a crash

Recent tabs and navigation can be lost after a crash or a force quit. Pinned tabs, Favorites and Spaces are saved in your workspace file, which [Your data](https://myarc.dev/docs/your-data/) describes, but whether every change survives a force quit isn't fully tested yet.

If a single page crashes, reload it with Cmd\+R. The rest of the browser keeps running.

Crash report uploads are off. Chromium keeps crash reports on your Mac, in `~/Library/Application Support/myArc Nightly/Crashpad/`. They can hold pieces of the pages you had open, so don't attach them to a public issue.

## Troubleshooting

### macOS won't open myArc

Each new download is blocked on its first launch. Follow the first-launch steps in [Install myArc](https://myarc.dev/docs/install/).

### macOS keeps asking for your password

myArc needs it to read the key that protects its saved data. Choose Always Allow. Nightlies before 2026.11.03 used a different signature, so moving from one of those asks once more.

### An agent says myArc can't be reached

The `myarc` command exits with code 3 when myArc isn't running or browser control is off. Start myArc and turn on Allow agents to control myArc in Settings \> Advanced. [CLI and MCP](https://myarc.dev/docs/cli-and-mcp/) has the details.

## Report a problem

Open an issue on [GitHub](https://github.com/bitbonsai/myarc/issues). Include the myArc version, shown at the top of the app menu next to the Apple menu, your macOS version and the steps that cause the problem. Leave out private pages.
