//! Runs `myarcctl serve-local` as a stand-in browser and checks that separate
//! `run` and `mcp` processes reach it over the control socket.

use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Output, Stdio};
use std::sync::atomic::{AtomicU32, Ordering};
use std::thread;

use serde_json::{Value, json};

/// A `serve-local` process on a unique temp directory. Dropping it kills the
/// process and removes the directory.
struct Stub {
    child: Child,
    dir: PathBuf,
}

impl Stub {
    fn start() -> Self {
        static NEXT: AtomicU32 = AtomicU32::new(0);
        let n = NEXT.fetch_add(1, Ordering::Relaxed);
        let dir = std::env::temp_dir().join(format!("mct-{}-{n}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        let mut child = Command::new(env!("CARGO_BIN_EXE_myarcctl"))
            .arg("serve-local")
            .arg("--dir")
            .arg(&dir)
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::inherit())
            .spawn()
            .expect("serve-local should start");
        let mut ready = String::new();
        BufReader::new(child.stdout.take().expect("stdout"))
            .read_line(&mut ready)
            .expect("serve-local should print a ready line");
        let stub = Self { child, dir };
        assert!(ready.contains("serving an in-memory workspace"), "{ready}");
        stub
    }

    fn command(&self) -> Command {
        let mut command = Command::new(env!("CARGO_BIN_EXE_myarcctl"));
        command.env("MYARC_CONTROL_DIR", &self.dir);
        command
    }

    fn run(&self, args: &[&str]) -> Output {
        self.command()
            .arg("run")
            .args(args)
            .output()
            .expect("myarcctl run should start")
    }

    fn dir(&self) -> &Path {
        &self.dir
    }
}

impl Drop for Stub {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
        let _ = std::fs::remove_dir_all(&self.dir);
    }
}

fn json_stdout(output: &Output) -> Value {
    assert_eq!(
        output.status.code(),
        Some(0),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    serde_json::from_slice(&output.stdout).expect("stdout should be one JSON value")
}

fn space_names(response: &Value) -> Vec<String> {
    response["data"]["spaces"]
        .as_array()
        .expect("spaces array")
        .iter()
        .map(|space| space["name"].as_str().unwrap().to_owned())
        .collect()
}

#[test]
fn separate_runs_share_the_served_workspace() {
    let stub = Stub::start();
    let created = stub.run(&["space.create", r#"{"name":"Work"}"#]);
    assert_eq!(
        created.status.code(),
        Some(0),
        "{}",
        String::from_utf8_lossy(&created.stderr)
    );
    assert!(String::from_utf8_lossy(&created.stdout).starts_with("space.create: ok "));

    let listed = json_stdout(&stub.run(&["space.list", "--json"]));
    assert_eq!(listed["status"], "success");
    assert_eq!(space_names(&listed), ["Everyday", "Work"]);

    // --local still ignores the endpoint.
    let local = json_stdout(&stub.run(&["space.list", "--json", "--local"]));
    assert_eq!(space_names(&local), ["Everyday"]);

    let unsupported = stub.run(&["tab.open", r#"{"url":"https://example.com"}"#]);
    assert_eq!(unsupported.status.code(), Some(1));
}

#[test]
fn run_reports_a_stale_endpoint_with_exit_3() {
    let stub = Stub::start();
    let dir = stub.dir().to_owned();
    let mut command = stub.command();
    drop(stub);
    // Recreate what a crashed browser leaves: a private directory with an
    // endpoint file whose socket is gone.
    std::fs::create_dir_all(&dir).unwrap();
    std::fs::set_permissions(&dir, std::os::unix::fs::PermissionsExt::from_mode(0o700)).unwrap();
    let endpoint = dir.join("control.json");
    let text = json!({
        "socket": dir.join("control.sock"),
        "token": "00",
        "pid": 1,
        "protocol_version": myarc_control::PROTOCOL_VERSION,
    })
    .to_string();
    std::fs::write(&endpoint, text).unwrap();
    std::fs::set_permissions(
        &endpoint,
        std::os::unix::fs::PermissionsExt::from_mode(0o600),
    )
    .unwrap();
    let output = command.args(["run", "space.list"]).output().unwrap();
    let _ = std::fs::remove_dir_all(&dir);
    assert_eq!(output.status.code(), Some(3));
    let text = String::from_utf8_lossy(&output.stderr);
    assert!(text.starts_with("myarcctl: endpoint is stale"), "{text}");
}

#[test]
fn mcp_session_goes_over_ipc() {
    let stub = Stub::start();
    let mut child = stub
        .command()
        .arg("mcp")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .expect("myarcctl mcp should start");
    let mut input = Vec::new();
    for message in [
        json!({ "jsonrpc": "2.0", "id": 1, "method": "tools/call",
                "params": { "name": "space_create", "arguments": { "name": "Play" } } }),
        json!({ "jsonrpc": "2.0", "id": 2, "method": "tools/call",
                "params": { "name": "space_list", "arguments": {} } }),
    ] {
        input.extend(message.to_string().bytes());
        input.push(b'\n');
    }
    let mut stdin = child.stdin.take().unwrap();
    let writer = thread::spawn(move || stdin.write_all(&input));
    let output = child.wait_with_output().unwrap();
    writer.join().unwrap().unwrap();
    assert_eq!(output.status.code(), Some(0));
    let replies: Vec<Value> = String::from_utf8(output.stdout)
        .unwrap()
        .lines()
        .map(|line| serde_json::from_str(line).unwrap())
        .collect();
    assert_eq!(replies.len(), 2);
    assert_eq!(replies[0]["result"]["isError"], false);
    let listed = &replies[1]["result"]["structuredContent"];
    assert_eq!(space_names(listed), ["Everyday", "Play"]);

    // A separate process sees the Space the MCP session created.
    let listed = json_stdout(&stub.run(&["space.list", "--json"]));
    assert_eq!(space_names(&listed), ["Everyday", "Play"]);
}

#[test]
fn mcp_without_a_browser_answers_tool_errors() {
    let missing = std::env::temp_dir().join(format!("mct-none-{}", std::process::id()));
    let mut child = Command::new(env!("CARGO_BIN_EXE_myarcctl"))
        .arg("mcp")
        .env("MYARC_CONTROL_DIR", &missing)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();
    let message = json!({ "jsonrpc": "2.0", "id": 1, "method": "tools/call",
                           "params": { "name": "space_list" } });
    let mut stdin = child.stdin.take().unwrap();
    writeln!(stdin, "{message}").unwrap();
    drop(stdin);
    let output = child.wait_with_output().unwrap();
    let reply: Value = serde_json::from_slice(&output.stdout).unwrap();
    assert_eq!(reply["result"]["isError"], true);
    let text = reply["result"]["content"][0]["text"].as_str().unwrap();
    assert!(text.starts_with("myArc isn't running"), "{text}");
}
