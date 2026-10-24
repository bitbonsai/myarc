# Security and maintenance plan

This document defines work required before supporting daily browsing. It is not a security audit or a claim that the planning repository contains a safe browser.

## Main boundaries

| Boundary | Failure to prevent | Required control and verification |
| --- | --- | --- |
| Web page → browser UI/commands | Untrusted content controlling privileged browser state | Preserve upstream sandbox/process isolation; narrow authenticated IPC; validate caller origin/frame, schema, and capability |
| Arc/local import → workspace | Corruption, resource exhaustion, source modification, script execution | Read-only snapshot, bounded parsing, safe schemes, transactional commit, explicit unsupported errors, fuzz/fault checks |
| Space/Profile/private window → another context | Cookie/history/search/AI leakage or private persistence | Use native browser Profiles; scope caches/indexes; separate private runtime state; integration checks |
| Boost/share → web page/privileged process | Excess permissions, hidden network leaks, silently activated script | Narrow origins, inspectable disabled import, resource controls, no privileged bridge, revocation |
| Model/page text → tools | Prompt-injected permissions or actions | Treat content/output as data; no ambient automation authority; explicit capability checks |
| Release/update → installed application | Supply-chain compromise, unsigned update, unsafe downgrade | Isolated signing, reviewed dependencies, signed metadata/artifacts, verification and recovery |
| Automation → browsing session | A control endpoint exceeding the context the user granted | Opt-in controller grants; dedicated Profile by default or deliberately selected normal tabs; authenticated local IPC; scope/expiry/revocation, indicator and Stop control; no unauthenticated listener |

## Maintenance is product scope

M0 must name the person responsible for upstream security triage, primary and backup build access, signing/update recovery, and responding when coverage is unavailable. Fund build machines, artifact hosting, notarization/signing, and review. Estimated costs remain unknown until measured.

Monitor upstream stable/security channels, pin revisions, track patch age, and practice a forward update across two revisions before release. Keep the patch set small and record why each patch exists. Do not use sandbox-disabling flags as a release fix. A missed security target triggers triage, a visible status update, and suspension of new-user promotion until a safe build is available.

The proposed urgent-update budget is in [SCORECARD.md](SCORECARD.md). It becomes a public commitment only when the actual owner and release path can support it. Browser updates can use the network without a user account; local browsing must remain independent of a hosted login or license check.

## Release checklist

- Record upstream revision, patches, dependencies, licenses, known vulnerabilities, compatibility limitations, and supported OS/architecture.
- Verify signing, notarization, install, first run, migration, update, rollback/recovery behavior, and uninstall on a clean machine.
- Keep signing secrets outside the repository and ordinary test jobs. Redact private URLs, tokens, profile databases, and AI content from logs.
- Test malicious import inputs, internal URL schemes, origin/permission confusion, executable Boost import, and hostile automation messages.
- Make backup/export and failure recovery understandable before people trust the browser with daily work.

Add a private vulnerability-reporting channel when repository ownership and an actively monitored destination are established. Do not solicit private security reports in public issues or invent a contact address.
