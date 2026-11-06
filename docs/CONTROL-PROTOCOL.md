# Browser control: CLI and MCP

Status: specified. A core stub in [crates/control](../crates/control/src/lib.rs) holds the command registry, runs the Space commands and checks the tab commands. Its [ipc module](../crates/control/src/ipc.rs) carries requests from a client to a running browser over an authenticated same-user Unix socket. The `myarcctl` CLI and its MCP server in [crates/myarcctl](../crates/myarcctl/src/main.rs) connect through that socket by default. The browser hosts the socket when started with `--myarc-enable-control` (see Browser host below), and `myarcctl serve-local` stands in for it in development. **Both interfaces are P0 browser launch requirements**, not just developer test tooling or a later paid AI feature. MA-011 delivers them. An external agent can control explicitly granted browser context while built-in AI remains off; the control interface itself requires no myarc account, subscription, model download or provider key.

## One control layer, two public interfaces

| Interface | User | Responsibility |
| --- | --- | --- |
| `myarcctl` CLI | People, scripts, debugging and reproducible tests | Human-readable output and explicit machine JSON; standalone workspace operations; authorized attachment for live page operations |
| MCP server launched by `myarcctl` | AI clients supporting the implemented transport/protocol | Discoverable typed tools and structured results; translates requests into the same operations |
| Shared control service | UI, CLI, MCP and future built-in agents | Validate scope/parameters, dispatch workspace commands or browser adapters, serialize conflicting actions and honor cancellation |

`myarcctl` and `myarcctl mcp` run today against the browser or the stub (see Try the stub and Browser host below). Browser UI and command bar share underlying operations, but their authorization context differs from an external agent's grant. Do not expose every internal command automatically through MCP.

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

## Try the stub

`run` and `mcp` connect to the running browser by default. The browser publishes its control endpoint in `~/Library/Application Support/myarc/control/`, or in `$MYARC_CONTROL_DIR` when that is set. The directory holds `control.json` with the socket path, a token that changes each time the server starts, the server's pid and its protocol version. Clients refuse the endpoint unless the directory (mode 700), the file and the socket (mode 600) belong to the current user and are closed to group and others. Each connection opens with a hello that carries the token, and every message after it is a length-prefixed JSON frame of at most 1 MiB.

The Space commands work everywhere. The tab commands, `page.snapshot` and the interaction commands work only in the browser, and `--local` and `serve-local` answer them with `unsupported` because they have no pages. Every other command answers `unsupported`. `myarcctl serve-local` is a development stub that serves an in-memory workspace in place of the browser. That workspace lasts as long as `serve-local` runs, so a Space created by one `run` shows up in the next.

```sh
cargo build -p myarcctl
target/debug/myarcctl serve-local --dir /tmp/myarc-control &
export MYARC_CONTROL_DIR=/tmp/myarc-control
target/debug/myarcctl commands
target/debug/myarcctl run space.create '{"name":"Work"}'
target/debug/myarcctl run space.list --json
```

`--local` skips the endpoint and runs the command against a fresh in-memory workspace, so a Space created by one `run --local` is gone by the next. `commands` always reads the registry locally.

`run` exits 0 on success, 1 when the command returns an error, 2 on a usage error and 3 when it can't reach myArc. On exit 3 the message names the cause, such as a missing endpoint, a stale one left by a process that has exited, a permission or ownership problem, a rejected token or a protocol version mismatch. `--json` prints the full response.

`myarcctl mcp` serves the same registry as MCP tools over stdio, with dots in command IDs replaced by underscores (`space.create` becomes `space_create`). Tool calls reach the endpoint over one connection, which opens on the first call and reopens after it drops or sits idle. With no endpoint, each call returns a tool error that says why. `myarcctl mcp --local` keeps one in-memory workspace until the client disconnects instead. To add it to an MCP client, put the built binary on your `PATH` and use this server entry:

```json
{"command": "myarcctl", "args": ["mcp"]}
```

## Browser host

The browser serves the endpoint only when launched with `--myarc-enable-control`. Without the switch it writes nothing to the control directory. The switch is a stand-in: a Settings toggle and the grant UI described above replace it later, and until then every same-user process that can read `control.json` gets full control of the Space commands and the agent Space's tabs.

The first browser window starts the host after its Profile's workspace service exists. A dedicated background thread binds the endpoint in `$MYARC_CONTROL_DIR` or the default directory, and `myarc_control::ipc::Host` serves it. Each request goes to the UI thread, which runs `myarc_control::dispatch` against the browser's own workspace and then saves and notifies through the same path the sidebar's commands use, so the footer updates at once and the change survives a restart. A request with no answer after 10 seconds gets `timeout`, and the UI thread skips it if it hasn't started it yet. Quitting stops the host before the workspace's final save and removes `control.json` and the socket. A cancelled quit starts it again.

Each Profile has its own workspace. A request runs in the most recently active normal window that isn't private, against that window's Profile, and `space.select` and `selected` refer to that window. With no such window open, or while its workspace is still loading, the request gets `denied` with the reason.

