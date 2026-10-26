# Arc clone backlog from public documentation

Date: 2026-10-26. Scope: Arc for macOS desktop. Easels are out of scope (AGENTS.md); Arc Max and sync are listed only to record they stay off or optional.

This is a checklist for dispatching one small task per line, not a parity claim. Status comes from reading the code and experiment notes, not from running the app.

## How this was built

- The live help center ([resources.arc.net](https://resources.arc.net/hc/en-us)) now serves only 22 articles, almost all release notes and account pages; the Spaces, Pinned Tabs, Peek and shortcuts articles are gone. The feature and FAQ articles below were read from Internet Archive copies of the same URLs (snapshots from January to September 2024), and the macOS 2021-2026 release notes from the live site's public article API on 2026-10-26. Each line links the canonical help-center URL; the archive timestamps are in [R21](../docs/RESEARCH.md).
- Wording is original, with a few short phrases quoted. No images or assets were copied.
- Arc's public docs name the section below the divider "Unpinned" (older release notes: "Explore"). They never call it "Today".
- Status values. `done`: shown working in a myArc native run or experiment note. `partial`: some of the behavior exists, or only stock Chromium provides it and no myArc check has exercised it. `missing`: nothing in the code or notes. Evidence names a repo file, experiment or DEVLOG entry.
- Behavior only the public docs imply (timings, exact animations, reset edge cases) is still unverified; see [BEHAVIOR-CONTRACTS.md](BEHAVIOR-CONTRACTS.md) BC-003.

## Counts

The ordered dispatch list, [Next 15](#next-15), is at the end.

Feature lines: 159 total, 23 done, 29 partial, 107 missing. Shortcut table: 44 rows, 6 done, 13 partial, 25 missing.

## Window & sidebar

- [ ] Cmd+S hides the whole sidebar and gives the page the freed width; pressing it again restores the previous width. | status: done | evidence: browser/experiments/native-interactions.md, browser/experiments/overnight-2026-10-25.md (native Cmd+S smoke) | source: https://resources.arc.net/hc/en-us/articles/25619487530519-How-Do-You-Hide-the-Sidebar
- [ ] Hiding the sidebar reserves zero width and leaves no icon rail. | status: done | evidence: browser/experiments/native-sidebar-hover.md | source: https://resources.arc.net/hc/en-us/articles/25619487530519-How-Do-You-Hide-the-Sidebar
- [ ] A hidden sidebar reappears as a floating overlay when the pointer rests on the window's left edge, and dismisses when the pointer leaves. | status: partial | evidence: browser/experiments/native-sidebar-hover.md (basic journey verified; hit width and delay are myArc prototype values) | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] A toast confirms a new tab opened while the sidebar is hidden, and clicking it reveals the tab. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Dragging the sidebar edge resizes it; double-clicking the edge resets the default width. | status: partial | evidence: log/DEVLOG.md 2026-10-26 (drag clamp 220 pt, default 270 pt; no edge double-click reset) | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] The sidebar header keeps back, forward and reload visible when the sidebar is narrow. | status: done | evidence: log/DEVLOG.md 2026-10-26 "Sidebar keeps its width and full header" | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Narrow sidebar mode keeps nav buttons and tiles legible down to Arc's narrowest width (Arc fits its header near 198 pt; myArc clamps at 220 pt). | status: partial | evidence: log/DEVLOG.md 2026-10-26 (still differs) | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] The sidebar tops with traffic lights, back/forward/reload and a URL field that opens the command bar rather than editing in place. | status: done | evidence: browser/experiments/native-interactions.md, log/DEVLOG.md 2026-10-26 | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] The URL field shows the current page as a pill; the sidebar loading indicator moves into the address field. | status: partial | evidence: browser/foundation/sidebar_view.cc, patch 0027 (URL pill hover); no loading indicator in the pill | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] Favorite tiles, URL field and New Tab row use translucent fills over the window material, matching Arc's light and dark values. | status: done | evidence: log/DEVLOG.md 2026-10-26 "Sidebar tiles ... lighter" | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Dragging empty sidebar space moves the whole window. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Footer holds Library on the left, Space dots/icons in the middle and a plus button on the right; the plus offers New Space (and in Arc New Folder, Split, Boost, Note). | status: partial | evidence: browser/foundation/sidebar_view.cc (Library, Space dots, Create menu with New Space and New Tab only); browser/experiments/native-sidebar-row-footer.md | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Space themes tint the sidebar and window chrome; the page area carries a faint tint of the Space color. | status: partial | evidence: browser/experiments/native-page-background.md (blank-page material only); no per-Space color | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] The toolbar can be toggled with Cmd+Shift+D; it shows pinned extensions at top right and can show the full URL. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25625617866263-How-Do-You-Move-Your-Pinned-Extensions-to-the-Top-Right
- [ ] The window background uses system material (vibrancy) with rounded content card. | status: done | evidence: browser/experiments/native-shell.md, patch 0003 rounded-content | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] Option-hold peeks at the sidebar while it is hidden (preference in Arc). | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] Full-screen: Esc prefers the website, and holding or double-pressing Esc exits full screen; a Shortcuts setting flips this. | status: missing | evidence: none (stock Chromium Esc behavior unchanged) | source: https://resources.arc.net/hc/en-us/articles/25619348451223-How-Do-I-Stop-the-Esc-Key-From-Exiting-Full-Screen-on-Arc-for-macOS
- [ ] A new window opened while another is full screen also opens full screen. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Quit warning is off by default (Cmd+Q quits at once) with a setting to enable it. | status: done | evidence: browser/experiments/native-confirm-quit.md | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Dragging a tab out of the sidebar creates a new window; the drag avoids accidental windows. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] A new tab in a Space appears in that Space across every window, while a Blank Window gives an independent set. | status: partial | evidence: log/DEVLOG.md 2026-10-26 Ctrl+Cmd+N (blank window opens, but still lists shared Profile tabs; per-window isolation open) | source: https://resources.arc.net/hc/en-us/articles/25590417429783-Why-Are-the-Same-Tabs-Appearing-Across-Multiple-Arc-Windows
- [ ] Dock menu offers New Incognito Window and Show/Hide All Little Arc Windows. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Sidebar tooltips show keyboard shortcuts on hover. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes

