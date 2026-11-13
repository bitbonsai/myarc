//! Split View: two Today tabs of one Space shown side by side in one window,
//! drawn as one sidebar row, as in Arc.
//!
//! A split pairs two temporary items of the same Space, so both belong to the
//! same Profile. `left` is the pane on the left and the row shows it first.
//! An item sits in at most one split. Pins and Favorites can't join a split
//! yet; pinning a split is a later step.
//!
//! The browser owns the panes, their sizes and focus. This model keeps which
//! items form a split so the sidebar can draw one row and a restart brings the
//! pair back. Splits follow their items: closing, pinning, saving or moving
//! either tab to another Space ends the split and the other stays a normal
//! tab. A loaded snapshot keeps every split it can and drops the rest, so a
//! damaged split never fails the workspace and never loses a tab.

use std::collections::BTreeSet;

use crate::{ItemId, MAX_ITEMS, Placement, SpaceId, Workspace, WorkspaceError};

/// Each split holds two items, so this many fit every item.
pub const MAX_SPLITS: usize = MAX_ITEMS / 2;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Split {
    pub left: ItemId,
    pub right: ItemId,
}

impl Split {
    pub fn contains(&self, item_id: ItemId) -> bool {
        self.left == item_id || self.right == item_id
    }

    /// The other pane of `item_id`, which must be in this split.
    pub fn partner(&self, item_id: ItemId) -> ItemId {
        if self.left == item_id {
            self.right
        } else {
            self.left
        }
    }
}

impl Workspace {
    /// Every split in workspace order.
    pub fn splits(&self) -> &[Split] {
        &self.state.splits
    }

    /// The split holding `item_id`, if any.
    pub fn split_of(&self, item_id: ItemId) -> Option<Split> {
        self.state
            .splits
            .iter()
            .find(|split| split.contains(item_id))
            .copied()
    }

    /// Shows `left` and `right` side by side. Both must be Today tabs of one
    /// Space and in no other split. The same pair again is a no-op; the same
    /// tabs in the other order swap the panes. `right` moves to just after
    /// `left` in the Space's Today order, where the split's row shows.
    /// Returns whether anything changed, so adapters can skip writes.
    pub fn create_split(&mut self, left: ItemId, right: ItemId) -> Result<bool, WorkspaceError> {
        let space = self.split_space(left)?;
        if self.split_space(right)? != space {
            return Err(WorkspaceError::SplitAcrossSpaces(right));
        }
        if left == right {
            return Err(WorkspaceError::SplitWithItself(left));
        }
        let wanted = Split { left, right };
        match (self.split_of(left), self.split_of(right)) {
            (Some(split), Some(other)) if split == other => {
                if split == wanted {
                    return Ok(false);
                }
                // The other order: swap the panes in place.
                for split in &mut self.state.splits {
                    if split.contains(left) {
                        *split = wanted;
                    }
                }
                return Ok(true);
            }
            (Some(_), _) => return Err(WorkspaceError::AlreadySplit(left)),
            (_, Some(_)) => return Err(WorkspaceError::AlreadySplit(right)),
            (None, None) => {}
        }
        // Distinct items keep this below the item limit; the check only bounds it.
        crate::check_limit(self.state.splits.len(), MAX_SPLITS, "splits")?;
        let items = &mut self.state.items;
        if let Some(from) = items.iter().position(|item| item.id == right) {
            let item = items.remove(from);
            let to = items
                .iter()
                .position(|item| item.id == left)
                .map_or(items.len(), |at| at + 1);
            items.insert(to, item);
        }
        self.state.splits.push(wanted);
        Ok(true)
    }

    /// Ends the split holding `item_id`, as Arc's Separate All Tabs does. Both
    /// tabs stay open as normal Today tabs in their current order. Returns the
    /// split that ended, or `None` when the item wasn't in one.
    pub fn separate_split(&mut self, item_id: ItemId) -> Result<Option<Split>, WorkspaceError> {
        self.item(item_id)?;
        let split = self.split_of(item_id);
        if split.is_some() {
            self.state.splits.retain(|split| !split.contains(item_id));
        }
        Ok(split)
    }

    /// The Space of a temporary item, or why it can't join a split.
    fn split_space(&self, item_id: ItemId) -> Result<SpaceId, WorkspaceError> {
        match self.item(item_id)?.placement {
            Placement::Temporary { space_id } => Ok(space_id),
            Placement::Pin { .. } | Placement::Favorite { .. } => {
                Err(WorkspaceError::SplitNeedsTemporary(item_id))
            }
        }
    }

