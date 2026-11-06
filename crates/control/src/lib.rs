#![forbid(unsafe_code)]

//! Shared control layer for MA-011 browser control. The `myarcctl` CLI and its
//! MCP server translate requests into `Request` values and print `Response`
//! values; both use the one command registry below.
//!
//! This is a core stub. It owns an in-memory workspace and runs the Space
//! commands against it. Commands that need a live browser are listed in the
//! registry with `implemented: false` and answer `Unsupported`. Requests are
//! untrusted: strings are bounded, unknown parameters are rejected and no input
//! panics.
//!
//! On Unix, `ipc` carries requests from a client process to the service hosted
//! by a running browser over an authenticated same-user socket.

use std::sync::OnceLock;

#[cfg(unix)]
pub mod ipc;

use myarc_workspace::{
    DEFAULT_PROFILE_ID, DEFAULT_WINDOW_ID, RenameError, SpaceId, WindowId, Workspace,
    WorkspaceError,
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
const MAX_URL_LENGTH: usize = myarc_workspace::MAX_URL_BYTES;
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
        command: String,
        reason: String,
    },
    Stale {
        reason: String,
    },
    Cancelled,
    Timeout,
    OutcomeUnknown {
        reason: String,
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

/// Runs registry commands against an in-memory workspace for one window.
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
        let Request {
            id,
            command,
            params,
        } = req;
        if id.len() > MAX_REQUEST_ID_BYTES {
            return Response {
                id: String::new(),
                command: bounded(&command).to_owned(),
                outcome: invalid(format!(
                    "Request ID exceeds {MAX_REQUEST_ID_BYTES} bytes; use a shorter ID"
                )),
            };
        }
        if command.len() > MAX_COMMAND_BYTES {
            return Response {
                id,
                command: String::new(),
                outcome: invalid(format!(
                    "Command exceeds {MAX_COMMAND_BYTES} bytes; use an ID from the registry"
                )),
            };
        }
        let outcome = match registry().iter().find(|spec| spec.id == command) {
            None => invalid(format!(
                "Unknown command {:?}; use an ID from the registry, such as space.list",
                command
            )),
            Some(spec) if !spec.implemented => Outcome::Unsupported {
                command: command.clone(),
                reason: format!(
                    "{command} needs live browser attachment (MA-011), which this core stub does not have"
                ),
            },
            Some(_) => match self.run(&command, &params) {
                Ok(data) => Outcome::Success { data },
                Err(reason) => invalid(reason),
            },
        };
        Response {
            id,
            command,
            outcome,
        }
    }

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
        let value = self
            .get(key)?
            .as_str()
            .ok_or_else(|| format!("Parameter {key} must be a string"))?;
        if value.len() > MAX_STRING_PARAM_BYTES {
            return Err(format!(
                "Parameter {key} exceeds {MAX_STRING_PARAM_BYTES} bytes"
            ));
        }
        Ok(value)
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
    let mut end = text.len().min(MAX_QUOTED_BYTES);
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

fn tab_id() -> Value {
    text("Stable tab handle from tabs.list", MAX_STRING_PARAM_BYTES)
}

fn revision() -> Value {
    json!({
        "type": "integer",
        "minimum": 0,
        "description": "Document revision from the page.snapshot the reference came from",
    })
}

fn element_ref() -> Value {
    text(
        "Element reference from page.snapshot at that revision",
        MAX_STRING_PARAM_BYTES,
    )
}

fn tab_only() -> Value {
    schema(json!({ "tab_id": tab_id() }), &["tab_id"])
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
            "List tabs in the grant with stable handles, URL and title",
            no_params(),
            false,
        ),
        spec(
            "tab.open",
            Tabs,
            "Open a tab at a URL",
            schema(
                json!({
                    "url": text("HTTP(S) URL to open", MAX_URL_LENGTH),
                    "space_id": id("Space to open the tab in; defaults to the selected Space"),
                }),
                &["url"],
            ),
            false,
        ),
        spec(
            "tab.activate",
            Tabs,
            "Bring a tab to the front",
            tab_only(),
            false,
        ),
        spec("tab.close", Tabs, "Close a tab", tab_only(), false),
        spec(
            "tab.navigate",
            Tabs,
            "Load a URL in a tab",
            schema(
                json!({
                    "tab_id": tab_id(),
                    "url": text("HTTP(S) URL to load", MAX_URL_LENGTH),
                }),
                &["tab_id", "url"],
            ),
            false,
        ),
        spec(
            "tab.back",
            Tabs,
            "Go back in a tab's history",
            tab_only(),
            false,
        ),
        spec(
            "tab.forward",
            Tabs,
            "Go forward in a tab's history",
            tab_only(),
            false,
        ),
        spec("tab.reload", Tabs, "Reload a tab", tab_only(), false),
        spec(
            "page.snapshot",
            Observation,
            "Bounded semantic view of a page with element references, URL, revision and loading state",
            tab_only(),
            false,
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
            "Click an element from the current snapshot",
            schema(
                json!({ "tab_id": tab_id(), "revision": revision(), "ref": element_ref() }),
                &["tab_id", "revision", "ref"],
            ),
            false,
        ),
        spec(
            "page.fill",
            Interaction,
            "Fill a text field from the current snapshot",
            schema(
                json!({
                    "tab_id": tab_id(),
                    "revision": revision(),
                    "ref": element_ref(),
                    "value": text("Text to enter", MAX_STRING_PARAM_BYTES),
                }),
                &["tab_id", "revision", "ref", "value"],
            ),
            false,
        ),
        spec(
            "page.select",
            Interaction,
            "Choose an option in a select element from the current snapshot",
            schema(
                json!({
                    "tab_id": tab_id(),
                    "revision": revision(),
                    "ref": element_ref(),
                    "value": text("Option value to choose", MAX_STRING_PARAM_BYTES),
                }),
                &["tab_id", "revision", "ref", "value"],
            ),
            false,
        ),
        spec(
            "page.press",
            Interaction,
            "Press a key, optionally on an element from the current snapshot",
            schema(
                json!({
                    "tab_id": tab_id(),
                    "revision": revision(),
                    "ref": element_ref(),
                    "key": text("Key name, such as Enter or Tab", MAX_QUOTED_BYTES),
                }),
                &["tab_id", "revision", "key"],
            ),
            false,
        ),
        spec(
            "page.scroll",
            Interaction,
            "Scroll the page or an element from the current snapshot",
            schema(
                json!({
                    "tab_id": tab_id(),
                    "revision": revision(),
                    "ref": element_ref(),
                    "direction": { "type": "string", "enum": ["up", "down", "left", "right"] },
                }),
                &["tab_id", "revision", "direction"],
            ),
            false,
        ),
        spec(
            "page.wait",
            Interaction,
            "Wait, within a bound, for a tab to finish loading",
            schema(
                json!({
                    "tab_id": tab_id(),
                    "timeout_ms": {
                        "type": "integer",
                        "minimum": 0,
                        "maximum": 30000,
                        "description": "Longest wait in milliseconds",
                    },
                }),
                &["tab_id", "timeout_ms"],
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
