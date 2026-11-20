# Welcome tour plan

The native welcome tour closes two rows in [docs/ALPHA.md](../docs/ALPHA.md), "Animated onboarding in the native browser" and "Skip, Back and restart in onboarding". It follows [docs/ONBOARDING.md](../docs/ONBOARDING.md) and replaces the web study in [design/onboarding](../design/onboarding/README.md) as evidence. The study stays as the design reference for motion tokens, copy tone and focus rules.

Terms used below:
- A **coach mark** is a small bubble anchored to a real control. It says what to press and has Skip step and Back. It never blocks the window.
- A **step** is one state of the tour that the Rust state machine knows about.
- The **tour record** is the saved state of the tour, one per install.

## What Arc did

Screen recordings of Arc's first run show this order. A short animated intro plays, then one card per question sits on a full-window color field: account, import from another browser, favorite web apps, bookmarks, a theme color, ad blocking and default browser. Each card has Back, Next and Skip for now. A member card ends the setup, and the window then becomes the real browser with a "settle in" page listing four basics and an "Arc Basics" folder of pins in the sidebar. Import was also reachable later from the Arc menu.

myArc keeps the intro, the reveal of your own Spaces and the guided first actions. It drops the account, web apps, bookmarks and member card screens. The theme color comes from the imported Spaces instead of a picker. The default browser request moves to the handoff as a separate, optional button.

