use myarc_import::arc_markdown::{Report, parse};
use myarc_import::arc_workspace::{
    FolderAddition, ImportError, Skipped, apply, as_written, entry_urls, preview,
};
use myarc_workspace::{
    DEFAULT_PROFILE_ID, MAX_FOLDERS, MAX_ITEMS, MAX_SPACES, Placement, Workspace, WorkspaceError,
};

const FIXTURE: &str = include_str!("../../../tests/fixtures/arc-markdown/workspace.md");

fn report(text: &str) -> Report {
    parse(text.as_bytes()).expect("synthetic input parses")
}

fn import(text: &str, workspace: &mut Workspace) -> Result<(), ImportError> {
    let report = report(text);
    apply(&report, &as_written(&report), workspace).map(|_| ())
}

/// Each Space's folders in order, with their Pin URLs in folder order.
fn folders(workspace: &Workspace) -> Vec<(String, String, Vec<String>)> {
    let state = workspace.snapshot();
    state
        .folders
        .iter()
        .map(|folder| {
            let space = state
                .spaces
                .iter()
                .find(|space| space.id == folder.space_id)
                .unwrap();
            let urls = folder
                .items
                .iter()
                .map(|id| {
                    let item = state.items.iter().find(|item| item.id == *id).unwrap();
                    item.canonical_url.clone().unwrap()
                })
                .collect();
            (space.name.clone(), folder.name.clone(), urls)
        })
        .collect()
}

/// Space names in order, each with its ungrouped Pin URLs in sidebar order.
fn ungrouped(workspace: &Workspace) -> Vec<(String, Vec<String>)> {
    let state = workspace.snapshot();
    state
        .spaces
        .iter()
        .map(|space| {
            let pins = state
                .items
                .iter()
                .filter(|item| item.placement == Placement::Pin { space_id: space.id })
                .filter(|item| workspace.folder_of(item.id).is_none())
                .map(|item| item.canonical_url.clone().unwrap_or_default())
                .collect();
            (space.name.clone(), pins)
        })
        .collect()
}

/// Space names in order, each with all its Pin URLs in workspace order.
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
fn fixture_becomes_spaces_folders_and_pins_in_arc_order() {
    let mut workspace = Workspace::default();
    let report = report(FIXTURE);
    let applied = apply(&report, &as_written(&report), &mut workspace).unwrap();
    assert_eq!(applied.new_space_count(), 4);
    assert_eq!(applied.new_folder_count(), 1);
    assert_eq!(applied.new_pin_count(), 19);
    assert_eq!(applied.moved_pin_count(), 0);
    assert_eq!(applied.kept_pin_count(), 0);
    assert!(applied.skipped.is_empty());

    let favorites = [
        "https://example.test/mail",
        "https://example.test/code",
        "https://example.test/status",
    ];
    let mut desk = strings(&favorites);
    desk.push("https://example.test/space-only".into());
    // Favorites stay ungrouped Pins, the sidebar's tiles.
    assert_eq!(
        ungrouped(&workspace),
        [
            ("Everyday".to_owned(), vec![]),
            ("Desk".to_owned(), desk),
            ("Research".to_owned(), strings(&favorites)),
            ("Reading".to_owned(), strings(&favorites)),
            ("Quiet".to_owned(), strings(&favorites)),
        ]
    );
    assert_eq!(
        folders(&workspace),
        [(
            "Desk".to_owned(),
            "Reference".to_owned(),
            strings(&[
                "https://docs.example.test/",
                "https://docs.example.test/start",
                "https://docs.example.test/keyboard",
                "https://video.example.test/",
                "https://notes.example.test/",
                "https://news.example.test/",
            ])
        )]
    );
    assert!(workspace.folders()[0].open);
    // The export does not mark Profile favorites, so none are made.
    let state = workspace.snapshot();
    assert!(
        !state
            .items
            .iter()
            .any(|item| matches!(item.placement, Placement::Favorite { .. }))
    );
}

