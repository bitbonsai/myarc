HTML: https://myarc.dev/docs/known-limitations/

# Known limitations

What the pre-alpha nightly can't do yet, and what to do after a crash or when something breaks.

myArc is a pre-alpha. Use it to try the Arc-style sidebar, and keep daily and sensitive browsing in another browser for now.

## Safety and updates

- Each nightly is built from a Chromium 158 development revision, before Chromium ships it as a stable release.
- There are no automatic updates and no security updates. Install each new nightly by hand, as [Install myArc](https://myarc.dev/docs/install/) describes.
- The protections around private windows, Profile separation, cookies and saved passwords aren't verified yet.
- Safe Browsing doesn't scan RAR downloads, because the UnRAR license conflicts with GPL-3.0, the license myArc uses.
- myArc is an independent project with no ties to Arc or The Browser Company.

## Video and media

- There is no Widevine, the copy protection that Netflix, Spotify and Disney\+ need, so those services won't play.
- H.264 and AAC are included, so most other web video plays.

## What doesn't work yet

- Arc Favorites arrive as pinned tabs in their Space, because the export doesn't mark them.
- Folders inside folders, and dragging a tab up to pin it.
- Split View holds two tabs at most, and a split can't be pinned or put in a folder.
- Peek can't open into a split, and a closed Peek can't be reopened.
- Little Arc windows don't come back after a relaunch. Option-Cmd-N works from any app, unless another app such as Raycast already uses that key.
- Boosts and the Mini Player.
- The animated onboarding.
- The Links and Shortcuts tabs in Settings.
- Only the dark appearance has been compared with Arc. Light mode, Reduce Transparency and full-screen layouts haven't been checked.

## After a crash

Recent tabs and navigation can be lost after a crash or a force quit. Pinned tabs, Favorites, folders and Spaces are saved to your workspace file, which [Your data](https://myarc.dev/docs/your-data/) describes, before the sidebar shows the change, so a force quit right after keeps it. If that file is ever damaged, myArc opens the copy it kept before the last change and leaves the damaged one beside it as `.corrupt`. A power cut or a full disk hasn't been tested yet.

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
