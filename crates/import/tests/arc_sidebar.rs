#![cfg(feature = "arc-sidebar")]

use myarc_import::arc_markdown::{Omission, Report};
use myarc_import::arc_report::{Line, lines};
use myarc_import::arc_sidebar::{
    ArcProfile, MAX_INPUT_BYTES, MAX_LOCAL_STATE_BYTES, ProfileReport, SidebarError, name_profiles,
    parse, parse_profiles, profile_names,
};
use myarc_import::arc_workspace::{Preview, apply, apply_to_new_profile, as_written};
use myarc_workspace::{DEFAULT_PROFILE_ID, Placement, Workspace};

const FIXTURE_PATH: &str = concat!(
    env!("CARGO_MANIFEST_DIR"),
    "/../../tests/fixtures/arc-sidebar/StorableSidebar.json"
);

fn fixture() -> Report {
    parse(&std::fs::read(FIXTURE_PATH).unwrap()).expect("fixture parses")
}

fn fixture_profiles() -> Vec<ProfileReport> {
    parse_profiles(&std::fs::read(FIXTURE_PATH).unwrap()).expect("fixture parses")
}

const LOCAL_STATE_PATH: &str = concat!(
    env!("CARGO_MANIFEST_DIR"),
    "/../../tests/fixtures/arc-sidebar/User Data/Local State"
);

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
            (Omission::NestedFolder, 2),
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
    let new: Vec<_> = plan.new_favorites.iter().map(|n| n.url.as_str()).collect();
    assert_eq!(new, ["https://mail.example.test/"]);
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
            "Folders inside folders, their tabs moved to the outer folder: 2",
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

#[test]
fn reads_each_arc_profile_apart() {
    let profiles = fixture_profiles();
    // Profile 2 lists only an empty Favorites container, so it has no part.
    let names: Vec<_> = profiles.iter().map(|p| p.profile.clone()).collect();
    assert_eq!(
        names,
        [ArcProfile::Main, ArcProfile::Custom("Profile 1".into())]
    );
    assert_eq!(profiles[0].report, fixture());
    let main: Vec<_> = profiles[0].report.spaces.iter().map(|s| &s.label).collect();
    assert_eq!(main, ["Work", "Home"]);
    let other = &profiles[1].report;
    assert_eq!(
        urls(&other.favorites),
        ["https://other-profile.example.test/"]
    );
    assert_eq!(other.spaces.len(), 1);
    let side = &other.spaces[0];
    assert_eq!(side.label, "Side Project");
    assert_eq!(urls(&side.pins), ["https://repo.example.test/"]);
    assert_eq!(side.groups.len(), 1);
    assert_eq!(side.groups[0].label, "Reading");
    // "Later" sits inside "Reading"; its tab joins the outer folder.
    assert_eq!(
        urls(&side.groups[0].entries),
        ["https://paper.example.test/", "https://essay.example.test/"]
    );
    assert!(other.omitted.is_empty() && other.unrecognized.is_empty());
}

#[test]
fn custom_profiles_map_to_a_named_myarc_profile() {
    assert_eq!(ArcProfile::Main.myarc_name(), None);
    assert_eq!(
        ArcProfile::Custom("Profile 1".into())
            .myarc_name()
            .as_deref(),
        Some("Arc Profile 1")
    );
    let json = r#"{"sidebar":{"containers":[{"global":{}},{
        "topAppsContainerIDs":[{"custom":{}},"f"],
        "items":[{"id":"f","childrenIds":["t"]},
                 {"id":"t","data":{"tab":{"savedURL":"https://t.example.test/"}}}]}]}}"#;
    let profiles = parse_profiles(json.as_bytes()).unwrap();
    assert_eq!(profiles[1].profile, ArcProfile::Custom("Unnamed".into()));
}

#[test]
fn names_and_page_titles_reach_the_imported_items() {
    let report = fixture();
    let docs = &report.spaces[0].pins[0];
    assert_eq!(
        (docs.name.as_str(), docs.title.as_str()),
        ("Team Docs", "Docs - Home")
    );
    let mut workspace = Workspace::default();
    import(&report, &mut workspace);
    let state = workspace.snapshot();
    let item = |url: &str| {
        state
            .items
            .iter()
            .find(|item| item.canonical_url.as_deref() == Some(url))
            .unwrap()
    };
    let docs = item("https://docs.example.test/");
    assert_eq!(
        (docs.name.as_str(), docs.title.as_str()),
        ("Team Docs", "Docs - Home")
    );
    let alpha = item("https://alpha.example.test/");
    assert_eq!(
        (alpha.name.as_str(), alpha.title.as_str()),
        ("", "Alpha Board")
    );
    let mail = item("https://mail.example.test/");
    assert_eq!((mail.name.as_str(), mail.title.as_str()), ("", "Mail"));
    let beta = item("https://beta.example.test/");
    assert_eq!((beta.name.as_str(), beta.title.as_str()), ("", ""));
}