### Tabs in the agent Space

The tab commands reach only tabs in the agent Space of that window's Profile. `tab.open {url}` creates the agent Space first when the Profile has none, then opens the page there as a temporary tab behind the user's page. The window keeps the user's Space, and the new tab is the one the agent Space shows when the user selects it, for example by clicking the bot in the footer. `tab.activate` works the same way: it brings the tab forward only in a window that already shows the agent Space.

Each tab has a handle such as `t12` that stays valid while the tab lives. `tabs.list` returns the open agent Space tabs. A handle of a closed or unknown tab gets `stale`, and a tab in any other Space gets `denied` with "outside the agent Space". URLs must be http or https, without a user name or password, and at most 8 KiB. The core checks the text and the browser checks what it parsed from it, so `chrome:`, `file:`, `javascript:`, `data:` and other schemes get `denied`.

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

Links, buttons, form controls and anything focusable or clickable get `[ref=eN]`, numbered in output order. Headings, landmarks and text appear without one. Generic containers fold away, adjacent text joins into one `text` line, and text that repeats a control's name or label is dropped. States print as `[checked]`, `[checked=mixed]`, `[expanded]`, `[collapsed]`, `[selected]`, `[required]`, `[disabled]` and `[focused]`. Text fields show `value="…"` except password fields. Same-origin link targets print as paths. Nodes the tree marks invisible or ignored are left out; content scrolled out of view stays.

Page text is untrusted. Every name, value and text sits inside double quotes, with `"`, `\`, newlines, control characters and bidirectional formatting characters escaped, so a page can't start a line or forge a reference. The outline stops at 2000 lines or 64 KiB, cuts names at 200 characters, clamps indentation at 24 levels and leaves out subtrees deeper than 256. When it cuts anything, `truncated` is true and `cut` says what went.

`revision`, such as `1.2`, changes on every snapshot and whenever the tab's main document changes. The browser keeps the references of the latest revision per document for the interaction commands below. The snapshot comes from Chromium's `WebContents::RequestAXTreeSnapshot`, which asks the renderer for a one-time tree of the page and its same-origin frames, at most 5000 nodes per frame, without turning on accessibility, running page script or opening a debugging port. With no answer after 5 seconds the result is `timeout`.

### Page interaction

`page.click {tab, revision, ref}`, `page.fill {tab, revision, ref, text}`, `page.select {tab, revision, ref, value}` and `page.press {tab, revision, key, ref?}` act on an element of the tab's latest snapshot. `revision` must be that snapshot's and the tab must still show the same document, else the answer is `stale` with "run page.snapshot again". A `ref` the snapshot doesn't hold, such as one written in page text, is `invalid`. The browser never picks a replacement element.

- `click` scrolls the element into view and clicks it with real mouse events, which also focuses it.
- `fill` replaces the value of an enabled, writable text field. `text` is at most 64 KiB, and newline and tab are its only allowed control characters.
- `select` picks an option of a native select by value, or else by visible text. An unknown, ambiguous or disabled option is `invalid`.
- `press` sends one key to the focused element: Enter, Tab, Escape, Backspace, the four arrows, PageUp, PageDown, Home, End, Space or one printable character. With `ref`, the element must be focused already or be a text field, which a click focuses first.

Each snapshot node keeps its DOM node ID, because a same-origin snapshot keeps the renderer's IDs (a small patch to Chromium's tree combiner). The browser acts on that node through Chromium's renderer actor tools on the page's main frame. Those tools check the element is still attached, enabled and not covered before they send input events. Elements in cross-origin frames never get a usable reference.

After the action the browser waits until a started navigation commits and the new page loads, or until the renderer's page stability monitor sees the network and main thread idle, at most 3 seconds. Then it snapshots again and answers `{tab, url, revision, navigated, settled, changes, snapshot}`. `snapshot` has the shape of a `page.snapshot` answer and its revision replaces the old one. `settled` is false when the wait ran out. `changes` lists the references added, removed or changed since the snapshot the action used, unless the tab navigated. A page that refuses the action before anything happened, such as for a hidden element, answers `invalid` and the old revision stays usable. An action that times out, crashes the page or stops partway answers `outcome_unknown`, and nothing retries it.

## Launch evidence

1. Through CLI and through MCP, perform the same synthetic-site journey: open → inspect → fill/click → wait for an observable result → inspect/screenshot → close. Verify identical semantics and errors.
2. Drive actual Spaces/tabs/persistence through shared commands and verify resulting workspace/URL state.
3. Connect one real external MCP client with built-in AI off and no hosted myarc account. Record client/protocol/build versions and use synthetic page content.
4. Demonstrate denied cross-Profile/tab access, stale references, out-of-scope redirects, prompt-injected page instructions, oversized input, cancellation, disconnect and immediate Stop control.
5. Show the real browser's scope approval, control indicator and human takeover. Keep core checks separate from native browser/agent integration evidence.
