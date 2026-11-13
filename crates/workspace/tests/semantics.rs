use myarc_workspace::*;

const S1: SpaceId = TEST_SPACE_ID;
const P1: ProfileId = DEFAULT_PROFILE_ID;
const W1: WindowId = DEFAULT_WINDOW_ID;
const URL: &str = "https://example.test/saved";

fn pin(workspace: &mut Workspace, space: SpaceId) -> ItemId {
    let id = workspace.add_temporary_item(space).unwrap();
    workspace
        .set_placement(id, Placement::Pin { space_id: space }, Some(URL))
        .unwrap();
    id
}

fn find(workspace: &Workspace, id: ItemId) -> Option<Item> {
    workspace
        .snapshot()
        .items
        .into_iter()
        .find(|item| item.id == id)
}

fn unchanged<T: std::fmt::Debug>(
    workspace: &mut Workspace,
    command: impl FnOnce(&mut Workspace) -> Result<T, WorkspaceError>,
    error: WorkspaceError,
) {
    let before = workspace.snapshot();
    assert_eq!(command(workspace).unwrap_err(), error);
    assert_eq!(workspace.snapshot(), before);
}

#[test]
fn native_activation_reveals_owner_and_keeps_favorite_space() {
    let mut workspace = Workspace::default();
    let other = workspace.create_space(P1, "Other").unwrap();
    let local = pin(&mut workspace, other);
    let temporary = workspace.add_temporary_item(S1).unwrap();
    let favorite = pin(&mut workspace, S1);
    workspace
        .set_placement(favorite, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    workspace.reveal_item(W1, local).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().current_space.id,
        other
    );
    workspace.reveal_item(W1, favorite).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().current_space.id,
        other
    );
    workspace.reveal_item(W1, temporary).unwrap();
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(sidebar.current_space.id, S1);
    assert_eq!(sidebar.selected_item_id, Some(temporary));
    unchanged(
        &mut workspace,
        |w| w.reveal_item(W1, 999),
        WorkspaceError::UnknownItem(999),
    );
    unchanged(
        &mut workspace,
        |w| w.reveal_item(999, local),
        WorkspaceError::UnknownWindow(999),
    );
}

#[test]
fn spaces_share_one_profile_but_keep_local_items_and_order() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Research").unwrap();
    let first = pin(&mut workspace, S1);
    let favorite = pin(&mut workspace, S1);
    let third = pin(&mut workspace, S1);
    let local = pin(&mut workspace, s2);
    let temporary = workspace.add_temporary_item(S1).unwrap();
    workspace
        .set_placement(favorite, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();

    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(sidebar.spaces.len(), 2);
    assert!(sidebar.spaces.iter().all(|space| space.profile_id == P1));
    assert_eq!(sidebar.current_space.id, S1);
    assert_eq!(
        sidebar.pins.iter().map(|item| item.id).collect::<Vec<_>>(),
        [first, third]
    );
    assert_eq!(sidebar.favorites[0].id, favorite);
    assert_eq!(sidebar.temporary[0].id, temporary);
    // The Library's Space cards list every Space's Pins, never favorites or
    // temporary items.
    assert_eq!(
        sidebar
            .space_pins
            .iter()
            .map(|item| item.id)
            .collect::<Vec<_>>(),
        [first, third, local]
    );
    workspace.select_space(W1, s2).unwrap();
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(sidebar.current_space.id, s2);
    assert_eq!(
        sidebar.pins.iter().map(|item| item.id).collect::<Vec<_>>(),
        [local]
    );
    assert_eq!(sidebar.favorites[0].id, favorite);
    assert!(sidebar.temporary.is_empty());
    assert_eq!(workspace.space_name(), "Everyday");
    workspace.rename_space(s2, "Study").unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().current_space.name,
        "Study"
    );
}

#[test]
fn selections_are_per_window_and_hidden_selection_clears() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Research").unwrap();
    let w2 = workspace.create_window(s2).unwrap();
    let item = workspace.add_temporary_item(S1).unwrap();
    let other = workspace.add_temporary_item(s2).unwrap();
    workspace.select_item(W1, item).unwrap();
    workspace.select_item(w2, other).unwrap();
    unchanged(
        &mut workspace,
        |w| w.select_item(W1, other),
        WorkspaceError::ItemNotVisible(other),
    );
    workspace.select_space(W1, S1).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(item)
    );
    workspace.select_space(W1, s2).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        None
    );
    assert_eq!(
        workspace.sidebar_snapshot(w2).unwrap().selected_item_id,
        Some(other)
    );
    // Returning to a Space restores its last selection in that window only.
    workspace.select_space(W1, S1).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(item)
    );
    let second = workspace.add_temporary_item(S1).unwrap();
    workspace.select_item(W1, second).unwrap();
    workspace.select_space(W1, s2).unwrap();
    workspace.live_tab_closed(second).unwrap();
    workspace.select_space(W1, S1).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        None
    );

    workspace
        .set_placement(item, Placement::Favorite { profile_id: P1 }, Some(URL))
        .unwrap();
    workspace.select_item(W1, item).unwrap();
    workspace.select_space(W1, s2).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(item)
    );
    workspace
        .set_placement(item, Placement::Pin { space_id: S1 }, None)
        .unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        None
    );
}

#[test]
fn saved_close_reopen_keeps_identity_and_url_temporary_close_removes() {
    let mut workspace = Workspace::default();
    let saved = pin(&mut workspace, S1);
    workspace.select_item(W1, saved).unwrap();
    let before = workspace.snapshot();
    workspace.live_tab_closed(saved).unwrap();
    assert_eq!(workspace.snapshot(), before);
    workspace.select_item(W1, saved).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().pins[0]
            .canonical_url
            .as_deref(),
        Some(URL)
    );
    workspace
        .set_placement(saved, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    let before = workspace.snapshot();
    workspace.live_tab_closed(saved).unwrap();
    workspace.live_tab_closed(saved).unwrap();
    assert_eq!(workspace.snapshot(), before);

    let temporary = workspace.add_temporary_item(S1).unwrap();
    let w2 = workspace.create_window(S1).unwrap();
    workspace.select_item(W1, temporary).unwrap();
    workspace.select_item(w2, temporary).unwrap();
    workspace.live_tab_closed(temporary).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(w2).unwrap().selected_item_id,
        None
    );
    assert!(workspace.sidebar_snapshot(w2).unwrap().temporary.is_empty());
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert!(sidebar.temporary.is_empty());
    assert_eq!(sidebar.selected_item_id, None);
    assert_eq!(sidebar.favorites[0].id, saved);
    assert!(workspace.add_temporary_item(S1).unwrap() > temporary);
    unchanged(
        &mut workspace,
        |w| w.live_tab_closed(temporary),
        WorkspaceError::UnknownItem(temporary),
    );
}

