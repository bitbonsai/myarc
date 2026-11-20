//! Arc's sidebar file, `~/Library/Application Support/Arc/StorableSidebar.json`.
//!
//! Unlike the Markdown export, this file marks Favorites apart from Space pins,
//! so [`parse`] fills [`Report::favorites`] and [`Space::pins`]. The shape below
//! follows Firefox's Arc importer (`ArcMigrationUtils.sys.mjs`) and is
//! documented with a synthetic fixture in `tests/fixtures/arc-sidebar/README.md`:
//!
//! - `sidebar.containers` alternates a kind marker and a container. The
//!   `{"global": {}}` container holds the main sidebar. A `{"littleBrowser": {}}`
//!   container holds Little Arc tabs, counted as Today tabs.
//! - A container's `items` holds every sidebar item as an object with an `id`,
//!   a `parentID`, `childrenIds`, a `title` and `data`. `data.tab.savedURL` is a
//!   tab, `data.list` a folder and `data.splitView` a Split View.
//! - `topAppsContainerIDs` alternates an Arc profile marker and the id of the
//!   item holding that profile's Favorites. `{"default": {}}` is the main
//!   profile.
//! - Each of `spaces` has a `title`, a `profile` marker and `containerIDs`,
//!   which alternates `"pinned"` or `"unpinned"` and an item id.
//!
//! The main profile's Favorites become [`Report::favorites`]. Each Space's
//! pinned tabs become [`Space::pins`] and each pinned folder becomes a group,
//! with folders inside it flattened into it. Split Views are flattened into
//! their parent. Unpinned tabs, other profiles' Favorites, nested folders and
//! anything of an unknown kind are counted in [`Report::omitted`]. Saved URLs
//! that aren't `scheme:rest` become [`Report::unrecognized`] with line 0.
//! Titles, icons and colors are not read. Nothing is fetched and the file is
//! only read.

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

/// Reads Arc's sidebar file into a report. Fails only when the input is too
/// large, isn't JSON or has no main sidebar; anything else it can't place is
/// counted in the report.
pub fn parse(input: &[u8]) -> Result<Report, SidebarError> {
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
    let mut report = Report::default();
    let mut found = false;
    for pair in containers.chunks_exact(2) {
        let (kind, container) = (&pair[0], &pair[1]);
        if kind.get("global").is_some() {
            found = true;
            Reader::new(container, &mut report).global(container);
        } else if kind.get("littleBrowser").is_some() {
            Reader::new(container, &mut report).little_arc();
        } else {
            omit(&mut report, Omission::UnknownContainer, 1);
        }
    }
    if found {
        Ok(report)
    } else {
        Err(SidebarError::NoSidebar)
    }
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

/// The items of one container and the report they go into.
struct Reader<'a, 'r> {
    items: BTreeMap<&'a str, &'a Value>,
    /// Item ids in file order, for children found by `parentID`.
    order: Vec<&'a str>,
    report: &'r mut Report,
}

/// Where the tabs of a walk go.
#[derive(Default)]
struct Walk {
    tabs: Vec<Entry>,
    /// Folders directly under the walk's root, each with its tabs.
    folders: Vec<Group>,
}

impl<'a, 'r> Reader<'a, 'r> {
    fn new(container: &'a Value, report: &'r mut Report) -> Self {
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
            report,
        }
    }

    fn global(&mut self, container: &'a Value) {
        for (marker, id) in pairs(container.get("topAppsContainerIDs")) {
            let Some(id) = id.as_str() else { continue };
            let walk = self.walk(id, false);
            let tabs = walk.tabs.len();
            if is_main_profile(marker) {
                self.report.favorites.extend(walk.tabs);
            } else {
                omit(self.report, Omission::OtherProfileFavorite, tabs);
            }
        }
        let spaces = container
            .get("spaces")
            .and_then(Value::as_array)
            .into_iter()
            .flatten()
            .filter(|space| space.get("containerIDs").is_some());
        for space in spaces {
            let number = self.report.spaces.len() + 1;
            let label = clean(space.get("title").and_then(Value::as_str))
                .unwrap_or_else(|| format!("Space {number}"));
            let mut entry = Space {
                label,
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
                        omit(self.report, Omission::TodayTab, tabs);
                    }
                    _ => omit(self.report, Omission::UnknownContainer, 1),
                }
            }
            self.report.spaces.push(entry);
        }
    }

    fn little_arc(&mut self) {
        let tabs = self
            .items
            .values()
            .filter(|item| matches!(kind(item), ItemKind::Tab(_)))
            .count();
        omit(self.report, Omission::TodayTab, tabs);
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
            ItemKind::Tab(tab) => self.tab(tab, out),
            _ if depth >= MAX_DEPTH => omit(self.report, Omission::UnknownContainer, 1),
            ItemKind::Folder => {
                if depth > 0 {
                    omit(self.report, Omission::NestedFolder, 1);
                }
                self.collect(id, depth + 1, visited, out);
            }
            ItemKind::Split => self.collect(id, depth, visited, out),
            ItemKind::Unknown => omit(self.report, Omission::UnknownContainer, 1),
        }
    }

    fn tab(&mut self, tab: &Value, out: &mut Vec<Entry>) {
        let url = tab.get("savedURL").and_then(Value::as_str).unwrap_or("");
        match scheme(url) {
            Ok(scheme) => out.push(Entry {
                url: url.to_owned(),
                scheme,
                line: 0,
            }),
            Err(reason) => self.report.unrecognized.push(Unrecognized {
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
fn is_main_profile(marker: &Value) -> bool {
    marker.get("default").is_some()
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
