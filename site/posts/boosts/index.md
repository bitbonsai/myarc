---
title: "Boosts, minus the kitchen sink"
summary: "myArc now ships Boosts: CSS-only, simpler than Arc's editor, and easier to start from anywhere. Easels are staying out, and Arc Boost import is next."
date: 2026-11-25
draft: false
---

Getting Boosts into myArc took more work than I thought it would. Wiring live style overrides cleanly into Chromium's renderer, keeping a floating native editor anchored inside the page card without fighting window focus, and chasing down a shutdown crash along the way all took a few rounds.

The good news is we ended up with something simpler than Arc's version, and it works well.

## Starting a Boost shouldn't be a treasure hunt

In Arc, opening Library → Boosts shows your existing Boosts, and no button to create a new one.

![Arc's Library Boosts panel showing existing Boosts with no create button, beside a post on mauriciowolff.com][arc-library]

*Arc's Library Boosts panel has no way to create a Boost from inside it.*

![myArc's Library Boosts panel with a prominent New Boost button and a Disable All Boosts toggle][myarc-library]

*myArc puts New Boost and a Profile-wide Disable All Boosts toggle right in Library → Boosts.*

To actually make a Boost in Arc, you have to close the Library panel, right-click the tab in the sidebar, and pick **Customize Page...** from the context menu.

![Arc's tab context menu highlighting Customize Page on mauriciowolff.com][arc-menu]

*To make a Boost in Arc, you close the Library, right-click the tab, and pick Customize Page.*

In myArc, you can start a Boost from **Library → Boosts**, **View → New Boost**, the URL bar, or the tab context menu.

## CSS only, on purpose

Under the hood, Arc lets a Boost inject arbitrary JavaScript like an unpacked Chrome extension. That's powerful, and also a lot of surface area for a feature most people use to fix an ugly font, tone down a bright background, or hide a noisy sidebar.

In myArc, Boosts are CSS only. You get a color and contrast pad (with Invert and Palette modes), a font grid with size and case controls, Zap for hiding elements on the page, and a Code tab for raw CSS. As you drag the color pad, text colors adjust automatically to stay readable. Everything passes through a Rust sanitizer before it touches the page, and there is a Profile-wide kill switch in **Library → Boosts** if you ever want to turn all of them off at once.

![The mauriciowolff.com blog post restyled with a deep blue Boost in Arc][arc-boosted]

*A post on mauriciowolff.com restyled with a blue tint Boost in Arc.*

![myArc's floating Boost editor anchored top-right over the same mauriciowolff.com post with automatic text contrast][myarc-editor]

*The same post in myArc with the floating Boost editor open; text colors update automatically as you pick a background tint.*

I tested it on my own [myArc posts at mauriciowolff.com/blog](https://mauriciowolff.com/blog). Changing the background tint, swapping the font, and zapping a section updates the page live as you drag, and sticks the next time you open the site.

## No Easels, yes to importing Arc Boosts

People keep asking about Easels too. We're not doing Easels. myArc is a browser, and there are plenty of better canvas and whiteboard tools already.

Importing your existing Boosts from Arc, though, is on the list. Arc stores them on macOS as unpacked extensions under `~/Library/Application Support/Arc/boosts/`, with a `manifest.json` naming the matched sites and a `styles.css` file for the styles. That gives us a clear path to pull your CSS Boosts straight into myArc (leaving any JavaScript out) so you don't have to rebuild them by hand.

## What's next

Build `2026.11.25.2` is out now with Boosts, the macOS menu bar parity pass, Library Media, and Archived Tabs filtering. Open **View → New Boost** on any site to try it, or run **myArc → Check for Updates…** to grab the build.

Next up, I'm wiring the Library button's hover fan-up stack to read `~/Downloads` directly on disk with QuickLook previews, the way Arc does, so recent screenshots and saved files show up even when they didn't come from a browser download.

[arc-library]: arc-library-boosts.jpeg
[myarc-library]: myarc-library-boosts.png
[arc-menu]: arc-customize-page.png
[arc-boosted]: arc-boosted-post.png
[myarc-editor]: myarc-boost-editor.png
