# Optional native saved-page fade

MA-007.c-fade, 2026-10-25. [Patch 0014](../patches/legacy/0014-native-pinned-site-fade.patch) builds in native Chromium. The parent recorded visible fade intermediates between live native pins and a direct cut under app-only reduced motion. Automated browser regressions and the Rust-sidebar route remain unrun.

This is new myArc polish requested conditionally on a small native change. The 120 ms incoming fade is a prototype choice, not observed or measured Arc behavior. It fades the incoming page through the existing compositor. DOM morphing and old/new crossfading remain outside this slice.

## Presentation boundary

Two production files change, both under `chrome/browser/ui/views/frame/`:

- `browser_view.cc`: includes and two hunks inside `OnActiveTabChanged`. Determine eligibility while both pages are known, before the existing null detach. Start presentation only after existing attachment, layout, focus restoration and workspace-canvas update.
- `contents_web_view.cc`: includes, a local opacity-reset helper, destructor reset, blocked-modal reset and the start of `SetWebContents`. Stop opacity animation and restore opacity 1 before content changes, including null/empty-Space detach and ineligible switches.

`browser_view_browsertest.cc` adds one real-browser regression case to the existing fixture. No GN, Rust core, workspace policy, command, omnibox or sidebar-layout changes.

The fade uses the existing Mac `NativeViewHost` layer and `ui::ScopedLayerAnimationSettings`. Source ownership is explicit:

1. `NativeViewHostMac` creates its holder layer and returns it through `GetUiLayer`.
2. `WebContentsViewMac::ViewsHostableAttach` calls `RenderWidgetHostViewMac::SetParentUiLayer` for child render views; detach clears that parent.
3. `BrowserCompositorMac::TransitionToState(UseParentLayerCompositor)` adds its renderer `LayerSurface` beneath that parent. `RenderWidgetHostViewMac` disables the separate Cocoa display path when a parent layer is supplied.

Animating holder opacity therefore reaches the page surface, rather than only `ContentsWebView`'s background. The start also requires a drawn incoming view, attached compositor and child layer. Existing layer destruction detaches and destroys its animator; no timer, callback or retained page/image ownership is added. The required `MultiContentsView` layout parent stays visible.

## Eligibility and cancellation

Only different, already-live saved pages in the same window and exact browser context qualify. Both must have committed HTTP(S) URLs, live main frames, and no destruction, crash, load, pending navigation, split or blocked state. Fullscreen, inactive windows, closing-all, child modal dialogs and reduced motion skip animation. New tabs, first attachment and empty-Space returns remain immediate.

With the Rust sidebar adapter, one existing `Sidebar()` snapshot supplies Favorites and Space-pin membership through existing item bindings. This is a presentation query, not duplicated placement policy. Adapter failure skips the fade. Without that adapter, existing native pinned tabs qualify.

Selection, page attachment, navigation and focus handlers stay synchronous and unchanged. Rapid eligible switches reset opacity before restarting. Ineligible switches and null detaches reset to opacity 1. Modal blocking and view teardown cancel presentation too. No old-page image is held beneath a new URL.

## Baseline and application order

Source baseline: Chromium `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`, optional native patches 0002/0004/0005/corrected 0006/0007/0008/0009/0010/normalized 0011, with canonical owned foundation payload at `fd3880d`. Patches 0012 and 0013 were absent from that external source snapshot.

Merge order: existing native stack through 0011, corrected command patch 0012, hover patch 0013, then 0014. The `a115f80` 0012 BrowserView hunks apply before or after 0014 with byte-identical combined output. They touch other methods, not `OnActiveTabChanged`. Patch 0013 touches separate files. Parent must verify its final combined stack; this check is not a build of 0012/0013/0014 together.

Baseline SHA-256, paths relative to Chromium source root:

```text
chrome/browser/ui/views/frame/browser_view.cc
  aa62ccc0316c57095dbb9ed92675122eed962701f98fe7bd0c2facf9887882c3
chrome/browser/ui/views/frame/browser_view_browsertest.cc
  c8b080f31045a474a8ccf09c7b9895b5a844b79b55c790ed0fc3f6c8151feedd
chrome/browser/ui/views/frame/contents_web_view.cc
  6cd0a73d5cb4d2781d5930620d7e50e5176608f37cb8f0e01a4e8d17b28dcd49
```

Patch SHA-256: `5040cd31a14ababb5bdeb87a35cd4bd75c7924f3373a059765ae50517564b466`.

## Verification and remaining acceptance

Passed in copied scratch sources only:

- Changed-line Chromium clang-format, version 23.0.0git.
- Chromium target-flag `clang++ -fsyntax-only` for both production files and the browser-test file, using existing generated headers.
- Scratch GN generation and header-dependency checks for `//chrome/browser/ui/views/frame:browser_view`, `:contents_web_view` and `:browser_tests`.
- Strict indexed `git apply --whitespace=error` and reverse, including check mode. All three patched files match expected bytes; reverse restores every original byte. External input fingerprints remain unchanged.
- The 0012 BrowserView hunk-order check described above.

Parent verification added full native compilation/linking and fresh owned-window recordings in the marked disposable Profile. Plain mouse clicks switched between the first native pin and a synthetic HTTP pin; normal frames show intermediate opacity. With `--force-prefers-reduced-motion`, actual page observation reported reduced motion and the recorded switch cut directly to the new page. These are manual native observations, not execution of the browser regression. Capture rates vary, so the configured 120 ms is a source value, not a frame-derived timing measurement. The app-only flag was removed afterward; OS settings stayed unchanged.

`BrowserViewPinnedFadeTest.EligibilityMotionAndReset` is compiled for syntax, **not executed**. It uses actual local HTTP pages and native pinned tabs to check eligible switching, immediate selected contents, rapid switching, unsaved-page skip, reduced-motion skip and null-detach cancellation. Rust-sidebar Favorites/Space-pin routing is not exercised by that legacy fixture.

Parent still must verify both saved categories with the Rust body. Check actual page pixels, perceived timing, rapid switching, reduced motion, focus, loading/dormant pages, empty Spaces, split/fullscreen/modal/crash skips and teardown. Reject the candidate if native page motion is ineffective or distracting. Native acceptance remains required; no additional capture or platform pipeline is authorized by this candidate.
