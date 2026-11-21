//! Arc's sidebar file, `~/Library/Application Support/Arc/StorableSidebar.json`.
//!
//! Unlike the Markdown export, this file marks Favorites apart from Space pins
//! and says which Arc profile each belongs to, so [`parse_profiles`] returns
//! one [`Report`] per Arc profile with [`Report::favorites`] and
//! [`Space::pins`] filled. The shape below follows Firefox's Arc importer
//! (`ArcMigrationUtils.sys.mjs`) and is documented with a synthetic fixture in
//! `tests/fixtures/arc-sidebar/README.md`:
//!
//! - `sidebar.containers` alternates a kind marker and a container. The
//!   `{"global": {}}` container holds the main sidebar. A `{"littleBrowser": {}}`
//!   container holds Little Arc tabs, counted as Today tabs.
//! - A container's `items` holds every sidebar item as an object with an `id`,
//!   a `parentID`, `childrenIds`, a `title` and `data`. `data.tab.savedURL` is a
//!   tab, `data.list` a folder and `data.splitView` a Split View. An item's
//!   `title` is the name the user gave it; `data.tab.savedTitle` is the page
//!   title Arc saved.
//! - `topAppsContainerIDs` alternates an Arc profile marker and the id of the
//!   item holding that profile's Favorites. `{"default": {}}` is the main
//!   profile and `{"custom": {"_0": {"directoryBasename": "Profile 1"}}}`
//!   another.
//! - Each of `spaces` has a `title`, a `profile` marker and `containerIDs`,
//!   which alternates `"pinned"` or `"unpinned"` and an item id.
//!
//! Each Arc profile's Favorites become its report's [`Report::favorites`].
//! Each Space goes to its profile's report: its pinned tabs become
//! [`Space::pins`] and each pinned folder becomes a group, with folders inside
//! it flattened into it, because myarc's folders don't nest. Split Views are
//! flattened into their parent. Each tab keeps its name and saved page title.
//! Unpinned tabs, nested folders and anything of an unknown kind are counted
//! in the main profile's [`Report::omitted`]. Saved URLs that aren't
//! `scheme:rest` become [`Report::unrecognized`] with line 0. Icons and colors
//! are not read. Nothing is fetched and the file is only read.

use std::collections::{BTreeMap, BTreeSet};
use std::fmt;

use serde_json::Value;

use crate::arc_markdown::{
    Entry, Group, MAX_EXCERPT_BYTES, Omission, Report, Space, Unrecognized, scheme,
};

/// Largest sidebar file accepted, in bytes.
pub const MAX_INPUT_BYTES: usize = 32 * 1024 * 1024;
/// Deepest folder nesting followed. Deeper items are counted as unknown.
pub const MAX_DEPTH: usize = 32;
/// Most Arc profiles read, the main one included. Favorites and Spaces of
/// further profiles are counted as unknown.
pub const MAX_PROFILES: usize = 16;
/// Longest Arc profile directory name kept, in bytes.
pub const MAX_PROFILE_NAME_BYTES: usize = 64;

/// One Arc profile, from its marker.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum ArcProfile {
    /// `{"default": {}}`, Arc's main profile.
    Main,
    /// `{"custom": {"_0": {"directoryBasename": "Profile 1"}}}`, by its
    /// directory name.
    Custom(String),
}

impl ArcProfile {
    /// The name of the myArc Profile this Arc profile imports into, or `None`
    /// for the main profile, which imports into the Profile that runs the
    /// import. A re-import finds the Profile by this name.
    pub fn myarc_name(&self) -> Option<String> {
        match self {
            Self::Main => None,
            Self::Custom(directory) => Some(format!("Arc {directory}")),
        }
    }
}

/// One Arc profile's part of the sidebar file.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ProfileReport {
    pub profile: ArcProfile,
    pub report: Report,
}

#[derive(Debug, PartialEq, Eq)]
pub enum SidebarError {
    /// Input exceeds [`MAX_INPUT_BYTES`]. Nothing was parsed.
    TooLarge { len: usize, max: usize },
    /// Input is not JSON, or nests deeper than the JSON reader allows.
    NotJson(String),
    /// JSON without a `sidebar.containers` list holding a `global` container.
    NoSidebar,
}

