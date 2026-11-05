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

    enum SpaceIconKind {
        // The default glyph; `icon` is empty.
        Unset,
        // `icon` is one emoji.
        Emoji,
        // `icon` names a bundled icon, such as `book-open`.
        Named,
    }

    struct SpaceRecord {
        id: u64,
        profile_id: u64,
        name: String,
        icon_kind: SpaceIconKind,
        icon: String,
    }

    struct ItemRecord {
        id: u64,
        placement: PlacementKind,
        owner_id: u64,
        has_url: bool,
        canonical_url: String,
        // Last known page title of a saved item; empty when unknown.
        title: String,
        // Browser-open clock reading when a temporary item was last seen;
        // zero when not recorded yet.
        last_viewed_secs: u64,
    }

    struct WindowRecord {
        id: u64,
        space_id: u64,
        // Zero means no selection; core IDs are always nonzero.
        selected_item_id: u64,
    }

    struct ArchivedTabRecord {
        id: u64,
        space_id: u64,
        url: String,
        title: String,
        archived_secs: u64,
    }

    struct AutoArchiveResult {
        // Temporary items to close now; closing archives them.
        due: Vec<u64>,
        // The clock or a last-viewed time changed, so the snapshot needs
        // saving.
        changed: bool,
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
        // Oldest first, as stored.
        archived: Vec<ArchivedTabRecord>,
        // Auto Archive's browser-open clock in seconds; zero before its first
        // pass.
        open_secs: u64,
        // "Archive tabs after" in seconds. Restore reads anything but the
        // four choices as the 12-hour default.
        archive_interval_secs: u64,
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
        // Every Space's Pins in workspace order; `owner_id` is the Space.
        space_pins: Vec<ItemRecord>,
    }

    struct BridgeLimits {
        version: u32,
        spaces: usize,
        items: usize,
        windows: usize,
        archived_tabs: usize,
        name_bytes: usize,
        icon_bytes: usize,
        url_bytes: usize,
        title_bytes: usize,
    }

    extern "Rust" {
        type Workspace;
        fn new_workspace() -> Box<Workspace>;
        fn current_name(workspace: &Workspace) -> String;
        fn rename_space(workspace: &mut Workspace, id: u64, name: &str) -> RenameResult;
        fn bridge_limits() -> BridgeLimits;
        fn create_space(workspace: &mut Workspace, name: &str) -> MutationResult;
        // Create a Space panel: like create_space, with an icon unless
        // `icon_kind` is Unset, in which case `icon` must be empty.
        fn create_space_with_icon(
            workspace: &mut Workspace,
            name: &str,
            icon_kind: SpaceIconKind,
            icon: &str,
        ) -> MutationResult;
        // Unset restores the default glyph. `id` is 1 when the icon changed,
        // else 0.
        fn set_space_icon(
            workspace: &mut Workspace,
            space_id: u64,
            icon_kind: SpaceIconKind,
            icon: &str,
        ) -> MutationResult;
        fn create_window(workspace: &mut Workspace, space_id: u64) -> MutationResult;
        fn close_window(workspace: &mut Workspace, window_id: u64) -> MutationResult;
        fn select_space(workspace: &mut Workspace, window_id: u64, space_id: u64)
        -> MutationResult;
        fn add_temporary_item(workspace: &mut Workspace, space_id: u64) -> MutationResult;
        fn select_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> MutationResult;
        fn live_tab_closed(workspace: &mut Workspace, item_id: u64) -> MutationResult;
        // Like live_tab_closed, and archives a temporary item. `id` is 1 when
        // archived, else 0. Never call it for off-the-record Profiles or when
        // the Profile saves no history.
        fn archive_closed_tab(
            workspace: &mut Workspace,
            item_id: u64,
            url: &str,
            title: &str,
            now_secs: u64,
        ) -> MutationResult;
        // Unexpired entries, newest first.
        fn archived_tabs(workspace: &Workspace, now_secs: u64) -> Vec<ArchivedTabRecord>;
        fn remove_archived_tab(workspace: &mut Workspace, id: u64) -> MutationResult;
        // One Auto Archive pass, `elapsed_secs` of monotonic time after the
        // previous one (zero for the first of a launch): advances the
        // browser-open clock, records views of `viewed` (visible tabs),
        // selected and new temporary items, then names the idle ones among
        // `closable` (items whose pages can close and archive) that are due.
        // `media` lists items playing or capturing media.
        fn auto_archive(
            workspace: &mut Workspace,
            viewed: &[u64],
            closable: &[u64],
            media: &[u64],
            elapsed_secs: u64,
        ) -> AutoArchiveResult;
        // "Archive tabs after" in seconds: 12 hours, 24 hours, 7 days or 30
        // days.
        fn archive_interval_secs(workspace: &Workspace) -> u64;
        // Rejects any other value.
        fn set_archive_interval(workspace: &mut Workspace, secs: u64) -> MutationResult;
        // Clear Archive: drops every entry; `id` is how many went.
        fn clear_archived_tabs(workspace: &mut Workspace) -> MutationResult;
        // Drops expired entries; `id` is how many went.
        fn prune_archived_tabs(workspace: &mut Workspace, now_secs: u64) -> MutationResult;
        // History deletion: drops entries whose URL is in `urls`, and with
        // `has_range` those archived in begin..=end seconds. `id` is how many
        // went.
        fn forget_archived_tabs(
            workspace: &mut Workspace,
            urls: Vec<String>,
            has_range: bool,
            begin_secs: u64,
            end_secs: u64,
        ) -> MutationResult;
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
        // Saved items only. `id` is 1 when the stored title changed, else 0.
        fn set_title(workspace: &mut Workspace, item_id: u64, title: &str) -> MutationResult;
        // Temporary items only: the page a dormant row reopens. `id` is 1 when
        // the stored URL or title changed, else 0.
        fn set_temporary_page(
            workspace: &mut Workspace,
            item_id: u64,
            url: &str,
            title: &str,
        ) -> MutationResult;
        // A dormant temporary row of `space_id` remembering `url` (ignoring
        // the fragment) and not in `live`; zero when none.
        fn dormant_temporary_item(
            workspace: &Workspace,
            space_id: u64,
            url: &str,
            live: &[u64],
        ) -> u64;
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
        // Zero before means move to the end.
        fn move_space_before(
            workspace: &mut Workspace,
            space_id: u64,
            before: u64,
        ) -> MutationResult;
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
        name: workspace.0.space_name_of(id).unwrap_or_default().to_owned(),
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
        archived_tabs: workspace::MAX_ARCHIVED_TABS,
        name_bytes: workspace::MAX_SPACE_NAME_BYTES,
        icon_bytes: workspace::MAX_SPACE_ICON_BYTES,
        url_bytes: workspace::MAX_URL_BYTES,
        title_bytes: workspace::MAX_TITLE_BYTES,
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

fn space_icon(
    kind: ffi::SpaceIconKind,
    icon: &str,
) -> Result<Option<workspace::SpaceIcon>, &'static str> {
    match kind {
        ffi::SpaceIconKind::Unset if icon.is_empty() => Ok(None),
        ffi::SpaceIconKind::Unset => Err("Icon supplied without an icon kind"),
        ffi::SpaceIconKind::Emoji => Ok(Some(workspace::SpaceIcon::Emoji(icon.to_owned()))),
        ffi::SpaceIconKind::Named => Ok(Some(workspace::SpaceIcon::Named(icon.to_owned()))),
        _ => Err("Unknown Space icon kind"),
    }
}

