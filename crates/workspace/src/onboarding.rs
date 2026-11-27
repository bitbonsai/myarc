//! Welcome tour rules (docs/ONBOARDING.md, .plans/2026-11-19-welcome-tour.md).
//!
//! Pure: takes the saved tour record and one event and returns the next record plus
//! the effect the browser should run. Holds no timers and calls nothing. The caller
//! saves the returned record before it runs the effect, so a choice made just before
//! a kill is there on relaunch.
//!
//! The record is untrusted text from a Local State pref. Anything that fails to decode
//! reads as `Complete`, so a damaged file never traps someone in the tour.

use crate::{MAX_SPACES, SpaceId};

/// Record format version `encode` writes. A newer version reads as unreadable.
pub const TOUR_VERSION: u32 = 1;
/// Practice steps between the reveal and the handoff.
pub const PRACTICE_STEPS: u8 = 3;
/// Largest record `decode` reads, in bytes. A full record is well under this.
pub const MAX_RECORD_BYTES: usize = 2048;
/// Longest import receipt hash, in lowercase hex characters.
pub const MAX_HASH_BYTES: usize = 128;

/// Where the tour is.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TourState {
    Welcome,
    /// The import dialog is open. Never resumed: the dialog closes with its window.
    Importing,
    Reveal,
    /// Practice step 1 to `PRACTICE_STEPS`.
    Practice(u8),
    Handoff,
    Complete,
}

/// What an import committed, saved before the reveal starts so resume never offers
/// the same export again.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ImportReceipt {
    /// Lowercase hex hash of the export's content.
    pub content_hash: String,
    /// Spaces the commit created or merged into.
    pub space_ids: Vec<SpaceId>,
}

impl ImportReceipt {
    fn valid(&self) -> bool {
        !self.content_hash.is_empty()
            && self.content_hash.len() <= MAX_HASH_BYTES
            && self
                .content_hash
                .bytes()
                .all(|byte| byte.is_ascii_digit() || (b'a'..=b'f').contains(&byte))
            && self.space_ids.len() <= MAX_SPACES
            && self.space_ids.iter().all(|id| *id != 0)
    }
}

/// The saved tour, one per install.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct TourRecord {
    pub state: TourState,
    /// Bit n-1 set when practice step n was done.
    done: u8,
    /// Bit n-1 set when practice step n was skipped. A skipped step is not done.
    skipped: u8,
    pub receipt: Option<ImportReceipt>,
}

impl TourRecord {
    /// A tour at the welcome, as a fresh workspace starts it.
    pub fn fresh() -> Self {
        Self::at(TourState::Welcome)
    }

    /// A finished tour, as an existing workspace or an unreadable record gets.
    pub fn complete() -> Self {
        Self::at(TourState::Complete)
    }

    fn at(state: TourState) -> Self {
        Self {
            state,
            done: 0,
            skipped: 0,
            receipt: None,
        }
    }

    pub fn is_done(&self, step: u8) -> bool {
        valid_step(step) && self.done & bit(step) != 0
    }

    pub fn is_skipped(&self, step: u8) -> bool {
        valid_step(step) && self.skipped & bit(step) != 0
    }

    /// The welcome offers Import from Arc… only while nothing was imported. After a
    /// replay it shows as already imported instead.
    pub fn offers_import(&self) -> bool {
        self.receipt.is_none()
    }

    /// The Space whose theme the welcome's colour field crossfades to once an import
    /// commits: the first Space the commit reported. The window also switches to it
    /// before the reveal, so the reveal shows the imported rows rather than an empty
    /// default Space.
    pub fn theme_space(&self) -> Option<SpaceId> {
        self.receipt
            .as_ref()
            .and_then(|receipt| receipt.space_ids.first().copied())
    }

    /// True when Back changes the tour, so the coach mark shows a Back button. From
    /// the reveal that holds only while nothing was imported.
    pub fn offers_back(&self) -> bool {
        apply(self, TourEvent::Back).record.state != self.state
    }

    /// True when an export with this hash was already imported through the tour.
    pub fn imported(&self, content_hash: &str) -> bool {
        self.receipt
            .as_ref()
            .is_some_and(|receipt| receipt.content_hash == content_hash)
    }
}

fn valid_step(step: u8) -> bool {
    (1..=PRACTICE_STEPS).contains(&step)
}

fn bit(step: u8) -> u8 {
    1 << (step - 1)
}

/// Something that happened in the browser.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum TourEvent {
    StartBrowsing,
    ImportStarted,
    ImportCommitted(ImportReceipt),
    ImportCancelled,
    ImportFailed,
    /// The reveal finished, so the first coach mark can show.
    RevealDone,
    StepDone(u8),
    SkipStep(u8),
    Back,
    SkipTour,
    Finish,
    Replay,
    WindowClosed,
    Relaunched,
}

/// What the browser runs after it saved the new record.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TourEffect {
    /// Nothing changes on screen.
    None,
    /// Show the new state: the welcome sheet, the reveal, a coach mark, the handoff,
    /// or nothing once complete. `animate` is false on resume, so a relaunch never
    /// replays the intro or the reveal.
    Show { animate: bool },
    /// Run the existing Import from Arc… dialog.
    OpenImport,
    /// An import committed and its receipt is saved: crossfade the welcome's colour
    /// field to `TourRecord::theme_space`'s theme, then show the reveal.
    Imported,
    /// The tour was skipped and is already saved complete; fade out through the
    /// handoff once.
    Handoff,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Transition {
    pub record: TourRecord,
    pub effect: TourEffect,
}

/// The tour for a Profile whose workspace just loaded. `saved` is the pref's text, or
/// `None` when no record exists. `first_run` means the load found no workspace file
/// and no backup. Only a fresh workspace with no record starts the tour; an existing
/// workspace with no record gets `Complete`.
pub fn on_load(saved: Option<&str>, first_run: bool) -> Transition {
    match saved {
        None if first_run => Transition {
            record: TourRecord::fresh(),
            effect: TourEffect::Show { animate: true },
        },
        None => Transition {
            record: TourRecord::complete(),
            effect: TourEffect::None,
        },
        Some(text) => apply(&decode(text), TourEvent::Relaunched),
    }
}

