use myarc_workspace::*;

const S1: SpaceId = TEST_SPACE_ID;
const P1: ProfileId = DEFAULT_PROFILE_ID;
const W1: WindowId = DEFAULT_WINDOW_ID;

fn pin(workspace: &mut Workspace, space: SpaceId, url: &str) -> ItemId {
    let id = workspace.add_temporary_item(space).unwrap();
    workspace
        .set_placement(id, Placement::Pin { space_id: space }, Some(url))
        .unwrap();
    id
}

fn folder(workspace: &Workspace, id: FolderId) -> Folder {
    workspace
        .folders()
        .iter()
        .find(|folder| folder.id == id)
        .cloned()
        .unwrap()
}

fn names(workspace: &Workspace, space: SpaceId) -> Vec<String> {
    workspace
        .folders()
        .iter()
        .filter(|folder| folder.space_id == space)
        .map(|folder| folder.name.clone())
        .collect()
}

#[test]
fn new_folders_open_empty_after_the_spaces_others() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let a = workspace.create_folder(S1, "Reading").unwrap();
    let b = workspace.create_folder(work, "Clients").unwrap();
    let c = workspace.create_folder(S1, "Tools").unwrap();
    assert!(a < b && b < c);
    assert_eq!(names(&workspace, S1), ["Reading", "Tools"]);
    let made = folder(&workspace, a);
    assert!(made.open);
    assert!(made.items.is_empty());
    assert_eq!(made.space_id, S1);
}

#[test]
fn folder_names_follow_space_name_rules() {
    let mut workspace = Workspace::default();
    let before = workspace.snapshot();
    for (name, message) in [
        ("  ", "Enter a folder name"),
        ("a\nb", "Folder name cannot contain control characters"),
    ] {
        let error = workspace.create_folder(S1, name).unwrap_err();
        assert_eq!(error.to_string(), message);
    }
    let long = "x".repeat(MAX_FOLDER_NAME_BYTES + 1);
    assert_eq!(
        workspace.create_folder(S1, &long).unwrap_err(),
        WorkspaceError::InvalidFolderName(RenameError::NameTooLong)
    );
    assert_eq!(
        workspace.create_folder(99, "Lost").unwrap_err(),
        WorkspaceError::UnknownSpace(99)
    );
    assert_eq!(workspace.snapshot(), before);

    let id = workspace.create_folder(S1, "Reading").unwrap();
    workspace.rename_folder(id, "Later").unwrap();
    assert_eq!(folder(&workspace, id).name, "Later");
    assert!(workspace.rename_folder(id, "").is_err());
    assert_eq!(folder(&workspace, id).name, "Later");
    assert_eq!(
        workspace.rename_folder(42, "x").unwrap_err(),
        WorkspaceError::UnknownFolder(42)
    );
}

#[test]
fn open_state_changes_report_and_survive_a_snapshot() {
    let mut workspace = Workspace::default();
    let id = workspace.create_folder(S1, "Reading").unwrap();
    assert_eq!(workspace.set_folder_open(id, true), Ok(false));
    assert_eq!(workspace.set_folder_open(id, false), Ok(true));
    let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    assert!(!folder(&restored, id).open);
    assert_eq!(restored, workspace);
}

#[test]
fn folders_reorder_within_their_space_only() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let a = workspace.create_folder(S1, "A").unwrap();
    let other = workspace.create_folder(work, "Other").unwrap();
    let b = workspace.create_folder(S1, "B").unwrap();
    let c = workspace.create_folder(S1, "C").unwrap();
    workspace.move_folder_before(c, Some(a)).unwrap();
    assert_eq!(names(&workspace, S1), ["C", "A", "B"]);
    workspace.move_folder_before(c, None).unwrap();
    assert_eq!(names(&workspace, S1), ["A", "B", "C"]);
    workspace.move_folder_before(b, Some(b)).unwrap();
    assert_eq!(names(&workspace, S1), ["A", "B", "C"]);
    assert_eq!(
        workspace.move_folder_before(a, Some(other)).unwrap_err(),
        WorkspaceError::DifferentFolderSpace(other)
    );
    workspace.move_folder_before(other, None).unwrap();
    assert_eq!(names(&workspace, work), ["Other"]);
}

