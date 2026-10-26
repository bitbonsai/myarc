//! Pure Auto Archive decision rule (BC-006). No clock reads, no live tabs, no I/O.
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

use crate::{ItemId, Placement};

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
