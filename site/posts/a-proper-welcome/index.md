---
title: "A proper welcome"
summary: "myArc now says hello before your first tab. The screen dims, a soft blob of colour grows into a window, and a short tour has you try the basics on the real browser."
date: 2026-11-20
draft: false
---

Arc made its first minute feel like an event. Before you opened a single tab, it had already said hello, and that set the tone for everything after. I wanted myArc to have that too.

![The myArc welcome, from a dimmed desktop to the browser](welcome.mp4)

*From a dimmed desktop to "myArc, forever yours." in a few seconds.*

## The arrival

The first time you open myArc, your desktop dims and a blob of colour shows up in the middle, in the same blue, violet, pink and coral as the logo. It grows into the window, and the colours keep drifting slowly behind it. The 3D "A" plays once, then moves up to make room for "myArc, forever yours."

Below that, myArc asks one thing: are you bringing anything with you? Import from Arc… turns an Arc export into Spaces, folders and pinned tabs, and Start fresh gives you an empty sidebar. Either way, the window opens into the browser and the sidebar fills in row by row.

The 3D "A" wasn't in the plan. Once the welcome got this big, a flat logo looked out of place, so the logo got a version that moves, and more polish came with it. The browser chrome is glassy now, so you see your desktop blurred behind the sidebar, and with Reduce Transparency on it stays solid. The default colours got darker and warmer, closer to Arc's, and the selected tile in the sidebar is a soft highlight now instead of a bright grey block.

## A sound of its own

The arrival has a short ambient sound, with one accent when the blob becomes the window and another when the headline comes in. A small script in the repo generates it from a fixed seed, so it sounds the same every time and anyone can tweak it. You only hear it if your Mac's interface sounds are on, and it fades out as soon as you click a button.

## Learning by doing

After the welcome, a small card points at a real control and asks you to try it. Press ⌘T and open a site. Press ⌘D to pin that tab, so it stays in the sidebar after you close it. Press ⌃2 to jump to another Space, or create your first one if you only have one. Every card has Back and Skip tour, and none of them takes focus, so the shortcut you're practising goes to the browser the same way it will tomorrow.

The last card says "You're ready" and offers to make myArc your default browser. If you want to take the tour again later, it's in Settings, under Advanced.

## The details

With Reduce Motion on, there's no dim, no blob and no drifting colours. The welcome fades in with the headline already in place, and it stays quiet.

The window keeps a 16:10 shape and stops growing at a comfortable width, so on an ultrawide it still looks like a window and not a banner.

If you miss the arrival, press ⌃⌥⌘A to watch it again, sound and all, without restarting the tour. Esc or a click closes it. Consider it a small secret between you and the shortcuts page.

## What's next

In the next nightly, the tour ends on a quick start page with the basics and the shortcuts worth learning first.

The welcome is already in the latest nightly on the [download page](https://myarc.dev/download/). If you use myArc today, press ⌃⌥⌘A to see it. The code is [on GitHub](https://github.com/bitbonsai/myarc) if you'd like to help make that first minute even better.
