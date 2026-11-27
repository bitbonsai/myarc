HTML: https://myarc.dev/docs/known-limitations/

# Known limitations

What myArc Alpha can't do yet, and what to do after a crash or when something breaks.

myArc is an alpha build on Chromium 158 dev. Expect rough edges, and Chromium security fixes arrive with the next release.

## Safety and updates

- Each build is based on a Chromium 158 development revision, before Chromium ships it as a stable release.
- There are no separate security updates between builds. myArc updates itself from myarc.dev; older `myArc Nightly` builds need `myArc.app` installed by hand once. [Install myArc](https://myarc.dev/docs/install/) covers both.
- Saved-password and credential guarantees aren't verified yet.
- Safe Browsing doesn't scan RAR downloads, because the UnRAR license conflicts with GPL-3.0, the license myArc uses.
- myArc is an independent project with no ties to Arc or The Browser Company.

## Video and media

- There is no Widevine, the copy protection that Netflix, Spotify and Disney\+ need, so those services won't play.
- H.264 and AAC are included, so most other web video plays.

## What doesn't work yet

- Arc Favorites imported from Arc's Markdown export arrive as pinned tabs in their Space, because the export doesn't mark them. Import Arc's sidebar file instead, as [Import from Arc](https://myarc.dev/docs/import-from-arc/) describes.
- Folders inside folders.
- Split View holds two tabs at most, and a split can't be pinned or put in a folder.
- Peek's split button splits a copy of the pinned page, because a split can't hold a pinned tab yet. Cmd\+Z doesn't bring back a closed Peek, so use Cmd\+Shift\+T.
- Little Arc windows don't come back after a relaunch. Option-Cmd-N works from any app while Links from other apps open in Little Arc is on in Settings \> Links, unless another app such as Raycast already uses that key.
- Boosts and the Mini Player.
- The Shortcuts tab in Settings.
- Only the dark appearance has been compared with Arc. Light mode, Reduce Transparency and full-screen layouts haven't been checked.

## After a crash

After a crash, myArc reopens your last session on its own. If the next launch crashes too, it asks before restoring, so a session that keeps crashing can't trap you. Recent tabs and navigation can still be lost after a crash or a force quit. Pinned tabs, Favorites, folders and Spaces are saved to your workspace file, which [Your data](https://myarc.dev/docs/your-data/) describes, before the sidebar shows the change, so a force quit right after keeps it. If that file is ever damaged, myArc opens the copy it kept before the last change and leaves the damaged one beside it as `.corrupt`. A power cut or a full disk hasn't been tested yet.

If a single page crashes, reload it with Cmd\+R. The rest of the browser keeps running.

A crash also brings up a card that asks "myArc quit unexpectedly. Report it?" Report… opens a new GitHub issue in a tab, filled in with the myArc and macOS versions and the top of the crash log macOS wrote, without the lines that identify your Mac. Nothing is sent until you read it and submit it, which needs a GitHub account. Not Now closes the card. It offers each crash log once.

Crash report uploads are off. Chromium keeps crash reports on your Mac, in `~/Library/Application Support/myArc/Crashpad/`. They can hold pieces of the pages you had open, so don't attach them to a public issue.

## Troubleshooting

### macOS won't open myArc

Each new download is notarized, so macOS only asks once whether to open an app downloaded from the internet. Follow the first-launch steps in [Install myArc](https://myarc.dev/docs/install/).

### macOS keeps asking for your password

myArc needs it to read the key that protects its saved data. Choose Always Allow. Nightlies before 2026.11.03 used a different signature, so moving from one of those asks once more.

### An agent says myArc can't be reached

The `myarc` command exits with code 3 when myArc isn't running or browser control is off. Start myArc and turn on Allow agents to control myArc in Settings \> Advanced. [CLI and MCP](https://myarc.dev/docs/cli-and-mcp/) has the details.

## Report a problem

Open an issue on [GitHub](https://github.com/bitbonsai/myarc/issues). Include the myArc version, shown at the top of the app menu next to the Apple menu, your macOS version and the steps that cause the problem. Leave out private pages.

[Next: Design system →](https://myarc.dev/docs/design-system/)
