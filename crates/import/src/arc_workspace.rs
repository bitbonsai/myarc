//! Maps an Arc [`Report`] into a myarc [`Workspace`].
//!
//! The report's [`Report::favorites`] become Favorites of the workspace's
//! Profile, after any it already has. Only Arc's sidebar file fills that list.
//! Each Space becomes a Space with that name, and its [`Space::pins`] become
//! Pins outside any folder. Each group becomes a folder of that Space's Pins
//! with the group's name, and its URLs become Pins in that folder, in order,
//! with child groups nested as subfolders.
//! A Markdown `Favorites` group is the exception: its URLs become ungrouped
//! Pins, which the sidebar shows as tiles, because Arc's Favorites are tiles
//! rather than a folder. They stay Space Pins rather than Profile Favorites
//! because the Markdown export does not say which they are (see
//! `tests/fixtures/arc-markdown/README.md`). A new Favorite or Pin takes its
//! entry's name as its Rename name and its saved page title as its title; only
//! Arc's sidebar file has them. [`apply_to_new_profile`] is for a Profile made
//! for the import: its first new Space takes the place of the empty default
//! Space (`Workspace::create_first_space`), so the Profile holds only Arc's
//! Spaces.
//!
//! Importing is idempotent. A Favorite whose URL the Profile already has is
//! kept. An export Space reuses an existing ordinary Space of the same name,
//! and a group reuses the first folder of that Space with the same parent and
//! name. A URL the folder already holds is kept. A URL of a group that the
//! Space pins outside any folder moves into the folder rather than being added
//! again, which tidies up an import made before folders existed. An ungrouped
//! URL the Space already pins outside any folder is kept. Each existing
//! Favorite or Pin answers for one entry, so repeated URLs stay repeated.
//! Every import is rehearsed on a copy first, so a failed import changes
//! nothing. Nothing here opens tabs or fetches URLs.
//!
//! The browser canonicalizes URLs before they reach the workspace, so callers
//! pass one canonical URL per entry, in [`entry_urls`] order, or `None` for an
//! entry the browser rejected. [`as_written`] keeps HTTP(S) URLs unchanged, for
//! tests and tools without a URL parser.

use std::collections::BTreeMap;
use std::fmt;

use myarc_workspace::{
    DEFAULT_SPACE_ID, FolderId, ItemId, MAX_FOLDER_NAME_BYTES, MAX_SPACE_NAME_BYTES, Placement,
    SpaceId, Workspace, WorkspaceError,
};

use crate::arc_markdown::{Entry, Group, Report, Scheme};

/// The group label Arc gives a Space's Favorites. Its URLs become ungrouped Pins.
pub const FAVORITES_LABEL: &str = "Favorites";

/// What an import would change, computed against the current workspace.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Preview {
    /// The Profile Favorites the import adds, in order.
    pub new_favorites: Vec<NewItem>,
    /// Favorites the Profile already has, so the import leaves them alone.
    pub kept_favorites: usize,
    /// One per distinct Space name, in the order the export first lists it.
    pub spaces: Vec<SpacePreview>,
    /// Entries that will not be imported.
    pub skipped: Vec<Skipped>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SpacePreview {
    pub name: String,
    /// The Space this name reuses, or `None` when the import creates it.
    pub existing: Option<SpaceId>,
    /// What the import adds as ungrouped Pins, in order: the Space's pins and
    /// its Markdown `Favorites` groups.
    pub new_pins: Vec<NewItem>,
    /// How many of `new_pins` come from a Markdown `Favorites` group.
    pub favorites_as_pins: usize,
    /// Entries the Space already pins in the right place, so the import leaves
    /// them alone. Covers folders too.
    pub kept_pins: usize,
    /// Top-level folders in export order.
    pub folders: Vec<FolderPreview>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FolderPreview {
    pub name: String,
    /// The folder this name reuses, or `None` when the import creates it.
    pub existing: Option<FolderId>,
    /// What the folder gains at its end, in export order.
    pub additions: Vec<FolderAddition>,
    /// Nested subfolders in export order.
    pub children: Vec<FolderPreview>,
}

impl FolderPreview {
    fn collect_all<'a>(&'a self, out: &mut Vec<&'a FolderPreview>) {
        out.push(self);
        for child in &self.children {
            child.collect_all(out);
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum FolderAddition {
    /// A new Pin.
    New(NewItem),
    /// An ungrouped Pin of the Space with the entry's URL, moved in.
    Move(ItemId),
}

/// A Favorite or Pin the import adds.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NewItem {
    /// Canonical URL.
    pub url: String,
    /// The entry's name, set as the item's Rename name. Empty when none.
    pub name: String,
    /// The entry's saved page title. Empty when unknown.
    pub title: String,
}

impl NewItem {
    fn new(url: &str, entry: &Entry) -> Self {
        Self {
            url: url.to_owned(),
            name: entry.name.clone(),
            title: entry.title.clone(),
        }
    }
}

/// An entry the import leaves out.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Skipped {
    /// 1-based line of the list item.
    pub line: usize,
    pub url: String,
}

#[derive(Debug, PartialEq, Eq)]
pub enum ImportError {
    /// The canonical URL list does not have one slot per entry.
    UrlCountMismatch { expected: usize, got: usize },
    /// The workspace refused a change, for example because a limit was reached.
    Workspace(WorkspaceError),
}

impl fmt::Display for ImportError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UrlCountMismatch { expected, got } => {
                write!(f, "expected {expected} canonical URLs, got {got}")
            }
            Self::Workspace(error) => write!(f, "{error}"),
        }
    }
}

