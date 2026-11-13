# Browser control: CLI and MCP

Status: specified. A core stub in [crates/control](../crates/control/src/lib.rs) holds the command registry, runs the Space commands and checks the tab commands. Its [ipc module](../crates/control/src/ipc.rs) carries requests from a client to a running browser over an authenticated same-user Unix socket. The `myarc` CLI and its MCP server in [crates/myarc-cli](../crates/myarc-cli/src/main.rs) connect through that socket by default. The browser hosts the socket while the user has turned on browser control in Settings and asks the user to approve each client (see Browser host below), and `myarc serve-local` stands in for it in development. **Both interfaces are P0 browser launch requirements**, not just developer test tooling or a later paid AI feature. MA-011 delivers them. An external agent can control explicitly granted browser context while built-in AI remains off; the control interface itself requires no myarc account, subscription, model download or provider key.

## One control layer, two public interfaces

| Interface | User | Responsibility |
| --- | --- | --- |
| `myarc` CLI | People, scripts, debugging and reproducible tests | Human-readable output and explicit machine JSON; standalone workspace operations; authorized attachment for live page operations |
| MCP server launched by `myarc` | AI clients supporting the implemented transport/protocol | Discoverable typed tools and structured results; translates requests into the same operations |
| Shared control service | UI, CLI, MCP and future built-in agents | Validate scope/parameters, dispatch workspace commands or browser adapters, serialize conflicting actions and honor cancellation |

`myarc` and `myarc mcp` run today against the browser or the stub (see Try the stub and Browser host below). Browser UI and command bar share underlying operations, but their authorization context differs from an external agent's grant. Do not expose every internal command automatically through MCP.

## Required control surface

Names below are proposed command/tool IDs. Keep one versioned registry/schema rather than separate implementations.

| Area | Initial operations | Observable result |
| --- | --- | --- |
| Sessions | `session.status`, `session.request`, `session.release` | Request a grant; show authorized context/capabilities; release control |
| Tabs | `tabs.list`, `tab.open`, `tab.activate`, `tab.close`, `tab.navigate`, `tab.back`, `tab.forward`, `tab.reload` | Stable handles and resulting URL/title/navigation state within the grant |
| Observation | `page.snapshot`, `page.screenshot` | Bounded semantic accessibility/DOM view with element references, URL, document revision and loading state; screenshot of granted context |
| Interaction | `page.click`, `page.fill`, `page.select`, `page.press`, `page.scroll`, `page.wait` | Act on a current scoped reference and report resulting state or explicit uncertainty |
| Workspace | `space.list`, `space.create`, `space.select`, `space.rename`, `space.ensure_agent`, `node.move`, `tab.pin`, `tab.archive`, `tab.restore` | Validated changes through the same core used by the UI |
| Preservation | `import.preview`, `import.commit`, `import.rollback`, `workspace.export` | Supported local conversion/transaction; file destinations require a grant |
| Human control | Browser indicator and Stop control action | Shows controller/scope; revokes further access and cancels pending requests |

Use semantic interaction first. Coordinate actions, if added for canvas/inaccessible content, require a fresh scoped screenshot and defined viewport/revision semantics. Arbitrary JavaScript evaluation, unrestricted CDP, credential/cookie export, extension installation, internal browser pages and unrestricted local filesystem access are outside the default tool set. Advanced developer capabilities require a separate grant design and review.

## Observe → act → verify

Observations carry a tab/document revision. Resolve handles within that revision; navigation, invalidation or ambiguity returns an actionable error requiring a new observation. Never click a guessed replacement. Handle frames explicitly and recheck origin/context scope after redirects and before returning content.

Commands carry request/action IDs, timeouts and cancellation. Return structured success, denied, unsupported, stale, cancelled, timeout or outcome-unknown results. Cancellation does not prove a page-side effect was undone. Do not automatically repeat clicks/submissions after uncertain completion. Bound message/snapshot sizes, screenshot dimensions and waits; serialize conflicting actions on one tab.

An agent journey must inspect the real page after acting. A command acknowledgement or shell screenshot does not prove the intended website action happened.

## User grants and visibility

External control is disabled until the user enables it and approves the requested session/scope in browser-owned UI. Offer a dedicated automation Profile by default; allow deliberately selected tabs in a normal Profile. Bind the grant to controller/session, Profile/tab/origin scope, observation/action capabilities and expiry. Scope expansion requires approval; private browsing is excluded by default.

Routine actions within the granted task should not cause repetitive per-click prompts. Preserve appropriate client/user confirmation for consequential actions and browser-known privileged operations. Page text/model output never expands authority. Do not claim perfect classification of arbitrary website consequences from button labels.