## Spaces & Profiles

- [ ] A Space has its own Pinned section, Unpinned section, theme and icon. | status: partial | evidence: crates/workspace/src/lib.rs (Space name + Profile; pins and temporary tabs per Space; no theme or icon) | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] Create a Space from the footer plus button. | status: done | evidence: browser/experiments/native-sidebar-row-footer.md (real Create Space dialog) | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] Switch Space by clicking its icon at the bottom of the sidebar. | status: done | evidence: browser/experiments/native-sidebar-row-footer.md, crates/workspace/src/lib.rs select_space | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] Switch Space with a two-finger horizontal swipe over the sidebar, with an icon transition animation. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] Switch Space with Control+1..9 and with Option-Cmd-Left/Right. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20595231349911-Keyboard-Shortcuts
- [ ] Switch Space by typing its name in the command bar. | status: missing | evidence: none (palette lists tabs only, browser/experiments/native-command-palette.md) | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] Mouse buttons 3 and 4 cycle Spaces. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Rename a Space inline from its heading. | status: done | evidence: browser/foundation/sidebar_view.cc (heading button), crates/workspace/src/lib.rs rename_space | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] Delete a Space, with its routes removed too. | status: missing | evidence: none (crates/workspace/src/lib.rs has no delete_space) | source: https://resources.arc.net/hc/en-us/articles/22932014625431-Air-Traffic-Control-Automate-Your-Link-Routing
- [ ] Reorder Spaces from the Space Manager (Cmd+T, Manage Spaces). | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Pick an icon or emoji per Space shown in the footer. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] Theme picker: gradient colors, intensity, graininess, light/dark/automatic, and a button to restore the default theme. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25625261733143-How-Do-I-Restore-the-Default-Theme-for-Arc-for-Desktop
- [ ] Light/dark/automatic mode applies to all Spaces, not only the current one. | status: partial | evidence: log/DEVLOG.md (system themes; no per-app toggle) | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] A Profile scopes logins, cookies, history, archive timing, favorites, extensions and settings, and one Profile can serve several Spaces. | status: partial | evidence: crates/workspace/src/lib.rs profile_id on Space, docs/PARITY.md WS-01; native two-profile isolation in log/DEVLOG.md 2026-10-25; Profile UI missing | source: https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing
- [ ] Assign a Profile to a Space from the Space edit menu; a new Profile starts empty. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing
- [ ] Delete a Profile only after removing it from every Space. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing
- [ ] Favorites are shared across all Spaces of one Profile. | status: done | evidence: crates/workspace/src/lib.rs Placement::Favorite { profile_id } | source: https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing
- [ ] Each Profile has its own download location and archive timing. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25614213458711-How-Do-You-Change-Your-Download-Location-in-Arc-for-Desktop
- [ ] Spaces and tabs sync across devices even when a Space uses a Profile. | status: missing | evidence: none (no sync service, per AGENTS.md no mandatory hosted service) | source: https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing
- [ ] Incognito windows are separate, show an indicator and do not expose any Space. | status: done | evidence: log/DEVLOG.md 2026-10-25 "Native private workspace lifecycle" | source: https://resources.arc.net/hc/en-us/articles/22354398337687-Open-Incognito-Window
- [ ] Moving a tab to a Space in another Profile is controlled by Profile boundaries. | status: partial | evidence: crates/workspace/src/lib.rs (Profile ownership check, BC-001/BC-003 in .plans/BEHAVIOR-CONTRACTS.md); no move UI | source: https://resources.arc.net/hc/en-us/articles/19227964556183-Profiles-Separate-Work-Personal-Browsing
- [ ] Share a Space as a permalink viewable in any browser; static snapshot, not live. | status: missing | evidence: none (needs hosted viewer; defer) | source: https://resources.arc.net/hc/en-us/articles/19228534606743-Share-Spaces-Folders-Splits-with-Anyone
- [ ] Route links to a Space with Air Traffic Control (see Settings). | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/22932014625431-Air-Traffic-Control-Automate-Your-Link-Routing

