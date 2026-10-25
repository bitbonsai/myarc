# Blank-only native page material

MA-007.c-page-background, 2026-10-25. Build candidate over myarc `7e827d5d134696060865fee460019af064bfbc39` and external Chromium `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`, with the installed optional stack through 0016 and independent rounding. Native acceptance remains pending; workspace renderer stays off by default.

## Cause and lifecycle

The parent's settled unstyled HTTP captures showed black form text over shell material, including after saved close/reopen. `ContentsWebView::SetWebContents` unconditionally hid the background and set a transparent page-base override for every visual-parity page. `UpdateBackgroundColor` also sent a transparent renderer-view background. Authored CSS could hide the fault.

[Patch 0017](../patches/0017-native-page-background.patch) limits these overrides to an initial empty document, `about:blank` and upstream-recognized NTP origins. It uses the primary main frame's committed URL, because NTP's virtual `chrome://newtab` URL can differ from its actual origin. An empty workspace still detaches its contents through the existing adapter path.

- On attachment and `PrimaryPageChanged`, select blank material or ordinary upstream backgrounds. This supported observer covers cross-document navigation, same-WebContents navigation, back/forward-cache restoration and prerender activation after committed properties update. Same-document changes cannot cross the blank/ordinary origin boundary. The inherited WebView callback still runs.
- Ordinary pages receive `SetPageBaseBackgroundColor(std::nullopt)`, restoring Blink's default and per-page color-scheme handling. The renderer view receives the existing opaque browser color-provider fallback, including letterboxing. Theme updates and `RenderViewReady` keep their existing background refresh paths. Authored CSS is unchanged.
- Before detaching or switching, clear the outgoing page-base override and restore its renderer-view fallback. Hidden pages may navigate while unobserved. Teardown also detaches while the derived view can clear its override. Reattachment recomputes from the current committed page.

Actual callers traced: WebView attachment/teardown and main-frame lifecycle; ContentsWebView theme/letterboxing/render-ready updates; the ChromeOS background-visibility delegate; BrowserView tab switching and workspace-empty detachment. The content API's stored override, page broadcasts, Blink reset behavior and render-widget opacity hook were inspected. No CSS injection, forced white canvas, polling or OS appearance change was added.

Four external files change, all under `chrome/browser/ui/views/frame/`:

- `contents_web_view.cc` and `.h`: background lifecycle only. Existing rounding, clipping, fade reset and modal handling remain.
- `BUILD.gn`: declare the existing upstream search predicate dependency.
- `browser_view_browsertest.cc`: one additional case in the existing shell/fade fixture. It checks unstyled HTTP opacity, authored color, dark page canvas, same-WebContents blank transitions, back/forward, empty detachment, hidden navigation, tab reattachment and NTP return. The previous fade regression is retained.

Flag-off paths retain upstream behavior. No workspace model, saved identity, native pin policy, 120 ms fade eligibility/timing, reduced-motion policy, header or recovery-anchor source changed.

## Packaging and actual checks

Apply 0017 once against the frozen installed stack. It is already installed in the handed-off external source tree.

- 0017 SHA-256: `a0d19e3ca8afc4bbf48e6d45fd8f6261938e56ef7717379281d376bb2614e59b`.
- Unchanged 0012 SHA-256: `8f8023789fdb66841128879766afdb89866c7e393453185336eba69745f66e53`.
- Unchanged 0016 SHA-256: `d394c5e398bc73e820f84a08b758a620020331bf6ac685a8654b9b61bb851ae8`.
- Captured 72 external source files before edits. Strict indexed reverse restores every baseline hash; replay reproduces every installed hash. All 68 unrelated files, including owned foundation and rounding sources, remain byte-identical. All pre-existing patch files, including canonical 0001, remain unchanged.
- `gn gen out/MyarcM0`: exit 0. Header dependency checks for `frame:contents_web_view` and `frame:browser_tests`: exit 0 each.
- Test translation unit compiled with its actual generated target flags and `-fsyntax-only`: exit 0, empty diagnostics. No browser-test dependency graph was built.
- `PATH+=depot_tools DEPOT_TOOLS_UPDATE=0 autoninja -C out/MyarcM0 -j8 chrome`: exit 0, 20:46:54–20:47:22 UTC. Fresh Ninja entries confirm production compilation and `libchrome_dll.dylib` linking. Actual executable `comm` paths were checked before the build; no MyarcM0 executable was running. Post-build process checks found no remaining build.
- Changed-line Chromium clang-format, GN formatting, document links, foundation payload freshness and whitespace checks pass.

Private proof directory: `/tmp/myarc-m0-runtime/page-background-fix/`. It retains `baseline.json`, source copies, `installed.json`, strict replay helper/log, build log/terminal exit/timestamps, fresh build entries, syntax command/log, GN logs and process snapshots. An initial replay check needed Git's index stat refresh after non-indexed reversal; bytes already matched. An initial multi-label `gn check` invocation was rejected by its CLI; separate checks passed.

Native cases executed: **zero**. No app launch, desktop input, profile access or service kill occurred. Parent owns fresh blank/HTTP/authored-color/light-dark checks, same-page navigation and tab/empty-Space switching, plus saved-page fade/reduced-motion and saved-identity regression acceptance. Existing successful Pin → Favorite → generic Unpin, deliberate Unsave and saved close/reopen observations remain historical evidence, not a runtime recheck of this binary. Recovery/durability, full C++ suites and broader policy gates remain open.