Videos studied, with frames pulled at 2 to 4 per second (timings below come from those frames and are rough):
- [Arc Browser for Mac Welcome Intro (2024)](https://www.youtube.com/watch?v=5LcrfMiUvmo)
- [Arc Browser for Mac Welcome Intro (2022)](https://www.youtube.com/watch?v=wI7PDKOKC8I)
- [Onboarding Breakdown: Arc Browser, Mike Bal](https://www.youtube.com/watch?v=ALuiwEUJN6Q), with its transcript

Written guides: [Supasidebar migration guide](https://supasidebar.com/blog/how-to-migrate-from-arc-step-by-step-2026), [TechRepublic](https://www.techrepublic.com/article/how-to-use-arc-browser/), [allthings.how](https://allthings.how/how-to-use-the-arc-browser). None of these is Arc's own help page.

## Flow

1. **Welcome.** A sheet covers the page area of the first window, and the sidebar sits dimmed beside it. A field of color gathers into a frame, then a sidebar, in about one second. Three actions are live from the first frame: Start browsing, Import from Arc… and Skip tour. Start browsing goes to the reveal. Import from Arc… runs the existing import (picker, preview, report from `arc_import.cc`), then goes to the reveal. Cancel in the picker or preview returns to Welcome with nothing changed.
2. **Reveal.** The sheet slides away and the real sidebar undims. Up to eight rows of the saved workspace settle into place, staggered, in about 800 ms. The rest appear at once. An empty workspace gets the same frame and one line of text. The reveal reads the workspace after commit, so it never shows something that wasn't saved.
3. **Practice 1, command bar.** A coach mark points at the address area and says "Press Cmd+T and type a site." The step completes when the bar opens a page.
4. **Practice 2, pin.** A coach mark points at that tab's row and says "Press Cmd+D to keep it." The step completes when the pin is saved. If practice 1 was skipped, this step offers to pin the current tab, and it skips itself when no web tab is open. A tab pinned already counts as done. Back from practice 3 shows a "Pinned" card with Next when the tab is pinned, and goes on to practice 1 when there's no web tab, so Back never bounces forward.
5. **Practice 3, Spaces.** With two or more Spaces, the coach mark points at the footer icons and says "Press Ctrl+2 to switch Space." With one Space it points at the "+" button and offers Create a Space, which opens the existing panel. The card names Ctrl+2 while the first Space shows and Ctrl+1 otherwise, so the press always switches. The step completes on the switch or on the new Space.
6. **Handoff.** The last coach mark says "You're ready" and fades into the window. Focus moves to the sidebar's current row. Set as default browser is an optional button in it. Replay welcome tour lives in Settings > Advanced, next to Import from Arc….

Each practice step has Skip step and Back in its coach mark. A skipped step is not counted as done. Skip tour from any step goes straight to the handoff state and saves it.

Import stays where it is today. The welcome offers it as one button that calls the same `ImportFromArc` code, so there is one import path, one preview and one report. Storyboard scenes 2 and 3 in ONBOARDING.md (bring your setup, review the move) are that existing dialog for alpha.

## Motion

Each Arc motion below is described from the recordings, followed by the original myArc motion it inspires and its Reduce Motion version. myArc copies none of Arc's art, logo, colors or wording.

**Intro.** In Arc, the desktop dims to near black over about half a second, and a soft blue blob appears in the middle of the screen. The blob wobbles and shifts its highlights for about three seconds. It then grows into the window's full size in about half a second, filled with a moving blue and violet gradient. The logo comes into focus from a blur, holds for about a second and blurs away. The headline then sharpens out of a blur, one line after the other, and a small button fades in under it about a second later. The easing looks like a slow ease-out throughout, with no bounce.

myArc instead starts inside the window, so it never dims the desktop. Five to eight small dots, in the colors of the default Space themes, drift in from the window edges and meet in the middle in about 600 ms, easing out. They join into one rounded rectangle that widens to the sheet's size in 300 ms, and a thin strip splits off to its left and slides into the sidebar's place. The headline rises 8 DIP and fades in, one line every 120 ms, and the three buttons fade in with it, so they are pressable from the first frame. The whole intro takes about a second, against Arc's six or so.

Under Reduce Motion, the finished sheet, sidebar strip and headline fade in together in 150 ms.

**Color field.** In Arc, the full-window gradient behind each card drifts slowly the whole time, and picking a theme color crossfades the field to that color in about a quarter second.

myArc keeps a slow color field behind the welcome sheet, but it moves only while the intro plays and then holds still. When an import commits, the field crossfades to the first imported Space's theme in 350 ms, using the tints from `sidebar_tint.cc`. That crossfade is the first sign that the workspace is yours. Under Reduce Motion the field is a still gradient, and the theme change is a 150 ms crossfade.

**Between steps.** In Arc, Next slides the card's text column a short way left while it fades, in about a quarter second, and the next card's content appears in place.

myArc has one welcome screen, so this motion applies to coach marks. A coach mark fades in and grows from 96% to full size in 180 ms, easing out, from the side of its anchor. Moving to the next step, the old mark fades out in 120 ms while the new one comes in at its own anchor. Under Reduce Motion, marks only fade.

**Handoff and reveal.** In Arc, the setup card disappears, and the real window appears with its sidebar and an empty page in a light tint. On that page a small logo appears, then about a second later the "settle in" heading, then a second after that a list of four basics cards, and last a preview video beside them. Each block fades in. It is a slow, calm stagger.

myArc does this on the real sidebar. The sheet slides right and fades in 350 ms. The sidebar undims, and up to eight rows settle in from 12 DIP below, 35 ms apart, each in 300 ms, easing out, about 800 ms in all. The first coach mark follows 200 ms after the last row. There is no checklist page, because the practice happens on the real controls. Under Reduce Motion the sheet fades out, every row is in place at once, and the coach mark fades in.

**End of tour.** Arc ends with a member card and sparkle particles before the handoff. myArc skips this, since [ONBOARDING.md](../docs/ONBOARDING.md) rules out confetti and looping spectacle. The last coach mark fades out in 180 ms, and the sidebar's current row gets one 150 ms highlight pulse. Under Reduce Motion, the mark fades out and the row gets no pulse.

## States and persistence

The state machine lives in a new `crates/workspace/src/onboarding.rs` (the `browser/foundation/workspace` symlink picks it up). It is pure: it takes the saved record and an event and returns the new record plus the effects the browser should run. It holds no timers and calls nothing.

States: `welcome → importing → reveal → practice(1..3) → handoff → complete`. `importing` exists only while the import dialog is open.

Events: `start_browsing`, `import_started`, `import_committed(receipt)`, `import_cancelled`, `import_failed`, `reveal_done`, `step_done(n)`, `skip_step(n)`, `back`, `skip_tour`, `finish`, `replay`, `window_closed`, `relaunched`. `reveal_done` moves the reveal on to practice 1 once its rows have settled. `relaunched` needs no workspace summary: the only workspace fact the rules use is whether the load found a first run, and `on_load` takes that directly.

The tour record is a small JSON string in a Local State pref, `myarc.onboarding.tour`, registered the way `little_arc_prefs.cc` registers its prefs. Rust encodes and decodes it and treats it as untrusted. It holds the state, the steps done, the steps skipped, the import receipt and a version number. An unreadable record reads as `complete`, so a damaged file never traps someone in the tour.

Rules, each with a Rust test:
- **First run.** The tour starts only when no tour record exists and the workspace load reported a first run (no workspace file, as `workspace_service.cc` already detects). An existing workspace with no record gets `complete` and no tour.
- **Skip.** `skip_tour` from any state saves `complete` before the handoff fades. Relaunch shows nothing.
- **Back.** From practice n it goes to practice n-1, from practice 1 to reveal, and from reveal to welcome only while nothing was imported. Back keeps done and skipped steps as they were.
- **Window closed mid-flow.** The record stays at its step. The next window, or the next launch, resumes there with a coach mark and no welcome replay. A closed window during `importing` saves `welcome`, because the dialog closes with it and Cancel changes nothing.
- **Restart mid-flow.** `relaunched` checks the record against the workspace. A record at `reveal` resumes at practice 1 with no animation. A record at `importing` whose receipt is missing goes back to `welcome`.
- **No duplicate import.** The record saves the import receipt (export content hash and committed Space ids) right after the commit returns and before the reveal starts. Resume never offers import again for that hash, and the import itself already adds nothing on a second run.
- **No lost choice.** Every event is saved before its effect runs, so a choice made just before a kill is there on relaunch.
- **Replay.** `replay` starts at welcome with Import from Arc… shown as "already imported" when a receipt exists, and practice counts reset.

## Native implementation

New files in `browser/foundation/`:
- `onboarding_prefs.cc/.h` registers the pref and reads and writes the record through the bridge.
- `onboarding_controller.cc/.h`, one per window, owned next to the sidebar. It feeds events to Rust, runs the returned effects and observes the commands that complete steps: the command bar opening a page, `IDC_MYARC_TOGGLE_PIN`, `IDC_MYARC_SELECT_SPACE_*` and the Create a Space panel's create callback. It never fakes a command.
- `welcome_view.cc/.h`, the welcome sheet, a `views::View` over the page area, built like `profile_swap_cover.cc`, with buttons from `native_button.cc`.
- `coach_mark.cc/.h`, the bubble, built on `MakeSpacePickerPopover`'s pattern from `create_space_panel.cc`: anchored, non-modal, with a `hold` flag so it stays up in an inactive window for background captures.
- `bridge.rs` gets a few cxx functions (`onboarding_event`, `onboarding_decode`) that return plain structs and never panic.

Motion:
- Animate layers with `views::AnimationBuilder` on opacity and transform only, as `peek.cc` and `library_view.cc` do. Timings and easing come from the [Motion](#motion) section. Ease-out maps to `gfx::Tween::EASE_OUT`, and the study's `cubic-bezier(.2, .8, .2, 1)` to `gfx::Tween::FAST_OUT_SLOW_IN`.
- The intro dots are one custom view that paints soft radial gradients with `cc::PaintShader::MakeRadialGradient`, stepped by a `gfx::LinearAnimation`. The dots are paint only, so the view's cost stays flat.
- The color field is a gradient painted once per theme. Its theme crossfade animates the opacity of a second layer on top.
- The reveal animates at most eight rows and one container, whatever the workspace size.
- No button moves while it can be pressed. Actions work during every animation, and pressing one ends the running animation at its final frame.
- Reduce Motion reads `gfx::Animation::PrefersReducedMotion()` at the start of every animation, as the existing views do, so a change mid-tour applies from the next animation. The reduced path shows the final layout with a 150 ms fade and no movement.

Keyboard and VoiceOver:
- Welcome: Tab cycles Start browsing, Import from Arc…, Skip tour. Return presses the focused one, Esc means Skip tour. Focus starts on Start browsing.
- Coach marks don't take focus, so the shortcut they teach still reaches the browser. Cmd+. or Esc while the bubble shows skips the step. F6, which already cycles panes in Chromium, moves focus into the bubble for Skip step and Back.
- Each step change announces the coach mark text with `GetViewAccessibility().AnnounceText`, as `copied_url_toast.cc` does. The import announces only real progress from the import dialog.
- After the handoff, focus lands on the sidebar's current row.

Test hooks, behind the existing test-hook build flag:
- `--myarc-test-onboarding=<state>` seeds the tour record and opens at that step.
- `--myarc-test-onboarding-event=<event>` feeds one event after load, so Skip, Back and resume can be captured.
- `tools/capture_native.sh` gets `ONBOARDING=<state>` and `ONBOARDING_EVENT=<event>`, and holds coach marks with `hold`.

## Steps

Each step ships on its own and lands behind the tour record, so an unfinished tour never shows to anyone with an existing workspace.

1. **Rust state machine.** `onboarding.rs` with the states, events, record encoding and a test for every rule above. Check: `cargo test --workspace --locked`.
2. **Pref, bridge and first-run detection.** Register the pref, add the bridge calls and the controller with no UI. Existing workspaces get `complete`. Check: `bridge_check.cc` covers the record round trip, and a capture of a fresh profile and a seeded one shows the record written as expected.
3. **Welcome sheet, static.** View, three buttons, Skip tour, keyboard order and announcements, with no motion yet. Check: captures of `ONBOARDING=welcome` and of `ONBOARDING_EVENT=skip_tour`.
4. **Welcome motion and Reduce Motion.** The gather animation and the reduced fade. Check: frame sequence captures in both modes.
5. **Import from the welcome.** Import from Arc… calls `ImportFromArc` and feeds its result back. Cancel returns to Welcome. Check: a capture through `ImportArcFileForCapture` with a fixture export, then a relaunch capture showing no second import and no import offer.
6. **Reveal.** Sheet away, rows settle, empty variant, bounded to eight rows. Check: frame captures of the fixture workspace, an empty one and one with 1,000 synthetic pins, in both motion modes.
7. **Coach mark and practice 1.** The bubble view, Skip step, Back and the command bar step. Check: captures of the held bubble, and of the step completing through the command bar's existing test hook.
8. **Practice 2 and 3.** Pin and Space steps with their fallbacks. Check: captures with one Space and with three, and of Back from practice 3.
9. **Handoff and replay.** Final fade, focus, optional default browser button, Replay welcome tour in Settings > Advanced. Check: captures of the handoff and of Settings > Advanced.
10. **Interruption suite and recordings.** A script like `tools/check_durability.sh` kills the browser during each state and checks the relaunch state. Record the whole tour in normal and reduced motion from the real build, then update the two ALPHA rows. Check: the script passes and both recordings show every scene.

The frame-time trace, the VoiceOver review and the five-person study stay on the separate "Onboarding quality checks" row.

## Open questions

Decided on 2026-11-19:
1. Only a fresh workspace sees the tour. Someone who upgrades finds it later under Settings > Advanced > Replay welcome tour, which step 9 adds.
2. Practice 3 is switching Space with Ctrl+2, or Create a Space when there's only one Space.
3. The intro motion stays inside the browser window and never dims the desktop.
