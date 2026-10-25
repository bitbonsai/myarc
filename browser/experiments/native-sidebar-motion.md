# Native sidebar motion

MA-007.c-motion follows the populated native sidebar journey. The target is fixed sidebar geometry during hide/show, with actual native browsing retained.

## Reference evidence

Two user-supplied recordings show Arc at 1714 × 1080 and myArc at 1724 × 1080, both 30 fps. The Arc build version is unverified. Original clips and extracted frames remain private, outside the repository.

An Arc intermediate frame shows the sidebar translated left with favorite tile dimensions and spacing intact. The corresponding myArc interaction narrows the layout: three tiles become a column, and header controls reposition. Page content and theme differ between clips, so this comparison establishes sidebar geometry only. It does not establish an exact animation curve or duration.

## Hidden-state edge hover

The user also requires revealing a hidden sidebar by hovering the browser's left edge. Around 9.0 seconds the reference remains hidden with the pointer at that edge. Frames around 9.1–9.3 seconds show a rounded, outlined sidebar floating over the page; the page logo and heading keep their previous positions. A later toggle changes it to the persistent sidebar and resizes the page. Treat temporary hover reveal and persistent expansion as distinct layouts.

The sequence does not isolate pointer-exit dismissal without a click. Exact activation-zone width, delay and easing remain unmeasured. Reuse native hover locks and dismissal machinery, then verify pointer/focus/menu behavior in the running build.

## Implementation boundary

Keep the inner sidebar at its expanded layout width while hide/show changes the outer reserved width. Translate the inner controls together behind a clipped viewport. Genuine user resizing still relayouts the sidebar. Reuse Chromium's animation controller, including reversal and reduced motion. Preserve the zero-width hidden endpoint and previous expanded width.

The native candidate must support the existing tab body and the Rust-backed body. Caption controls, input hit testing and popup anchors must follow the same visible geometry. The pinned grid also needs a symmetric right inset; clipping the third tile is a separate regression from motion.

## Native build checkpoint

The 0009 frame and 0010 motion candidates compile and link in the native Chromium build. Frame/common-tab GN checks, 32 Rust tests and the native bridge/metadata assertions pass. A fresh resting-state capture shows three real favicon tiles with both gutters preserved, and the Profile follows the system dark theme.

Interactive capture stopped when the owned-window focus precondition failed, before sending toggle keys. That recording was rejected and deleted. The changed animation has not yet been accepted from a fresh sequence. AppKit caption-button motion remains unresolved.

## Acceptance still required

Compile and record the actual native app. Verify unchanged inner tile bounds throughout collapse/expand, rapid reversal and resized-width restoration. Inspect focus and pointer behavior, reduced motion and fullscreen. Hover-peek implementation and command-palette motion need their own native acceptance; reference hover overlay is now observed. Extracted reference frames and static geometry checks cannot close native acceptance.
