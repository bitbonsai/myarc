use myarc_workspace::*;

const S1: SpaceId = TEST_SPACE_ID;
const P1: ProfileId = DEFAULT_PROFILE_ID;
const W1: WindowId = DEFAULT_WINDOW_ID;

fn pin(workspace: &mut Workspace, url: &str) -> ItemId {
    let id = workspace.add_temporary_item(S1).unwrap();
    workspace
        .set_placement(id, Placement::Pin { space_id: S1 }, Some(url))
        .unwrap();
    id
}

fn today(workspace: &mut Workspace, url: &str) -> ItemId {
    let id = workspace.add_temporary_item(S1).unwrap();
    workspace.set_temporary_page(id, url, "Page").unwrap();
    id
}

fn items(workspace: &Workspace, id: FolderId) -> Vec<ItemId> {
    workspace
        .folders()
        .iter()
        .find(|folder| folder.id == id)
        .unwrap()
        .items
        .clone()
}

fn item(workspace: &Workspace, id: ItemId) -> Item {
    workspace
        .items()
        .iter()
        .find(|item| item.id == id)
        .cloned()
        .unwrap()
}

fn ungrouped(workspace: &Workspace) -> Vec<ItemId> {
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    sidebar
        .pins
        .iter()
        .map(|item| item.id)
        .filter(|&id| workspace.folder_of(id).is_none())
        .collect()
}

fn temporary(workspace: &Workspace) -> Vec<ItemId> {
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    sidebar.temporary.iter().map(|item| item.id).collect()
}

#[test]
fn a_pin_dropped_on_a_folder_moves_in_at_the_end() {
    let mut workspace = Workspace::default();
    let a = pin(&mut workspace, "https://a.example/");
    let b = pin(&mut workspace, "https://b.example/");
    let folder = workspace.create_folder(S1, "Reading").unwrap();
    workspace
        .drop_item(
            W1,
            a,
            DropTarget::Folder {
                folder,
                before: None,
            },
            None,
        )
        .unwrap();
    workspace
        .drop_item(
            W1,
            b,
            DropTarget::Folder {
                folder,
                before: None,
            },
            None,
        )
        .unwrap();
    assert_eq!(items(&workspace, folder), [a, b]);
    assert!(ungrouped(&workspace).is_empty());
}

#[test]
fn dragging_inside_a_folder_reorders_it() {
    let mut workspace = Workspace::default();
    let folder = workspace.create_folder(S1, "Reading").unwrap();
    let ids: Vec<ItemId> = ["a", "b", "c"]
        .iter()
        .map(|name| {
            let id = pin(&mut workspace, &format!("https://{name}.example/"));
            workspace
                .move_pin_to_folder(id, Some(folder), None)
                .unwrap();
            id
        })
        .collect();
    let [a, b, c] = ids[..] else { unreachable!() };
    workspace
        .drop_item(
            W1,
            c,
            DropTarget::Folder {
                folder,
                before: Some(a),
            },
            None,
        )
        .unwrap();
    assert_eq!(items(&workspace, folder), [c, a, b]);
    workspace
        .drop_item(
            W1,
            c,
            DropTarget::Folder {
                folder,
                before: None,
            },
            None,
        )
        .unwrap();
    assert_eq!(items(&workspace, folder), [a, b, c]);
    // Dropping a row above itself changes nothing.
    let before = workspace.snapshot();
    workspace
        .drop_item(
            W1,
            b,
            DropTarget::Folder {
                folder,
                before: Some(b),
            },
            None,
        )
        .unwrap();
    assert_eq!(workspace.snapshot(), before);
}

#[test]
fn a_today_tab_dropped_on_a_folder_is_pinned_into_it() {
    let mut workspace = Workspace::default();
    let folder = workspace.create_folder(S1, "Reading").unwrap();
    let kept = pin(&mut workspace, "https://kept.example/");
    workspace
        .move_pin_to_folder(kept, Some(folder), None)
        .unwrap();
    let tab = today(&mut workspace, "https://remembered.example/");
    workspace
        .drop_item(
            W1,
            tab,
            DropTarget::Folder {
                folder,
                before: Some(kept),
            },
            Some("https://live.example/"),
        )
        .unwrap();
    assert_eq!(items(&workspace, folder), [tab, kept]);
    let moved = item(&workspace, tab);
    assert_eq!(moved.placement, Placement::Pin { space_id: S1 });
    // A live page saves where it is now, not the page the row remembers.
    assert_eq!(
        moved.canonical_url.as_deref(),
        Some("https://live.example/")
    );

    // A dormant Today tab pins the page it remembers.
    let dormant = today(&mut workspace, "https://dormant.example/");
    workspace
        .drop_item(
            W1,
            dormant,
            DropTarget::Folder {
                folder,
                before: None,
            },
            None,
        )
        .unwrap();
    assert_eq!(
        item(&workspace, dormant).canonical_url.as_deref(),
        Some("https://dormant.example/")
    );
}