#[test]
fn another_profile_imports_into_its_own_new_profile_once() {
    let profiles = fixture_profiles();
    let report = &profiles[1].report;
    let mut workspace = Workspace::default();
    let plan = apply_to_new_profile(report, &as_written(report), &mut workspace).unwrap();
    assert_eq!(plan.new_space_count(), 1);
    assert_eq!(
        favorites(&workspace),
        ["https://other-profile.example.test/"]
    );
    // The new Profile's empty default Space gave way to Arc's Space.
    assert_eq!(
        spaces(&workspace),
        [(
            "Side Project".to_owned(),
            vec!["https://repo.example.test/".to_owned()],
            vec![(
                "Reading".to_owned(),
                vec![
                    "https://paper.example.test/".to_owned(),
                    "https://essay.example.test/".to_owned(),
                ]
            )]
        )]
    );
    let repo = workspace.snapshot();
    let repo = repo
        .items
        .iter()
        .find(|item| item.canonical_url.as_deref() == Some("https://repo.example.test/"))
        .unwrap();
    assert_eq!(
        (repo.name.as_str(), repo.title.as_str()),
        ("My Repo", "repo: main")
    );
    let first = workspace.snapshot();
    let again = apply(report, &as_written(report), &mut workspace).unwrap();
    assert!(again.is_noop());
    assert_eq!(workspace.snapshot(), first);
}

#[test]
fn local_state_gives_each_arc_profile_its_name() {
    let names = profile_names(&std::fs::read(LOCAL_STATE_PATH).unwrap());
    assert_eq!(
        names.get("Profile 1").map(String::as_str),
        Some("Side Work")
    );
    assert_eq!(names.get("Default").map(String::as_str), Some("Personal"));
    let mut profiles = fixture_profiles();
    name_profiles(&mut profiles, &names);
    // The main profile still imports into the Profile running the import.
    assert_eq!(profiles[0].name, None);
    assert_eq!(profiles[0].arc_label(), None);
    assert_eq!(profiles[0].myarc_name(), None);
    let other = &profiles[1];
    assert_eq!(other.arc_label(), Some("Side Work"));
    assert_eq!(other.myarc_name().as_deref(), Some("Side Work"));
    // Earlier imports named the Profile after the directory; a re-import
    // looks for that name too.
    assert_eq!(other.profile.myarc_name().as_deref(), Some("Arc Profile 1"));
}

#[test]
fn missing_or_bad_local_state_keeps_the_directory_name() {
    let unnamed = |local_state: &[u8]| {
        let mut profiles = fixture_profiles();
        name_profiles(&mut profiles, &profile_names(local_state));
        (
            profiles[1].arc_label().map(str::to_owned),
            profiles[1].myarc_name(),
        )
    };
    let fallback = (
        Some("Profile 1".to_owned()),
        Some("Arc Profile 1".to_owned()),
    );
    assert_eq!(unnamed(b""), fallback);
    assert_eq!(unnamed(b"{not json"), fallback);
    assert_eq!(unnamed(br#"{"profile":{"info_cache":[]}}"#), fallback);
    assert_eq!(
        unnamed(br#"{"profile":{"info_cache":{"Profile 1":{"name":" \n "}}}}"#),
        fallback
    );
    assert_eq!(
        unnamed(br#"{"profile":{"info_cache":{"Profile 1":{"name":7}}}}"#),
        fallback
    );
    let mut big = br#"{"profile":{"info_cache":{"Profile 1":{"name":"Big"}}},"pad":""#.to_vec();
    big.resize(MAX_LOCAL_STATE_BYTES + 1, b' ');
    assert!(profile_names(&big).is_empty());
}

#[test]
fn arc_profile_names_are_cleaned_and_shortened() {
    let long = "W".repeat(100);
    let json = format!(
        r#"{{"profile":{{"info_cache":{{"Profile 1":{{"name":"  Work\tStuff "}},"Profile 2":{{"name":"{long}"}}}}}}}}"#
    );
    let names = profile_names(json.as_bytes());
    assert_eq!(names["Profile 1"], "Work Stuff");
    assert_eq!(names["Profile 2"].len(), 64);
}

#[test]
fn two_arc_profiles_with_one_name_keep_their_directories() {
    let json = r#"{"sidebar":{"containers":[{"global":{}},{
        "topAppsContainerIDs":[
            {"custom":{"_0":{"directoryBasename":"Profile 1"}}},"a",
            {"custom":{"_0":{"directoryBasename":"Profile 2"}}},"b"],
        "items":[{"id":"a","childrenIds":["t"]},{"id":"b","childrenIds":["u"]},
                 {"id":"t","data":{"tab":{"savedURL":"https://t.example.test/"}}},
                 {"id":"u","data":{"tab":{"savedURL":"https://u.example.test/"}}}]}]}}"#;
    let mut profiles = parse_profiles(json.as_bytes()).unwrap();
    let names = profile_names(
        br#"{"profile":{"info_cache":{"Profile 1":{"name":"Work"},"Profile 2":{"name":"Work"}}}}"#,
    );
    name_profiles(&mut profiles, &names);
    let mapped: Vec<_> = profiles[1..]
        .iter()
        .map(ProfileReport::myarc_name)
        .collect();
    assert_eq!(
        mapped,
        [
            Some("Arc Profile 1".to_owned()),
            Some("Arc Profile 2".to_owned())
        ]
    );
}
