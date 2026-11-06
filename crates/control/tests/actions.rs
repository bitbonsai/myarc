use myarc_control::actions::{
    ActKind, ActionResult, MAX_CHANGES, MAX_FILL_BYTES, NAMED_KEYS, PageAction, SelectOption,
    action_response, changes, choose_option, parse_key, prepare,
};
use myarc_control::snapshot::{AxNode, SnapshotPage, SnapshotStore, Taken};
use myarc_control::{ControlService, Outcome, Request, Routed, TabAction, TabState, route};
use myarc_workspace::{DEFAULT_PROFILE_ID, DEFAULT_SPACE_ID, DEFAULT_WINDOW_ID, Workspace};
use serde_json::{Value, json};

const PAGE: &str = "http://127.0.0.1:8000/form.html";

fn request(command: &str, params: Value) -> Request {
    Request {
        id: String::from("r1"),
        command: command.to_owned(),
        params,
    }
}

fn node(id: i32, role: &str, name: &str) -> AxNode {
    AxNode {
        id,
        role: role.to_owned(),
        name: name.to_owned(),
        focusable: true,
        ..AxNode::default()
    }
}

/// A form: email field (e1, node 10), checkbox (e2, 11), select (e3, 12),
/// a disabled button (e4, 13), a read-only field (e5, 14) and a link (e6,
/// 15), under root 1. `checked` sets the checkbox.
fn form(checked: bool) -> Vec<AxNode> {
    let mut root = node(1, "rootWebArea", "Form");
    root.children = (10..=15).collect();
    let mut email = node(10, "textField", "Email");
    email.editable = true;
    let mut news = node(11, "checkBox", "News");
    if checked {
        news.checked = myarc_control::snapshot::Checked::Yes;
    }
    let plan = node(12, "comboBoxSelect", "Plan");
    let mut pay = node(13, "button", "Pay");
    pay.disabled = true;
    let mut code = node(14, "textField", "Code");
    code.editable = true;
    code.read_only = true;
    let mut link = node(15, "link", "Docs");
    link.url = String::from("http://127.0.0.1:8000/docs.html");
    vec![root, email, news, plan, pay, code, link]
}

fn page(document: u64, nodes: Vec<AxNode>, focus: i32) -> SnapshotPage {
    SnapshotPage {
        tab: TabState {
            item: 4,
            url: PAGE.to_owned(),
            title: String::from("Form"),
            ..TabState::default()
        },
        document,
        root: 1,
        focus,
        source_truncated: false,
        nodes,
    }
}

/// A store holding one snapshot of `form(false)` as document 7, and its
/// revision.
fn stored() -> (SnapshotStore, String) {
    let mut store = SnapshotStore::new();
    let response = myarc_control::snapshot::snapshot_response(
        &request("page.snapshot", json!({ "tab": "t4" })),
        myarc_control::snapshot::SnapshotResult::Done(page(7, form(false), 0)),
        &mut store,
    );
    let value = serde_json::to_value(&response).unwrap();
    let revision = value["data"]["revision"].as_str().unwrap().to_owned();
    (store, revision)
}

fn act(revision: &str, element: Option<&str>, kind: ActKind) -> PageAction {
    PageAction {
        item: 4,
        revision: revision.to_owned(),
        element: element.map(str::to_owned),
        kind,
    }
}

fn fill(text: &str) -> ActKind {
    ActKind::Fill {
        text: text.to_owned(),
    }
}

fn press(key: &str) -> ActKind {
    ActKind::Press {
        key: parse_key(key).unwrap(),
    }
}

fn is_invalid(result: Result<impl std::fmt::Debug, Outcome>) -> bool {
    matches!(result, Err(Outcome::Invalid { .. }))
}

fn is_stale(result: Result<impl std::fmt::Debug, Outcome>) -> bool {
    matches!(result, Err(Outcome::Stale { .. }))
}

#[test]
fn prepare_resolves_the_dom_node_and_makes_the_revision_stale() {
    let (mut store, revision) = stored();
    let prepared = prepare(&mut store, 7, &act(&revision, Some("e1"), fill("a@b.c"))).unwrap();
    assert_eq!(prepared.node, 10);
    assert!(!prepared.focus_first);
    assert_eq!(prepared.taken.refs.len(), 6);
    // Once the action began, the old revision is stale.
    assert!(is_stale(prepare(
        &mut store,
        7,
        &act(&revision, Some("e2"), ActKind::Click)
    )));
    assert!(is_stale(store.resolve(7, &revision, "e1")));
}

