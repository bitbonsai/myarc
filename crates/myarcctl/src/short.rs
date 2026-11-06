//! Short commands such as `myarcctl open <url>` and `myarcctl click @e4`.
//! Each one sends registry commands through the same `ipc::Client` that
//! `run` uses. A small per-user state file remembers the current tab and the
//! revision this CLI last printed, so the browser's stale check still
//! protects the agent: the CLI never fetches a newer revision on its own.
//!
//! Setting `MYARCCTL_ECHO_REQUESTS` to a non-empty value writes each outgoing
//! request to stderr as `request: <json>`. Tests use it; it isn't in `--help`.

use std::fs::{self, DirBuilder, OpenOptions};
use std::io::{self, Read, Write};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt, PermissionsExt};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use myarc_control::ipc::{self, Client};
use myarc_control::{Outcome, Request, Response, actions, parse_tab_handle};
use serde_json::{Value, json};

use crate::{EXIT_NOT_CONNECTED, call, connect, request_id, write_line};

const ECHO_ENV: &str = "MYARCCTL_ECHO_REQUESTS";
const STATE_FILE: &str = "cli-state.json";
/// Largest state file the CLI reads back. A full outline's hashes take
/// about 40 KiB.
const MAX_STATE_BYTES: u64 = 256 * 1024;

#[derive(Clone, Debug, PartialEq)]
pub enum Short {
    Open {
        url: String,
    },
    Snapshot {
        interactive: bool,
    },
    Click {
        element: String,
    },
    Fill {
        element: String,
        text: String,
    },
    Select {
        element: String,
        value: String,
    },
    Press {
        key: String,
        element: Option<String>,
    },
    Back,
    Forward,
    Reload,
    Close,
    Tabs,
    Use {
        tab: String,
    },
}

/// `--tab` and `--rev`, which replace the current tab and the last revision
/// for one call.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Overrides {
    pub tab: Option<String>,
    pub revision: Option<String>,
}

/// Parses a short command, or `None` when `positional` names another
/// subcommand.
pub fn parse(positional: &[&str]) -> Option<Result<Short, String>> {
    let (name, args) = positional.split_first()?;
    let text = |value: &str| value.to_owned();
    let short = match (*name, args) {
        ("open", [url]) => Ok(Short::Open { url: text(url) }),
        ("open", _) => Err(String::from("open takes one URL")),
        ("snapshot", []) => Ok(Short::Snapshot { interactive: false }),
        ("snapshot", ["-i"]) => Ok(Short::Snapshot { interactive: true }),
        ("snapshot", _) => Err(String::from("snapshot takes only -i")),
        ("click", [element]) => parse_ref(element).map(|element| Short::Click { element }),
        ("click", _) => Err(String::from("click takes one ref, such as @e4")),
        ("fill", [element, value]) => parse_ref(element).map(|element| Short::Fill {
            element,
            text: text(value),
        }),
        ("fill", _) => Err(String::from(
            "fill takes a ref and the text; quote the text",
        )),
        ("select", [element, value]) => parse_ref(element).map(|element| Short::Select {
            element,
            value: text(value),
        }),
        ("select", _) => Err(String::from("select takes a ref and an option; quote it")),
        ("press", [key]) => Ok(Short::Press {
            key: text(key),
            element: None,
        }),
        ("press", [key, element]) => parse_ref(element).map(|element| Short::Press {
            key: text(key),
            element: Some(element),
        }),
        ("press", _) => Err(String::from("press takes a key and an optional ref")),
        ("back", []) => Ok(Short::Back),
        ("forward", []) => Ok(Short::Forward),
        ("reload", []) => Ok(Short::Reload),
        ("close", []) => Ok(Short::Close),
        ("tabs", []) => Ok(Short::Tabs),
        ("back" | "forward" | "reload" | "close" | "tabs", _) => {
            Err(format!("{name} takes no arguments"))
        }
        ("use", [tab]) => parse_tab(tab).map(|tab| Short::Use { tab }),
        ("use", _) => Err(String::from("use takes one tab handle, such as t12")),
        _ => return None,
    };
    Some(short)
}

/// A snapshot reference written as `@e4` or `e4`, returned as `e4`.
pub fn parse_ref(text: &str) -> Result<String, String> {
    let element = text.strip_prefix('@').unwrap_or(text);
    actions::check_ref(element)
        .map(|()| element.to_owned())
        .map_err(|_| {
            format!(
                "{:?} is not a ref from the outline, such as @e4",
                crate::mcp::bounded(text)
            )
        })
}

pub fn parse_tab(text: &str) -> Result<String, String> {
    match parse_tab_handle(text) {
        Some(_) => Ok(text.to_owned()),
        None => Err(format!(
            "{:?} is not a tab handle, such as t12; myarcctl tabs lists them",
            crate::mcp::bounded(text)
        )),
    }
}

