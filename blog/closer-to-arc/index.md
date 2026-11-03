HTML: https://myarc.dev/blog/closer-to-arc/

[All posts](https://myarc.dev/blog/)

# Chasing Arc's details

November 01, 2026

[Mauricio Wolff](https://github.com/bitbonsai)4 min read

[![myArc's sidebar with the creation menu open above the "+" button, listing New Space, a greyed-out New Split and a highlighted New Tab](https://myarc.dev/blog/closer-to-arc/creation-menu.6b5d41985107.webp)](https://myarc.dev/blog/closer-to-arc/creation-menu.6b5d41985107.webp)

The new creation menu. New Split stays greyed out until split view works. 2026.11.01 nightly.

New nightlies keep landing on the [download page](https://myarc.dev/download/). A nightly is a pre-alpha build I make from the main branch, so you can try myArc without compiling Chromium yourself, which is an 80,000-step build you don't want to run.

Most of the recent work came from recording Arc and myArc side by side. Any single frame looked close enough, but in motion myArc's sidebar and menus snapped into place where Arc's animate. I've been using Arc since 2023, so I feel these things before I can name them.

## Turning a feeling into numbers

"Feels off" is hard to hand to a coding agent, but a number is easy, so I gave the recordings to agents and had them measure how long each of Arc's animations takes. Arc hides its sidebar in about a tenth of a second, and its creation menu fades in over two frames. Those timings became the tasks.

`Cmd+S` now hides and shows the sidebar at Arc's pace, and it peeks back in when you move the pointer to the window's edge. The "\+" button opens Arc's creation menu instead of a plain macOS one. The footer is laid out like Arc's, and its buttons show a tooltip with their name and shortcut.

## The bug that emptied Spaces

The worst bug in older nightlies lost data. If you quit myArc and opened it again, every tab you hadn't pinned was gone and your Spaces came back empty. If you're on one, please update. Sorry about that.

myArc and Chromium disagreed about what to restore on launch, and myArc threw away the tabs it couldn't find. Now it restores your last session by default, and a tab that doesn't come back stays in the sidebar anyway. It loads when you click it.

## The Library fills in

The Library is Arc's panel for things that aren't tabs: Spaces, Downloads, Archived Tabs and a few more. Its button used to open a short menu, and now the whole sidebar turns into the Library.

Each Space gets a card with its pinned tabs. You can rename a Space from its card or drag cards to reorder your Spaces, and I keep doing it just to watch the other cards slide aside.

[![The Library's Spaces section with three Space cards, Everyday, Research and Reading, each listing synthetic pinned tabs, and the icon rail on the left](https://myarc.dev/blog/closer-to-arc/library-space-cards.ce64221778f8.webp)](https://myarc.dev/blog/closer-to-arc/library-space-cards.ce64221778f8.webp)

Space cards in the Library, filled with synthetic tabs. 2026.11.01 nightly.

Tabs you close land in Archived Tabs, and tabs you ignore for 12 hours get archived for you, like in Arc. Hover the Library button and your latest downloads fan out above it, ready to open or drag into another app.

[![Two crops of the sidebar footer: at rest with no downloads showing, then with the downloads stack showing sample.zip above the Library button](https://myarc.dev/blog/closer-to-arc/downloads-stack.cf200385ecfd.webp)](https://myarc.dev/blog/closer-to-arc/downloads-stack.cf200385ecfd.webp)

The footer at rest, then with the downloads stack open. 2026.11.01 nightly.

One place I stepped away from Arc on purpose is the Library's blue. Arc's looked too loud next to the rest of the sidebar, so myArc uses a softer one.

## How far from an alpha

Most of this work is about how myArc looks. How it behaves is further behind. The [parity register](https://github.com/bitbonsai/myarc/blob/main/docs/PARITY.md) lists 12 behaviors a release can't ship without, and none of them is fully proven yet. Honest, if a bit painful to write.

Some big pieces don't exist at all. myArc can't import your Arc workspace yet, there's no ad blocking, split view isn't wired up and there's no onboarding in the browser. So instead of putting a date on alpha, I wrote an [alpha checklist](https://github.com/bitbonsai/myarc/blob/main/docs/ALPHA.md) where each item says what done looks like.

## Why the details

I could ship features faster if I stopped caring how a sidebar hides. But a two-frame fade is a big part of why Arc felt so good to use, and it's the part I miss most. I wrote about [the joy of sweating tiny details](https://mauriciowolff.com/blog/joy) before. This is the same itch, only now it's a whole browser.

Next I'm working through that checklist, one item at a time. If you try a nightly and something feels off, tell me in an [issue](https://github.com/bitbonsai/myarc/issues), even if all you can say is "the sidebar feels slow". That's the kind of note that started this round.

See you in the next nightly.
