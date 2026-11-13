//! Maps an Arc Markdown [`Report`] into a myarc [`Workspace`].
//!
//! Each `#` Space becomes a Space with that name. Each `##` group becomes a
//! folder of that Space's Pins with the group's name, and its URLs become Pins
//! in that folder, in document order. A `Favorites` group is the exception:
//! its URLs become ungrouped Pins, which the sidebar shows as tiles, because
//! Arc's Favorites are tiles rather than a folder. They stay Space Pins rather
//! than Profile favorites because the export does not say which they are (see
//! `tests/fixtures/arc-markdown/README.md`). Folders don't nest, and neither
//! do export groups.
//!
//! Importing is idempotent. An export Space reuses an existing ordinary Space
//! of the same name, and a group reuses the first folder of that Space with
//! the same name. A URL the folder already holds is kept. A URL of a group
//! that the Space pins outside any folder moves into the folder rather than
//! being added again, which tidies up an import made before folders existed.
//! A `Favorites` URL the Space already pins outside any folder is kept. Each
//! existing Pin answers for one entry, so repeated URLs stay repeated. Every
//! import is rehearsed on a copy first, so a failed import changes nothing.
//! Nothing here opens tabs or fetches URLs.
//!
//! The browser canonicalizes URLs before they reach the workspace, so callers
//! pass one canonical URL per entry, in [`entry_urls`] order, or `None` for an
//! entry the browser rejected. [`as_written`] keeps HTTP(S) URLs unchanged, for
//! tests and tools without a URL parser.

use std::collections::BTreeMap;
use std::fmt;

use myarc_workspace::{
    FolderId, ItemId, MAX_FOLDER_NAME_BYTES, MAX_SPACE_NAME_BYTES, Placement, SpaceId, Workspace,
    WorkspaceError,
};

use crate::arc_markdown::{Report, Scheme};

/// The group label Arc gives a Space's Favorites. Its URLs become ungrouped Pins.
pub const FAVORITES_LABEL: &str = "Favorites";

/// What an import would change, computed against the current workspace.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Preview {
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
    /// Canonical URLs of `Favorites` groups the import adds as ungrouped Pins,
    /// in order.
    pub new_pins: Vec<String>,
    /// Entries the Space already pins in the right place, so the import leaves
    /// them alone. Covers folders too.
    pub kept_pins: usize,
    /// One per distinct group name other than `Favorites`, in export order.
    pub folders: Vec<FolderPreview>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FolderPreview {
    pub name: String,
    /// The folder this name reuses, or `None` when the import creates it.
    pub existing: Option<FolderId>,
    /// What the folder gains at its end, in export order.
    pub additions: Vec<FolderAddition>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum FolderAddition {
    /// A new Pin with this canonical URL.
    New(String),
    /// An ungrouped Pin of the Space with the entry's URL, moved in.
    Move(ItemId),
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
        self.folders().filter(|f| f.existing.is_none()).count()
    }

    /// New Pins, ungrouped and in folders.
    pub fn new_pin_count(&self) -> usize {
        let ungrouped: usize = self.spaces.iter().map(|s| s.new_pins.len()).sum();
        let foldered = self
            .folders()
            .flat_map(|f| &f.additions)
            .filter(|a| matches!(a, FolderAddition::New(_)))
            .count();
        ungrouped + foldered
    }

    /// Ungrouped Pins the import moves into a folder.
    pub fn moved_pin_count(&self) -> usize {
        self.folders()
            .flat_map(|f| &f.additions)
            .filter(|a| matches!(a, FolderAddition::Move(_)))
            .count()
    }

    fn folders(&self) -> impl Iterator<Item = &FolderPreview> {
        self.spaces.iter().flat_map(|s| &s.folders)
    }

    pub fn kept_pin_count(&self) -> usize {
        self.spaces.iter().map(|s| s.kept_pins).sum()
    }

    /// True when applying would change nothing.
    pub fn is_noop(&self) -> bool {
        self.new_space_count() == 0
            && self.new_folder_count() == 0
            && self.new_pin_count() == 0
            && self.moved_pin_count() == 0
    }
}

/// Every entry's URL as written, in document order.
pub fn entry_urls(report: &Report) -> Vec<&str> {
    entries(report)
        .map(|(_, _, entry)| entry.url.as_str())
        .collect()
}

