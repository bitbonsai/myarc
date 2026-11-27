//! Folders of a Space's Pins, as in Arc's sidebar.
//!
//! A folder belongs to one Space, may sit inside another folder of the same
//! Space up to `MAX_FOLDER_DEPTH` levels deep, and holds some of that Space's
//! Pins in its own order. Every other Pin of the Space is ungrouped and keeps
//! workspace order. A Pin sits in at most one folder.
//!
//! A folder is open or closed, and that state persists with the snapshot. A new
//! folder starts open. Deleting a folder deletes its subfolders and Pins too,
//! as Arc does, and archives each Pin so the Library's Archived Tabs can bring
//! it back.
//!
//! Folders follow their Pins. A Pin that moves to another Space, becomes a
//! Favorite, is unpinned or removed leaves its folder; deleting a Space deletes
//! its folders. A loaded snapshot keeps every folder it can and promotes or
//! drops what it can't, so a damaged folder never fails the workspace and never
//! loses a Pin: a Pin dropped from a folder shows ungrouped.

use std::collections::{BTreeMap, BTreeSet};

use crate::{
    ArchivedTab, Item, ItemId, MAX_ARCHIVED_TABS, MAX_ITEMS, MAX_SPACE_NAME_BYTES, Placement,
    RenameError, SpaceId, Workspace, WorkspaceError, check_limit, next_id, validate_name,
};

pub type FolderId = u64;

/// Folders across all Spaces.
pub const MAX_FOLDERS: usize = 256;
/// Maximum nesting depth for folders (0 = top-level, up to MAX_FOLDER_DEPTH - 1).
pub const MAX_FOLDER_DEPTH: usize = 8;
/// Same bound as a Space name.
pub const MAX_FOLDER_NAME_BYTES: usize = MAX_SPACE_NAME_BYTES;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Folder {
    pub id: FolderId,
    pub space_id: SpaceId,
    /// Parent folder in the same Space, or `None` for a top-level folder.
    pub parent_id: Option<FolderId>,
    pub name: String,
    /// Shows its Pins and child folders beneath it. Closed folders show only
    /// their row (and any selected descendant Pin).
    pub open: bool,
    /// Pins of `space_id` in the folder's order.
    pub items: Vec<ItemId>,
}

/// What `Workspace::delete_folder` removed, so adapters can close live pages.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FolderDeletion {
    /// The deleted folder and all descendant folders, in pre-order DFS.
    pub removed_folders: Vec<FolderId>,
    /// The removed Pins across the deleted subtree, in pre-order DFS.
    pub removed_items: Vec<ItemId>,
    /// How many of them went to Archived Tabs.
    pub archived: usize,
}

impl Workspace {
    /// Every folder in workspace order; siblings show in this relative order.
    pub fn folders(&self) -> &[Folder] {
        &self.state.folders
    }

    /// The folder holding `item_id`, if any.
    pub fn folder_of(&self, item_id: ItemId) -> Option<FolderId> {
        self.state
            .folders
            .iter()
            .find(|folder| folder.items.contains(&item_id))
            .map(|folder| folder.id)
    }

    /// Nesting depth of `id` (0 for top-level folders).
    pub fn folder_depth(&self, id: FolderId) -> usize {
        folder_depth_in(&self.state.folders, id)
    }

    /// Whether `ancestor` is an ancestor of `descendant`.
    pub fn is_folder_ancestor(&self, ancestor: FolderId, descendant: FolderId) -> bool {
        let mut current = self.folder(descendant).ok().and_then(|f| f.parent_id);
        let mut steps = 0;
        while let Some(pid) = current {
            if pid == ancestor {
                return true;
            }
            steps += 1;
            if steps > self.state.folders.len() {
                break;
            }
            current = self.folder(pid).ok().and_then(|f| f.parent_id);
        }
        false
    }

    /// Maximum depth below `id` (0 for a folder with no children).
    pub fn folder_subtree_height(&self, id: FolderId) -> usize {
        self.subtree_height_bounded(id, 0)
    }

    fn subtree_height_bounded(&self, id: FolderId, guard: usize) -> usize {
        if guard > MAX_FOLDERS {
            return 0;
        }
        self.state
            .folders
            .iter()
            .filter(|f| f.parent_id == Some(id))
            .map(|child| 1 + self.subtree_height_bounded(child.id, guard + 1))
            .max()
            .unwrap_or(0)
    }

