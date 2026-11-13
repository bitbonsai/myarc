//! Maps an Arc Markdown [`Report`] into a myarc [`Workspace`].
//!
//! Each `#` Space becomes a Space with that name and every URL under it becomes a
//! Pin of that Space, in document order. Groups flatten: the workspace has no
//! folders yet, so a group's label is not kept. `Favorites` groups become Pins
//! too, because the export does not say whether they are Profile favorites or
//! Space pins (see `tests/fixtures/arc-markdown/README.md`).
//!
//! Importing is idempotent. An export Space reuses an existing ordinary Space of
//! the same name, and a URL that Space already pins as often as the export lists
//! it is kept rather than added again. Every import is rehearsed on a copy
//! first, so a failed import changes nothing. Nothing here opens tabs or fetches URLs.
//!
//! The browser canonicalizes URLs before they reach the workspace, so callers
//! pass one canonical URL per entry, in [`entry_urls`] order, or `None` for an
//! entry the browser rejected. [`as_written`] keeps HTTP(S) URLs unchanged, for
//! tests and tools without a URL parser.

use std::collections::BTreeMap;
use std::fmt;

use myarc_workspace::{MAX_SPACE_NAME_BYTES, Placement, SpaceId, Workspace, WorkspaceError};

use crate::arc_markdown::{Report, Scheme};

/// What an import would change, computed against the current workspace.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Preview {
    /// One per distinct Space name, in the order the export first lists it.
    pub spaces: Vec<SpacePreview>,
    /// Entries that will not be imported.
    pub skipped: Vec<Skipped>,
    /// Groups whose label is dropped because the workspace has no folders.
    pub flattened_groups: usize,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SpacePreview {
    pub name: String,
    /// The Space this name reuses, or `None` when the import creates it.
    pub existing: Option<SpaceId>,
    /// Canonical URLs the import adds as Pins, in order.
    pub new_pins: Vec<String>,
    /// Entries the Space already pins, so the import leaves them alone.
    pub kept_pins: usize,
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

    pub fn new_pin_count(&self) -> usize {
        self.spaces.iter().map(|s| s.new_pins.len()).sum()
    }

    pub fn kept_pin_count(&self) -> usize {
        self.spaces.iter().map(|s| s.kept_pins).sum()
    }

    /// True when applying would change nothing.
    pub fn is_noop(&self) -> bool {
        self.new_space_count() == 0 && self.new_pin_count() == 0
    }
}

/// Every entry's URL as written, in document order.
pub fn entry_urls(report: &Report) -> Vec<&str> {
    entries(report)
        .map(|(_, entry)| entry.url.as_str())
        .collect()
}

/// HTTP(S) URLs as written and `None` for other schemes.
pub fn as_written(report: &Report) -> Vec<Option<String>> {
    entries(report)
        .map(|(_, entry)| {
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

fn entries(report: &Report) -> impl Iterator<Item = (usize, &crate::arc_markdown::Entry)> {
    report.spaces.iter().enumerate().flat_map(|(index, space)| {
        space
            .groups
            .iter()
            .flat_map(move |group| group.entries.iter().map(move |entry| (index, entry)))
    })
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
    let mut preview = Preview {
        flattened_groups: report.group_count(),
        ..Preview::default()
    };
    // Export Space index -> preview slot, merging Spaces that share a name.
    let mut slots = Vec::with_capacity(report.spaces.len());
    for space in &report.spaces {
        let name = space_name(&space.label);
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
                });
                preview.spaces.len() - 1
            }
        };
        slots.push(slot);
    }
    // Pins each reused Space already has, counted per URL.
    let mut present: Vec<BTreeMap<&str, usize>> = preview
        .spaces
        .iter()
        .map(|space| {
            let mut counts = BTreeMap::new();
            if let Some(id) = space.existing {
                for item in state.items.iter() {
                    if item.placement == (Placement::Pin { space_id: id }) {
                        if let Some(url) = item.canonical_url.as_deref() {
                            *counts.entry(url).or_default() += 1;
                        }
                    }
                }
            }
            counts
        })
        .collect();
    for ((index, entry), url) in entries(report).zip(canonical) {
        let Some(url) = url else {
            preview.skipped.push(Skipped {
                line: entry.line,
                url: entry.url.clone(),
            });
            continue;
        };
        let slot = slots[index];
        match present[slot].get_mut(url.as_str()) {
            Some(count) if *count > 0 => {
                *count -= 1;
                preview.spaces[slot].kept_pins += 1;
            }
            _ => preview.spaces[slot].new_pins.push(url.clone()),
        }
    }
    Ok(preview)
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
            let item = workspace.add_temporary_item(id)?;
            workspace.set_placement(item, Placement::Pin { space_id: id }, Some(url))?;
        }
    }
    Ok(())
}

/// The label trimmed and cut to the workspace's name limit on a character boundary.
fn space_name(label: &str) -> String {
    let label = label.trim();
    let mut end = label.len().min(MAX_SPACE_NAME_BYTES);
    while !label.is_char_boundary(end) {
        end -= 1;
    }
    label[..end].trim_end().to_owned()
}
