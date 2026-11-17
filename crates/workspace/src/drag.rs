//! Dragging rows in the sidebar, as in Arc, and the order the keyboard steps
//! through them.
//!
//! A window's Pins and Today tabs can be dragged. Dropping one on a folder puts
//! it in that folder, pinning a Today tab on the way. Dropping it among the
//! ungrouped Pins takes it out of any folder, and dropping it in Today unpins
//! it. Dragging within a section reorders it. Favorites don't take part: they
//! are Profile-wide, and their tiles have their own drop rules.
//!
//! A split's row drags as one item, as in Arc: both tabs move together within
//! Today, left then right, and a row dropped on a split lands above it. Pins
//! and folders refuse a split until a split can be pinned.
//!
//! A Pin dragged to Today keeps its identity, its saved URL as the page its
//! dormant row reopens and its page title, so a dormant Pin's row survives the
//! move. Its name goes, since temporary items have none. A drop either applies
//! in full or leaves the workspace unchanged.
//!
//! Next and previous tab follow the rows the sidebar shows: Favorites, the
//! ungrouped Pins, each folder's Pins while it is open, then Today. A closed
//! folder hides its Pins from the keyboard too, except its selected Pin, which
//! still shows.

use crate::{
    FolderId, ItemId, Placement, SidebarSnapshot, SpaceId, WindowId, Workspace, WorkspaceError,
};

/// Where a dragged row lands, in the window's current Space. `before` names the
/// row it lands above; `None` puts it at the end.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DropTarget {
    /// The ungrouped Pins. `before` is an ungrouped Pin of the Space.
    Pins { before: Option<ItemId> },
    /// A folder of the Space. `before` is one of its Pins.
    Folder {
        folder: FolderId,
        before: Option<ItemId>,
    },
    /// Today's tabs. `before` is a Today tab of the Space.
    Today { before: Option<ItemId> },
}

/// The band at the top and bottom of the sidebar's visible rows where a
/// dragged row scrolls them, in DIP.
pub const AUTOSCROLL_EDGE: i32 = 40;
/// The most a drag scrolls the rows per tick, in DIP, at the very edge.
pub const AUTOSCROLL_MAX_STEP: i32 = 16;

/// How far a row dragged to `pointer_y` scrolls rows shown from 0 to
/// `height`, per tick: negative scrolls up, positive down, and 0 in the
/// middle. The step grows toward the edge and stays at its most past it, so
/// a drag held at an edge keeps going. A view shorter than two bands uses a
/// third of its height for each, and an empty one never scrolls.
pub fn autoscroll_step(pointer_y: i32, height: i32) -> i32 {
    if height <= 0 {
        return 0;
    }
    let edge = AUTOSCROLL_EDGE.min(height / 3).max(1);
    let step = |distance: i32| {
        let depth = (edge - distance.max(0)).max(0);
        // Rounds up, so the first pixel inside the band already moves.
        (AUTOSCROLL_MAX_STEP * depth + edge - 1) / edge
    };
    if pointer_y < edge {
        -step(pointer_y)
    } else if pointer_y >= height - edge {
        step(height - 1 - pointer_y)
    } else {
        0
    }
}

impl Workspace {
    /// Drops `item_id`, a Pin or Today tab of the window's Space, on `target`.
    /// Pinning a Today tab saves `page_url` when the caller has a live page,
    /// else the page it remembers; the caller passes a browser-canonicalized
    /// URL. Dropping a row above itself is a no-op success. On error nothing
    /// changes.
    pub fn drop_item(
        &mut self,
        window_id: WindowId,
        item_id: ItemId,
        target: DropTarget,
        page_url: Option<&str>,
    ) -> Result<(), WorkspaceError> {
        let space_id = self.window(window_id)?.space_id;
        let placement = self.item(item_id)?.placement;
        let pinned = match placement {
            Placement::Pin { space_id: owner } if owner == space_id => true,
            Placement::Temporary { space_id: owner } if owner == space_id => false,
            _ => return Err(WorkspaceError::NotDraggable(item_id)),
        };
        if !pinned
            && self.split_of(item_id).is_some()
            && !matches!(target, DropTarget::Today { .. })
        {
            return Err(WorkspaceError::SplitNotPinnable(item_id));
        }
        self.check_drop_target(space_id, target)?;
        let saved = self.state.clone();
        let result = self.apply_drop(space_id, item_id, pinned, target, page_url);
        if result.is_err() {
            self.state = saved;
        }
        result
    }