#[test]
fn saved_removal_is_explicit_and_clears_all_selections() {
    let mut workspace = Workspace::default();
    let saved = pin(&mut workspace, S1);
    let w2 = workspace.create_window(S1).unwrap();
    workspace.select_item(W1, saved).unwrap();
    workspace.select_item(w2, saved).unwrap();
    workspace.remove_saved_item(saved).unwrap();
    for window in [W1, w2] {
        let sidebar = workspace.sidebar_snapshot(window).unwrap();
        assert!(sidebar.pins.is_empty());
        assert_eq!(sidebar.selected_item_id, None);
    }
    unchanged(
        &mut workspace,
        |w| w.remove_saved_item(saved),
        WorkspaceError::UnknownItem(saved),
    );
    let temporary = workspace.add_temporary_item(S1).unwrap();
    unchanged(
        &mut workspace,
        |w| w.remove_saved_item(temporary),
        WorkspaceError::NotSaved(temporary),
    );
}

#[test]
fn canonical_updates_are_deliberate_and_duplicate_urls_keep_distinct_ids() {
    let mut workspace = Workspace::default();
    let first = pin(&mut workspace, S1);
    let second = pin(&mut workspace, S1);
    assert_ne!(first, second);
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(sidebar.pins.len(), 2);
    assert_eq!(sidebar.pins[0].canonical_url, sidebar.pins[1].canonical_url);
    workspace
        .set_placement(first, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().favorites[0]
            .canonical_url
            .as_deref(),
        Some(URL)
    );
    workspace
        .set_placement(
            first,
            Placement::Favorite { profile_id: P1 },
            Some("https://example.test/updated"),
        )
        .unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().favorites[0]
            .canonical_url
            .as_deref(),
        Some("https://example.test/updated")
    );
    workspace.unsave_item(first, Some(S1)).unwrap();
    let temporary = workspace.sidebar_snapshot(W1).unwrap().temporary;
    assert_eq!(temporary[0].id, first);
    assert_eq!(temporary[0].canonical_url, None);
    unchanged(
        &mut workspace,
        |w| w.set_placement(first, Placement::Pin { space_id: S1 }, None),
        WorkspaceError::CanonicalUrlRequired,
    );
}

#[test]
fn missing_ids_ownership_and_names_reject_without_mutation() {
    let mut workspace = Workspace::default();
    let item = workspace.add_temporary_item(S1).unwrap();
    unchanged(
        &mut workspace,
        |w| w.create_space(2, "Other"),
        WorkspaceError::UnknownProfile(2),
    );
    unchanged(
        &mut workspace,
        |w| w.create_space(P1, " "),
        WorkspaceError::InvalidName(RenameError::EmptyName),
    );
    unchanged(
        &mut workspace,
        |w| w.create_space(P1, "bad\nname"),
        WorkspaceError::InvalidName(RenameError::ControlCharacter),
    );
    unchanged(
        &mut workspace,
        |w| w.create_space(P1, &"x".repeat(MAX_SPACE_NAME_BYTES + 1)),
        WorkspaceError::InvalidName(RenameError::NameTooLong),
    );
    unchanged(
        &mut workspace,
        |w| w.create_window(0),
        WorkspaceError::UnknownSpace(0),
    );
    unchanged(
        &mut workspace,
        |w| w.select_space(W1, 99),
        WorkspaceError::UnknownSpace(99),
    );
    unchanged(
        &mut workspace,
        |w| w.select_space(99, S1),
        WorkspaceError::UnknownWindow(99),
    );
    unchanged(
        &mut workspace,
        |w| w.select_item(W1, 0),
        WorkspaceError::UnknownItem(0),
    );
    unchanged(
        &mut workspace,
        |w| w.select_item(0, item),
        WorkspaceError::UnknownWindow(0),
    );
    unchanged(
        &mut workspace,
        |w| w.add_temporary_item(99),
        WorkspaceError::UnknownSpace(99),
    );
    unchanged(
        &mut workspace,
        |w| w.set_placement(99, Placement::Pin { space_id: S1 }, Some(URL)),
        WorkspaceError::UnknownItem(99),
    );
    unchanged(
        &mut workspace,
        |w| w.set_placement(item, Placement::Pin { space_id: 99 }, Some(URL)),
        WorkspaceError::UnknownSpace(99),
    );
    unchanged(
        &mut workspace,
        |w| w.set_placement(item, Placement::Favorite { profile_id: 99 }, Some(URL)),
        WorkspaceError::UnknownProfile(99),
    );
    unchanged(
        &mut workspace,
        |w| w.set_placement(item, Placement::Temporary { space_id: S1 }, Some(URL)),
        WorkspaceError::UnexpectedCanonicalUrl,
    );
    unchanged(
        &mut workspace,
        |w| w.close_window(0),
        WorkspaceError::UnknownWindow(0),
    );
    unchanged(
        &mut workspace,
        |w| w.remove_saved_item(99),
        WorkspaceError::UnknownItem(99),
    );
    assert_eq!(
        workspace.sidebar_snapshot(0),
        Err(WorkspaceError::UnknownWindow(0))
    );
}

#[test]
fn urls_are_bounded_and_gated_before_saving() {
    let mut workspace = Workspace::default();
    let item = workspace.add_temporary_item(S1).unwrap();
    let too_long = format!("https://example.test/{}", "x".repeat(MAX_URL_BYTES));
    for url in [
        "",
        "javascript:alert(1)",
        "file:///tmp/private",
        "https://",
        "https:///missing-host",
        "https://user:secret@example.test/",
        "https://example.test/\n",
        "https://example.test/has space",
        "https://example.test/\u{2003}",
        "https://example.test\\other.test",
        &too_long,
    ] {
        unchanged(
            &mut workspace,
            |w| w.set_placement(item, Placement::Pin { space_id: S1 }, Some(url)),
            WorkspaceError::InvalidUrl,
        );
    }
    let prefix = "https://example.test/";
    let maximum = format!("{prefix}{}", "x".repeat(MAX_URL_BYTES - prefix.len()));
    for url in [maximum.as_str(), "http://example.test/", "about:blank"] {
        workspace
            .set_placement(item, Placement::Pin { space_id: S1 }, Some(url))
            .unwrap();
        assert_eq!(
            workspace.sidebar_snapshot(W1).unwrap().pins[0]
                .canonical_url
                .as_deref(),
            Some(url)
        );
    }
}

#[test]
fn typed_snapshot_round_trip_preserves_order_selection_and_counters() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Research").unwrap();
    let w2 = workspace.create_window(s2).unwrap();
    let saved = pin(&mut workspace, s2);
    let removed = pin(&mut workspace, S1);
    workspace.remove_saved_item(removed).unwrap();
    workspace.select_item(w2, saved).unwrap();
    workspace.close_window(W1).unwrap();
    let snapshot = workspace.snapshot();
    let mut restored = Workspace::from_snapshot(snapshot.clone()).unwrap();
    assert_eq!(restored, workspace);
    assert_eq!(
        restored.sidebar_snapshot(w2),
        workspace.sidebar_snapshot(w2)
    );
    assert!(restored.add_temporary_item(S1).unwrap() > removed);
    assert!(restored.create_window(S1).unwrap() > w2);
    assert!(restored.create_space(P1, "Third").unwrap() > s2);
    let mut target = Workspace::default();
    target.restore(snapshot).unwrap();
    assert_eq!(target, workspace);
}