/// What the CLI remembers between calls: the current tab, the revision it
/// last printed for that tab, and hashes of that outline's ref lines without
/// their ref and focus markers, which tell focus-only changes apart.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct State {
    pub tab: Option<String>,
    pub revision: Option<String>,
    pub seen: Vec<u64>,
}

impl State {
    /// Reads the state file. A missing, oversized or malformed file reads as
    /// an empty state.
    pub fn load(path: &Path) -> Self {
        let mut text = String::new();
        let read = fs::File::open(path)
            .and_then(|file| file.take(MAX_STATE_BYTES + 1).read_to_string(&mut text));
        if !matches!(read, Ok(len) if len as u64 <= MAX_STATE_BYTES) {
            return Self::default();
        }
        let Ok(value) = serde_json::from_str::<Value>(&text) else {
            return Self::default();
        };
        let tab = value["tab"]
            .as_str()
            .filter(|tab| parse_tab_handle(tab).is_some());
        let revision = value["revision"]
            .as_str()
            .filter(|revision| tab.is_some() && actions::check_revision(revision).is_ok());
        let mut seen: Vec<u64> = value["seen"]
            .as_array()
            .map(|list| {
                list.iter()
                    .filter_map(|hash| u64::from_str_radix(hash.as_str()?, 16).ok())
                    .collect()
            })
            .unwrap_or_default();
        seen.sort_unstable();
        Self {
            tab: tab.map(str::to_owned),
            revision: revision.map(str::to_owned),
            seen,
        }
    }

    /// Writes the state file with mode 0600 through a temporary file and a
    /// rename, creating its directory with mode 0700 when missing.
    pub fn save(&self, path: &Path) -> io::Result<()> {
        if let Some(dir) = path.parent().filter(|dir| !dir.as_os_str().is_empty()) {
            DirBuilder::new().recursive(true).mode(0o700).create(dir)?;
        }
        let value = json!({
            "tab": self.tab,
            "revision": self.revision,
            "seen": self.seen.iter().map(|hash| format!("{hash:x}")).collect::<Vec<_>>(),
        });
        let temp = path.with_extension(format!("tmp-{}", std::process::id()));
        let written = (|| {
            let mut file = OpenOptions::new()
                .write(true)
                .create(true)
                .truncate(true)
                .mode(0o600)
                .open(&temp)?;
            fs::set_permissions(&temp, fs::Permissions::from_mode(0o600))?;
            file.write_all(value.to_string().as_bytes())?;
            file.sync_all()?;
            fs::rename(&temp, path)
        })();
        if written.is_err() {
            let _ = fs::remove_file(&temp);
        }
        written
    }

    /// The tab a call acts on: `--tab`, else the current tab.
    fn tab(&self, overrides: &Overrides) -> Result<String, String> {
        overrides
            .tab
            .clone()
            .or_else(|| self.tab.clone())
            .ok_or_else(|| {
                String::from("no current tab; run myarcctl open <url> or myarcctl use <tab>")
            })
    }

    /// The revision a call on `tab` uses: `--rev`, else the revision last
    /// printed for that tab.
    fn revision(&self, tab: &str, overrides: &Overrides) -> Result<String, String> {
        if let Some(revision) = &overrides.revision {
            return Ok(revision.clone());
        }
        match &self.revision {
            Some(revision) if self.tab.as_deref() == Some(tab) => Ok(revision.clone()),
            _ => Err(format!(
                "no revision for {tab}; run myarcctl snapshot first"
            )),
        }
    }

    /// Takes the tab and revision a response carries. A tab command's answer
    /// clears the revision, since the page navigated or the tab changed, and
    /// closing the current tab clears both.
    pub fn apply(&mut self, response: &Response) {
        let (data, success) = match &response.outcome {
            Outcome::Success { data } => (data, true),
            Outcome::OutcomeUnknown {
                data: Some(data), ..
            } => (data, false),
            _ => return,
        };
        match &data["tab"] {
            Value::String(handle) => {
                if self.tab.as_deref() != Some(handle) {
                    *self = Self {
                        tab: Some(handle.clone()),
                        ..Self::default()
                    };
                }
            }
            Value::Object(tab) => {
                if let Some(handle) = tab.get("handle").and_then(Value::as_str) {
                    if data["closed"] == true {
                        if self.tab.as_deref() == Some(handle) {
                            *self = Self::default();
                        }
                        return;
                    }
                    *self = Self {
                        tab: Some(handle.to_owned()),
                        ..Self::default()
                    };
                }
            }
            _ => {}
        }
        if !success {
            return;
        }
        if let Some(revision) = data["revision"].as_str() {
            self.revision = Some(revision.to_owned());
        }
        if let Some(text) = data["snapshot"]["text"].as_str().or(data["text"].as_str()) {
            self.seen = ref_hashes(text);
        }
    }
}