    /// `id` followed by all descendant folder IDs in pre-order DFS.
    pub fn folder_subtree_ids(&self, id: FolderId) -> Vec<FolderId> {
        let mut out = Vec::new();
        let mut visited = BTreeSet::new();
        self.collect_subtree_ids(id, &mut visited, &mut out);
        out
    }

    fn collect_subtree_ids(
        &self,
        id: FolderId,
        visited: &mut BTreeSet<FolderId>,
        out: &mut Vec<FolderId>,
    ) {
        if !visited.insert(id) {
            return;
        }
        out.push(id);
        for folder in &self.state.folders {
            if folder.parent_id == Some(id) {
                self.collect_subtree_ids(folder.id, visited, out);
            }
        }
    }

    /// All folders of `space_id` in pre-order DFS tree order, preserving
    /// sibling order from `self.state.folders`.
    pub fn folders_in_tree_order(&self, space_id: SpaceId) -> Vec<Folder> {
        let mut out = Vec::new();
        let mut visited = BTreeSet::new();
        for folder in &self.state.folders {
            if folder.space_id == space_id && folder.parent_id.is_none() {
                self.collect_tree_folders(space_id, folder.id, &mut visited, &mut out);
            }
        }
        out
    }

    fn collect_tree_folders(
        &self,
        space_id: SpaceId,
        id: FolderId,
        visited: &mut BTreeSet<FolderId>,
        out: &mut Vec<Folder>,
    ) {
        if !visited.insert(id) {
            return;
        }
        let Some(folder) = self
            .state
            .folders
            .iter()
            .find(|f| f.id == id && f.space_id == space_id)
        else {
            return;
        };
        out.push(folder.clone());
        let child_ids: Vec<FolderId> = self
            .state
            .folders
            .iter()
            .filter(|f| f.space_id == space_id && f.parent_id == Some(id))
            .map(|f| f.id)
            .collect();
        for child_id in child_ids {
            self.collect_tree_folders(space_id, child_id, visited, out);
        }
    }

    /// A new, empty, open top-level folder after the Space's other folders.
    pub fn create_folder(
        &mut self,
        space_id: SpaceId,
        name: &str,
    ) -> Result<FolderId, WorkspaceError> {
        self.create_folder_in(space_id, None, name)
    }

    /// A new, empty, open folder inside `parent_id` (or top-level when `None`).
    pub fn create_folder_in(
        &mut self,
        space_id: SpaceId,
        parent_id: Option<FolderId>,
        name: &str,
    ) -> Result<FolderId, WorkspaceError> {
        self.space(space_id)?;
        validate_name(name).map_err(WorkspaceError::InvalidFolderName)?;
        if let Some(pid) = parent_id {
            let parent = self.folder(pid)?;
            if parent.space_id != space_id {
                return Err(WorkspaceError::DifferentFolderSpace(pid));
            }
            if self.folder_depth(pid) + 1 >= MAX_FOLDER_DEPTH {
                return Err(WorkspaceError::FolderDepthExceeded);
            }
        }
        check_limit(self.state.folders.len(), MAX_FOLDERS, "folders")?;
        let id = self.state.next_folder_id;
        let next = next_id(id, "folder")?;
        self.state.folders.push(Folder {
            id,
            space_id,
            parent_id,
            name: name.to_owned(),
            open: true,
            items: Vec::new(),
        });
        self.state.next_folder_id = next;
        Ok(id)
    }

