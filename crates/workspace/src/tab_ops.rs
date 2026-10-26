//! Pure tab operations behind two Arc shortcuts: Clear Unpinned (Cmd+Shift+K, BC-007) and
//! the recent-tab switcher (Ctrl+Tab, BC-008). No clock, no live pages, no I/O.
//!
//! Neither rule mutates the workspace. Callers close or select the returned ids through
//! the usual mutations and keep native page lifetimes themselves.

use std::collections::BTreeSet;

use crate::{ItemId, MAX_ITEMS, Placement, SpaceId};

/// One item of the current Space as the window shows it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ClearCandidate {
    pub id: ItemId,
    pub placement: Placement,
    pub media_playing: bool,
    /// Selected in the window that triggered the command.
    pub selected: bool,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ClearDecision {
    /// Items to close or archive, in input order, each id once.
    pub close: Vec<ItemId>,
    /// The item that stays selected. `None` means nothing is selected afterwards: either
    /// nothing was selected before or the selected item was closed. The adapter then
    /// shows its empty state, as `clear_hidden_selections` does for removed items.
    pub selected_after: Option<ItemId>,
}

/// Decide what Clear Unpinned closes in `space_id`.
///
/// Only `Placement::Temporary` items owned by `space_id` close, so Favorites, Pins and
/// Temporary items of other Spaces never do. Items playing audio or media stay, selected
/// or not. The selected item stays selected unless it closes. Work is linear in
/// `items`; the result never exceeds `items`.
pub fn clear_unpinned(space_id: SpaceId, items: &[ClearCandidate]) -> ClearDecision {
    let mut closing = BTreeSet::new();
    let mut decision = ClearDecision::default();
    for item in items {
        let owned = matches!(
            item.placement,
            Placement::Temporary { space_id: owner } if owner == space_id
        );
        if owned && !item.media_playing && closing.insert(item.id) {
            decision.close.push(item.id);
        }
    }
    decision.selected_after = items
        .iter()
        .find(|item| item.selected && !closing.contains(&item.id))
        .map(|item| item.id);
    decision
}

/// The switcher reaches the five most recent tabs.
pub const SWITCHER_DEPTH: usize = 5;

/// Most-recently-used item ids for one window, newest first. Bounded by `MAX_ITEMS`;
/// the oldest ids fall off. Holds ids only, never pages.
///
/// A switch is `cycle` presses followed by one `touch` of the chosen id when the user
/// commits. The adapter must not `touch` while the switcher is open, or the order
/// changes under the cursor. `touch` and `remove` end any cycle in progress.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct RecentTabs {
    order: Vec<ItemId>,
    cursor: usize,
}

impl RecentTabs {
    pub fn new() -> Self {
        Self::default()
    }

    /// Ids newest first.
    pub fn recent(&self) -> &[ItemId] {
        &self.order
    }

    pub fn len(&self) -> usize {
        self.order.len()
    }

    pub fn is_empty(&self) -> bool {
        self.order.is_empty()
    }

    /// Mark `id` as just shown: move it to the front, adding it when unknown.
    /// Ends any cycle in progress.
    pub fn touch(&mut self, id: ItemId) {
        self.order.retain(|known| *known != id);
        self.order.insert(0, id);
        self.order.truncate(MAX_ITEMS);
        self.cursor = 0;
    }

    /// Forget `id` (the item closed or archived). Unknown ids do nothing.
    pub fn remove(&mut self, id: ItemId) {
        let before = self.order.len();
        self.order.retain(|known| *known != id);
        if self.order.len() != before {
            self.cursor = 0;
        }
    }

