#![cfg(feature = "arc-sidebar")]

use myarc_import::arc_markdown::{Omission, Report};
use myarc_import::arc_report::{Line, lines};
use myarc_import::arc_sidebar::{MAX_INPUT_BYTES, SidebarError, parse};
use myarc_import::arc_workspace::{Preview, apply, as_written};
use myarc_workspace::{DEFAULT_PROFILE_ID, Placement, Workspace};

const FIXTURE_PATH: &str = concat!(
    env!("CARGO_MANIFEST_DIR"),
    "/../../tests/fixtures/arc-sidebar/StorableSidebar.json"
);

fn fixture() -> Report {
    parse(&std::fs::read(FIXTURE_PATH).unwrap()).expect("fixture parses")
}

fn urls(entries: &[myarc_import::arc_markdown::Entry]) -> Vec<&str> {
    entries.iter().map(|entry| entry.url.as_str()).collect()
}

fn import(report: &Report, workspace: &mut Workspace) -> Preview {
    apply(report, &as_written(report), workspace).expect("import succeeds")
}

/// Profile Favorite URLs in sidebar order.
fn favorites(workspace: &Workspace) -> Vec<String> {
    let favorite = Placement::Favorite {
        profile_id: DEFAULT_PROFILE_ID,
    };
    let state = workspace.snapshot();
    state
        .items
        .iter()
        .filter(|item| item.placement == favorite)
        .map(|item| item.canonical_url.clone().unwrap())
        .collect()
}

/// A Space's name, its ungrouped Pin URLs, then its folders with theirs.
type SpacePins = (String, Vec<String>, Vec<(String, Vec<String>)>);

fn spaces(workspace: &Workspace) -> Vec<SpacePins> {
    let state = workspace.snapshot();
    let url = |id| {
        let item = state.items.iter().find(|item| item.id == id).unwrap();
        item.canonical_url.clone().unwrap()
    };
    state
        .spaces
        .iter()
        .map(|space| {
            let pins = state
                .items
                .iter()
                .filter(|item| item.placement == Placement::Pin { space_id: space.id })
                .filter(|item| workspace.folder_of(item.id).is_none())
                .map(|item| url(item.id))
                .collect();
            let folders = state
                .folders
                .iter()
                .filter(|folder| folder.space_id == space.id)
                .map(|folder| {
                    let pins = folder.items.iter().map(|id| url(*id)).collect();
                    (folder.name.clone(), pins)
                })
                .collect();
            (space.name.clone(), pins, folders)
        })
        .collect()
}

#[test]
fn reads_favorites_apart_from_space_pins() {
    let report = fixture();
    assert_eq!(
        urls(&report.favorites),
        [
            "https://mail.example.test/",
            "https://calendar.example.test/"
        ]
    );
    let work = &report.spaces[0];
    assert_eq!(work.label, "Work");
    assert_eq!(
        urls(&work.pins),
        [
            "https://docs.example.test/",
            "https://left.example.test/",
            "https://right.example.test/",
        ]
    );
    assert_eq!(work.groups.len(), 1);
    assert_eq!(work.groups[0].label, "Projects");
    assert_eq!(
        urls(&work.groups[0].entries),
        ["https://alpha.example.test/", "https://beta.example.test/"]
    );
    let home = &report.spaces[1];
    assert_eq!(home.label, "Home");
    assert_eq!(
        urls(&home.pins),
        [
            "https://news.example.test/",
            "file:///Users/example/notes.txt"
        ]
    );
    assert!(home.groups.is_empty());
    assert!(report.unrecognized.is_empty());
}

#[test]
fn counts_what_it_leaves_out() {
    let report = fixture();
    let omitted: Vec<_> = report.omitted.into_iter().collect();
    assert_eq!(
        omitted,
        [
            (Omission::TodayTab, 2),
            (Omission::OtherProfileFavorite, 1),
            (Omission::NestedFolder, 1),
            (Omission::UnknownContainer, 3),
        ]
    );
}

#[test]
fn favorites_become_favorites_and_pins_stay_pins() {
    let mut workspace = Workspace::default();
    let plan = import(&fixture(), &mut workspace);
    assert_eq!(plan.new_favorite_count(), 2);
    assert_eq!(
        favorites(&workspace),
        [
            "https://mail.example.test/",
            "https://calendar.example.test/"
        ]
    );
    let spaces = spaces(&workspace);
    let work = spaces.iter().find(|space| space.0 == "Work").unwrap();
    assert_eq!(
        work.1,
        [
            "https://docs.example.test/",
            "https://left.example.test/",
            "https://right.example.test/",
        ]
    );
    assert_eq!(
        work.2,
        [(
            "Projects".to_owned(),
            vec![
                "https://alpha.example.test/".to_owned(),
                "https://beta.example.test/".to_owned(),
            ]
        )]
    );
    let home = spaces.iter().find(|space| space.0 == "Home").unwrap();
    assert_eq!(home.1, ["https://news.example.test/"]);
    // No Favorite became a Pin anywhere.
    for (_, pins, folders) in &spaces {
        let pinned = pins.iter().chain(folders.iter().flat_map(|f| &f.1));
        for url in pinned {
            assert!(
                !url.contains("mail.") && !url.contains("calendar."),
                "{url}"
            );
        }
    }
}

