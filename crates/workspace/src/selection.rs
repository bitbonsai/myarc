//! Selecting several sidebar rows at once, as in Arc.
//!
//! Cmd-click adds a row to the selection or takes it out, and Shift-click
//! selects every row from the anchor to the clicked one. The first Cmd-click
//! or Shift-click starts from the row on screen, so it joins the selection
//! too, as in Arc. A plain click ends the selection. Rows are the ones the
//! sidebar shows for the window's Space, top to bottom: Pins, each folder's
//! Pins while open, then Today. Favorites don't take part.
//!
//! Cmd+W closes the selection, Cmd+Shift+C copies its URLs, and a row's menu
//! acts on the selection when the row is in it. Without a selection those
//! commands act on one row, as before. The selection is per window, lives
//! only in memory and never names a row the sidebar no longer shows.

use crate::ItemId;

/// The rows selected in one window's sidebar, in row order, and the row a
/// Shift-click range starts from.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Selection {
    items: Vec<ItemId>,
    anchor: Option<ItemId>,
}

impl Selection {
    pub fn new() -> Self {
        Self::default()
    }

    /// A selection of `items` kept elsewhere, for example across a call
    /// from the browser. Call `retain` with the rows shown to put it back in
    /// row order.
    pub fn from_parts(items: &[ItemId], anchor: Option<ItemId>) -> Self {
        Self {
            items: items.to_vec(),
            anchor,
        }
    }

    /// The row a Shift-click range starts from.
    pub fn anchor(&self) -> Option<ItemId> {
        self.anchor
    }

    /// The selected rows, top to bottom.
    pub fn items(&self) -> &[ItemId] {
        &self.items
    }

    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    pub fn contains(&self, id: ItemId) -> bool {
        self.items.contains(&id)
    }

    /// Cmd-click on `clicked`, one of `order`, the rows shown top to bottom.
    /// `active` is the row on screen, which joins a new selection first. A row
    /// outside `order` changes nothing.
    pub fn toggle(&mut self, order: &[ItemId], active: Option<ItemId>, clicked: ItemId) {
        if !order.contains(&clicked) {
            return;
        }
        if self.items.is_empty()
            && let Some(active) = active.filter(|id| *id != clicked && order.contains(id))
        {
            self.items.push(active);
        }
        if self.contains(clicked) {
            self.items.retain(|id| *id != clicked);
        } else {
            self.items.push(clicked);
        }
        self.anchor = Some(clicked);
        self.sort(order);
    }

    /// Shift-click on `clicked`: selects every row of `order` from the anchor
    /// to `clicked`, both included, in place of the earlier selection. Without
    /// an anchor the range starts at `active`, or at `clicked` alone. The
    /// anchor stays, so a second Shift-click redraws the range from it.
    pub fn extend(&mut self, order: &[ItemId], active: Option<ItemId>, clicked: ItemId) {
        let Some(to) = order.iter().position(|id| *id == clicked) else {
            return;
        };
        let anchor = self
            .anchor
            .or(active)
            .filter(|id| order.contains(id))
            .unwrap_or(clicked);
        let from = order.iter().position(|id| *id == anchor).unwrap_or(to);
        let (first, last) = (from.min(to), from.max(to));
        self.items = order[first..=last].to_vec();
        self.anchor = Some(anchor);
    }

    /// Ends the selection. Returns whether there was one.
    pub fn clear(&mut self) -> bool {
        self.anchor = None;
        !std::mem::take(&mut self.items).is_empty()
    }

    /// Keeps only rows still in `order`, after the sidebar rebuilt, and drops
    /// the anchor once its row is gone. Returns whether the selection changed.
    pub fn retain(&mut self, order: &[ItemId]) -> bool {
        let before = self.items.clone();
        self.items.retain(|id| order.contains(id));
        self.sort(order);
        if self.anchor.is_some_and(|id| !order.contains(&id)) {
            self.anchor = None;
        }
        self.items != before
    }

    /// The rows a command from `row`'s menu applies to: the whole selection
    /// when `row` is in it, else `row` alone.
    pub fn menu_targets(&self, row: ItemId) -> Vec<ItemId> {
        if self.contains(row) {
            self.items.clone()
        } else {
            vec![row]
        }
    }