    fn check_drop_target(
        &self,
        space_id: SpaceId,
        target: DropTarget,
    ) -> Result<(), WorkspaceError> {
        match target {
            DropTarget::Pins {
                before: Some(before),
            } => {
                if self.item(before)?.placement != (Placement::Pin { space_id })
                    || self.folder_of(before).is_some()
                {
                    return Err(WorkspaceError::DifferentSection(before));
                }
            }
            DropTarget::Folder { folder, before } => {
                let folder = self
                    .folders()
                    .iter()
                    .find(|entry| entry.id == folder)
                    .ok_or(WorkspaceError::UnknownFolder(folder))?;
                if folder.space_id != space_id {
                    return Err(WorkspaceError::DifferentFolderSpace(folder.id));
                }
                if let Some(before) = before {
                    if !folder.items.contains(&before) {
                        return Err(WorkspaceError::DifferentSection(before));
                    }
                }
            }
            DropTarget::Today {
                before: Some(before),
            } => {
                if self.item(before)?.placement != (Placement::Temporary { space_id }) {
                    return Err(WorkspaceError::DifferentSection(before));
                }
            }
            DropTarget::Pins { before: None } | DropTarget::Today { before: None } => {}
        }
        Ok(())
    }

    fn apply_drop(
        &mut self,
        space_id: SpaceId,
        item_id: ItemId,
        pinned: bool,
        target: DropTarget,
        page_url: Option<&str>,
    ) -> Result<(), WorkspaceError> {
        let pin = Placement::Pin { space_id };
        match target {
            DropTarget::Pins { before } => {
                if !pinned {
                    self.set_placement(item_id, pin, page_url)?;
                }
                self.move_pin_to_folder(item_id, None, None)?;
                self.move_item_before(item_id, before)
            }
            DropTarget::Folder { folder, before } => {
                if !pinned {
                    self.set_placement(item_id, pin, page_url)?;
                }
                self.move_pin_to_folder(item_id, Some(folder), before)
            }
            DropTarget::Today { before } => {
                if pinned {
                    self.unpin_to_today(item_id, space_id);
                }
                // A row never lands between a split's two tabs.
                let before = before.map(|id| self.split_row(id));
                match self.split_of(item_id) {
                    Some(split) => self.move_split_before(split, before),
                    None => self.move_item_before(item_id, before),
                }
            }
        }
    }

    /// A Pin becomes a Today tab that remembers its saved URL and title.
    fn unpin_to_today(&mut self, item_id: ItemId, space_id: SpaceId) {
        if let Some(item) = self.state.items.iter_mut().find(|item| item.id == item_id) {
            item.placement = Placement::Temporary { space_id };
            item.name.clear();
            item.last_viewed_secs = 0;
        }
        self.prune_folders();
        self.clear_hidden_selections();
    }
}

impl SidebarSnapshot {
    /// The rows next and previous tab step through, in the order the sidebar
    /// shows them. A closed folder's Pins are left out, except the selected one.
    pub fn row_order(&self) -> Vec<ItemId> {
        let foldered = |id: ItemId| self.folders.iter().any(|folder| folder.items.contains(&id));
        let mut order: Vec<ItemId> = self.favorites.iter().map(|item| item.id).collect();
        order.extend(
            self.pins
                .iter()
                .map(|item| item.id)
                .filter(|&id| !foldered(id)),
        );
        for folder in &self.folders {
            order.extend(folder.items.iter().copied().filter(|&id| {
                (folder.open || self.selected_item_id == Some(id))
                    && self.pins.iter().any(|item| item.id == id)
            }));
        }
        order.extend(self.temporary.iter().map(|item| item.id));
        order
    }
}

#[cfg(test)]
mod autoscroll_tests {
    use super::{AUTOSCROLL_EDGE, AUTOSCROLL_MAX_STEP, autoscroll_step};

    #[test]
    fn the_middle_does_not_scroll() {
        assert_eq!(autoscroll_step(300, 600), 0);
        assert_eq!(autoscroll_step(AUTOSCROLL_EDGE, 600), 0);
        assert_eq!(autoscroll_step(600 - 1 - AUTOSCROLL_EDGE, 600), 0);
    }

    #[test]
    fn edges_scroll_faster_the_closer_the_pointer() {
        assert_eq!(autoscroll_step(0, 600), -AUTOSCROLL_MAX_STEP);
        assert_eq!(autoscroll_step(599, 600), AUTOSCROLL_MAX_STEP);
        let near = autoscroll_step(10, 600);
        let far = autoscroll_step(30, 600);
        assert!(near < far && far < 0, "{near} {far}");
        assert!(autoscroll_step(590, 600) > autoscroll_step(570, 600));
        assert!(autoscroll_step(600 - AUTOSCROLL_EDGE, 600) > 0);
        assert!(autoscroll_step(AUTOSCROLL_EDGE - 1, 600) < 0);
    }

    #[test]
    fn past_an_edge_keeps_the_fastest_step() {
        assert_eq!(autoscroll_step(-50, 600), -AUTOSCROLL_MAX_STEP);
        assert_eq!(autoscroll_step(900, 600), AUTOSCROLL_MAX_STEP);
    }

    #[test]
    fn short_views_use_narrower_bands() {
        assert_eq!(autoscroll_step(0, 0), 0);
        assert_eq!(autoscroll_step(0, -5), 0);
        assert_eq!(autoscroll_step(30, 90), 0);
        assert!(autoscroll_step(10, 90) < 0);
        assert!(autoscroll_step(80, 90) > 0);
        assert_eq!(autoscroll_step(0, 1), -AUTOSCROLL_MAX_STEP);
    }
}
