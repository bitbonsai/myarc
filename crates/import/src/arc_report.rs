//! The report shown once an Arc import has committed. It names every part of
//! the export the import left out or changed, each with a count, so nothing is
//! dropped without the user being told. The preview before the commit lists
//! unreadable lines one by one; this report counts them by kind instead.

use std::collections::BTreeMap;

use myarc_workspace::{MAX_FOLDER_NAME_BYTES, MAX_SPACE_NAME_BYTES};

use crate::arc_markdown::{Omission, Reason, Report, Scheme};
use crate::arc_workspace::{FAVORITES_LABEL, Preview};

/// What Arc's export never carries, so no import can bring it over.
pub const NOT_IN_EXPORT: &str = "Arc's export has no page titles, Space icons, \
     colors or archived tabs, so none of those were imported.";

/// One kind of entry the import left out or changed.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Kind {
    /// Links whose scheme is not http or https, such as `file:`.
    NotWebLink,
    /// http(s) links the browser could not read.
    UnreadableLink,
    /// Lines the reader did not understand, by reason.
    UnreadableLine(Reason),
    /// Markdown `Favorites` entries added as pinned tabs, because that export
    /// does not say whether they are Profile Favorites or Space pins. Arc's
    /// sidebar file does, so its Favorites never count here.
    FavoriteAsPin,
    /// Parts of Arc's sidebar file left out on purpose.
    Omitted(Omission),
    /// Space names cut to the workspace's length limit.
    SpaceNameShortened,
    /// Folder names cut to the workspace's length limit.
    FolderNameShortened,
    /// Spaces whose name an earlier Space of the export already used, merged
    /// into it.
    RepeatedSpace,
    /// Groups whose name an earlier group of the same Space already used,
    /// merged into its folder.
    RepeatedFolder,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Line {
    pub kind: Kind,
    pub count: usize,
}

impl Line {
    /// "Links left out, not web pages: 2".
    pub fn text(&self) -> String {
        let name = match self.kind {
            Kind::NotWebLink => "Links left out, not web pages",
            Kind::UnreadableLink => "Links left out, unreadable",
            Kind::UnreadableLine(reason) => {
                return format!("Lines left out, {}: {}", reason.label(), self.count);
            }
            Kind::FavoriteAsPin => "Favorites added as pinned tabs",
            Kind::Omitted(Omission::TodayTab) => "Today tabs left out",
            Kind::Omitted(Omission::OtherProfileFavorite) => {
                "Favorites of other Arc profiles left out"
            }
            Kind::Omitted(Omission::NestedFolder) => {
                "Folders inside folders, their tabs moved to the outer folder"
            }
            Kind::Omitted(Omission::UnknownContainer) => "Unknown sidebar parts left out",
            Kind::SpaceNameShortened => "Space names shortened to fit",
            Kind::FolderNameShortened => "Folder names shortened to fit",
            Kind::RepeatedSpace => "Spaces merged into an earlier one with the same name",
            Kind::RepeatedFolder => "Folders merged into an earlier one with the same name",
        };
        format!("{name}: {}", self.count)
    }
}

/// Every kind the import of `report` left out or changed, in [`Kind`] order,
/// leaving out kinds with a count of zero. `canonical` is what the import was
/// given and `plan` is what it returned.
pub fn lines(report: &Report, canonical: &[Option<String>], plan: &Preview) -> Vec<Line> {
    let mut counts: BTreeMap<Kind, usize> = BTreeMap::new();
    let mut add = |kind, count| {
        if count > 0 {
            *counts.entry(kind).or_default() += count;
        }
    };
    for (entry, url) in report.entries().zip(canonical) {
        if url.is_none() {
            let kind = match entry.scheme {
                Scheme::Http | Scheme::Https => Kind::UnreadableLink,
                Scheme::Other(_) => Kind::NotWebLink,
            };
            add(kind, 1);
        }
    }
    for line in &report.unrecognized {
        add(Kind::UnreadableLine(line.reason), 1);
    }
    add(
        Kind::FavoriteAsPin,
        plan.spaces
            .iter()
            .map(|space| space.favorites_as_pins)
            .sum(),
    );
    for (&omission, &count) in &report.omitted {
        add(Kind::Omitted(omission), count);
    }
    let groups: Vec<_> = report
        .spaces
        .iter()
        .flat_map(|space| &space.groups)
        .filter(|group| group.label.trim() != FAVORITES_LABEL)
        .collect();
    add(
        Kind::SpaceNameShortened,
        report
            .spaces
            .iter()
            .filter(|space| space.label.trim().len() > MAX_SPACE_NAME_BYTES)
            .count(),
    );
    add(
        Kind::FolderNameShortened,
        groups
            .iter()
            .filter(|group| group.label.trim().len() > MAX_FOLDER_NAME_BYTES)
            .count(),
    );
    add(
        Kind::RepeatedSpace,
        report.spaces.len().saturating_sub(plan.spaces.len()),
    );
    let folders: usize = plan.spaces.iter().map(|space| space.folders.len()).sum();
    add(Kind::RepeatedFolder, groups.len().saturating_sub(folders));
    counts
        .into_iter()
        .map(|(kind, count)| Line { kind, count })
        .collect()
}
