---
title: "Why myArc is now GPL-3.0"
summary: "myArc moved from the MIT license to GPL-3.0 so it stays open for good. I don't plan to make money from it, and any modified version has to publish its code."
draft: false
---

Today I switched myArc's license from MIT to GPL-3.0. Both are open-source licenses, and for anyone who just downloads and uses myArc nothing changes. They differ in what someone can do with the code after taking it.

## Why I changed it

myArc exists because Arc is being sunsetted. Arc's code was never open, so when its company moved on, the people who used it every day had no way to keep it going. I don't want myArc to end up in the same place.

Under MIT, anyone can take the code, change it and ship a closed browser without sharing a line. Under GPL-3.0, anyone who distributes a modified myArc has to publish their source under the same license. Forks are welcome, but they stay open, and their improvements are available to everyone, including this project.

## I don't plan to make money from it

MIT's main advantage for an author is that it keeps commercial doors open: you can sell a closed edition or a separate commercial license later. I don't want either. myArc is something I'm building because I want to keep browsing this way, and I'm sharing it so others can too. Giving up those options costs me nothing.

## What else it gets us

- **Code from other browsers.** Privacy-focused Chromium browsers such as [Helium](https://github.com/imputnet/helium) publish their patches under GPL-3.0. myArc can now borrow them, for example for ad blocking, without license conflicts.
- **Patent protection.** GPL-3.0 includes a patent grant: contributors can't later sue users over patents covering the code they contributed.

## What changes in practice

- Releases and commits before the switch stay available under MIT. GPL-3.0 applies from [this commit](https://github.com/bitbonsai/myarc/commit/3bb4d23) onward.
- New contributions are licensed under GPL-3.0. No contributor agreement is needed.
- Chromium and the other bundled components keep their own licenses, listed in [ATTRIBUTION.md](https://github.com/bitbonsai/myarc/blob/main/ATTRIBUTION.md).
- One Chromium component, the RAR decompressor used to scan downloaded archives, has a license that conflicts with the GPL. It's turned off from the 2026.10.29 nightly onward, so RAR files you download aren't scanned.

The full text is in [LICENSE](https://github.com/bitbonsai/myarc/blob/main/LICENSE).
