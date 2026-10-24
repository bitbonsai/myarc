# Observed Markdown export shape

MA-003.a, inspected 2026-10-24. One user-supplied Arc export is held privately outside this repository. Its producing menu/action and build remain unverified. Arc 1.170.0 was observed locally, but the attachment itself identifies neither producer nor schema version. This is format reconnaissance, not importer acceptance.

[workspace.md](workspace.md) is an independently written counterpart. All labels and URLs are invented; hosts use reserved `.test` names. It preserves the observed layout and repetition without publishing personal browsing data. The source file and its fingerprint remain private. No URLs were opened during inspection.

## Observed fields

- First-level headings contain Space labels in document order.
- Second-level headings introduce named groups. `Favorites` repeats under multiple Spaces; another named group appears at the same heading level.
- List items contain bare HTTPS URLs, ordered within their group.
- Blank lines separate groups. The supplied sample decodes as UTF-8 and has no embedded version marker.

This shape is generic Markdown. It cannot authenticate Arc provenance or identify a supported producer version by itself. General Markdown syntax, nested groups and other export variants remain unexamined.

## Preservation boundary

A future reader can retain labels, grouping, URL text and document order. Repeated occurrences must survive until their ownership is resolved. Global URL deduplication could discard Space membership.

The meaning of `Favorites` remains unverified. The file does not distinguish Profile-level favorites from Space-level top-level pins, or provide Profile identities. Repeated groups cannot establish that Spaces share a Profile. Ask for explicit mapping rather than creating one Profile per Space or merging equal groups automatically.

Tab titles, stable item IDs, canonical-versus-current URL distinctions and explicit object types are absent. Themes, timestamps, archives, sessions, extension inventory and Boost data are also absent. Missing data must be reported rather than reconstructed through network requests. Folder nesting and executable-data handling need separate samples.

## Fixture checks

The synthetic counterpart has four first-level headings, five second-level groups, 19 URL occurrences and ten distinct URLs. Its first `Favorites` group has one extra URL, preserving an ownership ambiguity worth testing. The other groups share three URL strings. This proves textual repetition, not actual Profile relationships.

No production parser, conversion or round trip has been implemented or tested. The next evidence needed is the exact export action, a known top-level pin versus favorite case, and a nested-folder sample.
