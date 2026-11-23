HTML: https://myarc.dev/blog/alpha-checklist-and-agent-control/

[All posts](https://myarc.dev/blog/)

# Closing the alpha checklist and giving agents eyes

November 23, 2026

[Mauricio Wolff](https://github.com/bitbonsai)3 min read

Nightly `2026.11.23.1` is out, and the alpha checklist is green from top to bottom. Every remaining item got a repeatable, hands-free verification script against the notarized app, and browser control picked up the three commands agents ask for most.

## Browser control, verified with Claude Code

Browser control ships inside `myArc Nightly.app/Contents/Helpers/myarc` as both a CLI and an MCP server (`myarc mcp`). Turn it on in Settings, and an external agent connects over a `0600` local socket, asks for permission through a real Allow card, and works inside its own Agent Space. Your own tabs and other Profiles stay out of reach, and clicking Stop in the Agent Space revokes the session on the spot.

I ran the full verification suite against `Helpers/myarc`, raw MCP stdio, and Claude Code (`--strict-mcp-config`) on a synthetic form fixture with adversarial inputs: stale snapshot revisions, forged element refs, hidden bidi characters, cross-Profile tab IDs, user tab IDs, blocked URL schemes, and oversized fills. All 161 control and CLI tests passed alongside the live runs.

[![myArc running the local synthetic form fixture used to verify CLI, MCP, and Claude Code control](https://myarc.dev/blog/alpha-checklist-and-agent-control/agent-space-active.c5a8193b5686.webp)](https://myarc.dev/blog/alpha-checklist-and-agent-control/agent-space-active.c5a8193b5686.webp)

The local synthetic form fixture used for CLI, MCP, and Claude Code verification, including the untrusted prompt-injection note.

This nightly adds three new commands to both the CLI and MCP:

- `page.screenshot` captures the active agent tab via Chromium's `CopyFromSurface`, scales it down to 1280 px max, and returns PNG (falling back to JPEG to stay under 600 KiB). Over MCP it comes back as a native image block alongside metadata.
- `page.scroll` scrolls `up`, `down`, `left`, or `right`, either on the main viewport or targeting a specific element ref.
- `page.wait` observes Chromium's `DidStopLoading` event with a bounded timeout so scripts wait for real navigation instead of guessing with sleeps.

## A new Agent Space icon and glass polish

The Agent Space has a new filled robot-head icon in both the Space header and the sidebar footer, matching the weight of the other Space icons.

[![myArc sidebar showing the new filled robot-head icon in the Agent Space header and bottom footer](https://myarc.dev/blog/alpha-checklist-and-agent-control/agent-space-icon.ccfc09de7010.webp)](https://myarc.dev/blog/alpha-checklist-and-agent-control/agent-space-icon.ccfc09de7010.webp)

The filled robot-head icon in the Agent Space header and footer.

A few rough visual spots also got fixed in this build:

- Frosted glass modals had a stray `ScrollView` backdrop painting over the blur; clearing it lets the vibrancy layer show through cleanly.
- Pinned extension icons in the URL pill dropped their hover plate and now use the same frosted tooltips as the sidebar footer.

## Core browsing, offline audit, and 1,000 tabs

To close the remaining alpha gates without hand-waving, I added three more scripts that run against a fresh disposable Profile:

- **Core browsing smoke:** verified native notification permission prompts (`Block` / `Allow`), frosted find-in-page (`⌘F`), docked DevTools (`⌘⌥I`), and `chrome://history` backed by SQLite.
- **Offline and no-account audit:** launched with `--host-resolver-rules` blocking external hosts and captured a full NetLog. myArc opened with zero account prompts, loaded a local page, pinned it with `⌘D`, persisted it to disk, and made zero calls to any myArc service.
- **1,000-tab responsiveness benchmark:** seeded 10 Spaces with 100 dormant items each on an Apple M5 Pro (macOS 26.7.3). First on-screen window appeared in **797.5 ms**, steady-state memory settled at **953.5 MB** across 11 processes, Space switches ran at **75.5 ms median / 83.1 ms p95**, and atomic workspace saves finished in **2.08 ms median**.

[![myArc showing frosted find-in-page highlighting matches on a synthetic fixture](https://myarc.dev/blog/alpha-checklist-and-agent-control/core-smoke-find.fc9d74b8a3ba.webp)](https://myarc.dev/blog/alpha-checklist-and-agent-control/core-smoke-find.fc9d74b8a3ba.webp)

Find-in-page highlighting matches inside the glass bar.

[![myArc showing docked Chromium DevTools inspecting the synthetic fixture](https://myarc.dev/blog/alpha-checklist-and-agent-control/core-smoke-devtools.aa0c0f10953f.webp)](https://myarc.dev/blog/alpha-checklist-and-agent-control/core-smoke-devtools.aa0c0f10953f.webp)

Docked DevTools inspecting a local test page.

[![myArc sidebar populated with 1,000 synthetic dormant items across 10 Spaces](https://myarc.dev/blog/alpha-checklist-and-agent-control/thousand-tabs.79ee9153b8e8.webp)](https://myarc.dev/blog/alpha-checklist-and-agent-control/thousand-tabs.79ee9153b8e8.webp)

The 1,000-item synthetic workspace used for startup, memory, and Space-switch measurements.

## Grab the update

If you already have myArc Nightly installed, open **myArc \> Check for Updates…** to pull `2026.11.23.1` directly. Fresh installs are on the [download page](https://myarc.dev/download/).
