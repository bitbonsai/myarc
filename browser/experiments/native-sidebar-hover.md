# Hidden sidebar hover candidate

MA-007.c-hover uses [patch 0013](../patches/legacy/0013-native-sidebar-hover-peek.patch) to add edge activation to Chromium's existing expand-on-hover path. The parent built it and verified the basic legacy-sidebar journey in the actual browser. Automated native regressions and full visual/motion acceptance remain open.

## Behavior and evidence

A user-supplied Arc recording shows a hidden sidebar appearing as an overlay at the browser's left edge. The page logo and heading keep their positions during this reveal. A later click pins the sidebar and then shifts page layout. The reference is 1714 × 1080 at 30 fps; producing build is unverified. No-click dismissal, exact hit width, delay and easing have not been isolated. Original media and frames remain outside Git.

Project requirement: hovering the hidden browser's leading client edge temporarily reveals the full sidebar. Its controls and tabs remain usable. Leaving dismisses it unless focus, a menu or a drag requires retention. The existing toggle converts the peek to permanent visibility, which reserves page width. A hidden sidebar reserves zero width and leaves no icon rail.

The candidate uses the native two-DIP collapsed resize width as its activation zone. This is a prototype choice, not an Arc measurement. It keeps Chromium's existing velocity samples, delay and animation constants. The existing sidebar preference still controls hover expansion. The leading edge mirrors in RTL. New edge activation is gated to active, ordinary browser windows outside fullscreen; fullscreen hover remains unaccepted rather than acquiring a second reveal controller.

## Implementation boundary

`VerticalTabStripRegionView` owns a `views::EventMonitor::CreateWindowMonitor`. It observes only its native window and never consumes events, adds an input view, activates a window or requests focus. Page presses cancel pending reveal samples before normal dispatch. The monitor is released on widget removal and region destruction. A delayed sample rechecks edge location, activation and native locks before revealing. Parent source review found Cocoa event root coordinates are window-relative; the candidate now obtains screen coordinates through `EventMonitor::GetLastMouseLocation`. The regression moves its real window away from the screen origin to cover this distinction.

The parity-only early return in `UpdateExpandOnHoverState` is removed. Force-collapse, keep-current and keep-expanded locks keep their upstream precedence. Existing focus containment retains an interactive peek. Because the parity omnibox lives inside the sidebar, its popup holds keep-expanded instead of the stock force-collapse lock; flag-off behavior keeps the stock lock.

`BrowserViewTabbedLayoutImpl` continues to distinguish overlay width from reserved horizontal width. Hover uses the saved expanded target instead of the stock default, so a resized sidebar reveals completely. Persistent toggles still use `kTabStripWidth`. Patch 0010's fixed inner layout and clip/translation are unchanged, for both the legacy and optional Rust body. Native inspection exposed stock hover behavior revealing truncated titles inside favorite tiles. The shared pinned layout now keeps those icons centered and titles hidden under the parity flag; the regression checks both properties while the pointer moves inside the peek. No Rust, service, lifecycle, command, palette or BrowserView policy functions change. `--myarc-workspace-sidebar` remains independently off until its runtime gates pass.

## Exact baseline and order

Apply after the native stack through 0010 and normalized 0011, over Chromium `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`, with the canonical 16-file foundation payload from project commit `fd3880d`. The optional order is 0002, 0004, 0005, corrected 0006, 0007, 0008, 0009, 0010, normalized 0011, then 0013. Patch 0012 is independent and absent from this baseline. Keep independent endpoint-rounding changes.

Input SHA-256 fingerprints, relative to Chromium's source root:

```text
c811acb9cd4a07cfe0e251ef5eb7aa68d63cb84100dfdd34eb5d362909d439b3  chrome/browser/ui/views/frame/vertical_tab_strip_region_view.cc
09307b4d2989cc1f1c64212193a1e01d3c4b8abf5cdf3c6dcde30527094224e8  chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h
3966c2b33a077cdd4db93950325d2b7a41ea4ae5672ee69de14ce1571b630e47  chrome/browser/ui/views/frame/vertical_tab_strip_region_view_interactive_uitest.cc
47acba95fc8ead27423a9648416145e01eba09fa4ad1da82121841be4c6822f7  chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.cc
```

The fifth input is `chrome/browser/ui/views/tabs/common/tab_view_vertical_layout.cc`, SHA-256 `514ef19638b958df7b5b2fea2bc2e707e2ce70fdf8f7e9445b871636053d5c7e`.

Full Git input/output blob hashes are also in the patch. Five upstream files change. There are no BrowserView header/source hunks to merge with parallel command-safety work.

## Checks and remaining acceptance

Passed in scratch, with the external checkout read-only:

- Strict patch apply, exact output-byte comparison, reverse and exact baseline-byte restoration for all four files.
- Three C++ translation units checked with real Chromium headers, generated headers and existing target compiler flags, using `-fsyntax-only` and a header overlay. No browser objects or external build output were written. The first pass exposed an overloaded `OnEvent` warning and unsupported captured `BindOnce` callbacks; the final pass is clean.
- GN generation in a separate source overlay, then header-graph checks for `vertical_tab_strip_region_view`, `browser_view_tabbed_layout_impl` and `interactive_ui_tests`. Existing frame `BUILD.gn` passes GN format; no GN dependency edits were needed.
- Changed-line Chromium clang-format is idempotent; patch whitespace and repository relative document links pass.

The added native test is `MyarcHoverSidebarInteractiveUiTest.HiddenEdgeOverlaysThenToggleReservesWidth` in `interactive_ui_tests`. It sends real window-local mouse moves, checks hidden width/visibility, overlay page bounds, expanded inner bounds, no-click dismissal and persistent toggle width restoration at a resized 278-DIP width. It does not call the edge observer directly. It is syntax-checked but UNRUN. Retain the 0010 `MyarcSlidingSidebarTest.FixedInnerBoundsAndBothGutters` regression in `browser_tests` for the full inner-geometry sweep.

Parent checks now include four real-header syntax compilations after the coordinate/pin fixes, a full native `chrome` build, and five-file reverse/apply byte reproduction. In the marked disposable browser, Cmd+S hid the sidebar, edge hover revealed it, movement inside retained it, and movement onto the page dismissed it. Fresh recordings show real pin switching and centered favicon tiles. With the app-only reduced-motion flag, the page viewport remained 1280 × 788 before and during the overlay. Normal launch flags and Follow System appearance were restored afterward. Real caption visibility is tracked by [patch 0015](native-hover-caption-controls.md).

The parent still must execute the automated tests and inspect interrupted/reversed interactions. Check page bounds throughout animation, legacy and optional body controls, keyboard focus transfer, menu/drag lock release, edge clicks, inactive windows, RTL, fullscreen entry/exit and popup anchors. AppKit caption behavior and the exact floating corner/outline treatment remain native seams requiring inspection. This patch reuses upstream painting; it does not establish 1:1 timing, visual parity or safe daily use.
