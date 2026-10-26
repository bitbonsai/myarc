# Native sidebar rows, footer and review profile

MA-007.c.sidebar26, 2026-10-26. Bounded native UI acceptance passed. Complete Arc parity and browser release acceptance remain open.

## Boundary and correction

[0026](../patches/legacy/0026-native-sidebar-row-footer.patch), with the refreshed [owned payload](../patches/legacy/0001-workspace-foundation.patch), changes native presentation and Library dispatch. Rust placement rules, storage, session restoration and the default-off workspace switch remain unchanged.

A row owns its full-width rounded background. Its separate close button has a 32-DIP hit target, an inset 24-DIP rounded hover chip and a 20-DIP icon. The glyph appears on row hover or focus within. The activation button's own ink drop is disabled so its hover paint cannot end before the close target. Keyboard focus remains available; title bounds stay fixed through hover.

Profile Favorites remain top tiles. Space Pins now appear above the separator and New Tab, with Temporary rows below New Tab. Previously, a successful Space Pin stayed below New Tab and looked unchanged. The observed saved record was retained; this correction changes section order rather than converting a Space Pin to a Profile Favorite.

Library, active Space and Create Space use 32-DIP controls. The active Space has a neutral rounded-square background and globe; inactive Spaces use dots. The native Library menu invokes the owning browser's Downloads command, existing Space selection and Create Space dialog. This is a small working menu, not the complete Arc Library overview. Easels remain excluded.

## Native evidence

[Evidence record](../evidence/native-sidebar-row-footer.json). The lead checked actual installed source/component hashes before using the marked preview, with existing permissions and exact Profile/PID/window guards. No debugging endpoint, test extension or automation flag was added.

- Real pointer enter/exit and close-target hover showed the row and inset chip; pointer exit hid the glyph after focus left the row.
- Actual Tab reached the close button from its row. The close and footer hit targets measured 32 by 32 DIP through native accessibility bounds.
- Actual Library interaction opened Chromium's Downloads page. Closing only that newly created Temporary retained both saved records. Create Space opened its real dialog, which was cancelled without adding a Space.
- The existing Space Pin remained above New Tab. A separately requested Profile Favorite appeared as a top tile. The requested public address redirected to the same destination, so the two saved placements legitimately shared a favicon and URL.
- Audited native graceful quit, exact-process exit and new-PID alias relaunch reused the same marked profile and retained both saved IDs, owners and canonical URLs.
- A separate disposable app-only reduced-motion run reported a true renderer media query through its synthetic document title. Pointer hover and actual Tab focus passed. That app was closed; the original review records stayed unchanged and its window was reactivated. OS preferences were untouched.

Private captures, profiles and detailed logs remain outside Git. Guarded attempts stopped on a loading-label race; actions resumed only after observing the settled native title. No model edits or weaker ownership checks were used.

## Launcher and verification limits

`python3 tools/launch_native_preview.py` still creates a fresh disposable profile. `--review` keeps one marked review profile; an optional `native-preview-*` cache directory name can reuse an existing preview without moving its data. Existing preferences are preserved, later starts use native `--restore-last-session` without startup URLs, and a matching live process is rejected instead of forwarding to an older build. Neither mode compiles source changes.

Full Chrome build, real-context production/fixture syntax, nonzero GN owners/header checks, canonical16 freshness, strict indexed replay83 and 20 synthetic launcher tests passed. Four browser fixtures cover row bounds/focus, saved-section order, footer overflow and Library dispatch; their native suite execution remains **UNRUN**. The unchanged core retained the implementation delegate's 32-test Rust and bridge/lifecycle checks.

Motion timing, VoiceOver, IME and the full display/accessibility matrix remain unmeasured. This UI and clean-restart result does not repair the retained [recent-navigation crash loss](native-session-navigation-crash.md) or establish acknowledgement/power-loss durability. Keep important data in a normal browser.