    /// Creates a new open folder named after `source_space_id` inside
    /// `target_space_id` and copies all of the source Space's pinned tabs
    /// (ungrouped Pins into the new folder, and each source folder as a
    /// subfolder preserving hierarchy and Pin order). The source Space and its
    /// items are left untouched. Atomic: fails without mutating if limits,
    /// depth, or IDs would be exceeded.
    pub fn export_space_into_folder(
        &mut self,
        source_space_id: SpaceId,
        target_space_id: SpaceId,
    ) -> Result<FolderId, WorkspaceError> {
        let folder_name = self.space(source_space_id)?.name.clone();
        self.space(target_space_id)?;
        if source_space_id == target_space_id {
            return Err(WorkspaceError::SameSpace(source_space_id));
        }
        validate_name(&folder_name).map_err(WorkspaceError::InvalidFolderName)?;

        let src_folders = self.folders_in_tree_order(source_space_id);
        let needed_folders = 1 + src_folders.len();
        if self.state.folders.len().saturating_add(needed_folders) > MAX_FOLDERS {
            return Err(WorkspaceError::LimitExceeded("folders"));
        }
        for f in &src_folders {
            if self.folder_depth(f.id) + 1 >= MAX_FOLDER_DEPTH {
                return Err(WorkspaceError::FolderDepthExceeded);
            }
        }

        // Collect loose Pins and per-folder Pins before mutating.
        type PinCopy = (Option<String>, String, String);
        let mut grouped: BTreeSet<ItemId> = BTreeSet::new();
        for folder in &src_folders {
            grouped.extend(folder.items.iter().copied());
        }
        let mut loose_copies: Vec<PinCopy> = Vec::new();
        for item in &self.state.items {
            if item.placement
                == (Placement::Pin {
                    space_id: source_space_id,
                })
                && !grouped.contains(&item.id)
            {
                loose_copies.push((
                    item.canonical_url.clone(),
                    item.title.clone(),
                    item.name.clone(),
                ));
            }
        }
        let mut folder_copies: Vec<(&Folder, Vec<PinCopy>)> = Vec::with_capacity(src_folders.len());
        let mut total_items = loose_copies.len();
        for folder in &src_folders {
            let mut copies = Vec::new();
            for &item_id in &folder.items {
                if let Some(item) = self.state.items.iter().find(|item| {
                    item.id == item_id
                        && item.placement
                            == Placement::Pin {
                                space_id: source_space_id,
                            }
                }) {
                    copies.push((
                        item.canonical_url.clone(),
                        item.title.clone(),
                        item.name.clone(),
                    ));
                }
            }
            total_items += copies.len();
            folder_copies.push((folder, copies));
        }

        if self.state.items.len().saturating_add(total_items) > MAX_ITEMS {
            return Err(WorkspaceError::LimitExceeded("items"));
        }
        let mut next_folder = self.state.next_folder_id;
        for _ in 0..needed_folders {
            next_folder = next_id(next_folder, "folder")?;
        }
        let mut next_item = self.state.next_item_id;
        for _ in 0..total_items {
            next_item = next_id(next_item, "item")?;
        }

        let root_folder_id = self.state.next_folder_id;
        let mut cur_folder_id = root_folder_id + 1;
        let mut cur_item_id = self.state.next_item_id;

        let mut root_items = Vec::with_capacity(loose_copies.len());
        for (canonical_url, title, name) in loose_copies {
            self.state.items.push(Item {
                id: cur_item_id,
                placement: Placement::Pin {
                    space_id: target_space_id,
                },
                canonical_url,
                title,
                name,
                last_viewed_secs: 0,
            });
            root_items.push(cur_item_id);
            cur_item_id += 1;
        }
        self.state.folders.push(Folder {
            id: root_folder_id,
            space_id: target_space_id,
            parent_id: None,
            name: folder_name,
            open: true,
            items: root_items,
        });

        let mut id_map: BTreeMap<FolderId, FolderId> = BTreeMap::new();
        for (src_folder, copies) in folder_copies {
            let new_id = cur_folder_id;
            cur_folder_id += 1;
            id_map.insert(src_folder.id, new_id);
            let new_parent = match src_folder.parent_id {
                Some(pid) => id_map.get(&pid).copied().unwrap_or(root_folder_id),
                None => root_folder_id,
            };
            let mut new_items = Vec::with_capacity(copies.len());
            for (canonical_url, title, name) in copies {
                self.state.items.push(Item {
                    id: cur_item_id,
                    placement: Placement::Pin {
                        space_id: target_space_id,
                    },
                    canonical_url,
                    title,
                    name,
                    last_viewed_secs: 0,
                });
                new_items.push(cur_item_id);
                cur_item_id += 1;
            }
            self.state.folders.push(Folder {
                id: new_id,
                space_id: target_space_id,
                parent_id: Some(new_parent),
                name: src_folder.name.clone(),
                open: src_folder.open,
                items: new_items,
            });
        }
        self.state.next_item_id = next_item;
        self.state.next_folder_id = next_folder;
        Ok(root_folder_id)
    }

