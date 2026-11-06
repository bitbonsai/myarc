//! `page.click`, `page.fill`, `page.select` and `page.press`: act on an
//! element of the latest `page.snapshot` and answer with a fresh one.
//!
//! The core checks the parameters in [`parse`], before the browser runs
//! anything. The browser then hands over the tab's current main document,
//! and [`prepare`] checks the revision and the reference against
//! [`SnapshotStore`] and takes the snapshot's references, so the revision
//! turns stale while the action runs. The browser acts on the node behind
//! the reference, waits for the page to settle and snapshots it again, and
//! [`action_response`] formats the answer: the new revision, whether the
//! tab navigated, the outline and what changed among the references.
//!
//! Key names come from a short allowlist and map to a US keyboard here, so
//! the browser only turns names into events.

use serde_json::{Value, json};

use crate::snapshot::{
    RefTarget, SnapshotPage, SnapshotStore, Taken, collapse, quote, record_page,
};
use crate::{Outcome, Request, Response, TabState, answer, bounded, tab_handle, truncate};
use myarc_workspace::ItemId;

/// Longest text `page.fill` types, 64 KiB.
pub const MAX_FILL_BYTES: usize = 64 * 1024;
/// How long the browser waits for the page to run the action itself before
/// answering `outcome_unknown`.
pub const ACTION_WAIT_MS: u64 = 3_000;
/// Longest wait for the page to settle after the action.
pub const SETTLE_CAP_MS: u64 = 3_000;
/// How long the snapshot after an action may take. With the two waits above
/// it stays under the host's 10 second reply timeout.
pub const ACTION_SNAPSHOT_TIMEOUT_MS: u64 = 2_500;
/// Most entries in each list of `changes`.
pub const MAX_CHANGES: usize = 20;
/// Most options a `page.select` reads from the page.
pub const MAX_SELECT_OPTIONS: usize = 1_000;

/// What to do to the element.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ActKind {
    Click,
    Fill { text: String },
    Select { value: String },
    Press { key: KeySpec },
}

/// An interaction command that passed its parameter checks.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PageAction {
    pub item: ItemId,
    pub revision: String,
    /// The `ref` parameter. `page.press` may leave it out.
    pub element: Option<String>,
    pub kind: ActKind,
}

/// A key press as the browser sends it, on a US layout. `key` and `code`
/// are DOM `KeyboardEvent.key` and `.code` values; `code` is empty for a
/// character with no key of its own. `text` is the character a keypress
/// carries, 0 for none, and `key_code` the Windows virtual key code.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct KeySpec {
    pub key: String,
    pub code: String,
    pub text: u16,
    pub shift: bool,
    pub key_code: u16,
}

/// Named keys `page.press` accepts besides single characters.
pub const NAMED_KEYS: &[&str] = &[
    "Enter",
    "Tab",
    "Escape",
    "Backspace",
    "ArrowUp",
    "ArrowDown",
    "ArrowLeft",
    "ArrowRight",
    "PageUp",
    "PageDown",
    "Home",
    "End",
    "Space",
];

fn named(key: &str, code: &str, key_code: u16, text: u16) -> KeySpec {
    KeySpec {
        key: key.to_owned(),
        code: code.to_owned(),
        text,
        shift: false,
        key_code,
    }
}

/// The key `name` stands for: one of [`NAMED_KEYS`] or a single printable
/// character, such as `a`, `A`, `7` or `?`.
pub fn parse_key(name: &str) -> Result<KeySpec, String> {
    let spec = match name {
        "Enter" => named("Enter", "Enter", 0x0d, 0x0d),
        "Tab" => named("Tab", "Tab", 0x09, 0),
        "Escape" => named("Escape", "Escape", 0x1b, 0),
        "Backspace" => named("Backspace", "Backspace", 0x08, 0),
        "ArrowUp" => named("ArrowUp", "ArrowUp", 0x26, 0),
        "ArrowDown" => named("ArrowDown", "ArrowDown", 0x28, 0),
        "ArrowLeft" => named("ArrowLeft", "ArrowLeft", 0x25, 0),
        "ArrowRight" => named("ArrowRight", "ArrowRight", 0x27, 0),
        "PageUp" => named("PageUp", "PageUp", 0x21, 0),
        "PageDown" => named("PageDown", "PageDown", 0x22, 0),
        "Home" => named("Home", "Home", 0x24, 0),
        "End" => named("End", "End", 0x23, 0),
        "Space" | " " => named(" ", "Space", 0x20, 0x20),
        _ => {
            let mut chars = name.chars();
            match (chars.next(), chars.next()) {
                (Some(c), None) if !c.is_control() && !c.is_whitespace() => char_key(c)?,
                _ => {
                    return Err(format!(
                        "Key {:?} is not allowed; use {} or a single printable character",
                        bounded(name),
                        NAMED_KEYS.join(", ")
                    ));
                }
            }
        }
    };
    Ok(spec)
}

