use std::collections::BTreeSet;
use std::io::Write;
use std::process::{Command, Output, Stdio};
use std::thread;

use myarc_control::registry;
use serde_json::{Value, json};

/// What the binary reports: the build's `MYARC_VERSION`, else the crate
/// version marked `-dev`, the same rule as `VERSION` in main.rs.
const EXPECTED_VERSION: &str = match option_env!("MYARC_VERSION") {
    Some(version) => version,
    None => concat!(env!("CARGO_PKG_VERSION"), "-dev"),
};

/// Runs `myarc mcp` with `input` on stdin, then closes stdin.
fn session(input: Vec<u8>) -> Output {
    let mut child = Command::new(env!("CARGO_BIN_EXE_myarc"))
        .args(["mcp", "--local"])
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .expect("myarc mcp should start");
    let mut stdin = child.stdin.take().expect("stdin");
    let writer = thread::spawn(move || stdin.write_all(&input));
    let output = child.wait_with_output().expect("myarc mcp should exit");
    writer.join().unwrap().expect("stdin write");
    output
}

fn lines(messages: &[Value]) -> Vec<u8> {
    let mut input = Vec::new();
    for message in messages {
        input.extend(message.to_string().bytes());
        input.push(b'\n');
    }
    input
}

/// Every stdout line, each of which must be a JSON-RPC 2.0 reply.
fn replies(output: &Output) -> Vec<Value> {
    assert_eq!(output.status.code(), Some(0), "mcp should exit cleanly");
    let text = String::from_utf8(output.stdout.clone()).expect("stdout should be UTF-8");
    text.lines()
        .map(|line| {
            let reply: Value = serde_json::from_str(line)
                .unwrap_or_else(|error| panic!("stdout line is not JSON ({error}): {line}"));
            assert_eq!(reply["jsonrpc"], "2.0", "{line}");
            assert!(reply.get("id").is_some(), "{line}");
            assert!(
                reply.get("result").is_some() != reply.get("error").is_some(),
                "{line}"
            );
            reply
        })
        .collect()
}

fn request(id: Value, method: &str, params: Value) -> Value {
    json!({ "jsonrpc": "2.0", "id": id, "method": method, "params": params })
}

fn call(id: u64, tool: &str, arguments: Value) -> Value {
    request(
        json!(id),
        "tools/call",
        json!({ "name": tool, "arguments": arguments }),
    )
}

fn initialize(version: &str) -> Value {
    request(
        json!(1),
        "initialize",
        json!({
            "protocolVersion": version,
            "capabilities": {},
            "clientInfo": { "name": "test", "version": "0" },
        }),
    )
}

#[test]
fn scripted_session() {
    let mut input = lines(&[
        initialize("2025-06-18"),
        json!({ "jsonrpc": "2.0", "method": "notifications/initialized" }),
        request(json!(2), "tools/list", json!({})),
        call(3, "space_create", json!({ "name": "Work" })),
        call(4, "space_list", json!({})),
        call(5, "tab_open", json!({ "url": "https://example.com" })),
        request(json!("six"), "resources/list", json!({})),
    ]);
    input.extend(b"{\"jsonrpc\": \"2.0\", \"id\": 7, \"method\"\n");
    input.extend(lines(&[request(json!(8), "ping", Value::Null)]));
    let output = session(input);
    let replies = replies(&output);
    assert_eq!(replies.len(), 8, "the notification gets no reply");

    let init = &replies[0];
    assert_eq!(init["id"], 1);
    assert_eq!(init["result"]["protocolVersion"], "2025-06-18");
    assert_eq!(init["result"]["capabilities"], json!({ "tools": {} }));
    assert_eq!(init["result"]["serverInfo"]["name"], "myarc");
    assert_eq!(init["result"]["serverInfo"]["version"], EXPECTED_VERSION);

    let list = &replies[1];
    assert_eq!(list["id"], 2);
    let tools = list["result"]["tools"].as_array().expect("tools array");
    assert_eq!(tools.len(), registry().len());
    let names: BTreeSet<&str> = tools
        .iter()
        .map(|tool| tool["name"].as_str().unwrap())
        .collect();
    assert_eq!(names.len(), tools.len(), "tool names are unique");
    for (tool, spec) in tools.iter().zip(registry()) {
        let name = tool["name"].as_str().unwrap();
        assert!(!name.contains('.'), "{name}");
        assert_eq!(name, spec.id.replace('.', "_"));
        assert_eq!(tool["inputSchema"], spec.params);
        let description = tool["description"].as_str().unwrap();
        assert!(description.starts_with(spec.summary));
        assert_eq!(
            description.ends_with(" (not yet implemented)"),
            !spec.implemented,
            "{description}"
        );
    }

    let created = &replies[2];
    assert_eq!(created["id"], 3);
    assert_eq!(created["result"]["isError"], false);
    let structured = &created["result"]["structuredContent"];
    assert_eq!(structured["command"], "space.create");
    assert_eq!(structured["status"], "success");
    assert_eq!(structured["data"]["space"]["name"], "Work");
    let content = &created["result"]["content"][0];
    assert_eq!(content["type"], "text");
    let text: Value = serde_json::from_str(content["text"].as_str().unwrap()).unwrap();
    assert_eq!(&text, structured);
    let work_id = structured["data"]["space"]["id"].clone();

    let listed = &replies[3];
    assert_eq!(listed["id"], 4);
    assert_eq!(listed["result"]["isError"], false);
    let spaces = listed["result"]["structuredContent"]["data"]["spaces"]
        .as_array()
        .unwrap();
    assert_eq!(spaces.len(), 2, "state persists across calls");
    assert!(
        spaces
            .iter()
            .any(|space| space["id"] == work_id && space["name"] == "Work")
    );

    let tab_open = &replies[4];
    assert_eq!(tab_open["id"], 5);
    assert_eq!(tab_open["result"]["isError"], true);
    assert_eq!(
        tab_open["result"]["structuredContent"]["status"],
        "unsupported"
    );

    let unknown = &replies[5];
    assert_eq!(unknown["id"], "six");
    assert_eq!(unknown["error"]["code"], -32601);

    let malformed = &replies[6];
    assert_eq!(malformed["id"], Value::Null);
    assert_eq!(malformed["error"]["code"], -32700);

    let ping = &replies[7];
    assert_eq!(ping["id"], 8);
    assert_eq!(ping["result"], json!({}));
}

