# Command and automation contract

Status: proposed; there is no running command server, CLI, or MCP endpoint yet. This contract guides MA-004/007 and future tooling.

## Shared semantics

Workspace actions use stable command IDs and a versioned parameter schema. The native UI and command bar dispatch the same core operations used by a headless preservation CLI and integration checks. State reads do not mutate. Mutations validate ownership, Profile scope, input limits and authorization before committing.

Initial command inventory: `workspace.export`, `space.create`, `space.select`, `space.rename`, `node.move`, `tab.open`, `tab.pin`, `tab.archive`, `tab.restore`, `import.preview`, `import.commit`, `import.rollback`, `onboarding.advance`, and `onboarding.skip`. These names are proposed API design, not executable commands today. Browser-opening commands need platform adapters; core command tests need not navigate the network.

Every command records ID, schema/version, parameter definition, enabled/authorization conditions, mutation outcome/error, and whether idempotency or undo is supported. Do not apply a global “undo” promise to irreversible external page effects.

## Transport separation

Do not expose a debugging interface by default. If local automation is added, use a per-launch authenticated local channel, a dedicated synthetic test profile, strict message limits and explicit capability scope. Prefer no listener in ordinary builds; never bind an unauthenticated HTTP/TCP endpoint or enable permissive CORS. A website or AI response cannot acquire authority by naming a command.

Future MCP adapters must use a narrower permission surface than the internal test driver. Reading live page content, exporting a real workspace, changing credentials, installing extensions, and executing Boosts require independent policy decisions. The existence of an internal command does not automatically make it a remote tool.

## Evidence and reproducibility

Use deterministic synthetic workspaces and a controllable clock for archive/recovery cases. Capture semantic results alongside UI recordings so a smooth animation cannot hide a failed mutation. Redact real URLs/titles in logs. Add a harness command only when it executes a real check and fails honestly; no dummy parity or security scanner.