#[test]
fn sidebar_snapshots_are_owned_and_cannot_mutate_workspace() {
    let mut workspace = Workspace::default();
    pin(&mut workspace, S1);
    let before = workspace.snapshot();
    let mut sidebar = workspace.sidebar_snapshot(W1).unwrap();
    sidebar.current_space.name.clear();
    sidebar.spaces.clear();
    sidebar.pins[0].canonical_url = None;
    assert_eq!(workspace.snapshot(), before);
}

fn assert_bad_snapshot(snapshot: WorkspaceSnapshot) {
    assert!(Workspace::from_snapshot(snapshot.clone()).is_err());
    let mut workspace = Workspace::default();
    workspace.rename_space(S1, "Keep this state").unwrap();
    let before = workspace.snapshot();
    assert!(workspace.restore(snapshot).is_err());
    assert_eq!(workspace.snapshot(), before);
}

#[test]
fn newer_and_older_snapshot_versions_reject_without_replacement() {
    for version in [0, SNAPSHOT_VERSION + 1, u32::MAX] {
        let snapshot = WorkspaceSnapshot {
            version,
            ..Workspace::default().snapshot()
        };
        assert_eq!(
            Workspace::from_snapshot(snapshot.clone()),
            Err(WorkspaceError::UnsupportedSnapshotVersion(version))
        );
        assert_bad_snapshot(snapshot);
    }
}

#[test]
fn snapshots_reject_duplicate_zero_and_invalid_next_ids() {
    let mut workspace = Workspace::default();
    pin(&mut workspace, S1);
    let original = workspace.snapshot();
    for kind in 0..3 {
        let mut duplicate = original.clone();
        let mut zero = original.clone();
        let mut stale_counter = original.clone();
        match kind {
            0 => {
                duplicate.spaces.push(duplicate.spaces[0].clone());
                zero.spaces[0].id = 0;
                stale_counter.next_space_id = S1;
            }
            1 => {
                duplicate.items.push(duplicate.items[0].clone());
                zero.items[0].id = 0;
                stale_counter.next_item_id = 1;
            }
            _ => {
                duplicate.windows.push(duplicate.windows[0].clone());
                zero.windows[0].id = 0;
                stale_counter.next_window_id = W1;
            }
        }
        for snapshot in [duplicate, zero, stale_counter] {
            assert_bad_snapshot(snapshot);
        }
    }
    let mut snapshot = original;
    snapshot.next_item_id = 0;
    assert_bad_snapshot(snapshot);
}

#[test]
fn snapshots_reject_dangling_owners_inconsistent_urls_and_selections() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Research").unwrap();
    let saved = pin(&mut workspace, S1);
    workspace.select_item(W1, saved).unwrap();
    let original = workspace.snapshot();
    let corruptions: Vec<fn(&mut WorkspaceSnapshot)> = vec![
        |s| s.profile_id = 2,
        |s| s.spaces[0].profile_id = 2,
        |s| s.spaces[0].name.clear(),
        |s| s.spaces[0].name = "x".repeat(MAX_SPACE_NAME_BYTES + 1),
        |s| s.spaces[0].name = "bad\nname".into(),
        |s| s.items[0].placement = Placement::Pin { space_id: 99 },
        |s| s.items[0].placement = Placement::Temporary { space_id: 99 },
        |s| s.items[0].placement = Placement::Favorite { profile_id: 2 },
        |s| {
            s.items[0].placement = Placement::Temporary { space_id: S1 };
            s.items[0].canonical_url = Some("chrome://newtab".into());
        },
        |s| s.items[0].canonical_url = None,
        |s| s.items[0].canonical_url = Some("javascript:alert(1)".into()),
        |s| s.items[0].canonical_url = Some("x".repeat(MAX_URL_BYTES + 1)),
        |s| s.windows[0].space_id = 99,
        |s| s.windows[0].selected_item_id = Some(99),
    ];
    for corrupt in corruptions {
        let mut snapshot = original.clone();
        corrupt(&mut snapshot);
        assert_bad_snapshot(snapshot);
    }
    let mut snapshot = original;
    snapshot.windows[0].space_id = s2;
    assert_bad_snapshot(snapshot);
}

#[test]
fn commands_and_restore_enforce_collection_limits() {
    let mut workspace = Workspace::default();
    for _ in 1..MAX_SPACES {
        workspace.create_space(P1, "Space").unwrap();
    }
    unchanged(
        &mut workspace,
        |w| w.create_space(P1, "Extra"),
        WorkspaceError::LimitExceeded("Spaces"),
    );
    for _ in 1..MAX_WINDOWS {
        workspace.create_window(S1).unwrap();
    }
    unchanged(
        &mut workspace,
        |w| w.create_window(S1),
        WorkspaceError::LimitExceeded("windows"),
    );
    for _ in 0..MAX_ITEMS {
        workspace.add_temporary_item(S1).unwrap();
    }
    unchanged(
        &mut workspace,
        |w| w.add_temporary_item(S1),
        WorkspaceError::LimitExceeded("items"),
    );
    let snapshot = workspace.snapshot();
    assert_eq!(
        Workspace::from_snapshot(snapshot.clone()).unwrap(),
        workspace
    );
    let all_live: Vec<_> = snapshot.items.iter().map(|item| item.id).collect();
    workspace.reconcile_live_items(&all_live).unwrap();
    assert_eq!(workspace.snapshot(), snapshot);
    for kind in 0..3 {
        let mut excessive = snapshot.clone();
        match kind {
            0 => excessive.spaces.push(excessive.spaces[0].clone()),
            1 => excessive.items.push(excessive.items[0].clone()),
            _ => excessive.windows.push(excessive.windows[0].clone()),
        }
        assert_eq!(
            Workspace::from_snapshot(excessive.clone()),
            Err(WorkspaceError::InvalidSnapshot(
                "collection count exceeds workspace limits"
            ))
        );
        assert_bad_snapshot(excessive);
    }
    workspace.close_window(W1).unwrap();
    assert!(workspace.create_window(S1).unwrap() > MAX_WINDOWS as u64);
    let removed = workspace.snapshot().items[0].id;
    workspace.live_tab_closed(removed).unwrap();
    assert!(workspace.add_temporary_item(S1).unwrap() > removed);
}

#[test]
fn exhausted_counters_reject_without_mutation() {
    let mut snapshot = Workspace::default().snapshot();
    snapshot.next_space_id = u64::MAX;
    snapshot.next_item_id = u64::MAX;
    snapshot.next_window_id = u64::MAX;
    let mut workspace = Workspace::from_snapshot(snapshot).unwrap();
    unchanged(
        &mut workspace,
        |w| w.create_space(P1, "Research"),
        WorkspaceError::IdExhausted("Space"),
    );
    unchanged(
        &mut workspace,
        |w| w.add_temporary_item(S1),
        WorkspaceError::IdExhausted("item"),
    );
    unchanged(
        &mut workspace,
        |w| w.create_window(S1),
        WorkspaceError::IdExhausted("window"),
    );
}

