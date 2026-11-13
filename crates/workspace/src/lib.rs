#![forbid(unsafe_code)]

//! Dependency-free workspace decisions. Browser services own page lifetimes and URL
//! canonicalization; this crate owns organization and stable identities, never live tabs.
//! Successful mutations are in-memory only. Storage adapters must persist separately.
//!
//! Default state is Space 1 (Everyday), Profile 1 and Window 1. Space 1 is only the
//! starting Space of a new Profile and can be deleted like any other; restore needs
//! at least one Space, not Space 1. Additional windows
//! use model-generated IDs via `create_window`; adapters map them to native windows.
//! Multiple windows can select the same item. Closing the last live page should
//! trigger `live_tab_closed`; closing one of several projections must not remove
//! a temporary item that still has a live page. This crate stores no live count.
//! A temporary item may remember the last page it showed (`set_temporary_page`) so
//! its row survives, dormant, when the browser does not restore its tab. After
//! native-session restoration and stable-ID binding finish, adapters must call
//! `reconcile_live_items` with all bound live item IDs to prune unbound temporary rows
//! that remember no page. Saved rows and temporary rows with a page survive.
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
pub mod drag;
pub mod folder;
pub mod split;

pub use drag::DropTarget;
pub use folder::{Folder, FolderDeletion, FolderId, MAX_FOLDER_NAME_BYTES, MAX_FOLDERS};
pub use split::{MAX_SPLITS, Split};

pub const MAX_SPACE_NAME_BYTES: usize = 128;
/// Bound for either kind of Space icon. Long emoji ZWJ sequences with skin
/// tones stay under 40 bytes.
pub const MAX_SPACE_ICON_BYTES: usize = 64;
/// A Space theme's opacity runs from 0 to this, in percent.
pub const MAX_SPACE_THEME_OPACITY: u8 = 100;
pub const MAX_URL_BYTES: usize = 8192;
pub const MAX_TITLE_BYTES: usize = 1024;
pub const MAX_SPACES: usize = 64;
pub const MAX_ITEMS: usize = 1024;
pub const MAX_WINDOWS: usize = 64;
/// Archived Tabs keep at most this many entries; the oldest go first.
pub const MAX_ARCHIVED_TABS: usize = 500;
/// Archived Tabs older than 30 days are dropped. Retention counts from the
/// close, so it is independent of the Auto Archive interval.
pub const ARCHIVE_RETENTION_SECS: u64 = 30 * 24 * 60 * 60;
pub const SNAPSHOT_VERSION: u32 = 1;
pub const DEFAULT_SPACE_ID: u64 = 1;
pub const TEST_SPACE_ID: u64 = DEFAULT_SPACE_ID;
pub const DEFAULT_PROFILE_ID: u64 = 1;
pub const DEFAULT_WINDOW_ID: u64 = 1;
/// Name `ensure_agent_space` gives a new agent Space. Users can rename it.
pub const AGENT_SPACE_NAME: &str = "Agent";
/// Bundled icon `ensure_agent_space` gives a new agent Space. Users can
/// change it.
pub const AGENT_SPACE_ICON: &str = "bot";

// IDs have separate namespaces. Item IDs are never native tab/session identifiers.
pub type SpaceId = u64;
pub type ProfileId = u64;
pub type ItemId = u64;
pub type WindowId = u64;
pub mod peek;
pub mod site_icon;
pub mod tab_ops;
pub mod url_label;

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
    UnknownArchivedTab(ItemId),
    UnknownFolder(FolderId),
    InvalidName(RenameError),
    InvalidFolderName(RenameError),
    InvalidIcon,
    InvalidTheme,
    InvalidUrl,
    CanonicalUrlRequired,
    UnexpectedCanonicalUrl,
    NotSaved(ItemId),
    /// Temporary items have no canonical URL to replace or compare.
    NoCanonicalUrl(ItemId),
    /// A Profile must keep at least one Space.
    LastSpaceInProfile(SpaceId),
    ExplicitUnsaveRequired(ItemId),
    DifferentSection(ItemId),
    /// The folder belongs to another Space than the Pin or folder it was paired with.
    DifferentFolderSpace(FolderId),
    /// Only Pins go in folders.
    NotPinned(ItemId),
    /// Only the window's Pins and Today tabs can be dragged.
    NotDraggable(ItemId),
    DuplicateLiveItem(ItemId),
    ItemNotVisible(ItemId),
    /// Split View holds Today tabs only.
    SplitNeedsTemporary(ItemId),
    /// A split's tabs share one Space, and so one Profile.
    SplitAcrossSpaces(ItemId),
    AlreadySplit(ItemId),
    SplitWithItself(ItemId),
    /// A split row drags within Today only; pinning a split is a later step.
    SplitNotPinnable(ItemId),
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
            Self::UnknownArchivedTab(id) => write!(formatter, "Archived tab {id} does not exist"),
            Self::UnknownFolder(id) => write!(formatter, "Folder {id} does not exist"),
            Self::InvalidName(error) => error.fmt(formatter),
            Self::InvalidFolderName(error) => formatter.write_str(error.folder_message()),
            Self::InvalidIcon => formatter.write_str(
                "Use an emoji or an icon name of lowercase letters, digits and hyphens, at most 64 bytes",
            ),
            Self::InvalidTheme => formatter.write_str(
                "Use a theme colour of 0xRRGGBB and an opacity from 0 to 100",
            ),
            Self::InvalidUrl => formatter.write_str(
                "Use either a browser-validated canonical HTTP(S) URL without credentials, or about:blank; at most 8192 bytes and without whitespace/control characters",
            ),
            Self::CanonicalUrlRequired => formatter.write_str("Supply a canonical URL when saving a temporary item"),
            Self::UnexpectedCanonicalUrl => formatter.write_str("Temporary items cannot store a canonical URL"),
            Self::NotSaved(id) => write!(formatter, "Item {id} is temporary; close its live tab instead"),
            Self::NoCanonicalUrl(id) => write!(formatter, "Item {id} is temporary and has no pinned URL; pin or favorite it first"),
            Self::LastSpaceInProfile(id) => write!(formatter, "Space {id} is the last Space in its Profile; create another Space before deleting it"),
            Self::ExplicitUnsaveRequired(id) => write!(formatter, "Item {id} is saved; use unsave_item with its live-page destination"),
            Self::DifferentSection(id) => write!(formatter, "Item {id} belongs to a different placement or owner section"),
            Self::DifferentFolderSpace(id) => write!(formatter, "Folder {id} belongs to a different Space"),
            Self::NotPinned(id) => write!(formatter, "Item {id} is not a Space Pin; pin it before putting it in a folder"),
            Self::NotDraggable(id) => write!(formatter, "Item {id} is not a Pin or Today tab of this window's Space"),
            Self::DuplicateLiveItem(id) => write!(formatter, "Live item {id} is listed more than once"),
            Self::ItemNotVisible(id) => write!(formatter, "Item {id} does not belong to this window's selected Space or Profile"),
            Self::SplitNeedsTemporary(id) => write!(formatter, "Item {id} is pinned or a Favorite; Split View holds Today tabs"),
            Self::SplitAcrossSpaces(id) => write!(formatter, "Item {id} belongs to another Space or Profile; a split holds tabs of one Space"),
            Self::AlreadySplit(id) => write!(formatter, "Item {id} is already in a split; separate it first"),
            Self::SplitWithItself(id) => write!(formatter, "Item {id} can't be split with itself; pick another tab"),
            Self::SplitNotPinnable(id) => write!(formatter, "Item {id} is in a split, which can't be pinned yet; separate it first"),
            Self::LimitExceeded(kind) => write!(formatter, "Workspace {kind} limit reached"),
            Self::IdExhausted(kind) => write!(formatter, "Workspace {kind} IDs exhausted"),
            Self::UnsupportedSnapshotVersion(version) => write!(formatter, "Workspace snapshot version {version} is unsupported; expected {SNAPSHOT_VERSION}"),
            Self::InvalidSnapshot(reason) => write!(formatter, "Invalid workspace snapshot: {reason}"),
        }
    }
}

