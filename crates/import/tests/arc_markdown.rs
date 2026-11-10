use myarc_import::arc_markdown::{
    Entry, MAX_EXCERPT_BYTES, MAX_INPUT_BYTES, MAX_LABEL_BYTES, MAX_URL_BYTES, ParseError, Reason,
    Report, Scheme, parse,
};

const FIXTURE: &str = include_str!("../../../tests/fixtures/arc-markdown/workspace.md");

fn parse_str(text: &str) -> Report {
    parse(text.as_bytes()).expect("synthetic input parses")
}

fn reasons(report: &Report) -> Vec<(usize, Reason)> {
    report
        .unrecognized
        .iter()
        .map(|item| (item.line, item.reason))
        .collect()
}

fn urls(entries: &[Entry]) -> Vec<&str> {
    entries.iter().map(|entry| entry.url.as_str()).collect()
}

#[test]
fn fixture_matches_documented_counts() {
    let report = parse_str(FIXTURE);
    assert!(report.unrecognized.is_empty(), "{:?}", report.unrecognized);
    let labels: Vec<_> = report.spaces.iter().map(|s| s.label.as_str()).collect();
    assert_eq!(labels, ["Desk", "Research", "Reading", "Quiet"]);
    assert_eq!(report.group_count(), 5);
    assert_eq!(report.entry_count(), 19);
    assert_eq!(report.distinct_url_count(), 10);
}

#[test]
fn fixture_keeps_order_lines_and_repeated_favorites() {
    let report = parse_str(FIXTURE);
    let desk = &report.spaces[0];
    assert_eq!(desk.line, 1);
    let groups: Vec<_> = desk.groups.iter().map(|g| g.label.as_str()).collect();
    assert_eq!(groups, ["Favorites", "Reference"]);
    assert_eq!(desk.groups[0].line, 3);
    assert_eq!(
        urls(&desk.groups[0].entries),
        [
            "https://example.test/mail",
            "https://example.test/code",
            "https://example.test/status",
            "https://example.test/space-only",
        ]
    );
    assert_eq!(desk.groups[0].entries[0].line, 4);
    assert_eq!(desk.groups[0].entries[0].scheme, Scheme::Https);
    for space in &report.spaces[1..] {
        assert_eq!(space.groups.len(), 1);
        assert_eq!(space.groups[0].label, "Favorites");
        assert_eq!(space.groups[0].entries.len(), 3);
    }
}

#[test]
fn favorites_is_only_a_label() {
    let a = parse_str("# A\n\n## Favorites\n- https://a.test/\n");
    let b = parse_str("# A\n\n## Other\n- https://a.test/\n");
    assert_eq!(a.spaces[0].groups[0].entries, b.spaces[0].groups[0].entries);
    assert_eq!(a.spaces[0].groups[0].label, "Favorites");
}

#[test]
fn empty_groups_and_spaces_survive() {
    let report = parse_str("# Empty\n\n# One\n\n## None\n\n## Some\n- https://a.test/\n");
    assert!(report.unrecognized.is_empty());
    assert!(report.spaces[0].groups.is_empty());
    assert!(report.spaces[1].groups[0].entries.is_empty());
    assert_eq!(report.spaces[1].groups[1].entries.len(), 1);
}

#[test]
fn duplicates_are_kept_everywhere() {
    let report = parse_str(
        "# A\n## G\n- https://a.test/\n- https://a.test/\n## G\n- https://a.test/\n# A\n## G\n- https://a.test/\n",
    );
    assert_eq!(report.spaces.len(), 2);
    assert_eq!(report.group_count(), 3);
    assert_eq!(report.entry_count(), 4);
    assert_eq!(report.distinct_url_count(), 1);
}

#[test]
fn crlf_bom_and_trailing_whitespace_parse_like_lf() {
    let crlf = format!("\u{feff}{}", FIXTURE.replace('\n', " \t\r\n"));
    assert_eq!(parse(crlf.as_bytes()).unwrap(), parse_str(FIXTURE));
}

#[test]
fn indented_and_nested_lines_are_reported() {
    let report = parse_str(
        "# A\n## G\n- https://a.test/\n  - https://nested.test/\n\t- https://tab.test/\n ## Indented\n",
    );
    assert_eq!(
        reasons(&report),
        [
            (4, Reason::Indented),
            (5, Reason::Indented),
            (6, Reason::Indented)
        ]
    );
    assert_eq!(report.entry_count(), 1);
}

#[test]
fn deeper_headings_and_other_markdown_are_reported() {
    let report = parse_str(
        "Intro text\n# A\n## G\n### Folder\n* https://star.test/\n+ https://plus.test/\n1. https://num.test/\n#NoSpace\n> quote\n---\n",
    );
    assert_eq!(
        reasons(&report),
        [
            (1, Reason::Other),
            (4, Reason::DeeperHeading),
            (5, Reason::Other),
            (6, Reason::Other),
            (7, Reason::Other),
            (8, Reason::Other),
            (9, Reason::Other),
            (10, Reason::Other),
        ]
    );
    assert_eq!(report.unrecognized[3].excerpt, "+ https://plus.test/");
}

#[test]
fn non_url_items_are_reported() {
    let report = parse_str(
        "# A\n## G\n- plain words\n- [Title](https://link.test/)\n- example.test/no-scheme\n- https:\n- 1http://x.test/\n-\n- https://ok.test/\n",
    );
    assert_eq!(
        reasons(&report),
        [
            (3, Reason::NotUrl),
            (4, Reason::NotUrl),
            (5, Reason::NotUrl),
            (6, Reason::NotUrl),
            (7, Reason::NotUrl),
            (8, Reason::NotUrl),
        ]
    );
    assert_eq!(
        urls(&report.spaces[0].groups[0].entries),
        ["https://ok.test/"]
    );
}