The indicator names the controller/scope and exposes Stop control, revocation and a bounded local action history. Stop/release/expiry/disconnect revokes the grant; subsequent and queued calls fail. Keep logs free of credentials, tokens and page bodies by default. Enforce capabilities inside the browser/control service, not merely in CLI flags or tool descriptions.

## Local transport and compatibility

Start with MCP over **stdio** for supported local AI clients that launch the server process. Use a maintained SDK and explicitly supported protocol revisions. Test discovery, invocation, errors, cancellation and disconnect with an actual external client. Protocol traffic uses stdout; diagnostics use stderr. CLI JSON uses the same structured result schema.

CLI/MCP attach to the live browser through authenticated same-user local IPC and per-session authorization. Do not expose an unauthenticated TCP/HTTP listener or unrestricted debugging port. Reuse browser/platform mechanisms internally while enforcing the narrower grant; merely enabling CDP is not the product.

Local stdio does not imply compatibility with every MCP host or ChatGPT on the web. Publish a tested client/version/transport matrix. Remote/cloud clients require a separately authorized bridge and authenticated transport. Such a convenience service remains optional and cannot become a prerequisite for local control.

References: [MCP transports](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports) and [MCP tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools). These inform design, not a claim that interfaces have shipped.

## Install from the nightly

From 2026.11.13 on, the nightly carries `myarc` at `/Applications/myArc Nightly.app/Contents/Helpers/myarc`, signed with the app. Earlier nightlies called it `myarcctl`, and 2026.11.13 also shipped `Helpers/myarcctl` as a link to `myarc`. Later nightlies drop that link, so point any old `myarcctl` link or MCP config at `Helpers/myarc`. Link it into your `PATH`:

```sh
sudo mkdir -p /usr/local/bin && sudo ln -sf "/Applications/myArc Nightly.app/Contents/Helpers/myarc" /usr/local/bin/myarc
```

If macOS refuses to run it, clear the download's quarantine flag with `xattr -dr com.apple.quarantine "/Applications/myArc Nightly.app"`. To give Claude Code the MCP server in every project, run:

```sh
claude mcp add --scope user myarc -- "/Applications/myArc Nightly.app/Contents/Helpers/myarc" mcp
```

For the [skill](../skills/myarc-browser/SKILL.md), copy the `skills/myarc-browser` folder from this repository into `~/.claude/skills/`. The skill runs `myarc` through the link above. The CLI, the MCP server and the skill need "Allow agents to control myArc" turned on under Settings > Advanced, and the first command asks you in the myArc window to allow it (see Approval and Stop below).

## Try the stub

`run` and `mcp` connect to the running browser by default. The browser publishes its control endpoint in `~/Library/Application Support/myarc/control/`, or in `$MYARC_CONTROL_DIR` when that is set. The directory holds `control.json` with the socket path, a token that changes each time the server starts, the server's pid and its protocol version. Clients refuse the endpoint unless the directory (mode 700), the file and the socket (mode 600) belong to the current user and are closed to group and others. The browser creates a missing directory with mode 700 and refuses an existing one that group or others can enter, so it never changes the mode of a directory such as `$HOME`. Each connection opens with a hello that carries the token, and every message after it is a length-prefixed JSON frame of at most 1 MiB.

The Space commands work everywhere. The tab commands, `page.snapshot` and the interaction commands work only in the browser, and `--local` and `serve-local` answer them with `unsupported` because they have no pages. Every other command answers `unsupported`. `myarc serve-local` is a development stub that serves an in-memory workspace in place of the browser. That workspace lasts as long as `serve-local` runs, so a Space created by one `run` shows up in the next.

```sh
cargo build -p myarc-cli
target/debug/myarc serve-local --dir /tmp/myarc-control &
export MYARC_CONTROL_DIR=/tmp/myarc-control
target/debug/myarc commands
target/debug/myarc run space.create '{"name":"Work"}'
target/debug/myarc run space.list --json
```

`--local` skips the endpoint and runs the command against a fresh in-memory workspace, so a Space created by one `run --local` is gone by the next. `commands` always reads the registry locally.

