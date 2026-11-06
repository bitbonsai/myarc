#![forbid(unsafe_code)]

//! Shared control layer for MA-011 browser control. The `myarcctl` CLI and its
//! MCP server translate requests into `Request` values and print `Response`
//! values; both use the one command registry below.
//!
//! [`dispatch`] runs the Space commands against a workspace the caller owns.
//! [`ControlService`] wraps it around an in-memory workspace for
//! `myarcctl --local` and `serve-local`, where the tab commands answer
//! `Unsupported` because there are no pages. A browser calls [`route`]
//! instead: it answers the Space commands the same way and turns each tab
//! command into a [`TabAction`] scoped to the agent Space, which the browser
//! runs and answers later with [`tab_response`]. `page.snapshot` becomes
//! [`TabAction::Snapshot`] the same way, and [`snapshot`] formats the page's
//! accessibility tree into the answer. `page.click`, `page.fill`,
//! `page.select` and `page.press` become [`TabAction::Act`], which
//! [`actions`] checks against the latest snapshot and answers with a new
//! one. Commands not built yet are
//! listed in the registry with `implemented: false` and answer `Unsupported`.
//! Requests are untrusted: strings are bounded, unknown parameters are
//! rejected and no input panics.
//!
//! On Unix, `ipc` carries requests from a client process to the service hosted
//! by a running browser over an authenticated same-user socket.

use std::sync::OnceLock;

pub mod actions;
#[cfg(unix)]
pub mod ipc;
pub mod snapshot;

use myarc_workspace::{
    DEFAULT_PROFILE_ID, DEFAULT_WINDOW_ID, ItemId, MAX_TITLE_BYTES, Placement, RenameError,
    SpaceId, WindowId, Workspace, WorkspaceError,
};
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value, json};

pub const PROTOCOL_VERSION: &str = "0.1.0";
/// Longest request ID echoed back in a response.
pub const MAX_REQUEST_ID_BYTES: usize = 128;
/// Longest command ID accepted; registry IDs are far shorter.
pub const MAX_COMMAND_BYTES: usize = 64;
/// Longest string parameter accepted before the workspace applies its own
/// limits, such as 128 bytes for a Space name.
pub const MAX_STRING_PARAM_BYTES: usize = 256;
/// Longest piece of untrusted input quoted inside an error message.
const MAX_QUOTED_BYTES: usize = 64;
/// Longest URL a tab command accepts or reports, 8 KiB.
pub const MAX_URL_LENGTH: usize = myarc_workspace::MAX_URL_BYTES;
/// How long a browser waits for a tab's navigation to commit before it
/// answers `outcome_unknown` with the latest state. Shorter than the host's
/// 10 second reply timeout, so the client gets that state, not `timeout`.
pub const NAVIGATION_WAIT_MS: u64 = 9_000;
const MAX_PATH_LENGTH: usize = 4096;

/// Group a command belongs to, matching the areas in docs/CONTROL-PROTOCOL.md.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Area {
    Session,
    Tabs,
    Observation,
    Interaction,
    Workspace,
    Preservation,
}

/// One entry in the command registry. `params` is a JSON Schema object that
/// the MCP server can publish as a tool's input schema.
#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct CommandSpec {
    pub id: &'static str,
    pub area: Area,
    pub summary: &'static str,
    pub params: Value,
    pub implemented: bool,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Request {
    /// Caller-chosen request ID, echoed in the response.
    pub id: String,
    pub command: String,
    #[serde(default)]
    pub params: Value,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "status", rename_all = "snake_case")]
pub enum Outcome {
    Success {
        data: Value,
    },
    Denied {
        reason: String,
    },
    Unsupported {
        reason: String,
    },
    Stale {
        reason: String,
    },
    Cancelled,
    Timeout,
    OutcomeUnknown {
        reason: String,
        /// Latest known state, such as a tab whose navigation had not
        /// committed yet.
        #[serde(default, skip_serializing_if = "Option::is_none")]
        data: Option<Value>,
    },
    /// Bad parameters or an unknown command.
    Invalid {
        reason: String,
    },
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Response {
    pub id: String,
    pub command: String,
    #[serde(flatten)]
    pub outcome: Outcome,
}

/// Every command in the versioned control surface, in the order of the spec
/// table. The UI-only Stop control is not a command.
pub fn registry() -> &'static [CommandSpec] {
    static REGISTRY: OnceLock<Vec<CommandSpec>> = OnceLock::new();
    REGISTRY.get_or_init(build_registry)
}