#[test]
fn prepare_refuses_old_revisions_and_other_documents() {
    let (mut store, revision) = stored();
    // A newer snapshot of the same document.
    store.record(7, vec![10]);
    assert!(is_stale(prepare(
        &mut store,
        7,
        &act(&revision, Some("e1"), ActKind::Click)
    )));
    // The tab navigated: its document now is 8.
    let (mut store, revision) = stored();
    assert!(is_stale(prepare(
        &mut store,
        8,
        &act(&revision, Some("e1"), ActKind::Click)
    )));
}

#[test]
fn unknown_refs_are_invalid_and_keep_the_revision() {
    let (mut store, revision) = stored();
    for element in ["e7", "e99"] {
        assert!(is_invalid(prepare(
            &mut store,
            7,
            &act(&revision, Some(element), ActKind::Click)
        )));
    }
    assert_eq!(store.resolve(7, &revision, "e1"), Ok(10));
}

#[test]
fn fill_needs_an_enabled_writable_text_field() {
    let (mut store, revision) = stored();
    // Checkbox, select, disabled button, read-only field.
    for element in ["e2", "e3", "e4", "e5"] {
        assert!(
            is_invalid(prepare(
                &mut store,
                7,
                &act(&revision, Some(element), fill("x"))
            )),
            "{element}"
        );
    }
    assert!(prepare(&mut store, 7, &act(&revision, Some("e1"), fill(""))).is_ok());
}

#[test]
fn select_needs_a_native_select_and_click_an_enabled_element() {
    let (mut store, revision) = stored();
    let select = |value: &str| ActKind::Select {
        value: value.to_owned(),
    };
    assert!(is_invalid(prepare(
        &mut store,
        7,
        &act(&revision, Some("e1"), select("Pro"))
    )));
    assert!(is_invalid(prepare(
        &mut store,
        7,
        &act(&revision, Some("e4"), ActKind::Click)
    )));
    assert_eq!(
        prepare(&mut store, 7, &act(&revision, Some("e3"), select("Pro")))
            .unwrap()
            .node,
        12
    );
}

#[test]
fn press_focuses_text_fields_only() {
    let (mut store, revision) = stored();
    // The checkbox isn't focused and isn't a text field.
    assert!(is_invalid(prepare(
        &mut store,
        7,
        &act(&revision, Some("e2"), press("Enter"))
    )));
    let prepared = prepare(&mut store, 7, &act(&revision, Some("e1"), press("Enter"))).unwrap();
    assert!(prepared.focus_first);

    // Already focused: no click first, whatever the element.
    let mut store = SnapshotStore::new();
    let response = myarc_control::snapshot::snapshot_response(
        &request("page.snapshot", json!({ "tab": "t4" })),
        myarc_control::snapshot::SnapshotResult::Done(page(7, form(false), 11)),
        &mut store,
    );
    let revision = serde_json::to_value(&response).unwrap()["data"]["revision"]
        .as_str()
        .unwrap()
        .to_owned();
    let prepared = prepare(&mut store, 7, &act(&revision, Some("e2"), press(" "))).unwrap();
    assert!(!prepared.focus_first);
    assert_eq!(prepared.node, 11);
}

#[test]
fn press_without_ref_still_checks_the_revision() {
    let (mut store, revision) = stored();
    assert!(is_stale(prepare(
        &mut store,
        7,
        &act("7.9", None, press("Tab"))
    )));
    let prepared = prepare(&mut store, 7, &act(&revision, None, press("Tab"))).unwrap();
    assert_eq!(prepared.node, 0);
}

#[test]
fn nodes_without_a_dom_node_are_invalid() {
    let mut store = SnapshotStore::new();
    let mut nodes = form(false);
    nodes[1].id = -5;
    nodes[0].children[0] = -5;
    let response = myarc_control::snapshot::snapshot_response(
        &request("page.snapshot", json!({ "tab": "t4" })),
        myarc_control::snapshot::SnapshotResult::Done(page(7, nodes, 0)),
        &mut store,
    );
    let revision = serde_json::to_value(&response).unwrap()["data"]["revision"]
        .as_str()
        .unwrap()
        .to_owned();
    assert!(is_invalid(prepare(
        &mut store,
        7,
        &act(&revision, Some("e1"), ActKind::Click)
    )));
}

