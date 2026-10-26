chromium::import! {
    "//myarc:workspace";
}

pub struct Workspace(workspace::Workspace);

// All records own their buffers. Calls are synchronous on the browser UI sequence;
// only exported records, never Workspace or native runtime IDs, cross to storage.
#[cxx::bridge(namespace = "myarc::foundation")]
mod ffi {
    struct RenameResult {
        ok: bool,
        name: String,
        error: String,
    }

    struct MutationResult {
        ok: bool,
        id: u64,
        error: String,
    }

    enum PlacementKind {
        Temporary,
        Pin,
        Favorite,
    }

    struct SpaceRecord {
        id: u64,
        profile_id: u64,
        name: String,
    }

    struct ItemRecord {
        id: u64,
        placement: PlacementKind,
        owner_id: u64,
        has_url: bool,
        canonical_url: String,
    }

    struct WindowRecord {
        id: u64,
        space_id: u64,
        // Zero means no selection; core IDs are always nonzero.
        selected_item_id: u64,
    }

    struct StateSnapshot {
        version: u32,
        profile_id: u64,
        next_space_id: u64,
        next_item_id: u64,
        next_window_id: u64,
        spaces: Vec<SpaceRecord>,
        items: Vec<ItemRecord>,
        windows: Vec<WindowRecord>,
    }

    struct SidebarResult {
        ok: bool,
        error: String,
        window_id: u64,
        current_space: SpaceRecord,
        selected_item_id: u64,
        spaces: Vec<SpaceRecord>,
        favorites: Vec<ItemRecord>,
        pins: Vec<ItemRecord>,
        temporary: Vec<ItemRecord>,
    }

    struct BridgeLimits {
        version: u32,
        spaces: usize,
        items: usize,
        windows: usize,
        name_bytes: usize,
        url_bytes: usize,
    }

    extern "Rust" {
        type Workspace;
        fn new_workspace() -> Box<Workspace>;
        fn current_name(workspace: &Workspace) -> String;
        fn rename_space(workspace: &mut Workspace, id: u64, name: &str) -> RenameResult;
        fn bridge_limits() -> BridgeLimits;
        fn create_space(workspace: &mut Workspace, name: &str) -> MutationResult;
        fn create_window(workspace: &mut Workspace, space_id: u64) -> MutationResult;
        fn close_window(workspace: &mut Workspace, window_id: u64) -> MutationResult;
        fn select_space(workspace: &mut Workspace, window_id: u64, space_id: u64)
        -> MutationResult;
        fn add_temporary_item(workspace: &mut Workspace, space_id: u64) -> MutationResult;
        fn select_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> MutationResult;
        fn live_tab_closed(workspace: &mut Workspace, item_id: u64) -> MutationResult;
        fn set_placement(
            workspace: &mut Workspace,
            item_id: u64,
            placement: PlacementKind,
            owner_id: u64,
            has_url: bool,
            canonical_url: &str,
        ) -> MutationResult;
        fn remove_saved_item(workspace: &mut Workspace, item_id: u64) -> MutationResult;
        // Saved items only. Navigation never calls this implicitly.
        fn set_canonical_url(workspace: &mut Workspace, item_id: u64, url: &str) -> MutationResult;
        // Saved items only. `id` is 1 when current_url differs from the saved URL, else 0.
        fn differs_from_canonical(
            workspace: &Workspace,
            item_id: u64,
            current_url: &str,
        ) -> MutationResult;
        // `id` is the Space that windows showing the deleted one now show. The caller
        // confirms first and closes live pages of the deleted Space's items.
        fn delete_space(workspace: &mut Workspace, space_id: u64) -> MutationResult;
        fn reveal_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> MutationResult;
        // Zero destination means no live page; zero before means append.
        fn unsave_item(workspace: &mut Workspace, item_id: u64, live_space: u64) -> MutationResult;
        fn move_item_before(workspace: &mut Workspace, item_id: u64, before: u64)
        -> MutationResult;
        fn reconcile_live_items(workspace: &mut Workspace, live: &[u64]) -> MutationResult;
        fn sidebar_snapshot(workspace: &Workspace, window_id: u64) -> SidebarResult;
        fn export_snapshot(workspace: &Workspace) -> StateSnapshot;
        fn restore_snapshot(workspace: &mut Workspace, snapshot: StateSnapshot) -> MutationResult;
    }
}

pub fn new_workspace() -> Box<Workspace> {
    Box::new(Workspace(workspace::Workspace::default()))
}

pub fn current_name(workspace: &Workspace) -> String {
    workspace.0.space_name().to_owned()
}

