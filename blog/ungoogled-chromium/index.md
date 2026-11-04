HTML: https://myarc.dev/blog/ungoogled-chromium/

[All posts](https://myarc.dev/blog/)

# Taking a few patches from ungoogled-chromium

November 04, 2026

[Mauricio Wolff](https://github.com/bitbonsai)3 min read

myArc's nightly now takes a handful of privacy changes from [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium), a Chromium fork that exists to cut out the parts that talk back to Google. Its code is BSD-3-Clause, which lets me reuse it as long as I credit it. The credit is in [ATTRIBUTION.md](https://github.com/bitbonsai/myarc/blob/main/ATTRIBUTION.md) and in a comment at every edit.

I only took the pieces that fit myArc.

## Why not rebase on it

The obvious move would be to build myArc on top of ungoogled-chromium and be done. Two things stop me. It follows Chromium's stable releases, while myArc follows Chromium's main branch, so switching would move the whole project onto an older base. And it strips out the real Chrome Web Store, which people coming from Arc need for their extensions.

So I read through its flags and patches and cherry-picked.

## The four flags

Build flags are switches you set when Chromium gets compiled. These four now live in `browser/args-nightly.gn`:

- `disable_fieldtrial_testing_config` gives the nightly Chromium's stable feature defaults, instead of the experiments Chromium switches on for test builds.
- `enable_reporting = false` removes the Reporting API and Network Error Logging, which let sites collect error reports from your browser.
- `enable_remoting = false` removes the Chrome Remote Desktop host.
- `enable_hangout_services_extension = false` removes the built-in Hangouts services extension.

## The five patches

Flags only go so far, so I also adapted five of ungoogled-chromium's patches. Some of them change more than I wanted, so I kept only the parts I needed. Agents dry-ran each one against myArc's Chromium before it went in, and the nightly builds with all five.

- Chromium no longer asks Google's servers for the current time.
- No Domain Reliability beacons, the small reports Chromium sends Google when a connection to one of its sites fails.
- WebRTC text logs, which video calls can upload for debugging, are off by default.
- No bundled Privacy Sandbox attestations list, the list of ad companies allowed to use Chrome's built-in ad APIs.
- Manifest V2 extensions keep working. That's the older extension format that full ad blockers like uBlock Origin were built on, and Chromium now disables it.

## What I left out

Some of ungoogled-chromium's changes would hurt myArc, and others add little here.

- `disable-ai` deletes pieces of Reading mode and PDF text recognition along with the AI features. myArc keeps AI off by default instead, so those pieces still work.
- Google host detection mostly stops Chromium from sending Google's sites an experiments header, `X-Client-Data`. With stable feature defaults that header should already carry very little, and the patch touches hundreds of lines.
- The crash reporter patch wasn't needed. Uploads are already off in myArc's builds, and a pre-alpha needs its local crash dumps.
- The Web Store patches, domain substitution (rewriting Google domains across the whole source) and binary pruning would break the real Chrome Web Store and extensions.
- `mei-preload` drops the list of media sites allowed to autoplay with sound, so those sites would stay muted until you'd used them for a while.

So myArc talks back to Google a little less now, and the Chrome Web Store still works. ungoogled-chromium itself cuts much deeper. myArc wants something narrower, a browser Arc's people can move into with their extensions intact, and these patches fit inside that.
