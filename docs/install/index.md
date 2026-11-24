HTML: https://myarc.dev/docs/install/

# Install myArc

What your Mac needs, what macOS asks on first launch, the welcome tour, and how myArc moves to a newer nightly.

myArc ships as an alpha nightly, a build made from the latest code and published without release testing. Each nightly is a disk image (DMG) on the [download page](https://myarc.dev/download/).

## What you need

- A Mac with Apple silicon. Intel Macs aren't supported.
- macOS 26 or later. The glass window frame needs macOS 26, and the nightly has only been tested on macOS 26.7.

## Install the app

1. Download the DMG from the download page.
2. Check the file against the SHA-256 checksum listed next to the download button. Run this in Terminal and compare the two values. They must match. If you kept older nightlies in Downloads, it prints one line per file.

   ```
   shasum -a 256 ~/Downloads/myArc-Nightly-*.dmg
   ```
3. Open the DMG and drag myArc Nightly to Applications.

## First launch

Open myArc Nightly from Applications. From 2026.11.18 on, each nightly is signed with an Apple Developer ID and notarized, which means Apple has scanned the build. Gatekeeper, the part of macOS that checks downloaded apps, asks once whether to open an app from the internet. Choose Open.

macOS may then ask for your login password so myArc can read the key that protects its saved data. Choose Always Allow. Nightlies from 2026.11.03 on are signed with the same Developer ID, so later nightlies stop asking.

## The welcome tour

The first time myArc opens, a welcome window plays with sound. Choose Import from Arc… to bring your Spaces over, as [Import from Arc](https://myarc.dev/docs/import-from-arc/) describes, Start fresh to begin without an import, or Skip tour. Three short practice steps follow: open a site from the command bar, pin a tab with Cmd\+D, and switch to another Space or make one.

When the tour ends after Start fresh or Skip tour, myArc opens a quick start page, [myarc.dev/welcome](https://myarc.dev/welcome/), in a new tab. It does this only once, and not after an import. To take the tour again, open Settings, go to Advanced and click Replay welcome tour.

## Update to a newer nightly

Nightlies after 2026.11.20.2 update themselves. Once a day, myArc checks myarc.dev for a newer nightly and asks before it installs one. To check right away, choose myArc \> Check for Updates…. Each nightly is signed, and myArc refuses an update whose signature doesn't match.

myArc gets no security updates between nightlies, so a Chromium fix reaches you with the next nightly.

Nightly 2026.11.20.2 and older can't update themselves. To move from one of those, install the new nightly by hand once:

1. Quit myArc with Cmd\+Q.
2. Download the new DMG and drag myArc Nightly to Applications, replacing the old copy.
3. Open it. The new download goes through the first-launch step again.

Your Spaces, tabs and settings live in your user folder, outside the app, so replacing the app keeps them. [Your data](https://myarc.dev/docs/your-data/) shows where.

The [GitHub releases page](https://github.com/bitbonsai/myarc/releases) lists every nightly.

[Next: Import from Arc →](https://myarc.dev/docs/import-from-arc/)