impl std::error::Error for WorkspaceError {}

/// Icon the Space shows in place of its default glyph, picked in the Emoji/Icon
/// picker. Both kinds are bounded by `MAX_SPACE_ICON_BYTES`.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum SpaceIcon {
    /// One emoji as the picker inserts it, including ZWJ sequences, skin tones
    /// and keycaps. The core gate bounds size and rejects controls, whitespace
    /// and plain ASCII; it has no emoji table, so the picker owns the set.
    Emoji(String),
    /// Name of a bundled icon, such as `star` or `book-open`: lowercase ASCII
    /// letters, digits and single hyphens, starting with a letter. Native code
    /// maps names it does not know to the default glyph.
    Named(String),
}

/// Colour the sidebar takes in the Space, picked in Choose a Theme.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SpaceTheme {
    /// 0xRRGGBB.
    pub color: u32,
    /// How strongly the colour tints the sidebar, 0 to
    /// `MAX_SPACE_THEME_OPACITY` percent.
    pub opacity: u8,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Space {
    pub id: SpaceId,
    pub profile_id: ProfileId,
    pub name: String,
    /// `None` shows the default glyph. Snapshots from before icons have none.
    pub icon: Option<SpaceIcon>,
    /// `None` keeps the default sidebar tint. Snapshots from before themes
    /// have none.
    pub theme: Option<SpaceTheme>,
    /// Marks the Profile's agent Space, where external agents open their
    /// tabs. At most one Space per Profile carries it. Renaming the Space or
    /// changing its icon keeps it; deleting the Space drops it.
    pub agent: bool,
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
    /// Saved URL of a saved item. Navigating a live page must not update it
    /// implicitly. A temporary item holds the last page it showed here, written
    /// only by `set_temporary_page`, so its dormant row can reopen it.
    pub canonical_url: Option<String>,
    /// Last known page title, shown while the item is dormant. Empty when unknown.
    pub title: String,
    /// The user's name for a saved item, from Rename. It shows instead of the
    /// page title. Empty when the item has none. Temporary items never have one.
    pub name: String,
    /// Reading of the browser-open clock (`WorkspaceSnapshot::open_secs`) when a
    /// temporary item was last seen, for Auto Archive. Zero means not recorded
    /// yet; saving or unsaving resets it.
    pub last_viewed_secs: u64,
}

/// A closed temporary page, kept so the Library can reopen it (Archived Tabs).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ArchivedTab {
    /// ID of the closed item. Item IDs are never reused, so this stays unique.
    pub id: ItemId,
    /// Space the page belonged to. That Space may since have been deleted.
    pub space_id: SpaceId,
    /// Browser-validated URL the page showed when it closed.
    pub url: String,
    pub title: String,
    /// Caller-supplied wall-clock seconds when it closed.
    pub archived_secs: u64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WindowState {
    pub id: WindowId,
    pub space_id: SpaceId,
    /// Last explicitly selected visible item, including a dormant saved item.
    /// Switching Space clears it if hidden, then restores the target Space's
    /// last selection in this window from in-memory history, if still visible.
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
    /// Archived Tabs, oldest first. Never holds private-browsing pages: the
    /// adapter records nothing for off-the-record Profiles.
    pub archived: Vec<ArchivedTab>,
    /// Browser-open clock for Auto Archive: seconds the browser has run, summed
    /// across launches. It stands still while the browser is closed or the
    /// computer sleeps. Zero until the first Auto Archive pass.
    pub open_secs: u64,
    /// This Profile's Auto Archive interval ("Archive tabs after").
    pub archive_interval: archive::ArchiveInterval,
    /// Folders of Pins, in display order within each Space. Snapshots from
    /// before folders have none.
    pub folders: Vec<Folder>,
    /// Pairs of Today tabs shown side by side. Snapshots from before Split
    /// View have none.
    pub splits: Vec<Split>,
    pub next_space_id: SpaceId,
    pub next_item_id: ItemId,
    pub next_window_id: WindowId,
    pub next_folder_id: FolderId,
}

/// What `Workspace::delete_space` changed, so adapters can close live pages and
/// refresh the windows it moved.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SpaceDeletion {
    /// Space that windows showing the deleted Space now show.
    pub fallback_space: SpaceId,
    /// Temporary and Pin items the deleted Space owned, in vector order.
    pub removed_items: Vec<ItemId>,
    /// Windows moved to `fallback_space`, in vector order.
    pub moved_windows: Vec<WindowId>,
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
    /// Every Space's Pins in workspace order, for the Library's Space cards.
    pub space_pins: Vec<Item>,
    /// The current Space's folders in order. Their Pins are also in `pins`;
    /// the rest of `pins` is ungrouped.
    pub folders: Vec<Folder>,
    /// The current Space's splits. Their items are in `temporary` too; the
    /// sidebar draws each split as one row where its left tab sits.
    pub splits: Vec<Split>,
}

#[derive(Debug)]
pub struct Workspace {
    state: WorkspaceSnapshot,
    /// Last selected item per (window, Space), recorded when a window leaves a
    /// Space. In memory only: not part of the snapshot, so it is not persisted
    /// and does not take part in equality.
    space_selections: Vec<(WindowId, SpaceId, ItemId)>,
}

impl PartialEq for Workspace {
    fn eq(&self, other: &Self) -> bool {
        self.state == other.state
    }
}

impl Eq for Workspace {}

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
                    icon: None,
                    theme: None,
                    agent: false,
                }],
                items: Vec::new(),
                windows: vec![WindowState {
                    id: DEFAULT_WINDOW_ID,
                    space_id: DEFAULT_SPACE_ID,
                    selected_item_id: None,
                }],
                archived: Vec::new(),
                open_secs: 0,
                archive_interval: archive::ArchiveInterval::Hours12,
                folders: Vec::new(),
                splits: Vec::new(),
                next_space_id: 2,
                next_item_id: 1,
                next_window_id: 2,
                next_folder_id: 1,
            },
            space_selections: Vec::new(),
        }
    }
}

impl Workspace {
    /// Legacy bridge view of the first Space in order, independent of any window
    /// selection. That is Space 1 until it is deleted; restore guarantees one Space.
    pub fn space_name(&self) -> &str {
        self.state
            .spaces
            .first()
            .map(|space| space.name.as_str())
            .unwrap_or_default()
    }

