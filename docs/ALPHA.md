# Alpha checklist

Updated: 2026-11-01. Statuses come from the [parity register](PARITY.md), [ranked gaps](GAPS.md), the [Arc clone backlog](../.plans/2026-10-26-arc-clone-backlog.md), the [development log](../log/DEVLOG.md) and `git log` up to `44edec7`.

Alpha means early users can use myArc as their daily browser on macOS Apple Silicon, with their Arc setup brought over and no data loss. It is not full parity. The [release scorecard](SCORECARD.md) lists the gates a full release still has to pass.

## Terms

- A **Space** is a named set of pinned tabs and temporary tabs. Several Spaces can share one **Profile**, which keeps cookies, history and extensions apart from other Profiles.
- A **pinned tab** is a tab saved in a Space. A **temporary tab** is any tab you have not pinned.
- The **Library** is the sidebar panel for Media, Downloads, Spaces, Boosts and Archived Tabs. Boosts are per-site style tweaks.
- **Split view** shows tabs side by side in one window, saved as one sidebar row.
- **Little Arc** is a small window with one page and an address bar, and no sidebar. It is for quick lookups and for links opened from other apps. In Arc it can be promoted into a Space.
- **P0** marks a behavior the parity register says a full release cannot ship without. A **gate** is one row of the scorecard.

## Status

- `done` means the behavior works in a nightly build, according to the log or a commit. It does not mean the scorecard evidence exists.
- `partial` means some of it works, or only the Rust rule exists.
- `missing` means nothing exists.
- `unknown` means the docs and log don't settle it.

## Required for alpha

### Spaces and tabs

- [x] **Create, switch, rename and reorder Spaces** (done). Done when the "+" menu creates and selects a Space, Ctrl+1 to Ctrl+9 switch, the heading renames and dragging Library cards reorders. Maps to [WS-02](PARITY.md).
- [x] **Pin with Cmd+D, and closing a pinned tab keeps the pin** (done). Done when Cmd+D pins and unpins the current tab and a closed pinned page leaves its row in place. Maps to [TAB-01](PARITY.md).

### Split view

Alpha covers two panels. [VIEW-01](PARITY.md) and the [roadmap](../ROADMAP.md) call it two-pane, and more panels are under nice to have.

- [ ] **Create a split** (missing). Done when New Split in the "+" menu is enabled and puts two tabs side by side, and a tab from another Profile is refused. Maps to [VIEW-01](PARITY.md) and [GAPS 10](GAPS.md).
- [ ] **One sidebar row, restored after a restart** (missing). Done when the pair shows as one row and the same layout returns after a restart. Maps to [VIEW-01](PARITY.md).
- [ ] **Focus and the address bar inside a split** (missing). Done when clicking a panel focuses it, Ctrl+Shift+1 and Ctrl+Shift+2 move focus, Cmd+L edits that panel's address and closing one panel leaves the other. Maps to [VIEW-01](PARITY.md).
- [ ] **Leave a split** (missing). Done when Separate All Tabs or Ctrl+Shift+minus gives each tab its own row and none is lost. Maps to [VIEW-01](PARITY.md).

### Little Arc

- [ ] **Open with Cmd+Opt+N** (missing). Done when Cmd+Opt+N, with another app in front, opens an empty small window with the address field focused. A system-wide shortcut in a Chromium app is not yet worked out. Maps to [CONV-01](PARITY.md).
- [ ] **Links from other apps** (missing). Done when, with myArc as the default browser, clicking a web link in another app opens it in a Little Arc window and a second click on the same link brings that window forward. Maps to [CONV-01](PARITY.md).
- [ ] **Promote into a Space** (missing). Done when the window's "Open in" button and Cmd+O move the page into the last-used Space and close the window, and Cmd+Opt+O picks another Space. Maps to [CONV-01](PARITY.md).
- [ ] **Turn it off** (missing). Done when a setting stops links from other apps from opening in Little Arc and they open as a new tab instead. Maps to [CONV-01](PARITY.md).

### Import and first run

