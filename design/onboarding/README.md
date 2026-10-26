# The first minute, motion study 001

MA-001.a is a disposable, original web study. It runs six onboarding scenes with invented content. It does not import files, open websites, store a workspace, or implement the production browser. MA-008 native acceptance remains open.

## Run and verify

From the repository root:

```sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory design/onboarding
```

Open `http://127.0.0.1:8765`. No package installation, font download or service is required. Fonts use locally available Avenir and Palatino families with fallbacks. Every illustration is original CSS geometry; no Arc or brand-kit assets are loaded.

```sh
node --test design/onboarding/state.test.mjs
node --check design/onboarding/study.mjs
python3 tools/check_onboarding.py
```

State tests need Node.js 18 or newer. The browser check needs Python 3.9+, `agent-browser` with its Chrome installed, and `ffprobe` from FFmpeg. It serves only this directory on an ephemeral loopback port, drives actual controls, and writes fresh recordings, screenshots and a hashed manifest under ignored `out/onboarding/`. It closes its browser and, on macOS, kills agent-browser helpers and Chrome for Testing processes, including lingering sessions. Run it when those test-browser sessions can be closed.

The manifest identifies source hashes, executed checks, browser version, video frame counts and artifact hashes. Videos must contain multiple frames and last at least two seconds; existence alone does not pass. These thresholds detect broken captures, not animation performance. Source hashes must match before reusing a recording as current evidence.

## Scenes and behavior

- Welcome gathers the sample frame over 1,000 ms. Both actions remain available during playback.
- Setup selects familiar, empty, 1,000-pin or failure fixtures. Three Spaces reference two Profiles, with Everyday and Weekend sharing Personal.
- Review shows counts and exclusions. Back preserves fixture choice. No file or profile access occurs.
- Apply dispatches an in-memory fixture result in a microtask. Successful application permits reveal. The failure fixture rejects its first attempt and permits retry. Animation does not decide success.
- Reveal settles at most eight pins and one artwork container over 800 ms, staggered by 35 ms. Initial sidebar shows at most 12 pins; practice can request more. Empty setups receive the same frame and an empty-state message.
- Practice opens local sample content, switches Spaces and searches through a modal command bar with Cmd+T or Ctrl+T. Each lesson can be skipped independently; skipped actions are not counted as completed. Handoff retains the selected sample content and offers workspace focus.

The study reloads to Welcome and writes no browser storage. Replay discards study state. This deliberately leaves persisted onboarding, interrupted-commit recovery and import idempotency to the Rust model/store and native integration work.

## Motion and accessibility

Tokens in `study.css`: micro feedback 150 ms, reveal 800 ms, easing `cubic-bezier(.2, .8, .2, 1)`. Welcome uses 1,000 ms. Controls under active interaction stay still. Normal playback has no minimum waiting period.

OS reduced motion applies through CSS on the first frame. The Motion selector can follow the OS, suppress animation, or explicitly request full playback. Reduced mode uses immediate layouts without spatial movement. Both modes expose the same content and actions.

Scene and lesson changes focus the heading. Pin and Space actions retain their control's focus. The native dialog provides modal focus containment; Escape and Close return focus to its opener, or to a chosen search result. Status announcements describe sample actions. Source choices use a labeled radio group. Desktop and 390-pixel layouts are exercised by the browser check. Automated focus/accessibility-tree checks do not substitute for VoiceOver review.

## Production integration boundary

`state.mjs` owns storyboard navigation only. It is not a JavaScript copy of production workspace rules. `study.mjs` reads fixed fixtures and renders them; it never performs a real import or browser command.

Replace fixture results with shared Rust import/receipt state after MA-004/005/006. Reveal must read the committed generation's stable IDs. Practice must call the same native commands as daily browsing. Persist completion before playback, resume without re-importing, and verify cancellation/restart in the actual browser. Keep source parsing and persistence outside motion code. Details: [BC-002](../../.plans/BEHAVIOR-CONTRACTS.md) and [onboarding acceptance](../../docs/ONBOARDING.md).

## Evidence and limits

Recorded run on 2026-10-24: [six state tests](evidence/state-tests.txt) and [40 browser checks with source/artifact hashes](evidence/run.json) passed using agent-browser 0.27.3 and its Headless Chrome 151. Recordings: [normal](evidence/normal.webm) and [reduced motion](evidence/reduced.webm). [Welcome](evidence/welcome.png), [handoff](evidence/handoff.png) and [mobile](evidence/mobile.png) captures come from that same run. Both recordings were decoded and their frame sequences inspected. This is design evidence only, independent of the Chromium 158 source-build experiment.

Unmeasured: native frame time, control latency, VoiceOver behavior, high-DPI/fullscreen transitions and five-person formative task results. No Arc timing or layout fidelity is asserted. No browser release gate is closed by this study.
