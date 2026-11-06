use myarc_control::{
    Area, ControlService, MAX_URL_LENGTH, Outcome, Request, Routed, TabAction, TabResult, TabState,
    check_url, in_profile, parse_tab_handle, profile_tag, registry, route, tab_handle,
    tab_response,
};
use myarc_workspace::{DEFAULT_PROFILE_ID, DEFAULT_SPACE_ID, DEFAULT_WINDOW_ID, Workspace};
use serde_json::{Value, json};

fn request(command: &str, params: Value) -> Request {
    Request {
        id: String::from("r1"),
        command: command.to_owned(),
        params,
    }
}

fn routed(workspace: &mut Workspace, command: &str, params: Value) -> Routed {
    route(workspace, DEFAULT_WINDOW_ID, &request(command, params))
}

fn action(workspace: &mut Workspace, command: &str, params: Value) -> TabAction {
    match routed(workspace, command, params) {
        Routed::Tab(action) => action,
        Routed::Answered(response) => panic!("{command} answered {response:?}"),
    }
}

fn refused(workspace: &mut Workspace, command: &str, params: Value) -> Outcome {
    match routed(workspace, command, params) {
        Routed::Answered(response) => response.outcome,
        Routed::Tab(action) => panic!("{command} should be refused, got {action:?}"),
    }
}

/// A workspace with an agent Space holding one tab, and a tab in Everyday.
fn seeded() -> (Workspace, u64, u64) {
    let mut workspace = Workspace::default();
    let agent = workspace.ensure_agent_space(DEFAULT_PROFILE_ID).unwrap();
    let agent_tab = workspace.add_temporary_item(agent).unwrap();
    let user_tab = workspace.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
    (workspace, agent_tab, user_tab)
}

fn selected_space(workspace: &Workspace) -> u64 {
    workspace
        .sidebar_snapshot(DEFAULT_WINDOW_ID)
        .unwrap()
        .current_space
        .id
}

#[test]
fn tab_commands_are_implemented_but_unsupported_without_a_browser() {
    let tabs: Vec<_> = registry()
        .iter()
        .filter(|spec| spec.area == Area::Tabs)
        .collect();
    assert_eq!(tabs.len(), 8);
    let mut service = ControlService::new();
    for spec in tabs {
        assert!(spec.implemented, "{}", spec.id);
        let response = service.handle(request(
            spec.id,
            json!({ "url": "https://example.test/", "tab": "t1" }),
        ));
        match response.outcome {
            Outcome::Unsupported { reason } => {
                assert!(reason.contains("running myArc browser"), "{reason}");
            }
            other => panic!("{} should be unsupported: {other:?}", spec.id),
        }
    }
    // Answered before it ran, so no agent Space appeared.
    let spaces = service.handle(request("space.list", Value::Null));
    let Outcome::Success { data } = spaces.outcome else {
        panic!("space.list failed");
    };
    assert_eq!(data["spaces"].as_array().unwrap().len(), 1);
}

#[test]
fn open_creates_the_agent_space_without_switching() {
    let mut workspace = Workspace::default();
    let opened = action(
        &mut workspace,
        "tab.open",
        json!({ "url": "http://127.0.0.1:8000/page1.html" }),
    );
    let agent = workspace.agent_space_of(DEFAULT_PROFILE_ID).expect("agent");
    assert_eq!(
        opened,
        TabAction::Open {
            space_id: agent,
            url: String::from("http://127.0.0.1:8000/page1.html"),
        }
    );
    assert_eq!(selected_space(&workspace), DEFAULT_SPACE_ID);
    // A second open reuses it.
    let again = action(
        &mut workspace,
        "tab.open",
        json!({ "url": "HTTPS://example.test/" }),
    );
    assert!(matches!(again, TabAction::Open { space_id, .. } if space_id == agent));
}

#[test]
fn only_http_and_https_urls_open() {
    let mut workspace = Workspace::default();
    for url in [
        "file:///etc/hosts",
        "chrome://settings",
        "javascript:alert(1)",
        "data:text/html,<p>hi</p>",
        "about:blank",
        "blob:https://example.test/1",
        "view-source:https://example.test/",
        "https://user:secret@example.test/",
        "http://user@example.test/",
    ] {
        match refused(&mut workspace, "tab.open", json!({ "url": url })) {
            Outcome::Denied { .. } => {}
            other => panic!("{url} should be denied: {other:?}"),
        }
    }
    for url in [
        "",
        "example.test",
        " javascript:alert(1)",
        "java\tscript:alert(1)",
        "https://example.test/\n",
    ] {
        match refused(&mut workspace, "tab.open", json!({ "url": url })) {
            Outcome::Invalid { .. } => {}
            other => panic!("{url:?} should be invalid: {other:?}"),
        }
    }
    let long = format!("https://example.test/{}", "a".repeat(MAX_URL_LENGTH));
    match refused(&mut workspace, "tab.open", json!({ "url": long })) {
        Outcome::Invalid { reason } => assert!(reason.contains("8192"), "{reason}"),
        other => panic!("long URL should be invalid: {other:?}"),
    }
    // Nothing was opened, so no agent Space was made either.
    assert_eq!(workspace.agent_space_of(DEFAULT_PROFILE_ID), None);
    assert!(check_url("https://example.test/a@b?c=d@e").is_ok());
    assert!(check_url(&format!("https://e.test/{}", "a".repeat(8000))).is_ok());
}

