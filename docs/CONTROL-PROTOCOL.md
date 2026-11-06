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

The Space commands work everywhere. The tab commands work only in the browser, and `--local` and `serve-local` answer them with `unsupported` because they have no pages. Every other command answers `unsupported`. `myarcctl serve-local` is a development stub that serves an in-memory workspace in place of the browser. That workspace lasts as long as `serve-local` runs, so a Space created by one `run` shows up in the next.

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

## Launch evidence

1. Through CLI and through MCP, perform the same synthetic-site journey: open → inspect → fill/click → wait for an observable result → inspect/screenshot → close. Verify identical semantics and errors.
2. Drive actual Spaces/tabs/persistence through shared commands and verify resulting workspace/URL state.
3. Connect one real external MCP client with built-in AI off and no hosted myarc account. Record client/protocol/build versions and use synthetic page content.
4. Demonstrate denied cross-Profile/tab access, stale references, out-of-scope redirects, prompt-injected page instructions, oversized input, cancellation, disconnect and immediate Stop control.
5. Show the real browser's scope approval, control indicator and human takeover. Keep core checks separate from native browser/agent integration evidence.
