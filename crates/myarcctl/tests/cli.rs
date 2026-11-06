use std::process::{Command, Output};

use myarc_control::{PROTOCOL_VERSION, registry};
use serde_json::Value;

fn myarcctl(args: &[&str]) -> Output {
    Command::new(env!("CARGO_BIN_EXE_myarcctl"))
        .args(args)
        .output()
        .expect("myarcctl should start")
}

fn stdout(output: &Output) -> String {
    String::from_utf8(output.stdout.clone()).expect("stdout should be UTF-8")
}

fn stderr(output: &Output) -> String {
    String::from_utf8(output.stderr.clone()).expect("stderr should be UTF-8")
}

fn json_stdout(output: &Output) -> Value {
    serde_json::from_str(stdout(output).trim()).expect("stdout should be one JSON value")
}

#[test]
fn commands_lists_every_registry_entry() {
    let output = myarcctl(&["commands"]);
    assert_eq!(output.status.code(), Some(0));
    let text = stdout(&output);
    assert_eq!(text.lines().count(), registry().len() + 1);
    for spec in registry() {
        let line = text
            .lines()
            .find(|line| line.split_whitespace().next() == Some(spec.id))
            .unwrap_or_else(|| panic!("{} missing from commands", spec.id));
        assert!(line.contains(spec.summary), "{line}");
    }
    let create = text
        .lines()
        .find(|line| line.starts_with("space.create "))
        .expect("space.create listed");
    assert!(
        create.contains("workspace") && create.contains("yes"),
        "{create}"
    );
}

#[test]
fn commands_json_has_id_area_implemented_and_summary() {
    let output = myarcctl(&["commands", "--json"]);
    assert_eq!(output.status.code(), Some(0));
    let list = json_stdout(&output);
    let list = list.as_array().expect("an array");
    assert_eq!(list.len(), registry().len());
    for (entry, spec) in list.iter().zip(registry()) {
        let keys: Vec<&str> = entry
            .as_object()
            .expect("an object")
            .keys()
            .map(String::as_str)
            .collect();
        assert_eq!(keys.len(), 4, "{entry}");
        assert_eq!(entry["id"], spec.id);
        assert_eq!(entry["implemented"], spec.implemented);
        assert_eq!(entry["summary"], spec.summary);
        assert!(entry["area"].is_string());
    }
    let tab_open = list.iter().find(|entry| entry["id"] == "tab.open").unwrap();
    assert_eq!(tab_open["area"], "tabs");
    assert_eq!(tab_open["implemented"], false);
}

#[test]
fn run_space_create_prints_one_line_and_exits_0() {
    let output = myarcctl(&["run", "space.create", r#"{"name":"Work"}"#]);
    assert_eq!(output.status.code(), Some(0), "{}", stderr(&output));
    let text = stdout(&output);
    assert_eq!(text.lines().count(), 1, "{text}");
    assert!(text.starts_with("space.create: ok "), "{text}");
    assert!(text.contains(r#""name":"Work""#), "{text}");
}

#[test]
fn run_json_prints_the_response() {
    let output = myarcctl(&[
        "run",
        "space.rename",
        r#"{"space_id":1,"name":"Work"}"#,
        "--json",
    ]);
    assert_eq!(output.status.code(), Some(0), "{}", stderr(&output));
    let response = json_stdout(&output);
    assert!(response["id"].as_str().unwrap().starts_with("cli-"));
    assert_eq!(response["command"], "space.rename");
    assert_eq!(response["status"], "success");
    assert_eq!(response["data"]["space"]["id"], 1);
    assert_eq!(response["data"]["space"]["name"], "Work");
}

#[test]
fn each_run_starts_from_a_fresh_workspace() {
    myarcctl(&["run", "space.create", r#"{"name":"Work"}"#]);
    let response = json_stdout(&myarcctl(&["run", "space.list", "--json"]));
    assert_eq!(response["data"]["spaces"].as_array().unwrap().len(), 1);
}

#[test]
fn unsupported_command_exits_1() {
    let output = myarcctl(&["run", "tab.open", r#"{"url":"https://example.com"}"#]);
    assert_eq!(output.status.code(), Some(1));
    assert!(stdout(&output).is_empty());
    assert!(
        stderr(&output).starts_with("tab.open: unsupported: "),
        "{}",
        stderr(&output)
    );

    let output = myarcctl(&["run", "tab.open", "--json"]);
    assert_eq!(output.status.code(), Some(1));
    let response = json_stdout(&output);
    assert_eq!(response["status"], "unsupported");
    assert_eq!(response["command"], "tab.open");
}

#[test]
fn invalid_params_and_unknown_commands_exit_1() {
    let output = myarcctl(&["run", "space.rename", r#"{"space_id":99,"name":"x"}"#]);
    assert_eq!(output.status.code(), Some(1));
    assert!(stderr(&output).contains("invalid: Space 99 does not exist"));

    let output = myarcctl(&["run", "space.explode"]);
    assert_eq!(output.status.code(), Some(1));
    assert!(stderr(&output).contains("Unknown command"));
}

#[test]
fn bad_json_params_exit_2() {
    let output = myarcctl(&["run", "space.create", "{name"]);
    assert_eq!(output.status.code(), Some(2));
    assert!(stdout(&output).is_empty());
    assert!(stderr(&output).contains("params are not valid JSON"));
    assert!(stderr(&output).contains("Usage:"));
}

#[test]
fn usage_errors_exit_2() {
    for args in [
        &[][..],
        &["launch"][..],
        &["run"][..],
        &["run", "space.list", "{}", "extra"][..],
        &["commands", "--verbose"][..],
        &["mcp", "now"][..],
    ] {
        let output = myarcctl(args);
        assert_eq!(output.status.code(), Some(2), "{args:?}");
        assert!(stdout(&output).is_empty(), "{args:?}");
        assert!(stderr(&output).contains("Usage:"), "{args:?}");
    }
}

#[test]
fn help_mentions_the_fresh_workspace() {
    let output = myarcctl(&["--help"]);
    assert_eq!(output.status.code(), Some(0));
    assert!(stdout(&output).contains("fresh in-memory"));
}

#[test]
fn version_prints_crate_and_protocol_versions() {
    let output = myarcctl(&["--version"]);
    assert_eq!(output.status.code(), Some(0));
    assert_eq!(
        stdout(&output).trim(),
        format!(
            "myarcctl {} (control protocol {PROTOCOL_VERSION})",
            env!("CARGO_PKG_VERSION")
        )
    );
}