    pub fn rename_folder(&mut self, id: FolderId, name: &str) -> Result<(), WorkspaceError> {
        validate_name(name).map_err(WorkspaceError::InvalidFolderName)?;
        self.folder_mut(id)?.name = name.to_owned();
        Ok(())
    }

    /// Opens or closes a folder. Returns whether it changed, so adapters can
    /// skip writes.
    pub fn set_folder_open(&mut self, id: FolderId, open: bool) -> Result<bool, WorkspaceError> {
        let folder = self.folder_mut(id)?;
        if folder.open == open {
            return Ok(false);
        }
        folder.open = open;
        Ok(true)
    }

    /// Reorder a folder among its current siblings. `None` moves to the end of
    /// its sibling group; moving before oneself is a no-op.
    pub fn move_folder_before(
        &mut self,
        id: FolderId,
        before: Option<FolderId>,
    ) -> Result<(), WorkspaceError> {
        let parent_id = self.folder(id)?.parent_id;
        self.move_folder(id, parent_id, before)
    }

    /// Moves folder `id` under `parent_id` (`None` = top-level of its Space),
    /// before sibling `before` (`None` = end of `parent_id`'s children).
    /// Rejects cross-Space targets, cycles, and moves exceeding
    /// `MAX_FOLDER_DEPTH`.
    pub fn move_folder(
        &mut self,
        id: FolderId,
        parent_id: Option<FolderId>,
        before: Option<FolderId>,
    ) -> Result<(), WorkspaceError> {
        let current = self.folder(id)?;
        let space_id = current.space_id;
        let current_parent = current.parent_id;
        if let Some(pid) = parent_id {
            let parent = self.folder(pid)?;
            if parent.space_id != space_id {
                return Err(WorkspaceError::DifferentFolderSpace(pid));
            }
            if pid == id || self.is_folder_ancestor(id, pid) {
                return Err(WorkspaceError::FolderCycle(id));
            }
            let new_depth = self.folder_depth(pid) + 1;
            let height = self.folder_subtree_height(id);
            if new_depth + height >= MAX_FOLDER_DEPTH {
                return Err(WorkspaceError::FolderDepthExceeded);
            }
        }
        if let Some(before_id) = before {
            let target = self.folder(before_id)?;
            if target.space_id != space_id {
                return Err(WorkspaceError::DifferentFolderSpace(before_id));
            }
            if before_id == id {
                if current_parent == parent_id {
                    return Ok(());
                }
                return Err(WorkspaceError::FolderCycle(id));
            }
            if target.parent_id != parent_id {
                return Err(WorkspaceError::DifferentFolderSpace(before_id));
            }
        }
        let folders = &mut self.state.folders;
        let source = folders
            .iter()
            .position(|folder| folder.id == id)
            .ok_or(WorkspaceError::UnknownFolder(id))?;
        let mut folder = folders.remove(source);
        folder.parent_id = parent_id;
        let target = match before {
            Some(before_id) => folders.iter().position(|f| f.id == before_id),
            None => folders
                .iter()
                .rposition(|f| f.space_id == space_id && f.parent_id == parent_id)
                .or_else(|| folders.iter().rposition(|f| f.space_id == space_id))
                .map(|last| last + 1),
        };
        let target = target.unwrap_or(source).min(folders.len());
        folders.insert(target, folder);
        Ok(())
    }

    /// Moves a Pin into `folder`, before `before` (a Pin of that folder) or at
    /// its end, taking it out of any other folder. `None` takes it out of its
    /// folder, so it shows ungrouped; `before` must then be `None`. The folder
    /// must belong to the Pin's Space: pin a page there first.
    pub fn move_pin_to_folder(
        &mut self,
        item_id: ItemId,
        folder: Option<FolderId>,
        before: Option<ItemId>,
    ) -> Result<(), WorkspaceError> {
        let Placement::Pin { space_id } = self.item(item_id)?.placement else {
            return Err(WorkspaceError::NotPinned(item_id));
        };
        match folder {
            Some(folder_id) => {
                let target = self.folder(folder_id)?;
                if target.space_id != space_id {
                    return Err(WorkspaceError::DifferentFolderSpace(folder_id));
                }
                if let Some(before) = before {
                    if !target.items.contains(&before) {
                        return Err(WorkspaceError::DifferentSection(before));
                    }
                    if before == item_id {
                        return Ok(());
                    }
                }
            }
            None => {
                if let Some(before) = before {
                    return Err(WorkspaceError::DifferentSection(before));
                }
            }
        }
        for folder in &mut self.state.folders {
            folder.items.retain(|&id| id != item_id);
        }
        if let Some(folder_id) = folder {
            let folder = self.folder_mut(folder_id)?;
            let index = before
                .and_then(|before| folder.items.iter().position(|&id| id == before))
                .unwrap_or(folder.items.len());
            folder.items.insert(index, item_id);
        }
        Ok(())
    }

