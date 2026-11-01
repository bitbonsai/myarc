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
//!
//! `Workspace::auto_archive` is the pass the browser runs on a timer and on tab
//! activation. Its clock is the snapshot's `open_secs`, which counts only time
//! the browser is open: each pass adds the monotonic time since the previous one,
//! capped at `MAX_PASS_GAP_SECS`, so a sleeping computer or a closed browser adds
//! nothing. It records views in each temporary item's `last_viewed_secs` against
//! that clock, both persist with the snapshot, then it applies the rule with the
//! snapshot's Profile interval (`archive_interval`, 12 hours by default).

use std::cmp::Reverse;
use std::collections::BTreeSet;
use std::ops::RangeInclusive;

use crate::{
    ARCHIVE_RETENTION_SECS, ArchivedTab, ItemId, MAX_ARCHIVED_TABS, MAX_TITLE_BYTES, Placement,
    Workspace, WorkspaceError, WorkspaceSnapshot, clean_title, validate_url,
};

const HOUR_SECS: u64 = 60 * 60;
const DAY_SECS: u64 = 24 * HOUR_SECS;

/// Little Arc windows archive after six hours whatever the Profile interval says.
pub const LITTLE_ARC_INTERVAL_SECS: u64 = 6 * HOUR_SECS;

/// Per-Profile archive interval, Arc's "Archive tabs after" choices. Default
/// is 12 hours. Arc offers no way to turn Auto Archive off.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum ArchiveInterval {
    #[default]
    Hours12,
    Hours24,
    Days7,
    Days30,
}

impl ArchiveInterval {
    /// Every choice, shortest first.
    pub const ALL: [Self; 4] = [Self::Hours12, Self::Hours24, Self::Days7, Self::Days30];

    /// Idle seconds before an item is due.
    pub const fn seconds(self) -> u64 {
        match self {
            Self::Hours12 => 12 * HOUR_SECS,
            Self::Hours24 => DAY_SECS,
            Self::Days7 => 7 * DAY_SECS,
            Self::Days30 => 30 * DAY_SECS,
        }
    }