## Tabs lifecycle (favorites, pinned, unpinned, archive)

- [ ] Favorites show as a tile grid at the top of the sidebar, above the Space title, up to 12. | status: partial | evidence: browser/foundation/sidebar_view.cc, crates/workspace/src/lib.rs (tiles done; no 12-tile cap, MAX_ITEMS only) | source: https://resources.arc.net/hc/en-us/articles/19230755904151-Favorites-Top-Tabs-Across-Every-Space
- [ ] Drag a tab above the Space title to favorite it; right-click Move To > Favorites does the same. | status: partial | evidence: browser/foundation/sidebar_view.cc DropFavorite, context menu "Add to Profile Favorites"; drag from sidebar rows unverified | source: https://resources.arc.net/hc/en-us/articles/19230755904151-Favorites-Top-Tabs-Across-Every-Space
- [ ] Favoriting can be undone by dragging the tile back or Move To a section. | status: done | evidence: browser/foundation/sidebar_view.cc context menu "Remove from Profile Favorites" / "Move to Space Pins" | source: https://resources.arc.net/hc/en-us/articles/19230755904151-Favorites-Top-Tabs-Across-Every-Space
- [ ] Favorites can be empty, with no placeholder tiles. | status: partial | evidence: crates/workspace/src/lib.rs (Favorites optional in the model); no placeholder tiles seen in captures, not asserted | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Pinned tabs live above the divider, are per Space, can sit in folders, and never auto archive. | status: partial | evidence: crates/workspace/src/lib.rs Placement::Pin, browser/experiments/native-sidebar-row-footer.md; folders and archive missing | source: https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around
- [ ] Pin or unpin the current tab with Cmd+D. | status: missing | evidence: none (stock Cmd+D bookmarks) | source: https://resources.arc.net/hc/en-us/articles/20595231349911-Keyboard-Shortcuts
- [ ] Pin through the context menu or by dragging above the divider. | status: partial | evidence: browser/foundation/sidebar_view.cc context menu "Pin to Space"; drag-to-pin unverified | source: https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around
- [ ] Clicking a pinned tab icon resets it to the pinned URL; a slash after its name marks a page that differs from the pin. | status: missing | evidence: none (canonical_url stored and never updated by navigation per crates/workspace/src/lib.rs; reset gesture is open in BC-003) | source: https://resources.arc.net/hc/en-us/articles/25625148480279-Why-is-There-a-Slash-Next-to-My-Pinned-Tab-s-Name
- [ ] Reset Tab is also a command-bar action and can get a custom shortcut. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around
- [ ] Edit Pinned Page: Replace Pinned URL with Current, or Edit the URL by hand. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25541939922199-Why-Are-My-Pinned-Tabs-and-Favorites-Reverting-Back-to-Their-Original-URL-in-Arc-for-Desktop
- [ ] Links that would navigate away from a pinned tab's site open in Peek instead of replacing the pin. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around
- [ ] Option-drag or right-click Duplicate copies a tab in the sidebar. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around
- [ ] The caret next to the Space title collapses the Pinned section and expands it again. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around
- [ ] Closing a pinned tab's page keeps the pin; closing never deletes saved organization. | status: done | evidence: browser/experiments/native-sidebar-row-footer.md (close live page keeps saved record), crates/workspace/src/lib.rs live_tab_closed | source: https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around
- [ ] Unpinned tabs sit below the divider under a New Tab row; new tabs land here. | status: done | evidence: browser/experiments/native-sidebar-row-footer.md (Temporary below New Tab; Arc docs call this section Unpinned, older notes call it Explore; no "Today" name found in public docs) | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] Clear Unpinned (Cmd+Shift+K) closes the unpinned tabs with an animation, skipping tabs that play media; Cmd+Z undoes. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20595231349911-Keyboard-Shortcuts
- [ ] Auto Archive moves idle unpinned tabs to the Archive after 12 hours by default; viewing a tab resets the timer. | status: missing | evidence: none (docs/PARITY.md TAB-02 time model open) | source: https://resources.arc.net/hc/en-us/articles/19228855311127-Auto-Archive-Clean-as-you-go
- [ ] Tabs playing media are never archived or cleared. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Archive timing is configurable per Profile; Little Arc windows archive after 6 hours. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19228855311127-Auto-Archive-Clean-as-you-go
- [ ] View Archive from the command bar or Library, restore a tab with the un-archive icon, and Clear Archive after confirmation. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19228855311127-Auto-Archive-Clean-as-you-go
- [ ] Restoring from the Archive returns the tab to its original Space and follows Air Traffic Control routes. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Downloads archive on the same schedule as tabs. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Cmd+Shift+T reopens the last closed tab with its history, and also a closed Peek or window. | status: done | evidence: browser/experiments/native-late-window-restore.md (window), log/DEVLOG.md 2026-10-25 two-window restore; tab history restore unverified | source: https://resources.arc.net/hc/en-us/articles/20595231349911-Keyboard-Shortcuts
- [ ] Restore Data from the Help menu offers local sidebar backups (10 today, then daily, weekly, monthly). | status: partial | evidence: browser/experiments/native-recovery-lifecycle.md (pending recovery choice after crash; no backup ladder) | source: https://resources.arc.net/hc/en-us/articles/25625071960215-How-Do-I-Restore-My-Tabs-on-Arc-for-Desktop
- [ ] Folders group tabs, hold pinned tabs, never archive, peek the active tab out of a closed folder, and have icons. | status: missing | evidence: none (crates/workspace/src/lib.rs Placement has no folder) | source: https://resources.arc.net/hc/en-us/articles/19228419623447-Folders-Stash-Similar-Tabs-Together
- [ ] Hover a folder to search and open its tabs without expanding it. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19228419623447-Folders-Stash-Similar-Tabs-Together
- [ ] Multi-select tabs with Shift/Cmd-click, then close, move, copy URLs or put them in a new folder in one action. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Dropping on a closed folder does not expand it. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Live Folders: GitHub pull requests collect automatically into a Pull Requests folder. | status: missing | evidence: none (later) | source: https://resources.arc.net/hc/en-us/articles/22731612065815-Automatic-Github-Live-Folders
- [ ] Live Calendar: a pinned Google Calendar tile shows a countdown and a Join button. | status: missing | evidence: none (later) | source: https://resources.arc.net/hc/en-us/articles/24158102740631-Live-Calendars
- [ ] Hover previews on pinned/favorite tiles for Gmail, Calendar, GitHub, Notion, Figma, Linear and Confluence. | status: missing | evidence: none (later) | source: https://resources.arc.net/hc/en-us/articles/19335284431639-Previews-Glance-Top-Sites
- [ ] Tab hover cards, mute, rename and favicon badges for chat/music sites. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] Undo with Cmd+Z for moves, clears and customizations in the sidebar. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes

