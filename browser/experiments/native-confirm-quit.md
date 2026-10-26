# Native quit-warning default

MA-007.c confirm quit, 2026-10-26. myArc now matches the preserved Arc General settings screenshot: “Warn before quitting” defaults off. Chromium's existing app-menu toggle and persisted Local State preference remain the control.

## Change

`confirm_quit::RegisterLocalState` now selects the default from `--myarc-visual-parity`. Stock Chromium still defaults the preference on. myArc defaults it off. Explicit user values remain unchanged, so the existing menu action continues to toggle and persist the setting.

[Evidence](../evidence/native-confirm-quit.json) records commit `0f100db8975107fbe780029200a54bc5075cff98`. A fresh marked myArc profile quit from one Cmd+Q tap. With the existing preference enabled, one tap kept the browser open and showed the native “Hold ⌘Q to Quit” panel. Chromium's supported double-tap path completed the quit. A new-PID relaunch retained the enabled setting and warning behavior.

## Checks and limits

`chrome` and `unit_tests` built. The focused pref-default test passed in stock and myArc modes. The existing app-menu action was verified in source; native acceptance seeded the exact Local State pref it writes rather than automating a menu-bar click. No custom settings window was added. Native C++ browser and interactive suites remain unrun.