    fn sort(&mut self, order: &[ItemId]) {
        self.items
            .sort_by_key(|id| order.iter().position(|row| row == id).unwrap_or(usize::MAX));
        self.items.dedup();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const ORDER: &[ItemId] = &[10, 11, 12, 13, 14];

    #[test]
    fn first_cmd_click_selects_the_active_row_and_the_clicked_one() {
        let mut selection = Selection::new();
        selection.toggle(ORDER, Some(11), 13);
        assert_eq!(selection.items(), &[11, 13]);
    }

    #[test]
    fn cmd_click_on_the_active_row_alone_selects_just_it() {
        let mut selection = Selection::new();
        selection.toggle(ORDER, Some(11), 11);
        assert_eq!(selection.items(), &[11]);
    }

    #[test]
    fn cmd_click_takes_a_selected_row_out() {
        let mut selection = Selection::new();
        selection.toggle(ORDER, Some(10), 12);
        selection.toggle(ORDER, Some(10), 14);
        selection.toggle(ORDER, Some(10), 12);
        assert_eq!(selection.items(), &[10, 14]);
    }

    #[test]
    fn selection_stays_in_row_order() {
        let mut selection = Selection::new();
        selection.toggle(ORDER, None, 14);
        selection.toggle(ORDER, None, 10);
        selection.toggle(ORDER, None, 12);
        assert_eq!(selection.items(), &[10, 12, 14]);
    }

    #[test]
    fn active_row_outside_the_rows_is_not_added() {
        // A Favorite on screen has no row here.
        let mut selection = Selection::new();
        selection.toggle(ORDER, Some(99), 12);
        assert_eq!(selection.items(), &[12]);
    }

    #[test]
    fn click_on_an_unknown_row_changes_nothing() {
        let mut selection = Selection::new();
        selection.toggle(ORDER, Some(10), 99);
        selection.extend(ORDER, Some(10), 99);
        assert!(selection.is_empty());
    }

    #[test]
    fn shift_click_selects_from_the_active_row() {
        let mut selection = Selection::new();
        selection.extend(ORDER, Some(13), 11);
        assert_eq!(selection.items(), &[11, 12, 13]);
    }

    #[test]
    fn shift_click_without_an_active_row_selects_the_clicked_row() {
        let mut selection = Selection::new();
        selection.extend(ORDER, None, 12);
        assert_eq!(selection.items(), &[12]);
    }

    #[test]
    fn shift_click_ranges_from_the_last_cmd_click() {
        let mut selection = Selection::new();
        selection.toggle(ORDER, Some(10), 12);
        selection.extend(ORDER, Some(10), 14);
        assert_eq!(selection.items(), &[12, 13, 14]);
    }

    #[test]
    fn second_shift_click_redraws_the_range_from_the_same_anchor() {
        let mut selection = Selection::new();
        selection.extend(ORDER, Some(12), 14);
        selection.extend(ORDER, Some(14), 10);
        assert_eq!(selection.items(), &[10, 11, 12]);
    }

    #[test]
    fn clear_reports_whether_anything_was_selected() {
        let mut selection = Selection::new();
        assert!(!selection.clear());
        selection.toggle(ORDER, Some(10), 11);
        assert!(selection.clear());
        assert!(selection.is_empty());
        // The anchor went too, so a range starts at the active row again.
        selection.extend(ORDER, Some(14), 13);
        assert_eq!(selection.items(), &[13, 14]);
    }

    #[test]
    fn retain_drops_rows_that_left_and_their_anchor() {
        let mut selection = Selection::new();
        selection.toggle(ORDER, Some(10), 12);
        assert!(selection.retain(&[10, 11, 13, 14]));
        assert_eq!(selection.items(), &[10]);
        assert!(!selection.retain(&[10, 11, 13, 14]));
        selection.extend(&[10, 11, 13, 14], Some(13), 14);
        assert_eq!(selection.items(), &[13, 14]);
    }

    #[test]
    fn retain_follows_a_new_row_order() {
        let mut selection = Selection::new();
        selection.extend(ORDER, Some(10), 11);
        assert!(selection.retain(&[11, 10, 12]));
        assert_eq!(selection.items(), &[11, 10]);
    }

    #[test]
    fn from_parts_keeps_shown_rows_in_order() {
        let mut selection = Selection::from_parts(&[13, 99, 10], Some(99));
        selection.retain(ORDER);
        assert_eq!(selection.items(), &[10, 13]);
        assert_eq!(selection.anchor(), None);
    }

    #[test]
    fn menu_acts_on_the_selection_only_from_a_selected_row() {
        let mut selection = Selection::new();
        selection.extend(ORDER, Some(10), 12);
        assert_eq!(selection.menu_targets(11), vec![10, 11, 12]);
        assert_eq!(selection.menu_targets(14), vec![14]);
    }
}