#[test]
fn restore_then_reconcile_prunes_only_unbound_temporary_and_clears_all_selections() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Research").unwrap();
    let w2 = workspace.create_window(S1).unwrap();
    let w3 = workspace.create_window(s2).unwrap();
    let w4 = workspace.create_window(s2).unwrap();
    let unbound = workspace.add_temporary_item(S1).unwrap();
    let bound = workspace.add_temporary_item(s2).unwrap();
    let dormant_pin = pin(&mut workspace, S1);
    let dormant_favorite = pin(&mut workspace, s2);
    workspace
        .set_placement(
            dormant_favorite,
            Placement::Favorite { profile_id: P1 },
            None,
        )
        .unwrap();
    workspace.select_item(W1, unbound).unwrap();
    workspace.select_item(w2, unbound).unwrap();
    workspace.select_item(w3, bound).unwrap();
    workspace.select_item(w4, dormant_favorite).unwrap();
    let original = workspace.snapshot();
    let mut restored = Workspace::from_snapshot(original.clone()).unwrap();
    assert_eq!(restored.snapshot(), original);

    restored.reconcile_live_items(&[bound]).unwrap();
    let after = restored.snapshot();
    assert_eq!(
        after.items,
        original
            .items
            .into_iter()
            .filter(|item| item.id != unbound)
            .collect::<Vec<_>>()
    );
    assert_eq!(after.next_item_id, original.next_item_id);
    assert!(after.items.iter().any(|item| item.id == dormant_pin));
    for window in [W1, w2] {
        let sidebar = restored.sidebar_snapshot(window).unwrap();
        assert!(sidebar.temporary.is_empty());
        assert_eq!(sidebar.selected_item_id, None);
    }
    assert_eq!(
        restored.sidebar_snapshot(w3).unwrap().selected_item_id,
        Some(bound)
    );
    assert_eq!(
        restored.sidebar_snapshot(w4).unwrap().selected_item_id,
        Some(dormant_favorite)
    );
    restored.reconcile_live_items(&[bound]).unwrap();
    assert_eq!(restored.snapshot(), after);

    // Saved IDs may be supplied; an empty live set removes the final temporary.
    restored.reconcile_live_items(&[dormant_pin]).unwrap();
    assert_eq!(
        restored.sidebar_snapshot(w3).unwrap().selected_item_id,
        None
    );
    let saved_only = restored.snapshot();
    restored.reconcile_live_items(&[]).unwrap();
    assert_eq!(restored.snapshot(), saved_only);
    assert_eq!(Workspace::from_snapshot(saved_only).unwrap(), restored);
}

#[test]
fn reconciliation_validates_entire_bounded_live_set_before_pruning() {
    let mut workspace = Workspace::default();
    let unbound = workspace.add_temporary_item(S1).unwrap();
    let live = workspace.add_temporary_item(S1).unwrap();
    workspace.select_item(W1, unbound).unwrap();
    for missing in [0, 99] {
        unchanged(
            &mut workspace,
            |w| w.reconcile_live_items(&[live, missing]),
            WorkspaceError::UnknownItem(missing),
        );
    }
    unchanged(
        &mut workspace,
        |w| w.reconcile_live_items(&[live, live]),
        WorkspaceError::DuplicateLiveItem(live),
    );
    unchanged(
        &mut workspace,
        |w| w.reconcile_live_items(&vec![live; MAX_ITEMS + 1]),
        WorkspaceError::LimitExceeded("live items"),
    );
}

#[test]
fn temporary_item_with_a_page_survives_reconciliation_without_a_tab() {
    let mut workspace = Workspace::default();
    let other = workspace.create_space(P1, "Other").unwrap();
    let remembered = workspace.add_temporary_item(S1).unwrap();
    let blank = workspace.add_temporary_item(S1).unwrap();
    assert_eq!(
        workspace.set_temporary_page(remembered, URL, "Alpha\n"),
        Ok(true)
    );
    assert_eq!(
        workspace.set_temporary_page(remembered, URL, "Alpha "),
        Ok(false)
    );
    unchanged(
        &mut workspace,
        |w| w.set_temporary_page(blank, "chrome://newtab", "New Tab"),
        WorkspaceError::InvalidUrl,
    );
    let saved = pin(&mut workspace, S1);
    unchanged(
        &mut workspace,
        |w| w.set_temporary_page(saved, URL, "Saved"),
        WorkspaceError::ExplicitUnsaveRequired(saved),
    );
    workspace.select_item(W1, remembered).unwrap();

    // Relaunch with no restored tabs: the row stays dormant with its page and
    // selection; only the row with nothing to reopen goes.
    let mut restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    restored.reconcile_live_items(&[]).unwrap();
    let item = find(&restored, remembered).unwrap();
    assert_eq!(item.placement, Placement::Temporary { space_id: S1 });
    assert_eq!(item.canonical_url.as_deref(), Some(URL));
    assert_eq!(item.title, "Alpha ");
    assert!(find(&restored, blank).is_none());
    assert!(find(&restored, saved).is_some());
    assert_eq!(
        restored.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(remembered)
    );
    assert_eq!(
        restored.differs_from_canonical(remembered, URL),
        Err(WorkspaceError::NoCanonicalUrl(remembered))
    );

    // Moving keeps the page; saving needs no URL; unsaving a live page drops it.
    restored
        .set_placement(remembered, Placement::Temporary { space_id: other }, None)
        .unwrap();
    assert_eq!(
        find(&restored, remembered)
            .unwrap()
            .canonical_url
            .as_deref(),
        Some(URL)
    );
    restored
        .set_placement(remembered, Placement::Pin { space_id: other }, None)
        .unwrap();
    assert_eq!(find(&restored, remembered).unwrap().title, "Alpha ");
    restored.unsave_item(remembered, Some(other)).unwrap();
    assert_eq!(find(&restored, remembered).unwrap().canonical_url, None);
    restored.reconcile_live_items(&[]).unwrap();
    assert!(find(&restored, remembered).is_none());
}

#[test]
fn a_remembered_page_keeps_its_full_url() {
    let mut workspace = Workspace::default();
    let id = workspace.add_temporary_item(S1).unwrap();
    let page = "https://example.test/watch?v=abc#t=42";
    assert_eq!(workspace.set_temporary_page(id, page, "Video"), Ok(true));
    let item = find(&workspace, id).unwrap();
    assert_eq!(item.canonical_url.as_deref(), Some(page));
    // Another query is another page.
    assert_eq!(
        workspace.set_temporary_page(id, "https://example.test/watch?v=def", "Video"),
        Ok(true)
    );
}

