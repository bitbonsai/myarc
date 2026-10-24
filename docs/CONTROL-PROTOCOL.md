# Browser control: CLI and MCP

Status: specified, not implemented. **Both interfaces are P0 browser launch requirements**, not just developer test tooling or a later paid AI feature. MA-011 delivers them. An external agent can control explicitly granted browser context while built-in AI remains off; the control interface itself requires no myarc account, subscription, model download or provider key.

## One control layer, two public interfaces

| Interface | User | Responsibility |
| --- | --- | --- |
| `myarcctl` CLI | People, scripts, debugging and reproducible tests | Human-readable output and explicit machine JSON; standalone workspace operations; authorized attachment for live page operations |
| MCP server launched by `myarcctl` | AI clients supporting the implemented transport/protocol | Discoverable typed tools and structured results; translates requests into the same operations |
| Shared control service | UI, CLI, MCP and future built-in agents | Validate scope/parameters, dispatch workspace commands or browser adapters, serialize conflicting actions and honor cancellation |

These executable names are proposed, not commands that run today. Browser UI and command bar share underlying operations, but their authorization context differs from an external agent's grant. Do not expose every internal command automatically through MCP.

## Required control surface

Names below are proposed command/tool IDs. Keep one versioned registry/schema rather than separate implementations.

| Area | Initial operations | Observable result |
| --- | --- | --- |
| Sessions | `session.status`, `session.request`, `session.release` | Request a grant; show authorized context/capabilities; release control |
| Tabs | `tabs.list`, `tab.open`, `tab.activate`, `tab.close`, `tab.navigate`, `tab.back`, `tab.forward`, `tab.reload` | Stable handles and resulting URL/title/navigation state within the grant |
| Observation | `page.snapshot`, `page.screenshot` | Bounded semantic accessibility/DOM view with element references, URL, document revision and loading state; screenshot of granted context |
| Interaction | `page.click`, `page.fill`, `page.select`, `page.press`, `page.scroll`, `page.wait` | Act on a current scoped reference and report resulting state or explicit uncertainty |
| Workspace | `space.list`, `space.create`, `space.select`, `space.rename`, `node.move`, `tab.pin`, `tab.archive`, `tab.restore` | Validated changes through the same core used by the UI |
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

## Launch evidence

1. Through CLI and through MCP, perform the same synthetic-site journey: open → inspect → fill/click → wait for an observable result → inspect/screenshot → close. Verify identical semantics and errors.
2. Drive actual Spaces/tabs/persistence through shared commands and verify resulting workspace/URL state.
3. Connect one real external MCP client with built-in AI off and no hosted myarc account. Record client/protocol/build versions and use synthetic page content.
4. Demonstrate denied cross-Profile/tab access, stale references, out-of-scope redirects, prompt-injected page instructions, oversized input, cancellation, disconnect and immediate Stop control.
5. Show the real browser's scope approval, control indicator and human takeover. Keep core checks separate from native browser/agent integration evidence.
