//! Runs the short commands against `myarcctl serve-local`, which has no pages
//! and answers the tab and page commands with an error. `MYARCCTL_ECHO_REQUESTS`
//! makes each command write the request it sends, so these tests check that
//! the short commands build the right requests from the state file and flags.

use std::io::{BufRead, BufReader};
use std::os::unix::fs::PermissionsExt;
use std::path::PathBuf;
use std::process::{Child, Command, Output, Stdio};
use std::sync::atomic::{AtomicU32, Ordering};

use serde_json::{Value, json};

/// A `serve-local` process whose control directory sits alone in a temp
/// directory, so the state file inside it is the test's own.
struct Stub {
    child: Child,
    base: PathBuf,
}

impl Stub {
    fn start() -> Self {
        static NEXT: AtomicU32 = AtomicU32::new(0);
        let n = NEXT.fetch_add(1, Ordering::Relaxed);
        let base = std::env::temp_dir().join(format!("mcs-{}-{n}", std::process::id()));
        let _ = std::fs::remove_dir_all(&base);
        std::fs::create_dir_all(&base).unwrap();
        let mut child = Command::new(env!("CARGO_BIN_EXE_myarcctl"))
            .arg("serve-local")
            .arg("--dir")
            .arg(base.join("control"))
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::inherit())
            .spawn()
            .expect("serve-local should start");
        let mut ready = String::new();
        BufReader::new(child.stdout.take().expect("stdout"))
            .read_line(&mut ready)
            .expect("serve-local should print a ready line");
        assert!(ready.contains("serving an in-memory workspace"), "{ready}");
        Self { child, base }
    }

    fn state_path(&self) -> PathBuf {
        self.base.join("control").join("cli-state.json")
    }

    fn write_state(&self, state: Value) {
        std::fs::write(self.state_path(), state.to_string()).unwrap();
    }

    fn state(&self) -> Value {
        serde_json::from_slice(&std::fs::read(self.state_path()).unwrap()).unwrap()
    }

    fn myarcctl(&self, args: &[&str]) -> Output {
        Command::new(env!("CARGO_BIN_EXE_myarcctl"))
            .args(args)
            .env("MYARC_CONTROL_DIR", self.base.join("control"))
            .env("MYARCCTL_ECHO_REQUESTS", "1")
            .output()
            .expect("myarcctl should start")
    }
}

impl Drop for Stub {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
        let _ = std::fs::remove_dir_all(&self.base);
    }
}

fn stderr(output: &Output) -> String {
    String::from_utf8(output.stderr.clone()).unwrap()
}

/// The `{command, params}` of each request the command echoed.
fn requests(output: &Output) -> Vec<Value> {
    stderr(output)
        .lines()
        .filter_map(|line| line.strip_prefix("request: "))
        .map(|text| {
            let request: Value = serde_json::from_str(text).unwrap();
            assert!(request["id"].as_str().unwrap().starts_with("cli-"));
            json!({ "command": request["command"], "params": request["params"] })
        })
        .collect()
}

/// Runs a command the stub answers with an error and returns its request.
fn sent(stub: &Stub, args: &[&str]) -> Value {
    let output = stub.myarcctl(args);
    assert_eq!(
        output.status.code(),
        Some(1),
        "{args:?}: {}",
        stderr(&output)
    );
    assert!(output.stdout.is_empty(), "{args:?}");
    let mut requests = requests(&output);
    assert_eq!(requests.len(), 1, "{args:?}");
    requests.remove(0)
}

#[test]
fn short_commands_build_registry_requests() {
    let stub = Stub::start();
    stub.write_state(json!({ "tab": "t12", "revision": "1.5" }));
    let cases = [
        (
            &["open", "https://example.test/"][..],
            json!({ "command": "tab.open", "params": { "url": "https://example.test/" } }),
        ),
        (
            &["snapshot", "-i"][..],
            json!({ "command": "page.snapshot", "params": { "tab": "t12" } }),
        ),
        (
            &["click", "@e4"][..],
            json!({ "command": "page.click", "params": { "tab": "t12", "revision": "1.5", "ref": "e4" } }),
        ),
        (
            &["fill", "e1", "me@example.test"][..],
            json!({ "command": "page.fill", "params": { "tab": "t12", "revision": "1.5", "ref": "e1", "text": "me@example.test" } }),
        ),
        (
            &["fill", "e1", "--", "--dash"][..],
            json!({ "command": "page.fill", "params": { "tab": "t12", "revision": "1.5", "ref": "e1", "text": "--dash" } }),
        ),
        (
            &["select", "@e3", "Pro"][..],
            json!({ "command": "page.select", "params": { "tab": "t12", "revision": "1.5", "ref": "e3", "value": "Pro" } }),
        ),
        (
            &["press", "Enter"][..],
            json!({ "command": "page.press", "params": { "tab": "t12", "revision": "1.5", "key": "Enter" } }),
        ),
        (
            &["press", "Enter", "@e1"][..],
            json!({ "command": "page.press", "params": { "tab": "t12", "revision": "1.5", "key": "Enter", "ref": "e1" } }),
        ),
        (
            &["back"][..],
            json!({ "command": "tab.back", "params": { "tab": "t12" } }),
        ),
        (
            &["forward"][..],
            json!({ "command": "tab.forward", "params": { "tab": "t12" } }),
        ),
        (
            &["reload"][..],
            json!({ "command": "tab.reload", "params": { "tab": "t12" } }),
        ),
        (
            &["close"][..],
            json!({ "command": "tab.close", "params": { "tab": "t12" } }),
        ),
        (
            &["tabs"][..],
            json!({ "command": "tabs.list", "params": {} }),
        ),
        (
            &["click", "e4", "--tab", "t3", "--rev", "2.1"][..],
            json!({ "command": "page.click", "params": { "tab": "t3", "revision": "2.1", "ref": "e4" } }),
        ),
    ];
    for (args, expected) in cases {
        assert_eq!(sent(&stub, args), expected, "{args:?}");
    }
    // Errors leave the state alone.
    assert_eq!(stub.state()["tab"], "t12");
    assert_eq!(stub.state()["revision"], "1.5");
}