#[test]
fn a_restored_tab_finds_the_dormant_row_of_its_page() {
    let mut workspace = Workspace::default();
    let other = workspace.create_space(P1, "Other").unwrap();
    let dormant = workspace.add_temporary_item(S1).unwrap();
    workspace
        .set_temporary_page(dormant, "https://example.test/doc?a=1#intro", "Doc")
        .unwrap();
    let live = workspace.add_temporary_item(S1).unwrap();
    workspace
        .set_temporary_page(live, "https://example.test/live", "Live")
        .unwrap();
    let saved = pin(&mut workspace, S1);
    // Matched by full URL, ignoring the fragment.
    let page = "https://example.test/doc?a=1#top";
    assert_eq!(
        workspace.dormant_temporary_item(S1, page, &[live]),
        Some(dormant)
    );
    assert_eq!(
        workspace.dormant_temporary_item(S1, "https://example.test/doc?a=2", &[live]),
        None
    );
    // Only dormant temporary rows of that Space, by page.
    assert_eq!(workspace.dormant_temporary_item(other, page, &[live]), None);
    assert_eq!(workspace.dormant_temporary_item(S1, page, &[dormant]), None);
    assert_eq!(
        workspace.dormant_temporary_item(S1, "https://example.test/live", &[live]),
        None
    );
    assert_eq!(workspace.dormant_temporary_item(S1, URL, &[]), None);
    assert!(find(&workspace, saved).is_some());
}

#[test]
fn implicit_saved_demotion_rejects_and_unsave_validates_before_mutation() {
    let mut workspace = Workspace::default();
    let saved = pin(&mut workspace, S1);
    let favorite = pin(&mut workspace, S1);
    workspace
        .set_placement(favorite, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    workspace.select_item(W1, saved).unwrap();
    for id in [saved, favorite] {
        unchanged(
            &mut workspace,
            |w| w.set_placement(id, Placement::Temporary { space_id: S1 }, None),
            WorkspaceError::ExplicitUnsaveRequired(id),
        );
        unchanged(
            &mut workspace,
            |w| w.unsave_item(id, Some(99)),
            WorkspaceError::UnknownSpace(99),
        );
    }
    for destination in [None, Some(S1)] {
        unchanged(
            &mut workspace,
            |w| w.unsave_item(99, destination),
            WorkspaceError::UnknownItem(99),
        );
    }
    let temporary = workspace.add_temporary_item(S1).unwrap();
    for destination in [None, Some(S1)] {
        unchanged(
            &mut workspace,
            |w| w.unsave_item(temporary, destination),
            WorkspaceError::NotSaved(temporary),
        );
    }
}

#[test]
fn unsave_live_keeps_identity_and_order_clears_url_and_hidden_selections() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Research").unwrap();
    let w2 = workspace.create_window(s2).unwrap();
    let w3 = workspace.create_window(S1).unwrap();
    let saved = pin(&mut workspace, S1);
    workspace
        .set_placement(saved, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    let other = workspace.add_temporary_item(S1).unwrap();
    for window in [W1, w2, w3] {
        workspace.select_item(window, saved).unwrap();
    }
    workspace.unsave_item(saved, Some(S1)).unwrap();
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(
        sidebar
            .temporary
            .iter()
            .map(|item| item.id)
            .collect::<Vec<_>>(),
        [saved, other]
    );
    assert_eq!(sidebar.temporary[0].canonical_url, None);
    for window in [W1, w3] {
        assert_eq!(
            workspace.sidebar_snapshot(window).unwrap().selected_item_id,
            Some(saved)
        );
    }
    assert_eq!(
        workspace.sidebar_snapshot(w2).unwrap().selected_item_id,
        None
    );

    // Pin-to-Temporary may report a different live Space, independent of old owner.
    workspace
        .set_placement(saved, Placement::Pin { space_id: S1 }, Some(URL))
        .unwrap();
    workspace.unsave_item(saved, Some(s2)).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        None
    );
    assert_eq!(
        workspace.sidebar_snapshot(w3).unwrap().selected_item_id,
        None
    );
    assert_eq!(
        workspace.sidebar_snapshot(w2).unwrap().temporary[0].id,
        saved
    );
    assert_eq!(
        workspace.sidebar_snapshot(w2).unwrap().temporary[0].canonical_url,
        None
    );
}

#[test]
fn unsave_dormant_explicitly_removes_saved_identity_and_all_selections() {
    let mut workspace = Workspace::default();
    let w2 = workspace.create_window(S1).unwrap();
    for placement in [
        Placement::Pin { space_id: S1 },
        Placement::Favorite { profile_id: P1 },
    ] {
        let saved = pin(&mut workspace, S1);
        workspace.set_placement(saved, placement, None).unwrap();
        for window in [W1, w2] {
            workspace.select_item(window, saved).unwrap();
        }
        workspace.live_tab_closed(saved).unwrap();
        workspace.unsave_item(saved, None).unwrap();
        assert!(
            !workspace
                .snapshot()
                .items
                .iter()
                .any(|item| item.id == saved)
        );
        for window in [W1, w2] {
            assert_eq!(
                workspace.sidebar_snapshot(window).unwrap().selected_item_id,
                None
            );
        }
        assert!(workspace.add_temporary_item(S1).unwrap() > saved);
    }
}

#[test]
fn reorder_each_section_preserves_identity_urls_selections_and_other_section_order() {
    for placement in [
        Placement::Temporary { space_id: S1 },
        Placement::Pin { space_id: S1 },
        Placement::Favorite { profile_id: P1 },
    ] {
        let mut workspace = Workspace::default();
        let s2 = workspace.create_space(P1, "Research").unwrap();
        let w2 = workspace.create_window(S1).unwrap();
        let mut ids = Vec::new();
        let mut other_ids = Vec::new();
        for _ in 0..3 {
            let id = workspace.add_temporary_item(S1).unwrap();
            if !matches!(placement, Placement::Temporary { .. }) {
                workspace.set_placement(id, placement, Some(URL)).unwrap();
            }
            ids.push(id);
            other_ids.push(pin(&mut workspace, s2));
        }
        let [first, second, third] = [ids[0], ids[1], ids[2]];
        workspace.select_item(W1, second).unwrap();
        workspace.select_item(w2, third).unwrap();
        let before = workspace.snapshot();
        for (item, target, expected) in [
            (third, Some(first), [third, first, second]),
            (third, None, [first, second, third]),
            (first, Some(third), [second, first, third]),
            (first, Some(first), [second, first, third]),
            (third, None, [second, first, third]),
            (first, Some(second), [first, second, third]),
        ] {
            workspace.move_item_before(item, target).unwrap();
            let after = workspace.snapshot();
            assert_eq!(
                after
                    .items
                    .iter()
                    .filter(|i| i.placement == placement)
                    .map(|i| i.id)
                    .collect::<Vec<_>>(),
                expected
            );
            assert_eq!(
                after
                    .items
                    .iter()
                    .filter(|i| i.placement != placement)
                    .map(|i| i.id)
                    .collect::<Vec<_>>(),
                other_ids
            );
            let mut normalized = after;
            normalized.items = before.items.clone();
            assert_eq!(normalized, before);
            for item in &workspace.snapshot().items {
                assert_eq!(
                    Some(item),
                    before.items.iter().find(|old| old.id == item.id)
                );
            }
        }
        let snapshot = workspace.snapshot();
        assert_eq!(Workspace::from_snapshot(snapshot).unwrap(), workspace);
    }
}

