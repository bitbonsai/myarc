# Native Blank Window shortcut

MA-007.c blank window, 2026-10-26. Ctrl+Cmd+N now invokes Chromium's existing new-window command in myArc mode, matching the preserved Arc Shortcuts screenshot. Cmd+N remains unchanged.

## Change

The shortcut uses macOS delayed command dispatch. This avoids replacing the primary Cmd+N accelerator and stays separate from concurrent BrowserView work. Stock Chromium leaves Ctrl+Cmd+N unbound. myArc maps it to `IDC_NEW_WINDOW`.

[Evidence](../evidence/native-blank-window-shortcut.json) records commit `5c5c1f76e7ddf2bdc323fbb27e228e7a2c306f98`. A marked disposable profile launched one loopback page in the background. ScreenCaptureKit reported one owned window before the shortcut and two afterward. The original title remained `myArc foundation check`; the new title was `about:blank`. Fresh capture showed blank content in the new window.

## Checks and limits

`chrome` and `unit_tests` built. Focused stock and myArc routing tests passed. Native C++ browser tests remain unrun. The new window still lists the original page through myArc's shared Profile projection, so this establishes blank content and window creation, not complete Arc per-window tab isolation or restart semantics.
