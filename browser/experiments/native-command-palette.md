# Native New Tab palette

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

## Remaining

Cmd/Ctrl+L current-tab editing and sidebar URL activation still use the older sidebar editor. Sidebar show/hide, complete autocomplete and the broader command catalog remain pending. Motion timing, interruption, reduced motion, full accessibility and Windows behavior are unverified.

A crash during development exposed the existing recovery bubble failing to find an omitted app-menu anchor. A fresh disposable profile was used for the final checks. Recovery and popup anchoring remain release blockers; use this prototype only with disposable profiles.