impl fmt::Display for SidebarError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::TooLarge { len, max } => {
                write!(f, "sidebar file is {len} bytes; the limit is {max} bytes")
            }
            Self::NotJson(error) => write!(f, "sidebar file is not JSON: {error}"),
            Self::NoSidebar => write!(f, "file has no Arc sidebar"),
        }
    }
}

impl std::error::Error for SidebarError {}

/// Reads the main Arc profile's part of the sidebar file. Other profiles'
/// Favorites and Spaces are left out; [`parse_profiles`] returns them.
pub fn parse(input: &[u8]) -> Result<Report, SidebarError> {
    Ok(parse_profiles(input)?.swap_remove(0).report)
}

/// Reads Arc's sidebar file into one report per Arc profile: the main profile
/// first, always, then the others with any Favorite or Space, in the order the
/// file first names them. File-wide counts go into the main profile's report.
/// Fails only when the input is too large, isn't JSON or has no main sidebar;
/// anything else it can't place is counted.
pub fn parse_profiles(input: &[u8]) -> Result<Vec<ProfileReport>, SidebarError> {
    if input.len() > MAX_INPUT_BYTES {
        return Err(SidebarError::TooLarge {
            len: input.len(),
            max: MAX_INPUT_BYTES,
        });
    }
    let root: Value =
        serde_json::from_slice(input).map_err(|e| SidebarError::NotJson(e.to_string()))?;
    let containers = root
        .pointer("/sidebar/containers")
        .and_then(Value::as_array)
        .ok_or(SidebarError::NoSidebar)?;
    let mut profiles = vec![ProfileReport {
        profile: ArcProfile::Main,
        report: Report::default(),
    }];
    let mut found = false;
    for pair in containers.chunks_exact(2) {
        let (kind, container) = (&pair[0], &pair[1]);
        if kind.get("global").is_some() {
            found = true;
            Reader::new(container).global(container, &mut profiles);
        } else if kind.get("littleBrowser").is_some() {
            Reader::new(container).little_arc(&mut profiles[0].report);
        } else {
            omit(&mut profiles[0].report, Omission::UnknownContainer, 1);
        }
    }
    if !found {
        return Err(SidebarError::NoSidebar);
    }
    // A profile met only through an empty Favorites container adds nothing.
    let main = profiles.remove(0);
    profiles.retain(|p| !p.report.favorites.is_empty() || !p.report.spaces.is_empty());
    profiles.insert(0, main);
    Ok(profiles)
}

/// The report of `profile`, added on first use. Past [`MAX_PROFILES`], `None`.
fn report_of(profiles: &mut Vec<ProfileReport>, profile: ArcProfile) -> Option<&mut Report> {
    let index = match profiles.iter().position(|p| p.profile == profile) {
        Some(index) => index,
        None if profiles.len() < MAX_PROFILES => {
            profiles.push(ProfileReport {
                profile,
                report: Report::default(),
            });
            profiles.len() - 1
        }
        None => return None,
    };
    Some(&mut profiles[index].report)
}

fn omit(report: &mut Report, omission: Omission, count: usize) {
    if count > 0 {
        *report.omitted.entry(omission).or_default() += count;
    }
}

/// What an item is, from its `data`.
enum ItemKind<'a> {
    Tab(&'a Value),
    Folder,
    Split,
    Unknown,
}

fn kind(item: &Value) -> ItemKind<'_> {
    let data = item.get("data");
    if let Some(tab) = data.and_then(|d| d.get("tab")) {
        ItemKind::Tab(tab)
    } else if data.and_then(|d| d.get("list")).is_some() {
        ItemKind::Folder
    } else if data.and_then(|d| d.get("splitView")).is_some() {
        ItemKind::Split
    } else {
        ItemKind::Unknown
    }
}

/// The items of one container, plus what reading them left out.
struct Reader<'a> {
    items: BTreeMap<&'a str, &'a Value>,
    /// Item ids in file order, for children found by `parentID`.
    order: Vec<&'a str>,
    /// Omissions and unreadable URLs met so far. They join the main
    /// profile's report.
    counts: Report,
}

/// Where the tabs of a walk go.
#[derive(Default)]
struct Walk {
    tabs: Vec<Entry>,
    /// Folders directly under the walk's root, each with its tabs.
    folders: Vec<Group>,
}