#[test]
fn reorder_rejects_unknown_ids_and_cross_placement_or_owner_without_mutation() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Research").unwrap();
    let saved = pin(&mut workspace, S1);
    let other_pin = pin(&mut workspace, s2);
    let temporary = workspace.add_temporary_item(S1).unwrap();
    let other_temporary = workspace.add_temporary_item(s2).unwrap();
    let favorite = pin(&mut workspace, S1);
    workspace
        .set_placement(favorite, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    unchanged(
        &mut workspace,
        |w| w.move_item_before(99, None),
        WorkspaceError::UnknownItem(99),
    );
    unchanged(
        &mut workspace,
        |w| w.move_item_before(saved, Some(99)),
        WorkspaceError::UnknownItem(99),
    );
    for (item, target) in [
        (saved, other_pin),
        (temporary, other_temporary),
        (saved, temporary),
        (saved, favorite),
    ] {
        unchanged(
            &mut workspace,
            |w| w.move_item_before(item, Some(target)),
            WorkspaceError::DifferentSection(target),
        );
    }
}

#[test]
fn snapshot_without_space_one_restores_and_round_trips() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Research").unwrap();
    let saved = pin(&mut workspace, s2);
    workspace.select_space(W1, s2).unwrap();
    workspace.select_item(W1, saved).unwrap();
    let mut snapshot = workspace.snapshot();
    snapshot.spaces.retain(|space| space.id != S1);
    let mut restored = Workspace::from_snapshot(snapshot.clone()).unwrap();
    assert_eq!(restored.snapshot(), snapshot);
    assert_eq!(restored.space_name(), "Research");
    assert_eq!(restored.sidebar_snapshot(W1).unwrap().pins[0].id, saved);
    let mut target = Workspace::default();
    target.restore(snapshot).unwrap();
    assert_eq!(target, restored);
    assert_eq!(restored.create_space(P1, "Next"), Ok(s2 + 1));
}

#[test]
fn snapshot_without_any_space_rejects_without_replacement() {
    let mut snapshot = Workspace::default().snapshot();
    snapshot.spaces.clear();
    snapshot.windows.clear();
    assert!(matches!(
        Workspace::from_snapshot(snapshot.clone()),
        Err(WorkspaceError::InvalidSnapshot(_))
    ));
    assert_bad_snapshot(snapshot);
}

#[test]
fn old_snapshot_with_space_one_loads_unchanged() {
    // Shape written before Space 1 became deletable: Space 1 plus one more.
    let snapshot = WorkspaceSnapshot {
        version: SNAPSHOT_VERSION,
        profile_id: P1,
        spaces: vec![
            Space {
                id: S1,
                profile_id: P1,
                name: "Everyday".into(),
                icon: None,
                theme: None,
                agent: false,
            },
            Space {
                id: 2,
                profile_id: P1,
                name: "Work".into(),
                icon: None,
                theme: None,
                agent: false,
            },
        ],
        items: vec![Item {
            id: 1,
            placement: Placement::Pin { space_id: S1 },
            canonical_url: Some(URL.into()),
            title: String::new(),
            name: String::new(),
            last_viewed_secs: 0,
        }],
        windows: vec![WindowState {
            id: W1,
            space_id: S1,
            selected_item_id: Some(1),
        }],
        next_space_id: 3,
        next_item_id: 2,
        archived: Vec::new(),
        open_secs: 0,
        archive_interval: Default::default(),
        folders: Vec::new(),
        next_window_id: 2,
        next_folder_id: 1,
    };
    let workspace = Workspace::from_snapshot(snapshot.clone()).unwrap();
    assert_eq!(workspace.snapshot(), snapshot);
    assert_eq!(workspace.space_name(), "Everyday");
}

#[test]
fn zero_windows_round_trip_requires_explicit_window_allocation() {
    let mut workspace = Workspace::default();
    let saved = pin(&mut workspace, S1);
    workspace.select_item(W1, saved).unwrap();
    workspace.close_window(W1).unwrap();
    let snapshot = workspace.snapshot();
    assert!(snapshot.windows.is_empty());
    let mut restored = Workspace::from_snapshot(snapshot.clone()).unwrap();
    assert_eq!(restored.snapshot(), snapshot);
    assert_eq!(
        restored.sidebar_snapshot(W1),
        Err(WorkspaceError::UnknownWindow(W1))
    );
    restored.reconcile_live_items(&[]).unwrap();
    assert_eq!(restored.snapshot(), snapshot);
    let window = restored.create_window(S1).unwrap();
    assert!(window > W1);
    assert_eq!(restored.sidebar_snapshot(window).unwrap().pins[0].id, saved);
}

#[test]
fn url_gate_is_only_a_shape_check_and_accepts_about_blank() {
    let mut workspace = Workspace::default();
    let item = workspace.add_temporary_item(S1).unwrap();
    for url in [
        "https://?query",
        "http://#fragment",
        "about:blank#fragment",
        "ABOUT:blank",
    ] {
        unchanged(
            &mut workspace,
            |w| w.set_placement(item, Placement::Pin { space_id: S1 }, Some(url)),
            WorkspaceError::InvalidUrl,
        );
    }
    // Deliberately malformed host syntax demonstrates the native-validation boundary.
    // Adapters must reject this before calling core; this gate is not a URL parser.
    for url in ["https://[invalid-host/", "about:blank"] {
        workspace
            .set_placement(item, Placement::Pin { space_id: S1 }, Some(url))
            .unwrap();
        let snapshot = workspace.snapshot();
        assert_eq!(Workspace::from_snapshot(snapshot).unwrap(), workspace);
    }
    assert!(
        WorkspaceError::InvalidUrl
            .to_string()
            .contains("or about:blank")
    );
}

#[test]
fn names_retain_line_separator_and_format_mark_compatibility_for_this_slice() {
    let mut workspace = Workspace::default();
    for name in [
        "Work\u{2028}Notes",
        "Work\u{2029}Notes",
        "Work\u{200f}Notes",
        "Work\u{202e}Notes",
    ] {
        workspace.rename_space(S1, name).unwrap();
        assert_eq!(workspace.space_name(), name);
        assert_eq!(
            Workspace::from_snapshot(workspace.snapshot()).unwrap(),
            workspace
        );
    }
}