impl std::error::Error for ImportError {}

impl From<WorkspaceError> for ImportError {
    fn from(error: WorkspaceError) -> Self {
        Self::Workspace(error)
    }
}

impl Preview {
    pub fn new_space_count(&self) -> usize {
        self.spaces.iter().filter(|s| s.existing.is_none()).count()
    }

    pub fn new_folder_count(&self) -> usize {
        self.all_folders()
            .into_iter()
            .filter(|f| f.existing.is_none())
            .count()
    }

    pub fn kept_folder_count(&self) -> usize {
        self.all_folders()
            .into_iter()
            .filter(|f| f.existing.is_some())
            .count()
    }

    pub fn total_folder_count(&self) -> usize {
        self.all_folders().len()
    }

    pub fn new_favorite_count(&self) -> usize {
        self.new_favorites.len()
    }

    /// New Pins, ungrouped and in folders.
    pub fn new_pin_count(&self) -> usize {
        let ungrouped: usize = self.spaces.iter().map(|s| s.new_pins.len()).sum();
        let foldered = self
            .all_folders()
            .into_iter()
            .flat_map(|f| &f.additions)
            .filter(|a| matches!(a, FolderAddition::New(_)))
            .count();
        ungrouped + foldered
    }

    /// Ungrouped Pins the import moves into a folder.
    pub fn moved_pin_count(&self) -> usize {
        self.all_folders()
            .into_iter()
            .flat_map(|f| &f.additions)
            .filter(|a| matches!(a, FolderAddition::Move(_)))
            .count()
    }

    fn all_folders(&self) -> Vec<&FolderPreview> {
        let mut out = Vec::new();
        for space in &self.spaces {
            for folder in &space.folders {
                folder.collect_all(&mut out);
            }
        }
        out
    }

    pub fn kept_pin_count(&self) -> usize {
        self.spaces.iter().map(|s| s.kept_pins).sum()
    }

    /// True when applying would change nothing.
    pub fn is_noop(&self) -> bool {
        self.new_favorite_count() == 0
            && self.new_space_count() == 0
            && self.new_folder_count() == 0
            && self.new_pin_count() == 0
            && self.moved_pin_count() == 0
    }
}

/// Every entry's URL as written, in document order.
pub fn entry_urls(report: &Report) -> Vec<&str> {
    report.entries().map(|entry| entry.url.as_str()).collect()
}

/// HTTP(S) URLs as written and `None` for other schemes.
pub fn as_written(report: &Report) -> Vec<Option<String>> {
    report
        .entries()
        .map(|entry| {
            matches!(entry.scheme, Scheme::Http | Scheme::Https).then(|| entry.url.clone())
        })
        .collect()
}

/// What [`apply`] would do, without changing `workspace`. Fails when the import
/// would fail, for example by exceeding the workspace's Space or item limit.
pub fn preview(
    report: &Report,
    canonical: &[Option<String>],
    workspace: &Workspace,
) -> Result<Preview, ImportError> {
    let plan = plan(report, canonical, workspace)?;
    let mut copy = Workspace::from_snapshot(workspace.snapshot())?;
    write(&plan, &mut copy, false)?;
    Ok(plan)
}

/// Imports `report` into `workspace`, all or nothing. Returns what changed.
pub fn apply(
    report: &Report,
    canonical: &[Option<String>],
    workspace: &mut Workspace,
) -> Result<Preview, ImportError> {
    apply_with(report, canonical, workspace, false)
}

