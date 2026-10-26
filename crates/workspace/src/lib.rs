#![forbid(unsafe_code)]

//! Dependency-free workspace decisions. Browser services own page lifetimes and URL
//! canonicalization; this crate owns organization and stable identities, never live tabs.
//! Successful mutations are in-memory only. Storage adapters must persist separately.
//!
//! Default state is Space 1 (Everyday), Profile 1 and Window 1. Additional windows
//! use model-generated IDs via `create_window`; adapters map them to native windows.
//! Multiple windows can select the same item. Closing the last live page should
//! trigger `live_tab_closed`; closing one of several projections must not remove
//! a temporary item that still has a live page. This crate stores no live count.
//! Temporary URLs/titles are browser-owned and absent from the typed snapshot;
//! restoring their organization does not restore browser sessions. After native-session
//! restoration and stable-ID binding finish, adapters must call `reconcile_live_items`
//! with all bound live item IDs to prune unbound temporary rows. Saved rows survive.
//!
//! Zero windows is valid during a Profile's lifetime. After restore or window close,
//! adapters must allocate/rebind a model window before requesting `sidebar_snapshot`;
//! Window 1 is only a default-state convenience, not a restore invariant.
//!
//! At untrusted decode and mutation boundaries, adapters must validate/canonicalize
//! HTTP(S) URLs using native browser URL machinery before passing strings here.
//! The core gate bounds size, schemes, credentials and whitespace/controls; it is
//! not a URL parser and cannot establish valid host syntax. `about:blank` is allowed.
//! Space names retain existing Unicode compatibility. Whether UI/security validation
//! should reject line separators or format marks remains an open question.

use std::collections::BTreeSet;
use std::fmt;

pub mod archive;

pub const MAX_SPACE_NAME_BYTES: usize = 128;
pub const MAX_URL_BYTES: usize = 8192;
pub const MAX_SPACES: usize = 64;
pub const MAX_ITEMS: usize = 1024;
pub const MAX_WINDOWS: usize = 64;
pub const SNAPSHOT_VERSION: u32 = 1;
pub const DEFAULT_SPACE_ID: u64 = 1;
pub const TEST_SPACE_ID: u64 = DEFAULT_SPACE_ID;
pub const DEFAULT_PROFILE_ID: u64 = 1;
pub const DEFAULT_WINDOW_ID: u64 = 1;

// IDs have separate namespaces. Item IDs are never native tab/session identifiers.
pub type SpaceId = u64;
pub type ProfileId = u64;
pub type ItemId = u64;
pub type WindowId = u64;
pub mod peek;
pub mod tab_ops;

#[derive(Debug, PartialEq, Eq)]
pub enum RenameError {
    UnknownSpace,
    EmptyName,
    NameTooLong,
    ControlCharacter,
}

impl fmt::Display for RenameError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(match self {
            Self::UnknownSpace => "Space does not exist",
            Self::EmptyName => "Enter a Space name",
            Self::NameTooLong => "Space name exceeds 128 UTF-8 bytes",
            Self::ControlCharacter => "Space name cannot contain control characters",
        })
    }
}

impl std::error::Error for RenameError {}