#[test]
fn reimport_adds_nothing_and_leaves_the_source_alone() {
    let before = std::fs::read(FIXTURE_PATH).unwrap();
    let mut workspace = Workspace::default();
    import(&fixture(), &mut workspace);
    let first = workspace.snapshot();
    let again = import(&fixture(), &mut workspace);
    assert!(again.is_noop());
    assert_eq!(again.kept_favorites, 2);
    assert_eq!(workspace.snapshot(), first);
    assert_eq!(std::fs::read(FIXTURE_PATH).unwrap(), before);
}

#[test]
fn keeps_a_favorite_the_profile_already_has() {
    let mut workspace = Workspace::default();
    let space = workspace.snapshot().spaces[0].id;
    let item = workspace.add_temporary_item(space).unwrap();
    workspace
        .set_placement(
            item,
            Placement::Favorite {
                profile_id: DEFAULT_PROFILE_ID,
            },
            Some("https://calendar.example.test/"),
        )
        .unwrap();
    let plan = import(&fixture(), &mut workspace);
    assert_eq!(plan.kept_favorites, 1);
    assert_eq!(plan.new_favorites, ["https://mail.example.test/"]);
    assert_eq!(
        favorites(&workspace),
        [
            "https://calendar.example.test/",
            "https://mail.example.test/"
        ]
    );
}

#[test]
fn report_names_omissions_and_no_favorites_as_pins() {
    let report = fixture();
    let canonical = as_written(&report);
    let mut workspace = Workspace::default();
    let plan = apply(&report, &canonical, &mut workspace).unwrap();
    let texts: Vec<String> = lines(&report, &canonical, &plan)
        .iter()
        .map(Line::text)
        .collect();
    assert_eq!(
        texts,
        [
            "Links left out, not web pages: 1",
            "Today tabs left out: 2",
            "Favorites of other Arc profiles left out: 1",
            "Folders inside folders, their tabs moved to the outer folder: 1",
            "Unknown sidebar parts left out: 3",
        ]
    );
}

#[test]
fn bad_saved_urls_are_reported_not_dropped() {
    let json = r#"{"sidebar":{"containers":[{"global":{}},{
        "spaces":[{"title":"S","containerIDs":["pinned","p"]}],
        "items":[{"id":"p","childrenIds":["a","b"]},
                 {"id":"a","data":{"tab":{"savedURL":"not a url"}}},
                 {"id":"b","data":{"tab":{}}}]}]}}"#;
    let report = parse(json.as_bytes()).unwrap();
    assert!(report.spaces[0].pins.is_empty());
    assert_eq!(report.unrecognized.len(), 2);
}

#[test]
fn cycles_and_deep_nesting_end() {
    let mut items = vec![
        r#"{"id":"p","childrenIds":["p","f0"]}"#.to_owned(),
        // A folder listing its parent as a child.
        r#"{"id":"loop","childrenIds":["p"],"data":{"list":{}}}"#.to_owned(),
    ];
    for depth in 0..100 {
        items.push(format!(
            r#"{{"id":"f{depth}","title":"F","childrenIds":["f{}","loop"],"data":{{"list":{{}}}}}}"#,
            depth + 1
        ));
    }
    let json = format!(
        r#"{{"sidebar":{{"containers":[{{"global":{{}}}},{{
            "spaces":[{{"title":"S","containerIDs":["pinned","p"]}}],
            "items":[{}]}}]}}}}"#,
        items.join(",")
    );
    let report = parse(json.as_bytes()).unwrap();
    assert_eq!(report.spaces[0].groups.len(), 1);
    assert!(report.omitted[&Omission::UnknownContainer] >= 1);
}

#[test]
fn rejects_what_is_not_a_sidebar() {
    assert!(matches!(parse(b"# Space"), Err(SidebarError::NotJson(_))));
    assert_eq!(parse(b"{}"), Err(SidebarError::NoSidebar));
    assert_eq!(
        parse(br#"{"sidebar":{"containers":[{"littleBrowser":{}},{}]}}"#),
        Err(SidebarError::NoSidebar)
    );
    let big = vec![b' '; MAX_INPUT_BYTES + 1];
    assert_eq!(
        parse(&big),
        Err(SidebarError::TooLarge {
            len: MAX_INPUT_BYTES + 1,
            max: MAX_INPUT_BYTES
        })
    );
}