/// A printable character on a US layout.
fn char_key(c: char) -> Result<KeySpec, String> {
    let Ok(text) = u16::try_from(u32::from(c)) else {
        return Err(String::from(
            "Characters outside the Basic Multilingual Plane can't be pressed; use page.fill",
        ));
    };
    let mut spec = KeySpec {
        key: c.to_string(),
        code: String::new(),
        text,
        shift: false,
        key_code: 0,
    };
    if c.is_ascii_alphabetic() {
        let upper = c.to_ascii_uppercase();
        spec.code = format!("Key{upper}");
        spec.key_code = u16::from(upper as u8);
        spec.shift = c.is_ascii_uppercase();
    } else if c.is_ascii_digit() {
        spec.code = format!("Digit{c}");
        spec.key_code = u16::from(c as u8);
    } else if let Some((code, key_code, shift)) = symbol_key(c) {
        spec.code = code.to_owned();
        spec.key_code = key_code;
        spec.shift = shift;
    }
    Ok(spec)
}

/// DOM code, Windows key code and Shift of an ASCII symbol on a US layout.
fn symbol_key(c: char) -> Option<(&'static str, u16, bool)> {
    const DIGITS: &str = ")!@#$%^&*(";
    if let Some(digit) = DIGITS.find(c) {
        let code = [
            "Digit0", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6", "Digit7",
            "Digit8", "Digit9",
        ][digit];
        return Some((code, 0x30 + digit as u16, true));
    }
    let (code, key_code, shift) = match c {
        ';' => ("Semicolon", 0xba, false),
        ':' => ("Semicolon", 0xba, true),
        '=' => ("Equal", 0xbb, false),
        '+' => ("Equal", 0xbb, true),
        ',' => ("Comma", 0xbc, false),
        '<' => ("Comma", 0xbc, true),
        '-' => ("Minus", 0xbd, false),
        '_' => ("Minus", 0xbd, true),
        '.' => ("Period", 0xbe, false),
        '>' => ("Period", 0xbe, true),
        '/' => ("Slash", 0xbf, false),
        '?' => ("Slash", 0xbf, true),
        '`' => ("Backquote", 0xc0, false),
        '~' => ("Backquote", 0xc0, true),
        '[' => ("BracketLeft", 0xdb, false),
        '{' => ("BracketLeft", 0xdb, true),
        '\\' => ("Backslash", 0xdc, false),
        '|' => ("Backslash", 0xdc, true),
        ']' => ("BracketRight", 0xdd, false),
        '}' => ("BracketRight", 0xdd, true),
        '\'' => ("Quote", 0xde, false),
        '"' => ("Quote", 0xde, true),
        _ => return None,
    };
    Some((code, key_code, shift))
}

/// Checks the text `page.fill` types: at most [`MAX_FILL_BYTES`], and no
/// control characters except newline and tab.
pub fn check_fill_text(text: &str) -> Result<(), String> {
    if text.len() > MAX_FILL_BYTES {
        return Err(format!("Parameter text exceeds {MAX_FILL_BYTES} bytes"));
    }
    if text
        .chars()
        .any(|c| c.is_control() && c != '\n' && c != '\t')
    {
        return Err(String::from(
            "Parameter text contains control characters; use page.press for keys such as Enter",
        ));
    }
    Ok(())
}

/// Checks a revision parameter's shape, such as `3.2`.
pub fn check_revision(revision: &str) -> Result<(), String> {
    let valid = revision.len() <= 41
        && revision.split_once('.').is_some_and(|(document, count)| {
            [document, count].iter().all(|part| {
                !part.is_empty()
                    && !part.starts_with('0')
                    && part.bytes().all(|byte| byte.is_ascii_digit())
            })
        });
    if valid {
        Ok(())
    } else {
        Err(format!(
            "Revision {:?} is not a page.snapshot revision, such as 3.2",
            bounded(revision)
        ))
    }
}