- [ ] **Arc import** (missing). Done when picking an Arc export shows a preview of Spaces, folders, pins and omissions, one confirm commits it, re-importing creates no duplicates and the source file is unchanged. Maps to [IMP-01](PARITY.md), [GAPS 2](GAPS.md), [GAPS 5](GAPS.md) and [the migration plan](MIGRATION.md).
- [ ] **Import report** (missing). Done when every field the import skips or can't read is listed by name and count after the commit. Maps to [IMP-01](PARITY.md) and the Migration gate in [the scorecard](SCORECARD.md).
- [ ] **Animated onboarding in the native browser** (partial). Done when a recording of the real build shows the welcome, a reveal of the actual workspace, three live practice steps and a handoff, with a reduced-motion variant. Only a web study with synthetic data exists. Maps to [UX-01](PARITY.md), [GAPS 3](GAPS.md) and [ONBOARDING](ONBOARDING.md).
- [ ] **Skip, Back and restart in onboarding** (missing). Done when Skip, Back, closing the window and restarting mid-flow each reach a defined state with no duplicate import and no lost choice. Maps to [ONBOARDING](ONBOARDING.md) criterion 4.

### Browsing and privacy

- [ ] **Ad and tracker blocking, on by default** (missing). Done when a fresh Profile blocks a synthetic tracker request on its first page load and one site can be switched off. Maps to [PRV-01](PARITY.md).
- [x] **Profile isolation** (done). Kept required because shared cookies between Profiles would leak user data, and the [roadmap](../ROADMAP.md) M2 gate lists it. Done when a test with two Profiles shows no shared cookies, storage or history. `tools/check_profile_isolation.sh` swaps one window between two Profiles in both directions, before and after a relaunch, and finds none of the other Profile's cookie, localStorage key, IndexedDB database, service worker, history entry, command bar result or extension; after quitting, each Profile's Cookies and History files hold only its own. A test extension loaded into one Profile runs there and is missing from the other's extension list and pages. Maps to [WS-01](PARITY.md) and the Profile isolation gate in [the scorecard](SCORECARD.md).
- [x] **Downloads** (done). Done when finished downloads show in the footer stack and in the Library, and still show after a relaunch. Maps to [CORE-01](PARITY.md).

### Durability and distribution

- [x] **Tabs survive quitting** (done). Done when Cmd+Q and a relaunch bring back temporary tabs, or leave a dormant row, which loads nothing until clicked, with the saved title that reopens the full address. Maps to [TAB-02](PARITY.md) and [REL-01](PARITY.md).
- [ ] **Crash and kill durability** (partial). Done when a forced quit after an acknowledged pin, rename, move or Space delete leaves that change in place on relaunch, in a fault-injection suite. Acknowledgement and power-loss durability remain open. Maps to [REL-01](PARITY.md), [GAPS 4](GAPS.md) and the Workspace durability gate in [the scorecard](SCORECARD.md).
- [ ] **Notarized build** (missing). Notarized means Apple has scanned the build. Done when a clean Apple Silicon Mac opens the downloaded build without a Gatekeeper override. The nightly is signed with a local self-signed certificate and not notarized. Maps to [REL-01](PARITY.md) and the Secure distribution gate in [the scorecard](SCORECARD.md).

## Nice to have

### Spaces and tabs

- [ ] **Delete a Space** (partial). Done when a Space menu item asks for confirmation, closes that Space's open pages and removes it, and refuses to remove the last Space. The rule exists as `delete_space` and has no button. Maps to [WS-02](PARITY.md).
- [ ] **Reset and replace a pinned URL** (partial). Done when clicking a pinned row's icon returns it to the saved address and its context menu replaces the saved address with the current page. Only the core rules exist. Maps to [TAB-01](PARITY.md).
- [ ] **Move rows by dragging** (unknown). Done when dragging a tab onto the pinned area pins it and dragging a row reorders it, checked by hand and by an automated test. An automated test failed on row reorder and nobody has rechecked it by hand. Maps to [TAB-01](PARITY.md).
- [ ] **Folders in pinned sections** (missing). Done when a folder holds pinned tabs, opens, closes, renames and survives a restart, and its tabs show on the Space's Library card. The workspace model has no folder yet. Maps to [TAB-01](PARITY.md).
- [ ] **Archive and restore temporary tabs** (partial). Done when a closed or idle tab appears in Archived Tabs and Restore returns it to its original Space. Archiving, the 12-hour timer and Clear Archive work, and restore is unconfirmed. Maps to [TAB-02](PARITY.md).
- [ ] **Command bar basics** (partial). Done when Cmd+T and Cmd+L open the bar and it opens a typed address, runs a search and switches to an open tab, using only the active Profile. Maps to [CMD-01](PARITY.md).
- [ ] **Command bar actions** (missing). Done when typing "pin", "reset", "new space" or "archive" runs that action and "/" filters to actions. Maps to [CMD-01](PARITY.md).
- [ ] **Peek** (partial). Peek is a preview overlay for a link clicked from a pinned tab. Done when a cross-site link from a pinned tab opens in Peek and Cmd+O turns it into a tab. Only the Rust rule exists. Maps to [VIEW-01](PARITY.md).
- [ ] **Space theme and icon** (missing). Done when a Space can take its own icon and color, shown in the footer. Maps to [the backlog](../.plans/2026-10-26-arc-clone-backlog.md).

