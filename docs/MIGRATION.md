# Arc migration and portable ownership

## Priority

Preserving workspace structure is the first engineering deliverable after feasibility. Existing users need a recoverable copy before losing access to Arc. Do not wait for a browser release to make export/conversion useful.

Arc advertises exports and its own migration to Dia. This does **not** establish that a third-party export contains every field or that myarc can read it. Supported producer versions and complete coverage remain unverified. One consented Markdown sample now has a [structure inventory and independent fixture](../tests/fixtures/arc-markdown/README.md). It contains Space labels and grouped URL lists from a user-confirmed built-in export action; `Favorites` scope still needs confirmation. The file lacks Profile identities and tab titles, so neither can be restored from this sample alone. A production importer remains unimplemented.

## Import order

1. Inspect user-produced official exports first, with consent and minimal local access.
2. Add a read-only local profile adapter only for unsupported fields with a documented need, version signature, permission flow, and safe snapshot procedure.
3. Offer bookmark HTML as a clearly lower-fidelity fallback. Do not call it a complete Arc workspace import.

Prefer a user-selected directory/file and OS-native access prompts. Never crawl unrelated profiles, copy locked databases inconsistently, or request Arc credentials. A live profile read needs a consistent snapshot; asking the user to close Arc is acceptable.

## Coverage contract

| Data | First migration goal | Initial truth / boundary |
| --- | --- | --- |
| Spaces, folders, hierarchy, order, labels | Preserve with stable target IDs and mapping | Needs real versioned sample/export evidence |
| Profile mapping | Preserve distinction and many-Spaces-to-one-Profile relationship | Do not equate each Space with an isolated Profile |
| Favorites and pinned URLs/names | Preserve scope, ordering, canonical pinned URL | Verify reset/navigation semantics separately |
| Temporary tabs and archive | Preserve supported entries/timestamps | Missing timestamps or archive data reported explicitly |
| Themes and preferences | Map settings with equivalent behavior | Unsupported settings remain listed; do not silently approximate |
| Extensions | Inventory IDs and offer supported reinstallation | Installation/data/native-host transfer is not assumed |
| History | Separate opt-in adapter if feasible | Not needed for initial preservation utility |
| Passwords, cookies, logged-in sessions, payment data | Explicit later adapter investigation | No initial transfer promise, no plaintext copies or security bypasses |
| Boosts | Inventory and later import compatible appearance/CSS | Imported executable code always disabled until deliberately reviewed/enabled |
| Easels and unsupported objects | Report omission | Easels remain outside product scope |

## Transaction

`select → read snapshot → detect format/version → validate → normalize → preview → confirm → commit → report`

The preview shows counts, hierarchy, profile mapping, warnings, duplicates, and unsupported categories. It does not fetch imported URLs or remote favicons. Give users control over what to include. Reject malformed structures and unsupported future versions; bound depth, counts, sizes, and processing time.

Write into a new target workspace generation with an import receipt, source fingerprint, and idempotency key. Either publish the complete validated generation or leave the prior workspace unchanged. Cancellation, disk-full, process termination, and validation failure cannot produce a half-imported visible workspace. Never modify the source.

Re-importing the same source is detectable and cannot silently duplicate it. An undo/rollback action identifies changes from that receipt; avoid deleting subsequent user work. Keep receipts free of credentials and minimize retained source details.

## Portable export

Define a documented, versioned myarc workspace JSON format independent of a hosted service. Include hierarchy, ordering, canonical URLs, profile references, supported preferences, and safe import metadata. Separate executable Boost code and sensitive browser data with explicit scope/disclosure. Export/import round trips must preserve supported semantics; bookmark HTML is an additional interoperable escape hatch.

Exports contain private URLs and titles even without passwords. Save only to the user's chosen destination. Public test fixtures must be synthetic; no raw user exports, cookies, databases, or private screenshots in this repository.

## Evidence to collect

- Record platform, Arc version, export route, file signature, and observed fields from independently consented samples. Keep personal bytes private.
- Create synthetic equivalents with nested folders, Unicode/RTL labels, duplicate URLs, multiple Spaces sharing a Profile, separate Profiles, empty Spaces, broken references, deep/cyclic structures, and large counts.
- Compare semantic summaries before and after round trip. Show every unsupported field and its handling.
- Fault-inject cancellation, restart, permission denial, disk full, truncated input, and duplicate import. Demonstrate source hashes unchanged.
- Record precisely which extensions require reinstallation and which data remains behind. Imported local JavaScript never executes during preview, import, or reveal.