#[test]
fn use_sets_the_tab_and_actions_need_a_revision() {
    let stub = Stub::start();
    let output = stub.myarcctl(&["use", "t7"]);
    assert_eq!(output.status.code(), Some(0), "{}", stderr(&output));
    assert_eq!(String::from_utf8_lossy(&output.stdout), "current tab t7\n");
    assert!(requests(&output).is_empty());
    let mode = std::fs::metadata(stub.state_path())
        .unwrap()
        .permissions()
        .mode();
    assert_eq!(mode & 0o777, 0o600);
    assert_eq!(stub.state()["tab"], "t7");
    assert_eq!(stub.state()["revision"], Value::Null);

    assert_eq!(
        sent(&stub, &["snapshot"]),
        json!({ "command": "page.snapshot", "params": { "tab": "t7" } })
    );
    let output = stub.myarcctl(&["click", "@e4"]);
    assert_eq!(output.status.code(), Some(2));
    assert!(stderr(&output).contains("no revision for t7; run myarcctl snapshot"));
    assert!(requests(&output).is_empty());
}

#[test]
fn usage_errors_send_nothing() {
    let stub = Stub::start();
    for args in [
        &["click", "@e4"][..],
        &["back"][..],
        &["click", "4"][..],
        &["click", "@e4", "extra"][..],
        &["use", "12"][..],
        &["snapshot", "--tab", "x"][..],
        &["snapshot", "--rev", "latest"][..],
        &["run", "space.list", "--tab", "t1"][..],
        &["open", "https://example.test/", "--local"][..],
    ] {
        let output = stub.myarcctl(args);
        assert_eq!(output.status.code(), Some(2), "{args:?}");
        assert!(stderr(&output).contains("Usage:"), "{args:?}");
        assert!(requests(&output).is_empty(), "{args:?}");
    }
    let output = stub.myarcctl(&["click", "@e4"]);
    assert!(stderr(&output).contains("no current tab"));
}

#[test]
fn json_prints_the_raw_response() {
    let stub = Stub::start();
    let output = stub.myarcctl(&["open", "https://example.test/", "--json"]);
    assert_eq!(output.status.code(), Some(1));
    let response: Value = serde_json::from_slice(&output.stdout).unwrap();
    assert_eq!(response["command"], "tab.open");
    assert_ne!(response["status"], "success");
}

#[test]
fn short_commands_without_a_browser_exit_3() {
    let base = std::env::temp_dir().join(format!("mcs-none-{}", std::process::id()));
    for args in [&["open", "https://example.test/"][..], &["tabs"][..]] {
        let output = Command::new(env!("CARGO_BIN_EXE_myarcctl"))
            .args(args)
            .env("MYARC_CONTROL_DIR", base.join("control"))
            .output()
            .unwrap();
        assert_eq!(output.status.code(), Some(3), "{args:?}");
        assert!(stderr(&output).starts_with("myarcctl: myArc isn't running"));
    }
    assert!(!base.exists());
}

#[test]
fn a_planted_state_symlink_leaves_its_target_alone() {
    let stub = Stub::start();
    let victim = stub.base.join("victim");
    std::fs::write(&victim, "precious").unwrap();
    std::fs::set_permissions(&victim, std::fs::Permissions::from_mode(0o644)).unwrap();
    std::os::unix::fs::symlink(&victim, stub.state_path()).unwrap();
    let output = stub.myarcctl(&["use", "t1"]);
    assert_eq!(output.status.code(), Some(0), "{}", stderr(&output));
    assert!(
        stderr(&output).contains("is a symlink"),
        "{}",
        stderr(&output)
    );
    assert_eq!(std::fs::read_to_string(&victim).unwrap(), "precious");
    let mode = std::fs::metadata(&victim).unwrap().permissions().mode();
    assert_eq!(mode & 0o777, 0o644);
    let meta = std::fs::symlink_metadata(stub.state_path()).unwrap();
    assert!(meta.file_type().is_file());
    assert_eq!(stub.state()["tab"], "t1");
}