/// Runs registry commands against an in-memory workspace for one window. The
/// browser keeps its own workspace and calls [`dispatch`] instead.
#[derive(Debug)]
pub struct ControlService {
    workspace: Workspace,
    window_id: WindowId,
}

impl Default for ControlService {
    fn default() -> Self {
        Self::new()
    }
}

impl ControlService {
    /// Starts from the default workspace, which holds one Space (Everyday)
    /// shown in Window 1.
    pub fn new() -> Self {
        Self {
            workspace: Workspace::default(),
            window_id: DEFAULT_WINDOW_ID,
        }
    }

    pub fn handle(&mut self, req: Request) -> Response {
        dispatch(&mut self.workspace, self.window_id, req)
    }
}

/// Runs one request against a workspace the caller owns. `window_id` is the
/// window `space.select` changes and `selected` describes. Commands act on
/// the workspace's own Profile, Profile 1. The caller saves and refreshes its
/// UI after a change.
/// The tab commands answer `unsupported` here, since there are no pages.
pub fn dispatch(workspace: &mut Workspace, window_id: WindowId, req: Request) -> Response {
    match run(workspace, window_id, &req, false) {
        Routed::Answered(response) => response,
        // Not reached: without pages, tab commands are answered above.
        Routed::Tab(_) => answer(
            &req,
            Outcome::Unsupported {
                reason: no_pages(&req.command),
            },
        ),
    }
}

/// A request [`route`] has handled.
#[derive(Clone, Debug, PartialEq)]
pub enum Routed {
    /// Ran in the core, or failed its checks; send this answer.
    Answered(Response),
    /// A tab command that passed its checks. The browser runs it and answers
    /// with [`tab_response`].
    Tab(TabAction),
}

/// What the browser does for a tab command after the core checked its
/// parameters and scope. Every `item` is a tab in the agent Space of the
/// workspace's Profile.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum TabAction {
    /// Report the tabs among `items` that have a live page, in this order.
    List {
        items: Vec<ItemId>,
    },
    /// Open `url` as a temporary tab of `space_id`, the agent Space, without
    /// switching the window's Space or selecting the tab.
    Open {
        space_id: SpaceId,
        url: String,
    },
    Activate {
        item: ItemId,
    },
    Close {
        item: ItemId,
    },
    Navigate {
        item: ItemId,
        url: String,
    },
    Back {
        item: ItemId,
    },
    Forward {
        item: ItemId,
    },
    Reload {
        item: ItemId,
    },
    /// Snapshot the page's accessibility tree and answer with
    /// [`snapshot::snapshot_response`].
    Snapshot {
        item: ItemId,
    },
    /// Act on an element of the latest snapshot and answer with
    /// [`actions::action_response`].
    Act(actions::PageAction),
}

/// Like [`dispatch`] for a host with live pages, such as the browser. Space
/// commands run at once. A tab command is checked against the agent Space and
/// handed back as a [`TabAction`]; `tab.open` creates the agent Space first
/// when the Profile has none.
pub fn route(workspace: &mut Workspace, window_id: WindowId, req: &Request) -> Routed {
    run(workspace, window_id, req, true)
}

fn run(workspace: &mut Workspace, window_id: WindowId, req: &Request, live: bool) -> Routed {
    let Request {
        id,
        command,
        params,
    } = req;
    if id.len() > MAX_REQUEST_ID_BYTES {
        return Routed::Answered(Response {
            id: String::new(),
            command: bounded(command).to_owned(),
            outcome: invalid(format!(
                "Request ID exceeds {MAX_REQUEST_ID_BYTES} bytes; use a shorter ID"
            )),
        });
    }
    if command.len() > MAX_COMMAND_BYTES {
        return Routed::Answered(Response {
            id: id.clone(),
            command: String::new(),
            outcome: invalid(format!(
                "Command exceeds {MAX_COMMAND_BYTES} bytes; use an ID from the registry"
            )),
        });
    }
    let outcome = match registry().iter().find(|spec| spec.id == command) {
        None => invalid(format!(
            "Unknown command {:?}; use an ID from the registry, such as space.list",
            command
        )),
        Some(spec) if !spec.implemented => Outcome::Unsupported {
            reason: format!(
                "{command} needs live browser attachment (MA-011), which this core stub does not have"
            ),
        },
        Some(spec) if on_pages(spec.area) && !live => Outcome::Unsupported {
            reason: no_pages(command),
        },
        Some(spec) if on_pages(spec.area) => match tab_action(workspace, command, params) {
            Ok(action) => return Routed::Tab(action),
            Err(outcome) => outcome,
        },
        Some(_) => {
            let mut context = Context {
                workspace,
                window_id,
            };
            match context.run(command, params) {
                Ok(data) => Outcome::Success { data },
                Err(reason) => invalid(reason),
            }
        }
    };
    Routed::Answered(Response {
        id: id.clone(),
        command: command.clone(),
        outcome,
    })
}