impl<'a> Reader<'a> {
    fn new(container: &'a Value) -> Self {
        let mut items = BTreeMap::new();
        let mut order = Vec::new();
        // Arc writes `[id, item, id, item]`; take every object with an id.
        for item in container
            .get("items")
            .and_then(Value::as_array)
            .into_iter()
            .flatten()
        {
            if let Some(id) = item.get("id").and_then(Value::as_str) {
                if items.insert(id, item).is_none() {
                    order.push(id);
                }
            }
        }
        Self {
            items,
            order,
            counts: Report::default(),
        }
    }

    /// Moves the counts into the main profile's report.
    fn finish(self, main: &mut Report) {
        for (omission, count) in self.counts.omitted {
            omit(main, omission, count);
        }
        main.unrecognized.extend(self.counts.unrecognized);
    }

    fn global(mut self, container: &'a Value, profiles: &mut Vec<ProfileReport>) {
        for (marker, id) in pairs(container.get("topAppsContainerIDs")) {
            let Some(id) = id.as_str() else { continue };
            let walk = self.walk(id, false);
            let tabs = walk.tabs.len();
            match report_of(profiles, profile(marker)) {
                Some(report) => report.favorites.extend(walk.tabs),
                None => omit(&mut self.counts, Omission::UnknownContainer, tabs),
            }
        }
        let spaces = container
            .get("spaces")
            .and_then(Value::as_array)
            .into_iter()
            .flatten()
            .filter(|space| space.get("containerIDs").is_some());
        for space in spaces {
            let mut entry = Space {
                label: String::new(),
                line: 0,
                pins: Vec::new(),
                groups: Vec::new(),
            };
            for (key, id) in pairs(space.get("containerIDs")) {
                let Some(id) = id.as_str() else { continue };
                match key.as_str() {
                    Some("pinned") => {
                        let walk = self.walk(id, true);
                        entry.pins.extend(walk.tabs);
                        entry.groups.extend(walk.folders);
                    }
                    Some("unpinned") => {
                        let tabs = self.walk(id, false).tabs.len();
                        omit(&mut self.counts, Omission::TodayTab, tabs);
                    }
                    _ => omit(&mut self.counts, Omission::UnknownContainer, 1),
                }
            }
            let marker = space.get("profile").unwrap_or(&Value::Null);
            match report_of(profiles, profile(marker)) {
                Some(report) => {
                    let number = report.spaces.len() + 1;
                    entry.label = clean(space.get("title").and_then(Value::as_str))
                        .unwrap_or_else(|| format!("Space {number}"));
                    report.spaces.push(entry);
                }
                None => omit(&mut self.counts, Omission::UnknownContainer, 1),
            }
        }
        self.finish(&mut profiles[0].report);
    }

    fn little_arc(mut self, main: &mut Report) {
        let tabs = self
            .items
            .values()
            .filter(|item| matches!(kind(item), ItemKind::Tab(_)))
            .count();
        omit(&mut self.counts, Omission::TodayTab, tabs);
        self.finish(main);
    }

    /// The tabs under item `root`, in order. With `folders`, folders directly
    /// under `root` become groups; otherwise their tabs join the list.
    fn walk(&mut self, root: &str, folders: bool) -> Walk {
        let mut walk = Walk::default();
        let mut visited = BTreeSet::from([root.to_owned()]);
        for child in self.children(root) {
            let id = child.get("id").and_then(Value::as_str).unwrap_or_default();
            if !visited.insert(id.to_owned()) {
                continue;
            }
            if folders && matches!(kind(child), ItemKind::Folder) {
                let mut entries = Vec::new();
                self.collect(id, 1, &mut visited, &mut entries);
                let label = clean(child.get("title").and_then(Value::as_str))
                    .unwrap_or_else(|| "Untitled folder".to_owned());
                walk.folders.push(Group {
                    label,
                    line: 0,
                    entries,
                });
            } else {
                self.place(child, 0, &mut visited, &mut walk.tabs);
            }
        }
        walk
    }

    /// Adds the tabs under item `id` to `out`.
    fn collect(
        &mut self,
        id: &str,
        depth: usize,
        visited: &mut BTreeSet<String>,
        out: &mut Vec<Entry>,
    ) {
        for child in self.children(id) {
            let child_id = child.get("id").and_then(Value::as_str).unwrap_or_default();
            if visited.insert(child_id.to_owned()) {
                self.place(child, depth, visited, out);
            }
        }
    }