    /// Name of the Space with `id`, if it exists.
    pub fn space_name_of(&self, id: SpaceId) -> Option<&str> {
        self.state
            .spaces
            .iter()
            .find(|space| space.id == id)
            .map(|space| space.name.as_str())
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

    /// Icon of the Space with `id`, if it exists and has one.
    /// Every item in workspace order.
    pub fn items(&self) -> &[Item] {
        &self.state.items
    }

    pub fn space_icon_of(&self, id: SpaceId) -> Option<&SpaceIcon> {
        self.space(id).ok().and_then(|space| space.icon.as_ref())
    }

    /// Theme of the Space with `id`, if it exists and has one.
    pub fn space_theme_of(&self, id: SpaceId) -> Option<SpaceTheme> {
        self.space(id).ok().and_then(|space| space.theme)
    }

    pub fn create_space(
        &mut self,
        profile_id: ProfileId,
        name: &str,
    ) -> Result<SpaceId, WorkspaceError> {
        self.create_styled_space(profile_id, name, None, None)
    }

    /// Like `create_space`, for the Create a Space panel, which can pick an icon
    /// and a theme. Atomic: a bad name, icon or theme leaves the workspace
    /// unchanged.
    pub fn create_styled_space(
        &mut self,
        profile_id: ProfileId,
        name: &str,
        icon: Option<SpaceIcon>,
        theme: Option<SpaceTheme>,
    ) -> Result<SpaceId, WorkspaceError> {
        validate_profile(profile_id)?;
        validate_name(name).map_err(WorkspaceError::InvalidName)?;
        if let Some(icon) = &icon {
            validate_icon(icon)?;
        }
        validate_theme(theme)?;
        check_limit(self.state.spaces.len(), MAX_SPACES, "Spaces")?;
        let id = self.state.next_space_id;
        let next = next_id(id, "Space")?;
        self.state.spaces.push(Space {
            id,
            profile_id,
            name: name.to_owned(),
            icon,
            theme,
            agent: false,
        });
        self.state.next_space_id = next;
        Ok(id)
    }

    /// The Profile's agent Space, if it has one.
    pub fn agent_space_of(&self, profile_id: ProfileId) -> Option<SpaceId> {
        self.state
            .spaces
            .iter()
            .find(|space| space.agent && space.profile_id == profile_id)
            .map(|space| space.id)
    }

    /// The Profile's agent Space, created when it has none: a Space named
    /// `AGENT_SPACE_NAME` with the `AGENT_SPACE_ICON` icon, after the
    /// Profile's other Spaces. Idempotent. It never changes a window's
    /// selected Space or item, so agents can work without moving the user.
    pub fn ensure_agent_space(&mut self, profile_id: ProfileId) -> Result<SpaceId, WorkspaceError> {
        validate_profile(profile_id)?;
        if let Some(id) = self.agent_space_of(profile_id) {
            return Ok(id);
        }
        let id = self.create_styled_space(
            profile_id,
            AGENT_SPACE_NAME,
            Some(SpaceIcon::Named(AGENT_SPACE_ICON.to_owned())),
            None,
        )?;
        if let Some(space) = self.state.spaces.iter_mut().find(|space| space.id == id) {
            space.agent = true;
        }
        Ok(id)
    }

    /// Like `create_styled_space`, for a Space made in a new Profile from
    /// another Profile's window. While the workspace is still the untouched
    /// default (one Space named "Everyday" with no icon or theme, no items or
    /// archive, and no Space ever created), the new Space takes the default
    /// Space's place and keeps its ID, so the Profile holds only the Space the
    /// user made. Otherwise it is created as usual. Atomic either way.
    pub fn create_first_space(
        &mut self,
        profile_id: ProfileId,
        name: &str,
        icon: Option<SpaceIcon>,
        theme: Option<SpaceTheme>,
    ) -> Result<SpaceId, WorkspaceError> {
        let untouched = self.state.next_space_id == DEFAULT_SPACE_ID + 1
            && self.state.items.is_empty()
            && self.state.archived.is_empty()
            && self.state.folders.is_empty()
            && matches!(
                self.state.spaces.as_slice(),
                [space] if space.id == DEFAULT_SPACE_ID
                    && space.profile_id == profile_id
                    && space.name == "Everyday"
                    && space.icon.is_none()
                    && space.theme.is_none()
                    && !space.agent
            );
        if !untouched {
            return self.create_styled_space(profile_id, name, icon, theme);
        }
        validate_profile(profile_id)?;
        validate_name(name).map_err(WorkspaceError::InvalidName)?;
        if let Some(icon) = &icon {
            validate_icon(icon)?;
        }
        validate_theme(theme)?;
        let space = &mut self.state.spaces[0];
        space.name = name.to_owned();
        space.icon = icon;
        space.theme = theme;
        Ok(DEFAULT_SPACE_ID)
    }

    /// Replace a Space's icon; `None` restores the default glyph. Returns
    /// whether it changed, so adapters can skip writes.
    pub fn set_space_icon(
        &mut self,
        id: SpaceId,
        icon: Option<SpaceIcon>,
    ) -> Result<bool, WorkspaceError> {
        if let Some(icon) = &icon {
            validate_icon(icon)?;
        }
        let space = self
            .state
            .spaces
            .iter_mut()
            .find(|space| space.id == id)
            .ok_or(WorkspaceError::UnknownSpace(id))?;
        if space.icon == icon {
            return Ok(false);
        }
        space.icon = icon;
        Ok(true)
    }

    /// Replace a Space's theme; `None` restores the default tint. Returns
    /// whether it changed, so adapters can skip writes.
    pub fn set_space_theme(
        &mut self,
        id: SpaceId,
        theme: Option<SpaceTheme>,
    ) -> Result<bool, WorkspaceError> {
        validate_theme(theme)?;
        let space = self
            .state
            .spaces
            .iter_mut()
            .find(|space| space.id == id)
            .ok_or(WorkspaceError::UnknownSpace(id))?;
        if space.theme == theme {
            return Ok(false);
        }
        space.theme = theme;
        Ok(true)
    }

    /// Delete a Space with the Temporary and Pin items it owns (BC-009). Any Space
    /// can go except the last of its Profile. Favorites are Profile-scoped and
    /// stay. Windows showing it move to the previous Space in vector order, or the
    /// next one when it was first; a selection hidden by the move is cleared.
    /// Atomic: every check runs before mutation. The caller confirms with the user
    /// first and closes live pages for `removed_items`. IDs are never reused.
    pub fn delete_space(&mut self, space_id: SpaceId) -> Result<SpaceDeletion, WorkspaceError> {
        let profile_id = self.space(space_id)?.profile_id;
        let siblings: Vec<SpaceId> = self
            .state
            .spaces
            .iter()
            .filter(|space| space.profile_id == profile_id)
            .map(|space| space.id)
            .collect();
        if siblings.len() < 2 {
            return Err(WorkspaceError::LastSpaceInProfile(space_id));
        }
        let position = siblings
            .iter()
            .position(|&id| id == space_id)
            .ok_or(WorkspaceError::UnknownSpace(space_id))?;
        let fallback_space = *siblings
            .get(position.wrapping_sub(1))
            .or_else(|| siblings.get(position + 1))
            .ok_or(WorkspaceError::LastSpaceInProfile(space_id))?;
        let owned = |placement: Placement| {
            matches!(
                placement,
                Placement::Temporary { space_id: owner } | Placement::Pin { space_id: owner }
                    if owner == space_id
            )
        };
        let removed_items = self
            .state
            .items
            .iter()
            .filter(|item| owned(item.placement))
            .map(|item| item.id)
            .collect();
        let mut moved_windows = Vec::new();
        for window in &mut self.state.windows {
            if window.space_id == space_id {
                window.space_id = fallback_space;
                moved_windows.push(window.id);
            }
        }
        self.state.items.retain(|item| !owned(item.placement));
        self.state.spaces.retain(|space| space.id != space_id);
        self.prune_folders();
        self.space_selections
            .retain(|&(_, space, _)| space != space_id);
        self.clear_hidden_selections();
        Ok(SpaceDeletion {
            fallback_space,
            removed_items,
            moved_windows,
        })
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
        self.space_selections
            .retain(|&(window, _, _)| window != window_id);
        Ok(())
    }

    pub fn select_space(
        &mut self,
        window_id: WindowId,
        space_id: SpaceId,
    ) -> Result<(), WorkspaceError> {
        self.space(space_id)?;
        let window = self.window(window_id)?;
        let (left, selected) = (window.space_id, window.selected_item_id);
        if let Some(item) = selected {
            self.space_selections
                .retain(|&(window, space, _)| (window, space) != (window_id, left));
            self.space_selections.push((window_id, left, item));
        }
        for window in &mut self.state.windows {
            if window.id == window_id {
                window.space_id = space_id;
            }
        }
        self.clear_hidden_selections();
        // Arc returns each Space to its last tab in this window.
        let remembered = self
            .space_selections
            .iter()
            .find(|&&(window, space, _)| (window, space) == (window_id, space_id))
            .map(|&(_, _, item)| item)
            .filter(|&id| {
                self.state
                    .items
                    .iter()
                    .any(|item| item.id == id && visible(item.placement, space_id))
            });
        if let Some(item) = remembered {
            for window in &mut self.state.windows {
                if window.id == window_id {
                    window.selected_item_id = Some(item);
                }
            }
        }
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

    /// Selects `item_id` in `window_id` when the window shows its Space, or
    /// makes it the item the window returns to the next time it selects that
    /// Space, without switching Space. Favorites select now.
    pub fn select_item_in_its_space(
        &mut self,
        window_id: WindowId,
        item_id: ItemId,
    ) -> Result<(), WorkspaceError> {
        let shown = self.window(window_id)?.space_id;
        match self.item(item_id)?.placement {
            Placement::Temporary { space_id } | Placement::Pin { space_id }
                if space_id != shown =>
            {
                self.space_selections
                    .retain(|&(window, space, _)| (window, space) != (window_id, space_id));
                self.space_selections.push((window_id, space_id, item_id));
                Ok(())
            }
            _ => self.select_item(window_id, item_id),
        }
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
            title: String::new(),
            name: String::new(),
            last_viewed_secs: 0,
        });
        self.state.next_item_id = next;
        Ok(id)
    }