## Command Bar

- [ ] Cmd+T opens a large centered bar for new tab, search and actions; pressing Cmd+T or Cmd+L again dismisses it. | status: partial | evidence: browser/experiments/native-command-palette.md, browser/experiments/native-palette-intent.md | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Cmd+L opens the bar on the current URL, selected; Enter on the unchanged URL reloads. | status: done | evidence: browser/experiments/native-interactions.md, log/DEVLOG.md 2026-10-26 current-address submission | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Bar geometry, colors and row styling match Arc in light and dark. | status: partial | evidence: log/DEVLOG.md 2026-10-26 "Command palette matches Arc's geometry" (2.10% pixels differ at 5% fuzz) | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Results mix open tabs (with Switch to Tab), history, searches and URL suggestions, ranked by use. | status: partial | evidence: browser/experiments/native-command-palette.md (open tabs only, up to eight; omnibox providers not wired) | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] Tab rows show the tab's favicon. | status: done | evidence: log/DEVLOG.md 2026-10-26 palette favicons | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Arrow keys move selection, Enter activates, Escape and outside click cancel and restore focus. | status: done | evidence: browser/experiments/native-palette-hover-focus.md | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Actions live in the bar: Pin Tab, Unpin, Move To, New Folder, Split, New Boost, Reset Tab, View Archive, Open Library, Capture, Copy URLs. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Typing / filters to Arc actions only. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Space names appear as results so typing one focuses that Space. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19228064149143-Spaces-Distinct-Browsing-Areas
- [ ] Site Search: type a shortcut, press Tab, type a query; custom sites use a URL with %s. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20855018192791-Site-Search-Directly-Search-any-Website
- [ ] Hover a suggestion and press X (or Cmd+Option+Delete) to remove a history suggestion. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25500065173399-How-Do-You-Remove-Suggested-Command-Bar-URLs-in-Arc-for-Desktop
- [ ] Pasted or copied addresses from the bar add https automatically. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Switch to Tab in the bar says when the target is a Split tab. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] New windows always open the bar centered. | status: done | evidence: browser/experiments/native-command-palette.md | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Bar input handles IME composition and non-Latin text without dropping characters. | status: partial | evidence: browser/experiments/native-interactions.md (contract only; IME not exercised) | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Bar opens in Little Arc and inside Split View panels; Cmd+L in a split edits that panel. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19335393146775-Split-View-View-Multiple-Tabs-at-Once
- [ ] Dev actions in the bar: Open Task Manager, Record Trace, Capture Full Page. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25481392111895-How-To-Take-Full-Page-Screen-Captures-in-Arc

## Peek & Split View