/// Areas whose commands act on live pages.
fn on_pages(area: Area) -> bool {
    matches!(area, Area::Tabs | Area::Observation | Area::Interaction)
}

fn no_pages(command: &str) -> String {
    format!(
        "{} needs a running myArc browser; --local and serve-local have no tabs",
        bounded(command)
    )
}

/// Stable handle of a tab, valid while its item lives.
pub fn tab_handle(item: ItemId) -> String {
    format!("t{item}")
}

/// The item a handle from [`tab_handle`] names, or `None` when `handle` is
/// not one.
pub fn parse_tab_handle(handle: &str) -> Option<ItemId> {
    let digits = handle.strip_prefix('t')?;
    if digits.is_empty()
        || digits.len() > 20
        || digits.starts_with('0')
        || !digits.bytes().all(|byte| byte.is_ascii_digit())
    {
        return None;
    }
    digits.parse().ok()
}

/// Checks a URL an agent asked to load. It must be at most
/// [`MAX_URL_LENGTH`] bytes with no whitespace or control characters, use
/// http or https and carry no user name or password. Anything else, such as
/// `chrome:`, `file:`, `javascript:` or `data:`, is denied. The browser
/// parses the URL again and repeats the scheme check on its result.
pub fn check_url(url: &str) -> Result<(), Outcome> {
    if url.len() > MAX_URL_LENGTH {
        return Err(invalid(format!("URL exceeds {MAX_URL_LENGTH} bytes")));
    }
    if url.chars().any(|c| c.is_whitespace() || c.is_control()) {
        return Err(invalid(String::from(
            "URL contains whitespace or control characters; percent-encode them",
        )));
    }
    let scheme = url
        .split_once(':')
        .map(|(scheme, _)| scheme)
        .filter(|scheme| {
            scheme.starts_with(|c: char| c.is_ascii_alphabetic())
                && scheme
                    .bytes()
                    .all(|byte| byte.is_ascii_alphanumeric() || b"+-.".contains(&byte))
        });
    let Some(scheme) = scheme else {
        return Err(invalid(String::from(
            "URL needs a scheme; use one that starts with https://",
        )));
    };
    if !scheme.eq_ignore_ascii_case("http") && !scheme.eq_ignore_ascii_case("https") {
        return Err(Outcome::Denied {
            reason: format!(
                "Only http and https URLs are allowed, not {:?}",
                bounded(scheme)
            ),
        });
    }
    let rest = url[scheme.len() + 1..].trim_start_matches(['/', '\\']);
    let authority = rest.split(['/', '\\', '?', '#']).next().unwrap_or_default();
    if authority.contains('@') {
        return Err(Outcome::Denied {
            reason: String::from("URLs with a user name or password are not allowed"),
        });
    }
    Ok(())
}

