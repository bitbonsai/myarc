# Native recovery anchor candidate

## Scope and cause, 2026-10-25

Bounded MA-007.c fix, based on myarc `2a706ad` and external Chromium `3e117efa80cec1c4e1e6fbfdf44edeb087a279a5` with the current optional shell stack. The agent handoff verified the build; the parent's later bounded native popup acceptance is recorded below. The workspace renderer stays off by default.

The parent freshly reproduced `AppMenuButton::GetAnchor` failing `CHECK(element)` while startup inserted its first tab. The disposable fixture forced `profile.exit_type=Crashed` and `exited_cleanly=false`, with vertical tabs and both shell flags enabled. This establishes a recovery-prompt startup crash, not actual session recovery or durability. No crash-loop/relaunch was attempted.

Shell `ToolbarView::Layout` hides the app-menu button. Startup's tab observer requests the recovery bubble before the browser window is shown. `View::IsDrawn` follows view ancestry, while `ElementTrackerViews` additionally requires widget visibility. Thus stock Chromium can anchor to its drawn button before window show, but the shell enters the fallback lookup. `ViewSubregionAnchor` already exists, yet its BrowserView host has no visible tracked element until widget show, so `BrowserElementsViews::GetElement` returns null. Moving anchor construction earlier cannot fix this visibility distinction.

## Narrow change

[0016-native-recovery-anchor.patch](../patches/0016-native-recovery-anchor.patch) changes five external files under `chrome/browser/ui/views/`:

- `frame/browser_view.cc` and `.h`: create a noninteractive, accessibility-ignored child only when the opt-in shell reparents its toolbar. Position it using the existing fallback rectangle on each layout. BrowserView owns it independently of the sidebar.
- `frame/app_menu_button.cc`: retain the drawn-button path, then use that shell child through supported Views anchoring. Stock tracked fallback and both CHECKs remain intact.
- `frame/BUILD.gn`: declare the BrowserView include dependency.
- `session_crashed_bubble_view_interactive_uitest.cc`: extend the existing recovery fixture with expanded/collapsed startup parameters, an unavailable tracked fallback, the actual recovery entry point, enabled Restore/close controls, hidden-sidebar anchoring and resize-bound checks.

All menu-anchor callers were traced, including extension/default anchors, idle/storage/search/network/restart/error/upgrade bubbles, survey/promo paths and test accessors. They continue using the shared anchor interface. Recovery implementation, restore/startup-page callbacks, consent handling and shutdown guards are unchanged. This adds no timer, recovery store or prompt suppression.

## Packaging and checks

Apply 0016 once after the installed optional stack, including current 0013/0014/0015 and complete replacement 0012. Preserve independent rounding. The external checkout already contains this delta.

- 0016 SHA-256: `d394c5e398bc73e820f84a08b758a620020331bf6ac685a8654b9b61bb851ae8`.
- Unchanged 0012 SHA-256: `8f8023789fdb66841128879766afdb89866c7e393453185336eba69745f66e53`.
- Captured 72 external source files before edits. Strict reverse restores every baseline hash; strict replay reproduces every installed hash. All 67 unrelated files remain identical, including hover/fade/caption/lifecycle/rounding and owned foundation sources. Canonical 0001 is unchanged.
- `autoninja -C out/MyarcM0 -j8 chrome`, with pinned depot_tools and updates disabled: exit 0, successful compilation and `libchrome_dll.dylib` link. Build ran 19:51:25–19:53:45 UTC.
- Actual interactive-test translation-unit compiler flags with `-fsyntax-only`: exit 0, empty final diagnostics. Initial missing `AddTabAt` declaration and incorrect sidebar accessor were corrected through existing public APIs.
- GN generation and header checks for `frame:app_menu_button_impl`, `frame:browser_view` and `views:interactive_ui_tests` pass. Changed-line formatting, document links, foundation freshness and whitespace pass.

Native cases executed: **zero**. Neither browser test graph was built. No app launch, desktop input, profile access or system-service kill occurred. Actual executable `comm` paths were checked before building/linking; resource-only notification mappings were left untouched. No build remains running. Private scratch retains baseline/result manifests, unrelated-file hashes, strict replay helper, compiler command, logs and process checks.

## Parent native acceptance, 2026-10-25

Two new marked disposable profiles repeated the forced crashed-exit preference fixture, following [fixture instructions](../../tests/fixtures/browser/README.md). Neither supplied a previous-session payload. Startup now showed the actual Restore pages? popup instead of failing `CHECK(element)`.

With the workspace renderer enabled only for acceptance, the popup remained visible after the native sidebar toggle and after resizing the main window from 1300 x 808 to 1100 x 740. Its native window bounds moved and stayed inside the owned main window. Fresh screenshots and accessibility output were inspected. Pressing the actual Restore button dismissed the popup without a crash and retained the synthetic page. This verifies button invocation and prompt lifetime, not restored session data.

After re-expanding the sidebar, the parent repeated the two-page/three-Space journey. Neutral Empty header, background loading, empty palette input/cancellation and blocked blank Cmd+R still passed. Different-page return restored the right URL/security controls and working native Reload. Same-page return preserved both live pages.

The second fresh profile omitted the workspace-renderer flag, retaining the legacy parity shell. Its recovery popup also appeared. The close button dismissed only that popup and kept its page open. The native Close label was ambiguous with the tab close button, so the automation stopped before input and was scoped to the verified recovery window before retrying.

[Bounded evidence](../evidence/recovery-anchor.json) records both native runs separately from compilation. Raw profiles, captures and logs remain private. Automated native browser cases executed remain zero. Initial collapsed startup, fullscreen and other popup types were not exercised. Actual session restoration, durability and broader lifecycle/privacy acceptance remain open.