#[derive(Debug, PartialEq, Eq)]
pub enum WorkspaceError {
    UnknownSpace(SpaceId),
    UnknownProfile(ProfileId),
    UnknownItem(ItemId),
    UnknownWindow(WindowId),
    InvalidName(RenameError),
    InvalidUrl,
    CanonicalUrlRequired,
    UnexpectedCanonicalUrl,
    NotSaved(ItemId),
    ExplicitUnsaveRequired(ItemId),
    DifferentSection(ItemId),
    DuplicateLiveItem(ItemId),
    ItemNotVisible(ItemId),
    LimitExceeded(&'static str),
    IdExhausted(&'static str),
    UnsupportedSnapshotVersion(u32),
    InvalidSnapshot(&'static str),
}

impl fmt::Display for WorkspaceError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnknownSpace(id) => write!(formatter, "Space {id} does not exist"),
            Self::UnknownProfile(id) => write!(formatter, "Profile {id} is unsupported; use Profile 1"),
            Self::UnknownItem(id) => write!(formatter, "Item {id} does not exist"),
            Self::UnknownWindow(id) => write!(formatter, "Window {id} does not exist; create it first"),
            Self::InvalidName(error) => error.fmt(formatter),
            Self::InvalidUrl => formatter.write_str(
                "Use either a browser-validated canonical HTTP(S) URL without credentials, or about:blank; at most 8192 bytes and without whitespace/control characters",
            ),
            Self::CanonicalUrlRequired => formatter.write_str("Supply a canonical URL when saving a temporary item"),
            Self::UnexpectedCanonicalUrl => formatter.write_str("Temporary items cannot store a canonical URL"),
            Self::NotSaved(id) => write!(formatter, "Item {id} is temporary; close its live tab instead"),
            Self::ExplicitUnsaveRequired(id) => write!(formatter, "Item {id} is saved; use unsave_item with its live-page destination"),
            Self::DifferentSection(id) => write!(formatter, "Item {id} belongs to a different placement or owner section"),
            Self::DuplicateLiveItem(id) => write!(formatter, "Live item {id} is listed more than once"),
            Self::ItemNotVisible(id) => write!(formatter, "Item {id} does not belong to this window's selected Space or Profile"),
            Self::LimitExceeded(kind) => write!(formatter, "Workspace {kind} limit reached"),
            Self::IdExhausted(kind) => write!(formatter, "Workspace {kind} IDs exhausted"),
            Self::UnsupportedSnapshotVersion(version) => write!(formatter, "Workspace snapshot version {version} is unsupported; expected {SNAPSHOT_VERSION}"),
            Self::InvalidSnapshot(reason) => write!(formatter, "Invalid workspace snapshot: {reason}"),
        }
    }
}

impl std::error::Error for WorkspaceError {}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Space {
    pub id: SpaceId,
    pub profile_id: ProfileId,
    pub name: String,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Placement {
    Temporary { space_id: SpaceId },
    Pin { space_id: SpaceId },
    Favorite { profile_id: ProfileId },
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Item {
    pub id: ItemId,
    pub placement: Placement,
    /// Saved URL only. Navigating a live page must not update this value implicitly.
    pub canonical_url: Option<String>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WindowState {
    pub id: WindowId,
    pub space_id: SpaceId,
    /// Last explicitly selected visible item, including a dormant saved item.
    /// Switching Space clears it if hidden. No per-Space selection history yet.
    pub selected_item_id: Option<ItemId>,
}

/// Typed storage boundary, not a disk format or durable acknowledgement.
///
/// Vectors define order. Placement changes preserve existing vector order unless
/// `move_item_before` explicitly reorders a section; equal URLs do not merge
/// identities. Counters persist even after removal so IDs are not reused.
/// Adapters must bound decoding before allocating these owned vectors/strings.
/// `from_snapshot` checks collection counts before allocating validation indexes,
/// consumes the supplied buffers and performs no unbounded copies.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WorkspaceSnapshot {
    pub version: u32,
    /// shortcut: only Profile 1 is supported, extend when native Profile binding exists.
    pub profile_id: ProfileId,
    pub spaces: Vec<Space>,
    pub items: Vec<Item>,
    pub windows: Vec<WindowState>,
    pub next_space_id: SpaceId,
    pub next_item_id: ItemId,
    pub next_window_id: WindowId,
}

/// Owned, bounded projection. Native callers render these sections as supplied.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SidebarSnapshot {
    pub window_id: WindowId,
    pub current_space: Space,
    pub spaces: Vec<Space>,
    pub selected_item_id: Option<ItemId>,
    pub favorites: Vec<Item>,
    pub pins: Vec<Item>,
    pub temporary: Vec<Item>,
}

#[derive(Debug, PartialEq, Eq)]
pub struct Workspace {
    state: WorkspaceSnapshot,
}

impl Default for Workspace {
    fn default() -> Self {
        Self {
            state: WorkspaceSnapshot {
                version: SNAPSHOT_VERSION,
                profile_id: DEFAULT_PROFILE_ID,
                spaces: vec![Space {
                    id: DEFAULT_SPACE_ID,
                    profile_id: DEFAULT_PROFILE_ID,
                    name: String::from("Everyday"),
                }],
                items: Vec::new(),
                windows: vec![WindowState {
                    id: DEFAULT_WINDOW_ID,
                    space_id: DEFAULT_SPACE_ID,
                    selected_item_id: None,
                }],
                next_space_id: 2,
                next_item_id: 1,
                next_window_id: 2,
            },
        }
    }
}

impl Workspace {
    /// Legacy bridge view of Space 1, independent of any window selection.
    pub fn space_name(&self) -> &str {
        // Space 1 is required by restore and cannot be removed by commands.
        self.state
            .spaces
            .iter()
            .find(|space| space.id == DEFAULT_SPACE_ID)
            .map(|space| space.name.as_str())
            .unwrap_or_default()
    }