/// Where the state lives: `cli-state.json` next to the control directory.
pub fn state_path() -> Option<PathBuf> {
    let dir = ipc::default_dir()?;
    Some(dir.parent().unwrap_or(Path::new("")).join(STATE_FILE))
}

/// Splits a line into runs outside and inside double quotes. A quoted run
/// keeps its quotes; backslash escapes stay inside it.
fn segments(line: &str) -> Vec<(bool, &str)> {
    let bytes = line.as_bytes();
    let mut out = Vec::new();
    let (mut start, mut quoted, mut i) = (0, false, 0);
    while i < bytes.len() {
        match bytes[i] {
            b'\\' if quoted => {
                i += 2;
                continue;
            }
            b'"' if quoted => {
                out.push((true, &line[start..=i]));
                start = i + 1;
                quoted = false;
            }
            b'"' => {
                if start < i {
                    out.push((false, &line[start..i]));
                }
                start = i;
                quoted = true;
            }
            _ => {}
        }
        i += 1;
    }
    if start < line.len() {
        out.push((quoted, &line[start..]));
    }
    out
}

/// Whether an outline line carries `[ref=eN]` outside its quoted page text
/// and before a link target.
pub fn has_ref(line: &str) -> bool {
    for (quoted, run) in segments(line) {
        if quoted {
            continue;
        }
        let (before, target) = match run.split_once(" -> ") {
            Some((before, _)) => (before, true),
            None => (run, false),
        };
        if before.contains("[ref=e") {
            return true;
        }
        if target {
            return false;
        }
    }
    false
}

/// Keeps the lines with a ref and the ancestor lines they sit under, for
/// `snapshot -i`.
pub fn interactive(text: &str) -> String {
    let mut out = Vec::new();
    // Open ancestors: indentation, line, already printed.
    let mut stack: Vec<(usize, &str, bool)> = Vec::new();
    for line in text.lines() {
        let depth = line.len() - line.trim_start_matches(' ').len();
        while stack.last().is_some_and(|(open, ..)| *open >= depth) {
            stack.pop();
        }
        let wanted = has_ref(line);
        if wanted {
            for (_, ancestor, printed) in stack.iter_mut().filter(|entry| !entry.2) {
                out.push(*ancestor);
                *printed = true;
            }
            out.push(line);
        }
        stack.push((depth, line, wanted));
    }
    out.join("\n")
}

/// A ref line without indentation, its `[ref=eN]` and its `[focused]`,
/// leaving quoted page text alone.
fn unmarked(line: &str) -> String {
    let mut out = String::with_capacity(line.len());
    for (quoted, run) in segments(line.trim_start()) {
        if quoted {
            out.push_str(run);
            continue;
        }
        let mut rest = run.replace(" [focused]", "");
        while let Some(start) = rest.find(" [ref=e") {
            let end = rest[start..]
                .find(']')
                .map_or(rest.len(), |end| start + end + 1);
            rest.replace_range(start..end, "");
        }
        out.push_str(&rest);
    }
    out
}

/// FNV-1a, stable across builds so the state file survives an update.
fn hash(text: &str) -> u64 {
    text.bytes().fold(0xcbf2_9ce4_8422_2325, |hash, byte| {
        (hash ^ u64::from(byte)).wrapping_mul(0x0100_0000_01b3)
    })
}

/// Sorted hashes of an outline's ref lines without their markers.
pub fn ref_hashes(text: &str) -> Vec<u64> {
    let mut hashes: Vec<u64> = text
        .lines()
        .filter(|line| has_ref(line))
        .map(|line| hash(&unmarked(line)))
        .collect();
    hashes.sort_unstable();
    hashes.dedup();
    hashes
}

/// The `changes` of an action as lines, leaving out `changed` entries that
/// differ from a line of the previous outline (`seen`) only in `[focused]`.
pub fn change_lines(changes: &Value, seen: &[u64]) -> Vec<String> {
    let mut lines = Vec::new();
    for key in ["added", "removed", "changed"] {
        for line in changes[key].as_array().into_iter().flatten() {
            let Some(line) = line.as_str() else { continue };
            if key == "changed" && seen.binary_search(&hash(&unmarked(line))).is_ok() {
                continue;
            }
            lines.push(format!("{key}: {line}"));
        }
    }
    if let Some(more) = changes["more"].as_u64() {
        lines.push(format!("({more} more changes)"));
    }
    lines
}

/// Page text, such as a title, as a JSON string so it can't break the line.
fn quoted(value: &Value) -> String {
    Value::String(value.as_str().unwrap_or_default().to_owned()).to_string()
}