pub fn rename_space(workspace: &mut Workspace, id: u64, name: &str) -> ffi::RenameResult {
    let result = workspace.0.rename_space(id, name);
    ffi::RenameResult {
        ok: result.is_ok(),
        name: current_name(workspace),
        error: result
            .err()
            .map(|error| error.to_string())
            .unwrap_or_default(),
    }
}

pub fn bridge_limits() -> ffi::BridgeLimits {
    ffi::BridgeLimits {
        version: workspace::SNAPSHOT_VERSION,
        spaces: workspace::MAX_SPACES,
        items: workspace::MAX_ITEMS,
        windows: workspace::MAX_WINDOWS,
        name_bytes: workspace::MAX_SPACE_NAME_BYTES,
        url_bytes: workspace::MAX_URL_BYTES,
    }
}

fn outcome<E: std::fmt::Display>(result: Result<u64, E>) -> ffi::MutationResult {
    match result {
        Ok(id) => ffi::MutationResult {
            ok: true,
            id,
            error: String::new(),
        },
        Err(error) => ffi::MutationResult {
            ok: false,
            id: 0,
            error: error.to_string(),
        },
    }
}

fn reject(message: &str) -> ffi::MutationResult {
    outcome(Err::<u64, _>(message))
}

pub fn create_space(workspace: &mut Workspace, name: &str) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .create_space(workspace::DEFAULT_PROFILE_ID, name),
    )
}

pub fn create_window(workspace: &mut Workspace, space_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.create_window(space_id))
}

pub fn close_window(workspace: &mut Workspace, window_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.close_window(window_id).map(|()| 0))
}

pub fn select_space(
    workspace: &mut Workspace,
    window_id: u64,
    space_id: u64,
) -> ffi::MutationResult {
    outcome(workspace.0.select_space(window_id, space_id).map(|()| 0))
}

pub fn add_temporary_item(workspace: &mut Workspace, space_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.add_temporary_item(space_id))
}

pub fn select_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.select_item(window_id, item_id).map(|()| 0))
}

pub fn live_tab_closed(workspace: &mut Workspace, item_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.live_tab_closed(item_id).map(|()| 0))
}

fn placement(
    kind: ffi::PlacementKind,
    owner_id: u64,
) -> Result<workspace::Placement, &'static str> {
    match kind {
        ffi::PlacementKind::Temporary => Ok(workspace::Placement::Temporary { space_id: owner_id }),
        ffi::PlacementKind::Pin => Ok(workspace::Placement::Pin { space_id: owner_id }),
        ffi::PlacementKind::Favorite => Ok(workspace::Placement::Favorite {
            profile_id: owner_id,
        }),
        _ => Err("Unknown item placement"),
    }
}

pub fn set_placement(
    workspace: &mut Workspace,
    item_id: u64,
    kind: ffi::PlacementKind,
    owner_id: u64,
    has_url: bool,
    canonical_url: &str,
) -> ffi::MutationResult {
    if !has_url && !canonical_url.is_empty() {
        return reject("URL supplied without has_url");
    }
    let placement = match placement(kind, owner_id) {
        Ok(value) => value,
        Err(error) => return reject(error),
    };
    outcome(
        workspace
            .0
            .set_placement(item_id, placement, has_url.then_some(canonical_url))
            .map(|()| 0),
    )
}

pub fn remove_saved_item(workspace: &mut Workspace, item_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.remove_saved_item(item_id).map(|()| 0))
}

pub fn set_canonical_url(
    workspace: &mut Workspace,
    item_id: u64,
    url: &str,
) -> ffi::MutationResult {
    outcome(workspace.0.set_canonical_url(item_id, url).map(|()| 0))
}

pub fn differs_from_canonical(
    workspace: &Workspace,
    item_id: u64,
    current_url: &str,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .differs_from_canonical(item_id, current_url)
            .map(u64::from),
    )
}

pub fn delete_space(workspace: &mut Workspace, space_id: u64) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .delete_space(space_id)
            .map(|deletion| deletion.fallback_space),
    )
}

pub fn reveal_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.reveal_item(window_id, item_id).map(|()| 0))
}

pub fn unsave_item(
    workspace: &mut Workspace,
    item_id: u64,
    live_space: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .unsave_item(item_id, (live_space != 0).then_some(live_space))
            .map(|()| 0),
    )
}

pub fn move_item_before(
    workspace: &mut Workspace,
    item_id: u64,
    before: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .move_item_before(item_id, (before != 0).then_some(before))
            .map(|()| 0),
    )
}

pub fn reconcile_live_items(workspace: &mut Workspace, live: &[u64]) -> ffi::MutationResult {
    outcome(workspace.0.reconcile_live_items(live).map(|()| 0))
}

