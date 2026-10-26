# Native Copy URL shortcut

MA-007.c copy URL, 2026-10-26. Cmd+Shift+C now copies the active page's full URL in myArc mode. Stock Chromium keeps its DevTools inspect command when the myArc flag is absent.

## Change

Chromium already exposed `IDC_COPY_URL`, including the blank-Space command gate and full-current-page clipboard behavior. macOS still reserved Cmd+Shift+C for DevTools inspect. The change replaces that hidden shortcut only under `--myarc-visual-parity`, registers it at high priority in BrowserView and keeps the static accelerator map consistent.

[Evidence](../evidence/native-copy-url-shortcut.json) records commit `52c9f57bc1634483ba7cac70cc718c76bf1d2940`. A marked disposable profile opened a loopback URL in the background. The targeted shortcut copied the exact URL, including query and fragment. The check saved and restored every pasteboard item afterward.

## Checks and limits

`chrome`, `browser_tests` and `unit_tests` built. Global shortcut tests passed, plus the focused default and myArc command-routing test. Native C++ browser tests compiled but were not executed. No clipboard notification, Windows/Linux mapping or settings control was added.