    pub fn rename_space(&mut self, id: u64, name: &str) -> Result<(), RenameError> {
        let space = self
            .state
            .spaces
            .iter_mut()
            .find(|space| space.id == id)
            .ok_or(RenameError::UnknownSpace)?;
        validate_name(name)?;
        space.name = name.to_owned();
        Ok(())
    }

    pub fn create_space(
        &mut self,
        profile_id: ProfileId,
        name: &str,
    ) -> Result<SpaceId, WorkspaceError> {
        validate_profile(profile_id)?;
        validate_name(name).map_err(WorkspaceError::InvalidName)?;
        check_limit(self.state.spaces.len(), MAX_SPACES, "Spaces")?;
        let id = self.state.next_space_id;
        let next = next_id(id, "Space")?;
        self.state.spaces.push(Space {
            id,
            profile_id,
            name: name.to_owned(),
        });
        self.state.next_space_id = next;
        Ok(id)
    }

    pub fn create_window(&mut self, space_id: SpaceId) -> Result<WindowId, WorkspaceError> {
        self.space(space_id)?;
        check_limit(self.state.windows.len(), MAX_WINDOWS, "windows")?;
        let id = self.state.next_window_id;
        let next = next_id(id, "window")?;
        self.state.windows.push(WindowState {
            id,
            space_id,
            selected_item_id: None,
        });
        self.state.next_window_id = next;
        Ok(id)
    }

    pub fn close_window(&mut self, window_id: WindowId) -> Result<(), WorkspaceError> {
        self.window(window_id)?;
        self.state.windows.retain(|window| window.id != window_id);
        Ok(())
    }

    pub fn select_space(
        &mut self,
        window_id: WindowId,
        space_id: SpaceId,
    ) -> Result<(), WorkspaceError> {
        self.space(space_id)?;
        self.window(window_id)?;
        for window in &mut self.state.windows {
            if window.id == window_id {
                window.space_id = space_id;
            }
        }
        self.clear_hidden_selections();
        Ok(())
    }

    /// Select a saved item to reopen it using its existing ID and canonical URL.
    /// Browser adapters own actual page creation/activation and live ID mappings.
    pub fn select_item(
        &mut self,
        window_id: WindowId,
        item_id: ItemId,
    ) -> Result<(), WorkspaceError> {
        let window = self.window(window_id)?;
        let item = self.item(item_id)?;
        if !visible(item.placement, window.space_id) {
            return Err(WorkspaceError::ItemNotVisible(item_id));
        }
        for window in &mut self.state.windows {
            if window.id == window_id {
                window.selected_item_id = Some(item_id);
            }
        }
        Ok(())
    }

    /// Native keyboard/palette activation reveals the owning Space through the
    /// same command path as sidebar activation. Favorites keep the current Space.
    pub fn reveal_item(
        &mut self,
        window_id: WindowId,
        item_id: ItemId,
    ) -> Result<(), WorkspaceError> {
        self.window(window_id)?;
        let space_id = match self.item(item_id)?.placement {
            Placement::Temporary { space_id } | Placement::Pin { space_id } => space_id,
            Placement::Favorite { .. } => self.window(window_id)?.space_id,
        };
        self.select_space(window_id, space_id)?;
        self.select_item(window_id, item_id)
    }