/// `t12 · rev 1.2 · "Title" · https://example.com/`, plus `loading` and
/// `truncated` when they hold.
pub fn snapshot_header(data: &Value) -> String {
    let mut parts = vec![
        data["tab"].as_str().unwrap_or("?").to_owned(),
        format!("rev {}", data["revision"].as_str().unwrap_or("?")),
        quoted(&data["title"]),
        data["url"].as_str().unwrap_or_default().to_owned(),
    ];
    if data["loading"] == true {
        parts.push(String::from("loading"));
    }
    if data["truncated"] == true {
        parts.push(String::from("truncated"));
    }
    parts.join(" · ")
}

/// A `page.snapshot` answer: the header, then the outline.
pub fn render_snapshot(data: &Value, interactive_only: bool) -> String {
    let text = data["text"].as_str().unwrap_or_default();
    let text = if interactive_only {
        interactive(text)
    } else {
        text.to_owned()
    };
    join_nonempty(&[snapshot_header(data), text])
}

/// An action's answer: `ok click e4 · rev 1.5 · navigated`, the changes, then
/// the new outline.
pub fn render_action(label: &str, data: &Value, seen: &[u64]) -> String {
    let snapshot = &data["snapshot"];
    let mut status = vec![
        format!("ok {label}"),
        format!("rev {}", data["revision"].as_str().unwrap_or("?")),
    ];
    if data["navigated"] == true {
        status.push(String::from("navigated"));
        status.push(data["url"].as_str().unwrap_or_default().to_owned());
    }
    if data["settled"] == false {
        status.push(String::from("not settled"));
    }
    if snapshot["truncated"] == true {
        status.push(String::from("truncated"));
    }
    let mut lines = vec![status.join(" · ")];
    lines.extend(change_lines(&data["changes"], seen));
    lines.push(snapshot["text"].as_str().unwrap_or_default().to_owned());
    join_nonempty(&lines)
}

/// `tabs.list` with `*` before the current tab.
pub fn render_tabs(data: &Value, current: Option<&str>) -> String {
    let tabs = data["tabs"]
        .as_array()
        .map(Vec::as_slice)
        .unwrap_or_default();
    if tabs.is_empty() {
        return String::from("no tabs in the agent Space");
    }
    tabs.iter()
        .map(|tab| {
            let handle = tab["handle"].as_str().unwrap_or("?");
            let mark = if Some(handle) == current { '*' } else { ' ' };
            let mut line = format!(
                "{mark} {handle} {} {}",
                quoted(&tab["title"]),
                tab["url"].as_str().unwrap_or_default()
            );
            if tab["loading"] == true {
                line.push_str(" · loading");
            }
            line
        })
        .collect::<Vec<_>>()
        .join("\n")
}

/// One line for a result other than success, named as in the protocol.
pub fn render_failure(response: &Response) -> String {
    let url = |data: &Value| {
        data["url"]
            .as_str()
            .or(data["tab"]["url"].as_str())
            .map(|url| format!(" · {url}"))
            .unwrap_or_default()
    };
    match &response.outcome {
        Outcome::Success { .. } => String::from("ok"),
        Outcome::Denied { reason } => format!("denied: {reason}"),
        Outcome::Unsupported { reason, .. } => format!("unsupported: {reason}"),
        Outcome::Stale { reason } if response.command.starts_with("page.") => {
            format!("stale: take a new snapshot ({reason})")
        }
        Outcome::Stale { reason } => format!("stale: {reason}"),
        Outcome::Cancelled => String::from("cancelled"),
        Outcome::Timeout => String::from("timeout"),
        Outcome::OutcomeUnknown { reason, data } => format!(
            "outcome_unknown: {reason}{}",
            data.as_ref().map(url).unwrap_or_default()
        ),
        Outcome::Invalid { reason } => format!("invalid: {reason}"),
    }
}

fn join_nonempty(parts: &[String]) -> String {
    parts
        .iter()
        .filter(|part| !part.is_empty())
        .cloned()
        .collect::<Vec<_>>()
        .join("\n")
}

