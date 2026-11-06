use std::collections::BTreeSet;

use myarc_control::{
    Area, ControlService, MAX_STRING_PARAM_BYTES, Outcome, PROTOCOL_VERSION, Request, Response,
    dispatch, registry,
};
use myarc_workspace::{AGENT_SPACE_NAME, DEFAULT_PROFILE_ID, DEFAULT_WINDOW_ID, Workspace};
use serde_json::{Value, json};

fn request(command: &str, params: Value) -> Request {
    Request {
        id: String::from("r1"),
        command: command.to_owned(),
        params,
    }
}

fn data(service: &mut ControlService, command: &str, params: Value) -> Value {
    match service.handle(request(command, params)).outcome {
        Outcome::Success { data } => data,
        other => panic!("{command} failed: {other:?}"),
    }
}

fn invalid_reason(service: &mut ControlService, command: &str, params: Value) -> String {
    match service.handle(request(command, params)).outcome {
        Outcome::Invalid { reason } => reason,
        other => panic!("{command} should be invalid: {other:?}"),
    }
}

/// Command IDs in the spec's "Required control surface" table.
fn spec_command_ids() -> BTreeSet<String> {
    let path = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../docs/CONTROL-PROTOCOL.md"
    );
    let doc = std::fs::read_to_string(path).expect("read CONTROL-PROTOCOL.md");
    let section = doc
        .split("## Required control surface")
        .nth(1)
        .and_then(|rest| rest.split("\n## ").next())
        .expect("Required control surface section");
    let mut ids = BTreeSet::new();
    for line in section.lines().filter(|line| line.starts_with('|')) {
        for (index, part) in line.split('`').enumerate() {
            if index % 2 == 1 && part.contains('.') {
                ids.insert(part.to_owned());
            }
        }
    }
    ids
}

#[test]
fn protocol_version_is_set() {
    assert_eq!(PROTOCOL_VERSION, "0.1.0");
}

#[test]
fn registry_ids_are_unique_and_match_the_spec_table() {
    let ids: Vec<&str> = registry().iter().map(|spec| spec.id).collect();
    let unique: BTreeSet<String> = ids.iter().map(|id| (*id).to_owned()).collect();
    assert_eq!(unique.len(), ids.len(), "duplicate IDs in {ids:?}");
    let spec = spec_command_ids();
    assert_eq!(spec.len(), 32);
    assert_eq!(unique, spec);
}

#[test]
fn registry_schemas_are_objects_and_only_space_commands_are_implemented() {
    for spec in registry() {
        assert_eq!(spec.params["type"], "object", "{}", spec.id);
        assert_eq!(spec.params["additionalProperties"], false, "{}", spec.id);
        assert!(spec.params["required"].is_array(), "{}", spec.id);
        assert!(!spec.summary.is_empty(), "{}", spec.id);
    }
    let implemented: Vec<&str> = registry()
        .iter()
        .filter(|spec| spec.implemented)
        .map(|spec| spec.id)
        .collect();
    assert_eq!(
        implemented,
        [
            "space.list",
            "space.create",
            "space.select",
            "space.rename",
            "space.ensure_agent"
        ]
    );
    let area = |id: &str| {
        registry()
            .iter()
            .find(|spec| spec.id == id)
            .map(|spec| spec.area)
    };
    assert_eq!(area("session.status"), Some(Area::Session));
    assert_eq!(area("tabs.list"), Some(Area::Tabs));
    assert_eq!(area("page.snapshot"), Some(Area::Observation));
    assert_eq!(area("page.click"), Some(Area::Interaction));
    assert_eq!(area("tab.pin"), Some(Area::Workspace));
    assert_eq!(area("workspace.export"), Some(Area::Preservation));
}

#[test]
fn space_list_starts_with_one_selected_space() {
    let mut service = ControlService::new();
    let list = data(&mut service, "space.list", Value::Null);
    assert_eq!(
        list,
        json!({
            "spaces": [{ "id": 1, "name": "Everyday", "selected": true, "agent": false }],
            "selected_space_id": 1,
        })
    );
    assert_eq!(data(&mut service, "space.list", json!({})), list);
}

#[test]
fn space_create_adds_a_space() {
    let mut service = ControlService::new();
    let created = data(&mut service, "space.create", json!({ "name": "Work" }));
    assert_eq!(
        created,
        json!({ "space": { "id": 2, "name": "Work", "selected": false, "agent": false } })
    );
    let list = data(&mut service, "space.list", Value::Null);
    assert_eq!(list["spaces"][1]["name"], "Work");
    assert_eq!(list["selected_space_id"], 1);
}

#[test]
fn space_select_changes_the_selected_space() {
    let mut service = ControlService::new();
    data(&mut service, "space.create", json!({ "name": "Work" }));
    let selected = data(&mut service, "space.select", json!({ "space_id": 2 }));
    assert_eq!(selected["space"]["selected"], true);
    let list = data(&mut service, "space.list", Value::Null);
    assert_eq!(list["selected_space_id"], 2);
    assert_eq!(list["spaces"][0]["selected"], false);
}