    pub fn add_temporary_item(&mut self, space_id: SpaceId) -> Result<ItemId, WorkspaceError> {
        self.space(space_id)?;
        check_limit(self.state.items.len(), MAX_ITEMS, "items")?;
        let id = self.state.next_item_id;
        let next = next_id(id, "item")?;
        self.state.items.push(Item {
            id,
            placement: Placement::Temporary { space_id },
            canonical_url: None,
        });
        self.state.next_item_id = next;
        Ok(id)
    }

    /// `Some(url)` deliberately replaces the saved URL. `None` preserves a saved
    /// URL when saving/moving. A first save requires `Some(url)`.
    /// Saved-to-Temporary demotion is rejected: use `unsave_item` to report whether
    /// and where a live page exists. Temporary-to-Temporary moves remain allowed.
    /// Placement changes keep vector order. This method never observes navigation.
    pub fn set_placement(
        &mut self,
        item_id: ItemId,
        placement: Placement,
        canonical_url: Option<&str>,
    ) -> Result<(), WorkspaceError> {
        let item = self.item(item_id)?;
        self.validate_placement(placement)?;
        let url = match placement {
            Placement::Temporary { .. } => {
                if canonical_url.is_some() {
                    return Err(WorkspaceError::UnexpectedCanonicalUrl);
                }
                if !matches!(item.placement, Placement::Temporary { .. }) {
                    return Err(WorkspaceError::ExplicitUnsaveRequired(item_id));
                }
                None
            }
            Placement::Pin { .. } | Placement::Favorite { .. } => {
                let url = canonical_url
                    .or(item.canonical_url.as_deref())
                    .ok_or(WorkspaceError::CanonicalUrlRequired)?;
                validate_url(url)?;
                Some(url.to_owned())
            }
        };
        for item in &mut self.state.items {
            if item.id == item_id {
                item.placement = placement;
                item.canonical_url = url;
                break;
            }
        }
        self.clear_hidden_selections();
        Ok(())
    }

    /// Temporary identity ends on close. Saved identity and selection survive;
    /// live/dormant state is browser-owned and intentionally absent here.
    pub fn live_tab_closed(&mut self, item_id: ItemId) -> Result<(), WorkspaceError> {
        if matches!(self.item(item_id)?.placement, Placement::Temporary { .. }) {
            self.remove_item(item_id);
        }
        Ok(())
    }

    pub fn remove_saved_item(&mut self, item_id: ItemId) -> Result<(), WorkspaceError> {
        if matches!(self.item(item_id)?.placement, Placement::Temporary { .. }) {
            return Err(WorkspaceError::NotSaved(item_id));
        }
        self.remove_item(item_id);
        Ok(())
    }

    /// Adapter reports a live page's destination Space, or `None` for a dormant
    /// saved item. Rust decides the transition: keep identity as Temporary and
    /// clear its saved URL when live; remove identity when dormant. Vector order
    /// is preserved, and selections hidden by the transition are cleared.
    pub fn unsave_item(
        &mut self,
        item_id: ItemId,
        live_destination: Option<SpaceId>,
    ) -> Result<(), WorkspaceError> {
        if matches!(self.item(item_id)?.placement, Placement::Temporary { .. }) {
            return Err(WorkspaceError::NotSaved(item_id));
        }
        if let Some(space_id) = live_destination {
            self.space(space_id)?;
            for item in &mut self.state.items {
                if item.id == item_id {
                    item.placement = Placement::Temporary { space_id };
                    item.canonical_url = None;
                    break;
                }
            }
            self.clear_hidden_selections();
        } else {
            self.remove_item(item_id);
        }
        Ok(())
    }