#[test]
fn schemes_are_classified_without_rewriting() {
    let report = parse_str(
        "# A\n## G\n- HTTP://Upper.test/Path\n- https://a.test/\n- file:///tmp/x\n- javascript:alert(1)\n- about:blank\n",
    );
    let entries = &report.spaces[0].groups[0].entries;
    assert_eq!(entries[0].url, "HTTP://Upper.test/Path");
    let schemes: Vec<_> = entries.iter().map(|e| e.scheme.clone()).collect();
    assert_eq!(
        schemes,
        [
            Scheme::Http,
            Scheme::Https,
            Scheme::Other("file".into()),
            Scheme::Other("javascript".into()),
            Scheme::Other("about".into()),
        ]
    );
}

#[test]
fn structure_out_of_place_is_reported() {
    let report = parse_str("- https://early.test/\n## Orphan\n# A\n- https://no-group.test/\n");
    assert_eq!(
        reasons(&report),
        [
            (1, Reason::EntryOutsideGroup),
            (2, Reason::GroupOutsideSpace),
            (4, Reason::EntryOutsideGroup),
        ]
    );
    assert_eq!(report.spaces.len(), 1);
}

#[test]
fn labels_are_bounded_and_checked() {
    let long = "x".repeat(MAX_LABEL_BYTES + 1);
    let fits = "é".repeat(MAX_LABEL_BYTES / 2);
    let text = format!("#\n# \u{7}bell\n# {long}\n# {fits}\n## \n## Space Ünïcode  ✓\n");
    let report = parse_str(&text);
    assert_eq!(
        reasons(&report),
        [
            (1, Reason::EmptyLabel),
            (2, Reason::ControlCharacter),
            (3, Reason::LabelTooLong),
            (5, Reason::EmptyLabel),
        ]
    );
    assert_eq!(report.spaces[0].label, fits);
    assert_eq!(report.spaces[0].groups[0].label, "Space Ünïcode  ✓");
    assert_eq!(report.unrecognized[2].excerpt.len(), MAX_EXCERPT_BYTES);
}

#[test]
fn long_urls_are_reported_with_short_excerpts() {
    let fits = format!("https://a.test/{}", "p".repeat(MAX_URL_BYTES - 15));
    let over = format!("https://a.test/{}", "é".repeat(MAX_URL_BYTES));
    let report = parse_str(&format!("# A\n## G\n- {fits}\n- {over}\n"));
    assert_eq!(
        report.spaces[0].groups[0].entries[0].url.len(),
        MAX_URL_BYTES
    );
    assert_eq!(reasons(&report), [(4, Reason::UrlTooLong)]);
    let excerpt = &report.unrecognized[0].excerpt;
    assert!(excerpt.len() <= MAX_EXCERPT_BYTES && excerpt.starts_with("- https://a.test/"));
}

#[test]
fn oversized_input_is_rejected_before_parsing() {
    let input = vec![b'#'; MAX_INPUT_BYTES + 1];
    assert_eq!(
        parse(&input),
        Err(ParseError::TooLarge {
            len: MAX_INPUT_BYTES + 1,
            max: MAX_INPUT_BYTES,
        })
    );
    assert!(parse(&vec![b'\n'; MAX_INPUT_BYTES]).is_ok());
}

#[test]
fn non_utf8_input_names_the_line() {
    assert_eq!(
        parse(b"# A\n## G\n- https://a.test/\xff\n"),
        Err(ParseError::NotUtf8 { line: 3 })
    );
    assert_eq!(parse(b"").unwrap(), Report::default());
}

/// Deterministic xorshift so failures reproduce without a dependency.
struct Rng(u64);

impl Rng {
    fn step(&mut self) -> u64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        self.0
    }

    fn below(&mut self, n: usize) -> usize {
        (self.step() % n as u64) as usize
    }
}

fn check_invariants(input: &[u8]) {
    let Ok(report) = parse(input) else { return };
    let lines = input.iter().filter(|&&b| b == b'\n').count() + 1;
    let mut seen = Vec::new();
    for space in &report.spaces {
        seen.push(space.line);
        for group in &space.groups {
            seen.push(group.line);
            seen.extend(group.entries.iter().map(|e| e.line));
        }
    }
    seen.extend(report.unrecognized.iter().map(|u| u.line));
    assert!(seen.iter().all(|&line| (1..=lines).contains(&line)));
    let unique = seen.len();
    seen.sort_unstable();
    seen.dedup();
    assert_eq!(seen.len(), unique, "a line was placed twice");
    assert!(
        report
            .unrecognized
            .iter()
            .all(|u| u.excerpt.len() <= MAX_EXCERPT_BYTES)
    );
}

#[test]
fn arbitrary_input_never_panics() {
    const PIECES: &[&str] = &[
        "#",
        "##",
        "###",
        "-",
        "- ",
        " ",
        "\t",
        "\r",
        "\n",
        "\r\n",
        "https://a.test/",
        "x:",
        ":",
        "é",
        "\u{feff}",
        "\u{0}",
        "Favorites",
        "[t](u)",
        "*",
        "\u{2028}",
    ];
    let mut rng = Rng(0x9e37_79b9_7f4a_7c15);
    for _ in 0..2000 {
        let len = rng.below(200);
        let bytes: Vec<u8> = (0..len).map(|_| rng.step() as u8).collect();
        check_invariants(&bytes);

        let mut text = String::new();
        for _ in 0..rng.below(60) {
            text.push_str(PIECES[rng.below(PIECES.len())]);
        }
        check_invariants(text.as_bytes());
    }
}