#[test]
fn replace_canonical_url_is_explicit_and_survives_snapshot() {
    let mut workspace = Workspace::default();
    let id = pin(&mut workspace, S1);
    let temporary = workspace.add_temporary_item(S1).unwrap();
    assert_eq!(workspace.differs_from_canonical(id, URL), Ok(false));
    assert_eq!(
        workspace.differs_from_canonical(id, "https://example.test/elsewhere"),
        Ok(true)
    );
    workspace
        .set_canonical_url(id, "https://example.test/elsewhere")
        .unwrap();
    assert_eq!(
        workspace.differs_from_canonical(id, "https://example.test/elsewhere"),
        Ok(false)
    );
    assert_eq!(workspace.differs_from_canonical(id, URL), Ok(true));
    unchanged(
        &mut workspace,
        |w| w.set_canonical_url(temporary, URL),
        WorkspaceError::NoCanonicalUrl(temporary),
    );
    unchanged(
        &mut workspace,
        |w| w.set_canonical_url(id, "javascript:alert(1)"),
        WorkspaceError::InvalidUrl,
    );
    unchanged(
        &mut workspace,
        |w| w.set_canonical_url(999, URL),
        WorkspaceError::UnknownItem(999),
    );
    let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    assert_eq!(restored, workspace);
    assert_eq!(
        restored.differs_from_canonical(id, "https://example.test/elsewhere"),
        Ok(false)
    );
}

#[test]
fn delete_space_moves_windows_keeps_favorites_and_round_trips() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Two").unwrap();
    let s3 = workspace.create_space(P1, "Three").unwrap();
    let favorite = pin(&mut workspace, s2);
    workspace
        .set_placement(favorite, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    let doomed = pin(&mut workspace, s2);
    let doomed_temporary = workspace.add_temporary_item(s2).unwrap();
    let survivor = pin(&mut workspace, S1);
    let w2 = workspace.create_window(s2).unwrap();
    let w3 = workspace.create_window(s2).unwrap();
    let w4 = workspace.create_window(s3).unwrap();
    workspace.select_item(W1, survivor).unwrap();
    workspace.select_item(w2, favorite).unwrap();
    workspace.select_item(w3, doomed).unwrap();
    workspace.select_item(w4, doomed_temporary).unwrap_err();

    let deletion = workspace.delete_space(s2).unwrap();
    assert_eq!(deletion.fallback_space, S1);
    assert_eq!(deletion.removed_items, vec![doomed, doomed_temporary]);
    assert_eq!(deletion.moved_windows, vec![w2, w3]);

    let sidebar = workspace.sidebar_snapshot(w2).unwrap();
    assert_eq!(sidebar.current_space.id, S1);
    assert_eq!(
        sidebar.spaces.iter().map(|s| s.id).collect::<Vec<_>>(),
        vec![S1, s3]
    );
    assert_eq!(
        sidebar.favorites.iter().map(|i| i.id).collect::<Vec<_>>(),
        vec![favorite]
    );
    assert_eq!(
        sidebar.pins.iter().map(|i| i.id).collect::<Vec<_>>(),
        vec![survivor]
    );
    assert_eq!(sidebar.selected_item_id, Some(favorite));
    assert_eq!(
        workspace.sidebar_snapshot(w3).unwrap().selected_item_id,
        None
    );
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(survivor)
    );
    let other = workspace.sidebar_snapshot(w4).unwrap();
    assert_eq!(other.current_space.id, s3);
    assert_eq!(other.favorites.len(), 1);

    let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    assert_eq!(restored, workspace);
    assert_eq!(restored.snapshot().next_space_id, s3 + 1);
    assert_eq!(restored.snapshot().next_item_id, doomed_temporary + 2);
    let mut restored = restored;
    assert_eq!(restored.create_space(P1, "Four"), Ok(s3 + 1));
}

#[test]
fn delete_space_rejects_unknown_and_last_without_mutation() {
    let mut workspace = Workspace::default();
    unchanged(
        &mut workspace,
        |w| w.delete_space(S1),
        WorkspaceError::LastSpaceInProfile(S1),
    );
    let s2 = workspace.create_space(P1, "Two").unwrap();
    pin(&mut workspace, s2);
    unchanged(
        &mut workspace,
        |w| w.delete_space(999),
        WorkspaceError::UnknownSpace(999),
    );
    workspace.delete_space(s2).unwrap();
    unchanged(
        &mut workspace,
        |w| w.delete_space(S1),
        WorkspaceError::LastSpaceInProfile(S1),
    );
}

#[test]
fn delete_space_one_when_others_exist_then_last_is_protected() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Two").unwrap();
    let s3 = workspace.create_space(P1, "Three").unwrap();
    let doomed = pin(&mut workspace, S1);
    let kept = pin(&mut workspace, s2);
    let favorite = pin(&mut workspace, S1);
    workspace
        .set_placement(favorite, Placement::Favorite { profile_id: P1 }, None)
        .unwrap();
    let w2 = workspace.create_window(s3).unwrap();
    workspace.select_item(W1, doomed).unwrap();

    let deletion = workspace.delete_space(S1).unwrap();
    assert_eq!(deletion.fallback_space, s2);
    assert_eq!(deletion.removed_items, vec![doomed]);
    assert_eq!(deletion.moved_windows, vec![W1]);
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(sidebar.current_space.id, s2);
    assert_eq!(sidebar.selected_item_id, None);
    assert_eq!(sidebar.pins[0].id, kept);
    assert_eq!(sidebar.favorites[0].id, favorite);
    assert_eq!(workspace.sidebar_snapshot(w2).unwrap().current_space.id, s3);
    assert_eq!(workspace.space_name(), "Two");
    assert_eq!(
        Workspace::from_snapshot(workspace.snapshot()).unwrap(),
        workspace
    );

    workspace.delete_space(s2).unwrap();
    unchanged(
        &mut workspace,
        |w| w.delete_space(s3),
        WorkspaceError::LastSpaceInProfile(s3),
    );
    assert_eq!(workspace.create_space(P1, "Four"), Ok(s3 + 1));
}

#[test]
fn delete_space_with_zero_windows_still_works() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Two").unwrap();
    workspace.close_window(W1).unwrap();
    let deletion = workspace.delete_space(s2).unwrap();
    assert!(deletion.moved_windows.is_empty());
    assert!(Workspace::from_snapshot(workspace.snapshot()).is_ok());
}

fn space_order(workspace: &Workspace) -> Vec<SpaceId> {
    workspace.snapshot().spaces.iter().map(|s| s.id).collect()
}

#[test]
fn move_space_before_reorders_ends_and_keeps_everything_else() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Two").unwrap();
    let s3 = workspace.create_space(P1, "Three").unwrap();
    let saved = pin(&mut workspace, s2);
    workspace.select_space(W1, s2).unwrap();
    workspace.select_item(W1, saved).unwrap();
    let before = workspace.snapshot();
    for (space, target, expected) in [
        (s3, Some(S1), [s3, S1, s2]),
        (s3, None, [S1, s2, s3]),
        (S1, Some(s3), [s2, S1, s3]),
        (S1, Some(S1), [s2, S1, s3]),
        (s3, None, [s2, S1, s3]),
        (s2, None, [S1, s3, s2]),
        (s2, Some(S1), [s2, S1, s3]),
    ] {
        workspace.move_space_before(space, target).unwrap();
        assert_eq!(space_order(&workspace), expected);
        let mut normalized = workspace.snapshot();
        normalized.spaces.sort_by_key(|s| s.id);
        assert_eq!(normalized, before);
    }
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(
        sidebar.spaces.iter().map(|s| s.id).collect::<Vec<_>>(),
        [s2, S1, s3]
    );
    assert_eq!(sidebar.current_space.id, s2);
    let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
    assert_eq!(space_order(&restored), [s2, S1, s3]);
}