#[test]
fn groups_become_folders_in_export_order_and_empty_ones_stay() {
    let text = "# Desk\n## Work\n- https://w.test/\n## Empty\n## Play\n- https://p.test/\n- https://q.test/\n";
    let mut workspace = Workspace::default();
    import(text, &mut workspace).unwrap();
    assert_eq!(
        folders(&workspace),
        [
            ("Desk".into(), "Work".into(), strings(&["https://w.test/"])),
            ("Desk".into(), "Empty".into(), vec![]),
            (
                "Desk".into(),
                "Play".into(),
                strings(&["https://p.test/", "https://q.test/"])
            ),
        ]
    );
    assert_eq!(ungrouped(&workspace)[1].1, Vec::<String>::new());
}

#[test]
fn groups_sharing_a_name_in_a_space_fill_one_folder() {
    let text = "# Desk\n## A\n- https://a.test/\n# Desk\n## A\n- https://b.test/\n# Other\n## A\n- https://c.test/\n";
    let mut workspace = Workspace::default();
    import(text, &mut workspace).unwrap();
    assert_eq!(
        folders(&workspace),
        [
            (
                "Desk".into(),
                "A".into(),
                strings(&["https://a.test/", "https://b.test/"])
            ),
            ("Other".into(), "A".into(), strings(&["https://c.test/"])),
        ]
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

#[test]
fn reimport_matches_folders_by_name_and_adds_only_missing_pins() {
    let mut workspace = Workspace::default();
    import(
        "# Desk\n## Work\n- https://a.test/\n- https://b.test/\n",
        &mut workspace,
    )
    .unwrap();
    let work = workspace.folders()[0].id;
    workspace.set_folder_open(work, false).unwrap();
    let text = "# Desk\n## Work\n- https://a.test/\n- https://c.test/\n- https://b.test/\n## New\n- https://d.test/\n";
    let report = report(text);
    let plan = preview(&report, &as_written(&report), &workspace).unwrap();
    assert_eq!(plan.new_space_count(), 0);
    assert_eq!(plan.new_folder_count(), 1);
    assert_eq!(plan.new_pin_count(), 2);
    assert_eq!(plan.kept_pin_count(), 2);
    assert_eq!(plan.spaces[0].folders[0].existing, Some(work));
    apply(&report, &as_written(&report), &mut workspace).unwrap();
    // Missing Pins go at the folder's end; the folder keeps its closed state.
    assert_eq!(
        folders(&workspace),
        [
            (
                "Desk".into(),
                "Work".into(),
                strings(&["https://a.test/", "https://b.test/", "https://c.test/"])
            ),
            ("Desk".into(), "New".into(), strings(&["https://d.test/"])),
        ]
    );
    assert!(!workspace.folders()[0].open);
    let again = preview(&report, &as_written(&report), &workspace).unwrap();
    assert!(again.is_noop());
    assert_eq!(again.kept_pin_count(), 4);
}

#[test]
fn fixture_reimport_with_folders_is_a_noop() {
    let mut workspace = Workspace::default();
    import(FIXTURE, &mut workspace).unwrap();
    let once = workspace.snapshot();
    import(FIXTURE, &mut workspace).unwrap();
    assert_eq!(workspace.snapshot(), once);
}

#[test]
fn a_folder_pins_title_survives_restart_and_reimport() {
    // The export has no titles. Opening a Pin stores its page title the way
    // any Pin's does, and a later import keeps it.
    let mut workspace = Workspace::default();
    import(FIXTURE, &mut workspace).unwrap();
    let pin = workspace.folders()[0].items[1];
    let item = workspace
        .items()
        .iter()
        .find(|item| item.id == pin)
        .unwrap();
    assert_eq!(
        item.canonical_url.as_deref(),
        Some("https://docs.example.test/start")
    );
    assert!(item.title.is_empty());

    assert_eq!(workspace.set_title(pin, "Getting started"), Ok(true));
    let mut restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    import(FIXTURE, &mut restored).unwrap();
    let title = |workspace: &Workspace| {
        let item = workspace.items().iter().find(|item| item.id == pin);
        item.unwrap().title.clone()
    };
    assert_eq!(title(&restored), "Getting started");
    assert_eq!(restored.snapshot(), workspace.snapshot());
}

#[test]
fn reimport_moves_pins_of_a_flattened_import_into_their_folder() {
    // An import from before folders left every URL as an ungrouped Pin.
    let mut workspace = Workspace::default();
    let desk = workspace.create_space(DEFAULT_PROFILE_ID, "Desk").unwrap();
    let mut ids = Vec::new();
    for url in ["https://fav.test/", "https://a.test/", "https://b.test/"] {
        let item = workspace.add_temporary_item(desk).unwrap();
        workspace
            .set_placement(item, Placement::Pin { space_id: desk }, Some(url))
            .unwrap();
        ids.push(item);
    }
    let text = "# Desk\n## Favorites\n- https://fav.test/\n## Work\n- https://b.test/\n- https://a.test/\n";
    let report = report(text);
    let plan = preview(&report, &as_written(&report), &workspace).unwrap();
    assert_eq!(plan.new_pin_count(), 0);
    assert_eq!(plan.moved_pin_count(), 2);
    assert_eq!(plan.kept_pin_count(), 1);
    assert_eq!(
        plan.spaces[0].folders[0].additions,
        [FolderAddition::Move(ids[2]), FolderAddition::Move(ids[1])]
    );
    apply(&report, &as_written(&report), &mut workspace).unwrap();
    assert_eq!(
        folders(&workspace),
        [(
            "Desk".into(),
            "Work".into(),
            strings(&["https://b.test/", "https://a.test/"])
        )]
    );
    assert_eq!(
        ungrouped(&workspace)[1],
        ("Desk".into(), strings(&["https://fav.test/"]))
    );
    assert_eq!(workspace.items().len(), 3);
    assert!(
        preview(&report, &as_written(&report), &workspace)
            .unwrap()
            .is_noop()
    );
}

#[test]
fn a_renamed_folder_no_longer_matches() {
    let mut workspace = Workspace::default();
    let text = "# Desk\n## Work\n- https://a.test/\n";
    import(text, &mut workspace).unwrap();
    let work = workspace.folders()[0].id;
    workspace.rename_folder(work, "Job").unwrap();
    import(text, &mut workspace).unwrap();
    // Folders match by name only, so "Work" comes back beside "Job".
    assert_eq!(
        folders(&workspace),
        [
            ("Desk".into(), "Job".into(), strings(&["https://a.test/"])),
            ("Desk".into(), "Work".into(), strings(&["https://a.test/"])),
        ]
    );
}

#[test]
fn a_pin_the_user_moved_out_of_its_folder_moves_back_in() {
    let mut workspace = Workspace::default();
    let text = "# Desk\n## Work\n- https://a.test/\n";
    import(text, &mut workspace).unwrap();
    let item = workspace.folders()[0].items[0];
    workspace.move_pin_to_folder(item, None, None).unwrap();
    import(text, &mut workspace).unwrap();
    assert_eq!(workspace.folder_of(item), Some(workspace.folders()[0].id));
    assert_eq!(workspace.items().len(), 1);
}

#[test]
fn too_many_folders_fails_and_changes_nothing() {
    let mut text = String::from("# Desk\n");
    for i in 0..=MAX_FOLDERS {
        text.push_str(&format!("## F{i}\n"));
    }
    let mut workspace = Workspace::default();
    let before = workspace.snapshot();
    let error = import(&text, &mut workspace).unwrap_err();
    assert_eq!(
        error,
        ImportError::Workspace(WorkspaceError::LimitExceeded("folders"))
    );
    assert_eq!(workspace.snapshot(), before);
}

#[test]
fn long_group_labels_are_cut_to_the_folder_name_limit() {
    let label = "é".repeat(100);
    let text = format!("# Desk\n## {label}\n- https://a.test/\n");
    let mut workspace = Workspace::default();
    import(&text, &mut workspace).unwrap();
    import(&text, &mut workspace).unwrap();
    assert_eq!(workspace.folders().len(), 1);
    assert_eq!(workspace.folders()[0].name, "é".repeat(64));
}