    /// Deletes a folder, its descendant subfolders, and all of their Pins, as
    /// Arc does. With `archive_at`, the caller's wall-clock seconds, each Pin
    /// goes to Archived Tabs first; pass `None` for Profiles that save no
    /// history. The caller confirms with the user first and closes live pages
    /// for `removed_items`.
    pub fn delete_folder(
        &mut self,
        id: FolderId,
        archive_at: Option<u64>,
    ) -> Result<FolderDeletion, WorkspaceError> {
        let space_id = self.folder(id)?.space_id;
        let subtree_ids = self.folder_subtree_ids(id);
        let mut removed_items = Vec::new();
        for &fid in &subtree_ids {
            if let Ok(f) = self.folder(fid) {
                removed_items.extend(f.items.iter().copied());
            }
        }
        let mut archived = 0;
        if let Some(now_secs) = archive_at {
            self.prune_archived_tabs(now_secs);
            for &item_id in &removed_items {
                let Ok(item) = self.item(item_id) else {
                    continue;
                };
                let Some(url) = item.canonical_url.clone() else {
                    continue;
                };
                if url == "about:blank" {
                    continue;
                }
                let title = item.title.clone();
                if self.state.archived.len() >= MAX_ARCHIVED_TABS {
                    self.state.archived.remove(0);
                }
                self.state.archived.push(ArchivedTab {
                    id: item_id,
                    space_id,
                    url,
                    title,
                    archived_secs: now_secs,
                });
                archived += 1;
            }
        }
        let removed_item_set: BTreeSet<ItemId> = removed_items.iter().copied().collect();
        let removed_folder_set: BTreeSet<FolderId> = subtree_ids.iter().copied().collect();
        self.state
            .items
            .retain(|item| !removed_item_set.contains(&item.id));
        self.state
            .folders
            .retain(|folder| !removed_folder_set.contains(&folder.id));
        self.clear_hidden_selections();
        Ok(FolderDeletion {
            removed_folders: subtree_ids,
            removed_items,
            archived,
        })
    }

    fn folder(&self, id: FolderId) -> Result<&Folder, WorkspaceError> {
        self.state
            .folders
            .iter()
            .find(|folder| folder.id == id)
            .ok_or(WorkspaceError::UnknownFolder(id))
    }

    fn folder_mut(&mut self, id: FolderId) -> Result<&mut Folder, WorkspaceError> {
        self.state
            .folders
            .iter_mut()
            .find(|folder| folder.id == id)
            .ok_or(WorkspaceError::UnknownFolder(id))
    }

    /// Drops folders of deleted Spaces and folder entries that are no longer
    /// Pins of the folder's Space. Mutations that move or remove items call it.
    pub(crate) fn prune_folders(&mut self) {
        let spaces: BTreeSet<SpaceId> = self.state.spaces.iter().map(|space| space.id).collect();
        let items = &self.state.items;
        self.state.folders.retain_mut(|folder| {
            folder.items.retain(|&id| {
                items.iter().any(|item| {
                    item.id == id
                        && item.placement
                            == Placement::Pin {
                                space_id: folder.space_id,
                            }
                })
            });
            spaces.contains(&folder.space_id)
        });
    }

