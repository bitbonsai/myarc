//! Archived Tabs: the store of closed temporary pages, and the pure Auto Archive
//! decision rule (BC-006). No clock reads, no live tabs, no I/O.
//!
//! The store records a temporary item when its last live page closes, with the
//! URL and title the adapter read from that page. Saved items keep their identity
//! and are never archived. Entries expire after `ARCHIVE_RETENTION_SECS` and the
//! oldest go first past `MAX_ARCHIVED_TABS`. The store persists with the snapshot.
//!
//! The caller supplies `now_secs` and each item's `last_viewed_secs` from one
//! monotonic or wall clock, in whole seconds. The rule answers two questions: which
//! items to archive now, and when to wake up next. Callers run it again on every
//! wake-up and whenever selection, media state, placement or a view changes. Items
//! excluded for being selected or playing media add no deadline, so those events
//! must trigger a fresh call.
//!
//! One call covers one Profile policy. Callers filter items by Profile first.

use std::collections::BTreeSet;

use crate::{
    ARCHIVE_RETENTION_SECS, ArchivedTab, ItemId, MAX_ARCHIVED_TABS, MAX_TITLE_BYTES, Placement,
    Workspace, WorkspaceError, clean_title, validate_url,
};

const HOUR_SECS: u64 = 60 * 60;
const DAY_SECS: u64 = 24 * HOUR_SECS;

/// Little Arc windows archive after six hours whatever the Profile interval says.
pub const LITTLE_ARC_INTERVAL_SECS: u64 = 6 * HOUR_SECS;

/// Per-Profile archive interval. Default is 12 hours.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum ArchiveInterval {
    #[default]
    Hours12,
    Hours24,
    Days7,
    Days30,
    Never,
}

