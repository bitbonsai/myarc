---
title: Install myArc
summary: "What your Mac needs, what macOS asks on first launch, and how to move to a newer nightly."
area: Getting started
order: 1
---

myArc ships as a pre-alpha nightly, a build made from the latest code and published without release testing. Each nightly is a disk image (DMG) on the [download page](https://myarc.dev/download/).

## What you need

- A Mac with Apple silicon. Intel Macs aren't supported.
- macOS 26 or later. The glass window frame needs macOS 26, and the nightly has only been tested on macOS 26.7.

## Install the app

1. Download the DMG from the download page.
2. Check the file against the SHA-256 checksum listed next to the download button. Run this in Terminal and compare the two values. They must match. If you kept older nightlies in Downloads, it prints one line per file.

   ```sh
   shasum -a 256 ~/Downloads/myArc-Nightly-*.dmg
   ```

3. Open the DMG and drag myArc Nightly to Applications.

## First launch

Open myArc Nightly from Applications. From 2026.11.18 on, each nightly is signed with an Apple Developer ID and notarized, which means Apple has scanned the build. Gatekeeper, the part of macOS that checks downloaded apps, asks once whether to open an app from the internet. Choose Open.

macOS may then ask for your login password so myArc can read the key that protects its saved data. Choose Always Allow. Nightlies from 2026.11.03 on are signed with the same Developer ID, so later nightlies stop asking.

## Update to a newer nightly

myArc doesn't update itself, and it gets no security updates between nightlies. To move to a newer one:

1. Quit myArc with Cmd+Q.
2. Download the new DMG and drag myArc Nightly to Applications, replacing the old copy.
3. Open it. The new download goes through the first-launch step again.

Your Spaces, tabs and settings live in your user folder, outside the app, so replacing the app keeps them. [Your data](../your-data/) shows where.

The [GitHub releases page](https://github.com/bitbonsai/myarc/releases) lists every nightly.