fn tab_action(
    workspace: &mut Workspace,
    command: &str,
    params: &Value,
) -> Result<TabAction, Outcome> {
    match command {
        "tabs.list" => {
            Params::parse(params, &[]).map_err(invalid)?;
            let agent = workspace.agent_space_of(DEFAULT_PROFILE_ID);
            let items = workspace
                .items()
                .iter()
                .filter(|item| agent.is_some() && space_of(item.placement) == agent)
                .map(|item| item.id)
                .collect();
            Ok(TabAction::List { items })
        }
        "tab.open" => {
            let params = Params::parse(params, &["url"]).map_err(invalid)?;
            let url = params.url()?;
            let space_id = workspace
                .ensure_agent_space(DEFAULT_PROFILE_ID)
                .map_err(|error| invalid(workspace_error(error)))?;
            Ok(TabAction::Open { space_id, url })
        }
        "page.click" | "page.fill" | "page.select" | "page.press" => {
            page_action(workspace, command, params).map(TabAction::Act)
        }
        "tab.navigate" => {
            let params = Params::parse(params, &["tab", "url"]).map_err(invalid)?;
            let item = agent_tab(workspace, &params)?;
            let url = params.url()?;
            Ok(TabAction::Navigate { item, url })
        }
        _ => {
            let params = Params::parse(params, &["tab"]).map_err(invalid)?;
            let item = agent_tab(workspace, &params)?;
            match command {
                "tab.activate" => Ok(TabAction::Activate { item }),
                "tab.close" => Ok(TabAction::Close { item }),
                "tab.back" => Ok(TabAction::Back { item }),
                "tab.forward" => Ok(TabAction::Forward { item }),
                "tab.reload" => Ok(TabAction::Reload { item }),
                "page.snapshot" => Ok(TabAction::Snapshot { item }),
                other => Err(invalid(format!(
                    "{other} is registered as implemented but has no handler"
                ))),
            }
        }
    }
}

/// Checks the parameters of an interaction command.
fn page_action(
    workspace: &Workspace,
    command: &str,
    params: &Value,
) -> Result<actions::PageAction, Outcome> {
    use actions::ActKind;
    let extra: &[&str] = match command {
        "page.fill" => &["tab", "revision", "ref", "text"],
        "page.select" => &["tab", "revision", "ref", "value"],
        "page.press" => &["tab", "revision", "ref", "key"],
        _ => &["tab", "revision", "ref"],
    };
    let params = Params::parse(params, extra).map_err(invalid)?;
    let item = agent_tab(workspace, &params)?;
    let revision = params.text("revision", MAX_QUOTED_BYTES).map_err(invalid)?;
    actions::check_revision(revision).map_err(invalid)?;
    let element = if command == "page.press" && params.get("ref").is_err() {
        None
    } else {
        let element = params.text("ref", MAX_QUOTED_BYTES).map_err(invalid)?;
        actions::check_ref(element).map_err(invalid)?;
        Some(element.to_owned())
    };
    let kind = match command {
        "page.fill" => {
            let text = params
                .text("text", actions::MAX_FILL_BYTES)
                .map_err(invalid)?;
            actions::check_fill_text(text).map_err(invalid)?;
            ActKind::Fill {
                text: text.to_owned(),
            }
        }
        "page.select" => ActKind::Select {
            value: params.string("value").map_err(invalid)?.to_owned(),
        },
        "page.press" => ActKind::Press {
            key: actions::parse_key(params.text("key", MAX_QUOTED_BYTES).map_err(invalid)?)
                .map_err(invalid)?,
        },
        _ => ActKind::Click,
    };
    Ok(actions::PageAction {
        item,
        revision: revision.to_owned(),
        element,
        kind,
    })
}

fn space_of(placement: Placement) -> Option<SpaceId> {
    match placement {
        Placement::Temporary { space_id } | Placement::Pin { space_id } => Some(space_id),
        Placement::Favorite { .. } => None,
    }
}

/// The item behind the `tab` parameter: `stale` when no such item lives,
/// `denied` when it is not in the agent Space.
fn agent_tab(workspace: &Workspace, params: &Params) -> Result<ItemId, Outcome> {
    let handle = params.string("tab").map_err(invalid)?;
    let Some(item) = parse_tab_handle(handle) else {
        return Err(invalid(format!(
            "Tab handle {:?} is not a handle from tabs.list, such as t12",
            bounded(handle)
        )));
    };
    let Some(found) = workspace.items().iter().find(|found| found.id == item) else {
        return Err(Outcome::Stale {
            reason: format!("Tab {handle} is closed or unknown; run tabs.list for current handles"),
        });
    };
    let agent = workspace.agent_space_of(DEFAULT_PROFILE_ID);
    if agent.is_none() || space_of(found.placement) != agent {
        return Err(Outcome::Denied {
            reason: format!("Tab {handle} is outside the agent Space"),
        });
    }
    Ok(item)
}

/// State of one tab as the browser reports it after a tab command.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct TabState {
    pub item: ItemId,
    pub url: String,
    pub title: String,
    pub loading: bool,
    pub can_go_back: bool,
    pub can_go_forward: bool,
}