- [ ] Clicking a link from a pinned or favorite tab to another site opens it in a Peek overlay. | status: partial | evidence: crates/workspace/src/peek.rs route_navigation (pure rule and tests; no overlay UI), BC-005; docs/PARITY.md VIEW-01 | source: https://resources.arc.net/hc/en-us/articles/19335302900887-Peek-Preview-Sites-From-Pinned-Tabs
- [ ] Peek expands into a real tab with Cmd+O, or into a split with the Split View button. | status: partial | evidence: crates/workspace/src/peek.rs resolve_peek_action OpenInTab (tab only; split not modeled), BC-005 | source: https://resources.arc.net/hc/en-us/articles/19335302900887-Peek-Preview-Sites-From-Pinned-Tabs
- [ ] Peek closes by clicking outside, the X button or Cmd+W; Cmd+Z/Cmd+Shift+T reopen it. | status: partial | evidence: crates/workspace/src/peek.rs resolve_peek_action Close returns to source (reopen not modeled), BC-005 | source: https://resources.arc.net/hc/en-us/articles/19335302900887-Peek-Preview-Sites-From-Pinned-Tabs
- [ ] A setting under Advanced turns Peek off so links open in new tabs. | status: partial | evidence: crates/workspace/src/peek.rs PeekSettings (rule only; no settings UI), BC-005 | source: https://resources.arc.net/hc/en-us/articles/19335302900887-Peek-Preview-Sites-From-Pinned-Tabs
- [ ] Split View combines tabs into one sidebar item that can be pinned, favorited and renamed. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19335393146775-Split-View-View-Multiple-Tabs-at-Once
- [ ] Enter split with Ctrl+Shift++, drag a tab to the content area, or Add Right/Left/Top/Bottom Split in the bar. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19335393146775-Split-View-View-Multiple-Tabs-at-Once
- [ ] Exit split with Ctrl+Shift+-, the panel X, Separate All Tabs, or Option-click the split tab. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19335393146775-Split-View-View-Multiple-Tabs-at-Once
- [ ] Switch split focus with Ctrl+Shift+1, 2...; panels swap by editing the URL. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20595231349911-Keyboard-Shortcuts
- [ ] Flip split orientation; mute or duplicate a split; share a split. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Option-click a link or tab opens it in a split. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes

## Little Arc

- [ ] Links opened from other apps open in a small Little Arc window by default. | status: missing | evidence: none (docs/PARITY.md CONV-01) | source: https://resources.arc.net/hc/en-us/articles/19235387524503-Little-Arc-Quick-Lookups-Instant-Triaging
- [ ] Cmd+Option+N opens an empty Little Arc for a quick search; Cmd+Option-click on a tab opens it there. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19235387524503-Little-Arc-Quick-Lookups-Instant-Triaging
- [ ] Open In moves the link to the last Space (Cmd+O) or a chosen Space (Cmd+Option+O); Cmd+Shift+O organizes Little Arc tabs by keyboard. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19235387524503-Little-Arc-Quick-Lookups-Instant-Triaging
- [ ] Little Arc windows archive after 6 hours; a dock menu item shows or hides all of them. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19235387524503-Little-Arc-Quick-Lookups-Instant-Triaging
- [ ] Re-clicking the same external link resurfaces the existing window and remembers the chosen Profile per domain. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Little Arc can be turned off so all links open in the sidebar. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19235387524503-Little-Arc-Quick-Lookups-Instant-Triaging

## Library, downloads, media

- [ ] Library opens from the footer icon with Media, Downloads, Easels & Notes, Spaces and Archived Tabs sections. | status: partial | evidence: browser/experiments/native-sidebar-row-footer.md (small menu: Downloads, Spaces, Create Space; Easels excluded) | source: https://resources.arc.net/hc/en-us/articles/19230634389911-Library-A-home-for-your-downloads-archived-tabs-easels-and-more
- [ ] Library remembers its last-used section and opens on Downloads right after a download. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Hovering the Library icon lists recent files that can be dragged out. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19230634389911-Library-A-home-for-your-downloads-archived-tabs-easels-and-more
- [ ] A Spaces overview in Library shows every tab in every Space and lets you drag tabs between them. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] The sidebar bottom shows in-progress downloads with size, time left and a cancel button. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] A downloads location setting per Profile, or Ask every time. | status: missing | evidence: none (stock chrome://settings) | source: https://resources.arc.net/hc/en-us/articles/25614213458711-How-Do-You-Change-Your-Download-Location-in-Arc-for-Desktop
- [ ] Picture in Picture Mini Player appears at a screen corner when leaving a playing video tab; resize from a corner, double-click returns to the tab. | status: missing | evidence: none (stock manual PiP only) | source: https://resources.arc.net/hc/en-us/articles/19234766331799-Mini-Player-Watch-or-Listen-as-you-Browse
- [ ] The Mini Player is disabled per site from the site controls, and playback keys control it. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25590734716823-How-Do-You-Disable-Mini-Player-on-Arc-for-Desktop
- [ ] A mini audio controller appears at the sidebar bottom when you leave a tab playing audio; a tray holds several players. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19234766331799-Mini-Player-Watch-or-Listen-as-you-Browse
- [ ] Music favorites animate their icon while audible. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Web notifications show on the desktop for allowed sites. | status: partial | evidence: stock Chromium notifications; not verified in myArc workspace mode | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] Capture and Capture Full Page save or share page images (Cmd+Shift+2 in older builds). | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25481392111895-How-To-Take-Full-Page-Screen-Captures-in-Arc
- [ ] Translation prompt in the URL bar and a Translate button at the sidebar top. | status: partial | evidence: stock Chromium translate; not verified | source: https://resources.arc.net/hc/en-us/articles/25626093607703-How-Does-Translation-Work-on-Arc-for-Desktop
- [ ] View History with Cmd+Y; Archive and History share one menu. | status: partial | evidence: stock Chromium history page | source: https://resources.arc.net/hc/en-us/articles/25591309234199-How-Do-I-View-My-Tab-History-in-Arc-for-Desktop