#[test]
fn pins_move_in_out_and_between_folders_in_order() {
    let mut workspace = Workspace::default();
    let x = workspace.create_folder(S1, "X").unwrap();
    let y = workspace.create_folder(S1, "Y").unwrap();
    let one = pin(&mut workspace, S1, "https://one.test/");
    let two = pin(&mut workspace, S1, "https://two.test/");
    let three = pin(&mut workspace, S1, "https://three.test/");
    workspace.move_pin_to_folder(one, Some(x), None).unwrap();
    workspace.move_pin_to_folder(two, Some(x), None).unwrap();
    workspace
        .move_pin_to_folder(three, Some(x), Some(one))
        .unwrap();
    assert_eq!(folder(&workspace, x).items, [three, one, two]);
    assert_eq!(workspace.folder_of(one), Some(x));

    workspace.move_pin_to_folder(one, Some(y), None).unwrap();
    assert_eq!(folder(&workspace, x).items, [three, two]);
    assert_eq!(folder(&workspace, y).items, [one]);

    workspace.move_pin_to_folder(two, None, None).unwrap();
    assert_eq!(folder(&workspace, x).items, [three]);
    assert_eq!(workspace.folder_of(two), None);
    // The Pin itself stays; only its folder changed.
    assert!(workspace.items().iter().any(|item| item.id == two));
}

#[test]
fn only_pins_of_the_folders_space_go_in() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let x = workspace.create_folder(S1, "X").unwrap();
    let elsewhere = pin(&mut workspace, work, "https://work.test/");
    let temporary = workspace.add_temporary_item(S1).unwrap();
    let favorite = workspace.add_temporary_item(S1).unwrap();
    workspace
        .set_placement(
            favorite,
            Placement::Favorite { profile_id: P1 },
            Some("https://fav.test/"),
        )
        .unwrap();
    let before = workspace.snapshot();
    assert_eq!(
        workspace.move_pin_to_folder(elsewhere, Some(x), None),
        Err(WorkspaceError::DifferentFolderSpace(x))
    );
    for id in [temporary, favorite] {
        assert_eq!(
            workspace.move_pin_to_folder(id, Some(x), None),
            Err(WorkspaceError::NotPinned(id))
        );
    }
    let inside = pin(&mut workspace, S1, "https://in.test/");
    let after_pin = workspace.snapshot();
    assert_eq!(
        workspace.move_pin_to_folder(inside, Some(x), Some(elsewhere)),
        Err(WorkspaceError::DifferentSection(elsewhere))
    );
    assert_eq!(
        workspace.move_pin_to_folder(inside, None, Some(inside)),
        Err(WorkspaceError::DifferentSection(inside))
    );
    assert_eq!(workspace.snapshot(), after_pin);
    assert_eq!(before.folders, after_pin.folders);
}

#[test]
fn a_pin_leaves_its_folder_when_it_stops_being_a_pin_there() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let x = workspace.create_folder(S1, "X").unwrap();
    let moved = pin(&mut workspace, S1, "https://moved.test/");
    let favored = pin(&mut workspace, S1, "https://favored.test/");
    let unpinned = pin(&mut workspace, S1, "https://unpinned.test/");
    let removed = pin(&mut workspace, S1, "https://removed.test/");
    let kept = pin(&mut workspace, S1, "https://kept.test/");
    for id in [moved, favored, unpinned, removed, kept] {
        workspace.move_pin_to_folder(id, Some(x), None).unwrap();
    }
    workspace
        .set_placement(moved, Placement::Pin { space_id: work }, None)
        .unwrap();
    workspace
        .set_placement(favored, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    workspace.unsave_item(unpinned, Some(S1)).unwrap();
    workspace.remove_saved_item(removed).unwrap();
    assert_eq!(folder(&workspace, x).items, [kept]);
}

#[test]
fn deleting_a_space_deletes_its_folders() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let gone = workspace.create_folder(work, "Gone").unwrap();
    let stays = workspace.create_folder(S1, "Stays").unwrap();
    workspace.delete_space(work).unwrap();
    assert_eq!(workspace.folders().len(), 1);
    assert_eq!(workspace.folders()[0].id, stays);
    assert_eq!(
        workspace.rename_folder(gone, "x"),
        Err(WorkspaceError::UnknownFolder(gone))
    );
}

