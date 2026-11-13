use myarc_import::arc_markdown::{Report, parse};
use myarc_import::arc_workspace::{ImportError, Skipped, apply, as_written, entry_urls, preview};
use myarc_workspace::{
    DEFAULT_PROFILE_ID, MAX_ITEMS, MAX_SPACES, Placement, Workspace, WorkspaceError,
};

const FIXTURE: &str = include_str!("../../../tests/fixtures/arc-markdown/workspace.md");

fn report(text: &str) -> Report {
    parse(text.as_bytes()).expect("synthetic input parses")
}

fn import(text: &str, workspace: &mut Workspace) -> Result<(), ImportError> {
    let report = report(text);
    apply(&report, &as_written(&report), workspace).map(|_| ())
}

/// Space names in order, each with its Pin URLs in sidebar order.
fn layout(workspace: &Workspace) -> Vec<(String, Vec<String>)> {
    let state = workspace.snapshot();
    state
        .spaces
        .iter()
        .map(|space| {
            let pins = state
                .items
                .iter()
                .filter(|item| item.placement == Placement::Pin { space_id: space.id })
                .map(|item| item.canonical_url.clone().unwrap_or_default())
                .collect();
            (space.name.clone(), pins)
        })
        .collect()
}

fn strings(urls: &[&str]) -> Vec<String> {
    urls.iter().map(|url| (*url).to_owned()).collect()
}

#[test]
fn fixture_becomes_spaces_and_pins_in_arc_order() {
    let mut workspace = Workspace::default();
    let report = report(FIXTURE);
    let applied = apply(&report, &as_written(&report), &mut workspace).unwrap();
    assert_eq!(applied.new_space_count(), 4);
    assert_eq!(applied.new_pin_count(), 19);
    assert_eq!(applied.kept_pin_count(), 0);
    assert_eq!(applied.flattened_groups, 5);
    assert!(applied.skipped.is_empty());

    let favorites = [
        "https://example.test/mail",
        "https://example.test/code",
        "https://example.test/status",
    ];
    let mut desk = strings(&favorites);
    desk.push("https://example.test/space-only".into());
    desk.extend(strings(&[
        "https://docs.example.test/",
        "https://docs.example.test/start",
        "https://docs.example.test/keyboard",
        "https://video.example.test/",
        "https://notes.example.test/",
        "https://news.example.test/",
    ]));
    assert_eq!(
        layout(&workspace),
        [
            ("Everyday".to_owned(), vec![]),
            ("Desk".to_owned(), desk),
            ("Research".to_owned(), strings(&favorites)),
            ("Reading".to_owned(), strings(&favorites)),
            ("Quiet".to_owned(), strings(&favorites)),
        ]
    );
    // Favorites stay Space Pins: the export does not mark Profile favorites.
    let state = workspace.snapshot();
    assert!(
        !state
            .items
            .iter()
            .any(|item| matches!(item.placement, Placement::Favorite { .. }))
    );
}

#[test]
fn import_leaves_windows_and_existing_spaces_alone() {
    let mut workspace = Workspace::default();
    let before = workspace.snapshot();
    import(FIXTURE, &mut workspace).unwrap();
    let after = workspace.snapshot();
    assert_eq!(after.windows, before.windows);
    assert_eq!(after.spaces[0], before.spaces[0]);
}

#[test]
fn importing_twice_adds_nothing_the_second_time() {
    let mut workspace = Workspace::default();
    import(FIXTURE, &mut workspace).unwrap();
    let once = workspace.snapshot();

    let report = report(FIXTURE);
    let second = preview(&report, &as_written(&report), &workspace).unwrap();
    assert!(second.is_noop());
    assert_eq!(second.new_space_count(), 0);
    assert_eq!(second.kept_pin_count(), 19);
    assert!(second.spaces.iter().all(|space| space.existing.is_some()));

    apply(&report, &as_written(&report), &mut workspace).unwrap();
    assert_eq!(workspace.snapshot(), once);
}

#[test]
fn reimport_adds_only_what_is_new_and_keeps_user_pins() {
    let mut workspace = Workspace::default();
    import("# Desk\n## Favorites\n- https://a.test/\n", &mut workspace).unwrap();
    let desk = workspace.snapshot().spaces[1].id;
    // A Pin the user added by hand after the first import.
    let mine = workspace.add_temporary_item(desk).unwrap();
    workspace
        .set_placement(
            mine,
            Placement::Pin { space_id: desk },
            Some("https://mine.test/"),
        )
        .unwrap();

    let text = "# Desk\n## Favorites\n- https://a.test/\n- https://b.test/\n# New\n## G\n- https://c.test/\n";
    let report = report(text);
    let plan = preview(&report, &as_written(&report), &workspace).unwrap();
    assert_eq!(plan.new_space_count(), 1);
    assert_eq!(plan.new_pin_count(), 2);
    assert_eq!(plan.kept_pin_count(), 1);
    apply(&report, &as_written(&report), &mut workspace).unwrap();
    assert_eq!(
        layout(&workspace),
        [
            ("Everyday".to_owned(), vec![]),
            (
                "Desk".to_owned(),
                strings(&["https://a.test/", "https://mine.test/", "https://b.test/"])
            ),
            ("New".to_owned(), strings(&["https://c.test/"])),
        ]
    );
}

