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
    workspace.delete_space(work, None).unwrap();
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
        parent_id: None,
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

#[test]
fn export_space_into_folder_copies_pins_and_folder_items_and_persists() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let dest = workspace.create_space(P1, "Archive").unwrap();

    // Source Space has loose Pins, a folder with Pins in a specific order, and
    // a Today tab (which should not be exported).
    let loose_a = pin(&mut workspace, work, "https://a.test/");
    workspace.set_title(loose_a, "Alpha Page").unwrap();
    workspace.rename_item(loose_a, "Alpha").unwrap();

    let in_folder_1 = pin(&mut workspace, work, "https://b.test/");
    workspace.set_title(in_folder_1, "Beta").unwrap();

    let loose_c = pin(&mut workspace, work, "https://c.test/");
    workspace.set_title(loose_c, "Gamma").unwrap();

    let in_folder_2 = pin(&mut workspace, work, "https://d.test/");
    workspace.set_title(in_folder_2, "Delta").unwrap();

    let src_folder = workspace.create_folder(work, "Specs").unwrap();
    workspace
        .move_pin_to_folder(in_folder_2, Some(src_folder), None)
        .unwrap();
    workspace
        .move_pin_to_folder(in_folder_1, Some(src_folder), None)
        .unwrap();

    // A Today tab in the source Space must not be copied into the folder.
    let today = workspace.add_temporary_item(work).unwrap();
    // An existing Pin in the target Space must stay untouched.
    let existing_dest = pin(&mut workspace, dest, "https://dest.test/");

    let exported_folder_id = workspace.export_space_into_folder(work, dest).unwrap();
    let exported = folder(&workspace, exported_folder_id);
    assert_eq!(exported.space_id, dest);
    assert_eq!(exported.parent_id, None);
    assert_eq!(exported.name, "Work");
    assert!(exported.open);
    // Loose Pins go into the root exported folder; source folders become subfolders.
    assert_eq!(exported.items.len(), 2);

    let sub = workspace
        .folders()
        .iter()
        .find(|f| f.space_id == dest && f.parent_id == Some(exported_folder_id))
        .unwrap()
        .clone();
    assert_eq!(sub.name, "Specs");
    assert_eq!(sub.items.len(), 2);

    let extract = |ids: &[ItemId]| -> Vec<(Option<&str>, &str, &str)> {
        ids.iter()
            .map(|&id| {
                let item = workspace.items().iter().find(|i| i.id == id).unwrap();
                assert_eq!(item.placement, Placement::Pin { space_id: dest });
                (
                    item.canonical_url.as_deref(),
                    item.title.as_str(),
                    item.name.as_str(),
                )
            })
            .collect()
    };
    assert_eq!(
        extract(&exported.items),
        [
            (Some("https://a.test/"), "Alpha Page", "Alpha"),
            (Some("https://c.test/"), "Gamma", ""),
        ]
    );
    assert_eq!(
        extract(&sub.items),
        [
            (Some("https://d.test/"), "Delta", ""),
            (Some("https://b.test/"), "Beta", ""),
        ]
    );

    // Source Space and its items/folders/Today tab remain intact, and the
    // target Space's existing Pin stays ungrouped.
    assert_eq!(
        folder(&workspace, src_folder).items,
        [in_folder_2, in_folder_1]
    );
    assert!(workspace.items().iter().any(|i| i.id == today));
    assert!(workspace.folder_of(existing_dest).is_none());

    // Round-trip through snapshot persistence.
    let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    assert_eq!(restored, workspace);
    assert_eq!(folder(&restored, exported_folder_id), exported);
}

#[test]
fn export_space_into_folder_handles_empty_space_and_rejects_errors_atomically() {
    let mut workspace = Workspace::default();
    let empty_space = workspace.create_space(P1, "Empty").unwrap();

    // Exporting an empty Space creates an empty open folder in the target.
    let folder_id = workspace.export_space_into_folder(empty_space, S1).unwrap();
    let made = folder(&workspace, folder_id);
    assert_eq!(made.space_id, S1);
    assert_eq!(made.name, "Empty");
    assert!(made.open);
    assert!(made.items.is_empty());

    // Rejects exporting into self and unknown Spaces without mutating.
    let before = workspace.snapshot();
    assert_eq!(
        workspace.export_space_into_folder(S1, S1),
        Err(WorkspaceError::SameSpace(S1))
    );
    assert_eq!(
        workspace.export_space_into_folder(99, S1),
        Err(WorkspaceError::UnknownSpace(99))
    );
    assert_eq!(
        workspace.export_space_into_folder(S1, 99),
        Err(WorkspaceError::UnknownSpace(99))
    );
    assert_eq!(workspace.snapshot(), before);

    // Rejects atomically if item limit would be exceeded.
    let src = workspace.create_space(P1, "Big").unwrap();
    pin(&mut workspace, src, "https://one.test/");
    pin(&mut workspace, src, "https://two.test/");
    while workspace.items().len() < MAX_ITEMS - 1 {
        pin(&mut workspace, S1, "https://fill.test/");
    }
    let before_limit = workspace.snapshot();
    assert_eq!(
        workspace.export_space_into_folder(src, S1),
        Err(WorkspaceError::LimitExceeded("items"))
    );
    assert_eq!(workspace.snapshot(), before_limit);
}