    /// Drops splits whose items are gone, no longer Today tabs, or now in
    /// different Spaces. Every mutation that moves or removes items calls it.
    pub(crate) fn prune_splits(&mut self) {
        let items = &self.state.items;
        let space_of = |id: ItemId| {
            items
                .iter()
                .find(|item| item.id == id)
                .and_then(|item| match item.placement {
                    Placement::Temporary { space_id } => Some(space_id),
                    _ => None,
                })
        };
        self.state.splits.retain(|split| {
            let left = space_of(split.left);
            left.is_some() && left == space_of(split.right)
        });
    }

    /// A loaded snapshot's splits, cut down to what is valid: a split of an
    /// item with itself, of items that aren't Today tabs of one Space, or with
    /// an item already in an earlier split goes, and only the first
    /// `MAX_SPLITS` stay.
    pub(crate) fn sanitize_splits(&mut self) {
        self.state.splits.truncate(MAX_SPLITS);
        let mut placed = BTreeSet::new();
        self.state.splits.retain(|split| {
            split.left != split.right && placed.insert(split.left) && placed.insert(split.right)
        });
        self.prune_splits();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{DEFAULT_PROFILE_ID, DEFAULT_SPACE_ID, DEFAULT_WINDOW_ID};

    const URL: &str = "https://example.test/";

    fn today(workspace: &mut Workspace, count: usize) -> Vec<ItemId> {
        (0..count)
            .map(|_| workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap())
            .collect()
    }

    fn order(workspace: &Workspace) -> Vec<ItemId> {
        workspace.items().iter().map(|item| item.id).collect()
    }

    #[test]
    fn create_pairs_two_today_tabs_and_moves_the_right_one_beside_the_left() {
        let mut workspace = Workspace::default();
        let ids = today(&mut workspace, 3);
        assert_eq!(workspace.create_split(ids[0], ids[2]), Ok(true));
        assert_eq!(
            workspace.splits(),
            &[Split {
                left: ids[0],
                right: ids[2]
            }]
        );
        assert_eq!(order(&workspace), vec![ids[0], ids[2], ids[1]]);
        assert_eq!(workspace.split_of(ids[2]).unwrap().partner(ids[2]), ids[0]);
        assert_eq!(workspace.split_of(ids[1]), None);
    }

    #[test]
    fn the_same_pair_is_a_no_op_and_the_reverse_swaps_panes() {
        let mut workspace = Workspace::default();
        let ids = today(&mut workspace, 2);
        workspace.create_split(ids[0], ids[1]).unwrap();
        assert_eq!(workspace.create_split(ids[0], ids[1]), Ok(false));
        assert_eq!(workspace.create_split(ids[1], ids[0]), Ok(true));
        assert_eq!(
            workspace.splits(),
            &[Split {
                left: ids[1],
                right: ids[0]
            }]
        );
    }

    #[test]
    fn refuses_tabs_already_split_pinned_or_alone() {
        let mut workspace = Workspace::default();
        let ids = today(&mut workspace, 4);
        workspace.create_split(ids[0], ids[1]).unwrap();
        assert_eq!(
            workspace.create_split(ids[2], ids[1]),
            Err(WorkspaceError::AlreadySplit(ids[1]))
        );
        assert_eq!(
            workspace.create_split(ids[0], ids[2]),
            Err(WorkspaceError::AlreadySplit(ids[0]))
        );
        assert_eq!(
            workspace.create_split(ids[2], ids[2]),
            Err(WorkspaceError::SplitWithItself(ids[2]))
        );
        workspace
            .set_placement(
                ids[3],
                Placement::Pin {
                    space_id: DEFAULT_SPACE_ID,
                },
                Some(URL),
            )
            .unwrap();
        assert_eq!(
            workspace.create_split(ids[2], ids[3]),
            Err(WorkspaceError::SplitNeedsTemporary(ids[3]))
        );
        assert_eq!(
            workspace.create_split(ids[2], 99),
            Err(WorkspaceError::UnknownItem(99))
        );
        assert_eq!(workspace.splits().len(), 1);
    }

    #[test]
    fn refuses_tabs_of_another_space() {
        // Each Space has one Profile, so this also refuses another Profile's tab.
        let mut workspace = Workspace::default();
        let other = workspace.create_space(DEFAULT_PROFILE_ID, "Work").unwrap();
        let here = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let there = workspace.add_temporary_item(other).unwrap();
        assert_eq!(
            workspace.create_split(here, there),
            Err(WorkspaceError::SplitAcrossSpaces(there))
        );
        assert!(workspace.splits().is_empty());
    }

    #[test]
    fn separate_keeps_both_tabs_as_normal_rows() {
        let mut workspace = Workspace::default();
        let ids = today(&mut workspace, 2);
        let split = Split {
            left: ids[0],
            right: ids[1],
        };
        workspace.create_split(ids[0], ids[1]).unwrap();
        assert_eq!(workspace.separate_split(ids[1]), Ok(Some(split)));
        assert!(workspace.splits().is_empty());
        assert_eq!(order(&workspace), ids);
        assert_eq!(workspace.separate_split(ids[1]), Ok(None));
        assert_eq!(
            workspace.separate_split(99),
            Err(WorkspaceError::UnknownItem(99))
        );
    }

    #[test]
    fn closing_pinning_or_moving_a_pane_ends_the_split() {
        let mut workspace = Workspace::default();
        let other = workspace.create_space(DEFAULT_PROFILE_ID, "Work").unwrap();
        let ids = today(&mut workspace, 6);

        workspace.create_split(ids[0], ids[1]).unwrap();
        workspace.live_tab_closed(ids[0]).unwrap();
        assert!(workspace.splits().is_empty());
        assert!(workspace.items().iter().any(|item| item.id == ids[1]));

        workspace.create_split(ids[2], ids[3]).unwrap();
        workspace
            .set_placement(
                ids[2],
                Placement::Favorite {
                    profile_id: DEFAULT_PROFILE_ID,
                },
                Some(URL),
            )
            .unwrap();
        assert!(workspace.splits().is_empty());

        workspace.create_split(ids[4], ids[5]).unwrap();
        workspace
            .set_placement(ids[5], Placement::Temporary { space_id: other }, None)
            .unwrap();
        assert!(workspace.splits().is_empty());
    }

    #[test]
    fn deleting_the_space_drops_its_splits() {
        let mut workspace = Workspace::default();
        let other = workspace.create_space(DEFAULT_PROFILE_ID, "Work").unwrap();
        let left = workspace.add_temporary_item(other).unwrap();
        let right = workspace.add_temporary_item(other).unwrap();
        workspace.create_split(left, right).unwrap();
        workspace.delete_space(other).unwrap();
        assert!(workspace.splits().is_empty());
    }

    #[test]
    fn sidebar_lists_only_the_current_spaces_splits() {
        let mut workspace = Workspace::default();
        let other = workspace.create_space(DEFAULT_PROFILE_ID, "Work").unwrap();
        let ids = today(&mut workspace, 2);
        let away = [
            workspace.add_temporary_item(other).unwrap(),
            workspace.add_temporary_item(other).unwrap(),
        ];
        workspace.create_split(ids[0], ids[1]).unwrap();
        workspace.create_split(away[0], away[1]).unwrap();
        let sidebar = workspace.sidebar_snapshot(DEFAULT_WINDOW_ID).unwrap();
        assert_eq!(
            sidebar.splits,
            vec![Split {
                left: ids[0],
                right: ids[1]
            }]
        );
    }

    #[test]
    fn splits_survive_a_snapshot_round_trip() {
        let mut workspace = Workspace::default();
        let ids = today(&mut workspace, 2);
        workspace.create_split(ids[1], ids[0]).unwrap();
        let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        assert_eq!(restored.splits(), workspace.splits());
        assert_eq!(restored, workspace);
    }

    #[test]
    fn a_loaded_snapshot_drops_bad_splits_and_keeps_every_tab() {
        let mut workspace = Workspace::default();
        let other = workspace.create_space(DEFAULT_PROFILE_ID, "Work").unwrap();
        let ids = today(&mut workspace, 5);
        let away = workspace.add_temporary_item(other).unwrap();
        workspace
            .set_placement(
                ids[4],
                Placement::Pin {
                    space_id: DEFAULT_SPACE_ID,
                },
                Some(URL),
            )
            .unwrap();
        let mut snapshot = workspace.snapshot();
        let pair = |left, right| Split { left, right };
        snapshot.splits = vec![
            pair(ids[0], ids[1]),
            // Repeats an item of the first split.
            pair(ids[1], ids[2]),
            pair(ids[2], ids[2]),
            pair(ids[2], away),
            pair(ids[3], ids[4]),
            pair(ids[3], 999),
        ];
        let restored = Workspace::from_snapshot(snapshot).unwrap();
        assert_eq!(restored.splits(), &[pair(ids[0], ids[1])]);
        assert_eq!(restored.items().len(), 6);
    }

    #[test]
    fn a_loaded_snapshot_keeps_a_repeated_split_once() {
        let mut workspace = Workspace::default();
        let ids = today(&mut workspace, 2);
        let mut snapshot = workspace.snapshot();
        let split = Split {
            left: ids[0],
            right: ids[1],
        };
        snapshot.splits = vec![split; MAX_SPLITS + 5];
        let restored = Workspace::from_snapshot(snapshot).unwrap();
        assert_eq!(restored.splits(), &[split]);
    }
}