/// The next record and effect for `event`. An event that doesn't apply in the
/// current state returns the record unchanged with `TourEffect::None`.
pub fn apply(record: &TourRecord, event: TourEvent) -> Transition {
    use TourState::*;
    let mut next = record.clone();
    let shown = TourEffect::Show { animate: true };
    let effect = match (record.state, event) {
        (_, TourEvent::Replay) => {
            next.state = Welcome;
            next.done = 0;
            next.skipped = 0;
            shown
        }
        (Complete, _) => TourEffect::None,
        (_, TourEvent::SkipTour) => {
            next.state = Complete;
            TourEffect::Handoff
        }
        (Welcome, TourEvent::StartBrowsing) => {
            next.state = Reveal;
            shown
        }
        (Welcome, TourEvent::ImportStarted) if record.offers_import() => {
            next.state = Importing;
            TourEffect::OpenImport
        }
        (Importing, TourEvent::ImportCommitted(receipt)) => {
            if receipt.valid() {
                next.receipt = Some(receipt);
            }
            next.state = Reveal;
            TourEffect::Imported
        }
        (Importing, TourEvent::ImportCancelled | TourEvent::ImportFailed) => {
            next.state = Welcome;
            TourEffect::Show { animate: false }
        }
        // The dialog closes with its window, and its Cancel changes nothing.
        (Importing, TourEvent::WindowClosed) => {
            next.state = Welcome;
            TourEffect::None
        }
        (Importing, TourEvent::Relaunched) => {
            next.state = Welcome;
            TourEffect::Show { animate: false }
        }
        (Reveal, TourEvent::RevealDone) => {
            next.state = Practice(1);
            shown
        }
        (Practice(step), TourEvent::StepDone(n)) if n == step => {
            next.done |= bit(step);
            next.skipped &= !bit(step);
            next.state = after(step);
            shown
        }
        (Practice(step), TourEvent::SkipStep(n)) if n == step => {
            next.skipped |= bit(step);
            next.done &= !bit(step);
            next.state = after(step);
            shown
        }
        (Practice(1), TourEvent::Back) => {
            next.state = Reveal;
            TourEffect::Show { animate: false }
        }
        (Practice(step), TourEvent::Back) => {
            next.state = Practice(step - 1);
            shown
        }
        (Reveal, TourEvent::Back) if record.receipt.is_none() => {
            next.state = Welcome;
            TourEffect::Show { animate: false }
        }
        (Handoff, TourEvent::Back) => {
            next.state = Practice(PRACTICE_STEPS);
            shown
        }
        (Handoff, TourEvent::Finish) => {
            next.state = Complete;
            shown
        }
        // A reveal cut short resumes at practice 1 with no animation.
        (Reveal, TourEvent::Relaunched) => {
            next.state = Practice(1);
            TourEffect::Show { animate: false }
        }
        (_, TourEvent::Relaunched) => TourEffect::Show { animate: false },
        _ => TourEffect::None,
    };
    Transition {
        record: next,
        effect,
    }
}

fn after(step: u8) -> TourState {
    if step < PRACTICE_STEPS {
        TourState::Practice(step + 1)
    } else {
        TourState::Handoff
    }
}

/// The tab practice 2 points at, as the window sees it: the tab practice 1 opened
/// while it's still open, otherwise the window's current tab, otherwise another
/// Today tab with a web page.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PinTab {
    /// No web tab open at all.
    Missing,
    /// The current tab isn't a web page, but another Today tab is. The card points
    /// at that tab's row.
    Other,
    /// A Today tab with a web page.
    Today,
    /// Already a Pin or a Favorite.
    Saved,
}

/// What practice 2 shows, or the event it feeds instead.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PinPractice {
    /// "Press Cmd+D", pointing at the Today tab.
    Pin,
    /// The tab is pinned already and the card says so, with Next. Only Back gets
    /// here, so Back from practice 3 doesn't bounce straight forward again.
    Pinned,
    /// Feed `step_done(2)`: the tab got pinned, or was pinned already.
    Done,
    /// Feed `skip_step(2)`: there's no web tab to pin.
    Skip,
    /// Feed `back`: Back landed here with no web tab, so it goes on to practice 1.
    Back,
}

/// Practice 2 for `tab`. `shown` is what the card shows now, or `None` as the step
/// starts; `by_back` says Back brought the tour here.
pub fn pin_practice(shown: Option<PinPractice>, tab: PinTab, by_back: bool) -> PinPractice {
    match (shown, tab) {
        // Another tab to pin keeps the step going, whatever the card showed.
        (_, PinTab::Other) => PinPractice::Pin,
        (None, PinTab::Today) => PinPractice::Pin,
        (None, PinTab::Saved) if by_back => PinPractice::Pinned,
        (None, PinTab::Saved) => PinPractice::Done,
        (None, PinTab::Missing) if by_back => PinPractice::Back,
        (_, PinTab::Missing) => PinPractice::Skip,
        // The pin the card asked for.
        (Some(PinPractice::Pin), PinTab::Saved) => PinPractice::Done,
        // Unpinned again under the "pinned" card: ask for the pin.
        (Some(PinPractice::Pinned), PinTab::Today) => PinPractice::Pin,
        (Some(shown), _) => shown,
    }
}

/// Which third practice step a workspace with `space_count` Spaces gets.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SpacePractice {
    /// "Press Ctrl+`key`", pointing at the footer icons. `key` names a Space
    /// other than the current one, so the press always switches.
    Switch { key: u8 },
    /// Create a Space, pointing at the "+" button.
    Create,
}

/// The third practice step for `space_count` Spaces, the current one at index
/// `current` in footer order.
pub fn space_practice(space_count: usize, current: usize) -> SpacePractice {
    if space_count < 2 {
        SpacePractice::Create
    } else if current == 0 {
        SpacePractice::Switch { key: 2 }
    } else {
        SpacePractice::Switch { key: 1 }
    }
}

/// True once the window shows another Space, or the Profile has more Spaces, than
/// when practice 3 showed. Either finishes the step, whichever card it showed.
pub fn space_step_done(start: (SpaceId, usize), now: (SpaceId, usize)) -> bool {
    now.0 != start.0 || now.1 > start.1
}