#[test]
fn a_pin_dragged_out_of_a_folder_joins_the_ungrouped_pins() {
    let mut workspace = Workspace::default();
    let a = pin(&mut workspace, "https://a.example/");
    let b = pin(&mut workspace, "https://b.example/");
    let folder = workspace.create_folder(S1, "Reading").unwrap();
    workspace.move_pin_to_folder(b, Some(folder), None).unwrap();
    workspace
        .drop_item(W1, b, DropTarget::Pins { before: Some(a) }, None)
        .unwrap();
    assert!(items(&workspace, folder).is_empty());
    assert_eq!(ungrouped(&workspace), [b, a]);
    workspace
        .drop_item(W1, b, DropTarget::Pins { before: None }, None)
        .unwrap();
    assert_eq!(ungrouped(&workspace), [a, b]);
}

#[test]
fn a_today_tab_dropped_among_the_pins_is_pinned() {
    let mut workspace = Workspace::default();
    let a = pin(&mut workspace, "https://a.example/");
    let tab = today(&mut workspace, "https://tab.example/");
    workspace
        .drop_item(W1, tab, DropTarget::Pins { before: Some(a) }, None)
        .unwrap();
    assert_eq!(ungrouped(&workspace), [tab, a]);
    assert!(temporary(&workspace).is_empty());
}

#[test]
fn a_pin_dragged_to_today_keeps_its_page_and_loses_its_name() {
    let mut workspace = Workspace::default();
    let first = today(&mut workspace, "https://first.example/");
    let folder = workspace.create_folder(S1, "Reading").unwrap();
    let a = pin(&mut workspace, "https://a.example/");
    workspace.set_title(a, "Docs").unwrap();
    workspace.rename_item(a, "My docs").unwrap();
    workspace.move_pin_to_folder(a, Some(folder), None).unwrap();
    workspace.select_item(W1, a).unwrap();
    workspace
        .drop_item(
            W1,
            a,
            DropTarget::Today {
                before: Some(first),
            },
            None,
        )
        .unwrap();
    let moved = item(&workspace, a);
    assert_eq!(moved.placement, Placement::Temporary { space_id: S1 });
    // The dormant row still reopens its page under its title.
    assert_eq!(moved.canonical_url.as_deref(), Some("https://a.example/"));
    assert_eq!(moved.title, "Docs");
    assert!(moved.name.is_empty());
    assert!(items(&workspace, folder).is_empty());
    assert_eq!(temporary(&workspace), [a, first]);
    // It stays selected: it is still in the window's Space.
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(a)
    );
    // A Pin that came back from Today survives a reload as a Today row.
    let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    assert_eq!(item(&restored, a), moved);
}

#[test]
fn today_tabs_reorder_by_dragging() {
    let mut workspace = Workspace::default();
    let a = today(&mut workspace, "https://a.example/");
    let b = today(&mut workspace, "https://b.example/");
    workspace
        .drop_item(W1, b, DropTarget::Today { before: Some(a) }, None)
        .unwrap();
    assert_eq!(temporary(&workspace), [b, a]);
}

#[test]
fn bad_drops_change_nothing() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let a = pin(&mut workspace, "https://a.example/");
    let b = pin(&mut workspace, "https://b.example/");
    let tab = today(&mut workspace, "https://tab.example/");
    let folder = workspace.create_folder(S1, "Reading").unwrap();
    workspace.move_pin_to_folder(b, Some(folder), None).unwrap();
    let elsewhere = workspace.create_folder(work, "Clients").unwrap();
    let favorite = pin(&mut workspace, "https://fav.example/");
    workspace
        .set_placement(favorite, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    let other = workspace.add_temporary_item(work).unwrap();
    // A Today tab that never showed a page has no URL to pin.
    let blank = workspace.add_temporary_item(S1).unwrap();
    let before = workspace.snapshot();
    let cases = [
        (
            favorite,
            DropTarget::Folder {
                folder,
                before: None,
            },
            WorkspaceError::NotDraggable(favorite),
        ),
        (
            other,
            DropTarget::Today { before: None },
            WorkspaceError::NotDraggable(other),
        ),
        (
            a,
            DropTarget::Folder {
                folder: elsewhere,
                before: None,
            },
            WorkspaceError::DifferentFolderSpace(elsewhere),
        ),
        (
            a,
            DropTarget::Folder {
                folder: 99,
                before: None,
            },
            WorkspaceError::UnknownFolder(99),
        ),
        (
            a,
            DropTarget::Folder {
                folder,
                before: Some(tab),
            },
            WorkspaceError::DifferentSection(tab),
        ),
        (
            tab,
            DropTarget::Pins { before: Some(b) },
            WorkspaceError::DifferentSection(b),
        ),
        (
            a,
            DropTarget::Today { before: Some(b) },
            WorkspaceError::DifferentSection(b),
        ),
        (
            blank,
            DropTarget::Folder {
                folder,
                before: None,
            },
            WorkspaceError::CanonicalUrlRequired,
        ),
    ];
    for (id, target, error) in cases {
        assert_eq!(
            workspace.drop_item(W1, id, target, None).unwrap_err(),
            error
        );
        assert_eq!(workspace.snapshot(), before);
    }
}

