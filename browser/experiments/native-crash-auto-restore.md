# Silent restore after a crash

MA-007.c, 2026-11-03. Arc doesn't ask after a crash; it brings the last session back on relaunch. myArc used to show Chromium's "Restore pages?" bubble, and while it waited the sidebar read "Workspace unavailable … Waiting for native session binding." That wait comes from the recovery barrier in [native-recovery-lifecycle.md](native-recovery-lifecycle.md), so the sidebar looked broken until someone clicked Restore.

## Change

With `--myarc-workspace-sidebar`, the switch that also gates the adapter, the first crashed launch skips the bubble. `SessionCrashedBubble::ShowIfNotOffTheRecordProfile` still creates the recovery-choice observer, takes the same `ExitTypeService` crashed lock the bubble's Restore button takes, and calls `SessionRestore::RestoreSessionAfterCrash`. The restorer exists before the lock and the choice are released, so the barrier and restorer count keep the order Restore already had and stable item IDs survive.

The silent path passes only each window's active tab to Chromium's background tab loader. The other restored tabs stay unloaded, and `TabModel::LoadIfNeeded` loads each one when it's activated. A `MYARC_ACTIVE_TABS_ONLY` behavior bit carries this, so foreign-session restores and the stock bubble's Restore later in the same run still load every tab. The silent path starts the restore from the `CommitPendingWrite` reply, after the count is on disk, and skips it if the window closed while the write was pending.

The profile pref `myarc.crashed_launches` counts consecutive crashed launches. The second one in a row shows the stock bubble, so a session that crashes while restoring can't loop. `StartupBrowserCreatorImpl::Launch` clears the count when the previous run exited cleanly. Without the switch, the bubble, loading and prefs behave as in stock Chromium; the only change there is a registered pref that's never written.

## Checks

- `autoninja -C out/MyarcM0 -j8 chrome` passed under the build lock. `SessionCrashedBubbleMyarcViewTest` gains `FirstCrashedLaunchRestoresWithoutBubble`, and its two bubble cases now seed one earlier crashed launch. The fixture passed `-fsyntax-only` with its real compiler flags and wasn't run.
- On a fresh synthetic `/tmp` profile launched in the background, setup made two Spaces (Everyday, Reading), a Favorite and two Temporary tabs. A SIGKILL of the browser PID, then a relaunch without URLs or `--restore-last-session`, gave no bubble and a bound sidebar with the same item IDs, owners, placements and URLs. After a clean exit and relaunch the count was cleared. The next SIGKILL and relaunch restored silently again, and the loopback fixture server saw only the active tab's page, with both Temporary rows still unloaded. A second SIGKILL in a row brought back the stock bubble, fetched nothing and kept the workspace file unchanged.
- In the very first crashed relaunch, the server also saw one Temporary page, which DevTools listed as loaded. A likely cause is the setup launch's session recording that tab as active before the seeded Favorite opened; this hasn't been confirmed.

Captures, workspace snapshots, the server log and the journey log are under `/tmp/crashrestore/evidence/`, outside the repo. Multiwindow crash restore, a crash during the restore itself and the profile picker path weren't exercised.