/// How the browser finished a [`TabAction`].
#[derive(Clone, Debug, PartialEq)]
pub enum TabResult {
    /// Done. `tabs` holds the tab afterwards, every live tab for `tabs.list`,
    /// or the tab just before it closed for `tab.close`.
    Done(Vec<TabState>),
    /// The navigation did not commit in time, or ended without committing.
    /// `tabs` holds the tab's latest state.
    Unknown { reason: String, tabs: Vec<TabState> },
    /// Not run, or the tab went away, such as `stale` or `denied`.
    Failed(Outcome),
}

/// The answer to a tab command from how the browser finished it. Data is
/// `{"tabs": [...]}` for `tabs.list` and `{"tab": {...}}` otherwise, with
/// `"closed": true` added for `tab.close`. Each tab has `handle`, `url`,
/// `title`, `loading`, `can_go_back` and `can_go_forward`; URLs and titles
/// are cut to their workspace limits.
pub fn tab_response(req: &Request, result: TabResult) -> Response {
    let data = |tabs: Vec<TabState>| {
        if req.command == "tabs.list" {
            return json!({ "tabs": tabs.iter().map(tab_json).collect::<Vec<_>>() });
        }
        let mut data = json!({ "tab": tabs.first().map(tab_json) });
        if req.command == "tab.close" {
            data["closed"] = Value::Bool(true);
        }
        data
    };
    let outcome = match result {
        TabResult::Done(tabs) => Outcome::Success { data: data(tabs) },
        TabResult::Unknown { reason, tabs } => Outcome::OutcomeUnknown {
            reason,
            data: Some(data(tabs)),
        },
        TabResult::Failed(outcome) => outcome,
    };
    answer(req, outcome)
}

fn tab_json(tab: &TabState) -> Value {
    json!({
        "handle": tab_handle(tab.item),
        "url": truncate(&tab.url, MAX_URL_LENGTH),
        "title": truncate(&tab.title, MAX_TITLE_BYTES),
        "loading": tab.loading,
        "can_go_back": tab.can_go_back,
        "can_go_forward": tab.can_go_forward,
    })
}

fn answer(req: &Request, outcome: Outcome) -> Response {
    Response {
        id: if req.id.len() > MAX_REQUEST_ID_BYTES {
            String::new()
        } else {
            req.id.clone()
        },
        command: bounded(&req.command).to_owned(),
        outcome,
    }
}

/// Answers `req` with `outcome`, for a host that finished a request itself.
pub fn answer_outcome(req: &Request, outcome: Outcome) -> Response {
    answer(req, outcome)
}

/// Answers `req` with `denied` without running it, for a host that can't run
/// commands right now, such as a browser whose workspace is still loading.
pub fn deny(req: &Request, reason: &str) -> Response {
    answer(
        req,
        Outcome::Denied {
            reason: reason.to_owned(),
        },
    )
}

struct Context<'a> {
    workspace: &'a mut Workspace,
    window_id: WindowId,
}

impl Context<'_> {
    fn run(&mut self, command: &str, params: &Value) -> Result<Value, String> {
        match command {
            "space.list" => {
                Params::parse(params, &[])?;
                Ok(self.space_list())
            }
            "space.create" => {
                let params = Params::parse(params, &["name"])?;
                let name = params.string("name")?;
                let id = self
                    .workspace
                    .create_space(DEFAULT_PROFILE_ID, name)
                    .map_err(workspace_error)?;
                Ok(json!({ "space": self.space_json(id) }))
            }
            "space.select" => {
                let params = Params::parse(params, &["space_id"])?;
                let id = params.id("space_id")?;
                self.workspace
                    .select_space(self.window_id, id)
                    .map_err(workspace_error)?;
                Ok(json!({ "space": self.space_json(id) }))
            }
            "space.rename" => {
                let params = Params::parse(params, &["space_id", "name"])?;
                let id = params.id("space_id")?;
                let name = params.string("name")?;
                self.workspace
                    .rename_space(id, name)
                    .map_err(|error| match error {
                        RenameError::UnknownSpace => unknown_space(id),
                        other => other.to_string(),
                    })?;
                Ok(json!({ "space": self.space_json(id) }))
            }
            "space.ensure_agent" => {
                Params::parse(params, &[])?;
                let id = self
                    .workspace
                    .ensure_agent_space(DEFAULT_PROFILE_ID)
                    .map_err(workspace_error)?;
                Ok(json!({ "space": self.space_json(id) }))
            }
            other => Err(format!(
                "{other} is registered as implemented but has no handler"
            )),
        }
    }

    fn selected_space(&self) -> Option<SpaceId> {
        self.workspace
            .sidebar_snapshot(self.window_id)
            .ok()
            .map(|sidebar| sidebar.current_space.id)
    }

    fn space_list(&self) -> Value {
        let selected = self.selected_space();
        let spaces: Vec<Value> = self
            .workspace
            .snapshot()
            .spaces
            .iter()
            .map(|space| {
                json!({
                    "id": space.id,
                    "name": space.name,
                    "selected": Some(space.id) == selected,
                    "agent": space.agent,
                })
            })
            .collect();
        json!({ "spaces": spaces, "selected_space_id": selected })
    }

    fn space_json(&self, id: SpaceId) -> Value {
        json!({
            "id": id,
            "name": self.workspace.space_name_of(id).unwrap_or_default(),
            "selected": self.selected_space() == Some(id),
            "agent": self.workspace.agent_space_of(DEFAULT_PROFILE_ID) == Some(id),
        })
    }
}