    /// `Some(url)` deliberately replaces the saved URL. `None` preserves a saved
    /// URL when saving/moving, and a temporary item's remembered page when saving
    /// it or moving it between Spaces. A first save without one requires `Some(url)`.
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
                item.canonical_url.clone()
            }
            Placement::Pin { .. } | Placement::Favorite { .. } => {
                let url = canonical_url
                    .or(item.canonical_url.as_deref())
                    .ok_or(WorkspaceError::CanonicalUrlRequired)?;
                validate_url(url)?;
                Some(url.to_owned())
            }
        };
        let saved = !matches!(placement, Placement::Temporary { .. });
        for item in &mut self.state.items {
            if item.id == item_id {
                if saved {
                    item.last_viewed_secs = 0;
                }
                item.placement = placement;
                item.canonical_url = url;
                break;
            }
        }
        self.prune_folders();
        self.clear_hidden_selections();
        Ok(())
    }

    /// Replace a saved item's canonical URL, like Arc's "Replace Pinned URL with
    /// Current" (BC-003). The caller passes the live page's browser-canonicalized
    /// URL; this is the only way a saved URL changes besides `set_placement`.
    /// Temporary items reject with `NoCanonicalUrl`. The same URL is a no-op success.
    pub fn set_canonical_url(&mut self, item_id: ItemId, url: &str) -> Result<(), WorkspaceError> {
        if matches!(self.item(item_id)?.placement, Placement::Temporary { .. }) {
            return Err(WorkspaceError::NoCanonicalUrl(item_id));
        }
        validate_url(url)?;
        for item in &mut self.state.items {
            if item.id == item_id {
                item.canonical_url = Some(url.to_owned());
                break;
            }
        }
        Ok(())
    }

    /// Remember a saved item's page title for dormant rows. The caller passes the
    /// live page's title only while it shows the canonical URL. Control characters
    /// become spaces and the text is cut to `MAX_TITLE_BYTES` on a character
    /// boundary. Returns whether the stored title changed, so adapters can skip
    /// writes. Temporary items reject with `NoCanonicalUrl`.
    pub fn set_title(&mut self, item_id: ItemId, title: &str) -> Result<bool, WorkspaceError> {
        if matches!(self.item(item_id)?.placement, Placement::Temporary { .. }) {
            return Err(WorkspaceError::NoCanonicalUrl(item_id));
        }
        let clean = clean_title(title);
        let item = self
            .state
            .items
            .iter_mut()
            .find(|item| item.id == item_id)
            .ok_or(WorkspaceError::UnknownItem(item_id))?;
        if item.title == clean {
            return Ok(false);
        }
        item.title = clean;
        Ok(true)
    }

    /// Rename a saved item, as Arc's Rename does for a pinned tab. The name shows
    /// instead of the page title until cleared. Control characters become
    /// spaces, the name is trimmed and cut to `MAX_TITLE_BYTES` on a character
    /// boundary, and an empty name clears it, so the row shows the page title
    /// again. Returns whether the stored name changed, so adapters can skip
    /// writes. Temporary items reject with `NotSaved`.
    pub fn rename_item(&mut self, item_id: ItemId, name: &str) -> Result<bool, WorkspaceError> {
        if matches!(self.item(item_id)?.placement, Placement::Temporary { .. }) {
            return Err(WorkspaceError::NotSaved(item_id));
        }
        let clean = clean_title(name).trim().to_owned();
        let item = self
            .state
            .items
            .iter_mut()
            .find(|item| item.id == item_id)
            .ok_or(WorkspaceError::UnknownItem(item_id))?;
        if item.name == clean {
            return Ok(false);
        }
        item.name = clean;
        Ok(true)
    }

    /// True when a live saved item shows a page other than its canonical URL, which
    /// drives Arc's slash next to the pinned name (BC-003). Comparison is exact string
    /// equality, so the caller must normalize `current_url` the same way as the saved
    /// URL; a trailing slash or fragment difference counts as navigated away. The
    /// string is not validated (pages may show any scheme) and is never stored.
    /// Temporary items reject with `NoCanonicalUrl`.
    pub fn differs_from_canonical(
        &self,
        item_id: ItemId,
        current_url: &str,
    ) -> Result<bool, WorkspaceError> {
        let item = self.item(item_id)?;
        if matches!(item.placement, Placement::Temporary { .. }) {
            return Err(WorkspaceError::NoCanonicalUrl(item_id));
        }
        item.canonical_url
            .as_deref()
            .map(|canonical| canonical != current_url)
            .ok_or(WorkspaceError::NoCanonicalUrl(item_id))
    }

    /// Remember the page a live temporary item shows, so its row stays in the
    /// sidebar, dormant, if the browser does not restore its tab, and reopens
    /// `url`. The caller passes a browser-canonicalized URL, stored whole like
    /// Chromium's session files. The title is cleaned like `set_title`.
    /// Returns whether anything changed. Saved items reject with
    /// `ExplicitUnsaveRequired`: their URL changes only through `set_canonical_url`.
    /// Callers whose Profile saves no history do not call this.
    pub fn set_temporary_page(
        &mut self,
        item_id: ItemId,
        url: &str,
        title: &str,
    ) -> Result<bool, WorkspaceError> {
        if !matches!(self.item(item_id)?.placement, Placement::Temporary { .. }) {
            return Err(WorkspaceError::ExplicitUnsaveRequired(item_id));
        }
        validate_url(url)?;
        let clean = clean_title(title);
        let item = self
            .state
            .items
            .iter_mut()
            .find(|item| item.id == item_id)
            .ok_or(WorkspaceError::UnknownItem(item_id))?;
        if item.canonical_url.as_deref() == Some(url) && item.title == clean {
            return Ok(false);
        }
        item.canonical_url = Some(url.to_owned());
        item.title = clean;
        Ok(true)
    }

    /// A dormant temporary row of `space_id` that remembers `url` (compared
    /// without fragment, which in-page navigation changes) and is not
    /// in `live`, the item IDs with a live page. A restored tab whose binding the
    /// workspace lost takes this row over instead of duplicating it.
    pub fn dormant_temporary_item(
        &self,
        space_id: SpaceId,
        url: &str,
        live: &[ItemId],
    ) -> Option<ItemId> {
        let url = without_fragment(url);
        let live: BTreeSet<ItemId> = live.iter().copied().collect();
        self.state
            .items
            .iter()
            .find(|item| {
                item.placement == Placement::Temporary { space_id }
                    && item.canonical_url.as_deref().map(without_fragment) == Some(url)
                    && !live.contains(&item.id)
            })
            .map(|item| item.id)
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
                    item.title.clear();
                    item.name.clear();
                    item.last_viewed_secs = 0;
                    break;
                }
            }
            self.prune_folders();
            self.clear_hidden_selections();
        } else {
            self.remove_item(item_id);
        }
        Ok(())
    }

    /// Call only after native-session restoration and stable-ID binding complete.
    /// Supply all bound live item IDs (saved IDs are allowed). Counts, existence
    /// and uniqueness are checked before mutation. Prunes only unbound Temporary
    /// items that remember no page, and their selections across all windows.
    /// Dormant saved items and temporary items with a page remain: a tab the
    /// browser did not restore is never grounds for dropping the user's row.
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
            !matches!(item.placement, Placement::Temporary { .. })
                || item.canonical_url.is_some()
                || live.contains(&item.id)
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

    /// Reorder Spaces: the sidebar's Space glyphs, Ctrl+1..9 and the Library
    /// follow this order. `None` moves to the end; moving before oneself is a
    /// no-op. IDs, items, windows and selections are unchanged. Spaces are
    /// all in Profile 1 until more Profiles exist.
    pub fn move_space_before(
        &mut self,
        space_id: SpaceId,
        before: Option<SpaceId>,
    ) -> Result<(), WorkspaceError> {
        self.space(space_id)?;
        if let Some(id) = before {
            self.space(id)?;
            if id == space_id {
                return Ok(());
            }
        }
        let spaces = &mut self.state.spaces;
        let source = spaces
            .iter()
            .position(|space| space.id == space_id)
            .ok_or(WorkspaceError::UnknownSpace(space_id))?;
        let space = spaces.remove(source);
        let target = match before {
            Some(id) => spaces.iter().position(|space| space.id == id),
            None => Some(spaces.len()),
        };
        // Both IDs were checked above, so the target is always found.
        let target = target.unwrap_or(source);
        spaces.insert(target, space);
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
            space_pins: Vec::new(),
            folders: self
                .state
                .folders
                .iter()
                .filter(|folder| folder.space_id == window.space_id)
                .cloned()
                .collect(),
            splits: Vec::new(),
        };
        for item in &self.state.items {
            if matches!(item.placement, Placement::Pin { .. }) {
                sidebar.space_pins.push(item.clone());
            }
            if !visible(item.placement, window.space_id) {
                continue;
            }
            match item.placement {
                Placement::Favorite { .. } => sidebar.favorites.push(item.clone()),
                Placement::Pin { .. } => sidebar.pins.push(item.clone()),
                Placement::Temporary { .. } => sidebar.temporary.push(item.clone()),
            }
        }
        sidebar.splits = self
            .state
            .splits
            .iter()
            .filter(|split| sidebar.temporary.iter().any(|item| item.id == split.left))
            .copied()
            .collect();
        Ok(sidebar)
    }

    pub fn snapshot(&self) -> WorkspaceSnapshot {
        self.state.clone()
    }

    pub fn from_snapshot(snapshot: WorkspaceSnapshot) -> Result<Self, WorkspaceError> {
        let mut workspace = Self {
            state: snapshot,
            space_selections: Vec::new(),
        };
        workspace.sanitize_archive();
        workspace.sanitize_icons();
        workspace.sanitize_names();
        workspace.sanitize_agent_spaces();
        workspace.validate()?;
        // After validation, so folders only see a consistent set of Spaces and items.
        workspace.sanitize_folders();
        workspace.sanitize_splits();
        Ok(workspace)
    }

    /// Replacement is atomic in memory on validation success, with no disk I/O.
    pub fn restore(&mut self, snapshot: WorkspaceSnapshot) -> Result<(), WorkspaceError> {
        *self = Self::from_snapshot(snapshot)?;
        Ok(())
    }

    /// A bad icon or theme falls back to the default instead of failing the
    /// snapshot: both are cosmetic, and a newer build may allow more icon names.
    fn sanitize_icons(&mut self) {
        for space in &mut self.state.spaces {
            if space
                .icon
                .as_ref()
                .is_some_and(|icon| validate_icon(icon).is_err())
            {
                space.icon = None;
            }
            if validate_theme(space.theme).is_err() {
                space.theme = None;
            }
        }
    }

    /// A name is cosmetic, so a bad one is dropped instead of failing the
    /// snapshot: oversized, with control characters or untrimmed, or on a
    /// temporary item, which never has one.
    fn sanitize_names(&mut self) {
        for item in &mut self.state.items {
            if matches!(item.placement, Placement::Temporary { .. })
                || item.name.len() > MAX_TITLE_BYTES
                || item.name.chars().any(char::is_control)
                || item.name.trim() != item.name
            {
                item.name.clear();
            }
        }
    }

    /// A Profile keeps only its first marked agent Space. A second mark can
    /// only come from a damaged or hand-edited file, so the extra Spaces load
    /// as ordinary ones instead of failing the snapshot.
    fn sanitize_agent_spaces(&mut self) {
        let mut marked = BTreeSet::new();
        for space in &mut self.state.spaces {
            if space.agent && !marked.insert(space.profile_id) {
                space.agent = false;
            }
        }
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
        self.prune_folders();
        self.clear_hidden_selections();
    }

    /// Every mutation that moves or removes items ends here, so it also ends
    /// splits those changes broke.
    fn clear_hidden_selections(&mut self) {
        self.prune_splits();
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
        if state.spaces.is_empty() {
            return Err(WorkspaceError::InvalidSnapshot(
                "a Profile needs at least one Space",
            ));
        }
        for space in &state.spaces {
            validate_profile(space.profile_id)?;
            validate_name(&space.name).map_err(WorkspaceError::InvalidName)?;
        }
        for item in &state.items {
            self.validate_placement(item.placement)?;
            match (item.placement, item.canonical_url.as_deref()) {
                (Placement::Temporary { .. }, None) => {}
                (_, Some(url)) => validate_url(url)?,
                (_, None) => return Err(WorkspaceError::CanonicalUrlRequired),
            }
            if item.title.len() > MAX_TITLE_BYTES || item.title.chars().any(char::is_control) {
                return Err(WorkspaceError::InvalidSnapshot(
                    "item title is oversized or has control characters",
                ));
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

fn validate_icon(icon: &SpaceIcon) -> Result<(), WorkspaceError> {
    let valid = match icon {
        SpaceIcon::Emoji(text) => {
            !text.is_empty()
                && text.len() <= MAX_SPACE_ICON_BYTES
                && !text.is_ascii()
                && !text.chars().any(|ch| ch.is_control() || ch.is_whitespace())
        }
        SpaceIcon::Named(name) => {
            name.len() <= MAX_SPACE_ICON_BYTES
                && name.starts_with(|ch: char| ch.is_ascii_lowercase())
                && !name.ends_with('-')
                && !name.contains("--")
                && name
                    .chars()
                    .all(|ch| ch.is_ascii_lowercase() || ch.is_ascii_digit() || ch == '-')
        }
    };
    if valid {
        Ok(())
    } else {
        Err(WorkspaceError::InvalidIcon)
    }
}

fn validate_theme(theme: Option<SpaceTheme>) -> Result<(), WorkspaceError> {
    match theme {
        Some(theme) if theme.color > 0xFF_FFFF || theme.opacity > MAX_SPACE_THEME_OPACITY => {
            Err(WorkspaceError::InvalidTheme)
        }
        _ => Ok(()),
    }
}

/// Control characters become spaces; the text is cut to `MAX_TITLE_BYTES` on a
/// character boundary.
fn clean_title(title: &str) -> String {
    let mut clean = String::with_capacity(title.len().min(MAX_TITLE_BYTES));
    for ch in title.chars() {
        let ch = if ch.is_control() { ' ' } else { ch };
        if clean.len() + ch.len_utf8() > MAX_TITLE_BYTES {
            break;
        }
        clean.push(ch);
    }
    clean
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

/// `url` up to its fragment.
fn without_fragment(url: &str) -> &str {
    url.find('#').map_or(url, |end| &url[..end])
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
    fn space_name_of_reports_the_renamed_space() {
        let mut workspace = Workspace::default();
        let second = workspace.create_space(DEFAULT_PROFILE_ID, "Two").unwrap();
        workspace.rename_space(second, "Study").unwrap();
        assert_eq!(workspace.space_name_of(second), Some("Study"));
        assert_eq!(workspace.space_name(), "Everyday");
        workspace.delete_space(TEST_SPACE_ID).unwrap();
        workspace.rename_space(second, "Work").unwrap();
        assert_eq!(workspace.space_name_of(second), Some("Work"));
        assert_eq!(workspace.space_name_of(TEST_SPACE_ID), None);
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

    const URL_A: &str = "https://example.test/a";
    const URL_B: &str = "https://example.test/b";

    fn pin(workspace: &mut Workspace, space: SpaceId) -> ItemId {
        let id = workspace.add_temporary_item(space).unwrap();
        workspace
            .set_placement(id, Placement::Pin { space_id: space }, Some(URL_A))
            .unwrap();
        id
    }

    #[test]
    fn set_canonical_url_replaces_pin_and_favorite() {
        let mut workspace = Workspace::default();
        let id = pin(&mut workspace, DEFAULT_SPACE_ID);
        assert_eq!(workspace.set_canonical_url(id, URL_B), Ok(()));
        assert_eq!(
            workspace.item(id).unwrap().canonical_url.as_deref(),
            Some(URL_B)
        );
        assert_eq!(workspace.set_canonical_url(id, URL_B), Ok(()));
        let placement = Placement::Favorite {
            profile_id: DEFAULT_PROFILE_ID,
        };
        workspace.set_placement(id, placement, None).unwrap();
        assert_eq!(workspace.set_canonical_url(id, URL_A), Ok(()));
        let item = workspace.item(id).unwrap();
        assert_eq!(item.placement, placement);
        assert_eq!(item.canonical_url.as_deref(), Some(URL_A));
    }

    #[test]
    fn set_canonical_url_rejects_without_mutation() {
        let mut workspace = Workspace::default();
        let id = pin(&mut workspace, DEFAULT_SPACE_ID);
        let temporary = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let before = workspace.snapshot();
        assert_eq!(
            workspace.set_canonical_url(99, URL_B),
            Err(WorkspaceError::UnknownItem(99))
        );
        assert_eq!(
            workspace.set_canonical_url(temporary, URL_B),
            Err(WorkspaceError::NoCanonicalUrl(temporary))
        );
        for bad in [
            "",
            "ftp://example.test",
            "https://u@example.test",
            "https://a b",
        ] {
            assert_eq!(
                workspace.set_canonical_url(id, bad),
                Err(WorkspaceError::InvalidUrl)
            );
        }
        assert_eq!(workspace.snapshot(), before);
    }

    #[test]
    fn set_title_round_trips_through_snapshot() {
        let mut workspace = Workspace::default();
        let id = pin(&mut workspace, DEFAULT_SPACE_ID);
        assert_eq!(workspace.set_title(id, "Mauricio\nWolff"), Ok(true));
        assert_eq!(workspace.set_title(id, "Mauricio Wolff"), Ok(false));
        let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        assert_eq!(restored.item(id).unwrap().title, "Mauricio Wolff");

        let long = "é".repeat(MAX_TITLE_BYTES);
        assert_eq!(workspace.set_title(id, &long), Ok(true));
        let title = &workspace.item(id).unwrap().title;
        assert!(title.len() <= MAX_TITLE_BYTES && title.chars().all(|ch| ch == 'é'));

        workspace.unsave_item(id, Some(DEFAULT_SPACE_ID)).unwrap();
        assert!(workspace.item(id).unwrap().title.is_empty());
        assert_eq!(
            workspace.set_title(id, "Page"),
            Err(WorkspaceError::NoCanonicalUrl(id))
        );

        let mut snapshot = workspace.snapshot();
        snapshot.items[0].title = String::from("Bad\ttitle");
        assert!(matches!(
            Workspace::from_snapshot(snapshot),
            Err(WorkspaceError::InvalidSnapshot(_))
        ));
    }

    #[test]
    fn differs_from_canonical_is_exact_comparison() {
        let mut workspace = Workspace::default();
        let id = pin(&mut workspace, DEFAULT_SPACE_ID);
        assert_eq!(workspace.differs_from_canonical(id, URL_A), Ok(false));
        assert_eq!(workspace.differs_from_canonical(id, URL_B), Ok(true));
        assert_eq!(
            workspace.differs_from_canonical(id, "https://example.test/a/"),
            Ok(true)
        );
        assert_eq!(workspace.differs_from_canonical(id, ""), Ok(true));
        assert_eq!(
            workspace.differs_from_canonical(id, "chrome://settings"),
            Ok(true)
        );
        workspace.set_canonical_url(id, URL_B).unwrap();
        assert_eq!(workspace.differs_from_canonical(id, URL_B), Ok(false));
        let temporary = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        assert_eq!(
            workspace.differs_from_canonical(temporary, URL_A),
            Err(WorkspaceError::NoCanonicalUrl(temporary))
        );
        assert_eq!(
            workspace.differs_from_canonical(99, URL_A),
            Err(WorkspaceError::UnknownItem(99))
        );
    }

    #[test]
    fn delete_space_removes_owned_items_and_keeps_others() {
        let mut workspace = Workspace::default();
        let s2 = workspace.create_space(DEFAULT_PROFILE_ID, "Two").unwrap();
        let s3 = workspace.create_space(DEFAULT_PROFILE_ID, "Three").unwrap();
        let kept = pin(&mut workspace, DEFAULT_SPACE_ID);
        let owned_pin = pin(&mut workspace, s2);
        let owned_temporary = workspace.add_temporary_item(s2).unwrap();
        let other = pin(&mut workspace, s3);
        let deletion = workspace.delete_space(s2).unwrap();
        assert_eq!(deletion.removed_items, vec![owned_pin, owned_temporary]);
        assert_eq!(deletion.fallback_space, DEFAULT_SPACE_ID);
        let state = workspace.snapshot();
        let ids: Vec<ItemId> = state.items.iter().map(|item| item.id).collect();
        assert_eq!(ids, vec![kept, other]);
        let spaces: Vec<SpaceId> = state.spaces.iter().map(|space| space.id).collect();
        assert_eq!(spaces, vec![DEFAULT_SPACE_ID, s3]);
        assert_eq!(
            workspace.create_space(DEFAULT_PROFILE_ID, "Four"),
            Ok(s3 + 1)
        );
    }

    #[test]
    fn delete_space_picks_previous_then_next_neighbor() {
        let mut workspace = Workspace::default();
        let s2 = workspace.create_space(DEFAULT_PROFILE_ID, "Two").unwrap();
        let s3 = workspace.create_space(DEFAULT_PROFILE_ID, "Three").unwrap();
        assert_eq!(workspace.delete_space(s3).unwrap().fallback_space, s2);
        assert_eq!(
            workspace.delete_space(s2).unwrap().fallback_space,
            DEFAULT_SPACE_ID
        );
    }

    #[test]
    fn delete_space_one_when_others_exist_renames_legacy_view() {
        let mut workspace = Workspace::default();
        let s2 = workspace.create_space(DEFAULT_PROFILE_ID, "Two").unwrap();
        let deletion = workspace.delete_space(DEFAULT_SPACE_ID).unwrap();
        assert_eq!(deletion.fallback_space, s2);
        assert_eq!(workspace.space_name(), "Two");
        assert_eq!(
            Workspace::from_snapshot(workspace.snapshot()).unwrap(),
            workspace
        );
    }

    fn emoji(text: &str) -> Option<SpaceIcon> {
        Some(SpaceIcon::Emoji(text.into()))
    }

    fn named(name: &str) -> Option<SpaceIcon> {
        Some(SpaceIcon::Named(name.into()))
    }

    #[test]
    fn space_icon_round_trips_through_snapshot() {
        let mut workspace = Workspace::default();
        let coffee = workspace
            .create_styled_space(DEFAULT_PROFILE_ID, "Coffee", emoji("☕"), None)
            .unwrap();
        let book = workspace
            .create_styled_space(DEFAULT_PROFILE_ID, "Reading", named("book-open"), None)
            .unwrap();
        let plain = workspace.create_space(DEFAULT_PROFILE_ID, "Plain").unwrap();
        for text in ["👩🏽‍💻", "🇧🇷", "1️⃣", "❤️", "👩🏽‍❤️‍💋‍👨🏻"]
        {
            assert_eq!(workspace.set_space_icon(plain, emoji(text)), Ok(true));
        }
        assert_eq!(workspace.set_space_icon(plain, named("h1")), Ok(true));
        assert_eq!(workspace.set_space_icon(plain, named("h1")), Ok(false));
        let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        assert_eq!(restored, workspace);
        assert_eq!(restored.space_icon_of(coffee), emoji("☕").as_ref());
        assert_eq!(restored.space_icon_of(book), named("book-open").as_ref());
        assert_eq!(restored.space_icon_of(plain), named("h1").as_ref());
        assert_eq!(restored.space_icon_of(DEFAULT_SPACE_ID), None);
        assert_eq!(workspace.set_space_icon(plain, None), Ok(true));
        assert_eq!(workspace.space_icon_of(plain), None);
    }

    #[test]
    fn rejects_invalid_space_icons_without_mutation() {
        let mut workspace = Workspace::default();
        let space = workspace
            .create_styled_space(DEFAULT_PROFILE_ID, "Two", emoji("☕"), None)
            .unwrap();
        let before = workspace.snapshot();
        let long_emoji = "☕".repeat(MAX_SPACE_ICON_BYTES / 3 + 1);
        let long_name = "a".repeat(MAX_SPACE_ICON_BYTES + 1);
        let bad = [
            emoji(""),
            emoji("a"),
            emoji(":)"),
            emoji("☕ "),
            emoji("☕\n"),
            emoji("\u{2003}"),
            emoji(&long_emoji),
            named(""),
            named("Star"),
            named("1star"),
            named("-star"),
            named("star-"),
            named("book--open"),
            named("book_open"),
            named("star "),
            named("é"),
            named(&long_name),
        ];
        for icon in bad {
            assert_eq!(
                workspace.create_styled_space(DEFAULT_PROFILE_ID, "Three", icon.clone(), None),
                Err(WorkspaceError::InvalidIcon),
                "{icon:?}"
            );
            assert_eq!(
                workspace.set_space_icon(space, icon.clone()),
                Err(WorkspaceError::InvalidIcon),
                "{icon:?}"
            );
        }
        assert_eq!(
            workspace.set_space_icon(99, emoji("☕")),
            Err(WorkspaceError::UnknownSpace(99))
        );
        assert_eq!(workspace.snapshot(), before);
        assert_eq!(
            workspace.create_space(DEFAULT_PROFILE_ID, "Three"),
            Ok(space + 1)
        );
    }

    #[test]
    fn snapshot_without_or_with_bad_icon_loads_with_default_glyph() {
        // Snapshots written before icons carry none.
        let mut legacy = Workspace::default().snapshot();
        legacy.spaces.push(Space {
            id: 2,
            profile_id: DEFAULT_PROFILE_ID,
            name: "Work".into(),
            icon: None,
            theme: None,
            agent: false,
        });
        legacy.next_space_id = 3;
        let workspace = Workspace::from_snapshot(legacy.clone()).unwrap();
        assert_eq!(workspace.snapshot(), legacy);
        assert_eq!(workspace.space_icon_of(2), None);

        legacy.spaces[0].icon = named("Not An Icon");
        legacy.spaces[1].icon = emoji("☕");
        let workspace = Workspace::from_snapshot(legacy).unwrap();
        assert_eq!(workspace.space_icon_of(DEFAULT_SPACE_ID), None);
        assert_eq!(workspace.space_icon_of(2), emoji("☕").as_ref());
    }

    fn theme(color: u32, opacity: u8) -> Option<SpaceTheme> {
        Some(SpaceTheme { color, opacity })
    }

    #[test]
    fn space_theme_round_trips_through_snapshot() {
        let mut workspace = Workspace::default();
        let pink = workspace
            .create_styled_space(DEFAULT_PROFILE_ID, "Pink", None, theme(0xF29BBB, 60))
            .unwrap();
        let plain = workspace.create_space(DEFAULT_PROFILE_ID, "Plain").unwrap();
        assert_eq!(workspace.set_space_theme(plain, theme(0, 0)), Ok(true));
        assert_eq!(workspace.set_space_theme(plain, theme(0, 0)), Ok(false));
        assert_eq!(
            workspace.set_space_theme(plain, theme(0xFF_FFFF, 100)),
            Ok(true)
        );
        let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        assert_eq!(restored, workspace);
        assert_eq!(restored.space_theme_of(pink), theme(0xF29BBB, 60));
        assert_eq!(restored.space_theme_of(plain), theme(0xFF_FFFF, 100));
        assert_eq!(restored.space_theme_of(DEFAULT_SPACE_ID), None);
        assert_eq!(workspace.set_space_theme(plain, None), Ok(true));
        assert_eq!(workspace.space_theme_of(plain), None);
    }

    #[test]
    fn first_space_replaces_only_the_untouched_default() {
        let mut fresh = Workspace::default();
        assert_eq!(
            fresh.create_first_space(
                DEFAULT_PROFILE_ID,
                "Client",
                emoji("🚀"),
                theme(0x336699, 50)
            ),
            Ok(DEFAULT_SPACE_ID)
        );
        assert_eq!(fresh.space_name_of(DEFAULT_SPACE_ID), Some("Client"));
        assert_eq!(fresh.space_icon_of(DEFAULT_SPACE_ID), emoji("🚀").as_ref());
        assert_eq!(fresh.space_theme_of(DEFAULT_SPACE_ID), theme(0x336699, 50));
        assert_eq!(fresh.snapshot().spaces.len(), 1);
        assert_eq!(fresh.snapshot().next_space_id, 2);

        // A bad request leaves the default Space alone.
        let mut fresh = Workspace::default();
        let before = fresh.snapshot();
        assert_eq!(
            fresh.create_first_space(DEFAULT_PROFILE_ID, "Client", None, theme(0, 101)),
            Err(WorkspaceError::InvalidTheme)
        );
        assert!(
            fresh
                .create_first_space(DEFAULT_PROFILE_ID, "", None, None)
                .is_err()
        );
        assert_eq!(fresh.snapshot(), before);

        // Any trace of use keeps the default Space and adds a new one.
        let renamed = |workspace: &mut Workspace| {
            workspace.rename_space(DEFAULT_SPACE_ID, "Mine").unwrap();
        };
        let iconed = |workspace: &mut Workspace| {
            workspace
                .set_space_icon(DEFAULT_SPACE_ID, emoji("☕"))
                .unwrap();
        };
        let themed = |workspace: &mut Workspace| {
            workspace
                .set_space_theme(DEFAULT_SPACE_ID, theme(0, 0))
                .unwrap();
        };
        let with_item = |workspace: &mut Workspace| {
            workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        };
        let created_and_deleted = |workspace: &mut Workspace| {
            let extra = workspace.create_space(DEFAULT_PROFILE_ID, "Extra").unwrap();
            workspace.delete_space(extra).unwrap();
        };
        let touches: [&dyn Fn(&mut Workspace); 5] =
            [&renamed, &iconed, &themed, &with_item, &created_and_deleted];
        for touch in touches {
            let mut workspace = Workspace::default();
            touch(&mut workspace);
            let kept = workspace.snapshot().spaces[0].clone();
            let id = workspace
                .create_first_space(DEFAULT_PROFILE_ID, "Client", None, None)
                .unwrap();
            assert_ne!(id, DEFAULT_SPACE_ID);
            assert_eq!(workspace.snapshot().spaces[0], kept);
            assert_eq!(workspace.space_name_of(id), Some("Client"));
        }
    }

    #[test]
    fn rejects_invalid_space_themes_without_mutation() {
        let mut workspace = Workspace::default();
        let space = workspace
            .create_styled_space(DEFAULT_PROFILE_ID, "Two", None, theme(0x336699, 50))
            .unwrap();
        let before = workspace.snapshot();
        for bad in [
            theme(0x100_0000, 50),
            theme(0x336699, 101),
            theme(u32::MAX, u8::MAX),
        ] {
            assert_eq!(
                workspace.create_styled_space(DEFAULT_PROFILE_ID, "Three", emoji("☕"), bad),
                Err(WorkspaceError::InvalidTheme),
                "{bad:?}"
            );
            assert_eq!(
                workspace.set_space_theme(space, bad),
                Err(WorkspaceError::InvalidTheme),
                "{bad:?}"
            );
        }
        assert_eq!(
            workspace.set_space_theme(99, theme(0, 0)),
            Err(WorkspaceError::UnknownSpace(99))
        );
        assert_eq!(workspace.snapshot(), before);
    }

    #[test]
    fn snapshot_without_or_with_bad_theme_loads_with_default_tint() {
        // Snapshots written before themes carry none.
        let mut legacy = Workspace::default().snapshot();
        legacy.spaces.push(Space {
            id: 2,
            profile_id: DEFAULT_PROFILE_ID,
            name: "Work".into(),
            icon: emoji("☕"),
            theme: None,
            agent: false,
        });
        legacy.next_space_id = 3;
        let workspace = Workspace::from_snapshot(legacy.clone()).unwrap();
        assert_eq!(workspace.snapshot(), legacy);
        assert_eq!(workspace.space_theme_of(2), None);

        legacy.spaces[0].theme = theme(0x123456, 101);
        legacy.spaces[1].theme = theme(0x123456, 40);
        let workspace = Workspace::from_snapshot(legacy).unwrap();
        assert_eq!(workspace.space_theme_of(DEFAULT_SPACE_ID), None);
        assert_eq!(workspace.space_theme_of(2), theme(0x123456, 40));
        assert_eq!(workspace.space_icon_of(2), emoji("☕").as_ref());
    }

    #[test]
    fn delete_space_rejects_without_mutation() {
        let mut workspace = Workspace::default();
        assert_eq!(
            workspace.delete_space(DEFAULT_SPACE_ID),
            Err(WorkspaceError::LastSpaceInProfile(DEFAULT_SPACE_ID))
        );
        let s2 = workspace.create_space(DEFAULT_PROFILE_ID, "Two").unwrap();
        pin(&mut workspace, s2);
        let before = workspace.snapshot();
        assert_eq!(
            workspace.delete_space(99),
            Err(WorkspaceError::UnknownSpace(99))
        );
        assert_eq!(workspace.snapshot(), before);
    }
}
