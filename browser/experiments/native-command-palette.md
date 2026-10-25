# Native command palette

MA-007.b1 proves one opt-in interaction within the [native shell experiment](native-shell.md). Full Arc command-bar parity remains open.

## Reproduce

Apply [0004-native-command-palette.patch](../patches/0004-native-command-palette.patch) after [0002-native-shell-prototype.patch](../patches/0002-native-shell-prototype.patch), then rebuild Chromium. Use the marked disposable-profile instructions in the shell experiment, including `--myarc-visual-parity` and `--disable-features=InitialWebUI`.

## Implementation

Interactive command, action and tab-strip entrypoints call `NewTabFromUI()`. Programmatic `NewTab()` keeps its real-tab return contract. The centered native Views bubble holds transient input/selection state and calls Chromium classification/navigation and tab activation. Rust workspace rules are unchanged.

Input is limited to 4,096 UTF-16 code units before classification. Existing-tab matching scans at most 512 tabs in the current window and displays up to eight results. HTTP(S), Chromium internal URLs and `about:blank` are accepted. Complete omnibox providers, inline completion and browser commands remain unimplemented.

The browser layout requires the content parent to remain visible. The palette makes it transparent and hides its native hosts while open, then restores them on close.

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

## Remaining

Cmd/Ctrl+L current-tab editing and sidebar URL activation still use the older sidebar editor. Sidebar show/hide, complete autocomplete and the broader command catalog remain pending. Motion timing, interruption, reduced motion, full accessibility and Windows behavior are unverified.

A crash during development exposed the existing recovery bubble failing to find an omitted app-menu anchor. A fresh disposable profile was used for the final checks. Recovery and popup anchoring remain release blockers; use this prototype only with disposable profiles.