#[test]
fn refused_actions_put_the_references_back() {
    let (mut store, revision) = stored();
    let prepared = prepare(&mut store, 7, &act(&revision, Some("e2"), ActKind::Click)).unwrap();
    let response = action_response(
        &request("page.click", json!({})),
        ActionResult::Refused(Outcome::Invalid {
            reason: String::from("hidden"),
        }),
        prepared.taken,
        &mut store,
    );
    assert!(matches!(response.outcome, Outcome::Invalid { .. }));
    assert_eq!(store.resolve(7, &revision, "e2"), Ok(11));
}

#[test]
fn put_back_loses_to_a_newer_snapshot() {
    let (mut store, revision) = stored();
    let prepared = prepare(&mut store, 7, &act(&revision, Some("e2"), ActKind::Click)).unwrap();
    let newer = store.record(7, vec![99]);
    store.put_back(prepared.taken);
    assert!(is_stale(store.resolve(7, &revision, "e1")));
    assert_eq!(store.resolve(7, &newer, "e1"), Ok(99));
}

#[test]
fn done_answers_with_a_new_snapshot_and_changes() {
    let (mut store, revision) = stored();
    let prepared = prepare(&mut store, 7, &act(&revision, Some("e2"), ActKind::Click)).unwrap();
    let mut after = form(true);
    // The link went away and a status line came in.
    after[0].children = vec![10, 11, 12, 13, 14, 16];
    after.push(node(16, "button", "Undo"));
    let response = action_response(
        &request("page.click", json!({ "tab": "t4" })),
        ActionResult::Done {
            page: page(7, after, 0),
            navigated: false,
            settled: true,
        },
        prepared.taken,
        &mut store,
    );
    let value = serde_json::to_value(&response).unwrap();
    assert_eq!(value["status"], "success");
    let data = &value["data"];
    assert_eq!(data["tab"], "t4");
    assert_eq!(data["url"], PAGE);
    assert_eq!(data["navigated"], false);
    assert_eq!(data["settled"], true);
    let new_revision = data["revision"].as_str().unwrap();
    assert_ne!(new_revision, revision);
    assert_eq!(data["snapshot"]["revision"], new_revision);
    assert!(
        data["snapshot"]["text"]
            .as_str()
            .unwrap()
            .contains("checkbox \"News\" [ref=e2] [checked]")
    );
    assert_eq!(
        data["changes"]["changed"],
        json!(["checkbox \"News\" [ref=e2] [checked]"])
    );
    assert_eq!(
        data["changes"]["added"],
        json!(["button \"Undo\" [ref=e6]"])
    );
    assert_eq!(
        data["changes"]["removed"],
        json!(["link \"Docs\" -> /docs.html (was e6)"])
    );
    // The new revision resolves; the old one is stale.
    assert_eq!(store.resolve(7, new_revision, "e6"), Ok(16));
    assert!(is_stale(store.resolve(7, &revision, "e1")));
}

#[test]
fn navigation_reports_no_changes() {
    let (mut store, revision) = stored();
    let prepared = prepare(&mut store, 7, &act(&revision, Some("e6"), ActKind::Click)).unwrap();
    let docs = vec![
        {
            let mut root = node(1, "rootWebArea", "Docs");
            root.children = vec![2];
            root
        },
        node(2, "link", "Back"),
    ];
    let response = action_response(
        &request("page.click", json!({ "tab": "t4" })),
        ActionResult::Done {
            page: page(8, docs, 0),
            navigated: true,
            settled: true,
        },
        prepared.taken,
        &mut store,
    );
    let value = serde_json::to_value(&response).unwrap();
    assert_eq!(value["data"]["navigated"], true);
    assert!(value["data"].get("changes").is_none());
    assert!(
        value["data"]["revision"]
            .as_str()
            .unwrap()
            .starts_with("8.")
    );
}

