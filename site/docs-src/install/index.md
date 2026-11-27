---
title: Install myArc
summary: "What your Mac needs, what macOS asks on first launch, the welcome tour, and how myArc updates."
area: Getting started
order: 1
---

myArc ships as an alpha build, made from the latest code and published without release testing. Each build is a disk image (DMG) on the [download page](https://myarc.dev/download/).

## What you need

- A Mac with Apple silicon. Intel Macs aren't supported.
- macOS 26 or later. The glass window frame needs macOS 26, and myArc has only been tested on macOS 26.7.

## Install the app

### With Homebrew

```sh
brew install --cask bitbonsai/tap/myarc
```

Homebrew puts `myArc.app` in Applications and links the `myarc` CLI onto your `PATH`. Updates continue through myArc's built-in updater.

### From the DMG

1. Download the DMG from the download page.
2. Check the file against the SHA-256 checksum listed next to the download button. Run this in Terminal and compare the two values. They must match. If you kept older builds in Downloads, it prints one line per file.

   ```sh
   shasum -a 256 ~/Downloads/myArc-*.dmg
   ```

3. Open the DMG and drag myArc to Applications.

## First launch

Open myArc from Applications. Each build is signed with an Apple Developer ID and notarized, which means Apple has scanned the build. Gatekeeper, the part of macOS that checks downloaded apps, asks once whether to open an app from the internet. Choose Open.

macOS may then ask for your login password so myArc can read the key that protects its saved data. Choose Always Allow. Builds are signed with the same Developer ID, so later updates stop asking.

## Moving from myArc Nightly

Earlier builds shipped as `myArc Nightly.app` with a separate bundle ID, so they won't auto-update to `myArc.app`. Install `myArc.app` from the DMG once, then remove `myArc Nightly.app` from Applications. On first launch, myArc moves your profile from `~/Library/Application Support/myArc Nightly/` to `~/Library/Application Support/myArc/` and migrates your Keychain encryption key automatically.

## The welcome tour

The first time myArc opens, a welcome window plays with sound. Choose Import from Arc… to bring your Spaces over, as [Import from Arc](../import-from-arc/) describes, Start fresh to begin without an import, or Skip tour. Three short practice steps follow: open a site from the command bar, pin a tab with Cmd+D, and switch to another Space or make one.

When you choose Start fresh or Skip tour, myArc seeds a `myArc Basics` folder and opens the quick start page, [myarc.dev/welcome](https://myarc.dev/welcome/), before the practice steps begin. It does this once, and skips it after an Arc import. To take the tour again, open Settings, go to Advanced and click Replay welcome tour.

## 1Password with Touch ID

myArc automatically reads Chrome, Arc and Chromium native messaging manifests on macOS, so the 1Password extension can talk to the 1Password desktop app with no terminal setup. To unlock with Touch ID instead of typing your account password, tell 1Password to trust `myArc.app` once:

1. Open and unlock 1Password on your Mac.
2. Go to Settings > Browser and click Add Browser.
3. Select `myArc.app` in Applications, then restart myArc.

## Update to a newer build

myArc checks myarc.dev once a day for a newer build and asks before it installs one. To check right away, choose myArc > Check for Updates…. Each build is signed, and myArc refuses an update whose signature doesn't match.

myArc gets no separate security updates between builds, so a Chromium fix reaches you with the next build.

To update by hand:

1. Quit myArc with Cmd+Q.
2. Download the new DMG and drag myArc to Applications, replacing the old copy.
3. Open it. The new download goes through the first-launch step again.

Your Spaces, tabs and settings live in your user folder, outside the app, so replacing the app keeps them. [Your data](../your-data/) shows where.

The [GitHub releases page](https://github.com/bitbonsai/myarc/releases) lists every build.
