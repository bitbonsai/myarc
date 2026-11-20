# Welcome redesign after Arc's recorded onboarding

The user recorded Arc's first launch on 2026-11-20 and named it the goal. The recording is in `research-private/arc/onboarding-user-20261120/` and is never committed or published, because it shows the user's own desktop. This plan replaces the welcome sheet and intro from steps 3 and 4 of [the welcome tour plan](2026-11-19-welcome-tour.md). The reveal, the practices and the handoff stay.

myArc's version keeps Arc's timing and staging. The mark, the colours, the copy and the illustrations are myArc's own. Nothing is copied from Arc's art or wording.

## What the recording shows

The clip is 21 s at 60 fps. Times are from the start of the clip, and the first 2.5 s are the Gatekeeper prompt.

1. **Desktop dims (about 2.6 to 3.4 s).** Everything below the menu bar fades to near black over about 0.75 s. The menu bar stays at full brightness. Other windows stay faintly visible. The blob is already on screen while the dim is still fading in.
2. **Blob (2.6 to 5.8 s).** One heavily blurred blob, about a quarter of the screen's width, sits at the centre of the screen. Its outline is irregular, like a soft rounded triangle, and keeps changing shape. It's mostly one blue, with cyan and mint highlight patches moving across it. There are no separate circles.
3. **Blob becomes the window (5.8 to 6.2 s).** In about 0.3 s the blob swells unevenly past the window's edges while staying blobby. The window's rounded frame clips it, and the highlights smear into a rounded window filled with a blue and violet gradient, light blue at top left and deep indigo at the bottom right. The traffic lights fade in about 0.3 s after that. The window is about three quarters of the screen's width and centred, with no toolbar or sidebar. The gradient keeps drifting slowly for the rest of the clip.
4. **Mark (6 to 8.5 s).** A white line mark sharpens out of a blur in the centre, holds, then fades.
5. **Headline (8.5 to 10 s).** One short bold white line slides in from the left while its motion blur clears.
6. **Button (about 10 s).** A small translucent pill with an arrow fades in under the headline. Nothing else is on screen.
7. **Split (after the click, about 14 s).** The window divides. A dark panel fades in over the left half, holding a small mark, a title, two lines of body text, a primary button and a text link. The right half keeps the gradient and slides a framed product preview in from the right. Later steps swap the left panel's text with a crossfade.

## myArc's version

The desktop dim is the user's choice (2026-11-20), and it reverses the earlier "inside the window only".

1. **Dim.** A borderless, click-through window covering each display below the menu bar fades to 85% midnight over 750 ms, starting together with the blob. The menu bar stays undimmed. The dim goes away when the welcome window lands.
2. **Blob.** At the centre of the main display, never following the pointer, a 120 pt blob in the ribbon logo's blue, violet, pink and coral is one soft shape whose edge keeps changing: a closed path through 6 to 8 points whose radii wobble on slow out-of-phase sine waves, filled with a turning gradient of those colours and lightly blurred. It must never read as circles. It's about a quarter of the screen's width and runs for about 3 s.
3. **Grow.** Over about 300 ms the blob swells unevenly past the welcome window's frame. The window's rounded frame clips it, and it crossfades into the gradient. The traffic lights fade in 300 ms later. The welcome window is the first browser window, with the sidebar and toolbar hidden until the reveal.
4. **Gradient.** The window uses myarc.dev's palette: a midnight base (`#101c2b`, `#0b1420` in dark mode) with two or three large blurred fields of the ribbon's colours and the site blue (`#034bf4`) drifting on 20 to 30 s loops. After an import it takes the imported Space's theme. Text is ivory (`#f5f2ec`).
5. **Mark.** The ribbon logo from `design/brand/assets/myarc-matte.png`, animated as on the myarc.dev hero: it rises 1.25rem while fading in over 1.1 s on the site's ease (`cubic-bezier(.2, .7, .2, 1)`), then drifts once over 3.5 s (up .875rem, 1° turn, and back). It then fades for the headline. This replaces the plain dark welcome panel (title, body, Start browsing, Import from Arc…, Skip tour), which today shows no logo or brand colour.
6. **Headline and button.** myArc's own short headline, with the slide and blur-clear, then the arrow pill. Return and the arrow both continue, and Esc skips the tour.
7. **Split.** The left panel offers the choice that used to be on the sheet: "Import from Arc…" as the primary button, and "Start fresh" and "Skip tour" as links. The right half shows a synthetic preview of a myArc window with a sidebar of made-up Spaces. After an import the preview is swapped for the user's own sidebar, and the reveal follows as built in step 6.
8. **Reduce Motion.** The dim, the blob and the drift are left out. The welcome window fades in already showing the headline, and each later change is a 150 ms crossfade.

The window, the dim layer and the blob must each be owned by one object. Each is removed through a posted task or a WeakPtr, never from inside its own callback. That's the cause of the 2026-11-20 crash in the nightly.

## Steps

1. Dim layer, blob and grow, with Reduce Motion, plus capture hooks that record the whole screen.
2. Gradient window, mark, headline and arrow pill, replacing `WelcomeView`. The traffic lights fade in on their own after the grow. Replay keeps the window's size and display. The "Google API keys are missing" bar never shows in the nightly or during the tour.
3. Split panel with import, start fresh and skip, the synthetic preview, and the hand-off into the existing reveal.
4. A recorded side-by-side check of the full sequence against the recording's timings, then a live run on the nightly with real keys.