/// Like [`apply`], for the workspace of a Profile made for this import. While
/// that workspace is untouched, the first Space the import creates takes the
/// place of its empty default Space.
pub fn apply_to_new_profile(
    report: &Report,
    canonical: &[Option<String>],
    workspace: &mut Workspace,
) -> Result<Preview, ImportError> {
    apply_with(report, canonical, workspace, true)
}

fn apply_with(
    report: &Report,
    canonical: &[Option<String>],
    workspace: &mut Workspace,
    first: bool,
) -> Result<Preview, ImportError> {
    let plan = plan(report, canonical, workspace)?;
    if plan.is_noop() {
        return Ok(plan);
    }
    // Rehearse on a copy, so a failure leaves `workspace` as it was. The same
    // steps on the same state then succeed again; the restore only guards
    // against a workspace rule that is not deterministic.
    let before = workspace.snapshot();
    let mut copy = Workspace::from_snapshot(before.clone())?;
    write(&plan, &mut copy, first)?;
    if let Err(error) = write(&plan, workspace, first) {
        workspace.restore(before)?;
        return Err(error);
    }
    Ok(plan)
}

/// Where a report entry goes.
enum Role<'a> {
    /// A Profile Favorite.
    Favorite,
    /// An ungrouped Pin of the export Space with this index.
    Pin(usize),
    /// A Markdown `Favorites` group entry of the export Space with this index.
    FavoritesGroup(usize),
    /// An entry of a folder (possibly nested) in the export Space with this index.
    /// The slice holds the folder label path from root to target.
    Grouped(usize, Vec<&'a str>),
}

/// Every entry with its role, in [`Report::entries`] order.
fn entries(report: &Report) -> Vec<(Role<'_>, &Entry)> {
    let mut out = Vec::new();
    for entry in &report.favorites {
        out.push((Role::Favorite, entry));
    }
    for (index, space) in report.spaces.iter().enumerate() {
        for entry in &space.pins {
            out.push((Role::Pin(index), entry));
        }
        for group in &space.groups {
            if is_favorites(group) {
                let mut fav_entries = Vec::new();
                group.collect_entries(&mut fav_entries);
                for entry in fav_entries {
                    out.push((Role::FavoritesGroup(index), entry));
                }
            } else {
                let mut path = Vec::new();
                collect_group_entries(index, group, &mut path, &mut out);
            }
        }
    }
    out
}

fn collect_group_entries<'a>(
    space_index: usize,
    group: &'a Group,
    path: &mut Vec<&'a str>,
    out: &mut Vec<(Role<'a>, &'a Entry)>,
) {
    path.push(group.label.as_str());
    for entry in &group.entries {
        out.push((Role::Grouped(space_index, path.clone()), entry));
    }
    for child in &group.children {
        collect_group_entries(space_index, child, path, out);
    }
    path.pop();
}

fn is_favorites(group: &Group) -> bool {
    group.label.trim() == FAVORITES_LABEL
}

/// Pins a reused Space already has, by URL, each answering for one entry.
#[derive(Default)]
struct Present<'a> {
    /// Ungrouped Pins in workspace order.
    ungrouped: BTreeMap<&'a str, Vec<ItemId>>,
    /// Pins of each existing folder, counted per URL.
    folders: BTreeMap<FolderId, BTreeMap<&'a str, usize>>,
}