    /// Call only after native-session restoration and stable-ID binding complete.
    /// Supply all bound live item IDs (saved IDs are allowed). Counts, existence
    /// and uniqueness are checked before mutation. Prunes only unbound Temporary
    /// items and their selections across all windows; dormant saved items remain.
    pub fn reconcile_live_items(&mut self, live_item_ids: &[ItemId]) -> Result<(), WorkspaceError> {
        if live_item_ids.len() > MAX_ITEMS {
            return Err(WorkspaceError::LimitExceeded("live items"));
        }
        let mut live = BTreeSet::new();
        for &id in live_item_ids {
            self.item(id)?;
            if !live.insert(id) {
                return Err(WorkspaceError::DuplicateLiveItem(id));
            }
        }
        self.state.items.retain(|item| {
            !matches!(item.placement, Placement::Temporary { .. }) || live.contains(&item.id)
        });
        self.clear_hidden_selections();
        Ok(())
    }

    /// Reorder within exactly the same placement and owner section. `None` appends
    /// within that section; moving before oneself is a no-op. Other sections keep
    /// their relative order. IDs, saved URLs and selections are unchanged.
    pub fn move_item_before(
        &mut self,
        item_id: ItemId,
        before: Option<ItemId>,
    ) -> Result<(), WorkspaceError> {
        let placement = self.item(item_id)?.placement;
        if let Some(id) = before {
            if self.item(id)?.placement != placement {
                return Err(WorkspaceError::DifferentSection(id));
            }
        }
        let source = self
            .state
            .items
            .iter()
            .position(|item| item.id == item_id)
            .ok_or(WorkspaceError::UnknownItem(item_id))?;
        let target = self
            .state
            .items
            .iter()
            .rposition(|item| match before {
                Some(id) => item.id == id,
                None => item.placement == placement,
            })
            .ok_or(WorkspaceError::UnknownItem(before.unwrap_or(item_id)))?;
        if source == target {
            return Ok(());
        }
        // Exclude the before-target when moving forwards, include the last
        // section item when appending. Both ranges contain the source item.
        let start = source.min(target);
        let end = if source < target && before.is_some() {
            target
        } else {
            source.max(target) + 1
        };
        let items = self
            .state
            .items
            .get_mut(start..end)
            .ok_or(WorkspaceError::UnknownItem(item_id))?;
        if source < target {
            items.rotate_left(1);
        } else {
            items.rotate_right(1);
        }
        Ok(())
    }

    /// Requires an allocated/rebound model window; restore may contain none.
    pub fn sidebar_snapshot(&self, window_id: WindowId) -> Result<SidebarSnapshot, WorkspaceError> {
        let window = self.window(window_id)?;
        let mut sidebar = SidebarSnapshot {
            window_id,
            current_space: self.space(window.space_id)?.clone(),
            spaces: self.state.spaces.clone(),
            selected_item_id: window.selected_item_id,
            favorites: Vec::new(),
            pins: Vec::new(),
            temporary: Vec::new(),
        };
        for item in &self.state.items {
            if !visible(item.placement, window.space_id) {
                continue;
            }
            match item.placement {
                Placement::Favorite { .. } => sidebar.favorites.push(item.clone()),
                Placement::Pin { .. } => sidebar.pins.push(item.clone()),
                Placement::Temporary { .. } => sidebar.temporary.push(item.clone()),
            }
        }
        Ok(sidebar)
    }

    pub fn snapshot(&self) -> WorkspaceSnapshot {
        self.state.clone()
    }

    pub fn from_snapshot(snapshot: WorkspaceSnapshot) -> Result<Self, WorkspaceError> {
        let workspace = Self { state: snapshot };
        workspace.validate()?;
        Ok(workspace)
    }

    /// Replacement is atomic in memory on validation success, with no disk I/O.
    pub fn restore(&mut self, snapshot: WorkspaceSnapshot) -> Result<(), WorkspaceError> {
        *self = Self::from_snapshot(snapshot)?;
        Ok(())
    }

    fn space(&self, id: SpaceId) -> Result<&Space, WorkspaceError> {
        self.state
            .spaces
            .iter()
            .find(|space| space.id == id)
            .ok_or(WorkspaceError::UnknownSpace(id))
    }

    fn window(&self, id: WindowId) -> Result<&WindowState, WorkspaceError> {
        self.state
            .windows
            .iter()
            .find(|window| window.id == id)
            .ok_or(WorkspaceError::UnknownWindow(id))
    }