/// Parameters of one request: `null` or an object with only allowed keys.
struct Params<'a>(Option<&'a Map<String, Value>>);

impl<'a> Params<'a> {
    fn parse(value: &'a Value, allowed: &[&str]) -> Result<Self, String> {
        match value {
            Value::Null => Ok(Self(None)),
            Value::Object(map) => {
                if let Some(key) = map.keys().find(|key| !allowed.contains(&key.as_str())) {
                    let expected = if allowed.is_empty() {
                        String::from("this command takes no parameters")
                    } else {
                        format!("expected {}", allowed.join(", "))
                    };
                    return Err(format!("Unknown parameter {:?}; {expected}", bounded(key)));
                }
                Ok(Self(Some(map)))
            }
            _ => Err(String::from("Params must be a JSON object or null")),
        }
    }

    fn get(&self, key: &str) -> Result<&'a Value, String> {
        self.0
            .and_then(|map| map.get(key))
            .ok_or_else(|| format!("Missing required parameter {key}"))
    }

    fn string(&self, key: &str) -> Result<&'a str, String> {
        self.text(key, MAX_STRING_PARAM_BYTES)
    }

    fn text(&self, key: &str, max: usize) -> Result<&'a str, String> {
        let value = self
            .get(key)?
            .as_str()
            .ok_or_else(|| format!("Parameter {key} must be a string"))?;
        if value.len() > max {
            return Err(format!("Parameter {key} exceeds {max} bytes"));
        }
        Ok(value)
    }

    /// The `url` parameter, checked by [`check_url`].
    fn url(&self) -> Result<String, Outcome> {
        let url = self.text("url", MAX_URL_LENGTH).map_err(invalid)?;
        check_url(url)?;
        Ok(url.to_owned())
    }

    fn id(&self, key: &str) -> Result<u64, String> {
        self.get(key)?
            .as_u64()
            .filter(|&id| id >= 1)
            .ok_or_else(|| format!("Parameter {key} must be a positive integer ID"))
    }
}

fn invalid(reason: String) -> Outcome {
    Outcome::Invalid { reason }
}

fn unknown_space(id: SpaceId) -> String {
    format!("Space {id} does not exist; run space.list to see Space IDs")
}

fn workspace_error(error: WorkspaceError) -> String {
    match error {
        WorkspaceError::UnknownSpace(id) => unknown_space(id),
        other => other.to_string(),
    }
}

/// Prefix of untrusted text short enough to quote in an error.
fn bounded(text: &str) -> &str {
    truncate(text, MAX_QUOTED_BYTES)
}

/// Longest prefix of `text` of at most `max` bytes that ends on a character.
fn truncate(text: &str, max: usize) -> &str {
    let mut end = text.len().min(max);
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    &text[..end]
}

fn schema(properties: Value, required: &[&str]) -> Value {
    json!({
        "type": "object",
        "properties": properties,
        "required": required,
        "additionalProperties": false,
    })
}

fn no_params() -> Value {
    schema(json!({}), &[])
}

fn text(description: &str, max_length: usize) -> Value {
    json!({ "type": "string", "maxLength": max_length, "description": description })
}

fn id(description: &str) -> Value {
    json!({ "type": "integer", "minimum": 1, "description": description })
}

fn tab_param() -> Value {
    text(
        "Tab handle from tabs.list or tab.open, such as t12",
        MAX_STRING_PARAM_BYTES,
    )
}