### Split view

- [ ] **Three or four panels** (missing). Done when a split holds up to four tabs and Ctrl+Shift+3 and Ctrl+Shift+4 move focus. Maps to [VIEW-01](PARITY.md).
- [ ] **More ways in** (missing). Done when dragging a tab onto the page, Option-clicking a link and expanding a Peek each start a split. Maps to [VIEW-01](PARITY.md).
- [ ] **Flip, mute and duplicate** (missing). Done when a split can switch between side by side and stacked, and its panels can be muted or duplicated. Maps to [the backlog](../.plans/2026-10-26-arc-clone-backlog.md).
- [ ] **Rename and pin a split** (missing). Done when the split's sidebar row can be renamed and pinned, and the pin survives a restart. Maps to [VIEW-01](PARITY.md).

### Little Arc

- [ ] **Archive and window controls** (missing). Done when an unpromoted window archives after 6 hours and the Dock menu shows and hides all of them. Maps to [CONV-01](PARITY.md).
- [ ] **More triggers** (missing). Done when Cmd+Opt-click on a link or tab opens Little Arc and Cmd+Shift+O files its tabs from the keyboard. Maps to [CONV-01](PARITY.md).

### Import and first run

- [ ] **Onboarding quality checks** (missing). Done when a screen-reader review, a frame-time trace on 1,000 synthetic tabs and a five-person task study are recorded. Maps to [ONBOARDING](ONBOARDING.md) criteria 3, 6 and 7.

### Browsing and privacy

- [ ] **Blocking updates and exception list** (missing). Done when filter updates, update failure and a list of per-site exceptions are tested. Maps to [PRV-01](PARITY.md).
- [ ] **Extensions** (partial). Done when one named content blocker or password manager installs from the store, works and survives a restart. A throwaway extension passed an on, off and restart smoke test. Maps to [EXT-01](PARITY.md) and the Extensions gate in [the scorecard](SCORECARD.md).
- [ ] **Private windows** (partial). Done when no title or address from a private window is in the workspace file after exit or a crash. Maps to [CORE-01](PARITY.md) and the Private browsing gate in [the scorecard](SCORECARD.md).
- [ ] **Permissions, history, find and DevTools** (unknown). Done when a native smoke test covers a permission prompt, history, find in page and opening DevTools. Maps to [CORE-01](PARITY.md).
- [ ] **No account needed** (unknown). Done when a fresh install with the network off reaches a usable workspace and a request audit shows no myArc service call. Maps to the Account independence gate in [the scorecard](SCORECARD.md).
- [ ] **No dead controls** (partial). Done when each visible control has an effect or is disabled with an explanation, including the Library's Media and Boosts placeholders. Maps to the Visible controls gate in [the scorecard](SCORECARD.md).

### Agent control

- [ ] **CLI and MCP control** (missing). MCP is the protocol AI agents use to drive apps. Done when both interfaces complete the same real page journey. Planned after alpha and still P0 for a full release. Maps to [AUTO-01](PARITY.md), [GAPS 9](GAPS.md) and [the control contract](CONTROL-PROTOCOL.md).

### Durability and distribution

- [ ] **Signed in-app updates** (missing). Done when an update is authenticated and a tampered or older build is refused. Alpha users can download a new build until this exists. Maps to [REL-01](PARITY.md).
- [ ] **Accessibility and speed budgets** (missing). Done when keyboard, screen-reader, command-bar and Space-switch checks are recorded on a reference Mac. Maps to the Accessibility and Responsiveness gates in [the scorecard](SCORECARD.md).