## Settings

- [ ] Settings opens with Cmd+, and has sections for General, Profiles, Links, Shortcuts, Previews, Little Arc, Advanced and Max. | status: missing | evidence: none (stock chrome://settings only) | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Shortcuts section lists every command, lets users remap with a modifier combo, search, and reset to default. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25622899860631-How-to-Set-Custom-Keyboard-Shortcuts-on-Arc-for-Desktop
- [ ] Links section holds Air Traffic Control: routes with URL contains/equals, a Default Space, and Most Recent Space or Little Arc. | status: missing | evidence: none (docs/PARITY.md CONV-01) | source: https://resources.arc.net/hc/en-us/articles/22932014625431-Air-Traffic-Control-Automate-Your-Link-Routing
- [ ] Previews can be switched on or off per site. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19335284431639-Previews-Glance-Top-Sites
- [ ] Advanced holds Peek toggle, haptics, Arc sounds off, and a button to open Chromium settings. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] A default notes app picker (Arc Notes, Google Docs, Notion, Word). | status: missing | evidence: none (out of scope; Notes phased out) | source: https://resources.arc.net/hc/en-us/articles/22557798824855-Customize-Default-Notes-App
- [ ] Warn before quitting toggle. | status: done | evidence: browser/experiments/native-confirm-quit.md (app-menu toggle) | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Default search engine and custom search engines. | status: partial | evidence: stock Chromium settings | source: https://resources.arc.net/hc/en-us/articles/20855018192791-Site-Search-Directly-Search-any-Website
- [ ] Make Arc the default browser prompt. | status: partial | evidence: stock Chromium | source: https://resources.arc.net/hc/en-us/articles/20498463803799-Arc-for-macOS-2021-Release-Notes
- [ ] Developer Mode: localhost tabs get a full URL bar, developer shortcuts and JSON Formatter, toggled per site. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20468488031511-Developer-Mode-Instant-Dev-Tools
- [ ] Site Control Center in the URL area: permissions, Mini Player, Previews, Developer Mode, Boosts and extensions. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20468488031511-Developer-Mode-Instant-Dev-Tools
- [ ] Import from Chrome, Brave, Edge: bookmarks, logins, history, extensions. | status: missing | evidence: none (MA-003 importer unimplemented, docs/PARITY.md IMP-01) | source: https://resources.arc.net/hc/en-us/articles/19335089616791-Import-Bookmarks-Logins-History-Extensions-from-Your-Previous-Browser
- [ ] Export pinned and favorite tabs for another browser. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19335089616791-Import-Bookmarks-Logins-History-Extensions-from-Your-Previous-Browser
- [ ] Sync via account. | status: missing | evidence: none (no mandatory account, AGENTS.md) | source: https://resources.arc.net/hc/en-us/articles/22369796990487-Arc-Sync

## Extensions & site controls

- [ ] Add extensions from the command bar or the Extensions menu; Cmd+E cycles pinned extensions. | status: partial | evidence: log/DEVLOG.md 2026-10-24 MV3 on/off/restart smoke; no Arc entry points | source: https://resources.arc.net/hc/en-us/articles/19434259167767-Extensions-in-Arc-How-to-Import-Add-Open
- [ ] Pin an extension so it appears in the URL area on hover; Cmd+Shift+D shows the toolbar with pinned extensions top right. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25625617866263-How-Do-You-Move-Your-Pinned-Extensions-to-the-Top-Right
- [ ] Right-click an extension to remove it or open its settings; Allow in Incognito. | status: partial | evidence: stock chrome://extensions | source: https://resources.arc.net/hc/en-us/articles/19434259167767-Extensions-in-Arc-How-to-Import-Add-Open
- [ ] Built-in ad and tracker blocker on by default. | status: missing | evidence: none (docs/PARITY.md PRV-01) | source: https://resources.arc.net/hc/en-us/articles/20498293324823-Arc-for-macOS-2024-2026-Release-Notes
- [ ] Progressive web apps are unsupported in Arc; Favorites stand in. | status: done | evidence: n/a (no PWA install either) | source: https://resources.arc.net/hc/en-us/articles/25678978728983-Does-Arc-for-Desktop-Support-Progressive-Web-Apps

## Boosts

