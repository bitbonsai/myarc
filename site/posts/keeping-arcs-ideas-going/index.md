---
title: "Keeping Arc's ideas going"
summary: "Arc shuts down on December 22, and its ideas are worth keeping. This round brought Peek, Split View and Little Arc to myArc, along with ad blocking and nightlies that open on a Mac without a security warning."
date: 2026-11-19
draft: false
---

Arc is a very good browser. Spaces, Peek, Little Arc and the sidebar of pinned tabs add up to a way of browsing I haven't found anywhere else. The Browser Company is [shutting Arc down](https://arc.net/) on December 22, 2026, and pointing people to Dia. After that day Arc stops working on Mac and Windows.

**myArc is my bet that the community can keep those ideas alive and keep making them better**, in a browser that doesn't ask for an account before you can use it.

The last post ended on the Arc import, because nobody should have to rebuild their Spaces by hand to try something else. It landed a few days later. This round covered what comes after. First the parts of Arc people reach for on their first morning somewhere new, then what anyone needs before trusting a browser from a stranger. The agents worked through it one task at a time, and I reviewed what came back.

## What the export leaves behind

Arc's export only works while Arc does, so it's worth making one before December 22 even if you're not ready to switch.

The file is a list of your Spaces and their links, and myArc turns it into Spaces with their folders and pinned tabs. The export is thin, though. It carries names and links and little else. Page titles, Space icons and colors stay in Arc, along with your archived tabs. It also doesn't mark which links were Favorites, the icon tiles at the top of Arc's sidebar, so those arrive as ordinary pinned tabs.

I'd rather hear that on day one than notice it a month later. After an import, myArc now shows what it added and what it left out or changed, with a count for each kind.

![The Import from Arc dialog over a dark myArc window. It reads "Import finished. Added 4 Spaces, 1 folder and 19 pinned tabs", then "Left out or changed" with "Favorites added as pinned tabs: 13", and a note that Arc's export has no page titles, Space icons, colors or archived tabs. The sidebar behind it shows a Reference folder of test pages](import-report.png)

*The report after importing a test export.*

Without titles, an imported tab shows a short address such as `docs.example.test/start` until you open it once and the page names itself.

## Peek

Peek is Arc's way of following a link without losing your place. Click a link in a pinned tab that goes to another site, and the page opens in a card while the one you were on blurs and dims behind it. Close the card and you're back, with no stray tab left in the sidebar.

![A myArc window with a pinned Reading list page blurred and dimmed behind a large rounded card showing a Release notes page from another site, with close, expand and split buttons beside the card's top right corner](peek-card.png)

*A link from a pinned tab, open in Peek.*

The card grows in when it opens and shrinks away when you close it. Cmd+O turns it into a regular tab, and Cmd+Shift+T brings back a card you shut too fast. If you'd rather links open as tabs, a switch in Settings turns Peek off.

## Two pages in one row

Split View puts two tabs side by side in one window. In the sidebar the pair is one row with both icons, and it comes back that way after a restart. New Split opens the command bar, myArc's search and action box, in the empty half. You type to pick the second page, as in Arc.

Peek got a third button that leads here. It puts the linked page in a split next to the pinned page, handy for reading release notes beside the docs they change.

![A split window with the Reading list page on the left and the Release notes page on the right, and a single sidebar row holding both tabs](peek-split.png)

*Release notes beside the reading list that linked to them.*

## Little Arc from anywhere

Little Arc is a small window with one page and an address bar, without the sidebar. Arc uses it for quick lookups and for links clicked in other apps, so a link from a chat doesn't land in whatever Space you had open. In myArc, Option-Cmd-N opens one from any app, and once myArc is your default browser, those links open there too. If the page turns out to matter, Cmd+O moves it into your Space as a tab.

![A small Little Arc window over the main myArc window, with an Everyday chip and an address field in its header, an "Open in Everyday ⌘O" button at the right and the command bar open inside it](little-arc.png)

*A fresh Little Arc, ready for a search.*

## The small habits

A lot of the round was the stuff you only notice when it's missing. Ctrl+Tab jumps back to the tab you used before this one, and each press goes further back while you hold Ctrl. Typing "/" in the command bar lists actions such as pinning a tab or switching Spaces, and Enter runs one. A pinned tab that wandered off shows a slash after its name, and clicking its icon takes it back to the page you pinned. You can delete a Space, change its theme or icon after creating it, and drag the Favorite tiles into a new order. The View menu has Arc's extras now, like clearing one site's cookies and reloading it.

## What a stranger needs

The rest is about trust.

Ad and tracker blocking is on by default. Each Profile in myArc keeps its own cookies and logins, and each gets its own copy of uBlock Origin Lite, a lighter edition of the uBlock Origin blocker. It blocks from a new Profile's very first page. If a site breaks, one switch in its info panel, behind the "i" in the address field, turns blocking off there.

![The site info panel open over a test page whose heading reads "tracker: blocked", with a "Block ads on this site" switch turned on and a "More blocker settings" link below it](blocker-switch.png)

*A test page whose tracker never loaded, and the switch that controls it.*

Then there's losing things. Older nightlies, the pre-alpha builds of myArc, saved a change a moment after showing it, so a force quit at the wrong time could drop a pin or a rename. Now each change is on disk before the sidebar shows it. A test kills the browser right after a pin or a rename and checks that it's still there. If the workspace file gets damaged anyway, myArc opens the copy saved before it.

The download needed work too. Opening a nightly used to mean getting past a macOS warning, because Apple had never checked the app. The nightlies are notarized now, which means Apple scans each build before it ships, so a downloaded myArc opens like any other app. That first open has only been tried on the Mac that builds it, so tell me if yours complains.

## Not there yet

This week a tester agent drove a nightly with a real mouse and keyboard, in a window of its own. It found two bugs. The Settings tabs ignored clicks, and New Split listed a page that should have stayed hidden. Both were fixed the same day. Plenty of what's above has only been checked by automated test runs, and the welcome tour for new users doesn't exist yet. The [alpha checklist](https://github.com/bitbonsai/myarc/blob/main/docs/ALPHA.md) has what's left.

Arc still does more than myArc. The gap got a lot smaller this round, and everything above is in the latest nightly on the [download page](https://myarc.dev/download/). The code is all [on GitHub](https://github.com/bitbonsai/myarc) if you'd like to help keep Arc's ideas going.