/// The state's name as `encode` writes it.
/// What the end of the tour does about the quick start page (myarc.dev/welcome/).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum QuickStart {
    /// The tour didn't end here, or the page isn't pending: change nothing.
    Keep,
    /// Open the page as the first Today tab, then clear the pending flag.
    Open,
    /// The tour ended after an import: clear the flag without opening the page.
    Drop,
}

/// The quick start rule, for one tour transition from `before` to `after`.
/// `pending` is the workspace's `quick_start_pending`, true only for a workspace
/// created fresh. The welcome folder and page open once, right as the user
/// leaves Welcome by Start fresh or Skip tour (before the tour steps start);
/// an import's receipt means the Arc setup is there, so it is dropped instead.
pub fn quick_start(before: &TourRecord, after: &TourRecord, pending: bool) -> QuickStart {
    let left_welcome = matches!(before.state, TourState::Welcome | TourState::Importing)
        && !matches!(after.state, TourState::Welcome | TourState::Importing);
    if !pending || !left_welcome {
        QuickStart::Keep
    } else if after.receipt.is_some() {
        QuickStart::Drop
    } else {
        QuickStart::Open
    }
}

pub fn state_name(state: TourState) -> &'static str {
    match state {
        TourState::Welcome => "welcome",
        TourState::Importing => "importing",
        TourState::Reveal => "reveal",
        TourState::Practice(_) => "practice",
        TourState::Handoff => "handoff",
        TourState::Complete => "complete",
    }
}

fn steps(bits: u8) -> String {
    (1..=PRACTICE_STEPS)
        .filter(|step| bits & bit(*step) != 0)
        .map(|step| step.to_string())
        .collect::<Vec<_>>()
        .join(",")
}

/// The record as the compact JSON the pref stores.
pub fn encode(record: &TourRecord) -> String {
    let step = match record.state {
        TourState::Practice(step) => step,
        _ => 0,
    };
    let receipt = match &record.receipt {
        None => "null".to_owned(),
        Some(receipt) => format!(
            "{{\"hash\":\"{}\",\"spaces\":[{}]}}",
            receipt.content_hash,
            receipt
                .space_ids
                .iter()
                .map(u64::to_string)
                .collect::<Vec<_>>()
                .join(",")
        ),
    };
    format!(
        "{{\"v\":{TOUR_VERSION},\"state\":\"{}\",\"step\":{step},\"done\":[{}],\"skipped\":[{}],\"import\":{receipt}}}",
        state_name(record.state),
        steps(record.done),
        steps(record.skipped),
    )
}

/// Reads a record `encode` wrote. Anything else, including a newer version, reads as
/// `Complete`.
pub fn decode(text: &str) -> TourRecord {
    if text.len() > MAX_RECORD_BYTES {
        return TourRecord::complete();
    }
    let decoded = parse(text);
    // Only the canonical form is accepted, so a hand edit can't smuggle in a record
    // encode would never write.
    match decoded {
        Some(record) if encode(&record) == text => record,
        _ => TourRecord::complete(),
    }
}

struct Cursor<'a>(&'a str);

impl<'a> Cursor<'a> {
    fn eat(&mut self, literal: &str) -> Option<()> {
        self.0 = self.0.strip_prefix(literal)?;
        Some(())
    }

    fn take_while(&mut self, keep: impl Fn(char) -> bool) -> &'a str {
        let end = self.0.find(|c| !keep(c)).unwrap_or(self.0.len());
        let (head, tail) = self.0.split_at(end);
        self.0 = tail;
        head
    }

    fn number(&mut self) -> Option<u64> {
        let digits = self.take_while(|c| c.is_ascii_digit());
        if digits.is_empty() || digits.len() > 20 {
            return None;
        }
        digits.parse().ok()
    }

    fn numbers(&mut self, max: usize) -> Option<Vec<u64>> {
        self.eat("[")?;
        let mut out = Vec::new();
        if self.eat("]").is_some() {
            return Some(out);
        }
        loop {
            if out.len() >= max {
                return None;
            }
            out.push(self.number()?);
            if self.eat("]").is_some() {
                return Some(out);
            }
            self.eat(",")?;
        }
    }

    fn steps(&mut self) -> Option<u8> {
        let mut bits = 0;
        for step in self.numbers(PRACTICE_STEPS as usize)? {
            let step = u8::try_from(step).ok().filter(|s| valid_step(*s))?;
            bits |= bit(step);
        }
        Some(bits)
    }
}

