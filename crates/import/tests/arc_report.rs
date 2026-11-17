use myarc_import::arc_markdown::{Reason, parse};
use myarc_import::arc_report::{Kind, Line, lines};
use myarc_import::arc_workspace::{apply, as_written};
use myarc_workspace::{MAX_FOLDER_NAME_BYTES, MAX_SPACE_NAME_BYTES, Workspace};

const FIXTURE: &str = include_str!("../../../tests/fixtures/arc-markdown/workspace.md");

/// The report after importing `text` into `workspace`, with HTTP(S) URLs as
/// written and every other scheme rejected.
fn import(text: &str, workspace: &mut Workspace) -> Vec<Line> {
    let report = parse(text.as_bytes()).expect("synthetic input parses");
    let canonical = as_written(&report);
    let plan = apply(&report, &canonical, workspace).expect("import succeeds");
    lines(&report, &canonical, &plan)
}

fn texts(lines: &[Line]) -> Vec<String> {
    lines.iter().map(Line::text).collect()
}

#[test]
fn fixture_reports_only_favorites() {
    let mut workspace = Workspace::default();
    let report = import(FIXTURE, &mut workspace);
    assert_eq!(texts(&report), vec!["Favorites added as pinned tabs: 13"]);
}

#[test]
fn reimport_reports_nothing() {
    let mut workspace = Workspace::default();
    import(FIXTURE, &mut workspace);
    assert!(import(FIXTURE, &mut workspace).is_empty());
}

#[test]
fn every_kind_is_named_with_its_count() {
    let long_space = "S".repeat(MAX_SPACE_NAME_BYTES + 1);
    let long_folder = "F".repeat(MAX_FOLDER_NAME_BYTES + 1);
    let text = format!(
        "- https://before.test/\n\
         # Desk\n\
         ## Favorites\n\
         - https://example.test/mail\n\
         - file:///etc/hosts\n\
         ## Reference\n\
         - https://docs.example.test/\n\
         - chrome://settings\n\
         \u{20} - https://nested.example.test/\n\
         ### Deeper\n\
         ## Reference\n\
         - https://docs.example.test/start\n\
         # Desk\n\
         ## {long_folder}\n\
         - https://long.example.test/\n\
         # {long_space}\n\
         ## Favorites\n\
         - not a link\n"
    );
    let mut workspace = Workspace::default();
    let report = import(&text, &mut workspace);
    assert_eq!(
        report,
        vec![
            Line {
                kind: Kind::NotWebLink,
                count: 2
            },
            Line {
                kind: Kind::UnreadableLine(Reason::Indented),
                count: 1
            },
            Line {
                kind: Kind::UnreadableLine(Reason::DeeperHeading),
                count: 1
            },
            Line {
                kind: Kind::UnreadableLine(Reason::EntryOutsideGroup),
                count: 1
            },
            Line {
                kind: Kind::UnreadableLine(Reason::NotUrl),
                count: 1
            },
            Line {
                kind: Kind::FavoriteAsPin,
                count: 1
            },
            Line {
                kind: Kind::SpaceNameShortened,
                count: 1
            },
            Line {
                kind: Kind::FolderNameShortened,
                count: 1
            },
            Line {
                kind: Kind::RepeatedSpace,
                count: 1
            },
            Line {
                kind: Kind::RepeatedFolder,
                count: 1
            },
        ]
    );
    assert_eq!(
        texts(&report),
        vec![
            "Links left out, not web pages: 2",
            "Lines left out, indented: 1",
            "Lines left out, nested heading: 1",
            "Lines left out, link before any group: 1",
            "Lines left out, not a link: 1",
            "Favorites added as pinned tabs: 1",
            "Space names shortened to fit: 1",
            "Folder names shortened to fit: 1",
            "Spaces merged into an earlier one with the same name: 1",
            "Folders merged into an earlier one with the same name: 1",
        ]
    );
}

#[test]
fn rejected_web_links_are_counted_apart_from_other_schemes() {
    let text = "# Desk\n## Reference\n- https://ok.test/\n- https://bad.test/\n- file:///x\n";
    let report = parse(text.as_bytes()).unwrap();
    // The browser rejects the second link, as it would a bad host.
    let canonical = vec![Some("https://ok.test/".to_owned()), None, None];
    let mut workspace = Workspace::default();
    let plan = apply(&report, &canonical, &mut workspace).unwrap();
    assert_eq!(
        texts(&lines(&report, &canonical, &plan)),
        vec![
            "Links left out, not web pages: 1",
            "Links left out, unreadable: 1",
        ]
    );
}