/// The first request a short command sends, or a usage error.
pub fn request(
    short: &Short,
    state: &State,
    overrides: &Overrides,
) -> Result<(String, Value), String> {
    let tab = || state.tab(overrides);
    let act = |command: &str, element: Option<&str>| -> Result<(String, Value), String> {
        let tab = tab()?;
        let revision = state.revision(&tab, overrides)?;
        let mut params = json!({ "tab": tab, "revision": revision });
        if let Some(element) = element {
            params["ref"] = json!(element);
        }
        Ok((command.to_owned(), params))
    };
    let on_tab = |command: &str| Ok((command.to_owned(), json!({ "tab": tab()? })));
    match short {
        Short::Open { url } => Ok((String::from("tab.open"), json!({ "url": url }))),
        Short::Snapshot { .. } => on_tab("page.snapshot"),
        Short::Click { element } => act("page.click", Some(element)),
        Short::Fill { element, text } => act("page.fill", Some(element)).map(|(c, mut p)| {
            p["text"] = json!(text);
            (c, p)
        }),
        Short::Select { element, value } => act("page.select", Some(element)).map(|(c, mut p)| {
            p["value"] = json!(value);
            (c, p)
        }),
        Short::Press { key, element } => act("page.press", element.as_deref()).map(|(c, mut p)| {
            p["key"] = json!(key);
            (c, p)
        }),
        Short::Back => on_tab("tab.back"),
        Short::Forward => on_tab("tab.forward"),
        Short::Reload => on_tab("tab.reload"),
        Short::Close => on_tab("tab.close"),
        Short::Tabs => Ok((String::from("tabs.list"), json!({}))),
        Short::Use { tab } => Ok((String::new(), json!({ "tab": tab }))),
    }
}

/// How an action names itself in its status line, such as `click e4`.
fn label(short: &Short) -> String {
    match short {
        Short::Click { element } => format!("click {element}"),
        Short::Fill { element, .. } => format!("fill {element}"),
        Short::Select { element, .. } => format!("select {element}"),
        Short::Press {
            key,
            element: Some(element),
        } => format!("press {key} {element}"),
        Short::Press { key, .. } => format!("press {key}"),
        Short::Open { .. } => String::from("open"),
        Short::Back => String::from("back"),
        Short::Forward => String::from("forward"),
        Short::Reload => String::from("reload"),
        _ => String::new(),
    }
}

/// Runs a short command against the running myArc and prints its result.
pub fn run(short: Short, overrides: Overrides, json_output: bool) -> ExitCode {
    let Some(path) = state_path() else {
        write_line(
            &mut io::stderr(),
            &format!(
                "myarcctl: can't find the control endpoint; set {} or HOME",
                ipc::DIR_ENV
            ),
        );
        return ExitCode::from(EXIT_NOT_CONNECTED);
    };
    let mut state = State::load(&path);
    let (command, params) = match request(&short, &state, &overrides) {
        Ok(request) => request,
        Err(message) => return crate::usage_error(&message),
    };
    if let Short::Use { tab } = &short {
        state = State {
            tab: Some(tab.clone()),
            ..State::default()
        };
        let text = if json_output {
            json!({ "tab": tab }).to_string()
        } else {
            format!("current tab {tab}")
        };
        return finish(&state, &path, write_line(&mut io::stdout(), &text));
    }
    let mut client = match connect() {
        Ok(client) => client,
        Err(message) => {
            write_line(&mut io::stderr(), &format!("myarcctl: {message}"));
            return ExitCode::from(EXIT_NOT_CONNECTED);
        }
    };
    let seen = state.seen.clone();
    let first = send(&mut client, command, params);
    state.apply(&first);
    let mut out = Output::new(json_output);
    out.response(&first);
    let navigates = matches!(
        short,
        Short::Open { .. } | Short::Back | Short::Forward | Short::Reload
    );
    let ok = match &first.outcome {
        Outcome::Success { data } if navigates => {
            let handle = data["tab"]["handle"]
                .as_str()
                .unwrap_or_default()
                .to_owned();
            let snapshot = send(
                &mut client,
                String::from("page.snapshot"),
                json!({ "tab": handle }),
            );
            state.apply(&snapshot);
            out.response(&snapshot);
            match &snapshot.outcome {
                Outcome::Success { data } => {
                    out.text(&format!(
                        "ok {} {}",
                        label(&short),
                        render_snapshot(data, false)
                    ));
                    true
                }
                _ => {
                    let url = data["tab"]["url"].as_str().unwrap_or_default();
                    out.text(&format!("ok {} {handle} · {url}", label(&short)));
                    out.failure(&snapshot);
                    false
                }
            }
        }
        Outcome::Success { data } => {
            let text = match &short {
                Short::Snapshot { interactive } => render_snapshot(data, *interactive),
                Short::Tabs => render_tabs(data, state.tab.as_deref()),
                Short::Close => format!(
                    "ok close {}",
                    data["tab"]["handle"].as_str().unwrap_or_default()
                ),
                _ => render_action(&label(&short), data, &seen),
            };
            out.text(&text);
            true
        }
        _ => {
            out.failure(&first);
            false
        }
    };
    finish(&state, &path, ok && out.printed)
}