    /// Adds `item`, or the tabs inside it, to `out`. `depth` counts the
    /// folders above it.
    fn place(
        &mut self,
        item: &'a Value,
        depth: usize,
        visited: &mut BTreeSet<String>,
        out: &mut Vec<Entry>,
    ) {
        let id = item.get("id").and_then(Value::as_str).unwrap_or_default();
        match kind(item) {
            ItemKind::Tab(tab) => self.tab(item, tab, out),
            _ if depth >= MAX_DEPTH => omit(&mut self.counts, Omission::UnknownContainer, 1),
            ItemKind::Folder => {
                if depth > 0 {
                    omit(&mut self.counts, Omission::NestedFolder, 1);
                }
                self.collect(id, depth + 1, visited, out);
            }
            ItemKind::Split => self.collect(id, depth, visited, out),
            ItemKind::Unknown => omit(&mut self.counts, Omission::UnknownContainer, 1),
        }
    }

    fn tab(&mut self, item: &Value, tab: &Value, out: &mut Vec<Entry>) {
        let url = tab.get("savedURL").and_then(Value::as_str).unwrap_or("");
        match scheme(url) {
            Ok(scheme) => out.push(Entry {
                url: url.to_owned(),
                scheme,
                line: 0,
                name: clean(item.get("title").and_then(Value::as_str)).unwrap_or_default(),
                title: clean(tab.get("savedTitle").and_then(Value::as_str)).unwrap_or_default(),
            }),
            Err(reason) => self.counts.unrecognized.push(Unrecognized {
                line: 0,
                reason,
                excerpt: excerpt(url),
            }),
        }
    }

    /// The children of item `id`: its `childrenIds` in order, or else the
    /// items naming it as `parentID`, in file order.
    fn children(&self, id: &str) -> Vec<&'a Value> {
        let listed: Vec<&'a Value> = self
            .items
            .get(id)
            .and_then(|item| item.get("childrenIds"))
            .and_then(Value::as_array)
            .into_iter()
            .flatten()
            .filter_map(Value::as_str)
            .filter_map(|child| self.items.get(child).copied())
            .collect();
        if !listed.is_empty() {
            return listed;
        }
        self.order
            .iter()
            .map(|child| self.items[child])
            .filter(|item| item.get("parentID").and_then(Value::as_str) == Some(id))
            .collect()
    }
}

/// `[key, value, key, value]` as pairs. A trailing key is dropped.
fn pairs(list: Option<&Value>) -> impl Iterator<Item = (&Value, &Value)> {
    list.and_then(Value::as_array)
        .into_iter()
        .flat_map(|list| list.chunks_exact(2))
        .map(|pair| (&pair[0], &pair[1]))
}

/// Arc writes `{"default": {}}` for its main profile and
/// `{"custom": {"_0": {"directoryBasename": "Profile 1"}}}` for the others.
/// A missing or unknown marker means the main profile, as in files from before
/// Arc had profiles. A custom marker without a name gets "Unnamed".
fn profile(marker: &Value) -> ArcProfile {
    let Some(custom) = marker.get("custom") else {
        return ArcProfile::Main;
    };
    let name = clean(
        custom
            .pointer("/_0/directoryBasename")
            .and_then(Value::as_str),
    )
    .unwrap_or_else(|| "Unnamed".to_owned());
    let mut end = name.len().min(MAX_PROFILE_NAME_BYTES);
    while !name.is_char_boundary(end) {
        end -= 1;
    }
    ArcProfile::Custom(name[..end].trim_end().to_owned())
}

/// A title with control characters turned into spaces and trimmed, or `None`
/// when nothing is left.
fn clean(title: Option<&str>) -> Option<String> {
    let title: String = title?
        .chars()
        .map(|c| if c.is_control() { ' ' } else { c })
        .collect();
    let title = title.trim();
    (!title.is_empty()).then(|| title.to_owned())
}

fn excerpt(text: &str) -> String {
    let mut end = text.len().min(MAX_EXCERPT_BYTES);
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    text[..end].to_owned()
}