    /// A loaded snapshot's folders, cut down to what is valid: folders with a
    /// zero or repeated ID, a bad name or an unknown Space go, entries that are
    /// not Pins of the folder's Space or already in a folder go, invalid or
    /// cyclic `parent_id` links promote to top-level, folders deeper than
    /// `MAX_FOLDER_DEPTH` promote to top-level, and only the first
    /// `MAX_FOLDERS` stay. The next folder ID moves past every kept one.
    pub(crate) fn sanitize_folders(&mut self) {
        self.state.folders.truncate(MAX_FOLDERS);
        let mut ids = BTreeSet::new();
        let mut placed = BTreeSet::new();
        let spaces: BTreeSet<SpaceId> = self.state.spaces.iter().map(|space| space.id).collect();
        let items = &self.state.items;
        self.state.folders.retain_mut(|folder| {
            if folder.id == 0
                || folder.id == FolderId::MAX
                || !ids.insert(folder.id)
                || !spaces.contains(&folder.space_id)
                || validate_name(&folder.name).is_err()
            {
                return false;
            }
            folder.items.retain(|&id| {
                items.iter().any(|item| {
                    item.id == id
                        && item.placement
                            == Placement::Pin {
                                space_id: folder.space_id,
                            }
                }) && placed.insert(id)
            });
            true
        });

        // Clear parent_id pointing to self, missing folder, or another Space.
        let kept_spaces: BTreeMap<FolderId, SpaceId> = self
            .state
            .folders
            .iter()
            .map(|f| (f.id, f.space_id))
            .collect();
        for folder in &mut self.state.folders {
            if let Some(pid) = folder.parent_id {
                if pid == folder.id || kept_spaces.get(&pid) != Some(&folder.space_id) {
                    folder.parent_id = None;
                }
            }
        }

        // Break cycles by promoting the first folder encountered in a cycle.
        let count = self.state.folders.len();
        for i in 0..count {
            let start_id = self.state.folders[i].id;
            let mut seen = BTreeSet::from([start_id]);
            let mut cur = self.state.folders[i].parent_id;
            let mut cyclic = false;
            while let Some(pid) = cur {
                if !seen.insert(pid) {
                    cyclic = true;
                    break;
                }
                cur = self
                    .state
                    .folders
                    .iter()
                    .find(|f| f.id == pid)
                    .and_then(|f| f.parent_id);
            }
            if cyclic {
                self.state.folders[i].parent_id = None;
            }
        }

        // Promote folders exceeding MAX_FOLDER_DEPTH to top-level.
        for _ in 0..count {
            let mut promoted = false;
            for i in 0..count {
                let id = self.state.folders[i].id;
                if folder_depth_in(&self.state.folders, id) >= MAX_FOLDER_DEPTH {
                    self.state.folders[i].parent_id = None;
                    promoted = true;
                }
            }
            if !promoted {
                break;
            }
        }

        // Kept IDs are below `FolderId::MAX`, so the sum cannot overflow.
        let past_last = self
            .state
            .folders
            .iter()
            .map(|folder| folder.id + 1)
            .max()
            .unwrap_or(1);
        self.state.next_folder_id = self.state.next_folder_id.max(past_last);
    }
}

/// Computes the nesting depth of `id` in `folders` (0 = top-level).
pub fn folder_depth_in(folders: &[Folder], id: FolderId) -> usize {
    let mut depth = 0;
    let mut current = folders
        .iter()
        .find(|f| f.id == id)
        .and_then(|f| f.parent_id);
    while let Some(pid) = current {
        depth += 1;
        if depth > folders.len() {
            break;
        }
        current = folders
            .iter()
            .find(|f| f.id == pid)
            .and_then(|f| f.parent_id);
    }
    depth
}

/// Whether `id` and every ancestor folder in `folders` is open.
pub fn folder_chain_open(folders: &[Folder], id: FolderId) -> bool {
    let mut current = Some(id);
    let mut steps = 0;
    while let Some(fid) = current {
        let Some(folder) = folders.iter().find(|f| f.id == fid) else {
            return false;
        };
        if !folder.open {
            return false;
        }
        steps += 1;
        if steps > folders.len() {
            return false;
        }
        current = folder.parent_id;
    }
    true
}

impl RenameError {
    pub(crate) fn folder_message(&self) -> &'static str {
        match self {
            Self::UnknownSpace => "Folder does not exist",
            Self::EmptyName => "Enter a folder name",
            Self::NameTooLong => "Folder name exceeds 128 UTF-8 bytes",
            Self::ControlCharacter => "Folder name cannot contain control characters",
        }
    }
}
