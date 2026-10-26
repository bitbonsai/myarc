# Native hover caption controls

MA-007.c-caption supplies [patch 0015](../patches/legacy/0015-native-hover-caption-controls.patch), incremental after corrected 0013. The parent built the native app and observed real traffic lights during the hover overlay, including app-only reduced motion. Automated browser regressions and full caption-motion acceptance remain open.

## Defect and change

The parent observed working hidden-sidebar edge hover with fixed page positions, but real traffic lights disappeared during the peek. The collapse-only AppKit callback hid them because temporary hover leaves the permanent state collapsed. `GetCaptionButtonBounds()` also returned empty, losing their layout reservation.

The candidate moves that visibility write into `BrowserFrameViewMac::Layout`. Caption bounds remain nonempty while the existing `kTabStripHoverWidth` animation value is positive, including dismissal frames. Layout uses those same bounds to hide/show the real `NSWindow` close, minimize and zoom buttons. Persistent collapsed state still reserves no caption area. Buttons retain their upstream targets/actions and positions.

The verified source route is:

- `AnimateExpandOnHover()` starts the existing browser animation group.
- `OnAnimationProgressed()` invalidates region layout on progress and end, including reset endpoints.
- `View::InvalidateLayout()` propagates through BrowserView/ClientView to its ancestor frame. `FrameView::InsertClientView()` establishes that parent relationship.
- Normal root layout invokes the existing macOS frame layout. No new observer, timer or presentation notification is needed.

Only two production Objective-C++ files and their GN dependencies change. The fourth patch file adds native regression coverage. VTSRegion, pinned layout/tests, BrowserView command/query methods, workspace state and lifecycle stay unchanged. No external checkout or build output was modified; no desktop, browser, CDP, accessibility or input interaction was performed.

## Exact application boundary

Chromium base: `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5`.

Apply after 0002, 0004, 0005, corrected 0006, 0007, 0008, 0009, 0010, normalized 0011 and corrected 0013, with the parent's canonical 16-file payload context `fd3880d`. Patches 0012 and 0014 are independent. The inspected 0014 modifies only BrowserView, its browser test and ContentsWebView, disjoint from 0015.

[Evidence manifest](../evidence/native-hover-caption-controls.json) records every input/output SHA-256 and Git blob ID, the exact prerequisite 0013 patch hash and 0015 patch hash. Its four input hashes are authoritative for this incremental delta, rather than a claim that all external work matches a clean upstream commit. Preserve unrelated parent edits. Check those four inputs before applying:

```sh
git apply --check --index --whitespace=error-all "$MYARC/browser/patches/legacy/0015-native-hover-caption-controls.patch"
git apply --index --whitespace=error-all "$MYARC/browser/patches/legacy/0015-native-hover-caption-controls.patch"
```

The index commands require matching staged inputs. The parent owns actual integration and any merge with subsequent frame/GN changes.

## Checks performed

- Strict index/whitespace forward apply, reverse and reapply in scratch. All four files matched recorded bytes and Git blob IDs at every step.
- Real-header Objective-C++ syntax passed for both changed production sources and the native browser regression, using Chromium's generated target flags and fetched compiler. Includes macOS SDK, ARC and Chromium warning/plugin settings. First widget attempt selected its header-only target's flags; corrected to `browser_native_widget_mac_impl` and reran all three successfully.
- Scratch GN generation passed. Header dependency checks passed for `browser_frame_view_mac`, `browser_native_widget_mac_impl` and `browser_tests` with fresh candidate sources.
- Changed-line Chromium clang-format passed, version 23.0.0git, revision `20b6ec66967ac2a8f932863c1abf251e5b17a843`. GN format dry-run passed.

The added native browser test declares two cases, parity flag on/off. It uses actual controller endpoints and native hover locks, then checks existing `NSWindow` buttons, non-null actions, matching caption exclusions, layout invalidation, stable content bounds during peek/dismissal and reserved width after pinning. No fake button or Profile backend is introduced. **Zero automated native test cases executed.** Parent test filter:

```text
VisualParityFlag/MyarcCaptionHoverMacTest.NativeButtonsAndReservationFollowHoverLayout/*
```

Syntax and GN checks establish neither linking nor runtime behavior. Endpoint resets exercise source wiring, not measured animation timing.

## Remaining limits and parent acceptance

Visibility snaps as hover width becomes positive or returns to zero. Real AppKit controls do not yet translate behind the sidebar clip. They may appear before the expanding clip reaches their location, and remain at their normal position during dismissal. Exact slide, clipping, opacity and timing stay unresolved from 0008. This patch claims no full motion parity.

Ownership is limited to normal, in-process windows outside fullscreen. Remote Cocoa mirrors, popup/DevTools windows and fullscreen caption handling remain upstream-owned. Fullscreen entry/exit after hidden or peek states still needs native verification. Existing caption-bounds mirroring is retained for RTL and partial RTL; native placement and pointer behavior are untested there.

The existing animation controller remains responsible for reversal and reduced motion. Fresh native checks must cover edge peek, inside hold, dismissal, rapid reversal, toggle-to-pin and native caption clicks, with page bounds unchanged during temporary hover. Also verify flag-off behavior, reduced motion, focus, fullscreen transitions and RTL. Parent compilation/linking and fresh owned-window captures confirm the basic visibility path. Recorded page observations remained 1280 × 788 while peeking. The parent still must execute the two automated cases and verify native caption clicks and the remaining motion/ownership scenarios. Private profiles and reference media remain outside Git.
