HTML: https://myarc.dev/blog/closer-to-arc/

[All posts](https://myarc.dev/blog/)

# Chasing Arc's details in the 2026.11.01 nightly

November 01, 2026

[Mauricio Wolff](https://github.com/bitbonsai)4 min read

[![myArc's sidebar with the creation menu open above the "+" button, listing New Space, a greyed-out New Split and a highlighted New Tab](https://myarc.dev/blog/closer-to-arc/creation-menu.6b5d41985107.webp)](https://myarc.dev/blog/closer-to-arc/creation-menu.6b5d41985107.webp)

The new creation menu. New Split stays greyed out until split view works.

The 2026.11.01 nightly is out, on the [download page](https://myarc.dev/download/). A nightly is a pre-alpha build I make from the main branch, so you can try myArc without compiling Chromium yourself. That's an 80,000-step build. You don't want to.

Most of this round came from recording Arc and myArc side by side. Any single frame looked close enough. In motion it didn't. Arc's sidebar and menus animate, and myArc's just snapped into place. I've been using Arc since March 2023, so I feel these things before I can name them.

## Going frame by frame

"Feels off" is hard to hand to a coding agent. A number is easy. So I split Arc's recordings into frames and started counting, like the nerd I am. Those timings became tasks.

Cmd\+S now hides and shows the sidebar at Arc's pace, and it peeks back in when you move the pointer to the window's edge. The "\+" button opens Arc's creation menu instead of a plain macOS one. The footer is laid out like Arc's, and its buttons show a tooltip with their name and shortcut.

## The bug that emptied Spaces

A sidebar that jumps only looks bad. This bug lost data. Quit myArc, open it again, and every tab you hadn't pinned was gone. The Spaces came back empty. If you're still on 2026.10.29, it does this to you too. Sorry about that.

myArc and Chromium disagreed about what to restore on launch, and myArc threw away the tabs it couldn't find. Now it restores your last session by default, and a tab that doesn't come back stays in the sidebar anyway. It loads when you click it.

## The Library fills in

The Library is Arc's panel for things that aren't tabs: Spaces, Downloads, Archived Tabs and a few more. Its button used to open a short menu. Now the whole sidebar slides into it.

Each Space gets a card with its pinned tabs. You can rename a Space from its card or drag cards to reorder your Spaces, and I keep doing it just to watch the other cards slide aside.

[![The Library's Spaces section with three Space cards, Everyday, Research and Reading, each listing synthetic pinned tabs, and the icon rail on the left](https://myarc.dev/blog/closer-to-arc/library-space-cards.ce64221778f8.webp)](https://myarc.dev/blog/closer-to-arc/library-space-cards.ce64221778f8.webp)

Space cards in the Library, filled with synthetic tabs.

Tabs you close land in Archived Tabs, and tabs you ignore for 12 hours get archived for you, like in Arc. Hover the Library button and your latest downloads fan out above it, ready to open or drag into another app.

[![Two crops of the sidebar footer: at rest with no downloads showing, then with the downloads stack showing sample.zip above the Library button](https://myarc.dev/blog/closer-to-arc/downloads-stack.cf200385ecfd.webp)](https://myarc.dev/blog/closer-to-arc/downloads-stack.cf200385ecfd.webp)

The footer at rest, then with the downloads stack open.

One place I stepped away from Arc on purpose is the Library's blue. Arc's looked too loud next to the rest of the sidebar, so myArc uses a softer one.

## How far from an alpha

Most of this build is about looks. Behavior is a different story. The [parity register](https://github.com/bitbonsai/myarc/blob/main/docs/PARITY.md) lists 12 behaviors a release can't ship without, and none of them is fully proven yet. Honest, if a bit painful to write.

Some big pieces don't exist at all. myArc can't import your Arc workspace yet, there's no ad blocking, split view isn't wired up and there's no onboarding in the browser. So I'm not putting a date on alpha. I wrote an [alpha checklist](https://github.com/bitbonsai/myarc/blob/main/docs/ALPHA.md) instead, where each item says what done looks like.

## Why the details

I could ship features faster if I stopped caring how a sidebar hides. But a two-frame fade is a big part of why Arc felt so good to use, and it's the part I miss most. I wrote about [the joy of sweating tiny details](https://mauriciowolff.com/blog/joy) before. This is the same itch, only now it's a whole browser.

Next I'm working through that checklist, one item at a time. If you try this nightly and something feels off, tell me in an [issue](https://github.com/bitbonsai/myarc/issues), even if all you can say is "the sidebar feels slow". That's exactly how this round started.

See you in the next nightly.
