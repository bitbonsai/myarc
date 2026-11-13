# Observed Markdown export shape

MA-003.a, inspected 2026-10-24. One user-supplied Arc export is held privately outside this repository. The user confirmed the built-in route, `Arc > Settings > Advanced > Export Data`, with a settings screenshot. Arc 1.170.0 was observed locally; the exported file and screenshot carry no producer-build or schema version marker. This is format reconnaissance, not importer acceptance.

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

The [reader tests](../../../crates/import/tests/arc_markdown.rs) parse this fixture and check those counts. The [mapping tests](../../../crates/import/tests/arc_workspace.rs) import it into a workspace, check every Space and Pin in order, and import it again to show nothing is added twice. Round trips back to Arc are untested. The next evidence needed is a known top-level pin versus favorite case and a nested-folder sample. Producing-build binding remains contextual rather than embedded in the file.

## Reader

MA-003.b added `myarc_import::arc_markdown::parse` in [crates/import](../../../crates/import/src/arc_markdown.rs). It turns export bytes into a report and writes nothing. It opens no tabs and fetches no URLs. Mapping into the workspace model is MA-006.

| Export line | Report field |
| --- | --- |
| `# Label` | `Space { label, line }`, in document order |
| `## Label` | `Group { label, line }` inside the latest Space; may be empty |
| `- scheme:rest` | `Entry { url, scheme, line }` inside the latest group, in order |
| Blank line | Skipped |
| Anything else | `Unrecognized { line, reason, excerpt }` |

Labels and URLs keep their exact text. `Favorites` is an ordinary group label. Repeated Spaces, groups and URLs all stay as separate records; counts of entries and distinct URL strings are available for a preview. The report has no Profile or title fields because the export has none.

Limits:

- Input over 8 MiB, or input that isn't UTF-8, fails as a whole with the size or the line of the first bad byte. A leading byte order mark and CRLF line endings are accepted.
- An entry must be one token shaped `scheme:rest` with no whitespace or control characters, at most 8192 bytes. The scheme is classified as `http`, `https` or another lowercased name. Host syntax is not checked, so the write path must still validate URLs with browser machinery.
- Labels must be non-empty, at most 1024 bytes and free of control characters.
- Indented lines, `###` and deeper headings, `*`, `+` and numbered lists, Markdown links and stray prose are reported with a reason instead of placed. Nesting is unobserved, so the reader does not guess at folders.
- Groups before any Space and entries before any group are reported, not attached elsewhere.
- Unrecognized excerpts keep at most 256 bytes of the line.