Agents use the short commands, which send the same registry commands through the same client: `open <url>`, `snapshot [-i]`, `click @e4`, `fill @e1 <text>`, `select @e3 <value>`, `press <key> [@e1]`, `back`, `forward`, `reload`, `close`, `tabs` and `use <tab>`. They keep the current tab and the revision they last printed in `cli-state.json` (mode 600) inside the control directory, which must be theirs with mode 700. They write it through a new file with a random name and a rename, and a state file that is a symlink or someone else's reads as empty with a warning. They never fetch a newer revision themselves, so a stale outline still gets `stale`. `--tab` and `--rev` replace both for one call. `open`, `back`, `forward` and `reload` take a snapshot after the navigation. Actions print a status line, the changes in short form such as `changed e1 value`, then the new outline. `snapshot -i` keeps ref lines with their parents, the nearest heading and the text line just before each, and shows a link's target only when another kept link has the same name. `commands` lists the commands that run; `--all` adds the rest of the registry. `--json` prints each raw response. Against `serve-local` they get errors, since it has no pages.

`run` exits 0 on success, 1 when the command returns an error, 2 on a usage error, 3 when it can't reach myArc and 4 when myArc refused control, for example because the user declined. On exit 3 the message names the cause, such as a missing endpoint (myArc isn't running or browser control is off), a stale one left by a process that has exited, a permission or ownership problem, a rejected token or a protocol version mismatch. On exit 4 it gives the reason and what to tell the user. The short commands use the same codes. `--json` prints the full response.

`myarc mcp` serves the same registry as MCP tools over stdio, with dots in command IDs replaced by underscores (`space.create` becomes `space_create`). Tool calls reach the endpoint over one connection, which opens on the first call and reopens after it drops or sits idle. With no endpoint, each call returns a tool error that says why. `myarc mcp --local` keeps one in-memory workspace until the client disconnects instead. To add it to an MCP client, link the binary into your `PATH` as in Install from the nightly, or put a built one there, and use this server entry:

```json
{"command": "myarc", "args": ["mcp"]}
```

## Browser host

The browser serves the endpoint only while "Allow agents to control myArc" is on, a switch under Browser control in Settings > Advanced. It is off by default and saved in Local State as `myarc.control.enabled`, so one setting covers every Profile. Turning it on starts the host at once. Turning it off stops it, answers waiting requests with `cancelled` and removes `control.json` and the socket. While it is off the browser writes nothing to the control directory. `--myarc-enable-control` is a developer override that turns control on for that run, and the switch then shows on and can't be changed.

The first browser window starts the host after its Profile's workspace service exists. A dedicated background thread binds the endpoint in `$MYARC_CONTROL_DIR` or the default directory, and `myarc_control::ipc::Host` serves it. Each connection queues its request and waits for the answer on its own thread, so a slow page call holds up only its own connection. The UI thread takes requests one at a time in arrival order and runs `myarc_control::dispatch` against the browser's own workspace and then saves and notifies through the same path the sidebar's commands use, so the footer updates at once and the change survives a restart. A request with no answer after 10 seconds gets `timeout`, and the UI thread skips it if it hasn't started it yet. Quitting stops the host before the workspace's final save and removes `control.json` and the socket. A cancelled quit starts it again.

Each Profile has its own workspace, and each numbers its tabs from 1. The host's first request runs in the most recently active normal window that isn't private, and the host then stays in that window's Profile until it stops. Later requests run in the most recently active window of that Profile, even when the user has since switched to a window of another Profile, and `space.select` and `selected` refer to that window. With no window of the Profile open, or while its workspace is still loading, the request gets `denied` with the reason.

### Approval and Stop

Each connection's hello waits for the user before the browser sends its welcome. The first time a client name connects in a browser session, the last active normal window of the host's Profile shows a card at its top right that reads "myarc 2026.11.13 wants to control your Agent Space", with Allow and Don't allow. Name and version come from the hello, at most 64 bytes each. A nightly's `myarc` sends the nightly label as its version, which `tools/package_nightly.sh` sets through `MYARC_VERSION` at build time, and other builds send the crate version with `-dev`, such as `0.1.0-dev`. The card shows them without control or invisible format characters and cut to 40 characters. Allow welcomes every waiting connection of that name, lets later connections of that name in without asking until the browser quits, and pins the host to that window's Profile. Don't allow refuses the connection with `denied: the user declined`. With no answer after 60 seconds the connection gets `denied: waiting for approval timed out`, and with no window to ask in it gets `denied` and the reason. A restart or turning the switch off forgets every approval.

The wait happens before the welcome, so the 10 second reply timeout never runs during it. That timeout starts only with each request after the welcome. Clients wait up to 70 seconds for the welcome and 60 for each answer after it. `myarc` reports a refused hello with exit code 4, and `myarc mcp` as a tool error with the same text.

Each client names itself in its hello, so any process of the same user that can read `control.json` can claim to be any client. The prompt records consent and shows who is acting; it doesn't authenticate anyone. The token in `control.json` stays the authentication.