#[test]
fn unknown_outcomes_keep_the_revision_stale() {
    let (mut store, revision) = stored();
    let prepared = prepare(&mut store, 7, &act(&revision, Some("e2"), ActKind::Click)).unwrap();
    let response = action_response(
        &request("page.click", json!({ "tab": "t4" })),
        ActionResult::Unknown {
            reason: String::from("The page crashed during the action"),
            tab: TabState {
                item: 4,
                url: PAGE.to_owned(),
                ..TabState::default()
            },
            navigated: false,
        },
        prepared.taken,
        &mut store,
    );
    match response.outcome {
        Outcome::OutcomeUnknown { reason, data } => {
            assert!(reason.contains("crashed"));
            assert_eq!(data.unwrap()["tab"], "t4");
        }
        other => panic!("expected outcome_unknown, got {other:?}"),
    }
    assert!(is_stale(store.resolve(7, &revision, "e2")));
}

#[test]
fn changes_are_bounded_and_skip_nodes_without_dom_ids() {
    let target = |node: i32, name: &str| myarc_control::snapshot::RefTarget {
        node,
        head: format!("button \"{name}\""),
        ..Default::default()
    };
    let after: Vec<_> = (1..=30)
        .map(|n| target(n, &n.to_string()))
        .chain([target(-4, "generated")])
        .collect();
    let out = changes(&[target(-4, "generated")], &after);
    assert_eq!(out["added"].as_array().unwrap().len(), MAX_CHANGES);
    assert_eq!(out["more"], 10);
    assert!(out.get("removed").is_none());
    assert_eq!(changes(&after, &after), json!({}));
}

#[test]
fn keys_come_from_the_allowlist() {
    for name in NAMED_KEYS {
        assert!(parse_key(name).is_ok(), "{name}");
    }
    let enter = parse_key("Enter").unwrap();
    assert_eq!(
        (enter.code.as_str(), enter.key_code, enter.text),
        ("Enter", 0x0d, 0x0d)
    );
    let tab = parse_key("Tab").unwrap();
    assert_eq!(tab.text, 0);
    let space = parse_key("Space").unwrap();
    assert_eq!((space.key.as_str(), space.text), (" ", 0x20));
    let upper = parse_key("A").unwrap();
    assert_eq!(
        (upper.code.as_str(), upper.key_code, upper.shift),
        ("KeyA", 0x41, true)
    );
    let question = parse_key("?").unwrap();
    assert_eq!((question.code.as_str(), question.shift), ("Slash", true));
    let bang = parse_key("!").unwrap();
    assert_eq!((bang.code.as_str(), bang.key_code), ("Digit1", 0x31));
    let e = parse_key("é").unwrap();
    assert_eq!((e.code.as_str(), e.key_code, e.text), ("", 0, 0xe9));
    for bad in [
        "",
        "enter",
        "F5",
        "Meta",
        "Control+A",
        "ab",
        "\n",
        "\t",
        "\u{7}",
        "😀",
        "Delete",
    ] {
        assert!(parse_key(bad).is_err(), "{bad:?}");
    }
}

#[test]
fn options_match_by_value_then_by_visible_text() {
    let option = |value: &str, text: &str| SelectOption {
        value: value.to_owned(),
        text: text.to_owned(),
        disabled: false,
    };
    let options = [
        option("free", "Free"),
        option("pro", "Pro  plan"),
        option("Free", "Team"),
        option("old", "Old"),
        option("a", "Twin"),
        option("b", "Twin"),
    ];
    assert_eq!(choose_option(&options, "pro").unwrap(), "pro");
    assert_eq!(choose_option(&options, "Pro plan").unwrap(), "pro");
    // A value wins over another option's text.
    assert_eq!(choose_option(&options, "Free").unwrap(), "Free");
    assert!(matches!(
        choose_option(&options, "Twin"),
        Err(Outcome::Invalid { .. })
    ));
    match choose_option(&options, "Gold") {
        Err(Outcome::Invalid { reason }) => assert!(reason.contains("\"Free\""), "{reason}"),
        other => panic!("{other:?}"),
    }
    let mut disabled = options.to_vec();
    disabled[3].disabled = true;
    assert!(matches!(
        choose_option(&disabled, "old"),
        Err(Outcome::Invalid { .. })
    ));
}

/// A workspace whose agent Space holds tab `agent`, plus a user tab.
fn seeded() -> (Workspace, u64, u64) {
    let mut workspace = Workspace::default();
    let agent = workspace.ensure_agent_space(DEFAULT_PROFILE_ID).unwrap();
    let agent_tab = workspace.add_temporary_item(agent).unwrap();
    let user_tab = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
    (workspace, agent_tab, user_tab)
}

