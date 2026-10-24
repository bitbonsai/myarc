# Boosts

## Scope and sequencing

The basic mechanism—persisting site-scoped customization and applying CSS—is tractable. A dependable editor, selector picker, compatibility behavior, and safe JavaScript lifecycle are more work than injection alone. Deliver visible value early without treating the complete feature as trivial.

| Phase | Capability | Proof required |
| --- | --- | --- |
| B1: appearance | Site-scoped colors, type/scale, CSS, simple element hiding, preview, reset, disable | Works after navigation/restart; reversible; responsive and SPA fixtures; no Profile leakage |
| B2: editing polish | Selector picker, rule inspector, version history, local export/import | Keyboard-accessible UI; rules have a visible scope; broken selectors do not break the page |
| B3: executable customization | Optional JavaScript, only after a distinct permission and security review | Least privilege, isolation, revocation, clear scope, explicit per-device enablement; hostile fixture coverage |
| B4: sharing/AI assist | Inspectable sharing; optional AI drafting of CSS | No silent activation or remote code updates; readable diff and deliberate Apply; no AI dependency |

B1 is a beta target. B3/B4 do not block core browser launch. Exact Arc Boosts feature/platform behavior is only partially documented in the retrieved evidence; this is a myarc scope decision, not a full parity claim.

## Execution contract

A Boost has a stable ID, Profile scope, explicit allowed origins, name, version, enabled status, CSS, optional disabled-by-default script payload, and revision history. Do not grant wildcard/all-site access by default. Block internal browser pages and other privileged schemes. Define iframe, subdomain, private-browsing, and redirect behavior explicitly.

Apply styling through browser-supported mechanisms without creating a page-to-privileged-process bridge. CSS itself can trigger network requests and expose information: review/restrict remote resources, fonts, imports, and URL rules; do not describe CSS as risk-free. Imported/shared rules are inspectable and inactive until accepted.

JavaScript never receives browser credentials, arbitrary local filesystem access, or implicit cross-origin authority. Validate sender/frame/origin at privileged boundaries. A downloaded or synchronized script does not become trusted because another device or account enabled it. Revocation and a global “Disable Boosts” control must remain accessible if a page breaks.

AI-assisted editing uses the same optional provider controls and produces a visible draft. The user reviews a diff and applies it. AI cannot silently create, enable, widen, or update a rule.

## Acceptance examples

- Hide an element; reload, navigate within an SPA, and restore the previous page layout by disabling the Boost.
- Apply a type/color preset on one allowed origin; verify it does not affect a look-alike host, another Profile, browser settings, or private browsing by default.
- Recover from a malformed selector/rule or a site redesign without a browser crash or loss of settings.
- Import rules containing a script or remote CSS URL; verify preview/import does not execute/fetch it.
- Edit, undo, export, re-import, and inspect a rule's scope and version.