impl ArchiveInterval {
    /// Idle seconds before an item is due, or `None` when the Profile never archives.
    pub const fn seconds(self) -> Option<u64> {
        match self {
            Self::Hours12 => Some(12 * HOUR_SECS),
            Self::Hours24 => Some(DAY_SECS),
            Self::Days7 => Some(7 * DAY_SECS),
            Self::Days30 => Some(30 * DAY_SECS),
            Self::Never => None,
        }
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ArchivePolicy {
    pub interval: ArchiveInterval,
    /// Little Arc window: fixed six-hour interval, overriding `interval`.
    pub little_arc: bool,
}

impl ArchivePolicy {
    /// Effective idle seconds, or `None` when nothing archives.
    pub const fn interval_secs(self) -> Option<u64> {
        if self.little_arc {
            Some(LITTLE_ARC_INTERVAL_SECS)
        } else {
            self.interval.seconds()
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ArchiveCandidate {
    pub id: ItemId,
    pub placement: Placement,
    /// Last time any window showed this item. Viewing resets the timer.
    pub last_viewed_secs: u64,
    pub media_playing: bool,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ArchiveDecision {
    /// Items to archive now, in input order.
    pub archive: Vec<ItemId>,
    /// Earliest future deadline among eligible items, always greater than `now_secs`.
    /// `None` when no eligible item is waiting; schedule no timer.
    pub next_wake_secs: Option<u64>,
}

/// Decide which items to archive at `now_secs` and when to look again.
///
/// Only `Placement::Temporary` items qualify, so Favorites, Pins and any future
/// placement never archive until this rule names them. Items in `selected` (one per
/// window, or any superset) and items playing media never archive. An item is due
/// when `now_secs >= last_viewed_secs + interval`. A clock that moved backwards
/// (`now_secs < last_viewed_secs`) means not yet due. Arithmetic saturates.
/// Work is linear in `items` plus `selected`; the result never exceeds `items`.
pub fn decide_archive(
    items: &[ArchiveCandidate],
    selected: &[ItemId],
    policy: ArchivePolicy,
    now_secs: u64,
) -> ArchiveDecision {
    let mut decision = ArchiveDecision::default();
    let Some(interval) = policy.interval_secs() else {
        return decision;
    };
    let selected: BTreeSet<ItemId> = selected.iter().copied().collect();
    for item in items {
        if !matches!(item.placement, Placement::Temporary { .. })
            || item.media_playing
            || selected.contains(&item.id)
        {
            continue;
        }
        let deadline = item.last_viewed_secs.saturating_add(interval);
        if now_secs >= item.last_viewed_secs && now_secs >= deadline {
            decision.archive.push(item.id);
        } else {
            decision.next_wake_secs = Some(match decision.next_wake_secs {
                Some(earliest) => earliest.min(deadline),
                None => deadline,
            });
        }
    }
    decision
}

impl Workspace {
    /// The last live page of `item_id` closed. A temporary item ends, like
    /// `live_tab_closed`, and is archived with `url`, `title` and `now_secs`.
    /// Returns whether it was archived: saved items stay and archive nothing, and
    /// a URL the saved-URL gate rejects (such as `chrome://newtab`) is not kept.
    /// Off-the-record adapters call `live_tab_closed` instead.
    pub fn archive_closed_tab(
        &mut self,
        item_id: ItemId,
        url: &str,
        title: &str,
        now_secs: u64,
    ) -> Result<bool, WorkspaceError> {
        let Placement::Temporary { space_id } = self.item(item_id)?.placement else {
            return Ok(false);
        };
        self.remove_item(item_id);
        if validate_url(url).is_err() {
            return Ok(false);
        }
        self.state
            .archived
            .retain(|tab| !expired(tab.archived_secs, now_secs));
        if self.state.archived.len() >= MAX_ARCHIVED_TABS {
            let excess = self.state.archived.len() + 1 - MAX_ARCHIVED_TABS;
            self.state.archived.drain(..excess);
        }
        self.state.archived.push(ArchivedTab {
            id: item_id,
            space_id,
            url: url.to_owned(),
            title: clean_title(title),
            archived_secs: now_secs,
        });
        Ok(true)
    }

    /// Unexpired Archived Tabs at `now_secs`, newest first.
    pub fn archived_tabs(&self, now_secs: u64) -> Vec<ArchivedTab> {
        self.state
            .archived
            .iter()
            .rev()
            .filter(|tab| !expired(tab.archived_secs, now_secs))
            .cloned()
            .collect()
    }

    /// Remove an entry once the adapter has reopened it, returning it.
    pub fn remove_archived_tab(&mut self, id: ItemId) -> Result<ArchivedTab, WorkspaceError> {
        let index = self
            .state
            .archived
            .iter()
            .position(|tab| tab.id == id)
            .ok_or(WorkspaceError::UnknownArchivedTab(id))?;
        Ok(self.state.archived.remove(index))
    }

    pub(crate) fn validate_archive(&self) -> Result<(), WorkspaceError> {
        let state = &self.state;
        let mut seen = BTreeSet::new();
        for tab in &state.archived {
            if tab.id == 0
                || tab.id >= state.next_item_id
                || tab.space_id == 0
                || !seen.insert(tab.id)
                || state.items.iter().any(|item| item.id == tab.id)
                || tab.title.len() > MAX_TITLE_BYTES
                || tab.title.chars().any(char::is_control)
            {
                return Err(WorkspaceError::InvalidSnapshot(
                    "archived tab has a reused or invalid ID, Space or title",
                ));
            }
            validate_url(&tab.url)?;
        }
        Ok(())
    }
}

/// A clock that moved backwards keeps the entry.
fn expired(archived_secs: u64, now_secs: u64) -> bool {
    now_secs > archived_secs.saturating_add(ARCHIVE_RETENTION_SECS)
}

#[cfg(test)]
mod store_tests {
    use super::*;
    use crate::{DEFAULT_PROFILE_ID, DEFAULT_SPACE_ID};

    const URL: &str = "https://example.test/a";
    const DAY: u64 = 24 * 60 * 60;

    fn close(workspace: &mut Workspace, space: u64, now: u64) -> ItemId {
        let id = workspace.add_temporary_item(space).unwrap();
        assert_eq!(
            workspace.archive_closed_tab(id, URL, "Page\nTitle", now),
            Ok(true)
        );
        id
    }

    #[test]
    fn closing_a_temporary_item_archives_it_newest_first() {
        let mut workspace = Workspace::default();
        let other = workspace.create_space(DEFAULT_PROFILE_ID, "Two").unwrap();
        let first = close(&mut workspace, DEFAULT_SPACE_ID, 10);
        let second = close(&mut workspace, other, 20);
        assert!(workspace.snapshot().items.is_empty());
        let tabs = workspace.archived_tabs(30);
        assert_eq!(
            tabs.iter().map(|tab| tab.id).collect::<Vec<_>>(),
            vec![second, first]
        );
        assert_eq!(tabs[0].space_id, other);
        assert_eq!(tabs[0].title, "Page Title");
        assert_eq!(tabs[0].archived_secs, 20);
        let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        assert_eq!(restored.archived_tabs(30), tabs);
    }

    #[test]
    fn saved_items_and_unarchivable_urls_are_not_kept() {
        let mut workspace = Workspace::default();
        let pin = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace
            .set_placement(
                pin,
                Placement::Pin {
                    space_id: DEFAULT_SPACE_ID,
                },
                Some(URL),
            )
            .unwrap();
        assert_eq!(workspace.archive_closed_tab(pin, URL, "Pin", 1), Ok(false));
        assert_eq!(workspace.snapshot().items.len(), 1);
        let ntp = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        assert_eq!(
            workspace.archive_closed_tab(ntp, "chrome://newtab/", "New Tab", 1),
            Ok(false)
        );
        assert_eq!(workspace.snapshot().items.len(), 1);
        assert!(workspace.archived_tabs(1).is_empty());
        assert_eq!(
            workspace.archive_closed_tab(99, URL, "", 1),
            Err(WorkspaceError::UnknownItem(99))
        );
    }

    #[test]
    fn archive_expires_after_thirty_days_and_caps_its_size() {
        let mut workspace = Workspace::default();
        let old = close(&mut workspace, DEFAULT_SPACE_ID, 0);
        assert_eq!(workspace.archived_tabs(30 * DAY).len(), 1);
        assert!(workspace.archived_tabs(30 * DAY + 1).is_empty());
        close(&mut workspace, DEFAULT_SPACE_ID, 31 * DAY);
        assert!(
            workspace
                .snapshot()
                .archived
                .iter()
                .all(|tab| tab.id != old)
        );

        for _ in 0..MAX_ARCHIVED_TABS + 5 {
            close(&mut workspace, DEFAULT_SPACE_ID, 31 * DAY);
        }
        let archived = workspace.snapshot().archived;
        assert_eq!(archived.len(), MAX_ARCHIVED_TABS);
        assert_eq!(
            archived.last().map(|tab| tab.id),
            workspace.archived_tabs(31 * DAY).first().map(|tab| tab.id)
        );
    }

    #[test]
    fn removing_an_entry_returns_it_once() {
        let mut workspace = Workspace::default();
        let id = close(&mut workspace, DEFAULT_SPACE_ID, 5);
        let tab = workspace.remove_archived_tab(id).unwrap();
        assert_eq!((tab.id, tab.url.as_str()), (id, URL));
        assert_eq!(
            workspace.remove_archived_tab(id),
            Err(WorkspaceError::UnknownArchivedTab(id))
        );
    }

    #[test]
    fn snapshot_rejects_bad_archive_entries() {
        let mut workspace = Workspace::default();
        close(&mut workspace, DEFAULT_SPACE_ID, 5);
        let live = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let good = workspace.snapshot();
        let mut cases = Vec::new();
        for edit in [
            |tab: &mut ArchivedTab| tab.id = 0,
            |tab: &mut ArchivedTab| tab.id = 99,
            |tab: &mut ArchivedTab| tab.space_id = 0,
            |tab: &mut ArchivedTab| tab.title = String::from("a\nb"),
            |tab: &mut ArchivedTab| tab.url = String::from("ftp://example.test"),
        ] {
            let mut snapshot = good.clone();
            edit(&mut snapshot.archived[0]);
            cases.push(snapshot);
        }
        let mut reused = good.clone();
        reused.archived[0].id = live;
        cases.push(reused);
        let mut duplicate = good.clone();
        duplicate.archived.push(duplicate.archived[0].clone());
        cases.push(duplicate);
        for snapshot in cases {
            assert!(Workspace::from_snapshot(snapshot).is_err());
        }
        // A deleted Space is fine: the adapter reopens in the current Space.
        let mut gone = good;
        gone.archived[0].space_id = 7;
        assert!(Workspace::from_snapshot(gone).is_ok());
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const H: u64 = HOUR_SECS;

    fn temp(id: ItemId, last_viewed_secs: u64) -> ArchiveCandidate {
        ArchiveCandidate {
            id,
            placement: Placement::Temporary { space_id: 1 },
            last_viewed_secs,
            media_playing: false,
        }
    }

    fn decide(items: &[ArchiveCandidate], now: u64) -> ArchiveDecision {
        decide_archive(items, &[], ArchivePolicy::default(), now)
    }

    #[test]
    fn default_interval_is_twelve_hours() {
        assert_eq!(ArchivePolicy::default().interval_secs(), Some(12 * H));
    }

    #[test]
    fn interval_options_map_to_seconds() {
        assert_eq!(ArchiveInterval::Hours12.seconds(), Some(12 * H));
        assert_eq!(ArchiveInterval::Hours24.seconds(), Some(24 * H));
        assert_eq!(ArchiveInterval::Days7.seconds(), Some(7 * 24 * H));
        assert_eq!(ArchiveInterval::Days30.seconds(), Some(30 * 24 * H));
        assert_eq!(ArchiveInterval::Never.seconds(), None);
    }

    #[test]
    fn empty_input_decides_nothing() {
        assert_eq!(decide(&[], 1_000), ArchiveDecision::default());
    }

    #[test]
    fn archives_exactly_at_deadline_not_before() {
        let items = [temp(1, 100)];
        let before = decide(&items, 100 + 12 * H - 1);
        assert!(before.archive.is_empty());
        assert_eq!(before.next_wake_secs, Some(100 + 12 * H));
        let at = decide(&items, 100 + 12 * H);
        assert_eq!(at.archive, vec![1]);
        assert_eq!(at.next_wake_secs, None);
    }

    #[test]
    fn viewing_resets_the_timer() {
        let stale = [temp(1, 0)];
        assert_eq!(decide(&stale, 12 * H).archive, vec![1]);
        let viewed = [temp(1, 11 * H)];
        let decision = decide(&viewed, 12 * H);
        assert!(decision.archive.is_empty());
        assert_eq!(decision.next_wake_secs, Some(23 * H));
    }

    #[test]
    fn favorites_and_pins_never_archive() {
        let items = [
            ArchiveCandidate {
                placement: Placement::Favorite { profile_id: 1 },
                ..temp(1, 0)
            },
            ArchiveCandidate {
                placement: Placement::Pin { space_id: 1 },
                ..temp(2, 0)
            },
        ];
        assert_eq!(decide(&items, 365 * 24 * H), ArchiveDecision::default());
    }

    #[test]
    fn media_playing_never_archives_and_adds_no_deadline() {
        let items = [ArchiveCandidate {
            media_playing: true,
            ..temp(1, 0)
        }];
        assert_eq!(decide(&items, 100 * 24 * H), ArchiveDecision::default());
    }

    #[test]
    fn selected_item_never_archives_in_any_window() {
        let items = [temp(1, 0), temp(2, 0), temp(3, 0)];
        let decision = decide_archive(&items, &[1, 3], ArchivePolicy::default(), 12 * H);
        assert_eq!(decision.archive, vec![2]);
        assert_eq!(decision.next_wake_secs, None);
    }

    #[test]
    fn selection_of_unknown_item_is_ignored() {
        let items = [temp(1, 0)];
        let decision = decide_archive(&items, &[99], ArchivePolicy::default(), 12 * H);
        assert_eq!(decision.archive, vec![1]);
    }

    #[test]
    fn never_interval_archives_nothing() {
        let policy = ArchivePolicy {
            interval: ArchiveInterval::Never,
            little_arc: false,
        };
        let decision = decide_archive(&[temp(1, 0)], &[], policy, u64::MAX);
        assert_eq!(decision, ArchiveDecision::default());
    }

    #[test]
    fn longer_intervals_delay_archiving() {
        let items = [temp(1, 0)];
        for (interval, secs) in [
            (ArchiveInterval::Hours24, 24 * H),
            (ArchiveInterval::Days7, 7 * 24 * H),
            (ArchiveInterval::Days30, 30 * 24 * H),
        ] {
            let policy = ArchivePolicy {
                interval,
                little_arc: false,
            };
            let early = decide_archive(&items, &[], policy, secs - 1);
            assert!(early.archive.is_empty());
            assert_eq!(early.next_wake_secs, Some(secs));
            assert_eq!(decide_archive(&items, &[], policy, secs).archive, vec![1]);
        }
    }

    #[test]
    fn little_arc_uses_six_hours_over_profile_interval() {
        for interval in [
            ArchiveInterval::Hours12,
            ArchiveInterval::Days30,
            ArchiveInterval::Never,
        ] {
            let policy = ArchivePolicy {
                interval,
                little_arc: true,
            };
            let items = [temp(1, 0)];
            assert!(
                decide_archive(&items, &[], policy, 6 * H - 1)
                    .archive
                    .is_empty()
            );
            assert_eq!(decide_archive(&items, &[], policy, 6 * H).archive, vec![1]);
        }
    }

    #[test]
    fn clock_going_backwards_is_not_yet_due() {
        let items = [temp(1, 50 * H)];
        let decision = decide(&items, 10 * H);
        assert!(decision.archive.is_empty());
        assert_eq!(decision.next_wake_secs, Some(62 * H));
    }

    #[test]
    fn last_viewed_at_u64_max_with_clock_at_zero_is_not_due() {
        let items = [temp(1, u64::MAX)];
        let decision = decide(&items, 0);
        assert!(decision.archive.is_empty());
        assert_eq!(decision.next_wake_secs, Some(u64::MAX));
    }

    #[test]
    fn overflowing_deadline_saturates() {
        let items = [temp(1, u64::MAX - 5)];
        let waiting = decide(&items, u64::MAX - 1);
        assert!(waiting.archive.is_empty());
        assert_eq!(waiting.next_wake_secs, Some(u64::MAX));
        assert_eq!(decide(&items, u64::MAX).archive, vec![1]);
    }

    #[test]
    fn next_wake_is_earliest_deadline_and_after_now() {
        let items = [temp(1, 5 * H), temp(2, 2 * H), temp(3, 0), temp(4, 9 * H)];
        let decision = decide(&items, 12 * H);
        assert_eq!(decision.archive, vec![3]);
        assert_eq!(decision.next_wake_secs, Some(14 * H));
        assert!(decision.next_wake_secs > Some(12 * H));
    }

    #[test]
    fn archive_list_keeps_input_order() {
        let items = [temp(7, 0), temp(3, 0), temp(5, 0)];
        assert_eq!(decide(&items, 12 * H).archive, vec![7, 3, 5]);
    }

    #[test]
    fn huge_input_is_linear_and_bounded() {
        let items: Vec<ArchiveCandidate> = (0..200_000).map(|id| temp(id, id)).collect();
        let selected: Vec<ItemId> = (0..1_000).collect();
        let now = 12 * H + 100_000;
        let decision = decide_archive(&items, &selected, ArchivePolicy::default(), now);
        // Due: ids 0..=100_000; the first 1_000 are selected.
        assert_eq!(decision.archive.len(), 100_001 - 1_000);
        assert_eq!(decision.archive.first(), Some(&1_000));
        assert_eq!(decision.next_wake_secs, Some(100_001 + 12 * H));
    }
}