#[test]
fn handles_round_trip_and_reject_lookalikes() {
    assert_eq!(tab_handle(12), "t12");
    assert_eq!(parse_tab_handle("t12"), Some(12));
    for handle in [
        "", "t", "12", "t0", "t012", "T12", "t1x", "t-1", "t+1", " t1",
    ] {
        assert_eq!(parse_tab_handle(handle), None, "{handle:?}");
    }
    assert_eq!(parse_tab_handle("t99999999999999999999"), None);
}

#[test]
fn handles_name_their_profile() {
    assert_eq!(profile_tag("Default"), "");
    assert_eq!(profile_tag("Profile 1"), "p1");
    assert_eq!(profile_tag("Profile 12"), "p12");
    let odd = profile_tag("Profile 01");
    assert!(odd.starts_with('x') && odd.len() == 17, "{odd}");
    let custom = profile_tag("Work");
    assert!(custom.starts_with('x') && custom.len() == 17, "{custom}");
    assert_eq!(profile_tag("Work"), custom);
    assert_ne!(profile_tag("Work 2"), custom);

    assert_eq!(in_profile("p1", || tab_handle(12)), "t12-p1");
    assert_eq!(tab_handle(12), "t12", "the tag ends with in_profile");
    assert_eq!(parse_tab_handle("t12-p1"), Some(12));
    assert_eq!(parse_tab_handle(&format!("t12-{custom}")), Some(12));
    for handle in [
        "t12-",
        "t12-p",
        "t12-p01",
        "t12-q1",
        "t12-xABC",
        "t12-p1-p2",
        "t-p1",
    ] {
        assert_eq!(parse_tab_handle(handle), None, "{handle:?}");
    }
}

#[test]
fn a_handle_from_another_profile_is_denied() {
    let (mut workspace, agent_tab, _) = seeded();
    let default_handle = tab_handle(agent_tab);
    let p1_handle = in_profile("p1", || tab_handle(agent_tab));
    let p2_handle = in_profile("p2", || tab_handle(agent_tab));
    // Both Profiles number their items from 1, so only the tag tells them apart.
    for (profile, handle) in [
        ("p1", &default_handle),
        ("p1", &p2_handle),
        ("", &p1_handle),
    ] {
        for command in ["tab.close", "tab.navigate", "page.snapshot"] {
            let params = json!({ "tab": handle, "url": "https://example.test/" });
            let params = if command == "tab.navigate" {
                params
            } else {
                json!({ "tab": handle })
            };
            match in_profile(profile, || refused(&mut workspace, command, params)) {
                Outcome::Denied { reason } => {
                    assert!(reason.contains("another Profile"), "{reason}")
                }
                other => panic!("{command} {handle} in {profile:?} should be denied: {other:?}"),
            }
        }
    }
    assert_eq!(
        in_profile("p1", || action(
            &mut workspace,
            "tab.close",
            json!({ "tab": p1_handle })
        )),
        TabAction::Close { item: agent_tab }
    );
    let tab = TabState {
        item: agent_tab,
        ..TabState::default()
    };
    let response = in_profile("p1", || {
        tab_response(&request("tabs.list", json!({})), TabResult::Done(vec![tab]))
    });
    match response.outcome {
        Outcome::Success { data } => assert_eq!(data["tabs"][0]["handle"], p1_handle),
        other => panic!("tabs.list should succeed: {other:?}"),
    }
}

#[test]
fn tab_handles_are_scoped_to_the_agent_space() {
    let (mut workspace, agent_tab, user_tab) = seeded();
    let handle = tab_handle(agent_tab);
    assert_eq!(
        action(&mut workspace, "tab.reload", json!({ "tab": handle })),
        TabAction::Reload { item: agent_tab }
    );
    assert_eq!(
        action(
            &mut workspace,
            "tab.navigate",
            json!({ "tab": handle, "url": "https://example.test/2" }),
        ),
        TabAction::Navigate {
            item: agent_tab,
            url: String::from("https://example.test/2"),
        }
    );
    for command in [
        "tab.activate",
        "tab.close",
        "tab.back",
        "tab.forward",
        "tab.reload",
    ] {
        match refused(
            &mut workspace,
            command,
            json!({ "tab": tab_handle(user_tab) }),
        ) {
            Outcome::Denied { reason } => {
                assert!(reason.contains("outside the agent Space"), "{reason}")
            }
            other => panic!("{command} should be denied: {other:?}"),
        }
        match refused(&mut workspace, command, json!({ "tab": "t999" })) {
            Outcome::Stale { reason } => assert!(reason.contains("t999"), "{reason}"),
            other => panic!("{command} should be stale: {other:?}"),
        }
        assert!(matches!(
            refused(&mut workspace, command, json!({ "tab": "tab-1" })),
            Outcome::Invalid { .. }
        ));
    }
    // A closed tab's handle goes stale.
    workspace.live_tab_closed(agent_tab).unwrap();
    assert!(matches!(
        refused(&mut workspace, "tab.back", json!({ "tab": handle })),
        Outcome::Stale { .. }
    ));
    // A denied navigate never checks its URL first.
    assert!(matches!(
        refused(
            &mut workspace,
            "tab.navigate",
            json!({ "tab": tab_handle(user_tab), "url": "file:///etc/hosts" }),
        ),
        Outcome::Denied { .. }
    ));
}

