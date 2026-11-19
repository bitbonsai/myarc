# Bundled blocker: uBlock Origin Lite

myArc blocks ads and trackers by default with uBlock Origin Lite (uBOL), Raymond Hill's Manifest V3 content blocker, under GPL-3.0 ([LICENSE.txt](LICENSE.txt), copied unchanged from the release).

## What is here

- `uBOLite_2026.1116.1650.chromium.zip` is the Chromium package of [release 2026.1116.1650](https://github.com/uBlockOrigin/uBOL-home/releases/tag/2026.1116.1650), downloaded unchanged from that page on 2026-11-17. Its rulesets, compiled from the filter lists, are inside, so nothing is fetched at first run.
- `source/` holds the source the package was built from: [gorhill/uBlock](https://github.com/gorhill/uBlock) at commit `9510a0b38cdbb95e88f2bd033c0da85bfd4b1b4c`, the newest commit when the release was cut and the one its notes link, plus its two submodules at the commits that tree pins, [codemirror-ubol](https://github.com/gorhill/codemirror-ubol) `a9762d064abb81c24a83de7f36507b7c0f2cbb70` and [s14e-serializer](https://github.com/gorhill/s14e-serializer) `1f028eb315c14bb11f23f0ef6c4bf6c339d3b0b1`. The archives are GitHub's tarballs of those commits. `platform/mv3/README.md` and `tools/make-mv3.sh` in the uBlock tree explain the build, which downloads the filter lists named in `platform/mv3/make-rulesets.js`.
- `SHA256SUMS` lists every file's SHA-256. `tools/test_blocker_vendor.py` checks them.

## How myArc uses it

`//myarc:blocker_bundle_data` copies the zip into the app's framework Resources as `ublock_origin_lite.zip`. On a Profile's first launch, `blocker.cc` unzips it into the Profile directory and installs it with Chromium's own installer, as a regular extension with the fixed ID `aefnaoagnnooajnoebfnnnpdngplgemc`. The ID comes from a public key made for myArc; its private half was never kept, because an unpacked install signs nothing. A Profile pref (`myarc.blocker.installed`) records the install, so a blocker the user removes stays removed. Main-frame page loads wait, at most 10 seconds, until the blocker's network rules are live, so a new Profile's first page is filtered.

The site info popover's "Block ads on this site" switch (`blocker_switch.cc`) uses the blocker's own per-site setting, without changing its files. It opens the blocker's `LICENSE.txt` in a hidden page of the Profile's copy and sends the messages the blocker's popup sends its service worker: `popupPanelData` and `getDefaultFilteringMode` to read the site's level, `setFilteringMode` to change it. So the switch and the popup always agree. An update that renames those messages breaks the switch; `tools/check_blocker.sh` catches that.

The extension makes no network requests of its own with its default settings, and its manifest has no update URL. myArc's extension updater skips it, so it never asks the Chrome Web Store about it. Updating the bundled copy is a separate task: replace the zip and source archives, update this file, `SHA256SUMS`, the zip name in `BUILD.gn` and the entry in `ATTRIBUTION.md`, then run `python3 tools/test_blocker_vendor.py` and `tools/check_blocker.sh`.