#[test]
fn space_rename_changes_the_name() {
    let mut service = ControlService::new();
    let renamed = data(
        &mut service,
        "space.rename",
        json!({ "space_id": 1, "name": "Home" }),
    );
    assert_eq!(
        renamed,
        json!({ "space": { "id": 1, "name": "Home", "selected": true, "agent": false } })
    );
    assert_eq!(
        data(&mut service, "space.list", Value::Null)["spaces"][0]["name"],
        "Home"
    );
}

#[test]
fn workspace_errors_are_invalid_with_actionable_reasons() {
    let mut service = ControlService::new();
    let reason = invalid_reason(&mut service, "space.select", json!({ "space_id": 9 }));
    assert_eq!(
        reason,
        "Space 9 does not exist; run space.list to see Space IDs"
    );
    let reason = invalid_reason(
        &mut service,
        "space.rename",
        json!({ "space_id": 9, "name": "X" }),
    );
    assert!(reason.contains("run space.list"), "{reason}");
    assert_eq!(
        invalid_reason(&mut service, "space.create", json!({ "name": "" })),
        "Enter a Space name"
    );
    assert_eq!(
        invalid_reason(
            &mut service,
            "space.create",
            json!({ "name": "a".repeat(200) })
        ),
        "Space name exceeds 128 UTF-8 bytes"
    );
    // Nothing above changed the workspace.
    assert_eq!(
        data(&mut service, "space.list", Value::Null)["spaces"]
            .as_array()
            .map(Vec::len),
        Some(1)
    );
}

#[test]
fn bad_params_are_invalid() {
    let mut service = ControlService::new();
    let cases = [
        ("space.create", json!({}), "Missing required parameter name"),
        (
            "space.create",
            json!({ "name": 5 }),
            "Parameter name must be a string",
        ),
        (
            "space.create",
            json!("Work"),
            "Params must be a JSON object or null",
        ),
        (
            "space.create",
            json!([]),
            "Params must be a JSON object or null",
        ),
        (
            "space.create",
            json!({ "name": "Work", "color": "red" }),
            "Unknown parameter \"color\"; expected name",
        ),
        (
            "space.list",
            json!({ "x": 1 }),
            "Unknown parameter \"x\"; this command takes no parameters",
        ),
        (
            "space.select",
            json!({ "space_id": "1" }),
            "Parameter space_id must be a positive integer ID",
        ),
        (
            "space.select",
            json!({ "space_id": -1 }),
            "Parameter space_id must be a positive integer ID",
        ),
        (
            "space.select",
            json!({ "space_id": 0 }),
            "Parameter space_id must be a positive integer ID",
        ),
        (
            "space.select",
            json!({ "space_id": 1.5 }),
            "Parameter space_id must be a positive integer ID",
        ),
        (
            "space.rename",
            json!({ "space_id": 1 }),
            "Missing required parameter name",
        ),
    ];
    for (command, params, expected) in cases {
        assert_eq!(
            invalid_reason(&mut service, command, params.clone()),
            expected,
            "{command} {params}"
        );
    }
    let long = "x".repeat(MAX_STRING_PARAM_BYTES + 1);
    assert_eq!(
        invalid_reason(&mut service, "space.create", json!({ "name": long })),
        format!("Parameter name exceeds {MAX_STRING_PARAM_BYTES} bytes")
    );
    let long_key = "k".repeat(10_000);
    let reason = invalid_reason(&mut service, "space.list", json!({ long_key: 1 }));
    assert!(reason.len() < 200, "{reason}");
}

#[test]
fn unimplemented_commands_are_unsupported() {
    let mut service = ControlService::new();
    for spec in registry().iter().filter(|spec| !spec.implemented) {
        let response = service.handle(request(spec.id, json!({ "tab_id": "t1" })));
        assert_eq!(response.id, "r1");
        assert_eq!(response.command, spec.id);
        match response.outcome {
            Outcome::Unsupported { command, reason } => {
                assert_eq!(command, spec.id);
                assert!(reason.contains("MA-011"), "{reason}");
            }
            other => panic!("{} should be unsupported: {other:?}", spec.id),
        }
    }
}

#[test]
fn unknown_commands_are_invalid() {
    let mut service = ControlService::new();
    let reason = invalid_reason(&mut service, "tab.teleport", Value::Null);
    assert!(
        reason.starts_with("Unknown command \"tab.teleport\""),
        "{reason}"
    );
    let response = service.handle(request(&"z".repeat(10_000), Value::Null));
    assert_eq!(response.command, "");
    assert!(matches!(response.outcome, Outcome::Invalid { .. }));
    let response = service.handle(Request {
        id: "i".repeat(10_000),
        command: String::from("space.list"),
        params: Value::Null,
    });
    assert_eq!(response.id, "");
    assert!(matches!(response.outcome, Outcome::Invalid { .. }));
}

