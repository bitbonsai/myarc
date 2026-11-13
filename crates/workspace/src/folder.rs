//! Folders of a Space's Pins, as in Arc's sidebar.
//!
//! A folder belongs to one Space and holds some of that Space's Pins in its own
//! order. Every other Pin of the Space is ungrouped and keeps workspace order.
//! A Pin sits in at most one folder. Folders don't nest: Arc can put a folder
//! inside another, and this model doesn't yet.
//!
//! A folder is open or closed, and that state persists with the snapshot. A new
//! folder starts open. Deleting a folder deletes its Pins too, as Arc does, and
//! archives each of them so the Library's Archived Tabs can bring it back.
//!
//! Folders follow their Pins. A Pin that moves to another Space, becomes a
//! Favorite, is unpinned or removed leaves its folder; deleting a Space deletes
//! its folders. A loaded snapshot keeps every folder it can and drops what it
//! can't, so a damaged folder never fails the workspace and never loses a Pin:
//! a Pin dropped from a folder shows ungrouped.

use std::collections::BTreeSet;

use crate::{
    ArchivedTab, ItemId, MAX_ARCHIVED_TABS, MAX_SPACE_NAME_BYTES, Placement, RenameError, SpaceId,
    Workspace, WorkspaceError, check_limit, next_id, validate_name,
};

pub type FolderId = u64;

/// Folders across all Spaces.
pub const MAX_FOLDERS: usize = 256;
/// Same bound as a Space name.
pub const MAX_FOLDER_NAME_BYTES: usize = MAX_SPACE_NAME_BYTES;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Folder {
    pub id: FolderId,
    pub space_id: SpaceId,
    pub name: String,
    /// Shows its Pins beneath it. Closed folders show only their row.
    pub open: bool,
    /// Pins of `space_id` in the folder's order.
    pub items: Vec<ItemId>,
}

/// What `Workspace::delete_folder` removed, so adapters can close live pages.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FolderDeletion {
    /// The folder's Pins in folder order. Their identities are gone.
    pub removed_items: Vec<ItemId>,
    /// How many of them went to Archived Tabs.
    pub archived: usize,
}

impl Workspace {
    /// Every folder in workspace order; a Space's folders show in this order.
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

    /// A new, empty, open folder after the Space's other folders.
    pub fn create_folder(
        &mut self,
        space_id: SpaceId,
        name: &str,
    ) -> Result<FolderId, WorkspaceError> {
        self.space(space_id)?;
        validate_name(name).map_err(WorkspaceError::InvalidFolderName)?;
        check_limit(self.state.folders.len(), MAX_FOLDERS, "folders")?;
        let id = self.state.next_folder_id;
        let next = next_id(id, "folder")?;
        self.state.folders.push(Folder {
            id,
            space_id,
            name: name.to_owned(),
            open: true,
            items: Vec::new(),
        });
        self.state.next_folder_id = next;
        Ok(id)
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

    /// Reorder a Space's folders. `None` moves to the end of the Space's
    /// folders; moving before oneself is a no-op. Folders of other Spaces keep
    /// their places.
    pub fn move_folder_before(
        &mut self,
        id: FolderId,
        before: Option<FolderId>,
    ) -> Result<(), WorkspaceError> {
        let space_id = self.folder(id)?.space_id;
        if let Some(before) = before {
            if self.folder(before)?.space_id != space_id {
                return Err(WorkspaceError::DifferentFolderSpace(before));
            }
            if before == id {
                return Ok(());
            }
        }
        let folders = &mut self.state.folders;
        let source = folders
            .iter()
            .position(|folder| folder.id == id)
            .ok_or(WorkspaceError::UnknownFolder(id))?;
        let folder = folders.remove(source);
        let target = match before {
            Some(before) => folders.iter().position(|folder| folder.id == before),
            None => folders
                .iter()
                .rposition(|folder| folder.space_id == space_id)
                .map(|last| last + 1),
        };
        // `before` was checked above; a Space with no other folder puts it back.
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

    /// Deletes a folder and its Pins, as Arc does. With `archive_at`, the
    /// caller's wall-clock seconds, each Pin goes to Archived Tabs first; pass
    /// `None` for Profiles that save no history. The caller confirms with the
    /// user first and closes live pages for `removed_items`.
    pub fn delete_folder(
        &mut self,
        id: FolderId,
        archive_at: Option<u64>,
    ) -> Result<FolderDeletion, WorkspaceError> {
        let folder = self.folder(id)?.clone();
        let mut archived = 0;
        if let Some(now_secs) = archive_at {
            self.prune_archived_tabs(now_secs);
            for &item_id in &folder.items {
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
                    space_id: folder.space_id,
                    url,
                    title,
                    archived_secs: now_secs,
                });
                archived += 1;
            }
        }
        let removed: BTreeSet<ItemId> = folder.items.iter().copied().collect();
        self.state.items.retain(|item| !removed.contains(&item.id));
        self.state.folders.retain(|folder| folder.id != id);
        self.clear_hidden_selections();
        Ok(FolderDeletion {
            removed_items: folder.items,
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
    /// not Pins of the folder's Space or already in a folder go, and only the
    /// first `MAX_FOLDERS` stay. The next folder ID moves past every kept one.
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