#[test]
fn nested_folders_create_move_and_enforce_cycles_and_depth() {
    let mut workspace = Workspace::default();
    let root = workspace.create_folder(S1, "Root").unwrap();
    let child = workspace.create_folder_in(S1, Some(root), "Child").unwrap();
    let grand = workspace
        .create_folder_in(S1, Some(child), "Grand")
        .unwrap();
    assert_eq!(workspace.folder_depth(root), 0);
    assert_eq!(workspace.folder_depth(child), 1);
    assert_eq!(workspace.folder_depth(grand), 2);
    assert!(workspace.is_folder_ancestor(root, grand));
    assert!(!workspace.is_folder_ancestor(grand, root));

    // Rejects cycles (self and descendant).
    assert_eq!(
        workspace.move_folder(root, Some(root), None),
        Err(WorkspaceError::FolderCycle(root))
    );
    assert_eq!(
        workspace.move_folder(root, Some(grand), None),
        Err(WorkspaceError::FolderCycle(root))
    );

    // Promote grandchild to top-level and move back under root before child.
    workspace.move_folder(grand, None, None).unwrap();
    assert_eq!(folder(&workspace, grand).parent_id, None);
    assert_eq!(workspace.folder_depth(grand), 0);
    workspace
        .move_folder(grand, Some(root), Some(child))
        .unwrap();
    assert_eq!(folder(&workspace, grand).parent_id, Some(root));
    let tree_names: Vec<String> = workspace
        .sidebar_snapshot(W1)
        .unwrap()
        .folders
        .into_iter()
        .map(|f| f.name)
        .collect();
    assert_eq!(tree_names, ["Root", "Grand", "Child"]);

    // Depth cap: chain of MAX_FOLDER_DEPTH folders succeeds; one more fails.
    let mut cur = root;
    for level in 1..MAX_FOLDER_DEPTH {
        cur = workspace
            .create_folder_in(S1, Some(cur), &format!("L{level}"))
            .unwrap();
    }
    assert_eq!(workspace.folder_depth(cur), MAX_FOLDER_DEPTH - 1);
    assert_eq!(
        workspace.create_folder_in(S1, Some(cur), "TooDeep"),
        Err(WorkspaceError::FolderDepthExceeded)
    );
    // Moving a subtree that would push a leaf past MAX_FOLDER_DEPTH fails.
    let top = workspace.create_folder(S1, "Top").unwrap();
    let _leaf = workspace.create_folder_in(S1, Some(top), "Leaf").unwrap();
    let near_bottom = folder(&workspace, cur).parent_id.unwrap();
    assert_eq!(
        workspace.move_folder(top, Some(near_bottom), None),
        Err(WorkspaceError::FolderDepthExceeded)
    );
}

#[test]
fn deleting_a_parent_folder_deletes_and_archives_entire_subtree() {
    let mut workspace = Workspace::default();
    let parent = workspace.create_folder(S1, "Parent").unwrap();
    let child = workspace
        .create_folder_in(S1, Some(parent), "Child")
        .unwrap();
    let p_pin = pin(&mut workspace, S1, "https://parent.test/");
    let c_pin = pin(&mut workspace, S1, "https://child.test/");
    workspace
        .move_pin_to_folder(p_pin, Some(parent), None)
        .unwrap();
    workspace
        .move_pin_to_folder(c_pin, Some(child), None)
        .unwrap();

    let deletion = workspace.delete_folder(parent, Some(2_000)).unwrap();
    assert_eq!(deletion.removed_folders, [parent, child]);
    assert_eq!(deletion.removed_items, [p_pin, c_pin]);
    assert_eq!(deletion.archived, 2);
    assert!(workspace.folders().is_empty());
    assert!(workspace.items().is_empty());
}

#[test]
fn closed_ancestor_hides_nested_pins_from_row_order_except_selected() {
    let mut workspace = Workspace::default();
    let parent = workspace.create_folder(S1, "Parent").unwrap();
    let child = workspace
        .create_folder_in(S1, Some(parent), "Child")
        .unwrap();
    let p_pin = pin(&mut workspace, S1, "https://parent.test/");
    let c_pin = pin(&mut workspace, S1, "https://child.test/");
    workspace
        .move_pin_to_folder(p_pin, Some(parent), None)
        .unwrap();
    workspace
        .move_pin_to_folder(c_pin, Some(child), None)
        .unwrap();

    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().row_order(),
        [p_pin, c_pin]
    );
    workspace.set_folder_open(parent, false).unwrap();
    assert!(
        workspace
            .sidebar_snapshot(W1)
            .unwrap()
            .row_order()
            .is_empty()
    );
    workspace.select_item(W1, c_pin).unwrap();
    assert_eq!(workspace.sidebar_snapshot(W1).unwrap().row_order(), [c_pin]);
}

#[test]
fn sanitize_folders_breaks_cycles_and_promotes_invalid_or_deep_parents() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let mut snapshot = workspace.snapshot();
    snapshot.folders = vec![
        // Cycle: 1 <-> 2
        Folder {
            id: 1,
            space_id: S1,
            parent_id: Some(2),
            name: "One".into(),
            open: true,
            items: vec![],
        },
        Folder {
            id: 2,
            space_id: S1,
            parent_id: Some(1),
            name: "Two".into(),
            open: true,
            items: vec![],
        },
        // Cross-space parent
        Folder {
            id: 3,
            space_id: work,
            parent_id: Some(1),
            name: "Cross".into(),
            open: true,
            items: vec![],
        },
        // Missing parent
        Folder {
            id: 4,
            space_id: S1,
            parent_id: Some(99),
            name: "Orphan".into(),
            open: true,
            items: vec![],
        },
    ];
    let loaded = Workspace::from_snapshot(snapshot).unwrap();
    assert_eq!(folder(&loaded, 3).parent_id, None);
    assert_eq!(folder(&loaded, 4).parent_id, None);
    // Cycle broken: at least one of 1, 2 is promoted to top-level.
    assert!(folder(&loaded, 1).parent_id.is_none() || folder(&loaded, 2).parent_id.is_none());
    assert!(!loaded.is_folder_ancestor(1, 1));
    assert!(!loaded.is_folder_ancestor(2, 2));
}