#[test]
fn repeated_urls_in_one_space_survive_and_stay_idempotent() {
    let text = "# Desk\n## A\n- https://a.test/\n## B\n- https://a.test/\n";
    let mut workspace = Workspace::default();
    import(text, &mut workspace).unwrap();
    import(text, &mut workspace).unwrap();
    assert_eq!(
        layout(&workspace)[1],
        (
            "Desk".to_owned(),
            strings(&["https://a.test/", "https://a.test/"])
        )
    );
}

#[test]
fn spaces_sharing_a_name_merge_into_one() {
    let text = "# Desk\n## A\n- https://a.test/\n# Desk\n## B\n- https://b.test/\n";
    let mut workspace = Workspace::default();
    import(text, &mut workspace).unwrap();
    assert_eq!(
        layout(&workspace)[1..],
        [(
            "Desk".to_owned(),
            strings(&["https://a.test/", "https://b.test/"])
        )]
    );
}

#[test]
fn empty_spaces_are_created_and_not_duplicated() {
    let mut workspace = Workspace::default();
    import("# Empty\n", &mut workspace).unwrap();
    import("# Empty\n", &mut workspace).unwrap();
    assert_eq!(
        layout(&workspace),
        [
            ("Everyday".to_owned(), vec![]),
            ("Empty".to_owned(), vec![])
        ]
    );
}

#[test]
fn rejected_and_non_web_urls_are_skipped_with_their_line() {
    let text = "# Desk\n## A\n- file:///tmp/x\n- https://ok.test/\n- https://bad.test/\n";
    let report = report(text);
    assert_eq!(
        entry_urls(&report),
        ["file:///tmp/x", "https://ok.test/", "https://bad.test/"]
    );
    // The browser canonicalizes; here it rejects the third entry.
    let canonical = vec![None, Some("https://ok.test/".to_owned()), None];
    let mut workspace = Workspace::default();
    let applied = apply(&report, &canonical, &mut workspace).unwrap();
    assert_eq!(
        applied.skipped,
        [
            Skipped {
                line: 3,
                url: "file:///tmp/x".into()
            },
            Skipped {
                line: 5,
                url: "https://bad.test/".into()
            },
        ]
    );
    assert_eq!(layout(&workspace)[1].1, strings(&["https://ok.test/"]));
}

#[test]
fn canonical_urls_drive_matching() {
    let text = "# Desk\n## A\n- https://A.test\n";
    let report = report(text);
    let canonical = vec![Some("https://a.test/".to_owned())];
    let mut workspace = Workspace::default();
    apply(&report, &canonical, &mut workspace).unwrap();
    let again = preview(&report, &canonical, &workspace).unwrap();
    assert!(again.is_noop());
    assert_eq!(layout(&workspace)[1].1, strings(&["https://a.test/"]));
}

#[test]
fn agent_space_is_never_reused() {
    let mut workspace = Workspace::default();
    let agent = workspace.ensure_agent_space(DEFAULT_PROFILE_ID).unwrap();
    import("# Agent\n## A\n- https://a.test/\n", &mut workspace).unwrap();
    let state = workspace.snapshot();
    assert_eq!(state.spaces.len(), 3);
    assert!(
        !state
            .items
            .iter()
            .any(|item| item.placement == Placement::Pin { space_id: agent })
    );
}

#[test]
fn long_labels_are_cut_to_the_space_name_limit() {
    let label = "é".repeat(100);
    let mut workspace = Workspace::default();
    import(&format!("# {label}\n"), &mut workspace).unwrap();
    import(&format!("# {label}\n"), &mut workspace).unwrap();
    let state = workspace.snapshot();
    assert_eq!(state.spaces.len(), 2);
    assert_eq!(state.spaces[1].name, "é".repeat(64));
}

#[test]
fn too_many_spaces_fails_and_changes_nothing() {
    let text: String = (0..MAX_SPACES).map(|i| format!("# S{i}\n")).collect();
    let mut workspace = Workspace::default();
    let before = workspace.snapshot();
    let report = report(&text);
    let error = preview(&report, &as_written(&report), &workspace).unwrap_err();
    assert_eq!(
        error,
        ImportError::Workspace(WorkspaceError::LimitExceeded("Spaces"))
    );
    let error = apply(&report, &as_written(&report), &mut workspace).unwrap_err();
    assert_eq!(
        error,
        ImportError::Workspace(WorkspaceError::LimitExceeded("Spaces"))
    );
    assert_eq!(workspace.snapshot(), before);
}

#[test]
fn too_many_pins_fails_and_changes_nothing() {
    let mut text = String::from("# Desk\n## A\n");
    for i in 0..=MAX_ITEMS {
        text.push_str(&format!("- https://a.test/{i}\n"));
    }
    let mut workspace = Workspace::default();
    let before = workspace.snapshot();
    let error = import(&text, &mut workspace).unwrap_err();
    assert_eq!(
        error,
        ImportError::Workspace(WorkspaceError::LimitExceeded("items"))
    );
    assert_eq!(workspace.snapshot(), before);
}

#[test]
fn url_list_must_match_the_entries() {
    let report = report(FIXTURE);
    let error = preview(&report, &[], &Workspace::default()).unwrap_err();
    assert_eq!(
        error,
        ImportError::UrlCountMismatch {
            expected: 19,
            got: 0
        }
    );
}

#[test]
fn empty_export_is_a_noop() {
    let mut workspace = Workspace::default();
    let before = workspace.snapshot();
    let report = report("");
    let applied = apply(&report, &[], &mut workspace).unwrap();
    assert!(applied.is_noop());
    assert_eq!(workspace.snapshot(), before);
}
