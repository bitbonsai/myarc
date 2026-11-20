# Welcome redesign after Arc's recorded onboarding

The user recorded Arc's first launch on 2026-11-20 and named it the goal. The recording is in `research-private/arc/onboarding-user-20261120/` and is never committed or published, because it shows the user's own desktop. This plan replaces the welcome sheet and intro from steps 3 and 4 of [the welcome tour plan](2026-11-19-welcome-tour.md). The reveal, the practices and the handoff stay.

myArc's version keeps Arc's timing and staging. The mark, the colours, the copy and the illustrations are myArc's own. Nothing is copied from Arc's art or wording.

## What the recording shows

The clip is 21 s at 60 fps. Times are from the start of the clip, and the first 2.5 s are the Gatekeeper prompt.

1. **Desktop dims (about 2.5 to 3 s).** The whole screen, menu bar included, fades to near black over about 0.5 s. Other windows stay faintly visible.
2. **Blob at the cursor (3 to 6 s).** A soft blue blob appears under the pointer. It wobbles and its edge changes shape, with cyan and green highlights turning inside it. It stays where the pointer is.
3. **Blob becomes the window (about 6 s).** The blob grows into a rounded window filled with a blue and violet gradient, light blue at top left and deep indigo at the bottom right. The window shows only the traffic lights, with no toolbar or sidebar. The gradient keeps drifting slowly for the rest of the clip.
4. **Mark (6 to 8.5 s).** A white line mark sharpens out of a blur in the centre, holds, then fades.
5. **Headline (8.5 to 10 s).** One short bold white line slides in from the left while its motion blur clears.
6. **Button (about 10 s).** A small translucent pill with an arrow fades in under the headline. Nothing else is on screen.
7. **Split (after the click, about 14 s).** The window divides. A dark panel fades in over the left half, holding a small mark, a title, two lines of body text, a primary button and a text link. The right half keeps the gradient and slides a framed product preview in from the right. Later steps swap the left panel's text with a crossfade.

## myArc's version

The desktop dim is the user's choice (2026-11-20), and it reverses the earlier "inside the window only".

1. **Dim.** A borderless, click-through, full-screen window on each display fades to 85% black over 400 ms. It's above other apps' windows and below the menu bar's level only if macOS requires that. It goes away when the welcome window lands.
2. **Blob.** Under the pointer, or at the screen centre when the pointer is on another display, a 120 pt blob of myArc's dot colours is drawn as a few overlapping blurred circles moving on slow sine paths. It runs for about 2 s.
3. **Grow.** The blob scales into the welcome window's frame over 500 ms with ease-in-out, crossfading into the gradient. The welcome window is the first browser window, with the sidebar and toolbar hidden until the reveal.
4. **Gradient.** Two or three large blurred colour fields drift on 20 to 30 s loops. They use the default theme's blues and violets, or the imported Space's theme after an import.
5. **Mark.** myArc's own mark sharpens in from a blur, holds 1 s, then fades.
6. **Headline and button.** myArc's own short headline, with the slide and blur-clear, then the arrow pill. Return and the arrow both continue, and Esc skips the tour.
7. **Split.** The left panel offers the choice that used to be on the sheet: "Import from Arc…" as the primary button, and "Start fresh" and "Skip tour" as links. The right half shows a synthetic preview of a myArc window with a sidebar of made-up Spaces. After an import the preview is swapped for the user's own sidebar, and the reveal follows as built in step 6.
8. **Reduce Motion.** The dim, the blob and the drift are left out. The welcome window fades in already showing the headline, and each later change is a 150 ms crossfade.

The window, the dim layer and the blob must each be owned by one object. Each is removed through a posted task or a WeakPtr, never from inside its own callback. That's the cause of the 2026-11-20 crash in the nightly.

## Steps

1. Dim layer, blob and grow, with Reduce Motion, plus capture hooks that record the whole screen.
2. Gradient window, mark, headline and arrow pill, replacing `WelcomeView`.
3. Split panel with import, start fresh and skip, the synthetic preview, and the hand-off into the existing reveal.
4. A recorded side-by-side check of the full sequence against the recording's timings, then a live run on the nightly with real keys.