#[test]
fn move_space_before_rejects_unknown_ids_without_mutation() {
    let mut workspace = Workspace::default();
    let s2 = workspace.create_space(P1, "Two").unwrap();
    unchanged(
        &mut workspace,
        |w| w.move_space_before(99, None),
        WorkspaceError::UnknownSpace(99),
    );
    unchanged(
        &mut workspace,
        |w| w.move_space_before(s2, Some(99)),
        WorkspaceError::UnknownSpace(99),
    );
    unchanged(
        &mut workspace,
        |w| w.move_space_before(99, Some(99)),
        WorkspaceError::UnknownSpace(99),
    );
    workspace.move_space_before(S1, None).unwrap();
    assert_eq!(space_order(&workspace), [s2, S1]);
}

fn agent_space(workspace: &Workspace, id: SpaceId) -> Space {
    workspace
        .snapshot()
        .spaces
        .into_iter()
        .find(|space| space.id == id)
        .unwrap()
}

#[test]
fn ensure_agent_space_is_idempotent_and_leaves_windows_alone() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let saved = pin(&mut workspace, S1);
    workspace.select_item(W1, saved).unwrap();
    let window = workspace.create_window(work).unwrap();
    assert_eq!(workspace.agent_space_of(P1), None);
    let windows = workspace.snapshot().windows;

    let agent = workspace.ensure_agent_space(P1).unwrap();
    let space = agent_space(&workspace, agent);
    assert_eq!(space.name, AGENT_SPACE_NAME);
    assert_eq!(space.icon, Some(SpaceIcon::Named(AGENT_SPACE_ICON.into())));
    assert!(space.agent);
    assert_eq!(workspace.snapshot().spaces.last().unwrap().id, agent);
    assert_eq!(workspace.agent_space_of(P1), Some(agent));
    assert_eq!(workspace.snapshot().windows, windows);

    let before = workspace.snapshot();
    assert_eq!(workspace.ensure_agent_space(P1), Ok(agent));
    assert_eq!(workspace.snapshot(), before);
    assert_eq!(
        workspace.sidebar_snapshot(window).unwrap().current_space.id,
        work
    );
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(saved)
    );
    unchanged(
        &mut workspace,
        |workspace| workspace.ensure_agent_space(2),
        WorkspaceError::UnknownProfile(2),
    );
}

#[test]
fn agent_space_round_trips_through_snapshot() {
    let mut workspace = Workspace::default();
    let agent = workspace.ensure_agent_space(P1).unwrap();
    let snapshot = workspace.snapshot();
    let restored = Workspace::from_snapshot(snapshot.clone()).unwrap();
    assert_eq!(restored.snapshot(), snapshot);
    assert_eq!(restored.agent_space_of(P1), Some(agent));
}

#[test]
fn renaming_or_restyling_the_agent_space_keeps_the_mark() {
    let mut workspace = Workspace::default();
    let agent = workspace.ensure_agent_space(P1).unwrap();
    workspace.rename_space(agent, "Robots").unwrap();
    workspace
        .set_space_icon(agent, Some(SpaceIcon::Named("rocket".into())))
        .unwrap();
    assert_eq!(workspace.agent_space_of(P1), Some(agent));
    assert_eq!(workspace.ensure_agent_space(P1), Ok(agent));
    let space = agent_space(&workspace, agent);
    assert_eq!(space.name, "Robots");
    assert_eq!(space.icon, Some(SpaceIcon::Named("rocket".into())));
    workspace.set_space_icon(agent, None).unwrap();
    assert_eq!(workspace.agent_space_of(P1), Some(agent));
}

#[test]
fn deleting_the_agent_space_clears_the_mark() {
    let mut workspace = Workspace::default();
    let agent = workspace.ensure_agent_space(P1).unwrap();
    workspace.select_space(W1, agent).unwrap();
    let deletion = workspace.delete_space(agent).unwrap();
    assert_eq!(deletion.fallback_space, S1);
    assert_eq!(workspace.agent_space_of(P1), None);
    assert!(workspace.snapshot().spaces.iter().all(|space| !space.agent));

    // The next ensure makes a fresh Space with a new ID.
    let again = workspace.ensure_agent_space(P1).unwrap();
    assert_ne!(again, agent);
    assert_eq!(workspace.agent_space_of(P1), Some(again));
}

#[test]
fn a_profile_keeps_one_agent_space() {
    let mut workspace = Workspace::default();
    let work = workspace.create_space(P1, "Work").unwrap();
    let agent = workspace.ensure_agent_space(P1).unwrap();
    assert_eq!(workspace.ensure_agent_space(P1), Ok(agent));
    let marked = |workspace: &Workspace| {
        workspace
            .snapshot()
            .spaces
            .iter()
            .filter(|space| space.agent)
            .count()
    };
    assert_eq!(marked(&workspace), 1);

    // A file that marks two Spaces keeps the first mark only.
    let mut snapshot = workspace.snapshot();
    for space in &mut snapshot.spaces {
        space.agent = space.id == work || space.id == agent;
    }
    let restored = Workspace::from_snapshot(snapshot).unwrap();
    assert_eq!(restored.agent_space_of(P1), Some(work));
    assert_eq!(marked(&restored), 1);
}

#[test]
fn agent_space_does_not_count_as_an_untouched_default() {
    let mut snapshot = Workspace::default().snapshot();
    snapshot.spaces[0].agent = true;
    let mut workspace = Workspace::from_snapshot(snapshot).unwrap();
    let created = workspace
        .create_first_space(P1, "Home", None, None)
        .unwrap();
    assert_ne!(created, S1);
    assert_eq!(workspace.agent_space_of(P1), Some(S1));
}

#[test]
fn selecting_an_item_in_a_hidden_space_waits_for_that_space() {
    let mut workspace = Workspace::default();
    let agent = workspace.ensure_agent_space(P1).unwrap();
    let shown = workspace.add_temporary_item(S1).unwrap();
    workspace.select_item(W1, shown).unwrap();
    let first = workspace.add_temporary_item(agent).unwrap();
    let second = workspace.add_temporary_item(agent).unwrap();

    workspace.select_item_in_its_space(W1, first).unwrap();
    workspace.select_item_in_its_space(W1, second).unwrap();
    let sidebar = workspace.sidebar_snapshot(W1).unwrap();
    assert_eq!(sidebar.current_space.id, S1);
    assert_eq!(sidebar.selected_item_id, Some(shown));

    workspace.select_space(W1, agent).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(second)
    );
    // In the shown Space it selects at once.
    workspace.select_item_in_its_space(W1, first).unwrap();
    assert_eq!(
        workspace.sidebar_snapshot(W1).unwrap().selected_item_id,
        Some(first)
    );
    assert_eq!(
        workspace.select_item_in_its_space(W1, 999),
        Err(WorkspaceError::UnknownItem(999))
    );
}
