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
/// Largest state file the CLI reads back.
const MAX_STATE_BYTES: u64 = 64 * 1024;

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

/// What the CLI remembers between calls: the current tab and the revision
/// it last printed for that tab.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct State {
    pub tab: Option<String>,
    pub revision: Option<String>,
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
        Self {
            tab: tab.map(str::to_owned),
            revision: revision.map(str::to_owned),
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

/// A link line's quoted name and the byte offset of its ` -> ` target, or
/// `None` for other lines.
fn link_parts(line: &str) -> Option<(&str, Option<usize>)> {
    let trimmed = line.trim_start();
    if !trimmed.starts_with("link \"") {
        return None;
    }
    let indent = line.len() - trimmed.len();
    let parts = segments(trimmed);
    let name = parts.iter().find(|(quoted, _)| *quoted)?.1;
    let mut offset = indent;
    for (quoted, run) in &parts {
        if !quoted && let Some(at) = run.find(" -> ") {
            return Some((name, Some(offset + at)));
        }
        offset += run.len();
    }
    Some((name, None))
}

fn starts_with_kind(line: &str, kind: &str) -> bool {
    line.trim_start()
        .strip_prefix(kind)
        .is_some_and(|rest| rest.starts_with(" \""))
}

/// The outline for `snapshot -i`: lines with a ref, the ancestor lines they
/// sit under, the nearest heading before each and the text line right
/// before it, such as the item a "Details" link belongs to. A link keeps its
/// target only when another kept link shares its name.
pub fn interactive(text: &str) -> String {
    let lines: Vec<&str> = text.lines().collect();
    let mut keep = vec![false; lines.len()];
    // Open ancestors by indentation, the latest heading, and the latest
    // text since the previous ref line.
    let mut stack: Vec<(usize, usize)> = Vec::new();
    let mut heading = None;
    let mut last_text = None;
    for (index, line) in lines.iter().enumerate() {
        let depth = line.len() - line.trim_start_matches(' ').len();
        while stack.last().is_some_and(|(open, _)| *open >= depth) {
            stack.pop();
        }
        if has_ref(line) {
            for (_, ancestor) in &stack {
                keep[*ancestor] = true;
            }
            for extra in [heading, last_text.take()].into_iter().flatten() {
                keep[extra] = true;
            }
            keep[index] = true;
        } else if starts_with_kind(line, "heading") {
            heading = Some(index);
            last_text = None;
        } else if starts_with_kind(line, "text") {
            last_text = Some(index);
        }
        stack.push((depth, index));
    }
    let mut names: std::collections::HashMap<&str, usize> = std::collections::HashMap::new();
    for (index, line) in lines.iter().enumerate() {
        if keep[index]
            && has_ref(line)
            && let Some((name, _)) = link_parts(line)
        {
            *names.entry(name).or_default() += 1;
        }
    }
    let mut out = Vec::new();
    for (index, line) in lines.iter().enumerate() {
        if !keep[index] {
            continue;
        }
        match link_parts(line) {
            Some((name, Some(at))) if has_ref(line) && names.get(name) == Some(&1) => {
                out.push(&line[..at]);
            }
            _ => out.push(line),
        }
    }
    out.join("\n")
}

/// The `changes` of an action as short lines, such as `changed e1 value`,
/// or `changes: many (see outline)` when the outline says more.
pub fn change_lines(changes: &Value) -> Vec<String> {
    if changes["many"] == true {
        return vec![String::from("changes: many (see outline)")];
    }
    let mut lines = Vec::new();
    for key in ["added", "removed", "changed"] {
        let list: Vec<&str> = changes[key]
            .as_array()
            .into_iter()
            .flatten()
            .filter_map(Value::as_str)
            .collect();
        if !list.is_empty() {
            lines.push(format!("{key} {}", list.join(", ")));
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
pub fn render_action(label: &str, data: &Value) -> String {
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
    lines.extend(change_lines(&data["changes"]));
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
            let again = format!("; {}", myarc_control::snapshot::AGAIN);
            let reason = reason.strip_suffix(&again).unwrap_or(reason);
            format!("stale: take a new snapshot ({reason})")
        }
        Outcome::Stale { reason } => format!("stale: {reason}"),
        Outcome::Cancelled => String::from("cancelled"),
        Outcome::Timeout => String::from("timeout"),
        Outcome::OutcomeUnknown { reason, data } => format!(
            "outcome_unknown: {}{}",
            short_names(reason),
            data.as_ref().map(url).unwrap_or_default()
        ),
        Outcome::Invalid { reason } => format!("invalid: {}", short_names(reason)),
    }
}

/// Names the short commands in a browser hint instead of the wire ones.
fn short_names(reason: &str) -> String {
    reason
        .replace("reload it and take page.snapshot", "run reload")
        .replace("reload it with tab.reload", "run reload")
        .replace("take page.snapshot", "run snapshot")
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
                _ => render_action(&label(&short), data),
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
                "added": ["e6"],
                "changed": ["e1 value"],
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
    fn interactive_keeps_ref_lines_their_ancestors_heading_and_text() {
        assert_eq!(
            interactive(OUTLINE),
            "\
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
  link \"Docs\" [ref=e5]"
        );
        assert_eq!(interactive("heading \"x\"\ntext \"[ref=e1]\""), "");
    }

    /// The list page of the 2026-11-06 dogfood: five "Details" links that
    /// differ only by target.
    const LIST: &str = "\
heading \"Lanterns\" [level=1]
searchbox \"Search\" [ref=e1] value=\"\"
text \"Item 1: Copper storm lantern\"
link \"Details\" [ref=e2] -> /details.html?id=1
text \"Item 2: Paper festival lantern\"
link \"Details\" [ref=e3] -> /details.html?id=2
text \"Item 3: Brass ship lantern\"
link \"Details\" [ref=e4] -> /details.html?id=3
text \"5 lanterns\"
link \"Help\" [ref=e7] -> /help.html
heading \"Footer\" [level=2]
text \"Made with care\"";

    #[test]
    fn interactive_keeps_list_items_and_targets_only_where_names_repeat() {
        assert_eq!(
            interactive(LIST),
            "\
heading \"Lanterns\" [level=1]
searchbox \"Search\" [ref=e1] value=\"\"
text \"Item 1: Copper storm lantern\"
link \"Details\" [ref=e2] -> /details.html?id=1
text \"Item 2: Paper festival lantern\"
link \"Details\" [ref=e3] -> /details.html?id=2
text \"Item 3: Brass ship lantern\"
link \"Details\" [ref=e4] -> /details.html?id=3
text \"5 lanterns\"
link \"Help\" [ref=e7]"
        );
        // The details page: the heading names the item above its button.
        assert_eq!(
            interactive(
                "heading \"Item 3: Brass ship lantern\" [level=1]\ntext \"Stock 12, shelf B3\"\nbutton \"Delete\" [ref=e1]\nlink \"Back to list\" [ref=e2] -> /list.html"
            ),
            "heading \"Item 3: Brass ship lantern\" [level=1]\ntext \"Stock 12, shelf B3\"\nbutton \"Delete\" [ref=e1]\nlink \"Back to list\" [ref=e2]"
        );
        // A quoted " -> " in a link's name is page text, not its target.
        assert_eq!(
            interactive("link \"a -> b\" [ref=e1] -> /x"),
            "link \"a -> b\" [ref=e1]"
        );
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
    fn changes_print_short_or_give_way_to_the_outline() {
        let changes = json!({
            "added": ["e6", "e7"],
            "removed": ["e5"],
            "changed": ["e1 value", "e3 name checked"],
            "more": 2,
        });
        assert_eq!(
            change_lines(&changes),
            vec![
                "added e6, e7",
                "removed e5",
                "changed e1 value, e3 name checked",
                "(2 more changes)",
            ]
        );
        assert_eq!(
            change_lines(&json!({ "many": true })),
            vec!["changes: many (see outline)"]
        );
        assert!(change_lines(&json!({})).is_empty());
    }

    #[test]
    fn action_output_has_status_changes_and_outline() {
        assert_eq!(
            render_action("click e4", &action_data()),
            "\
ok click e4 · rev 1.5
added e6
changed e1 value
form
  textbox \"Email\" [ref=e1] [focused] [required] value=\"me@example.test\"
  button \"Send\" [ref=e4]"
        );
        let mut data = action_data();
        data["navigated"] = json!(true);
        data["settled"] = json!(false);
        data.as_object_mut().unwrap().remove("changes");
        let text = render_action("press Enter", &data);
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
                "t12 · rev 1.2 · \"Two\\nlines\" · https://example.test/ · loading\nmain\n  heading "
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
        let wire = response(
            "page.click",
            Outcome::Stale {
                reason: format!("revision 1.2 is old; {}", myarc_control::snapshot::AGAIN),
            },
        );
        assert_eq!(
            render_failure(&wire),
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
        let crashed = response(
            "page.click",
            Outcome::OutcomeUnknown {
                reason: String::from(
                    "The page crashed during the action; reload it and take page.snapshot",
                ),
                data: None,
            },
        );
        assert_eq!(
            render_failure(&crashed),
            "outcome_unknown: The page crashed during the action; run reload"
        );
        let gone = response(
            "page.snapshot",
            Outcome::Invalid {
                reason: String::from("The page crashed; reload it with tab.reload first"),
            },
        );
        assert_eq!(
            render_failure(&gone),
            "invalid: The page crashed; run reload first"
        );
    }

    #[test]
    fn state_file_is_private_and_round_trips() {
        let path = temp_path("roundtrip");
        assert_eq!(State::load(&path), State::default());
        let state = State {
            tab: Some(String::from("t12")),
            revision: Some(String::from("1.5")),
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
        fs::write(&path, r#"{"tab":"tab 1","revision":"1.5"}"#).unwrap();
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

        state.apply(&success("page.click", action_data()));
        assert_eq!(state.revision.as_deref(), Some("1.5"));

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