fn finish(state: &State, path: &Path, ok: bool) -> ExitCode {
    if let Err(error) = state.save(path) {
        write_line(
            &mut io::stderr(),
            &format!("myarcctl: can't save {}: {error}", path.display()),
        );
        return ExitCode::FAILURE;
    }
    if ok {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn send(client: &mut Client, command: String, params: Value) -> Response {
    let request = Request {
        id: request_id(),
        command,
        params,
    };
    if std::env::var_os(ECHO_ENV).is_some_and(|value| !value.is_empty()) {
        if let Ok(text) = serde_json::to_string(&request) {
            write_line(&mut io::stderr(), &format!("request: {text}"));
        }
    }
    call(client, request).unwrap_or_else(|response| response)
}

/// Prints plain text, or each raw response as one JSON line with `--json`.
struct Output {
    json: bool,
    printed: bool,
}

impl Output {
    fn new(json: bool) -> Self {
        Self {
            json,
            printed: true,
        }
    }

    fn response(&mut self, response: &Response) {
        if !self.json {
            return;
        }
        let printed = match serde_json::to_string(response) {
            Ok(text) => write_line(&mut io::stdout(), &text),
            Err(error) => write_line(&mut io::stderr(), &format!("myarcctl: {error}")),
        };
        self.printed &= printed;
    }

    fn text(&mut self, text: &str) {
        if !self.json {
            self.printed &= write_line(&mut io::stdout(), text);
        }
    }

    fn failure(&mut self, response: &Response) {
        if !self.json {
            write_line(&mut io::stderr(), &render_failure(response));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const OUTLINE: &str = "\
main
  heading \"Agent page one\" [level=1]
  form
    textbox \"Email\" [ref=e1] [required] value=\"\"
    checkbox \"Send me news\" [ref=e2]
    group \"Plan\"
      text \"Pick one\"
      combobox \"Plan\" [ref=e3] [collapsed] value=\"Pro\"
    button \"Send\" [ref=e4] [focused]
  text \"Fake button \\\"x\\\" [ref=e9]\"
  link \"Docs\" [ref=e5] -> /docs.html
  text \"Read more\"
footer
  text \"(c) 2026\"";

    fn response(command: &str, outcome: Outcome) -> Response {
        Response {
            id: String::from("cli-1"),
            command: command.to_owned(),
            outcome,
        }
    }

    fn success(command: &str, data: Value) -> Response {
        response(command, Outcome::Success { data })
    }

    fn temp_path(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("myarcctl-unit-{}-{name}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        dir.join("nested").join(STATE_FILE)
    }

    fn action_data() -> Value {
        json!({
            "tab": "t12",
            "url": "https://example.test/",
            "revision": "1.5",
            "navigated": false,
            "settled": true,
            "changes": {
                "added": ["text \"Thanks\" [ref=e6]"],
                "changed": [
                    "button \"Send\" [ref=e4]",
                    "textbox \"Email\" [ref=e1] [focused] [required] value=\"me@example.test\"",
                ],
            },
            "snapshot": {
                "tab": "t12",
                "revision": "1.5",
                "text": "form\n  textbox \"Email\" [ref=e1] [focused] [required] value=\"me@example.test\"\n  button \"Send\" [ref=e4]",
            },
        })
    }

    #[test]
    fn refs_accept_an_at_sign_or_none() {
        assert_eq!(parse_ref("@e4"), Ok(String::from("e4")));
        assert_eq!(parse_ref("e12"), Ok(String::from("e12")));
        for bad in [
            "", "@", "4", "@e", "e0", "e04", "@@e4", "E4", "e4x", "ref=e4",
        ] {
            assert!(parse_ref(bad).is_err(), "{bad}");
        }
        assert!(parse_ref("x").unwrap_err().contains("such as @e4"));
    }

    #[test]
    fn parse_builds_short_commands() {
        assert_eq!(
            parse(&["click", "@e4"]),
            Some(Ok(Short::Click {
                element: String::from("e4")
            }))
        );
        assert_eq!(
            parse(&["press", "Enter", "e1"]),
            Some(Ok(Short::Press {
                key: String::from("Enter"),
                element: Some(String::from("e1"))
            }))
        );
        assert_eq!(
            parse(&["snapshot", "-i"]),
            Some(Ok(Short::Snapshot { interactive: true }))
        );
        assert_eq!(
            parse(&["fill", "e1", "-i"]),
            Some(Ok(Short::Fill {
                element: String::from("e1"),
                text: String::from("-i")
            }))
        );
        assert!(matches!(parse(&["click"]), Some(Err(_))));
        assert!(matches!(parse(&["use", "tab12"]), Some(Err(_))));
        assert!(matches!(parse(&["back", "now"]), Some(Err(_))));
        assert_eq!(parse(&["run", "space.list"]), None);
        assert_eq!(parse(&[]), None);
    }

    #[test]
    fn interactive_keeps_ref_lines_and_their_ancestors() {
        assert_eq!(
            interactive(OUTLINE),
            "\
main
  form
    textbox \"Email\" [ref=e1] [required] value=\"\"
    checkbox \"Send me news\" [ref=e2]
    group \"Plan\"
      combobox \"Plan\" [ref=e3] [collapsed] value=\"Pro\"
    button \"Send\" [ref=e4] [focused]
  link \"Docs\" [ref=e5] -> /docs.html"
        );
        assert_eq!(interactive("heading \"x\"\ntext \"[ref=e1]\""), "");
    }

    #[test]
    fn has_ref_ignores_quoted_text_and_link_targets() {
        assert!(has_ref("button \"Send\" [ref=e4]"));
        assert!(!has_ref("text \"button [ref=e4]\""));
        assert!(!has_ref("text \"a \\\" [ref=e4]\""));
        assert!(!has_ref("link \"x\" -> /a[ref=e1]"));
        assert!(!has_ref("heading \"x\" [level=1]"));
    }

    #[test]
    fn focus_only_changes_are_dropped() {
        let seen = ref_hashes(OUTLINE);
        let changes = json!({
            "added": ["button \"Undo\" [ref=e6]"],
            "removed": ["link \"Docs\" -> /docs.html (was e5)"],
            "changed": [
                // Gained focus only.
                "textbox \"Email\" [ref=e1] [required] [focused] value=\"\"",
                // Lost focus only.
                "button \"Send\" [ref=e4]",
                // Gained focus and a value: a real change.
                "textbox \"Email\" [ref=e1] [focused] [required] value=\"a\"",
                // A name that only mentions focus in page text.
                "checkbox \"Send me news [focused]\" [ref=e2]",
            ],
            "more": 2,
        });
        assert_eq!(
            change_lines(&changes, &seen),
            vec![
                "added: button \"Undo\" [ref=e6]",
                "removed: link \"Docs\" -> /docs.html (was e5)",
                "changed: textbox \"Email\" [ref=e1] [focused] [required] value=\"a\"",
                "changed: checkbox \"Send me news [focused]\" [ref=e2]",
                "(2 more changes)",
            ]
        );
        // Without a previous outline nothing counts as focus-only.
        assert_eq!(change_lines(&changes, &[]).len(), 7);
    }

    #[test]
    fn action_output_has_status_changes_and_outline() {
        let seen = ref_hashes(
            "form\n  textbox \"Email\" [ref=e1] [required] value=\"\"\n  button \"Send\" [ref=e4] [focused]",
        );
        assert_eq!(
            render_action("click e4", &action_data(), &seen),
            "\
ok click e4 · rev 1.5
added: text \"Thanks\" [ref=e6]
changed: textbox \"Email\" [ref=e1] [focused] [required] value=\"me@example.test\"
form
  textbox \"Email\" [ref=e1] [focused] [required] value=\"me@example.test\"
  button \"Send\" [ref=e4]"
        );
        let mut data = action_data();
        data["navigated"] = json!(true);
        data["settled"] = json!(false);
        data.as_object_mut().unwrap().remove("changes");
        let text = render_action("press Enter", &data, &seen);
        assert!(
            text.starts_with(
                "ok press Enter · rev 1.5 · navigated · https://example.test/ · not settled\nform"
            ),
            "{text}"
        );
    }

    #[test]
    fn snapshot_and_tabs_output() {
        let data = json!({
            "tab": "t12", "url": "https://example.test/", "title": "Two\nlines",
            "revision": "1.2", "loading": true, "truncated": false, "text": OUTLINE,
        });
        let text = render_snapshot(&data, true);
        assert!(
            text.starts_with(
                "t12 · rev 1.2 · \"Two\\nlines\" · https://example.test/ · loading\nmain\n  form\n"
            ),
            "{text}"
        );
        let tabs = json!({ "tabs": [
            { "handle": "t3", "title": "A", "url": "https://a.test/", "loading": false },
            { "handle": "t12", "title": "B", "url": "https://b.test/", "loading": true },
        ]});
        assert_eq!(
            render_tabs(&tabs, Some("t12")),
            "  t3 \"A\" https://a.test/\n* t12 \"B\" https://b.test/ · loading"
        );
        assert_eq!(
            render_tabs(&json!({ "tabs": [] }), None),
            "no tabs in the agent Space"
        );
    }

    #[test]
    fn failures_name_the_result() {
        let stale = response(
            "page.click",
            Outcome::Stale {
                reason: String::from("revision 1.2 is old"),
            },
        );
        assert_eq!(
            render_failure(&stale),
            "stale: take a new snapshot (revision 1.2 is old)"
        );
        let unknown = response(
            "page.click",
            Outcome::OutcomeUnknown {
                reason: String::from("timed out"),
                data: Some(json!({ "tab": "t1", "url": "https://x.test/" })),
            },
        );
        assert_eq!(
            render_failure(&unknown),
            "outcome_unknown: timed out · https://x.test/"
        );
    }

    #[test]
    fn state_file_is_private_and_round_trips() {
        let path = temp_path("roundtrip");
        assert_eq!(State::load(&path), State::default());
        let state = State {
            tab: Some(String::from("t12")),
            revision: Some(String::from("1.5")),
            seen: ref_hashes(OUTLINE),
        };
        state.save(&path).unwrap();
        let mode = |path: &Path| fs::metadata(path).unwrap().permissions().mode() & 0o777;
        assert_eq!(mode(&path), 0o600);
        assert_eq!(mode(path.parent().unwrap()), 0o700);
        assert_eq!(State::load(&path), state);

        // An existing file that others can read becomes private again.
        fs::set_permissions(&path, fs::Permissions::from_mode(0o644)).unwrap();
        state.save(&path).unwrap();
        assert_eq!(mode(&path), 0o600);

        // Bad values read as missing.
        fs::write(&path, r#"{"tab":"tab 1","revision":"1.5","seen":["zz"]}"#).unwrap();
        assert_eq!(State::load(&path), State::default());
        fs::write(&path, "not json").unwrap();
        assert_eq!(State::load(&path), State::default());
        let _ = fs::remove_dir_all(path.parent().unwrap().parent().unwrap());
    }

    #[test]
    fn responses_update_the_state() {
        let mut state = State::default();
        state.apply(&success(
            "tab.open",
            json!({ "tab": { "handle": "t12", "url": "https://example.test/" } }),
        ));
        assert_eq!(state.tab.as_deref(), Some("t12"));
        assert_eq!(state.revision, None);

        state.apply(&success(
            "page.snapshot",
            json!({ "tab": "t12", "revision": "1.1", "text": OUTLINE }),
        ));
        assert_eq!(state.revision.as_deref(), Some("1.1"));
        assert_eq!(state.seen, ref_hashes(OUTLINE));

        state.apply(&success("page.click", action_data()));
        assert_eq!(state.revision.as_deref(), Some("1.5"));
        assert_ne!(state.seen, ref_hashes(OUTLINE));

        // A failed action keeps the revision; the browser decides if it's stale.
        state.apply(&response(
            "page.click",
            Outcome::Stale {
                reason: String::new(),
            },
        ));
        assert_eq!(state.revision.as_deref(), Some("1.5"));

        // Navigation clears the revision.
        state.apply(&success("tab.back", json!({ "tab": { "handle": "t12" } })));
        assert_eq!(state.revision, None);

        // Closing another tab leaves the current one; closing it clears all.
        state.revision = Some(String::from("2.1"));
        state.apply(&success(
            "tab.close",
            json!({ "tab": { "handle": "t3" }, "closed": true }),
        ));
        assert_eq!(state.tab.as_deref(), Some("t12"));
        state.apply(&success(
            "tab.close",
            json!({ "tab": { "handle": "t12" }, "closed": true }),
        ));
        assert_eq!(state, State::default());
    }

    #[test]
    fn requests_use_the_state_and_the_overrides() {
        let state = State {
            tab: Some(String::from("t12")),
            revision: Some(String::from("1.5")),
            seen: Vec::new(),
        };
        let none = Overrides::default();
        let click = Short::Click {
            element: String::from("e4"),
        };
        assert_eq!(
            request(&click, &state, &none),
            Ok((
                String::from("page.click"),
                json!({ "tab": "t12", "revision": "1.5", "ref": "e4" })
            ))
        );
        // --rev replaces the revision; --tab alone has no revision to use.
        let rev = Overrides {
            tab: None,
            revision: Some(String::from("1.4")),
        };
        assert_eq!(request(&click, &state, &rev).unwrap().1["revision"], "1.4");
        let tab = Overrides {
            tab: Some(String::from("t3")),
            revision: None,
        };
        assert!(
            request(&click, &state, &tab)
                .unwrap_err()
                .contains("no revision for t3")
        );
        assert_eq!(
            request(&Short::Snapshot { interactive: false }, &state, &tab),
            Ok((String::from("page.snapshot"), json!({ "tab": "t3" })))
        );
        assert!(
            request(&Short::Back, &State::default(), &none)
                .unwrap_err()
                .contains("no current tab")
        );
        let press = Short::Press {
            key: String::from("Enter"),
            element: None,
        };
        assert_eq!(
            request(&press, &state, &none).unwrap().1,
            json!({ "tab": "t12", "revision": "1.5", "key": "Enter" })
        );
    }
}