fn revision() -> Value {
    text(
        "Revision from the page.snapshot the reference came from",
        MAX_QUOTED_BYTES,
    )
}

fn element_ref() -> Value {
    text(
        "Element reference from page.snapshot at that revision",
        MAX_STRING_PARAM_BYTES,
    )
}

fn tab_only() -> Value {
    schema(json!({ "tab": tab_param() }), &["tab"])
}

fn spec(
    id: &'static str,
    area: Area,
    summary: &'static str,
    params: Value,
    implemented: bool,
) -> CommandSpec {
    CommandSpec {
        id,
        area,
        summary,
        params,
        implemented,
    }
}

fn build_registry() -> Vec<CommandSpec> {
    use Area::*;
    vec![
        spec(
            "session.status",
            Session,
            "Show the authorized context and capabilities of this session",
            no_params(),
            false,
        ),
        spec(
            "session.request",
            Session,
            "Ask the user for a control grant",
            schema(
                json!({
                    "capabilities": {
                        "type": "array",
                        "items": text("Capability name", MAX_QUOTED_BYTES),
                        "maxItems": 32,
                        "description": "Observation and action capabilities to request",
                    },
                }),
                &[],
            ),
            false,
        ),
        spec(
            "session.release",
            Session,
            "Release control and end the grant",
            no_params(),
            false,
        ),
        spec(
            "tabs.list",
            Tabs,
            "List the open tabs in the agent Space with handles, URL, title and navigation state",
            no_params(),
            true,
        ),
        spec(
            "tab.open",
            Tabs,
            "Open a tab at a URL in the agent Space, creating that Space if needed, without switching the user's Space",
            schema(
                json!({ "url": text("HTTP(S) URL to open", MAX_URL_LENGTH) }),
                &["url"],
            ),
            true,
        ),
        spec(
            "tab.activate",
            Tabs,
            "Make an agent Space tab the one it shows; the user's window switches only when it already shows the agent Space",
            tab_only(),
            true,
        ),
        spec(
            "tab.close",
            Tabs,
            "Close an agent Space tab",
            tab_only(),
            true,
        ),
        spec(
            "tab.navigate",
            Tabs,
            "Load a URL in an agent Space tab and wait for it to commit",
            schema(
                json!({
                    "tab": tab_param(),
                    "url": text("HTTP(S) URL to load", MAX_URL_LENGTH),
                }),
                &["tab", "url"],
            ),
            true,
        ),
        spec(
            "tab.back",
            Tabs,
            "Go back in an agent Space tab's history and wait for it to commit",
            tab_only(),
            true,
        ),
        spec(
            "tab.forward",
            Tabs,
            "Go forward in an agent Space tab's history and wait for it to commit",
            tab_only(),
            true,
        ),
        spec(
            "tab.reload",
            Tabs,
            "Reload an agent Space tab and wait for it to commit",
            tab_only(),
            true,
        ),
        spec(
            "page.snapshot",
            Observation,
            "Compact outline of an agent Space tab's accessibility tree, one node per line, with [ref=eN] on interactive nodes, plus URL, title, revision and loading state",
            tab_only(),
            true,
        ),
        spec(
            "page.screenshot",
            Observation,
            "Screenshot of a granted tab",
            tab_only(),
            false,
        ),
        spec(
            "page.click",
            Interaction,
            "Click an element of the latest page.snapshot after scrolling it into view, wait for the page to settle and answer with a new snapshot",
            schema(
                json!({ "tab": tab_param(), "revision": revision(), "ref": element_ref() }),
                &["tab", "revision", "ref"],
            ),
            true,
        ),
        spec(
            "page.fill",
            Interaction,
            "Replace the value of a text field of the latest page.snapshot and answer with a new snapshot",
            schema(
                json!({
                    "tab": tab_param(),
                    "revision": revision(),
                    "ref": element_ref(),
                    "text": text("Text to type, replacing the field's value", actions::MAX_FILL_BYTES),
                }),
                &["tab", "revision", "ref", "text"],
            ),
            true,
        ),
        spec(
            "page.select",
            Interaction,
            "Choose an option of a select element of the latest page.snapshot and answer with a new snapshot",
            schema(
                json!({
                    "tab": tab_param(),
                    "revision": revision(),
                    "ref": element_ref(),
                    "value": text("The option's value or visible text", MAX_STRING_PARAM_BYTES),
                }),
                &["tab", "revision", "ref", "value"],
            ),
            true,
        ),
        spec(
            "page.press",
            Interaction,
            "Press a key in the focused element, or in a text field of the latest page.snapshot after focusing it, and answer with a new snapshot",
            schema(
                json!({
                    "tab": tab_param(),
                    "revision": revision(),
                    "ref": element_ref(),
                    "key": text("Enter, Tab, Escape, Backspace, ArrowUp, ArrowDown, ArrowLeft, ArrowRight, PageUp, PageDown, Home, End, Space or one printable character", MAX_QUOTED_BYTES),
                }),
                &["tab", "revision", "key"],
            ),
            true,
        ),
        spec(
            "page.scroll",
            Interaction,
            "Scroll the page or an element from the current snapshot",
            schema(
                json!({
                    "tab": tab_param(),
                    "revision": revision(),
                    "ref": element_ref(),
                    "direction": { "type": "string", "enum": ["up", "down", "left", "right"] },
                }),
                &["tab", "revision", "direction"],
            ),
            false,
        ),
        spec(
            "page.wait",
            Interaction,
            "Wait, within a bound, for a tab to finish loading",
            schema(
                json!({
                    "tab": tab_param(),
                    "timeout_ms": {
                        "type": "integer",
                        "minimum": 0,
                        "maximum": 30000,
                        "description": "Longest wait in milliseconds",
                    },
                }),
                &["tab", "timeout_ms"],
            ),
            false,
        ),
        spec(
            "space.list",
            Workspace,
            "List Spaces and the selected one",
            no_params(),
            true,
        ),
        spec(
            "space.create",
            Workspace,
            "Create a Space",
            schema(
                json!({ "name": text("Space name, at most 128 UTF-8 bytes", myarc_workspace::MAX_SPACE_NAME_BYTES) }),
                &["name"],
            ),
            true,
        ),
        spec(
            "space.select",
            Workspace,
            "Show a Space in the window",
            schema(
                json!({ "space_id": id("Space ID from space.list") }),
                &["space_id"],
            ),
            true,
        ),
        spec(
            "space.rename",
            Workspace,
            "Rename a Space",
            schema(
                json!({
                    "space_id": id("Space ID from space.list"),
                    "name": text("New name, at most 128 UTF-8 bytes", myarc_workspace::MAX_SPACE_NAME_BYTES),
                }),
                &["space_id", "name"],
            ),
            true,
        ),
        spec(
            "space.ensure_agent",
            Workspace,
            "Create the Profile's agent Space if it has none, without changing the selected Space",
            no_params(),
            true,
        ),
        spec(
            "node.move",
            Workspace,
            "Move a sidebar item before another one in its section",
            schema(
                json!({
                    "node_id": id("Item to move"),
                    "before_id": id("Item to place it before; omit to move it to the end"),
                }),
                &["node_id"],
            ),
            false,
        ),
        spec(
            "tab.pin",
            Workspace,
            "Pin a tab in its Space",
            tab_only(),
            false,
        ),
        spec(
            "tab.archive",
            Workspace,
            "Close a tab into Archived Tabs",
            tab_only(),
            false,
        ),
        spec(
            "tab.restore",
            Workspace,
            "Reopen a tab from Archived Tabs",
            schema(
                json!({ "archived_id": id("Archived tab ID") }),
                &["archived_id"],
            ),
            false,
        ),
        spec(
            "import.preview",
            Preservation,
            "Preview what an import would bring in, without changing anything",
            schema(
                json!({ "source": text("Granted file to import from", MAX_PATH_LENGTH) }),
                &["source"],
            ),
            false,
        ),
        spec(
            "import.commit",
            Preservation,
            "Apply a previewed import as one transaction",
            schema(
                json!({ "preview_id": text("ID from import.preview", MAX_STRING_PARAM_BYTES) }),
                &["preview_id"],
            ),
            false,
        ),
        spec(
            "import.rollback",
            Preservation,
            "Undo a committed import",
            schema(
                json!({ "import_id": text("ID from import.commit", MAX_STRING_PARAM_BYTES) }),
                &["import_id"],
            ),
            false,
        ),
        spec(
            "workspace.export",
            Preservation,
            "Export the workspace to a granted file",
            schema(
                json!({ "destination": text("Granted file to write", MAX_PATH_LENGTH) }),
                &["destination"],
            ),
            false,
        ),
    ]
}