#[test]
fn rename_wins_until_cleared() {
    let mut workspace = Workspace::default();
    let a = pin(&mut workspace, "https://a.example/");
    workspace.set_title(a, "Page title").unwrap();
    assert!(workspace.rename_item(a, "  Team\ndocs  ").unwrap());
    assert_eq!(item(&workspace, a).name, "Team docs");
    assert!(!workspace.rename_item(a, "Team docs").unwrap());
    // A new page title never touches the name.
    workspace.set_title(a, "Other title").unwrap();
    assert_eq!(item(&workspace, a).name, "Team docs");
    // It survives a reload, and moving to Favorites.
    let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    assert_eq!(item(&restored, a).name, "Team docs");
    workspace
        .set_placement(a, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    assert_eq!(item(&workspace, a).name, "Team docs");
    // An empty name goes back to the page title.
    assert!(workspace.rename_item(a, "   ").unwrap());
    assert!(item(&workspace, a).name.is_empty());
    assert_eq!(item(&workspace, a).title, "Other title");

    let long = "é".repeat(MAX_TITLE_BYTES);
    workspace.rename_item(a, &long).unwrap();
    assert_eq!(item(&workspace, a).name.len(), MAX_TITLE_BYTES);

    let tab = today(&mut workspace, "https://tab.example/");
    assert_eq!(
        workspace.rename_item(tab, "x").unwrap_err(),
        WorkspaceError::NotSaved(tab)
    );
    assert_eq!(
        workspace.rename_item(99, "x").unwrap_err(),
        WorkspaceError::UnknownItem(99)
    );
}

#[test]
fn unpinning_drops_the_name() {
    let mut workspace = Workspace::default();
    let a = pin(&mut workspace, "https://a.example/");
    workspace.rename_item(a, "Mine").unwrap();
    workspace.unsave_item(a, Some(S1)).unwrap();
    assert!(item(&workspace, a).name.is_empty());
}

#[test]
fn a_bad_stored_name_loads_as_none() {
    let mut workspace = Workspace::default();
    let a = pin(&mut workspace, "https://a.example/");
    let b = pin(&mut workspace, "https://b.example/");
    let c = pin(&mut workspace, "https://c.example/");
    let tab = today(&mut workspace, "https://tab.example/");
    let mut snapshot = workspace.snapshot();
    for (item, name) in snapshot.items.iter_mut().zip([
        "x".repeat(MAX_TITLE_BYTES + 1),
        "a\tb".to_owned(),
        "Fine".to_owned(),
        "Today".to_owned(),
    ]) {
        item.name = name;
    }
    let restored = Workspace::from_snapshot(snapshot).unwrap();
    assert!(item(&restored, a).name.is_empty());
    assert!(item(&restored, b).name.is_empty());
    assert_eq!(item(&restored, c).name, "Fine");
    assert!(item(&restored, tab).name.is_empty());
}

#[test]
fn next_tab_skips_pins_in_closed_folders() {
    let mut workspace = Workspace::default();
    let favorite = pin(&mut workspace, "https://fav.example/");
    workspace
        .set_placement(favorite, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    let loose = pin(&mut workspace, "https://loose.example/");
    let open = workspace.create_folder(S1, "Open").unwrap();
    let closed = workspace.create_folder(S1, "Closed").unwrap();
    let in_open = pin(&mut workspace, "https://open.example/");
    let hidden = pin(&mut workspace, "https://hidden.example/");
    let shown = pin(&mut workspace, "https://shown.example/");
    // Folder order, not workspace order, sets where a Pin steps.
    let late = pin(&mut workspace, "https://late.example/");
    workspace
        .move_pin_to_folder(late, Some(open), None)
        .unwrap();
    workspace
        .move_pin_to_folder(in_open, Some(open), Some(late))
        .unwrap();
    workspace
        .move_pin_to_folder(hidden, Some(closed), None)
        .unwrap();
    workspace
        .move_pin_to_folder(shown, Some(closed), None)
        .unwrap();
    workspace.set_folder_open(closed, false).unwrap();
    let tab = today(&mut workspace, "https://tab.example/");

    let order = |workspace: &Workspace| workspace.sidebar_snapshot(W1).unwrap().row_order();
    assert_eq!(order(&workspace), [favorite, loose, in_open, late, tab]);
    // A closed folder still shows its selected Pin, so the keyboard reaches it.
    workspace.select_item(W1, shown).unwrap();
    assert_eq!(
        order(&workspace),
        [favorite, loose, in_open, late, shown, tab]
    );
    workspace.set_folder_open(closed, true).unwrap();
    assert_eq!(
        order(&workspace),
        [favorite, loose, in_open, late, hidden, shown, tab]
    );
}