/// Checks a `ref` parameter's shape, such as `e4`.
pub fn check_ref(element: &str) -> Result<(), String> {
    match crate::snapshot::parse_ref(element) {
        Some(_) => Ok(()),
        None => Err(format!(
            "Reference {:?} is not a [ref=eN] from page.snapshot, such as e4",
            bounded(element)
        )),
    }
}

/// Chromium roles of native text fields.
fn is_text_role(role: &str) -> bool {
    matches!(role, "textField" | "searchBox" | "textFieldWithComboBox")
}

/// A field `page.fill` can type into.
fn fillable(target: &RefTarget) -> Result<(), String> {
    if !is_text_role(&target.role) && !target.editable {
        return Err(String::from(
            "is not an editable text field; page.fill types only into text fields",
        ));
    }
    if target.disabled {
        return Err(String::from("is disabled"));
    }
    if target.read_only {
        return Err(String::from("is read-only"));
    }
    Ok(())
}

/// What the browser acts on, once [`prepare`] accepted the action.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Prepared {
    /// The DOM node ID of the element, or 0 when `page.press` has none.
    pub node: i32,
    /// `page.press` clicks the text field first to focus it.
    pub focus_first: bool,
    /// The references taken from the store.
    pub taken: Taken,
}

/// Checks `action` against the latest snapshot of `document`, the tab's
/// current main document, and takes that snapshot's references. `stale`
/// when the revision is not the latest or the page navigated since;
/// `invalid` when the reference is unknown or the element can't take the
/// action. Never picks a replacement element.
pub fn prepare(
    store: &mut SnapshotStore,
    document: u64,
    action: &PageAction,
) -> Result<Prepared, Outcome> {
    let target = match &action.element {
        Some(element) => Some(store.target(document, &action.revision, element)?.clone()),
        None => None,
    };
    let invalid = |problem: String| {
        let element = action.element.as_deref().unwrap_or_default();
        Outcome::Invalid {
            reason: format!("Element {} {problem}", bounded(element)),
        }
    };
    let mut focus_first = false;
    if let Some(target) = &target {
        if target.node <= 0 {
            return Err(invalid(String::from(
                "has no DOM node myArc can act on; act on another element",
            )));
        }
        match &action.kind {
            ActKind::Click if target.disabled => {
                return Err(invalid(String::from("is disabled")));
            }
            ActKind::Click => {}
            ActKind::Fill { .. } => fillable(target).map_err(invalid)?,
            ActKind::Select { .. } => {
                if !matches!(target.role.as_str(), "comboBoxSelect" | "listBox") {
                    return Err(invalid(String::from(
                        "is not a select element; page.select picks options of native selects",
                    )));
                }
                if target.disabled {
                    return Err(invalid(String::from("is disabled")));
                }
            }
            ActKind::Press { .. } if target.focused => {}
            ActKind::Press { .. } => {
                fillable(target).map_err(|_| {
                    invalid(String::from(
                        "is not focused, and page.press focuses only text fields; click it or press Tab first, then press without ref",
                    ))
                })?;
                focus_first = true;
            }
        }
    }
    let taken = store.take(document, &action.revision)?;
    Ok(Prepared {
        node: target.map_or(0, |target| target.node),
        focus_first,
        taken,
    })
}

/// One option of a native select as the page has it.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SelectOption {
    pub value: String,
    pub text: String,
    pub disabled: bool,
}

/// The value of the option `wanted` names: the option with that value, or
/// else the one whose visible text it is. `invalid` when none matches, when
/// several options show that text, or when the option is disabled.
pub fn choose_option(options: &[SelectOption], wanted: &str) -> Result<String, Outcome> {
    let invalid = |reason: String| Outcome::Invalid { reason };
    let options = &options[..options.len().min(MAX_SELECT_OPTIONS)];
    let chosen = match options.iter().find(|option| option.value == wanted) {
        Some(option) => option,
        None => {
            let (wanted_text, _) = collapse(wanted, usize::MAX);
            let matches: Vec<&SelectOption> = options
                .iter()
                .filter(|option| collapse(&option.text, usize::MAX).0 == wanted_text)
                .collect();
            match matches.as_slice() {
                [option] => *option,
                [] => {
                    let shown: Vec<String> = options
                        .iter()
                        .take(10)
                        .map(|option| quote(&collapse(&option.text, 60).0, false))
                        .collect();
                    let more = if options.len() > 10 { ", …" } else { "" };
                    return Err(invalid(format!(
                        "No option has the value or text {}; options: {}{more}",
                        quote(bounded(wanted), false),
                        shown.join(", ")
                    )));
                }
                _ => {
                    return Err(invalid(format!(
                        "{} options show the text {}; pass the option's value instead",
                        matches.len(),
                        quote(bounded(wanted), false)
                    )));
                }
            }
        }
    };
    if chosen.disabled {
        return Err(invalid(format!(
            "Option {} is disabled",
            quote(bounded(&chosen.text), false)
        )));
    }
    Ok(chosen.value.clone())
}

