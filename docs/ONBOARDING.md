# Animated onboarding: launch requirement

## Creative direction

The first minute should feel cared for: calm, expressive, tactile, and immediately personal. Animation is central to this product. It explains how the browser works and makes a user's familiar workspace feel at home. A static setup form with a decorative video does not satisfy this requirement.

Take inspiration from Arc's sense of welcome and continuity, while creating original choreography, visuals, and copy. Exact historical Arc timing and sequences have not been verified; collecting permissible references and specifying what matters is MA-001. No copied Arc videos, sounds, illustrations, or traced brand assets.

## Experience storyboard

| Scene | What the person does | Motion and meaning | State / exit |
| --- | --- | --- | --- |
| 1. Welcome | Selects “Make it mine” or “Start browsing” | A quiet field of color gathers into a frame, then a sidebar. One short original signature animation, about 1–2 seconds, never delaying an available action. | Start browsing creates a usable local workspace immediately. No account form or AI upsell. |
| 2. Bring your setup | Chooses Arc export/local profile or starts fresh | A file/card moves into the preview area, anchoring the idea that data stays on this device. Show capabilities before asking for access. | Import is optional. Read permission follows a deliberate selection. Failure has retry/choose-file/start-fresh actions. |
| 3. Review the move | Reviews Spaces, profile mappings, counts, and omissions | Preview cards organize into a miniature sidebar. Use only successfully parsed data. Personal titles remain local. | Nothing is committed or navigated yet. Unsupported data is visible. Back preserves choices; cancel leaves the source unchanged. |
| 4. Your space arrives | Confirms the import | Following a successful commit, the actual imported Spaces, folders, and pins settle into their places. Stable item identities connect preview to the live workspace. Animate a bounded sample for large imports, then reveal the full list. | Real transaction progress and creative reveal are separate. Never indicate success before commit. Empty/new workspaces get equally considered motion. |
| 5. Learn by doing | Opens a pinned tab, switches a Space, tries Cmd+T | Short, contextual transitions draw attention to the actual UI. The user drives three small interactions. Reordering and tab opening share the same motion language as daily browsing. | Each lesson is independently skippable. Sample URLs are opened only after a deliberate action. No forced completion gate. |
| 6. Handoff | Chooses “You're ready” | The guided frame resolves into the real browser without a jarring scene cut or a fake screenshot being swapped out. | Focus lands on a useful control. Replay is available in Help/Settings. Default-browser request is optional and separate. |

AI setup belongs in Settings and an optional post-onboarding discovery entry. It is never part of the required welcome path. Do not make people choose a model or understand AI before browsing.

## Motion specification

- Design initial motion tokens: micro feedback 120–180 ms, movement between related surfaces 220–350 ms, major reveals 600–1,000 ms. These are proposed starting values, subject to prototype review.
- Use springs/easing sparingly to communicate weight and continuity; avoid constant bouncing, confetti, or looping spectacle.
- Use transforms and opacity where possible. Keep the animated element count bounded. Imported datasets can contain thousands of tabs.
- Keep text readable and interactive during transitions. Do not move the control someone is about to activate.
- Audio is off by default; no audio dependency. Any later sound must be original/licensed and independently controllable.
- Reduced motion substitutes an equally designed immediate layout and brief fades, avoiding spatial movement. Honor the OS setting from the first frame and support changing it during the flow.
- Window resize, high DPI, fullscreen, interrupted transitions, and Back/Skip must produce stable layouts.

## State model and implementation contract

Use a persisted state machine: `welcome → source → preview → commit → reveal → practice → complete`. Every optional path can reach `complete`. Commit errors return to a recoverable preview/error state. A restart after commit loads the saved workspace and resumes or skips the reveal; it never repeats the import automatically.

The import service owns progress and success, not an animation timer. The workspace model owns IDs, selections, and content. Motion reads these states through a presentation layer. Side effects have idempotency keys, and completion is persisted before playback starts. Daily UI and onboarding share tokens and components.

Build the early prototype with synthetic workspace data. Integrate with the real import command and native browser UI after the foundation decision. A web motion study is allowed for design exploration; it is not evidence that the browser's native animation has shipped.

## Acceptance criteria

1. A reviewer can see an original animated welcome, a personal workspace reveal, three live practice interactions, and a smooth handoff in a recording of the actual build.
2. The flow can be completed without creating an account, buying a plan, choosing AI, or accepting telemetry. Start-fresh works offline; import works offline when its selected source is local.
3. Every screen works with keyboard and screen reader. Focus order is deterministic; transitions do not steal focus; announcements describe real progress. Normal/reduced-motion paths expose equivalent information.
4. Skip, Back, window close, restart, and interrupted import/reveal have defined, verified outcomes. No duplicate imports or lost choices.
5. Animation never simulates import success, a working extension, or a live page. The report states what was imported, skipped, and unsupported.
6. Proposed performance budget: p95 animation frame time ≤16.7 ms on the M0 reference Mac at 60 Hz, measured during reveal with 1,000 synthetic tabs. No forced minimum waits; primary controls respond within 100 ms at p95. Record hardware and traces, then revise only with an explicit decision.
7. In a five-person formative study, at least four can recognize their workspace and complete the three practice tasks without assistance. Failures become backlog items. This is a design gate, not a population estimate.

## Required design deliverables

M0: storyboard, motion tokens, source notes, and a reviewable motion study. M1: state machine and synthetic successful/empty/error/large-import variants. M2: native integration and real-build recordings. M3/M4: keyboard/screen-reader and reduced-motion evidence, frame traces, interruption tests, and a signed-off quality review. Do not declare this feature complete from screenshots alone.