#[test]
fn initialize_echoes_supported_versions_only() {
    let older = replies(&session(lines(&[initialize("2025-03-26")])));
    assert_eq!(older[0]["result"]["protocolVersion"], "2025-03-26");
    let unknown = replies(&session(lines(&[initialize("1999-01-01")])));
    assert_eq!(unknown[0]["result"]["protocolVersion"], "2025-06-18");
}

#[test]
fn bad_params_are_rejected() {
    let replies = replies(&session(lines(&[
        request(json!(1), "initialize", json!({})),
        request(json!(2), "tools/call", json!({ "arguments": {} })),
        call(3, "space_explode", json!({})),
        request(
            json!(4),
            "tools/call",
            json!({ "name": "space_list", "arguments": [1] }),
        ),
        request(json!(5), "tools/list", json!("everything")),
        call(6, "space.list", json!({})),
    ])));
    assert_eq!(replies.len(), 6);
    for (index, reply) in replies.iter().enumerate() {
        assert_eq!(reply["id"], index + 1);
        assert_eq!(reply["error"]["code"], -32602, "{reply}");
    }
}

#[test]
fn invalid_tool_arguments_are_a_tool_error() {
    let replies = replies(&session(lines(&[call(
        1,
        "space_rename",
        json!({ "space_id": 99, "name": "x" }),
    )])));
    assert_eq!(replies[0]["result"]["isError"], true);
    assert_eq!(
        replies[0]["result"]["structuredContent"]["status"],
        "invalid"
    );
}

#[test]
fn invalid_messages_get_invalid_request() {
    let mut input = lines(&[
        json!([request(json!(1), "ping", Value::Null)]),
        json!({ "id": 2, "method": "ping" }),
        json!({ "jsonrpc": "2.0", "id": 3 }),
        json!({ "jsonrpc": "2.0", "id": true, "method": "ping" }),
    ]);
    input.extend(b"\xff\xfe\n\n");
    let replies = replies(&session(input));
    assert_eq!(replies.len(), 5, "the blank line gets no reply");
    for reply in &replies[..4] {
        assert_eq!(reply["error"]["code"], -32600, "{reply}");
    }
    assert_eq!(replies[4]["error"]["code"], -32700);
}

#[test]
fn oversized_line_is_rejected_and_the_session_continues() {
    let mut input = vec![b'a'; 1024 * 1024 + 1];
    input.push(b'\n');
    input.extend(lines(&[request(json!(1), "ping", Value::Null)]));
    // The last message may end at EOF without a newline.
    input.extend(request(json!(2), "ping", Value::Null).to_string().bytes());
    let output = session(input);
    let replies = replies(&output);
    assert_eq!(replies.len(), 3);
    assert_eq!(replies[0]["error"]["code"], -32600);
    assert_eq!(replies[1]["id"], 1);
    assert_eq!(replies[2]["id"], 2);
    let stderr = String::from_utf8(output.stderr).unwrap();
    assert!(stderr.contains("rejected a message"), "{stderr}");
}

#[test]
fn empty_input_exits_cleanly() {
    let output = session(Vec::new());
    assert!(replies(&output).is_empty());
}