/// How the browser finished an interaction command.
#[derive(Clone, Debug, PartialEq)]
pub enum ActionResult {
    /// The action ran and `page` is the snapshot after it settled.
    /// `navigated` says a navigation committed in the tab meanwhile, and
    /// `settled` is false when the settle wait ran out.
    Done {
        page: SnapshotPage,
        navigated: bool,
        settled: bool,
    },
    /// The page refused the action before anything happened, such as a
    /// disabled or hidden element. The references go back to the store.
    Refused(Outcome),
    /// The action may or may not have happened, such as after a timeout or
    /// a crash. `tab` is the latest state.
    Unknown {
        reason: String,
        tab: TabState,
        navigated: bool,
    },
}

/// The answer to an interaction command: `{tab, url, revision, navigated,
/// settled, snapshot, changes}`, where `snapshot` has the shape of
/// `page.snapshot`'s answer and `changes` lists the references added,
/// removed or changed since the snapshot the action used. The new revision
/// replaces the old one.
pub fn action_response(
    req: &Request,
    result: ActionResult,
    taken: Taken,
    store: &mut SnapshotStore,
) -> Response {
    match result {
        ActionResult::Done {
            page,
            navigated,
            settled,
        } => {
            let (snapshot, targets) = record_page(&page, store);
            let mut data = json!({
                "tab": tab_handle(page.tab.item),
                "url": truncate(&page.tab.url, crate::MAX_URL_LENGTH),
                "revision": snapshot["revision"].clone(),
                "navigated": navigated,
                "settled": settled,
            });
            if !navigated && page.document == taken.document {
                data["changes"] = changes(&taken.refs, &targets);
            }
            data["snapshot"] = snapshot;
            answer(req, Outcome::Success { data })
        }
        ActionResult::Refused(outcome) => {
            store.put_back(taken);
            answer(req, outcome)
        }
        ActionResult::Unknown {
            reason,
            tab,
            navigated,
        } => answer(
            req,
            Outcome::OutcomeUnknown {
                reason,
                data: Some(json!({
                    "tab": tab_handle(tab.item),
                    "url": truncate(&tab.url, crate::MAX_URL_LENGTH),
                    "navigated": navigated,
                })),
            },
        ),
    }
}

/// References added, removed or changed between two snapshots of one
/// document, matched by DOM node ID: `{added, removed, changed}`, each at
/// most [`MAX_CHANGES`] lines, with `more` counting what was left out.
/// Added and changed lines carry the new references; removed ones say
/// which reference they had.
pub fn changes(before: &[RefTarget], after: &[RefTarget]) -> Value {
    use std::collections::HashMap;
    let index = |refs: &[RefTarget]| -> HashMap<i32, usize> {
        refs.iter()
            .enumerate()
            .filter(|(_, target)| target.node > 0)
            .map(|(position, target)| (target.node, position))
            .collect()
    };
    let (old, new) = (index(before), index(after));
    let mut lists: [Vec<String>; 3] = Default::default();
    for (position, target) in after.iter().enumerate() {
        if target.node <= 0 {
            continue;
        }
        let line = target.line(position + 1);
        match old.get(&target.node).map(|&old| &before[old]) {
            None => lists[0].push(line),
            Some(was) if was.head != target.head || was.tail != target.tail => {
                lists[2].push(line);
            }
            Some(_) => {}
        }
    }
    for (position, target) in before.iter().enumerate() {
        if target.node > 0 && !new.contains_key(&target.node) {
            lists[1].push(format!(
                "{}{} (was e{})",
                target.head,
                target.tail,
                position + 1
            ));
        }
    }
    let mut out = json!({});
    let mut more = 0;
    for (name, list) in ["added", "removed", "changed"].iter().zip(lists) {
        if list.is_empty() {
            continue;
        }
        more += list.len().saturating_sub(MAX_CHANGES);
        out[*name] = json!(list.into_iter().take(MAX_CHANGES).collect::<Vec<_>>());
    }
    if more > 0 {
        out["more"] = json!(more);
    }
    out
}