fn plan(
    report: &Report,
    canonical: &[Option<String>],
    workspace: &Workspace,
) -> Result<Preview, ImportError> {
    let expected = report.entry_count();
    if canonical.len() != expected {
        return Err(ImportError::UrlCountMismatch {
            expected,
            got: canonical.len(),
        });
    }
    let state = workspace.snapshot();
    let mut preview = Preview::default();
    // Export Space index -> preview slot, merging Spaces that share a name.
    let mut slots = Vec::with_capacity(report.spaces.len());
    for space in &report.spaces {
        let name = bounded_name(&space.label, MAX_SPACE_NAME_BYTES);
        let slot = match preview.spaces.iter().position(|s| s.name == name) {
            Some(slot) => slot,
            None => {
                let existing = state
                    .spaces
                    .iter()
                    .find(|s| s.profile_id == state.profile_id && !s.agent && s.name == name)
                    .map(|s| s.id);
                preview.spaces.push(SpacePreview {
                    name,
                    existing,
                    new_pins: Vec::new(),
                    favorites_as_pins: 0,
                    kept_pins: 0,
                    folders: Vec::new(),
                });
                preview.spaces.len() - 1
            }
        };
        slots.push(slot);
        // Every group (including nested ones) names a folder, even an empty one.
        for group in space.groups.iter().filter(|g| !is_favorites(g)) {
            let mut path = Vec::new();
            register_group_tree(&mut preview.spaces[slot], group, &mut path, workspace);
        }
    }
    let mut present: Vec<Present> = preview
        .spaces
        .iter()
        .map(|space| {
            let mut present = Present::default();
            let Some(id) = space.existing else {
                return present;
            };
            for folder in workspace.folders().iter().filter(|f| f.space_id == id) {
                let counts = present.folders.entry(folder.id).or_default();
                for item in state.items.iter().filter(|i| folder.items.contains(&i.id)) {
                    if let Some(url) = item.canonical_url.as_deref() {
                        *counts.entry(url).or_default() += 1;
                    }
                }
            }
            for item in state.items.iter() {
                if item.placement == (Placement::Pin { space_id: id })
                    && workspace.folder_of(item.id).is_none()
                {
                    if let Some(url) = item.canonical_url.as_deref() {
                        present.ungrouped.entry(url).or_default().push(item.id);
                    }
                }
            }
            present
        })
        .collect();
    // The Profile's Favorites, by URL, each answering for one entry.
    let mut favorites: BTreeMap<&str, usize> = BTreeMap::new();
    for item in &state.items {
        if item.placement
            == (Placement::Favorite {
                profile_id: state.profile_id,
            })
        {
            if let Some(url) = item.canonical_url.as_deref() {
                *favorites.entry(url).or_default() += 1;
            }
        }
    }
    for ((role, entry), url) in entries(report).into_iter().zip(canonical) {
        let Some(url) = url else {
            preview.skipped.push(Skipped {
                line: entry.line,
                url: entry.url.clone(),
            });
            continue;
        };
        match role {
            Role::Favorite => match favorites.get_mut(url.as_str()).filter(|count| **count > 0) {
                Some(count) => {
                    *count -= 1;
                    preview.kept_favorites += 1;
                }
                None => preview.new_favorites.push(NewItem::new(url, entry)),
            },
            Role::Pin(index) | Role::FavoritesGroup(index) => {
                let from_favorites_group = matches!(role, Role::FavoritesGroup(_));
                let slot = slots[index];
                let space = &mut preview.spaces[slot];
                let present = &mut present[slot];
                let ungrouped = present
                    .ungrouped
                    .get_mut(url.as_str())
                    .filter(|ids| !ids.is_empty());
                match ungrouped {
                    Some(ids) => {
                        ids.remove(0);
                        space.kept_pins += 1;
                    }
                    None => {
                        space.new_pins.push(NewItem::new(url, entry));
                        space.favorites_as_pins += usize::from(from_favorites_group);
                    }
                }
            }
            Role::Grouped(index, path) => {
                let slot = slots[index];
                let space = &mut preview.spaces[slot];
                let present = &mut present[slot];
                let ungrouped = present
                    .ungrouped
                    .get_mut(url.as_str())
                    .filter(|ids| !ids.is_empty());
                let folder = folder_at_path(space, &path, workspace);
                let held = folder
                    .existing
                    .and_then(|id| present.folders.get_mut(&id))
                    .and_then(|counts| counts.get_mut(url.as_str()))
                    .filter(|count| **count > 0);
                if let Some(count) = held {
                    *count -= 1;
                    space.kept_pins += 1;
                } else if let Some(ids) = ungrouped {
                    let id = ids.remove(0);
                    folder.additions.push(FolderAddition::Move(id));
                } else {
                    folder
                        .additions
                        .push(FolderAddition::New(NewItem::new(url, entry)));
                }
            }
        }
    }
    Ok(preview)
}

fn register_group_tree<'a>(
    space: &mut SpacePreview,
    group: &'a Group,
    path: &mut Vec<&'a str>,
    workspace: &Workspace,
) {
    path.push(group.label.as_str());
    let _ = folder_at_path(space, path, workspace);
    for child in &group.children {
        register_group_tree(space, child, path, workspace);
    }
    path.pop();
}