While an approved client is connected, or has sent a request in the last 30 seconds, the agent Space's bot in the footer carries a small blue dot and its tooltip reads "myarc is controlling the Agent Space". Clicking it still selects the agent Space. Right-clicking it offers "Stop myarc", and while the agent Space shows, a Stop button next to its title does the same. Stop closes that client's connections, answers a request still waiting with `cancelled` and forgets the approval, so the next connection asks again. `myarc mcp` reconnects once when its connection closed before a request went out, so its first call after a Stop asks the user again instead of answering `outcome_unknown`.

Builds with the GN arg `myarc_test_hooks` read one word from `test-hook` in `$MYARC_CONTROL_DIR`: `allow` and `decline` press the card's buttons, `stop` stops the active client, and `control-on` and `control-off` flip the switch. `tools/capture_native.sh` with `CONTROL_SWITCH=0` sets the directory without the override. Nightly builds have no test hooks.

### Tabs in the agent Space

The tab commands reach only tabs in the agent Space of that window's Profile. `tab.open {url}` creates the agent Space first when the Profile has none, then opens the page there as a temporary tab behind the user's page. The window keeps the user's Space, and the new tab is the one the agent Space shows when the user selects it, for example by clicking the bot in the footer. `tab.activate` works the same way: it brings the tab forward only in a window that already shows the agent Space. A tab or popup that an agent Space page opens, through `window.open` or a link with `target=_blank`, opens the same way, as a background tab of the agent Space in that page's window, and `tabs.list` lists it.

Each tab has a handle such as `t12` that stays valid while the tab lives. A handle names its Profile: `t12` is a tab of the `Default` Profile, `t12-p1` one of `Profile 1`, and other Profile directories get `x` and 16 hex digits of a hash, as in `t12-x0123456789abcdef`. A handle of another Profile than the one the host uses gets `denied`, so it never reaches a tab with the same number elsewhere. `tabs.list` returns the open agent Space tabs. A handle of a closed or unknown tab gets `stale`, and a tab in any other Space gets `denied` with "outside the agent Space". URLs must be http or https, without a user name or password, and at most 8 KiB. The core checks the text and the browser checks what it parsed from it, so `chrome:`, `file:`, `javascript:`, `data:` and other schemes get `denied`.

A tab command answers with `{"tab": {...}}`, or `{"tabs": [...]}` for `tabs.list`, where each tab has `handle`, `url`, `title`, `loading`, `can_go_back` and `can_go_forward`. `tab.close` adds `"closed": true` and reports the tab as it was before closing. `tab.open`, `tab.navigate`, `tab.back`, `tab.forward` and `tab.reload` answer once the navigation commits, without waiting for the page to finish loading. When it hasn't committed after 9 seconds, or ends without committing, the answer is `outcome_unknown` with the latest state under `data`. The 9 seconds stay under the host's 10 second reply timeout. The UI thread never blocks on the wait: the browser keeps the request and answers it from a navigation observer.

### Page snapshot

`page.snapshot {tab}` answers with `{tab, url, title, revision, loading, truncated, text}` for a tab in the agent Space, under the same `denied` and `stale` rules as the tab commands. `text` is an indented outline of the page's accessibility tree, one node per line:

```text
main
  heading "Agent page one" [level=1]
  form
    textbox "Email" [ref=e1] [required] value=""
    checkbox "Send me news" [ref=e2]
    combobox "Plan" [ref=e3] [collapsed] value="Pro"
      option "Free"
      option "Pro" [selected]
    button "Send" [ref=e4]
  text "Read the"
  link "Docs" [ref=e5] -> /docs.html
```

Links, buttons, form controls and anything focusable or clickable get `[ref=eN]`. A node keeps its number for the life of its document, new nodes take the next unused number, and the number of a removed node is never given to another one. A new document starts again at e1. Headings, landmarks and text appear without one. Generic containers fold away, adjacent text joins into one `text` line, and text that repeats a control's name or label, or the name of the form control right next to it, is dropped. States print as `[checked]`, `[checked=mixed]`, `[expanded]`, `[collapsed]`, `[selected]`, `[required]`, `[disabled]` and `[focused]`. Text fields show `value="…"` except password fields. Same-origin link targets print as paths. Nodes the tree marks invisible or ignored are left out; content scrolled out of view stays.