#[test]
fn list_names_only_agent_space_items() {
    let mut empty = Workspace::default();
    empty.add_temporary_item(DEFAULT_SPACE_ID).unwrap();
    assert_eq!(
        action(&mut empty, "tabs.list", Value::Null),
        TabAction::List { items: vec![] }
    );
    assert_eq!(empty.agent_space_of(DEFAULT_PROFILE_ID), None);

    let (mut workspace, agent_tab, _) = seeded();
    let agent = workspace.agent_space_of(DEFAULT_PROFILE_ID).unwrap();
    let second = workspace.add_temporary_item(agent).unwrap();
    assert_eq!(
        action(&mut workspace, "tabs.list", Value::Null),
        TabAction::List {
            items: vec![agent_tab, second]
        }
    );
    assert!(matches!(
        refused(&mut workspace, "tabs.list", json!({ "tab": "t1" })),
        Outcome::Invalid { .. }
    ));
}

#[test]
fn space_commands_still_run_through_route() {
    let mut workspace = Workspace::default();
    match routed(&mut workspace, "space.create", json!({ "name": "Work" })) {
        Routed::Answered(response) => {
            assert!(matches!(response.outcome, Outcome::Success { .. }))
        }
        Routed::Tab(action) => panic!("space.create became {action:?}"),
    }
    assert_eq!(workspace.snapshot().spaces.len(), 2);
}

#[test]
fn tab_responses_carry_the_tab_state() {
    let tab = TabState {
        item: 7,
        url: String::from("http://127.0.0.1:8000/page1.html"),
        title: "t".repeat(5000),
        loading: false,
        can_go_back: true,
        can_go_forward: false,
    };
    let state = json!({
        "handle": "t7",
        "url": "http://127.0.0.1:8000/page1.html",
        "title": "t".repeat(1024),
        "loading": false,
        "can_go_back": true,
        "can_go_forward": false,
    });
    let shape = |command: &str, result: TabResult| {
        serde_json::to_value(tab_response(&request(command, Value::Null), result)).unwrap()
    };
    assert_eq!(
        shape("tab.back", TabResult::Done(vec![tab.clone()])),
        json!({ "id": "r1", "command": "tab.back", "status": "success", "data": { "tab": state } })
    );
    assert_eq!(
        shape("tabs.list", TabResult::Done(vec![tab.clone()]))["data"],
        json!({ "tabs": [state] })
    );
    assert_eq!(
        shape("tabs.list", TabResult::Done(vec![]))["data"],
        json!({ "tabs": [] })
    );
    assert_eq!(
        shape("tab.close", TabResult::Done(vec![tab.clone()]))["data"],
        json!({ "tab": state, "closed": true })
    );
    assert_eq!(
        shape(
            "tab.navigate",
            TabResult::Unknown {
                reason: String::from("no commit"),
                tabs: vec![tab],
            },
        ),
        json!({
            "id": "r1",
            "command": "tab.navigate",
            "status": "outcome_unknown",
            "reason": "no commit",
            "data": { "tab": state },
        })
    );
    assert_eq!(
        shape(
            "tab.reload",
            TabResult::Failed(Outcome::Stale {
                reason: String::from("gone"),
            }),
        ),
        json!({ "id": "r1", "command": "tab.reload", "status": "stale", "reason": "gone" })
    );
}

#[test]
fn snapshot_is_scoped_to_the_agent_space() {
    let (mut workspace, agent_tab, user_tab) = seeded();
    assert_eq!(
        action(
            &mut workspace,
            "page.snapshot",
            json!({ "tab": tab_handle(agent_tab) })
        ),
        TabAction::Snapshot { item: agent_tab }
    );
    match refused(
        &mut workspace,
        "page.snapshot",
        json!({ "tab": tab_handle(user_tab) }),
    ) {
        Outcome::Denied { reason } => assert!(reason.contains("outside the agent Space")),
        other => panic!("should be denied: {other:?}"),
    }
    assert!(matches!(
        refused(&mut workspace, "page.snapshot", json!({ "tab": "t999" })),
        Outcome::Stale { .. }
    ));
    assert!(matches!(
        refused(
            &mut workspace,
            "page.snapshot",
            json!({ "tab": tab_handle(agent_tab), "depth": 3 })
        ),
        Outcome::Invalid { .. }
    ));
    let mut service = ControlService::new();
    let response = service.handle(request("page.snapshot", json!({ "tab": "t1" })));
    assert!(matches!(response.outcome, Outcome::Unsupported { .. }));
}
