HTML: https://myarc.dev/blog/closer-to-arc/

[All posts](https://myarc.dev/blog/)

# Chasing Arc's details in the 2026.11.01 nightly

November 01, 2026

[Mauricio Wolff](https://github.com/bitbonsai)6 min read

[![myArc's sidebar with the creation menu open above the "+" button, listing New Space, a greyed-out New Split and a highlighted New Tab](https://myarc.dev/blog/closer-to-arc/creation-menu.6b5d41985107.webp)](https://myarc.dev/blog/closer-to-arc/creation-menu.6b5d41985107.webp)

The new creation menu. New Split stays greyed out until split view works.

The 2026.11.01 nightly is on the [download page](https://myarc.dev/download/). A nightly is a pre-alpha build I make from the main branch, so people can try myArc without compiling Chromium. It has 58 commits since 2026.10.29, and most of them came out of the [screenshot loop](https://github.com/bitbonsai/myarc/blob/main/docs/WORKFLOW.md). I take one Arc screenshot, a coding agent matches it, and I compare the two before the next task.

Screenshots only get you so far. After the last nightly I recorded Arc and myArc doing the same things next to each other, and the result felt off. Any single frame looked close enough, but the recording was missing details and finesse. The blue felt wrong. Arc's motion wasn't there. The footer didn't look like Arc's.

## Going frame by frame

"Feels off" is hard to hand to an agent. A number is easy. So I split Arc's recordings into frames with ffmpeg and counted. Arc hides its sidebar in about a tenth of a second. The creation menu above the "\+" button fades in over two frames.

Those numbers turned into tasks. Cmd\+S now hides and shows the sidebar at Arc's pace, and the window buttons fade out and back with it. While the sidebar is hidden, moving the pointer to the window's left edge peeks it back in, filled with the Space's color where it used to be Chromium's grey.

The "\+" button used to open a plain macOS menu. Now it opens Arc's creation menu, with New Space, New Split and New Tab. The menu grows out of the button, follows the pointer and the arrow keys, and closes on Esc or a click outside it. While it's open, the "\+" turns into a cross.

The footer now packs the Space icons to the left with "\+" on the right, as Arc's does. Rest the pointer on a footer button for a second and a glass tooltip shows its name and shortcut.

Easels, Arc's drawing canvases, had crept into myArc's Library along the way. I didn't want them, so they're gone.

## The bug that emptied Spaces

A sidebar that jumps only looks bad. The worst thing I found lost data. Running the build live, I quit with Cmd\+Q, and every temporary tab, meaning any tab you haven't pinned, was gone. The Spaces came back, but empty. If you're still on 2026.10.29, it does this to you too.

The cause sat between myArc and Chromium. myArc remembers which tabs belong to which Space and leaves it to Chromium to bring the pages back. Chromium was set to start with a New Tab page, so after a relaunch there were no tabs to match, and myArc deleted the saved ones as stale.

myArc now restores your last session by default. One setting shouldn't be all that stands between you and your tabs, though, so a row now survives even when its tab doesn't come back. It stays in the sidebar with its saved title and icon. I call that a dormant row. It loads nothing until you click it, and then it reopens the full address, so a video row brings back that video.

None of this is saved when your profile has history turned off, and private and guest windows write nothing.

## The Library fills in

The Library is Arc's panel for things that aren't tabs: Media, Downloads, Spaces, Boosts and Archived Tabs. In the last nightly its button opened a short menu. Now the whole sidebar eases into the Library, with Arc's icon rail down the left. Esc or the arrow at the bottom takes you back.

The Spaces section has a card for each Space, listing its pinned tabs. Click a pinned tab on a card and myArc switches to that Space and opens it. The brush icon on a card renames the Space. As of today you can also reorder Spaces by dragging a card by its handle. The other cards slide aside, Esc drops the card back where it was, and the footer icons and Ctrl\+1 to Ctrl\+9 follow the new order. If you'd rather use the keyboard, the card's "..." menu has Move Left and Move Right.

[![The Library's Spaces section with three Space cards, Everyday, Research and Reading, each listing synthetic pinned tabs, and the icon rail on the left](https://myarc.dev/blog/closer-to-arc/library-space-cards.ce64221778f8.webp)](https://myarc.dev/blog/closer-to-arc/library-space-cards.ce64221778f8.webp)

Space cards in the Library, filled with synthetic tabs.

Close a temporary tab and it lands in Archived Tabs, grouped by day and searchable. It stays there for 30 days, or until the archive holds 500 entries. Auto Archive closes temporary tabs you haven't looked at in 12 hours, Arc's default. That clock only runs while myArc is open, so tabs you leave on Friday are still there on Monday. Settings \> Profiles offers 24 hours, 7 days or 30 days instead.

Hover the Library button and your four most recent finished downloads fan out above it. Click one to open the file, or drag it into another app. Chromium's own download bubble no longer pops up in the corner, and past downloads still show after a relaunch.

[![Two crops of the sidebar footer: at rest with no downloads showing, then with the downloads stack showing sample.zip above the Library button](https://myarc.dev/blog/closer-to-arc/downloads-stack.cf200385ecfd.webp)](https://myarc.dev/blog/closer-to-arc/downloads-stack.cf200385ecfd.webp)

The footer at rest, then with the downloads stack open.

Media and Boosts are still placeholders. Boosts are Arc's per-site style tweaks.

The blue is where I stepped away from Arc on purpose. The Library's accent, on its "\+" button and the selected rail icon, used Arc's own #212AFB, and next to the rest of the sidebar it looked too vivid. Today's build softens it to #6E7BFF. Matching Arc is still the rule. Here I'd rather myArc be a little quieter.

## How far from an alpha

Most of this build is visual parity. Behavior is tracked separately, in the [parity register](https://github.com/bitbonsai/myarc/blob/main/docs/PARITY.md). It lists 12 P0 behaviors, the ones a release can't ship without. Some now work in part. You can create, rename and switch Spaces, temporary tabs archive on Arc's schedule, and downloads have a home. None of them has the end-to-end evidence the register asks for yet, so it still counts zero demonstrated browser journeys.

Other P0 pieces don't exist. myArc can't import an Arc workspace. Ad and tracker blocking, which should be on by default, isn't there. Neither is browser control from the command line and over MCP, the protocol AI agents use to drive apps. Onboarding has to be animated to meet its [release requirement](https://github.com/bitbonsai/myarc/blob/main/docs/ONBOARDING.md), and so far there's a clickable web study with synthetic data and nothing in the native browser. Extension compatibility, crash durability and signed updates aren't verified either.

Smaller gaps I hit while testing this build:

- Split view isn't wired up, which is why New Split is greyed out.
- There are no folders, so the sidebar and the Space cards show pinned tabs only.
- Deleting a Space has no button yet.
- Dragging a tab row to reorder it failed in an automated test. I haven't confirmed that by hand.
- I haven't checked right-to-left languages.

The [release scorecard](https://github.com/bitbonsai/myarc/blob/main/docs/SCORECARD.md) has 16 gates, such as account independence and accessibility, and none of them is measured yet. So I'm not putting a percentage or a date on alpha. Instead I wrote an [alpha checklist](https://github.com/bitbonsai/myarc/blob/main/docs/ALPHA.md), the same way the recordings turned "feels off" into timings. Each item says what done looks like. Split view and Little Arc, Arc's small window for a quick lookup, are on it, next to Arc import, ad and tracker blocking on by default, native animated onboarding and a notarized build.