fn routed(workspace: &mut Workspace, command: &str, params: Value) -> Routed {
    route(workspace, DEFAULT_WINDOW_ID, &request(command, params))
}

fn refusal(workspace: &mut Workspace, command: &str, params: Value) -> Outcome {
    match routed(workspace, command, params) {
        Routed::Answered(response) => response.outcome,
        Routed::Tab(action) => panic!("{command} should be refused, got {action:?}"),
    }
}

#[test]
fn route_turns_interaction_commands_into_page_actions() {
    let (mut workspace, agent_tab, _) = seeded();
    let tab = format!("t{agent_tab}");
    let cases = [
        (
            "page.click",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2" }),
            ActKind::Click,
        ),
        (
            "page.fill",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2", "text": "hi\nthere" }),
            fill("hi\nthere"),
        ),
        (
            "page.select",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2", "value": "Pro" }),
            ActKind::Select {
                value: String::from("Pro"),
            },
        ),
        (
            "page.press",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2", "key": "Enter" }),
            press("Enter"),
        ),
    ];
    for (command, params, kind) in cases {
        assert_eq!(
            routed(&mut workspace, command, params),
            Routed::Tab(TabAction::Act(PageAction {
                item: agent_tab,
                revision: String::from("3.1"),
                element: Some(String::from("e2")),
                kind,
            })),
            "{command}"
        );
    }
    let Routed::Tab(TabAction::Act(action)) = routed(
        &mut workspace,
        "page.press",
        json!({ "tab": tab, "revision": "3.1", "key": "Tab" }),
    ) else {
        panic!("press without ref");
    };
    assert_eq!(action.element, None);
}

#[test]
fn route_checks_scope_and_parameters_first() {
    let (mut workspace, agent_tab, user_tab) = seeded();
    let tab = format!("t{agent_tab}");
    let click = |tab: &str| json!({ "tab": tab, "revision": "3.1", "ref": "e2" });
    assert!(matches!(
        refusal(&mut workspace, "page.click", click(&format!("t{user_tab}"))),
        Outcome::Denied { .. }
    ));
    assert!(matches!(
        refusal(&mut workspace, "page.click", click("t999")),
        Outcome::Stale { .. }
    ));
    let invalid = [
        ("page.click", json!({ "tab": tab, "revision": "3.1" })),
        (
            "page.click",
            json!({ "tab": tab, "revision": "x", "ref": "e2" }),
        ),
        (
            "page.click",
            json!({ "tab": tab, "revision": "03.1", "ref": "e2" }),
        ),
        (
            "page.click",
            json!({ "tab": tab, "revision": "3.1", "ref": "e0" }),
        ),
        (
            "page.click",
            json!({ "tab": tab, "revision": "3.1", "ref": "button" }),
        ),
        (
            "page.click",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2", "x": 1 }),
        ),
        (
            "page.fill",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2" }),
        ),
        (
            "page.fill",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2", "text": "a\u{7}" }),
        ),
        (
            "page.fill",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2", "text": "x".repeat(MAX_FILL_BYTES + 1) }),
        ),
        (
            "page.select",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2", "value": 3 }),
        ),
        (
            "page.press",
            json!({ "tab": tab, "revision": "3.1", "key": "F12" }),
        ),
        ("page.press", json!({ "tab": tab, "key": "Enter" })),
    ];
    for (command, params) in invalid {
        assert!(
            matches!(
                refusal(&mut workspace, command, params.clone()),
                Outcome::Invalid { .. }
            ),
            "{command} {params}"
        );
    }
    // The full 64 KiB fits.
    assert!(matches!(
        routed(
            &mut workspace,
            "page.fill",
            json!({ "tab": tab, "revision": "3.1", "ref": "e2", "text": "x".repeat(MAX_FILL_BYTES) })
        ),
        Routed::Tab(_)
    ));
}

#[test]
fn interaction_commands_are_unsupported_without_a_browser() {
    let mut service = ControlService::new();
    for command in ["page.click", "page.fill", "page.select", "page.press"] {
        let response = service.handle(request(command, json!({})));
        assert!(
            matches!(response.outcome, Outcome::Unsupported { .. }),
            "{command}"
        );
    }
}

#[test]
fn taken_default_is_empty() {
    assert!(Taken::default().refs.is_empty());
}
