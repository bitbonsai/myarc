# Native command palette

MA-007.b1 proves one opt-in interaction within the [native shell experiment](native-shell.md). Full Arc command-bar parity remains open.

## Reproduce

Apply [0004-native-command-palette.patch](../patches/0004-native-command-palette.patch) after [0002-native-shell-prototype.patch](../patches/0002-native-shell-prototype.patch), then rebuild Chromium. Use the marked disposable-profile instructions in the shell experiment, including `--myarc-visual-parity` and `--disable-features=InitialWebUI`.

## Implementation

Interactive command, action and tab-strip entrypoints call `NewTabFromUI()`. Programmatic `NewTab()` keeps its real-tab return contract. The centered native Views bubble holds transient input/selection state and calls Chromium classification/navigation and tab activation. Rust workspace rules are unchanged.

Input is limited to 4,096 UTF-16 code units before classification. Existing-tab matching scans at most 512 tabs in the current window and displays up to eight results. HTTP(S), Chromium internal URLs and `about:blank` are accepted. Complete omnibox providers, inline completion and browser commands remain unimplemented.

The browser layout requires the content parent to remain visible. The original palette makes it transparent and hides its native hosts while open, then restores them on close. The 0007 candidate below removes that workaround so the real page stays visible behind the palette.

## Observed on macOS 26.7

- Native compilation and browser-view GN header checks passed. Applying the palette delta after 0002 reproduced all seven payload files exactly.
- Cmd+T opened the centered native palette without creating a page. URL submission added exactly one real local fixture tab.
- Ctrl+T opened the same UI. Escape preserved page IDs and URLs. Selecting an existing fixture tab kept the count unchanged and restored page focus.
- Captured an actual native-window screenshot and a short open/close sequence. Originals remain private, outside the repository. The runtime build also contained independent rounding work; these checks do not accept corner parity.

## History and appearance follow-up

MA-007.b2 adds [0005-palette-history-and-appearance.patch](../patches/0005-palette-history-and-appearance.patch) after 0004. This five-file delta is independent of the separate rounding follow-up.

The palette now reads the current Profile\'s existing Chromium history service. Queries return at most 16 recent unique URLs; open tabs and history share eight visible rows. Matching open URLs take the tab action, and history entries navigate through the same classifier/helper. Query changes cancel stale callbacks, destruction cancels outstanding work, and private windows skip regular-history queries. This creates no separate history store or remote suggestion requests. Icons currently distinguish tabs from history with generic globe/clock glyphs; cached site favicons remain pending.

Input, placeholder and result colors use Chromium color roles. Selected rows keep a paired indigo/white treatment. AppKit material tint follows the effective browser colors, including a light Profile appearance on a dark macOS desktop. New empty windows use `about:blank` under the prototype flag, while programmatic `NewTab()` remains unchanged.

Verification on the native build:

- Local fixture visits populated history after their tabs closed. Typing a stored title and pressing Enter reopened its actual URL in exactly one new tab.
- Explicit light Profile appearance rendered readable input and selected/unselected results while macOS stayed dark. A private-window dark palette displayed no regular-history suggestions. These are bounded appearance/privacy observations, not a full isolation audit.
- Cmd+N created one `about:blank` target instead of Google\'s new-tab page.
- Compilation, frame/browser-command GN header checks and exact five-file patch reproduction passed. Fresh screenshots use invented local pages and remain outside the repository.

An initial HoverButton label paint hit Chromium\'s opaque-layer subpixel assertion; grayscale label rendering fixed it. Live theme switching while the palette stays open remains unverified.

## Current-tab editing candidate

[0007-current-tab-palette.patch](../patches/0007-current-tab-palette.patch) follows 0005. Cmd/Ctrl+L and a sidebar URL click route to the same centered palette with the current full URL selected. Enter with unchanged input reloads; changed input and history submission navigate the current tab. Interactive New Tab retains its separate new-tab mode. The real page stays visible behind the bubble, and the light blank canvas uses a near-white translucent fill.

The candidate also registers macOS Cmd/Ctrl+S aliases for the existing collapse command. Full hide/show and truthful Save Page labels require the separate [0008 sidebar patch](../patches/0008-native-sidebar-toggle.patch). The [0006 tile patch](../patches/0006-native-pinned-tiles.patch) retains native pinning and includes corrected split-grid sizing and shared split fill.

The combined native build succeeded before a final omnibox include-layer correction. Frame and tab-layout GN checks passed; the corrected omnibox source passes its GN check and standalone object compilation. Full relink and runtime checks remain pending. The six-file 0007 delta reproduces the external source exactly after 0002/0004/0005 and excludes independent layout rounding. A system app-data permission prompt interrupted clean native capture and the remaining interaction checks. No OS permission was approved by the agent. Rebuild the final source and verify the marked profile, PID and foreground window before resuming.

## Remaining

Current-tab editing, sidebar URL activation and full sidebar hide/show still need native acceptance. Complete autocomplete and the broader command catalog remain pending. Motion timing, interruption, reduced motion, full accessibility and Windows behavior are unverified.

A crash during development exposed the existing recovery bubble failing to find an omitted app-menu anchor. A fresh disposable profile was used for the final checks. Recovery and popup anchoring remain release blockers; use this prototype only with disposable profiles.