fn space_record(space: workspace::Space) -> ffi::SpaceRecord {
    ffi::SpaceRecord {
        id: space.id,
        profile_id: space.profile_id,
        name: space.name,
    }
}

fn item_record(item: workspace::Item) -> ffi::ItemRecord {
    let (placement, owner_id) = match item.placement {
        workspace::Placement::Temporary { space_id } => (ffi::PlacementKind::Temporary, space_id),
        workspace::Placement::Pin { space_id } => (ffi::PlacementKind::Pin, space_id),
        workspace::Placement::Favorite { profile_id } => (ffi::PlacementKind::Favorite, profile_id),
    };
    ffi::ItemRecord {
        id: item.id,
        placement,
        owner_id,
        has_url: item.canonical_url.is_some(),
        canonical_url: item.canonical_url.unwrap_or_default(),
    }
}

pub fn sidebar_snapshot(workspace: &Workspace, window_id: u64) -> ffi::SidebarResult {
    match workspace.0.sidebar_snapshot(window_id) {
        Ok(sidebar) => ffi::SidebarResult {
            ok: true,
            error: String::new(),
            window_id: sidebar.window_id,
            current_space: space_record(sidebar.current_space),
            selected_item_id: sidebar.selected_item_id.unwrap_or(0),
            spaces: sidebar.spaces.into_iter().map(space_record).collect(),
            favorites: sidebar.favorites.into_iter().map(item_record).collect(),
            pins: sidebar.pins.into_iter().map(item_record).collect(),
            temporary: sidebar.temporary.into_iter().map(item_record).collect(),
        },
        Err(error) => ffi::SidebarResult {
            ok: false,
            error: error.to_string(),
            window_id,
            current_space: ffi::SpaceRecord {
                id: 0,
                profile_id: 0,
                name: String::new(),
            },
            selected_item_id: 0,
            spaces: Vec::new(),
            favorites: Vec::new(),
            pins: Vec::new(),
            temporary: Vec::new(),
        },
    }
}

pub fn export_snapshot(workspace: &Workspace) -> ffi::StateSnapshot {
    let state = workspace.0.snapshot();
    ffi::StateSnapshot {
        version: state.version,
        profile_id: state.profile_id,
        next_space_id: state.next_space_id,
        next_item_id: state.next_item_id,
        next_window_id: state.next_window_id,
        spaces: state.spaces.into_iter().map(space_record).collect(),
        items: state.items.into_iter().map(item_record).collect(),
        windows: state
            .windows
            .into_iter()
            .map(|window| ffi::WindowRecord {
                id: window.id,
                space_id: window.space_id,
                selected_item_id: window.selected_item_id.unwrap_or(0),
            })
            .collect(),
    }
}

pub fn restore_snapshot(
    workspace: &mut Workspace,
    snapshot: ffi::StateSnapshot,
) -> ffi::MutationResult {
    if snapshot.spaces.len() > workspace::MAX_SPACES
        || snapshot.items.len() > workspace::MAX_ITEMS
        || snapshot.windows.len() > workspace::MAX_WINDOWS
        || snapshot
            .spaces
            .iter()
            .any(|space| space.name.len() > workspace::MAX_SPACE_NAME_BYTES)
        || snapshot.items.iter().any(|item| {
            item.canonical_url.len() > workspace::MAX_URL_BYTES
                || (!item.has_url && !item.canonical_url.is_empty())
        })
    {
        return reject("Snapshot exceeds bridge bounds or has inconsistent URL presence");
    }
    let mut items = Vec::with_capacity(snapshot.items.len());
    for item in snapshot.items {
        let placement = match placement(item.placement, item.owner_id) {
            Ok(value) => value,
            Err(error) => return reject(error),
        };
        items.push(workspace::Item {
            id: item.id,
            placement,
            canonical_url: item.has_url.then_some(item.canonical_url),
        });
    }
    outcome(
        workspace
            .0
            .restore(workspace::WorkspaceSnapshot {
                version: snapshot.version,
                profile_id: snapshot.profile_id,
                next_space_id: snapshot.next_space_id,
                next_item_id: snapshot.next_item_id,
                next_window_id: snapshot.next_window_id,
                spaces: snapshot
                    .spaces
                    .into_iter()
                    .map(|space| workspace::Space {
                        id: space.id,
                        profile_id: space.profile_id,
                        name: space.name,
                    })
                    .collect(),
                items,
                windows: snapshot
                    .windows
                    .into_iter()
                    .map(|window| workspace::WindowState {
                        id: window.id,
                        space_id: window.space_id,
                        selected_item_id: (window.selected_item_id != 0)
                            .then_some(window.selected_item_id),
                    })
                    .collect(),
            })
            .map(|()| 0),
    )
}