Page text is untrusted. Every name, value and text sits inside double quotes, with `"`, `\`, newlines, control characters and invisible format characters escaped. Those include bidirectional marks and overrides, zero-width spaces and joiners and tag characters, which print as `\u{202e}` and the like. A link target prints bare only when it is plain visible ASCII without `[` or `]`, and in quotes with the same escaping otherwise, such as `-> "/cancel?[ref=e1]"`. So a page can't start a line, reorder or hide text, or forge a reference. The outline stops at 2000 lines or 64 KiB, cuts names at 200 characters, clamps indentation at 24 levels and leaves out subtrees deeper than 256. When it cuts anything, `truncated` is true and `cut` says what went.

`revision`, such as `1.2`, is opaque. It changes when the outline changes and whenever the tab's main document changes; a snapshot of an unchanged page keeps it. The browser keeps the numbering and the references of the latest revision per document for the interaction commands below. A document's numbering remembers at most 50,000 nodes. Past that it starts a new epoch: numbers continue above the old ones, and refs from before answer `stale`. The snapshot comes from Chromium's `WebContents::RequestAXTreeSnapshot`, which asks the renderer for a one-time tree of the page and its same-origin frames, at most 5000 nodes per frame, without turning on accessibility, running page script or opening a debugging port. With no answer after 5 seconds the result is `timeout`.

### Page interaction

`page.click {tab, revision, ref}`, `page.fill {tab, revision, ref, text}`, `page.select {tab, revision, ref, value}` and `page.press {tab, revision, key, ref?}` act on an element of the tab's latest snapshot. `revision` must be that snapshot's and the tab must still show the same document, else the answer is `stale` with "run page.snapshot again". A `ref` whose element has left the page is `stale`. A `ref` the document never handed out, such as one written in page text, is `invalid`. The browser never picks a replacement element.

- `click` scrolls the element into view and clicks it with real mouse events, which also focuses it.
- `fill` replaces the value of an enabled, writable text field. `text` is at most 64 KiB, and newline and tab are its only allowed control characters.
- `select` picks an option of a native select by value, or else by visible text. An unknown, ambiguous or disabled option is `invalid`.
- `press` sends one key to the focused element: Enter, Tab, Escape, Backspace, the four arrows, PageUp, PageDown, Home, End, Space or one printable character. With `ref`, the element must be focused already or be a text field, which a click focuses first. When focus is in a frame the snapshot leaves out, such as a cross-origin frame, the key isn't sent and the answer is `invalid`.

Each snapshot node keeps its DOM node ID, because a same-origin snapshot keeps the renderer's IDs (a small patch to Chromium's tree combiner). The browser acts on that node through Chromium's renderer actor tools on the page's main frame. Those tools check the element is still attached, enabled and not covered before they send input events. Elements in cross-origin frames never get a usable reference.

After the action the browser waits until a started navigation commits and the new page loads, or until the page goes quiet, at most 3 seconds. Quiet means two snapshots 250 ms apart show the same outline while the tab isn't loading. While the action runs, the tab counts as captured, so a background page runs its timers on time and sees itself as visible. A request that runs longer than 250 ms without changing the page doesn't hold the answer back. Then it snapshots again and answers `{tab, url, revision, navigated, settled, changes, snapshot}`. `snapshot` has the shape of a `page.snapshot` answer and its revision replaces the old one. `settled` is false when the wait ran out. Its revision is one step after the one the action used. `changes` lists the references added and removed, such as `e7`, and changed, such as `e1 value` or `e3 name checked`, since the snapshot the action used, unless the tab navigated. Focus alone is no change. When the changes outnumber half the outline's lines, `changes` is `{"many": true}` instead. A page that refuses the action before anything happened, such as for a hidden element, answers `invalid` and the old revision stays usable. A `select` whose options the page doesn't report in time answers `timeout`, and its revision stays usable too. An action that times out, crashes the page or stops partway answers `outcome_unknown`, and nothing retries it. A crash answer names how the renderer ended, such as `status=killed, code=2, exit=9`.

## Launch evidence

1. Through CLI and through MCP, perform the same synthetic-site journey: open → inspect → fill/click → wait for an observable result → inspect/screenshot → close. Verify identical semantics and errors.
2. Drive actual Spaces/tabs/persistence through shared commands and verify resulting workspace/URL state.
3. Connect one real external MCP client with built-in AI off and no hosted myarc account. Record client/protocol/build versions and use synthetic page content.
4. Demonstrate denied cross-Profile/tab access, stale references, out-of-scope redirects, prompt-injected page instructions, oversized input, cancellation, disconnect and immediate Stop control.
5. Show the real browser's scope approval, control indicator and human takeover. Keep core checks separate from native browser/agent integration evidence.
