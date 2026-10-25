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
    workspace.select_item(W1, temporary).unwrap();
    workspace.live_tab_closed(temporary).unwrap();
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
    workspace
        .set_placement(first, Placement::Temporary { space_id: S1 }, None)
        .unwrap();
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
        |s| s.spaces.retain(|space| space.id != S1),
        |s| s.items[0].placement = Placement::Pin { space_id: 99 },
        |s| s.items[0].placement = Placement::Temporary { space_id: 99 },
        |s| s.items[0].placement = Placement::Favorite { profile_id: 2 },
        |s| s.items[0].placement = Placement::Temporary { space_id: S1 },
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