pub fn create_space_with_icon(
    workspace: &mut Workspace,
    name: &str,
    icon_kind: ffi::SpaceIconKind,
    icon: &str,
) -> ffi::MutationResult {
    match space_icon(icon_kind, icon) {
        Ok(icon) => outcome(workspace.0.create_space_with_icon(
            workspace::DEFAULT_PROFILE_ID,
            name,
            icon,
        )),
        Err(error) => reject(error),
    }
}

pub fn set_space_icon(
    workspace: &mut Workspace,
    space_id: u64,
    icon_kind: ffi::SpaceIconKind,
    icon: &str,
) -> ffi::MutationResult {
    match space_icon(icon_kind, icon) {
        Ok(icon) => outcome(workspace.0.set_space_icon(space_id, icon).map(u64::from)),
        Err(error) => reject(error),
    }
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

pub fn archive_closed_tab(
    workspace: &mut Workspace,
    item_id: u64,
    url: &str,
    title: &str,
    now_secs: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .archive_closed_tab(item_id, url, title, now_secs)
            .map(u64::from),
    )
}

fn archived_record(tab: workspace::ArchivedTab) -> ffi::ArchivedTabRecord {
    ffi::ArchivedTabRecord {
        id: tab.id,
        space_id: tab.space_id,
        url: tab.url,
        title: tab.title,
        archived_secs: tab.archived_secs,
    }
}

pub fn archived_tabs(workspace: &Workspace, now_secs: u64) -> Vec<ffi::ArchivedTabRecord> {
    workspace
        .0
        .archived_tabs(now_secs)
        .into_iter()
        .map(archived_record)
        .collect()
}

pub fn remove_archived_tab(workspace: &mut Workspace, id: u64) -> ffi::MutationResult {
    outcome(workspace.0.remove_archived_tab(id).map(|tab| tab.id))
}

pub fn auto_archive(
    workspace: &mut Workspace,
    viewed: &[u64],
    closable: &[u64],
    media: &[u64],
    elapsed_secs: u64,
) -> ffi::AutoArchiveResult {
    let pass = workspace
        .0
        .auto_archive(viewed, closable, media, elapsed_secs);
    ffi::AutoArchiveResult {
        due: pass.due,
        changed: pass.changed,
    }
}

pub fn archive_interval_secs(workspace: &Workspace) -> u64 {
    workspace.0.archive_interval().seconds()
}

