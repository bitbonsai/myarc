# Widevine in myArc

Widevine is Google's DRM. Netflix, Spotify and Disney+ use it to protect streams. The piece that decrypts them is a closed binary called the CDM (content decryption module). myArc nightlies build without it, so those services don't play (see [known limitations](../../site/docs-src/known-limitations/index.md)).

Short answer: switching the CDM on is a small build change, and the browser can fetch it from Google at runtime. On macOS that isn't enough. Streaming services also check a VMP signature (Verified Media Path, Widevine's proof that the browser binary is genuine), and a custom Chromium build can only get one through a commercial agreement.

Chromium facts below come from the local checkout at 158.0.8122.0 (`1acd456ca119`). Paths are relative to `chromium/src`.

## Build flags

`third_party/widevine/cdm/widevine.gni`:

- `enable_widevine` is on by default only for Chrome-branded builds and Android. The comment says it "can be optionally enabled in Chromium on non-Android platforms".
- `enable_library_widevine_cdm` follows from it on mac arm64, since `enable_library_cdms` is true on Views platforms (`media/media_options.gni`).
- `enable_widevine_cdm_component` follows from that on mac. It isn't a GN arg you set. It registers the component installer in `chrome/browser/component_updater/widevine_cdm_component_installer.cc`, extension id `oimompecagnajdejgnnjijobebaeigek`.
- `bundle_widevine_cdm` defaults to true only for Chrome-branded builds. It copies CDM files into the app at build time, and those files aren't in the public tree (`third_party/widevine/README.chromium`: "No third-party files are checked in here").
- `enable_cdm_host_verification` (in `media/media_options.gni`) defaults to Chrome-branded mac and Windows builds only. It runs `third_party/widevine/scripts/signature_generator.py`, which comes from `chrome-internal` and is an empty directory in our checkout.

So `enable_widevine = true` with nothing else gives a build that ships no Google code and downloads the CDM on first use. That is how Brave, Vivaldi and other Chromium browsers get it.

