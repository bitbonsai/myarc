# Native shell experiment

MA-007.a targets 1:1 Arc visual parity. The current native iteration is incomplete. It keeps the full Chromium browser implementation and uses AppKit material through Objective-C++, with the existing navigation controls moved into the sidebar. Rust workspace rules are unchanged. A separate Swift/Tauri host has not been needed for this material/layout experiment.

## Reproduce the current iteration

Use the [foundation toolchain and build](../README.md). Start from its clean second integration, `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`, over upstream `8d208f20744ac6d4ce00cc3f9de6a14a9b8b926d`. This experiment was exercised on macOS 26.7, Apple Silicon. Its existing glass-frame integration requires macOS 26 or later.

The [optional prototype patch](../patches/0002-native-shell-prototype.patch) is separate from `upstream.json` and the accepted foundation patch payload. Apply it once in the external Chromium checkout, with `MYARC_REPO` set to this repository:

```sh
git apply "$MYARC_REPO/browser/patches/0002-native-shell-prototype.patch"
autoninja -C out/MyarcM0 -j 8 chrome
```

Create a disposable profile, never an existing browser profile:

```sh
PROFILE=$(mktemp -d "${TMPDIR:-/tmp}/myarc-native.XXXXXX")
mkdir -p "$PROFILE/Default"
touch "$PROFILE/.myarc-m0-test-profile"
printf '%s\n' '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"uncollapsed_width":230,"enabled_first_time":true}}' > "$PROFILE/Default/Preferences"
out/MyarcM0/Chromium.app/Contents/MacOS/Chromium \
  --user-data-dir="$PROFILE" \
  --no-first-run --no-default-browser-check \
  --enable-features=GlassFrame \
  --disable-features=InitialWebUI \
  --myarc-visual-parity --window-size=1300,808 about:blank
```

`InitialWebUI` must be disabled so the prototype receives Chromium's native navigation buttons. The sidebar setup rejects incompatible controls with an actionable check. Launching without the prototype flag preserves the existing foundation layout.

## Observed results

- Native compilation passed. The packaged patch was applied to copies of all thirteen baseline sources and the resulting bytes matched the modified checkout.
- AppKit behind-window material was installed. A controlled colored background visibly changed the sidebar and transparent empty content area; the reference palette and content inset were then refined.
- Native Cmd+L input navigated to a real page. Its explicit CSS background painted normally. Returning to `about:blank` restored the translucent empty canvas.
- A later controls pass removed the stock sidebar padding from the custom header, centered the existing navigation buttons and reused the native dock-to-left glyph. The address surface uses the dark reference palette outside high-contrast mode. The existing New Tab action now has a left-aligned label above the tab list. A native mouse click created exactly one page; sidebar Cmd+L then navigated that page to `about:blank`.
- Two relocation attempts crashed during layout. The first background access was guarded; the remaining crash was traced to missing native buttons in the mixed web/native toolbar. Consistent native controls fixed startup. Hiding and re-showing the address field during each layout also lost focus; preserving its visibility fixed keyboard navigation.

Original captures, supplied Arc references and raw logs remain private and outside the repository. The screenshots shown in the thread are actual native windows, not HTML browser mockups.

## Remaining differences and limits

The sidebar still uses upstream tab rows and a restyled native New Tab button. Arc's Favorites/Spaces arrangement and centered command bar remain unimplemented. Icon geometry, typography and interaction details need comparison; 1:1 parity has not been accepted.

The dark palette and expanded layout cover the first reference state only. Light mode, Reduce Transparency, collapsed/hover/fullscreen behavior, complete keyboard/VoiceOver behavior and extension/permission popup anchoring remain unverified. Several stock toolbar controls are omitted from this preview. It is unsuitable for daily-use acceptance. Existing foundation security/distribution gates remain open; import work and broader test expansion are deferred.