pub fn set_archive_interval(workspace: &mut Workspace, secs: u64) -> ffi::MutationResult {
    match workspace::archive::ArchiveInterval::from_seconds(secs) {
        Some(interval) => {
            workspace.0.set_archive_interval(interval);
            ffi::MutationResult {
                ok: true,
                id: secs,
                error: String::new(),
            }
        }
        None => reject("Choose 12 hours, 24 hours, 7 days or 30 days"),
    }
}

pub fn clear_archived_tabs(workspace: &mut Workspace) -> ffi::MutationResult {
    count(workspace.0.clear_archived_tabs())
}

fn count(removed: usize) -> ffi::MutationResult {
    ffi::MutationResult {
        ok: true,
        id: u64::try_from(removed).unwrap_or(u64::MAX),
        error: String::new(),
    }
}

pub fn prune_archived_tabs(workspace: &mut Workspace, now_secs: u64) -> ffi::MutationResult {
    count(workspace.0.prune_archived_tabs(now_secs))
}

pub fn forget_archived_tabs(
    workspace: &mut Workspace,
    urls: Vec<String>,
    has_range: bool,
    begin_secs: u64,
    end_secs: u64,
) -> ffi::MutationResult {
    count(
        workspace
            .0
            .forget_archived_tabs(&urls, has_range.then_some(begin_secs..=end_secs)),
    )
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

pub fn set_title(workspace: &mut Workspace, item_id: u64, title: &str) -> ffi::MutationResult {
    outcome(workspace.0.set_title(item_id, title).map(u64::from))
}

pub fn set_temporary_page(
    workspace: &mut Workspace,
    item_id: u64,
    url: &str,
    title: &str,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .set_temporary_page(item_id, url, title)
            .map(u64::from),
    )
}

pub fn dormant_temporary_item(
    workspace: &Workspace,
    space_id: u64,
    url: &str,
    live: &[u64],
) -> u64 {
    workspace
        .0
        .dormant_temporary_item(space_id, url, live)
        .unwrap_or(0)
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

pub fn move_space_before(
    workspace: &mut Workspace,
    space_id: u64,
    before: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .move_space_before(space_id, (before != 0).then_some(before))
            .map(|()| 0),
    )
}

pub fn reconcile_live_items(workspace: &mut Workspace, live: &[u64]) -> ffi::MutationResult {
    outcome(workspace.0.reconcile_live_items(live).map(|()| 0))
}

fn space_record(space: workspace::Space) -> ffi::SpaceRecord {
    let (icon_kind, icon) = match space.icon {
        None => (ffi::SpaceIconKind::Unset, String::new()),
        Some(workspace::SpaceIcon::Emoji(text)) => (ffi::SpaceIconKind::Emoji, text),
        Some(workspace::SpaceIcon::Named(name)) => (ffi::SpaceIconKind::Named, name),
    };
    ffi::SpaceRecord {
        id: space.id,
        profile_id: space.profile_id,
        name: space.name,
        icon_kind,
        icon,
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
        title: item.title,
        last_viewed_secs: item.last_viewed_secs,
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
            space_pins: sidebar.space_pins.into_iter().map(item_record).collect(),
        },
        Err(error) => ffi::SidebarResult {
            ok: false,
            error: error.to_string(),
            window_id,
            current_space: ffi::SpaceRecord {
                id: 0,
                profile_id: 0,
                name: String::new(),
                icon_kind: ffi::SpaceIconKind::Unset,
                icon: String::new(),
            },
            selected_item_id: 0,
            spaces: Vec::new(),
            favorites: Vec::new(),
            pins: Vec::new(),
            temporary: Vec::new(),
            space_pins: Vec::new(),
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
        archived: state.archived.into_iter().map(archived_record).collect(),
        open_secs: state.open_secs,
        archive_interval_secs: state.archive_interval.seconds(),
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
                || item.title.len() > workspace::MAX_TITLE_BYTES
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
            title: item.title,
            last_viewed_secs: item.last_viewed_secs,
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
                        // An oversized or malformed icon reads as none, like
                        // any other bad icon on restore.
                        icon: if space.icon.len() > workspace::MAX_SPACE_ICON_BYTES {
                            None
                        } else {
                            space_icon(space.icon_kind, &space.icon).unwrap_or_default()
                        },
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
                // An oversized entry is dropped like any other bad one; the
                // workspace keeps the newest entries within its limit.
                archived: snapshot
                    .archived
                    .into_iter()
                    .filter(|tab| {
                        tab.url.len() <= workspace::MAX_URL_BYTES
                            && tab.title.len() <= workspace::MAX_TITLE_BYTES
                    })
                    .map(|tab| workspace::ArchivedTab {
                        id: tab.id,
                        space_id: tab.space_id,
                        url: tab.url,
                        title: tab.title,
                        archived_secs: tab.archived_secs,
                    })
                    .collect(),
                open_secs: snapshot.open_secs,
                archive_interval: workspace::archive::ArchiveInterval::from_seconds(
                    snapshot.archive_interval_secs,
                )
                .unwrap_or_default(),
            })
            .map(|()| 0),
    )
}