#[test]
fn response_json_has_a_status_tag_and_flattened_fields() {
    let mut service = ControlService::new();
    let success = service.handle(request("space.create", json!({ "name": "Work" })));
    assert_eq!(
        serde_json::to_value(&success).expect("serialize"),
        json!({
            "id": "r1",
            "command": "space.create",
            "status": "success",
            "data": { "space": { "id": 2, "name": "Work", "selected": false, "agent": false } },
        })
    );
    let unsupported = service.handle(request("tabs.list", Value::Null));
    let value = serde_json::to_value(&unsupported).expect("serialize");
    assert_eq!(value["status"], "unsupported");
    assert_eq!(value["command"], "tabs.list");
    assert!(value["reason"].is_string());

    let shapes = [
        (
            Outcome::Cancelled,
            json!({ "id": "a", "command": "c", "status": "cancelled" }),
        ),
        (
            Outcome::Timeout,
            json!({ "id": "a", "command": "c", "status": "timeout" }),
        ),
        (
            Outcome::OutcomeUnknown {
                reason: String::from("r"),
            },
            json!({ "id": "a", "command": "c", "status": "outcome_unknown", "reason": "r" }),
        ),
        (
            Outcome::Invalid {
                reason: String::from("r"),
            },
            json!({ "id": "a", "command": "c", "status": "invalid", "reason": "r" }),
        ),
    ];
    for (outcome, expected) in shapes {
        let response = Response {
            id: String::from("a"),
            command: String::from("c"),
            outcome,
        };
        let value = serde_json::to_value(&response).expect("serialize");
        assert_eq!(value, expected);
        let back: Response = serde_json::from_value(value).expect("deserialize");
        assert_eq!(back, response);
    }
}

#[test]
fn requests_parse_from_json_without_params() {
    let request: Request =
        serde_json::from_str(r#"{"id":"7","command":"space.list"}"#).expect("parse");
    assert_eq!(request.params, Value::Null);
    let response = ControlService::new().handle(request);
    assert!(matches!(response.outcome, Outcome::Success { .. }));
}

#[test]
fn space_ensure_agent_creates_the_agent_space_once_without_selecting_it() {
    let mut service = ControlService::new();
    let created = data(&mut service, "space.ensure_agent", Value::Null);
    assert_eq!(
        created,
        json!({ "space": { "id": 2, "name": AGENT_SPACE_NAME, "selected": false, "agent": true } })
    );
    assert_eq!(data(&mut service, "space.ensure_agent", json!({})), created);
    let list = data(&mut service, "space.list", Value::Null);
    assert_eq!(list["selected_space_id"], 1);
    assert_eq!(list["spaces"].as_array().map(Vec::len), Some(2));
    assert_eq!(list["spaces"][0]["agent"], false);
    assert_eq!(list["spaces"][1]["agent"], true);
    let reason = invalid_reason(&mut service, "space.ensure_agent", json!({ "name": "x" }));
    assert!(reason.contains("takes no parameters"), "{reason}");
}

#[test]
fn renaming_the_agent_space_keeps_it_the_agent_space() {
    let mut service = ControlService::new();
    data(&mut service, "space.ensure_agent", Value::Null);
    let renamed = data(
        &mut service,
        "space.rename",
        json!({ "space_id": 2, "name": "Bots" }),
    );
    assert_eq!(renamed["space"]["agent"], true);
    assert_eq!(
        data(&mut service, "space.ensure_agent", Value::Null)["space"]["name"],
        "Bots"
    );
}

#[test]
fn dispatch_changes_the_callers_workspace() {
    let mut workspace = Workspace::default();
    let created = dispatch(
        &mut workspace,
        DEFAULT_WINDOW_ID,
        request("space.create", json!({ "name": "CLI" })),
    );
    assert!(
        matches!(created.outcome, Outcome::Success { .. }),
        "{created:?}"
    );
    let agent = dispatch(
        &mut workspace,
        DEFAULT_WINDOW_ID,
        request("space.ensure_agent", Value::Null),
    );
    assert!(
        matches!(agent.outcome, Outcome::Success { .. }),
        "{agent:?}"
    );
    let names: Vec<String> = workspace
        .snapshot()
        .spaces
        .into_iter()
        .map(|space| space.name)
        .collect();
    assert_eq!(names, ["Everyday", "CLI", AGENT_SPACE_NAME]);
    assert_eq!(workspace.agent_space_of(DEFAULT_PROFILE_ID), Some(3));
}

#[test]
fn dispatch_selects_in_the_given_window() {
    let mut workspace = Workspace::default();
    let work = workspace
        .create_space(DEFAULT_PROFILE_ID, "Work")
        .expect("create Work");
    let second = workspace.create_window(1).expect("second window");
    let response = dispatch(
        &mut workspace,
        second,
        request("space.select", json!({ "space_id": work })),
    );
    assert!(
        matches!(response.outcome, Outcome::Success { .. }),
        "{response:?}"
    );
    let in_second = dispatch(&mut workspace, second, request("space.list", Value::Null));
    let in_first = dispatch(
        &mut workspace,
        DEFAULT_WINDOW_ID,
        request("space.list", Value::Null),
    );
    let selected = |response: Response| match response.outcome {
        Outcome::Success { data } => data["selected_space_id"].clone(),
        other => panic!("space.list failed: {other:?}"),
    };
    assert_eq!(selected(in_second), json!(work));
    assert_eq!(selected(in_first), json!(1));
}
