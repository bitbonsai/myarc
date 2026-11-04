---
title: "Taking a few privacy pieces from ungoogled-chromium"
summary: "myArc now borrows four build flags and five patches from ungoogled-chromium. I left the rest, because it would break Google sign-in, sync and the Chrome Web Store."
date: 2026-11-04
draft: false
thumbnail: ungoogled-chromium-thumbnail.png
---

myArc's nightly now takes a handful of privacy changes from [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium), a Chromium fork that exists to cut out the parts that talk back to Google. Its code is BSD-3-Clause, which lets me reuse it as long as I credit it. The credit is in [ATTRIBUTION.md](https://github.com/bitbonsai/myarc/blob/main/ATTRIBUTION.md) and in a comment at every edit.

I only took the pieces that fit myArc, and skipped the rest on purpose.

## Why not rebase on it

The obvious move would be to build myArc on top of ungoogled-chromium and be done. Two things stop me. It tracks Chromium's stable releases, while myArc tracks Chromium main, so I'd be changing the ground under the whole project. And it removes things I want to keep, like the real Chrome Web Store and Google sign-in with sync. myArc keeps both.

So I read through its flags and patches and cherry-picked.

## The four flags

Build flags are switches you set when Chromium gets compiled. These four now live in `browser/args-nightly.gn`:

- `disable_fieldtrial_testing_config` ships stable feature defaults instead of the testing field trial config.
- `enable_reporting = false` removes the Reporting API and Network Error Logging.
- `enable_remoting = false` removes the Remote Desktop host.
- `enable_hangout_services_extension = false` removes the Hangouts services extension.

## The five patches

Flags only go so far, so I also adapted five of ungoogled-chromium's patches. Some of them touch more than I wanted, so I kept only the hunks I needed. Agents dry-ran each one before it went in, and the nightly build passes with them applied.

- No network time queries.
- No Domain Reliability beacons, which are small reports Chromium sends when a connection to a site fails.
- WebRTC text log uploads default to off.
- No bundled Privacy Sandbox attestations list.
- MV2 extensions stay enabled, without the deprecation warnings.

## What I left out

Some of ungoogled-chromium's best-known changes would hurt myArc more than help.

- **disable-ai** deletes pieces of Reading mode and PDF OCR along with the AI features. myArc keeps AI off by default instead, so those pieces still work.
- **Google host detection** would break Google sign-in and sync.
- **The crash reporter** change was unnecessary here. Uploads are already off, and a pre-alpha needs local crash dumps.
- **Web Store, domain substitution and binary pruning** would break the real Chrome Web Store and extensions.
- **mei-preload** blocks sound autoplay on media sites.