/// HTTP(S) URLs as written and `None` for other schemes.
pub fn as_written(report: &Report) -> Vec<Option<String>> {
    entries(report)
        .map(|(_, _, entry)| {
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
    write(&plan, &mut copy)?;
    Ok(plan)
}

/// Imports `report` into `workspace`, all or nothing. Returns what changed.
pub fn apply(
    report: &Report,
    canonical: &[Option<String>],
    workspace: &mut Workspace,
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
    write(&plan, &mut copy)?;
    if let Err(error) = write(&plan, workspace) {
        workspace.restore(before)?;
        return Err(error);
    }
    Ok(plan)
}

/// Every entry with its export Space index and whether its group is `Favorites`.
fn entries(
    report: &Report,
) -> impl Iterator<
    Item = (
        usize,
        &crate::arc_markdown::Group,
        &crate::arc_markdown::Entry,
    ),
> {
    report.spaces.iter().enumerate().flat_map(|(index, space)| {
        space
            .groups
            .iter()
            .flat_map(move |group| group.entries.iter().map(move |entry| (index, group, entry)))
    })
}

fn is_favorites(group: &crate::arc_markdown::Group) -> bool {
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
                    kept_pins: 0,
                    folders: Vec::new(),
                });
                preview.spaces.len() - 1
            }
        };
        slots.push(slot);
        // Every group names a folder, even an empty one.
        for group in space.groups.iter().filter(|g| !is_favorites(g)) {
            folder_slot(&mut preview.spaces[slot], &group.label, workspace);
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
    for ((index, group, entry), url) in entries(report).zip(canonical) {
        let Some(url) = url else {
            preview.skipped.push(Skipped {
                line: entry.line,
                url: entry.url.clone(),
            });
            continue;
        };
        let slot = slots[index];
        let space = &mut preview.spaces[slot];
        let present = &mut present[slot];
        let ungrouped = present
            .ungrouped
            .get_mut(url.as_str())
            .filter(|ids| !ids.is_empty());
        if is_favorites(group) {
            match ungrouped {
                Some(ids) => {
                    ids.remove(0);
                    space.kept_pins += 1;
                }
                None => space.new_pins.push(url.clone()),
            }
            continue;
        }
        let folder = folder_slot(space, &group.label, workspace);
        let held = space.folders[folder]
            .existing
            .and_then(|id| present.folders.get_mut(&id))
            .and_then(|counts| counts.get_mut(url.as_str()))
            .filter(|count| **count > 0);
        if let Some(count) = held {
            *count -= 1;
            space.kept_pins += 1;
        } else if let Some(ids) = ungrouped {
            let id = ids.remove(0);
            space.folders[folder]
                .additions
                .push(FolderAddition::Move(id));
        } else {
            space.folders[folder]
                .additions
                .push(FolderAddition::New(url.clone()));
        }
    }
    Ok(preview)
}

/// The preview slot of the folder `label` names in `space`, added on first use.
fn folder_slot(space: &mut SpacePreview, label: &str, workspace: &Workspace) -> usize {
    let name = bounded_name(label, MAX_FOLDER_NAME_BYTES);
    if let Some(slot) = space.folders.iter().position(|f| f.name == name) {
        return slot;
    }
    let existing = space.existing.and_then(|id| {
        workspace
            .folders()
            .iter()
            .find(|f| f.space_id == id && f.name == name)
            .map(|f| f.id)
    });
    space.folders.push(FolderPreview {
        name,
        existing,
        additions: Vec::new(),
    });
    space.folders.len() - 1
}

/// Applies `plan`. Stops at the first refused step, leaving earlier steps applied.
fn write(plan: &Preview, workspace: &mut Workspace) -> Result<(), ImportError> {
    let profile_id = workspace.snapshot().profile_id;
    for space in &plan.spaces {
        let id = match space.existing {
            Some(id) => id,
            None => workspace.create_space(profile_id, &space.name)?,
        };
        for url in &space.new_pins {
            add_pin(workspace, id, url)?;
        }
        for folder in &space.folders {
            let folder_id = match folder.existing {
                Some(folder_id) => folder_id,
                None => workspace.create_folder(id, &folder.name)?,
            };
            for addition in &folder.additions {
                let item = match addition {
                    FolderAddition::New(url) => add_pin(workspace, id, url)?,
                    FolderAddition::Move(item) => *item,
                };
                workspace.move_pin_to_folder(item, Some(folder_id), None)?;
            }
        }
    }
    Ok(())
}

fn add_pin(workspace: &mut Workspace, space: SpaceId, url: &str) -> Result<ItemId, ImportError> {
    let item = workspace.add_temporary_item(space)?;
    workspace.set_placement(item, Placement::Pin { space_id: space }, Some(url))?;
    Ok(item)
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