    /// Move the switcher `step` places through the five most recent ids and return the
    /// target. `1` goes to the next older tab, `-1` the other way; both wrap inside the
    /// window. The first `cycle(1)` after a `touch` returns the previous tab, so a single
    /// press toggles between the two latest. `None` when fewer than two ids are known,
    /// since there is nothing to switch to. A `step` of zero returns the current target.
    pub fn cycle(&mut self, step: i32) -> Option<ItemId> {
        let window = self.order.len().min(SWITCHER_DEPTH);
        if window < 2 {
            return None;
        }
        // `window` is at most five, so the casts cannot overflow.
        let next = (self.cursor as i64 + i64::from(step)).rem_euclid(window as i64);
        self.cursor = next as usize;
        self.order.get(self.cursor).copied()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn candidate(id: ItemId, placement: Placement) -> ClearCandidate {
        ClearCandidate {
            id,
            placement,
            media_playing: false,
            selected: false,
        }
    }

    fn temp(id: ItemId) -> ClearCandidate {
        candidate(id, Placement::Temporary { space_id: 1 })
    }

    fn pinned(id: ItemId) -> ClearCandidate {
        candidate(id, Placement::Pin { space_id: 1 })
    }

    fn favorite(id: ItemId) -> ClearCandidate {
        candidate(id, Placement::Favorite { profile_id: 1 })
    }

    fn playing(item: ClearCandidate) -> ClearCandidate {
        ClearCandidate {
            media_playing: true,
            ..item
        }
    }

    fn selected(item: ClearCandidate) -> ClearCandidate {
        ClearCandidate {
            selected: true,
            ..item
        }
    }

    #[test]
    fn clear_empty_input_decides_nothing() {
        assert_eq!(clear_unpinned(1, &[]), ClearDecision::default());
    }

    #[test]
    fn clear_closes_only_temporary_items_in_input_order() {
        let items = [temp(4), pinned(2), favorite(3), temp(1)];
        let decision = clear_unpinned(1, &items);
        assert_eq!(decision.close, vec![4, 1]);
    }

    #[test]
    fn clear_all_pinned_or_favorite_closes_nothing() {
        let items = [pinned(1), favorite(2), pinned(3)];
        assert_eq!(clear_unpinned(1, &items), ClearDecision::default());
    }

    #[test]
    fn clear_skips_items_playing_media() {
        let items = [temp(1), playing(temp(2)), temp(3)];
        assert_eq!(clear_unpinned(1, &items).close, vec![1, 3]);
    }

    #[test]
    fn clear_all_playing_closes_nothing() {
        let items = [playing(temp(1)), playing(temp(2))];
        assert!(clear_unpinned(1, &items).close.is_empty());
    }

    #[test]
    fn clear_ignores_temporary_items_of_other_spaces() {
        let items = [
            temp(1),
            candidate(2, Placement::Temporary { space_id: 2 }),
            candidate(3, Placement::Temporary { space_id: 0 }),
        ];
        assert_eq!(clear_unpinned(1, &items).close, vec![1]);
        assert_eq!(clear_unpinned(2, &items).close, vec![2]);
    }

    #[test]
    fn clear_keeps_pinned_selection() {
        let items = [selected(pinned(1)), temp(2)];
        let decision = clear_unpinned(1, &items);
        assert_eq!(decision.close, vec![2]);
        assert_eq!(decision.selected_after, Some(1));
    }

    #[test]
    fn clear_keeps_selected_item_that_plays_media() {
        let items = [selected(playing(temp(1))), temp(2)];
        let decision = clear_unpinned(1, &items);
        assert_eq!(decision.close, vec![2]);
        assert_eq!(decision.selected_after, Some(1));
    }

    #[test]
    fn clear_selected_temporary_item_closes_and_selection_empties() {
        let items = [pinned(1), selected(temp(2)), temp(3)];
        let decision = clear_unpinned(1, &items);
        assert_eq!(decision.close, vec![2, 3]);
        assert_eq!(decision.selected_after, None);
    }

    #[test]
    fn clear_without_selection_selects_nothing() {
        let items = [pinned(1), temp(2)];
        assert_eq!(clear_unpinned(1, &items).selected_after, None);
    }

    #[test]
    fn clear_unknown_selection_cannot_appear() {
        // Selection comes from the items themselves, so an id outside them is impossible.
        let items = [temp(1)];
        assert_eq!(clear_unpinned(1, &items).selected_after, None);
    }

    #[test]
    fn clear_duplicate_ids_close_once() {
        let items = [temp(1), temp(1), temp(2), temp(1)];
        assert_eq!(clear_unpinned(1, &items).close, vec![1, 2]);
    }

    #[test]
    fn clear_selected_duplicate_of_closing_id_still_counts_as_closed() {
        let items = [selected(temp(1)), temp(1)];
        let decision = clear_unpinned(1, &items);
        assert_eq!(decision.close, vec![1]);
        assert_eq!(decision.selected_after, None);
    }

    #[test]
    fn clear_huge_input_is_linear_and_bounded() {
        let items: Vec<ClearCandidate> = (0..200_000).map(temp).collect();
        let decision = clear_unpinned(1, &items);
        assert_eq!(decision.close.len(), 200_000);
        assert_eq!(decision.close.first(), Some(&0));
    }

    fn recents(ids: &[ItemId]) -> RecentTabs {
        let mut tabs = RecentTabs::new();
        // Touch oldest first so the last id is the most recent.
        for id in ids.iter().rev() {
            tabs.touch(*id);
        }
        tabs
    }

    #[test]
    fn recent_starts_empty_and_cycle_returns_none() {
        let mut tabs = RecentTabs::new();
        assert!(tabs.is_empty());
        assert_eq!(tabs.len(), 0);
        assert_eq!(tabs.cycle(1), None);
        assert_eq!(tabs.cycle(-1), None);
    }

    #[test]
    fn recent_single_entry_has_nothing_to_switch_to() {
        let mut tabs = recents(&[7]);
        assert_eq!(tabs.cycle(1), None);
        assert_eq!(tabs.cycle(-1), None);
        assert_eq!(tabs.recent(), &[7]);
    }

    #[test]
    fn touch_moves_to_front_and_dedupes() {
        let mut tabs = RecentTabs::new();
        for id in [1, 2, 3, 2, 2] {
            tabs.touch(id);
        }
        assert_eq!(tabs.recent(), &[2, 3, 1]);
    }

    #[test]
    fn touch_accepts_unknown_ids() {
        let mut tabs = recents(&[1]);
        tabs.touch(u64::MAX);
        assert_eq!(tabs.recent(), &[u64::MAX, 1]);
    }

    #[test]
    fn single_press_toggles_between_two_latest() {
        let mut tabs = recents(&[1, 2, 3]);
        assert_eq!(tabs.cycle(1), Some(2));
        tabs.touch(2);
        assert_eq!(tabs.recent(), &[2, 1, 3]);
        assert_eq!(tabs.cycle(1), Some(1));
    }

    #[test]
    fn cycle_walks_older_and_wraps_inside_five() {
        let mut tabs = recents(&[1, 2, 3, 4, 5, 6, 7]);
        let walked: Vec<Option<ItemId>> = (0..6).map(|_| tabs.cycle(1)).collect();
        assert_eq!(
            walked,
            vec![Some(2), Some(3), Some(4), Some(5), Some(1), Some(2)]
        );
    }

    #[test]
    fn items_beyond_five_are_unreachable() {
        let mut tabs = recents(&[1, 2, 3, 4, 5, 6, 7]);
        for _ in 0..20 {
            let target = tabs.cycle(1);
            assert!(matches!(target, Some(1..=5)));
        }
        assert_eq!(tabs.len(), 7);
    }

    #[test]
    fn negative_step_goes_to_oldest_of_window_first() {
        let mut tabs = recents(&[1, 2, 3, 4, 5, 6]);
        assert_eq!(tabs.cycle(-1), Some(5));
        assert_eq!(tabs.cycle(-1), Some(4));
    }

    #[test]
    fn window_shrinks_with_few_entries() {
        let mut tabs = recents(&[1, 2, 3]);
        let walked: Vec<Option<ItemId>> = (0..4).map(|_| tabs.cycle(1)).collect();
        assert_eq!(walked, vec![Some(2), Some(3), Some(1), Some(2)]);
    }

    #[test]
    fn step_zero_returns_current_target() {
        let mut tabs = recents(&[1, 2, 3]);
        assert_eq!(tabs.cycle(0), Some(1));
        tabs.cycle(1);
        assert_eq!(tabs.cycle(0), Some(2));
    }

    #[test]
    fn extreme_steps_never_panic() {
        let mut tabs = recents(&[1, 2, 3, 4, 5]);
        for step in [i32::MIN, i32::MAX, -1, 1, 0] {
            assert!(tabs.cycle(step).is_some());
        }
    }

    #[test]
    fn touch_ends_a_cycle() {
        let mut tabs = recents(&[1, 2, 3]);
        tabs.cycle(1);
        tabs.cycle(1);
        tabs.touch(3);
        assert_eq!(tabs.recent(), &[3, 1, 2]);
        assert_eq!(tabs.cycle(1), Some(1));
    }

    #[test]
    fn remove_drops_the_id_and_resets_the_cursor() {
        let mut tabs = recents(&[1, 2, 3, 4]);
        tabs.cycle(1);
        tabs.cycle(1);
        tabs.remove(3);
        assert_eq!(tabs.recent(), &[1, 2, 4]);
        assert_eq!(tabs.cycle(1), Some(2));
    }

    #[test]
    fn remove_unknown_id_changes_nothing() {
        let mut tabs = recents(&[1, 2, 3]);
        tabs.cycle(1);
        tabs.remove(99);
        assert_eq!(tabs.recent(), &[1, 2, 3]);
        assert_eq!(tabs.cycle(1), Some(3));
    }

    #[test]
    fn remove_down_to_one_entry_disables_cycling() {
        let mut tabs = recents(&[1, 2]);
        tabs.remove(2);
        assert_eq!(tabs.cycle(1), None);
        tabs.remove(1);
        assert!(tabs.is_empty());
        assert_eq!(tabs.cycle(1), None);
    }

    #[test]
    fn list_is_bounded_and_drops_the_oldest() {
        let mut tabs = RecentTabs::new();
        let total = MAX_ITEMS as u64 + 10;
        for id in 0..total {
            tabs.touch(id);
        }
        assert_eq!(tabs.len(), MAX_ITEMS);
        assert_eq!(tabs.recent().first(), Some(&(total - 1)));
        assert_eq!(tabs.recent().last(), Some(&10));
        // The newest five stay reachable.
        assert_eq!(tabs.cycle(1), Some(total - 2));
    }
}