    fn item(&self, id: ItemId) -> Result<&Item, WorkspaceError> {
        self.state
            .items
            .iter()
            .find(|item| item.id == id)
            .ok_or(WorkspaceError::UnknownItem(id))
    }

    fn validate_placement(&self, placement: Placement) -> Result<(), WorkspaceError> {
        match placement {
            Placement::Temporary { space_id } | Placement::Pin { space_id } => {
                self.space(space_id)?;
            }
            Placement::Favorite { profile_id } => validate_profile(profile_id)?,
        }
        Ok(())
    }

    fn remove_item(&mut self, item_id: ItemId) {
        self.state.items.retain(|item| item.id != item_id);
        self.clear_hidden_selections();
    }

    fn clear_hidden_selections(&mut self) {
        for window in &mut self.state.windows {
            if let Some(id) = window.selected_item_id {
                let retained = self
                    .state
                    .items
                    .iter()
                    .any(|item| item.id == id && visible(item.placement, window.space_id));
                if !retained {
                    window.selected_item_id = None;
                }
            }
        }
    }

    fn validate(&self) -> Result<(), WorkspaceError> {
        let state = &self.state;
        if state.version != SNAPSHOT_VERSION {
            return Err(WorkspaceError::UnsupportedSnapshotVersion(state.version));
        }
        validate_profile(state.profile_id)?;
        if state.spaces.len() > MAX_SPACES
            || state.items.len() > MAX_ITEMS
            || state.windows.len() > MAX_WINDOWS
        {
            return Err(WorkspaceError::InvalidSnapshot(
                "collection count exceeds workspace limits",
            ));
        }
        validate_ids(
            state.spaces.iter().map(|space| space.id),
            state.next_space_id,
        )?;
        validate_ids(state.items.iter().map(|item| item.id), state.next_item_id)?;
        validate_ids(
            state.windows.iter().map(|window| window.id),
            state.next_window_id,
        )?;
        self.space(DEFAULT_SPACE_ID)?;
        for space in &state.spaces {
            validate_profile(space.profile_id)?;
            validate_name(&space.name).map_err(WorkspaceError::InvalidName)?;
        }
        for item in &state.items {
            self.validate_placement(item.placement)?;
            match (item.placement, item.canonical_url.as_deref()) {
                (Placement::Temporary { .. }, Some(_)) => {
                    return Err(WorkspaceError::UnexpectedCanonicalUrl);
                }
                (Placement::Temporary { .. }, None) => {}
                (_, Some(url)) => validate_url(url)?,
                (_, None) => return Err(WorkspaceError::CanonicalUrlRequired),
            }
        }
        for window in &state.windows {
            self.space(window.space_id)?;
            if let Some(id) = window.selected_item_id {
                if !visible(self.item(id)?.placement, window.space_id) {
                    return Err(WorkspaceError::ItemNotVisible(id));
                }
            }
        }
        Ok(())
    }
}

fn validate_name(name: &str) -> Result<(), RenameError> {
    if name.len() > MAX_SPACE_NAME_BYTES {
        return Err(RenameError::NameTooLong);
    }
    if name.trim().is_empty() {
        return Err(RenameError::EmptyName);
    }
    if name.chars().any(char::is_control) {
        return Err(RenameError::ControlCharacter);
    }
    Ok(())
}

fn validate_profile(id: ProfileId) -> Result<(), WorkspaceError> {
    if id != DEFAULT_PROFILE_ID {
        return Err(WorkspaceError::UnknownProfile(id));
    }
    Ok(())
}

fn visible(placement: Placement, space_id: SpaceId) -> bool {
    match placement {
        Placement::Temporary { space_id: owner } | Placement::Pin { space_id: owner } => {
            owner == space_id
        }
        Placement::Favorite { profile_id } => profile_id == DEFAULT_PROFILE_ID,
    }
}

fn check_limit(length: usize, maximum: usize, kind: &'static str) -> Result<(), WorkspaceError> {
    if length >= maximum {
        return Err(WorkspaceError::LimitExceeded(kind));
    }
    Ok(())
}

fn next_id(id: u64, kind: &'static str) -> Result<u64, WorkspaceError> {
    id.checked_add(1).ok_or(WorkspaceError::IdExhausted(kind))
}

fn validate_ids(ids: impl Iterator<Item = u64>, next: u64) -> Result<(), WorkspaceError> {
    if next == 0 {
        return Err(WorkspaceError::InvalidSnapshot("next ID must be nonzero"));
    }
    let mut seen = BTreeSet::new();
    for id in ids {
        if id == 0 || id >= next || !seen.insert(id) {
            return Err(WorkspaceError::InvalidSnapshot(
                "IDs must be nonzero, unique within their kind and less than their next counter",
            ));
        }
    }
    Ok(())
}

fn validate_url(url: &str) -> Result<(), WorkspaceError> {
    if url.len() > MAX_URL_BYTES || url.chars().any(|ch| ch.is_control() || ch.is_whitespace()) {
        return Err(WorkspaceError::InvalidUrl);
    }
    if url == "about:blank" {
        return Ok(());
    }
    // The browser must parse/canonicalize full syntax. This is only a scheme,
    // credential and size gate before a URL can enter saved workspace state.
    let authority = url
        .strip_prefix("https://")
        .or_else(|| url.strip_prefix("http://"))
        .and_then(|rest| rest.split(['/', '?', '#']).next());
    match authority {
        Some(host) if !host.is_empty() && !host.contains('@') && !url.contains('\\') => Ok(()),
        _ => Err(WorkspaceError::InvalidUrl),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rename_updates_shared_state() {
        let mut workspace = Workspace::default();
        assert_eq!(workspace.rename_space(TEST_SPACE_ID, "Research"), Ok(()));
        assert_eq!(workspace.space_name(), "Research");
        assert_eq!(workspace.rename_space(TEST_SPACE_ID, "Everyday"), Ok(()));
        assert_eq!(workspace.space_name(), "Everyday");
    }

    #[test]
    fn rejects_unknown_space_without_mutation() {
        let mut workspace = Workspace::default();
        assert_eq!(
            workspace.rename_space(2, "Research"),
            Err(RenameError::UnknownSpace)
        );
        assert_eq!(workspace.space_name(), "Everyday");
    }

    #[test]
    fn rejects_blank_names_without_mutation() {
        let mut workspace = Workspace::default();
        for name in ["", " ", "\u{2003}"] {
            assert_eq!(
                workspace.rename_space(TEST_SPACE_ID, name),
                Err(RenameError::EmptyName)
            );
            assert_eq!(workspace.space_name(), "Everyday");
        }
    }

    #[test]
    fn rejects_control_characters_without_mutation() {
        let mut workspace = Workspace::default();
        for name in ["Work\nNotes", "Work\0Notes", "Work\u{0085}Notes"] {
            assert_eq!(
                workspace.rename_space(TEST_SPACE_ID, name),
                Err(RenameError::ControlCharacter)
            );
            assert_eq!(workspace.space_name(), "Everyday");
        }
    }

    #[test]
    fn bounds_utf8_bytes_and_preserves_unicode() {
        let mut workspace = Workspace::default();
        let maximum = "é".repeat(MAX_SPACE_NAME_BYTES / 2);
        assert_eq!(workspace.rename_space(TEST_SPACE_ID, &maximum), Ok(()));
        assert_eq!(workspace.space_name(), maximum);
        assert_eq!(
            workspace.rename_space(TEST_SPACE_ID, &(maximum.clone() + "x")),
            Err(RenameError::NameTooLong)
        );
        assert_eq!(workspace.space_name(), maximum);
        assert_eq!(workspace.rename_space(TEST_SPACE_ID, "Café ☕"), Ok(()));
        assert_eq!(workspace.space_name(), "Café ☕");
    }

    #[test]
    fn repeat_is_idempotent_and_instances_are_independent() {
        let mut first = Workspace::default();
        let second = Workspace::default();
        assert_eq!(first.rename_space(TEST_SPACE_ID, "Research"), Ok(()));
        assert_eq!(first.rename_space(TEST_SPACE_ID, "Research"), Ok(()));
        assert_eq!(first.space_name(), "Research");
        assert_eq!(second.space_name(), "Everyday");
    }
}
