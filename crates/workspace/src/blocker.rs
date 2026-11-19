//! The per-site ad blocking switch. The bundled blocker, uBlock Origin Lite,
//! filters each host at one of four levels: 0 no filtering, 1 basic, 2
//! optimal and 3 complete. The Profile has a default level and a host may
//! override it. myArc shows one switch per site instead of the four steps.
//! The switch is on at any level that filters, and off at no filtering.
//! Turning it on puts the host back at the Profile's default level, or at
//! optimal when the default itself is no filtering, so on always filters.
//! The blocker keeps the levels; this module only maps between them and the
//! switch.

/// No filtering: the blocker leaves the site alone.
pub const LEVEL_NONE: i32 = 0;
/// The blocker's own default level.
pub const LEVEL_OPTIMAL: i32 = 2;
/// The highest level, complete.
pub const LEVEL_MAX: i32 = 3;

/// What the switch shows for a level the blocker reported.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SwitchState {
    /// The level isn't one the blocker uses, so the switch stays disabled.
    Unknown,
    Off,
    On,
}

/// The switch for a host the blocker filters at `level`.
pub fn switch_state(level: i32) -> SwitchState {
    match level {
        LEVEL_NONE => SwitchState::Off,
        1..=LEVEL_MAX => SwitchState::On,
        _ => SwitchState::Unknown,
    }
}

/// The level to set for a host when the switch goes `on`, given the
/// Profile's default level.
pub fn level_for(on: bool, default_level: i32) -> i32 {
    match (on, default_level) {
        (false, _) => LEVEL_NONE,
        (true, 1..=LEVEL_MAX) => default_level,
        (true, _) => LEVEL_OPTIMAL,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_filtering_level_shows_on() {
        assert_eq!(switch_state(0), SwitchState::Off);
        for level in 1..=3 {
            assert_eq!(switch_state(level), SwitchState::On);
        }
    }

    #[test]
    fn unknown_levels_disable_the_switch() {
        for level in [-1, 4, i32::MIN, i32::MAX] {
            assert_eq!(switch_state(level), SwitchState::Unknown);
        }
    }

    #[test]
    fn off_is_no_filtering_whatever_the_default() {
        for default_level in [-1, 0, 1, 2, 3, 9] {
            assert_eq!(level_for(false, default_level), LEVEL_NONE);
        }
    }

    #[test]
    fn on_follows_the_profile_default() {
        assert_eq!(level_for(true, 1), 1);
        assert_eq!(level_for(true, 2), 2);
        assert_eq!(level_for(true, 3), 3);
    }

    #[test]
    fn on_filters_even_when_the_default_does_not() {
        for default_level in [0, -1, 4] {
            assert_eq!(level_for(true, default_level), LEVEL_OPTIMAL);
            assert_eq!(
                switch_state(level_for(true, default_level)),
                SwitchState::On
            );
        }
    }

    #[test]
    fn a_round_trip_lands_where_it_started() {
        for default_level in 1..=3 {
            let off = level_for(false, default_level);
            assert_eq!(switch_state(off), SwitchState::Off);
            let on = level_for(true, default_level);
            assert_eq!(switch_state(on), SwitchState::On);
            assert_eq!(on, default_level);
        }
    }
}