    /// The choice that lasts exactly `secs`, if any.
    pub fn from_seconds(secs: u64) -> Option<Self> {
        Self::ALL
            .into_iter()
            .find(|interval| interval.seconds() == secs)
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ArchivePolicy {
    pub interval: ArchiveInterval,
    /// Little Arc window: fixed six-hour interval, overriding `interval`.
    pub little_arc: bool,
}

impl ArchivePolicy {
    /// Effective idle seconds.
    pub const fn interval_secs(self) -> u64 {
        if self.little_arc {
            LITTLE_ARC_INTERVAL_SECS
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
    let interval = policy.interval_secs();
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

/// Longest gap one pass may add to the browser-open clock. The browser passes
/// every 5 minutes while it runs; a longer gap means the computer slept or the
/// clock jumped, and that time does not count.
pub const MAX_PASS_GAP_SECS: u64 = 10 * 60;

/// What one `Workspace::auto_archive` pass found.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct AutoArchive {
    /// Temporary items to close now, in item order.
    pub due: Vec<ItemId>,
    /// Whether the clock or any `last_viewed_secs` changed, so the snapshot
    /// needs saving.
    pub changed: bool,
}

impl Workspace {
    /// One Auto Archive pass with the Profile's `archive_interval`,
    /// `elapsed_secs` after the previous pass by a monotonic clock (zero for the
    /// first pass of a launch).
    ///
    /// The browser-open clock (`open_secs`) advances by `elapsed_secs`, capped
    /// at `MAX_PASS_GAP_SECS`. It starts at 1, since zero marks an unrecorded
    /// view. Then views are recorded at the new reading: a temporary item
    /// selected in any window, listed in `viewed` (every visible tab, split
    /// views included), with no recorded view yet (a new item, or a file from an
    /// older build) or with a view ahead of the clock (a damaged or older file)
    /// gets the current reading, so nothing expires early. Then
    /// `decide_archive` picks the due items among `closable`: the items whose
    /// live pages the adapter can close and archive, plus dormant temporary rows
    /// (a remembered page, no live tab). `media` lists items playing or
    /// capturing media. Unknown IDs are ignored. Nothing is removed here: the
    /// adapter closes each due page, and closing archives it through
    /// `archive_closed_tab`; a due dormant row it archives directly with its
    /// remembered page.
    pub fn auto_archive(
        &mut self,
        viewed: &[ItemId],
        closable: &[ItemId],
        media: &[ItemId],
        elapsed_secs: u64,
    ) -> AutoArchive {
        let before = self.state.open_secs;
        let now = before
            .saturating_add(elapsed_secs.min(MAX_PASS_GAP_SECS))
            .max(1);
        self.state.open_secs = now;
        let mut changed = now != before;
        let mut seen: BTreeSet<ItemId> = viewed.iter().copied().collect();
        seen.extend(
            self.state
                .windows
                .iter()
                .filter_map(|window| window.selected_item_id),
        );
        for item in &mut self.state.items {
            let last = item.last_viewed_secs;
            if matches!(item.placement, Placement::Temporary { .. })
                && (last == 0 || last > now || seen.contains(&item.id))
                && last != now
            {
                item.last_viewed_secs = now;
                changed = true;
            }
        }
        let closable: BTreeSet<ItemId> = closable.iter().copied().collect();
        let media: BTreeSet<ItemId> = media.iter().copied().collect();
        let candidates: Vec<ArchiveCandidate> = self
            .state
            .items
            .iter()
            .filter(|item| closable.contains(&item.id))
            .map(|item| ArchiveCandidate {
                id: item.id,
                placement: item.placement,
                last_viewed_secs: item.last_viewed_secs,
                media_playing: media.contains(&item.id),
            })
            .collect();
        let seen: Vec<ItemId> = seen.into_iter().collect();
        let policy = ArchivePolicy {
            interval: self.state.archive_interval,
            little_arc: false,
        };
        AutoArchive {
            due: decide_archive(&candidates, &seen, policy, now).archive,
            changed,
        }
    }

    /// This Profile's "Archive tabs after" choice.
    pub fn archive_interval(&self) -> ArchiveInterval {
        self.state.archive_interval
    }

    /// Changes this Profile's interval. Views already recorded keep their
    /// times, so a shorter interval can make items due at the next pass.
    pub fn set_archive_interval(&mut self, interval: ArchiveInterval) {
        self.state.archive_interval = interval;
    }

    /// The last live page of `item_id` closed. A temporary item ends, like
    /// `live_tab_closed`, and is archived with `url`, `title` and `now_secs`.
    /// Returns whether it was archived: saved items stay and archive nothing, and
    /// a blank page or a URL the saved-URL gate rejects (such as
    /// `chrome://newtab`) is not kept. Off-the-record adapters, and adapters
    /// whose Profile saves no history, call `live_tab_closed` instead.
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
        if !archivable(url) {
            return Ok(false);
        }
        self.prune_archived_tabs(now_secs);
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

    /// Unexpired Archived Tabs at `now_secs`, newest first by close time. Ties,
    /// and entries from a clock that moved backwards, keep the newest-added first.
    pub fn archived_tabs(&self, now_secs: u64) -> Vec<ArchivedTab> {
        let mut tabs: Vec<ArchivedTab> = self
            .state
            .archived
            .iter()
            .rev()
            .filter(|tab| !expired(tab.archived_secs, now_secs))
            .cloned()
            .collect();
        tabs.sort_by_key(|tab| Reverse(tab.archived_secs));
        tabs
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

    /// Clear Archive: drops every entry, returning how many went.
    pub fn clear_archived_tabs(&mut self) -> usize {
        std::mem::take(&mut self.state.archived).len()
    }

    /// Drops entries expired at `now_secs`, returning how many went. The adapter
    /// calls it after a load, since entries otherwise expire only on the next
    /// archive.
    pub fn prune_archived_tabs(&mut self, now_secs: u64) -> usize {
        let before = self.state.archived.len();
        self.state
            .archived
            .retain(|tab| !expired(tab.archived_secs, now_secs));
        before - self.state.archived.len()
    }

    /// History deletion: drops entries whose URL is in `urls`, and with `range`
    /// those archived within it (inclusive seconds). Returns how many went.
    pub fn forget_archived_tabs(
        &mut self,
        urls: &[String],
        range: Option<RangeInclusive<u64>>,
    ) -> usize {
        let urls: BTreeSet<&str> = urls.iter().map(String::as_str).collect();
        let before = self.state.archived.len();
        self.state.archived.retain(|tab| {
            !urls.contains(tab.url.as_str())
                && !range
                    .as_ref()
                    .is_some_and(|range| range.contains(&tab.archived_secs))
        });
        before - self.state.archived.len()
    }

    /// Archived Tabs are a convenience, so a bad entry in a loaded snapshot is
    /// dropped rather than failing the whole workspace: reused, duplicate or zero
    /// IDs, a zero Space and URLs that could not have been archived. Titles are
    /// cleaned like new ones, and only the newest `MAX_ARCHIVED_TABS` stay.
    pub(crate) fn sanitize_archive(&mut self) {
        let WorkspaceSnapshot {
            archived,
            items,
            next_item_id,
            ..
        } = &mut self.state;
        let excess = archived.len().saturating_sub(MAX_ARCHIVED_TABS);
        archived.drain(..excess);
        let live: BTreeSet<ItemId> = items.iter().map(|item| item.id).collect();
        let mut seen = BTreeSet::new();
        archived.retain_mut(|tab| {
            if tab.title.len() > MAX_TITLE_BYTES || tab.title.chars().any(char::is_control) {
                tab.title = clean_title(&tab.title);
            }
            tab.id != 0
                && tab.id < *next_item_id
                && tab.space_id != 0
                && !live.contains(&tab.id)
                && archivable(&tab.url)
                && seen.insert(tab.id)
        });
    }
}

/// Blank pages carry nothing to reopen, so only HTTP(S) URLs are archived.
fn archivable(url: &str) -> bool {
    url != "about:blank" && validate_url(url).is_ok()
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
        for blank in ["about:blank", ""] {
            let page = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
            assert_eq!(workspace.archive_closed_tab(page, blank, "", 1), Ok(false));
        }
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
    fn clear_archive_drops_every_entry_and_persists() {
        let mut workspace = Workspace::default();
        close(&mut workspace, DEFAULT_SPACE_ID, 5);
        close(&mut workspace, DEFAULT_SPACE_ID, 6);
        let live = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        assert_eq!(workspace.clear_archived_tabs(), 2);
        assert!(workspace.archived_tabs(6).is_empty());
        assert_eq!(workspace.clear_archived_tabs(), 0);
        // Open tabs stay, and a reload brings nothing back.
        let restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        assert!(restored.snapshot().archived.is_empty());
        assert_eq!(restored.snapshot().items[0].id, live);
    }

    #[test]
    fn prune_drops_expired_entries_only() {
        let mut workspace = Workspace::default();
        close(&mut workspace, DEFAULT_SPACE_ID, 0);
        let kept = close(&mut workspace, DEFAULT_SPACE_ID, DAY);
        assert_eq!(workspace.prune_archived_tabs(30 * DAY), 0);
        assert_eq!(workspace.prune_archived_tabs(30 * DAY + 1), 1);
        let archived = workspace.snapshot().archived;
        assert_eq!(
            archived.iter().map(|tab| tab.id).collect::<Vec<_>>(),
            [kept]
        );
    }

    #[test]
    fn history_deletion_forgets_matching_entries() {
        let mut workspace = Workspace::default();
        let ids: Vec<_> = (1..=4)
            .map(|secs| close(&mut workspace, DEFAULT_SPACE_ID, secs * 10))
            .collect();
        let other = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace
            .archive_closed_tab(other, "https://other.test/", "", 50)
            .unwrap();
        let urls = [String::from("https://other.test/")];
        assert_eq!(workspace.forget_archived_tabs(&urls, None), 1);
        assert_eq!(workspace.forget_archived_tabs(&[], Some(20..=30)), 2);
        let left: Vec<_> = workspace
            .archived_tabs(50)
            .iter()
            .map(|tab| tab.id)
            .collect();
        assert_eq!(left, [ids[3], ids[0]]);
        assert_eq!(workspace.forget_archived_tabs(&[], Some(0..=u64::MAX)), 2);
        assert!(workspace.archived_tabs(50).is_empty());
    }

    #[test]
    fn entries_list_by_close_time_even_after_a_clock_change() {
        let mut workspace = Workspace::default();
        let late = close(&mut workspace, DEFAULT_SPACE_ID, 100);
        let early = close(&mut workspace, DEFAULT_SPACE_ID, 50);
        let tie = close(&mut workspace, DEFAULT_SPACE_ID, 100);
        let order: Vec<_> = workspace
            .archived_tabs(100)
            .iter()
            .map(|tab| tab.id)
            .collect();
        assert_eq!(order, [tie, late, early]);
    }

    #[test]
    fn snapshot_drops_bad_archive_entries_and_keeps_the_rest() {
        let mut workspace = Workspace::default();
        let good_id = close(&mut workspace, DEFAULT_SPACE_ID, 5);
        close(&mut workspace, DEFAULT_SPACE_ID, 6);
        let live = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let good = workspace.snapshot();
        for edit in [
            |tab: &mut ArchivedTab| tab.id = 0,
            |tab: &mut ArchivedTab| tab.id = 99,
            |tab: &mut ArchivedTab| tab.space_id = 0,
            |tab: &mut ArchivedTab| tab.url = String::from("ftp://example.test"),
            |tab: &mut ArchivedTab| tab.url = String::from("about:blank"),
        ] {
            let mut snapshot = good.clone();
            edit(&mut snapshot.archived[1]);
            let restored = Workspace::from_snapshot(snapshot).unwrap();
            let ids: Vec<_> = restored.archived_tabs(6).iter().map(|tab| tab.id).collect();
            assert_eq!(ids, [good_id]);
            assert_eq!(restored.snapshot().items.len(), 1);
        }
        let mut reused = good.clone();
        reused.archived[1].id = live;
        let mut duplicate = good.clone();
        duplicate.archived[1].id = good_id;
        for snapshot in [reused, duplicate] {
            let restored = Workspace::from_snapshot(snapshot).unwrap();
            assert_eq!(restored.snapshot().archived.len(), 1);
            assert_eq!(restored.snapshot().archived[0].id, good_id);
        }
        // Titles are cleaned like new ones rather than dropped.
        let mut titled = good.clone();
        titled.archived[0].title = String::from("a\nb");
        let restored = Workspace::from_snapshot(titled).unwrap();
        assert_eq!(restored.snapshot().archived[0].title, "a b");
        // A deleted Space is fine: the adapter reopens in the current Space.
        let mut gone = good;
        gone.archived[0].space_id = 7;
        assert_eq!(
            Workspace::from_snapshot(gone)
                .unwrap()
                .snapshot()
                .archived
                .len(),
            2
        );
    }
}

#[cfg(test)]
mod auto_archive_tests {
    use super::*;
    use crate::{DEFAULT_SPACE_ID, DEFAULT_WINDOW_ID};

    const H: u64 = HOUR_SECS;
    /// The browser's pass interval.
    const PASS: u64 = 5 * 60;

    /// Runs passes every 5 minutes for `secs` of open browser time, as the
    /// browser's timer does, and returns the last one.
    fn run(workspace: &mut Workspace, closable: &[ItemId], secs: u64) -> AutoArchive {
        let mut last = AutoArchive::default();
        let mut left = secs;
        while left > 0 {
            let step = left.min(PASS);
            last = workspace.auto_archive(&[], closable, &[], step);
            left -= step;
        }
        last
    }

    fn viewed(workspace: &Workspace, id: ItemId) -> u64 {
        workspace
            .snapshot()
            .items
            .iter()
            .find(|item| item.id == id)
            .unwrap()
            .last_viewed_secs
    }

    #[test]
    fn idle_temporary_items_are_due_after_twelve_open_hours() {
        let mut workspace = Workspace::default();
        let a = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let b = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        // The first pass starts the clock, records the unknown views and
        // archives nothing.
        let first = workspace.auto_archive(&[], &[a, b], &[], 0);
        assert_eq!(
            first,
            AutoArchive {
                due: vec![],
                changed: true
            }
        );
        assert_eq!(workspace.snapshot().open_secs, 1);
        assert_eq!(viewed(&workspace, a), 1);
        assert!(run(&mut workspace, &[a, b], 12 * H - 1).due.is_empty());
        assert_eq!(run(&mut workspace, &[a, b], 1).due, vec![a, b]);
        // The pass only decides; the adapter's close removes the items.
        assert_eq!(workspace.snapshot().items.len(), 2);
    }

    #[test]
    fn the_profile_interval_sets_when_items_are_due_and_persists() {
        let mut workspace = Workspace::default();
        assert_eq!(workspace.archive_interval(), ArchiveInterval::Hours12);
        workspace.set_archive_interval(ArchiveInterval::Days7);
        let id = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace.auto_archive(&[], &[id], &[], 0);
        assert!(run(&mut workspace, &[id], 24 * H).due.is_empty());
        let mut restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        assert_eq!(restored.archive_interval(), ArchiveInterval::Days7);
        assert!(run(&mut restored, &[id], 6 * 24 * H - 1).due.is_empty());
        assert_eq!(run(&mut restored, &[id], 1).due, vec![id]);
        // Going back to 12 hours makes a long-idle item due at the next pass.
        let other = restored.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        restored.set_archive_interval(ArchiveInterval::Days30);
        restored.auto_archive(&[], &[], &[], PASS);
        run(&mut restored, &[], 20 * H);
        restored.set_archive_interval(ArchiveInterval::Hours12);
        assert_eq!(
            restored.auto_archive(&[], &[other], &[], 0).due,
            vec![other]
        );
    }

    #[test]
    fn selected_and_visible_items_are_kept_and_their_views_recorded() {
        let mut workspace = Workspace::default();
        let a = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let b = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let split = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace.auto_archive(&[], &[], &[], 0);
        workspace.select_item(DEFAULT_WINDOW_ID, a).unwrap();
        run(&mut workspace, &[], 20 * H);
        // `split` shares the window with `a` in a split view.
        let pass = workspace.auto_archive(&[split], &[a, b, split], &[], PASS);
        assert_eq!(pass.due, vec![b]);
        let now = workspace.snapshot().open_secs;
        assert_eq!(viewed(&workspace, a), now);
        assert_eq!(viewed(&workspace, split), now);
        // Leaving it starts its 12 hours from the last recorded view.
        let other = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace.select_item(DEFAULT_WINDOW_ID, other).unwrap();
        assert!(run(&mut workspace, &[a], 12 * H - 1).due.is_empty());
        assert_eq!(viewed(&workspace, a), now);
        assert_eq!(run(&mut workspace, &[a], 1).due, vec![a]);
    }

    #[test]
    fn an_activation_records_a_view_between_passes() {
        let mut workspace = Workspace::default();
        let id = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace.auto_archive(&[], &[], &[], 0);
        run(&mut workspace, &[], 11 * H);
        // Viewed 2 minutes after a pass, with nothing closable.
        let view = workspace.auto_archive(&[id], &[], &[], 120);
        assert!(view.changed && view.due.is_empty());
        assert!(run(&mut workspace, &[id], 12 * H - 1).due.is_empty());
        assert_eq!(run(&mut workspace, &[id], 1).due, vec![id]);
    }

    #[test]
    fn saved_unclosable_and_media_items_are_kept() {
        let mut workspace = Workspace::default();
        let pin = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let chrome_page = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let playing = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace.auto_archive(&[], &[], &[], 0);
        workspace
            .set_placement(
                pin,
                Placement::Pin {
                    space_id: DEFAULT_SPACE_ID,
                },
                Some("https://example.test/"),
            )
            .unwrap();
        assert_eq!(viewed(&workspace, pin), 0);
        run(&mut workspace, &[], 30 * H);
        let pass = workspace.auto_archive(&[], &[pin, playing, 99], &[playing], PASS);
        assert!(pass.due.is_empty());
        // Saved items record no views.
        assert_eq!(viewed(&workspace, pin), 0);
        assert_eq!(viewed(&workspace, chrome_page), 1);
    }

    #[test]
    fn an_unsaved_item_starts_a_fresh_timer() {
        let mut workspace = Workspace::default();
        let id = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace.auto_archive(&[], &[id], &[], 0);
        workspace
            .set_placement(
                id,
                Placement::Pin {
                    space_id: DEFAULT_SPACE_ID,
                },
                Some("https://example.test/"),
            )
            .unwrap();
        run(&mut workspace, &[], 48 * H);
        workspace.unsave_item(id, Some(DEFAULT_SPACE_ID)).unwrap();
        assert!(workspace.auto_archive(&[], &[id], &[], 0).due.is_empty());
        assert_eq!(viewed(&workspace, id), workspace.snapshot().open_secs);
    }

    #[test]
    fn time_with_the_browser_closed_does_not_count() {
        let mut workspace = Workspace::default();
        let id = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace.auto_archive(&[], &[id], &[], 0);
        run(&mut workspace, &[id], 8 * H);
        // Quit on Friday, relaunch on Monday: the clock and the view persist,
        // and the first pass of a launch reports no elapsed time.
        let mut restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        assert_eq!(restored.snapshot().open_secs, 1 + 8 * H);
        assert_eq!(viewed(&restored, id), 1);
        assert!(restored.auto_archive(&[], &[id], &[], 0).due.is_empty());
        assert!(run(&mut restored, &[id], 4 * H - 1).due.is_empty());
        assert_eq!(run(&mut restored, &[id], 1).due, vec![id]);
    }

    #[test]
    fn a_sleeping_computer_adds_at_most_one_pass_gap() {
        let mut workspace = Workspace::default();
        let id = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace.auto_archive(&[], &[id], &[], 0);
        run(&mut workspace, &[id], 6 * H);
        // A weekend asleep, or a clock jump, arrives as one huge gap.
        let woke = workspace.auto_archive(&[], &[id], &[], 60 * H);
        assert!(woke.due.is_empty());
        assert_eq!(
            workspace.snapshot().open_secs,
            1 + 6 * H + MAX_PASS_GAP_SECS
        );
        assert!(
            run(&mut workspace, &[id], 6 * H - MAX_PASS_GAP_SECS - 1)
                .due
                .is_empty()
        );
        assert_eq!(run(&mut workspace, &[id], 1).due, vec![id]);
    }

    #[test]
    fn a_view_ahead_of_the_clock_is_clamped_to_now() {
        let mut workspace = Workspace::default();
        let id = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        let mut snapshot = workspace.snapshot();
        snapshot.open_secs = 100;
        // A wall-clock stamp from an older build, far ahead of this clock.
        snapshot.items[0].last_viewed_secs = 1_800_000_000;
        let mut restored = Workspace::from_snapshot(snapshot).unwrap();
        assert!(restored.auto_archive(&[], &[id], &[], 0).changed);
        assert_eq!(viewed(&restored, id), 100);
        assert!(run(&mut restored, &[id], 12 * H - 1).due.is_empty());
        assert_eq!(run(&mut restored, &[id], 1).due, vec![id]);
    }

    #[test]
    fn an_idle_dormant_row_is_due_and_archives_its_remembered_page() {
        let mut workspace = Workspace::default();
        let id = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
        workspace
            .set_temporary_page(id, "https://example.test/doc", "Doc")
            .unwrap();
        workspace.auto_archive(&[], &[], &[], 0);
        // Relaunch without its tab: the row is dormant and keeps its last view.
        let mut restored = Workspace::from_snapshot(workspace.snapshot()).unwrap();
        restored.reconcile_live_items(&[]).unwrap();
        assert!(run(&mut restored, &[id], 12 * H - 1).due.is_empty());
        assert_eq!(run(&mut restored, &[id], 1).due, vec![id]);
        // No page to close, so the adapter archives the row directly.
        assert_eq!(
            restored.archive_closed_tab(id, "https://example.test/doc", "Doc", 7),
            Ok(true)
        );
        assert!(restored.snapshot().items.is_empty());
        let archived = restored.archived_tabs(7);
        assert_eq!(archived.len(), 1);
        assert_eq!(archived[0].url, "https://example.test/doc");
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
        assert_eq!(ArchivePolicy::default().interval_secs(), 12 * H);
    }

    #[test]
    fn interval_options_map_to_seconds() {
        assert_eq!(ArchiveInterval::Hours12.seconds(), 12 * H);
        assert_eq!(ArchiveInterval::Hours24.seconds(), 24 * H);
        assert_eq!(ArchiveInterval::Days7.seconds(), 7 * 24 * H);
        assert_eq!(ArchiveInterval::Days30.seconds(), 30 * 24 * H);
        for interval in ArchiveInterval::ALL {
            assert_eq!(
                ArchiveInterval::from_seconds(interval.seconds()),
                Some(interval)
            );
        }
        assert_eq!(ArchiveInterval::from_seconds(0), None);
        assert_eq!(ArchiveInterval::from_seconds(6 * H), None);
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
        for interval in [ArchiveInterval::Hours12, ArchiveInterval::Days30] {
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