The component updater asks `https://update.googleapis.com/service/update2/json`, with plain HTTP as the fallback (`components/component_updater/component_updater_url_constants.cc`). Brave puts its own server in front and [redirects Google's components to Google's server](https://github.com/brave/brave-browser/wiki/Component-Updater). myArc hasn't patched the updater, so it would talk to Google directly.

## License

The CDM's license (`third_party/widevine/LICENSE`) says you "may not use, modify, sell, or otherwise distribute the Software without a separate license agreement with Google". myArc must not bundle it.

Fetching it at runtime keeps it out of what myArc distributes. GPL-3.0 obligations attach to conveying the program, and the user's browser downloads the CDM from Google. A [2015 chromium-dev thread](https://groups.google.com/a/chromium.org/g/chromium-dev/c/16hDHEpgUb8) reports that the CDM is free to use when fetched through the component updater. That is secondhand and old, so treat it as a hint.

The one GPL question is that the CDM runs in a process with browser code. Chromium itself is BSD-licensed, so this only concerns myArc's own GPL-3.0 code. myArc owns that code and can add a GPL-3.0 section 7 permission that allows loading the Widevine CDM. Talk to a lawyer before shipping. This doc isn't legal advice.

What other projects do:

- Brave keeps Widevine off by default. You opt in, and its [support page](https://support.brave.app/hc/en-us/articles/360023851591) says the extension is owned by Google, loaded from Google's servers, and that you accept Google's terms.
- ungoogled-chromium's [FAQ](https://ungoogled-software.github.io/ungoogled-chromium-wiki/faq) says Widevine "should work" but ships nothing. It tells users to copy `WidevineCdm` from a Google Chrome of the same major version.
- Linux distributions leave it out because redistribution needs Google's permission.

## VMP on macOS

On macOS and Windows the CDM checks the browser's VMP signature and tells the license server the result. Linux has no VMP.

castLabs' [VMP page](https://github.com/castlabs/electron-releases/wiki/VMP) says that without it the status is `PLATFORM_UNVERIFIED`, which on macOS and Windows "means production streaming services will typically not work". castLabs maintainers say their development-signed builds "will not work with commercial services, like Netflix", and a Netflix error (M7121-1331) was traced to missing VMP ([issue 122](https://github.com/castlabs/electron-releases/issues/122)). Spotify returned `widevine-license` 500 errors on a build signed in the wrong order ([Lightrun copy of the issue](https://lightrun.com/answers/castlabs-electron-releases-getting-widevine-license-error-on-the-spotify-web-player)). Netflix may also have tightened this over time: community reports say unsigned forks like Thorium stopped working after a Netflix change ([Reddit thread](https://www.reddit.com/r/Chromium/comments/fxlbay/how_do_you_stream_netflix_on_chromium/)).

Expect all three services to refuse playback on macOS without VMP. Lower resolution only is the wrong mental model. I found no primary source for Disney+ specifically. Netflix and Spotify are better documented. Test each one before promising anything.

### castLabs EVS

From the [EVS wiki](https://github.com/castlabs/electron-releases/wiki/EVS):

- EVS is "a free service, but requires signup", with a verified email and a short code of conduct (one account, don't re-sign needlessly). The client is the `castlabs-evs` Python package.
- Free EVS only signs "application packages derived from official releases of Electron for Content Security". myArc is a custom Chromium build, so the free tier doesn't cover it.
- Custom builds need "commercial VMP certification" through the Widevine 3PL program. It "involves an audit of the application", and then "commercial 3PL access to EVS is granted" with 3PL-specific certificates. No price is published. Contact `3pl@castlabs.com`.
- Use streaming signatures. Persistent licenses were removed from the browser CDM in ECS v42.
- Order matters: "On macOS the VMP-signing needs to take place BEFORE the code-signing". The VMP step writes signature files into the bundle, and `codesign` then seals them, so notarization and the hardened runtime work as usual.

The other route is a direct Widevine license from Google. Vivaldi and Opera play Netflix on macOS, so they presumably have one, though I found no public statement. Google decides who gets one.

### Hardened runtime

The CDM is a dylib signed by Google. myArc signs with its own Team ID, and `tools/package_nightly.sh` gives the generic Helper library validation (`FULL=…,library`), as Chromium's `chrome/installer/mac/signing/parts.py` does for Chrome. In Chrome the Team IDs match. In myArc they won't, so library validation may block the CDM from loading in whichever helper hosts it. Check which helper starts the CDM process and whether it needs `com.apple.security.cs.disable-library-validation`. That would need a new entitlements plist, since `chrome/app` has none for the plain helper.

## Steps for myArc

1. Ask castLabs (`3pl@castlabs.com`) about 3PL terms, audit and price for a GPL-3.0 browser that is built from source. Nothing after step 3 matters without this.
2. Write the GPL-3.0 section 7 permission for loading the Widevine CDM and add it to `LICENSE` and `browser/README.md`.
3. Build a test with `enable_widevine = true` in `browser/args-nightly.gn`. Keep `bundle_widevine_cdm = false`. Check that the CDM appears in `chrome://components` and loads with no library validation error. Test on a site that reports VMP status, then Netflix, Spotify and Disney+.
4. Fix library validation if step 3 shows it. That touches the helper signing loop in `tools/package_nightly.sh`, plus an entitlements plist.
5. Add an opt-in before the first download, like Brave, so users accept Google's terms knowingly. This is a myArc patch under `browser/patches/chromium/`.
6. Once 3PL access exists, add EVS signing to `tools/package_nightly.sh` after the `ditto` and myarc CLI copy and before the first `codesign`. Load EVS credentials from the keychain or the environment, never from the repo.
7. Update `site/docs-src/known-limitations/index.md`, the `browser/README.md` nightly paragraph, the header comment in `browser/args-nightly.gn` and `site/release.json` fields if they list DRM.

## Open risks

- castLabs or Google may refuse, or charge more than the project can carry. Then step 3 alone gives working Widevine only for sites that accept unverified platforms.
- The 3PL audit may require a fixed release process or binaries that aren't reproducible from public source, which sits awkwardly with GPL-3.0.
- Every build's VMP signature goes stale when the binary changes. Nightlies would need signing on every release, with credentials on the build machine.
- Turning on the component updater makes the browser contact Google at runtime. That cuts against the privacy defaults taken from ungoogled-chromium, so the opt-in in step 5 matters.
- The Disney+ requirements are unconfirmed.