/// Ensures the folder chain described by `path` exists in `space` and returns
/// a mutable reference to the leaf `FolderPreview`.
fn folder_at_path<'a>(
    space: &'a mut SpacePreview,
    path: &[&str],
    workspace: &Workspace,
) -> &'a mut FolderPreview {
    let space_id = space.existing;
    let mut current_list = &mut space.folders;
    let mut parent_existing: Option<FolderId> = None;
    let last_idx = path.len() - 1;
    for (i, &label) in path.iter().enumerate() {
        let name = bounded_name(label, MAX_FOLDER_NAME_BYTES);
        let slot = match current_list.iter().position(|f| f.name == name) {
            Some(slot) => slot,
            None => {
                let existing = match (space_id, i == 0 || parent_existing.is_some()) {
                    (Some(sid), true) => workspace
                        .folders()
                        .iter()
                        .find(|f| {
                            f.space_id == sid && f.parent_id == parent_existing && f.name == name
                        })
                        .map(|f| f.id),
                    _ => None,
                };
                current_list.push(FolderPreview {
                    name,
                    existing,
                    additions: Vec::new(),
                    children: Vec::new(),
                });
                current_list.len() - 1
            }
        };
        if i == last_idx {
            return &mut current_list[slot];
        }
        parent_existing = current_list[slot].existing;
        current_list = &mut current_list[slot].children;
    }
    unreachable!("folder path is non-empty")
}

/// Applies `plan`. Stops at the first refused step, leaving earlier steps
/// applied. With `first`, new Spaces go through `create_first_space`.
fn write(plan: &Preview, workspace: &mut Workspace, first: bool) -> Result<(), ImportError> {
    let profile_id = workspace.snapshot().profile_id;
    // Spaces first: an untouched workspace stays untouched until the first
    // new Space can take its default Space's place.
    for space in &plan.spaces {
        let id = match space.existing {
            Some(id) => id,
            None if first => workspace.create_first_space(profile_id, &space.name, None, None)?,
            None => workspace.create_space(profile_id, &space.name)?,
        };
        for item in &space.new_pins {
            add_pin(workspace, id, item)?;
        }
        for folder in &space.folders {
            write_folder_tree(workspace, id, None, folder)?;
        }
    }
    if !plan.new_favorites.is_empty() {
        // A new item starts in a Space before it becomes a Favorite. A Profile
        // always keeps one; without it `add_temporary_item` refuses.
        let host = workspace
            .snapshot()
            .spaces
            .iter()
            .find(|s| s.profile_id == profile_id)
            .map_or(DEFAULT_SPACE_ID, |s| s.id);
        for new in &plan.new_favorites {
            let item = workspace.add_temporary_item(host)?;
            save(workspace, item, Placement::Favorite { profile_id }, new)?;
        }
    }
    Ok(())
}

fn write_folder_tree(
    workspace: &mut Workspace,
    space_id: SpaceId,
    parent_id: Option<FolderId>,
    folder: &FolderPreview,
) -> Result<(), ImportError> {
    let folder_id = match folder.existing {
        Some(folder_id) => folder_id,
        None => workspace.create_folder_in(space_id, parent_id, &folder.name)?,
    };
    for addition in &folder.additions {
        let item = match addition {
            FolderAddition::New(item) => add_pin(workspace, space_id, item)?,
            FolderAddition::Move(item) => *item,
        };
        workspace.move_pin_to_folder(item, Some(folder_id), None)?;
    }
    for child in &folder.children {
        write_folder_tree(workspace, space_id, Some(folder_id), child)?;
    }
    Ok(())
}

fn add_pin(
    workspace: &mut Workspace,
    space: SpaceId,
    new: &NewItem,
) -> Result<ItemId, ImportError> {
    let item = workspace.add_temporary_item(space)?;
    save(workspace, item, Placement::Pin { space_id: space }, new)?;
    Ok(item)
}

/// Saves temporary `item` at `placement` with `new`'s URL, name and title.
fn save(
    workspace: &mut Workspace,
    item: ItemId,
    placement: Placement,
    new: &NewItem,
) -> Result<(), ImportError> {
    workspace.set_placement(item, placement, Some(&new.url))?;
    workspace.rename_item(item, &new.name)?;
    workspace.set_title(item, &new.title)?;
    Ok(())
}

/// The label trimmed and cut to `max` bytes on a character boundary. An empty
/// result is left for the workspace to refuse.
fn bounded_name(label: &str, max: usize) -> String {
    let label = label.trim();
    let mut end = label.len().min(max);
    while !label.is_char_boundary(end) {
        end -= 1;
    }
    label[..end].trim_end().to_owned()
}