- [ ] Create a Boost from the plus menu, command bar (New Boost) or the Site Control paintbrush; it applies to one domain. | status: missing | evidence: none (docs/BOOSTS.md phases B1-B4 planned, nothing built) | source: https://resources.arc.net/hc/en-us/articles/19212718608151-Boosts-Customize-Any-Website
- [ ] Editor controls: color wheel, invert lightness, contrast/brightness/saturation, font, size 90-150%, case. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19212718608151-Boosts-Customize-Any-Website
- [ ] Zap removes page elements; a slash icon restores zapped areas. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19212718608151-Boosts-Customize-Any-Website
- [ ] Code editors for CSS and JavaScript; boosts with JavaScript cannot be shared. | status: missing | evidence: none (JS stays disabled by default per AGENTS.md) | source: https://resources.arc.net/hc/en-us/articles/19212718608151-Boosts-Customize-Any-Website
- [ ] Rename, reset, disable, delete (from Library > Boosts) and global disable. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/19212718608151-Boosts-Customize-Any-Website
- [ ] Share and install Boosts by link; edits after sharing need a new link. | status: missing | evidence: none (needs hosted gallery; defer) | source: https://resources.arc.net/hc/en-us/articles/19212718608151-Boosts-Customize-Any-Website
- [ ] Boosts can read Space theme colors. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes

## Keyboard shortcut behavior

- [ ] When a website and the browser claim the same shortcut, the website wins first and a repeat press runs the browser's. | status: missing | evidence: none (contract in browser/experiments/native-interactions.md asks for explicit conflict tests; not built) | source: https://resources.arc.net/hc/en-us/articles/20498377604887-Arc-for-macOS-2023-Release-Notes
- [ ] Every shortcut can be remapped in Settings > Shortcuts, including Reset Tab, Duplicate Tab and Capture Full Page, with per-shortcut reset. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/25622899860631-How-to-Set-Custom-Keyboard-Shortcuts-on-Arc-for-Desktop
- [ ] Cmd+9 can be set to jump to the ninth tab or the last tab, and Go to Sidebar Item can leave Favorites out of the numbering. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Cmd+W closes every selected tab when several are multi-selected. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Cmd+Shift+C copies all selected tab URLs when several are selected. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes
- [ ] Menu labels and accelerators agree with the action they run (Save Page keeps a File-menu entry without Cmd+S). | status: done | evidence: browser/experiments/native-interactions.md, patch accelerators_cocoa.mm | source: https://resources.arc.net/hc/en-us/articles/25619487530519-How-Do-You-Hide-the-Sidebar
- [ ] Tooltips on sidebar controls name their shortcut. | status: missing | evidence: none | source: https://resources.arc.net/hc/en-us/articles/20498417809815-Arc-for-macOS-2022-Release-Notes

### macOS shortcut table

