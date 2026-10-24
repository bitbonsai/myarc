# Optional AI and hosted services

## Product boundary

AI is off by default and never required for onboarding, importing, searching the web normally, restoring a session, or browsing. No background AI classification, page upload, model download, or inference before explicit enablement and action. An AI provider outage must have no effect on browser startup or local data access.

## Browser control is a separate core capability

[CLI and MCP control](CONTROL-PROTOCOL.md) are P0 launch requirements. A user can authorize an external agent to inspect/navigate pages, click/type/scroll, capture screenshots and manage browser state without enabling built-in AI or subscribing to myarc. Enabling external control is a separate deliberate grant: built-in AI being off must not prevent that choice or silently authorize it. The external client/model has its own data handling and provider costs.

The future built-in assistant uses the same granted control operations. Do not implement a privileged backdoor for hosted AI, make local/user-key agents second-class, or wait for a paid model gateway before building browser control. Start with tested local MCP clients; cloud-client connectivity requires a separate authenticated bridge.

## Provider model

| Mode | Setup | Data destination | Commercial relationship |
| --- | --- | --- | --- |
| Off | Default | None | None |
| Local | User selects a local model runtime/endpoint; explicit model-download consent if needed | Configured local process, subject to the runtime's own behavior | Core feature; no myarc subscription |
| User key | User selects supported provider/endpoint and enters their key | Clearly named provider, only for requested scope | User pays provider directly |
| Hosted myarc | Optional service account and paid allowance/credits | Disclosed gateway and named model provider | Convenience service with explicit limits |

The browser holds a common interface for capabilities, selected model, context scope, estimated cost, streaming, cancellation, errors, and usage. Adapters declare what they support. Do not pretend all “OpenAI-compatible” endpoints implement identical behavior; negotiate capabilities and test real adapters. No silent provider fallback, especially from local to cloud.

## First useful actions

After daily browsing is dependable, pilot built-in selected-text explanations, deliberate current-page summaries, and questions about specifically chosen tabs. Show which page/text will be sent before a first cloud request and whenever scope expands. External-agent page actions are already part of the P0 control scope; any built-in agent action must use that same grant/command path. Unscoped browser-wide access is not a default capability.

If an answer contains navigable URLs, show their destination and require a user action to open them. Page content and model output are untrusted. Do not let either alter permissions, read other Profiles, access local files, install Boosts/extensions, or execute browser commands without separately authorized capabilities.

## Privacy and secrets

Use OS credential storage for provider keys and service tokens. No secrets in workspace files, logs, telemetry, URL parameters, crash reports, or public fixtures. Exclude private browsing by default and require a separate deliberate choice for any private-context AI use. Profile boundaries apply to context assembly and conversation history.

Local mode can still use a separately configured runtime that has its own network behavior: display the configured endpoint accurately rather than making unsupported offline/privacy guarantees. BYOK requests should go directly to the provider when feasible; otherwise clearly disclose any proxy. Hosted retention/training rules must match actual provider contracts before launch.

## Hosted economics and billing

Start with a limited paid pilot after measuring real usage. Use a base subscription with a stated allowance and optional prepaid top-ups, or simple prepaid credits. Do not set final prices until costs are measured.

`contribution = collected revenue − payment fees − model usage − gateway/infrastructure − attributable support/refunds`

Measure input/output tokens, caching if supported, retries, cancellations, abuse, and expensive-tail users. Reserve an upper bound before starting an eligible request, settle actual usage, release unused reservations, and use idempotency keys. Server-side metering is authoritative; browser counters are informative. Set maximum request sizes, timeouts, per-account budgets, and abuse limits.

Show allowance remaining and any extra charge before it occurs. At a limit, offer stop/top-up/provider change; never silently bill overages. Cancelling or losing authentication disables only hosted requests. The user keeps local workspaces, browsing, export, local models, and user-key access.

## Later sync

Separate sync from AI and browser identity. Define encrypted data ownership, key recovery, conflicts, deletions, and independent export before implementing. Do not inherit Arc's cloud dependency while trying to replace it. No sync service or pricing is committed in this plan.