fn parse(text: &str) -> Option<TourRecord> {
    let mut cursor = Cursor(text);
    cursor.eat("{\"v\":")?;
    if cursor.number()? != u64::from(TOUR_VERSION) {
        return None;
    }
    cursor.eat(",\"state\":\"")?;
    let name = cursor.take_while(|c| c.is_ascii_lowercase());
    cursor.eat("\",\"step\":")?;
    let step = u8::try_from(cursor.number()?).ok()?;
    let state = match (name, step) {
        ("welcome", 0) => TourState::Welcome,
        ("importing", 0) => TourState::Importing,
        ("reveal", 0) => TourState::Reveal,
        ("practice", step) if valid_step(step) => TourState::Practice(step),
        ("handoff", 0) => TourState::Handoff,
        ("complete", 0) => TourState::Complete,
        _ => return None,
    };
    cursor.eat(",\"done\":")?;
    let done = cursor.steps()?;
    cursor.eat(",\"skipped\":")?;
    let skipped = cursor.steps()?;
    if done & skipped != 0 {
        return None;
    }
    cursor.eat(",\"import\":")?;
    let receipt = if cursor.eat("null").is_some() {
        None
    } else {
        cursor.eat("{\"hash\":\"")?;
        let content_hash = cursor.take_while(|c| c.is_ascii_alphanumeric()).to_owned();
        cursor.eat("\",\"spaces\":")?;
        let space_ids = cursor.numbers(MAX_SPACES)?;
        cursor.eat("}")?;
        let receipt = ImportReceipt {
            content_hash,
            space_ids,
        };
        if !receipt.valid() {
            return None;
        }
        Some(receipt)
    };
    cursor.eat("}")?;
    if !cursor.0.is_empty() {
        return None;
    }
    Some(TourRecord {
        state,
        done,
        skipped,
        receipt,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use TourState::*;

    fn receipt() -> ImportReceipt {
        ImportReceipt {
            content_hash: "ab12".to_owned(),
            space_ids: vec![5, 6],
        }
    }

    fn run(record: &TourRecord, events: impl IntoIterator<Item = TourEvent>) -> TourRecord {
        events
            .into_iter()
            .fold(record.clone(), |record, event| apply(&record, event).record)
    }

    fn at(state: TourState) -> TourRecord {
        TourRecord::at(state)
    }

    // Saving encodes the record and a relaunch decodes it, as the pref does.
    fn relaunch(record: &TourRecord) -> Transition {
        on_load(Some(&encode(record)), false)
    }

    #[test]
    fn fresh_workspace_starts_at_welcome_with_the_intro() {
        let started = on_load(None, true);
        assert_eq!(started.record, TourRecord::fresh());
        assert_eq!(started.effect, TourEffect::Show { animate: true });
    }

    #[test]
    fn existing_workspace_without_a_record_skips_the_tour() {
        let decided = on_load(None, false);
        assert_eq!(decided.record.state, Complete);
        assert_eq!(decided.effect, TourEffect::None);
    }

    #[test]
    fn saved_record_wins_over_first_run() {
        // A second Profile with a fresh workspace doesn't restart a finished tour.
        let decided = on_load(Some(&encode(&TourRecord::complete())), true);
        assert_eq!(decided.record.state, Complete);
        assert_eq!(decided.effect, TourEffect::None);
    }

    #[test]
    fn walks_every_state_in_order() {
        let mut record = TourRecord::fresh();
        let path = [
            (TourEvent::StartBrowsing, Reveal),
            (TourEvent::RevealDone, Practice(1)),
            (TourEvent::StepDone(1), Practice(2)),
            (TourEvent::StepDone(2), Practice(3)),
            (TourEvent::StepDone(3), Handoff),
            (TourEvent::Finish, Complete),
        ];
        for (event, state) in path {
            let next = apply(&record, event.clone());
            assert_eq!(next.record.state, state, "{event:?}");
            assert_eq!(next.effect, TourEffect::Show { animate: true });
            record = next.record;
        }
        assert!((1..=3).all(|step| record.is_done(step)));
    }

    #[test]
    fn events_that_do_not_apply_change_nothing() {
        let cases = [
            (at(Welcome), TourEvent::StepDone(1)),
            (at(Welcome), TourEvent::Finish),
            (at(Welcome), TourEvent::Back),
            (at(Reveal), TourEvent::StepDone(1)),
            (at(Practice(2)), TourEvent::StepDone(1)),
            (at(Practice(2)), TourEvent::SkipStep(3)),
            (at(Practice(1)), TourEvent::ImportStarted),
            (at(Complete), TourEvent::StartBrowsing),
            (at(Complete), TourEvent::SkipTour),
        ];
        for (record, event) in cases {
            let next = apply(&record, event.clone());
            assert_eq!(next.record, record, "{event:?}");
            assert_eq!(next.effect, TourEffect::None, "{event:?}");
        }
    }

    #[test]
    fn skip_tour_saves_complete_from_any_state_and_relaunch_shows_nothing() {
        for state in [
            Welcome,
            Importing,
            Reveal,
            Practice(1),
            Practice(2),
            Practice(3),
            Handoff,
        ] {
            let skipped = apply(&at(state), TourEvent::SkipTour);
            assert_eq!(skipped.record.state, Complete, "{state:?}");
            assert_eq!(skipped.effect, TourEffect::Handoff, "{state:?}");
            let resumed = relaunch(&skipped.record);
            assert_eq!(resumed.record.state, Complete);
            assert_eq!(resumed.effect, TourEffect::None);
        }
    }

    #[test]
    fn skip_step_advances_without_counting_the_step_done() {
        let record = run(
            &at(Practice(1)),
            [TourEvent::SkipStep(1), TourEvent::StepDone(2)],
        );
        assert_eq!(record.state, Practice(3));
        assert!(record.is_skipped(1) && !record.is_done(1));
        assert!(record.is_done(2) && !record.is_skipped(2));
        let record = apply(&record, TourEvent::SkipStep(3)).record;
        assert_eq!(record.state, Handoff);
    }

    #[test]
    fn back_steps_through_practice_to_reveal_and_welcome() {
        let record = run(
            &TourRecord::fresh(),
            [
                TourEvent::StartBrowsing,
                TourEvent::RevealDone,
                TourEvent::StepDone(1),
                TourEvent::SkipStep(2),
                TourEvent::StepDone(3),
            ],
        );
        assert_eq!(record.state, Handoff);
        let mut record = record;
        for state in [Practice(3), Practice(2), Practice(1), Reveal, Welcome] {
            record = apply(&record, TourEvent::Back).record;
            assert_eq!(record.state, state);
            // Back keeps done and skipped steps as they were.
            assert!(record.is_done(1) && record.is_skipped(2) && record.is_done(3));
        }
    }

    #[test]
    fn back_from_reveal_stays_once_something_was_imported() {
        let record = run(
            &TourRecord::fresh(),
            [
                TourEvent::ImportStarted,
                TourEvent::ImportCommitted(receipt()),
            ],
        );
        assert_eq!(record.state, Reveal);
        let back = apply(&record, TourEvent::Back);
        assert_eq!(back.record, record);
        assert_eq!(back.effect, TourEffect::None);
    }

    #[test]
    fn back_is_offered_exactly_where_it_changes_the_state() {
        let imported = TourRecord {
            receipt: Some(receipt()),
            ..at(Reveal)
        };
        for (record, offered) in [
            (at(Welcome), false),
            (at(Importing), false),
            (at(Reveal), true),
            (imported, false),
            (at(Practice(1)), true),
            (at(Practice(3)), true),
            (at(Handoff), true),
            (at(Complete), false),
        ] {
            assert_eq!(record.offers_back(), offered, "{record:?}");
            let back = apply(&record, TourEvent::Back);
            assert_eq!(back.effect != TourEffect::None, offered, "{record:?}");
        }
    }

    #[test]
    fn practice_one_backs_to_the_reveal_end_state_and_continues() {
        // Back shows the reveal as it ended, with no replay, and its coach mark's
        // Next moves on to practice 1 again.
        let back = apply(&at(Practice(1)), TourEvent::Back);
        assert_eq!(back.record.state, Reveal);
        assert_eq!(back.effect, TourEffect::Show { animate: false });
        let next = apply(&back.record, TourEvent::RevealDone);
        assert_eq!(next.record.state, Practice(1));
        // Opening a page from the command bar finishes practice 1 and moves on.
        let done = apply(&next.record, TourEvent::StepDone(1));
        assert_eq!(done.record.state, Practice(2));
        assert!(done.record.is_done(1));
        assert_eq!(done.effect, TourEffect::Show { animate: true });
        // A command bar page during another step changes nothing.
        let late = apply(&done.record, TourEvent::StepDone(1));
        assert_eq!(late.record, done.record);
        assert_eq!(late.effect, TourEffect::None);
    }

    #[test]
    fn window_closed_mid_flow_keeps_the_step() {
        for state in [Welcome, Reveal, Practice(2), Handoff] {
            let closed = apply(&at(state), TourEvent::WindowClosed);
            assert_eq!(closed.record.state, state);
            assert_eq!(closed.effect, TourEffect::None);
        }
    }

    #[test]
    fn window_closed_during_import_saves_welcome() {
        let closed = apply(&at(Importing), TourEvent::WindowClosed);
        assert_eq!(closed.record.state, Welcome);
        assert!(closed.record.offers_import());
    }

    #[test]
    fn relaunch_resumes_at_the_step_without_animation() {
        let record = run(
            &at(Practice(2)),
            [TourEvent::SkipStep(2), TourEvent::WindowClosed],
        );
        let resumed = relaunch(&record);
        assert_eq!(resumed.record, record);
        assert_eq!(resumed.record.state, Practice(3));
        assert_eq!(resumed.effect, TourEffect::Show { animate: false });
        // The welcome resumes too, without its intro.
        let resumed = relaunch(&TourRecord::fresh());
        assert_eq!(resumed.record.state, Welcome);
        assert_eq!(resumed.effect, TourEffect::Show { animate: false });
    }

    #[test]
    fn relaunch_during_reveal_resumes_at_practice_one() {
        let resumed = relaunch(&at(Reveal));
        assert_eq!(resumed.record.state, Practice(1));
        assert_eq!(resumed.effect, TourEffect::Show { animate: false });
    }

    #[test]
    fn relaunch_during_import_goes_back_to_welcome() {
        let resumed = relaunch(&at(Importing));
        assert_eq!(resumed.record.state, Welcome);
        assert_eq!(resumed.effect, TourEffect::Show { animate: false });
    }

    #[test]
    fn import_cancel_or_failure_returns_to_welcome_unchanged() {
        for event in [TourEvent::ImportCancelled, TourEvent::ImportFailed] {
            let record = run(&TourRecord::fresh(), [TourEvent::ImportStarted, event]);
            assert_eq!(record, TourRecord::fresh());
        }
    }

    #[test]
    fn import_saves_its_receipt_before_the_reveal() {
        let started = apply(&TourRecord::fresh(), TourEvent::ImportStarted);
        assert_eq!(started.effect, TourEffect::OpenImport);
        let committed = apply(&started.record, TourEvent::ImportCommitted(receipt()));
        assert_eq!(committed.record.state, Reveal);
        assert_eq!(committed.record.receipt, Some(receipt()));
        assert_eq!(committed.effect, TourEffect::Imported);
        // A kill right after the commit keeps the receipt.
        let resumed = relaunch(&committed.record).record;
        assert_eq!(resumed.receipt, Some(receipt()));
        assert!(resumed.imported("ab12"));
        assert!(!resumed.imported("cd34"));
    }

    #[test]
    fn no_duplicate_import_after_resume_or_replay() {
        let imported = run(
            &TourRecord::fresh(),
            [
                TourEvent::ImportStarted,
                TourEvent::ImportCommitted(receipt()),
            ],
        );
        for record in [
            relaunch(&imported).record,
            apply(&imported, TourEvent::Replay).record,
        ] {
            assert!(!record.offers_import());
            let again = run(&record, [TourEvent::Back, TourEvent::Back]);
            let start = apply(&again, TourEvent::ImportStarted);
            assert_ne!(start.effect, TourEffect::OpenImport);
            assert_ne!(start.record.state, Importing);
        }
    }

    #[test]
    fn import_without_a_valid_receipt_still_reveals() {
        let bad = ImportReceipt {
            content_hash: "NOT HEX".to_owned(),
            space_ids: vec![1],
        };
        let record = run(
            &TourRecord::fresh(),
            [TourEvent::ImportStarted, TourEvent::ImportCommitted(bad)],
        );
        assert_eq!(record.state, Reveal);
        assert_eq!(record.receipt, None);
    }

    #[test]
    fn replay_starts_at_welcome_keeps_the_receipt_and_resets_practice() {
        let mut finished = run(
            &at(Practice(1)),
            [
                TourEvent::StepDone(1),
                TourEvent::SkipStep(2),
                TourEvent::StepDone(3),
                TourEvent::Finish,
            ],
        );
        finished.receipt = Some(receipt());
        let replayed = apply(&finished, TourEvent::Replay);
        assert_eq!(replayed.record.state, Welcome);
        assert_eq!(replayed.effect, TourEffect::Show { animate: true });
        assert_eq!(replayed.record.receipt, Some(receipt()));
        assert!((1..=3).all(|s| !replayed.record.is_done(s) && !replayed.record.is_skipped(s)));
    }

    #[test]
    fn every_choice_survives_a_restart() {
        // Each record a choice leads to decodes to itself after a relaunch, apart
        // from the transient states relaunch resolves.
        let records = [
            TourRecord::fresh(),
            run(&at(Practice(1)), [TourEvent::SkipStep(1)]),
            run(
                &at(Practice(1)),
                [TourEvent::StepDone(1), TourEvent::SkipStep(2)],
            ),
            run(
                &at(Importing),
                [TourEvent::ImportCommitted(receipt()), TourEvent::RevealDone],
            ),
            at(Handoff),
            TourRecord::complete(),
        ];
        for record in records {
            assert_eq!(decode(&encode(&record)), record, "{}", encode(&record));
            assert_eq!(relaunch(&record).record, record);
        }
    }

    #[test]
    fn encodes_compact_json() {
        let mut record = run(&at(Practice(1)), [TourEvent::StepDone(1)]);
        record.receipt = Some(receipt());
        assert_eq!(
            encode(&record),
            r#"{"v":1,"state":"practice","step":2,"done":[1],"skipped":[],"import":{"hash":"ab12","spaces":[5,6]}}"#
        );
        assert_eq!(
            encode(&TourRecord::fresh()),
            r#"{"v":1,"state":"welcome","step":0,"done":[],"skipped":[],"import":null}"#
        );
    }

    #[test]
    fn unreadable_record_reads_as_complete() {
        let good = encode(&TourRecord::fresh());
        let cases = [
            String::new(),
            "garbage".to_owned(),
            "{}".to_owned(),
            good.replace("\"v\":1", "\"v\":2"),
            good.replace("welcome", "lost"),
            good.replace("\"step\":0", "\"step\":4"),
            good.replace("welcome\",\"step\":0", "practice\",\"step\":0"),
            good.replace("\"done\":[]", "\"done\":[9]"),
            good.replace("\"done\":[]", "\"done\":[1,1,1,1]"),
            good.replace("\"done\":[],\"skipped\":[]", "\"done\":[1],\"skipped\":[1]"),
            good.replace("null", r#"{"hash":"","spaces":[]}"#),
            good.replace("null", r#"{"hash":"ab","spaces":[0]}"#),
            good.replace("null", r#"{"hash":"AB","spaces":[1]}"#),
            good.replace("\"done\":[]", "\"done\":[99999999999999999999999]"),
            format!("{good} "),
            format!(" {good}"),
            good.replace(',', ", "),
            "x".repeat(MAX_RECORD_BYTES + 1),
        ];
        for text in cases {
            assert_eq!(decode(&text), TourRecord::complete(), "{text}");
            assert_eq!(on_load(Some(&text), true).record.state, Complete);
        }
    }

    #[test]
    fn receipt_is_bounded() {
        let mut big = receipt();
        big.space_ids = (1..=MAX_SPACES as u64 + 1).collect();
        assert!(!big.valid());
        big.space_ids.pop();
        assert!(big.valid());
        let mut record = at(Reveal);
        record.receipt = Some(big);
        assert_eq!(decode(&encode(&record)), record);
        let long = ImportReceipt {
            content_hash: "a".repeat(MAX_HASH_BYTES + 1),
            space_ids: vec![],
        };
        assert!(!long.valid());
    }

    #[test]
    fn import_crossfades_to_the_first_imported_space() {
        let committed = apply(&at(Importing), TourEvent::ImportCommitted(receipt()));
        assert_eq!(committed.effect, TourEffect::Imported);
        assert_eq!(committed.record.theme_space(), Some(5));
        // A commit that reported no Space crossfades to the default theme.
        let empty = ImportReceipt {
            content_hash: "ab12".to_owned(),
            space_ids: vec![],
        };
        let committed = apply(&at(Importing), TourEvent::ImportCommitted(empty));
        assert_eq!(committed.effect, TourEffect::Imported);
        assert_eq!(committed.record.theme_space(), None);
        assert_eq!(TourRecord::fresh().theme_space(), None);
    }

    #[test]
    fn kill_after_the_commit_resumes_past_the_welcome_without_import() {
        // The record is saved at the commit, so a kill before the tour moves on
        // relaunches at the reveal's resume point, never back at the welcome.
        let committed = apply(&TourRecord::fresh(), TourEvent::ImportStarted).record;
        let committed = apply(&committed, TourEvent::ImportCommitted(receipt())).record;
        let resumed = relaunch(&committed);
        assert_eq!(resumed.record.state, Practice(1));
        assert_eq!(resumed.effect, TourEffect::Show { animate: false });
        assert!(!resumed.record.offers_import());
        assert_eq!(resumed.record.theme_space(), Some(5));
        // A second relaunch, and a window closing on the way, keep it there.
        let again = relaunch(&apply(&resumed.record, TourEvent::WindowClosed).record);
        assert_eq!(again.record, resumed.record);
        // Nothing in the resumed tour opens the import again.
        for event in [TourEvent::ImportStarted, TourEvent::Back] {
            let next = apply(&resumed.record, event.clone());
            assert_ne!(next.effect, TourEffect::OpenImport, "{event:?}");
        }
    }

    #[test]
    fn a_second_commit_never_replaces_the_receipt() {
        let committed = apply(&at(Importing), TourEvent::ImportCommitted(receipt())).record;
        let other = ImportReceipt {
            content_hash: "cd34".to_owned(),
            space_ids: vec![9],
        };
        for record in [committed.clone(), relaunch(&committed).record] {
            let next = apply(&record, TourEvent::ImportCommitted(other.clone()));
            assert_eq!(next.record, record);
            assert_eq!(next.effect, TourEffect::None);
        }
    }

    #[test]
    fn third_practice_switches_space_or_creates_one() {
        assert_eq!(space_practice(0, 0), SpacePractice::Create);
        assert_eq!(space_practice(1, 0), SpacePractice::Create);
        // The key always names another Space than the current one.
        assert_eq!(space_practice(2, 0), SpacePractice::Switch { key: 2 });
        assert_eq!(space_practice(3, 1), SpacePractice::Switch { key: 1 });
        assert_eq!(space_practice(8, 7), SpacePractice::Switch { key: 1 });
    }

    #[test]
    fn third_practice_finishes_on_a_switch_or_a_new_space() {
        assert!(!space_step_done((1, 3), (1, 3)));
        assert!(space_step_done((1, 3), (2, 3)));
        // Creating a Space finishes it whether or not the window switches.
        assert!(space_step_done((1, 1), (1, 2)));
        assert!(space_step_done((1, 1), (4, 2)));
        // Deleting a Space doesn't.
        assert!(!space_step_done((1, 3), (1, 2)));
        let done = apply(&at(Practice(3)), TourEvent::StepDone(3));
        assert_eq!(done.record.state, Handoff);
        assert!(done.record.is_done(3));
    }

    #[test]
    fn pin_practice_points_at_a_today_tab_and_finishes_on_the_pin() {
        let shown = pin_practice(None, PinTab::Today, false);
        assert_eq!(shown, PinPractice::Pin);
        assert_eq!(pin_practice(Some(shown), PinTab::Today, false), shown);
        assert_eq!(
            pin_practice(Some(shown), PinTab::Saved, false),
            PinPractice::Done
        );
        // Back doesn't change what a Today tab gets.
        assert_eq!(pin_practice(None, PinTab::Today, true), PinPractice::Pin);
    }

    #[test]
    fn pin_practice_counts_a_tab_pinned_already_as_done() {
        assert_eq!(pin_practice(None, PinTab::Saved, false), PinPractice::Done);
        let done = apply(&at(Practice(2)), TourEvent::StepDone(2));
        assert_eq!(done.record.state, Practice(3));
    }

    #[test]
    fn pin_practice_skips_itself_without_a_web_tab() {
        assert_eq!(
            pin_practice(None, PinTab::Missing, false),
            PinPractice::Skip
        );
        // The tab closing under the card skips the step too.
        for shown in [PinPractice::Pin, PinPractice::Pinned] {
            assert_eq!(
                pin_practice(Some(shown), PinTab::Missing, false),
                PinPractice::Skip
            );
        }
        let skipped = apply(&at(Practice(2)), TourEvent::SkipStep(2)).record;
        assert_eq!(skipped.state, Practice(3));
        assert!(skipped.is_skipped(2) && !skipped.is_done(2));
    }

    #[test]
    fn pin_practice_points_at_another_web_tab_before_skipping() {
        // The current tab isn't a page, but another web tab is open: the card
        // points at that one, as the step starts, after Back and when the tab
        // under the card closes.
        for by_back in [false, true] {
            assert_eq!(pin_practice(None, PinTab::Other, by_back), PinPractice::Pin);
        }
        for shown in [PinPractice::Pin, PinPractice::Pinned] {
            assert_eq!(
                pin_practice(Some(shown), PinTab::Other, false),
                PinPractice::Pin
            );
        }
        // Only no web tab at all skips the step.
        assert_eq!(
            pin_practice(None, PinTab::Missing, false),
            PinPractice::Skip
        );
    }

    #[test]
    fn back_into_practice_two_never_bounces_forward() {
        // A tab pinned in practice 2 gets a card with Next instead of feeding
        // step_done again, and unpinning it asks for the pin.
        let shown = pin_practice(None, PinTab::Saved, true);
        assert_eq!(shown, PinPractice::Pinned);
        assert_eq!(pin_practice(Some(shown), PinTab::Saved, true), shown);
        assert_eq!(
            pin_practice(Some(shown), PinTab::Today, true),
            PinPractice::Pin
        );
        // With no tab to pin, Back carries on to practice 1.
        assert_eq!(pin_practice(None, PinTab::Missing, true), PinPractice::Back);
        let record = run(&at(Practice(3)), [TourEvent::Back, TourEvent::Back]);
        assert_eq!(record.state, Practice(1));
    }

    #[test]
    fn done_or_esc_on_the_handoff_completes_and_keeps_the_steps() {
        let handoff = run(
            &at(Practice(1)),
            [
                TourEvent::StepDone(1),
                TourEvent::SkipStep(2),
                TourEvent::StepDone(3),
            ],
        );
        assert_eq!(handoff.state, Handoff);
        // Done feeds finish, and Esc on the handoff card does too; skip_tour, which
        // Esc means on earlier cards, ends at the same record.
        for event in [TourEvent::Finish, TourEvent::SkipTour] {
            let done = apply(&handoff, event.clone());
            assert_eq!(done.record.state, Complete, "{event:?}");
            assert!(
                done.record.is_done(1) && done.record.is_skipped(2),
                "{event:?}"
            );
            assert!(done.record.is_done(3), "{event:?}");
            assert_eq!(relaunch(&done.record).effect, TourEffect::None);
        }
    }

    #[test]
    fn replay_from_any_state_never_imports_or_drops_the_receipt() {
        for state in [Welcome, Importing, Reveal, Practice(2), Handoff, Complete] {
            let mut record = at(state);
            record.receipt = Some(receipt());
            let replayed = apply(&record, TourEvent::Replay);
            assert_eq!(replayed.record.state, Welcome, "{state:?}");
            assert_eq!(replayed.effect, TourEffect::Show { animate: true });
            assert_eq!(replayed.record.receipt, Some(receipt()), "{state:?}");
            assert!(!replayed.record.offers_import(), "{state:?}");
            // The welcome's import offer stays off, so nothing opens the dialog.
            let pressed = apply(&replayed.record, TourEvent::ImportStarted);
            assert_eq!(pressed.record, replayed.record, "{state:?}");
            assert_eq!(pressed.effect, TourEffect::None, "{state:?}");
        }
        // With no receipt, the replayed welcome offers import again.
        let replayed = apply(&TourRecord::complete(), TourEvent::Replay).record;
        assert!(replayed.offers_import() && replayed.receipt.is_none());
    }

    // The quick start rule, fed each transition the way the browser feeds it.
    fn quick_starts(
        record: &TourRecord,
        events: impl IntoIterator<Item = TourEvent>,
        mut pending: bool,
    ) -> Vec<QuickStart> {
        let mut record = record.clone();
        let mut seen = Vec::new();
        for event in events {
            let next = apply(&record, event).record;
            let decided = quick_start(&record, &next, pending);
            if decided != QuickStart::Keep {
                pending = false;
                seen.push(decided);
            }
            record = next;
        }
        seen
    }

    const START_FRESH: [TourEvent; 6] = [
        TourEvent::StartBrowsing,
        TourEvent::RevealDone,
        TourEvent::StepDone(1),
        TourEvent::StepDone(2),
        TourEvent::StepDone(3),
        TourEvent::Finish,
    ];

    #[test]
    fn start_fresh_opens_the_quick_start_page_when_tour_starts() {
        assert_eq!(
            quick_starts(&TourRecord::fresh(), START_FRESH, true),
            [QuickStart::Open]
        );
        // StartBrowsing opens the welcome folder and page right away, before the tour steps.
        let after_start = apply(&TourRecord::fresh(), TourEvent::StartBrowsing).record;
        assert_eq!(
            quick_start(&TourRecord::fresh(), &after_start, true),
            QuickStart::Open
        );
        // Subsequent steps do not reopen it.
        let mut record = after_start;
        for event in &START_FRESH[1..] {
            let next = apply(&record, event.clone()).record;
            assert_eq!(quick_start(&record, &next, true), QuickStart::Keep);
            record = next;
        }
    }

    #[test]
    fn skip_tour_opens_it_from_the_welcome_and_from_a_coach_mark() {
        assert_eq!(
            quick_starts(&TourRecord::fresh(), [TourEvent::SkipTour], true),
            [QuickStart::Open]
        );
        assert_eq!(
            quick_starts(
                &TourRecord::fresh(),
                [
                    TourEvent::StartBrowsing,
                    TourEvent::RevealDone,
                    TourEvent::SkipStep(1),
                    TourEvent::SkipTour
                ],
                true
            ),
            [QuickStart::Open]
        );
    }

    #[test]
    fn a_tour_after_an_import_drops_the_page() {
        let import = [
            TourEvent::ImportStarted,
            TourEvent::ImportCommitted(receipt()),
            TourEvent::RevealDone,
        ];
        let skipped = import.iter().cloned().chain([TourEvent::SkipTour]);
        assert_eq!(
            quick_starts(&TourRecord::fresh(), skipped, true),
            [QuickStart::Drop]
        );
        let finished = import
            .iter()
            .cloned()
            .chain(START_FRESH[2..].iter().cloned());
        assert_eq!(
            quick_starts(&TourRecord::fresh(), finished, true),
            [QuickStart::Drop]
        );
        // A cancelled import leaves no receipt, so Start fresh still opens it.
        let cancelled = [TourEvent::ImportStarted, TourEvent::ImportCancelled]
            .into_iter()
            .chain(START_FRESH);
        assert_eq!(
            quick_starts(&TourRecord::fresh(), cancelled, true),
            [QuickStart::Open]
        );
    }

    #[test]
    fn the_page_opens_once_and_never_after_a_replay() {
        // The end clears the flag, so a replay that ends again keeps it shut.
        let events = START_FRESH
            .into_iter()
            .chain([TourEvent::Replay, TourEvent::SkipTour]);
        assert_eq!(
            quick_starts(&TourRecord::fresh(), events, true),
            [QuickStart::Open]
        );
        // A workspace that isn't new, or a finished tour fed again, never opens it.
        assert!(quick_starts(&TourRecord::fresh(), [TourEvent::SkipTour], false).is_empty());
        let complete = TourRecord::complete();
        assert_eq!(
            quick_start(&complete, &apply(&complete, TourEvent::Finish).record, true),
            QuickStart::Keep
        );
        let replayed = apply(&complete, TourEvent::Replay).record;
        assert!(quick_starts(&replayed, [TourEvent::SkipTour], false).is_empty());
    }

    #[test]
    fn closing_the_window_or_a_relaunch_mid_tour_keeps_it_pending() {
        for state in [Welcome, Reveal, Practice(2), Handoff] {
            for event in [TourEvent::WindowClosed, TourEvent::Relaunched] {
                let next = apply(&at(state), event).record;
                assert_eq!(quick_start(&at(state), &next, true), QuickStart::Keep);
            }
        }
    }

    #[test]
    fn only_a_fresh_workspace_starts_with_the_page_pending() {
        let mut fresh = crate::Workspace::default();
        assert!(fresh.quick_start_pending());
        // Saved and restored, it stays pending until the tour ends.
        let restored = crate::Workspace::from_snapshot(fresh.snapshot()).unwrap();
        assert!(restored.quick_start_pending());
        fresh.clear_quick_start();
        let restored = crate::Workspace::from_snapshot(fresh.snapshot()).unwrap();
        assert!(!restored.quick_start_pending());
    }

    #[test]
    fn seed_quick_start_basics_creates_open_folder_once_and_clears_pending() {
        let mut fresh = crate::Workspace::default();
        let space_id = fresh.snapshot().spaces[0].id;
        assert_eq!(fresh.seed_quick_start_basics(space_id), Ok(true));
        assert!(!fresh.quick_start_pending());
        assert_eq!(fresh.folders().len(), 1);
        let folder = &fresh.folders()[0];
        assert_eq!(folder.name, crate::BASICS_FOLDER_NAME);
        assert_eq!(folder.space_id, space_id);
        assert!(folder.open);
        assert_eq!(folder.items.len(), crate::BASICS_ITEMS.len());

        let snapshot = fresh.snapshot();
        for (&item_id, &(title, url)) in folder.items.iter().zip(crate::BASICS_ITEMS.iter()) {
            let item = snapshot.items.iter().find(|it| it.id == item_id).unwrap();
            assert_eq!(item.placement, crate::Placement::Pin { space_id });
            assert_eq!(item.canonical_url.as_deref(), Some(url));
            assert_eq!(item.title, title);
            assert_eq!(item.name, title);
        }

        // Calling again (e.g. after replay or relaunch) is a no-op.
        assert_eq!(fresh.seed_quick_start_basics(space_id), Ok(false));
        assert_eq!(fresh.folders().len(), 1);

        // Snapshot round-trip preserves the folder and cleared pending flag.
        let mut restored = crate::Workspace::from_snapshot(fresh.snapshot()).unwrap();
        assert!(!restored.quick_start_pending());
        assert_eq!(restored.folders().len(), 1);
        assert_eq!(restored.seed_quick_start_basics(space_id), Ok(false));

        // The folder behaves like a normal user folder and can be deleted.
        let folder_id = restored.folders()[0].id;
        let deletion = restored.delete_folder(folder_id, None).unwrap();
        assert_eq!(deletion.removed_items.len(), crate::BASICS_ITEMS.len());
        assert!(restored.folders().is_empty());
    }

    #[test]
    fn seed_quick_start_basics_falls_back_to_first_space_and_skips_after_clear() {
        let mut fresh = crate::Workspace::default();
        let first_space = fresh.snapshot().spaces[0].id;
        assert_eq!(fresh.seed_quick_start_basics(0), Ok(true));
        assert_eq!(fresh.folders()[0].space_id, first_space);

        // An import path calls clear_quick_start; seeding afterwards is a no-op.
        let mut imported = crate::Workspace::default();
        imported.clear_quick_start();
        assert_eq!(imported.seed_quick_start_basics(0), Ok(false));
        assert!(imported.folders().is_empty());
    }
}
