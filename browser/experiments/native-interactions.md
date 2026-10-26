# Native sidebar and command-bar interaction contract

MA-007.b is the next interaction slice after the [native shell experiment](native-shell.md). This is a behavior contract, not implemented parity.

## Sources and limits

Reviewed Arc's public help center on 2026-10-25. Its search currently returns release notes for these queries rather than the former dedicated shortcut guides.

- [macOS 2023 release notes](https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes), June 8: Cmd+L followed by Enter refreshes the selected current page; pressing Cmd+T or Cmd+L again dismisses an already-open command bar. Other entries document command actions, tab suggestions and IME fixes.
- [Windows 2023–2026 release notes](https://resources.arc.net/hc/en-us/articles/22513842649623-Arc-for-Windows-2023-2026-Release-Notes), May 16, 2024: Ctrl+L and Alt+D open the command bar showing the current URL. May 23 describes Ctrl+T command actions. Another entry documents handling conflicts between website shortcuts and Control+S/Control+Shift+C, without establishing every default shortcut.
- [macOS 2022 release notes](https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes) document a resizable sidebar and hidden-sidebar behavior. They do not establish hover timing or the current shortcut defaults.
- Supplied reference images show the centered command bar with selected current URL, suggestions with switch-to-tab actions, and fully hidden sidebar. Images remain private and are not product assets.

Cmd+S on macOS / Ctrl+S on Windows to show or hide the sidebar is an explicit user requirement. The exact defaults on current Arc builds remain unverified by the retrieved pages. Historical release notes are not proof of current platform parity.

## Sidebar

A toggle hides the entire sidebar and lets content use the freed width. Toggling again restores the prior width. A permanently visible narrow icon rail does not satisfy this behavior.

The keyboard shortcut and sidebar button must call one command. Menu labels and accelerators must agree with the actual action. Retain Save Page through the File menu; its replacement accelerator requires an explicit decision rather than silently retaining a conflicting Cmd/Ctrl+S label.

Focus must remain usable after hiding a sidebar control. Sidebar visibility must not close tabs, change Space/Profile ownership or navigate the current page. Edge-hover reveal, timing and restart persistence are separate parity questions requiring observation.

## Command bar

The sidebar displays the current location as a launch point. Editing happens in a large centered native command bar, not in the narrow sidebar field.

- Cmd/Ctrl+L opens current-tab mode with the current URL selected. Navigation replaces the current tab. Accepting the unchanged selected URL refreshes it.
- Cmd/Ctrl+T opens new-tab/search/command mode. Cancelling must not leave an unwanted tab behind.
- Repeating the opening shortcut dismisses the bar, matching the documented macOS behavior. Windows equivalence is a myArc target until observed.
- Escape and outside dismissal cancel without navigation and restore the previous usable focus. IME composition must be resolved before treating Escape or Enter as browser commands.
- Arrow keys move through real suggestions. Enter activates the selected result. Switching to an already-open tab is distinct from navigating a URL or creating a tab.

Reuse Chromium's omnibox controller, URL/security handling and autocomplete services. Keep result activation on its existing browser command paths. An HTML imitation with hardcoded suggestions does not meet this contract.

## Acceptance evidence and stopping condition

Use a marked disposable profile with invented/local pages. Verify hide/show geometry, restoration of width, keyboard/menu/button agreement, current-tab navigation, new-tab navigation, switch-to-tab, cancellation, focus restoration and resizing. Record the sequence and check accessibility/reduced-motion behavior. Test browser and website shortcut conflicts explicitly.

Complete this slice only when the native build demonstrates those journeys. Do not infer Windows behavior from a macOS build or close full sidebar/command-bar parity from screenshots alone.