From the public shortcuts article ([source](https://resources.arc.net/hc/en-us/articles/20595231349911-Keyboard-Shortcuts)) plus shortcuts named in release notes and FAQs. "stock" means stock Chromium provides it and no myArc check exists. Windows keys are in the source article and are not tracked here.

| Shortcut (macOS) | Action | myArc status | Evidence |
| --- | --- | --- | --- |
| Cmd+T | New tab / command bar | partial | browser/experiments/native-command-palette.md |
| Cmd+N | New window | partial | stock; two-window evidence in log/DEVLOG.md 2026-10-25 |
| Cmd+Shift+N | New incognito window | done | log/DEVLOG.md 2026-10-25 private workspace |
| Cmd+W | Close current tab or window | partial | closes live page, keeps saved record; stock for windows |
| Cmd+Option+N | Open Little Arc | missing | none |
| Ctrl+Cmd+N | Blank window (from Arc Shortcuts settings; not in public docs) | done | browser/experiments/native-blank-window-shortcut.md |
| Cmd+Shift+T | Reopen last closed tab | done | browser/experiments/native-late-window-restore.md |
| Cmd+D | Pin / unpin current tab | missing | none (stock bookmarks) |
| Cmd+Shift+C | Copy current tab URL | done | browser/experiments/native-copy-url-shortcut.md |
| Cmd+Option+Shift+C | Copy current tab URL as Markdown | missing | none |
| Cmd+L | Change current tab URL | done | browser/experiments/native-interactions.md |
| Cmd+S | Show / hide sidebar | done | browser/experiments/native-interactions.md |
| Cmd+Shift+K | Clear unpinned tabs | missing | none |
| Cmd+1 ... Cmd+8 | Go to sidebar item N (Cmd+9 last or ninth; option to leave Favorites out) | partial | stock tab-index shortcut; sidebar order not verified |
| Ctrl+1 ... Ctrl+9 | Focus Space N | missing | none |
| Ctrl+Tab | Toggle between five most recent tabs | missing | none (stock cycles tab order) |
| Option+Cmd+Up / Down | Previous / next tab in sidebar order | missing | none |
| Option+Cmd+Left / Right | Previous / next Space | missing | none (stock binds these to tab switching) |
| Cmd+[ or Cmd+Left | Back (public table lists back and forward with swapped arrow labels; standard directions assumed) | partial | stock |
| Cmd+] or Cmd+Right | Forward | partial | stock |
| Ctrl+Shift++ | Add Split View | missing | none |
| Ctrl+Shift+- | Close Split View | missing | none |
| Ctrl+Shift+1, 2 ... | Switch Split View focus | missing | none |
| Cmd+Y | View History | partial | stock history page |
| Cmd++ | Zoom in | partial | stock |
| Cmd+- | Zoom out | partial | stock |
| Cmd+0 | Reset zoom | partial | stock |
| Cmd+R | Reload | partial | stock |
| Cmd+F | Find in page | partial | stock |
| Cmd+, | Open Settings | partial | stock |
| Cmd+O | Expand Peek to tab / move Little Arc link to last Space | missing | none |
| Cmd+Option+O | Move Little Arc link to another Space | missing | none |
| Cmd+Shift+O | Organize Little Arc tabs by keyboard | missing | none |
| Cmd+E (hold, tap E) | Cycle pinned extensions | missing | none |
| Cmd+Shift+D | Show / hide toolbar | missing | none |
| Cmd+Z | Undo sidebar action; reopen closed Peek | missing | none |
| Cmd+Option+Delete | Delete selected command-bar suggestion | missing | none |
| Cmd+Shift+2 | Capture (2022 builds) | missing | none |
| Cmd+click / Shift+click | Multi-select sidebar tabs | missing | none |
| Cmd+Option+click | Open tab or link in Little Arc | missing | none |
| Option+click link | Open link in Split View | missing | none |
| Option+drag tab | Duplicate tab in sidebar | missing | none |
| Option (hold) | Peek hidden sidebar (preference) | missing | none |
| Esc (full screen) | Prefer website; hold or double-press to exit full screen | missing | none |

## Next 15

Ordered for a daily Arc user. Each is one small task (1-3 files). "Rust" means [crates/workspace/src/lib.rs](../crates/workspace/src/lib.rs). Chromium paths are in the `myarc` branch checkout; export with `tools/export_patches.py`.

1. Cmd+D pins or unpins the current tab. Code: `chrome/browser/ui/cocoa/accelerators_cocoa.mm` (replace the bookmark accelerator under `--myarc-visual-parity`), `chrome/browser/ui/browser_command_controller.cc`, adapter `PinItem`/`UnsaveItem` in `browser/foundation/browser_workspace_adapter.cc`.
2. Click a pinned tab's icon to reset it to its saved URL, and show a slash after the name while the page differs. Code: `browser/foundation/sidebar_view.cc`, adapter activation of the canonical URL. Settles the reset gesture in BC-003.
3. "Replace Pinned URL with Current" and "Edit..." in the pinned item's context menu. Code: `sidebar_view.cc` menu, one new explicit `set_canonical_url` in Rust (core rule: navigation never changes it implicitly).
4. Collapse and expand the Pinned section with a caret beside the Space title. Code: `sidebar_view.cc` (in-memory first, persist later).
5. Ctrl+1..9 focuses Space N; Option+Cmd+Left/Right moves between Spaces (and drop the stock tab-switch meaning in myArc mode). Code: `accelerators_cocoa.mm`, `browser_command_controller.cc`, adapter `SelectSpace`.
6. Cmd+1..8 jumps to sidebar item N in sidebar order, Cmd+9 to the last, and Option+Cmd+Up/Down steps through tabs in that order. Code: `browser_command_controller.cc`, `accelerators_cocoa.mm`, Rust `sidebar_snapshot` order.
7. Cmd+Shift+K clears unpinned (temporary) tabs, skipping any that play audio. Code: `browser_command_controller.cc`, adapter close of temporary items, `accelerators_cocoa.mm`.
8. Drag a tab onto the Pinned or Favorites area to pin it, and reorder rows by drag. Code: `sidebar_view.cc` drag handling, Rust `set_placement` and `move_item_before` (already exist).
9. Delete a Space with a confirmation, keeping at least one Space. Code: Rust `delete_space` rule with tests (what happens to its items must be decided and written in BC-001), `sidebar_view.cc` Space menu.
10. Auto Archive core rule: a pure Rust function that lists idle temporary items older than a cutoff, excluding pinned, favorite, active and media-playing items. Code: Rust only, with tests. Defines the time model open in PARITY.md TAB-02.
11. View Archive in the command bar and Library: list archived items and restore one to its original Space. Code: palette in `chrome/browser/ui/views/frame/browser_view.cc`, adapter, depends on item 10.
12. Peek decision rule: a pure Rust function deciding whether a link click from a pinned or favorite tab stays, opens as a new temporary tab or opens in Peek (cross-site only). Code: Rust only, with tests; the overlay UI follows as separate tasks.
13. Ctrl+Tab switcher over the five most recent tabs. Code: `browser_command_controller.cc`, a small MRU list in the adapter, `accelerators_cocoa.mm`.
14. Command-bar actions: type "pin", "unpin", "reset", "new space" and "archive" to run them, plus the `/` action filter. Code: palette in `browser_view.cc`, adapter commands; reuses items 1, 2 and 9.
15. Two-finger horizontal swipe over the sidebar switches Space. Code: `sidebar_view.cc` scroll-event handling, adapter `SelectSpace`.

Next after these: folders (Rust placement change), Split View, Site Search, Little Arc, download progress in the sidebar footer, Mini Player, Boosts B1 ([BOOSTS.md](../docs/BOOSTS.md)).