#[test]
fn deleting_a_folder_deletes_and_archives_its_pins() {
    let mut workspace = Workspace::default();
    let x = workspace.create_folder(S1, "X").unwrap();
    let a = pin(&mut workspace, S1, "https://a.test/");
    let b = pin(&mut workspace, S1, "https://b.test/");
    let loose = pin(&mut workspace, S1, "https://loose.test/");
    workspace.set_title(a, "A page").unwrap();
    workspace.move_pin_to_folder(b, Some(x), None).unwrap();
    workspace.move_pin_to_folder(a, Some(x), None).unwrap();
    workspace.select_item(W1, a).unwrap();

    let deletion = workspace.delete_folder(x, Some(1_000)).unwrap();
    assert_eq!(deletion.removed_items, [b, a]);
    assert_eq!(deletion.archived, 2);
    assert!(workspace.folders().is_empty());
    let ids: Vec<ItemId> = workspace.items().iter().map(|item| item.id).collect();
    assert_eq!(ids, [loose]);
    // The selection was in the folder, so it clears.
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        None
    );
    let archived = workspace.archived_tabs(1_000);
    assert_eq!(archived.len(), 2);
    assert!(archived.iter().any(|tab| tab.id == a
        && tab.url == "https://a.test/"
        && tab.title == "A page"
        && tab.space_id == S1));
    // A restored archive loads; the archive's IDs are not live items.
    Workspace::from_snapshot(workspace.snapshot()).unwrap();
}

#[test]
fn deleting_a_folder_without_history_archives_nothing() {
    let mut workspace = Workspace::default();
    let x = workspace.create_folder(S1, "X").unwrap();
    let a = pin(&mut workspace, S1, "https://a.test/");
    workspace.move_pin_to_folder(a, Some(x), None).unwrap();
    let deletion = workspace.delete_folder(x, None).unwrap();
    assert_eq!(deletion.removed_items, [a]);
    assert_eq!(deletion.archived, 0);
    assert!(workspace.archived_tabs(0).is_empty());
    assert!(workspace.items().is_empty());
}

#[test]
fn sidebar_lists_the_current_spaces_folders() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let x = workspace.create_folder(S1, "X").unwrap();
    workspace.create_folder(work, "Elsewhere").unwrap();
    let a = pin(&mut workspace, S1, "https://a.test/");
    workspace.move_pin_to_folder(a, Some(x), None).unwrap();
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(sidebar.folders.len(), 1);
    assert_eq!(sidebar.folders[0].items, [a]);
    // Folder Pins stay in `pins` too, so callers that list Pins see them.
    assert!(sidebar.pins.iter().any(|item| item.id == a));
}

#[test]
fn damaged_folders_load_without_losing_pins() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let a = pin(&mut workspace, S1, "https://a.test/");
    let b = pin(&mut workspace, work, "https://b.test/");
    let mut snapshot = workspace.snapshot();
    let folder = |id, space_id, name: &str, items: Vec<ItemId>| Folder {
        id,
        space_id,
        name: name.into(),
        open: false,
        items,
    };
    snapshot.folders = vec![
        // Keeps `a`, drops the other Space's Pin and the unknown item.
        folder(4, S1, "Good", vec![a, b, 77]),
        // `a` is already in a folder.
        folder(5, S1, "Twice", vec![a]),
        folder(4, S1, "Same ID", vec![]),
        folder(0, S1, "Zero", vec![]),
        folder(FolderId::MAX, S1, "Max", vec![]),
        folder(6, 99, "No Space", vec![]),
        folder(7, S1, "", vec![]),
    ];
    snapshot.next_folder_id = 1;
    let loaded = Workspace::from_snapshot(snapshot).unwrap();
    let kept: Vec<(FolderId, Vec<ItemId>)> = loaded
        .folders()
        .iter()
        .map(|folder| (folder.id, folder.items.clone()))
        .collect();
    assert_eq!(kept, [(4, vec![a]), (5, vec![])]);
    assert_eq!(loaded.items().len(), 2);
    assert!(!loaded.folders()[0].open);
    // New folders never reuse a loaded ID.
    let mut loaded = loaded;
    assert_eq!(loaded.create_folder(S1, "New").unwrap(), 6);
}

#[test]
fn folder_count_is_bounded() {
    let mut workspace = Workspace::default();
    for i in 0..MAX_FOLDERS {
        workspace.create_folder(S1, &format!("F{i}")).unwrap();
    }
    assert_eq!(
        workspace.create_folder(S1, "One more"),
        Err(WorkspaceError::LimitExceeded("folders"))
    );
}
