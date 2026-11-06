//! Replays the sign-in and list steps of the 2026-11-06 skill dogfood
//! against synthetic accessibility trees shaped like the Lantern Depot test
//! pages: refs keep their numbers, each action moves the revision one step,
//! focus alone is no change, and label text doesn't repeat a field's name.

use myarc_control::actions::{ActKind, ActionResult, PageAction, action_response, prepare};
use myarc_control::snapshot::{
    AxNode, SnapshotPage, SnapshotResult, SnapshotStore, snapshot_response,
};
use myarc_control::{Request, TabState};
use serde_json::{Value, json};

const SITE: &str = "http://127.0.0.1:8791";

fn node(id: i32, role: &str, name: &str, children: &[i32]) -> AxNode {
    AxNode {
        id,
        role: role.to_owned(),
        name: name.to_owned(),
        children: children.to_vec(),
        ..AxNode::default()
    }
}

fn text(id: i32, value: &str) -> AxNode {
    node(id, "staticText", value, &[])
}

fn field(id: i32, role: &str, name: &str, value: &str) -> Vec<AxNode> {
    let mut field = node(id, role, name, &[]);
    field.focusable = true;
    field.editable = true;
    field.value = value.to_owned();
    if value.is_empty() {
        return vec![field];
    }
    // Chromium puts the value inside the field as text.
    field.children = vec![id + 1000];
    vec![
        field,
        node(id + 1000, "genericContainer", "", &[id + 2000]),
        text(id + 2000, value),
    ]
}

fn link(id: i32, name: &str, path: &str) -> Vec<AxNode> {
    let mut link = node(id, "link", name, &[id + 1000]);
    link.focusable = true;
    link.url = format!("{SITE}{path}");
    vec![link, text(id + 1000, name)]
}

/// The sign-in page. `user` fills the Username field, and `signed_in`
/// hides the form and shows the welcome line. Hidden elements are left out
/// of the tree, as Chromium does.
fn sign_in(user: &str, signed_in: bool) -> Vec<AxNode> {
    let shown = if signed_in { [2, 30] } else { [2, 4] };
    let mut nodes = vec![
        node(1, "rootWebArea", "Lantern Depot · Sign in", &shown),
        {
            let mut heading = node(2, "heading", "Lantern Depot", &[3]);
            heading.level = 1;
            heading
        },
        text(3, "Lantern Depot"),
    ];
    nodes.push(node(4, "form", "", &[5, 7, 8, 10, 11]));
    nodes.push(node(5, "labelText", "", &[6]));
    nodes.push(text(6, "Username"));
    let mut username = field(7, "textField", "Username", user);
    username[0].required = true;
    nodes.extend(username);
    nodes.push(node(8, "labelText", "", &[9]));
    nodes.push(text(9, "Password"));
    let mut password = field(10, "textField", "Password", "");
    password[0].required = true;
    password[0].protected = true;
    nodes.extend(password);
    let mut button = node(11, "button", "Sign in", &[12]);
    button.focusable = true;
    nodes.push(button);
    nodes.push(text(12, "Sign in"));
    nodes.push(node(30, "genericContainer", "", &[31, 33]));
    nodes.push(node(31, "paragraph", "", &[32]));
    nodes.push(text(32, &format!("Welcome, {user}")));
    nodes.extend(link(33, "Go to the lantern list", "/list.html"));
    nodes
}

const ITEMS: [&str; 5] = [
    "Item 1: Copper storm lantern",
    "Item 2: Paper festival lantern",
    "Item 3: Brass ship lantern",
    "Item 4: Glass garden lantern",
    "Item 5: Tin camping lantern",
];

/// The list page with `term` in the search field, showing the items whose
/// text contains it.
fn list(term: &str) -> Vec<AxNode> {
    let mut heading = node(2, "heading", "Lanterns", &[3]);
    heading.level = 1;
    let mut nodes = vec![
        node(
            1,
            "rootWebArea",
            "Lantern Depot · Lanterns",
            &[2, 4, 50, 90],
        ),
        heading,
        text(3, "Lanterns"),
        node(4, "labelText", "", &[5, 6]),
        text(5, "Search"),
    ];
    nodes.extend(field(6, "searchBox", "Search", term));
    let hit = |item: &str| item.to_lowercase().contains(term);
    let rows: Vec<i32> = (0..5)
        .filter(|&n| hit(ITEMS[n as usize]))
        .map(|n| 60 + n * 3)
        .collect();
    let shown = rows.len();
    nodes.push(node(50, "list", "", &rows));
    for (n, item) in ITEMS.iter().enumerate() {
        let id = 60 + n as i32 * 3;
        nodes.push(node(id, "listItem", "", &[id + 1, id + 2]));
        nodes.push(text(id + 1, item));
        nodes.extend(link(
            id + 2,
            "Details",
            &format!("/details.html?id={}", n + 1),
        ));
    }
    nodes.push(node(90, "paragraph", "", &[91]));
    nodes.push(text(91, &format!("{shown} lanterns")));
    nodes
}

fn page(document: u64, path: &str, nodes: Vec<AxNode>, focus: i32) -> SnapshotPage {
    SnapshotPage {
        tab: TabState {
            item: 2,
            url: format!("{SITE}{path}"),
            title: String::from("Lantern Depot"),
            ..TabState::default()
        },
        document,
        root: 1,
        focus,
        source_truncated: false,
        nodes,
    }
}

fn request(command: &str) -> Request {
    Request {
        id: String::from("r1"),
        command: command.to_owned(),
        params: json!({ "tab": "t2" }),
    }
}

fn snapshot(store: &mut SnapshotStore, page: SnapshotPage) -> Value {
    let response = snapshot_response(&request("page.snapshot"), SnapshotResult::Done(page), store);
    serde_json::to_value(&response).unwrap()["data"].clone()
}

/// Runs `kind` on `element` of `revision` and answers with `after`.
fn act(
    store: &mut SnapshotStore,
    revision: &str,
    element: &str,
    kind: ActKind,
    after: SnapshotPage,
    navigated: bool,
) -> Value {
    let document = revision.split('.').next().unwrap().parse().unwrap();
    let action = PageAction {
        item: 2,
        revision: revision.to_owned(),
        element: Some(element.to_owned()),
        kind,
    };
    let prepared = prepare(store, document, &action).expect("the ref resolves");
    let response = action_response(
        &request("page.click"),
        ActionResult::Done {
            page: after,
            navigated,
            settled: true,
        },
        prepared.taken,
        store,
    );
    let data = serde_json::to_value(&response).unwrap()["data"].clone();
    eprintln!(
        "{element} -> rev {} changes {}\n{}",
        data["revision"],
        data.get("changes").unwrap_or(&Value::Null),
        data["snapshot"]["text"].as_str().unwrap()
    );
    data
}

fn fill(text: &str) -> ActKind {
    ActKind::Fill {
        text: text.to_owned(),
    }
}

#[test]
fn sign_in_keeps_refs_and_moves_one_revision_per_action() {
    let mut store = SnapshotStore::new();
    let first = snapshot(&mut store, page(1, "/", sign_in("", false), 0));
    assert_eq!(first["revision"], "1.1");
    assert_eq!(
        first["text"],
        "\
heading \"Lantern Depot\" [level=1]
form
  textbox \"Username\" [ref=e1] [required] value=\"\"
  textbox \"Password\" [ref=e2] [required]
  button \"Sign in\" [ref=e3]
"
    );
    // A second snapshot of the same page keeps the revision.
    let again = snapshot(&mut store, page(1, "/", sign_in("", false), 0));
    assert_eq!(again["revision"], "1.1");

    let filled = act(
        &mut store,
        "1.1",
        "e1",
        fill("dana"),
        page(1, "/", sign_in("dana", false), 7),
        false,
    );
    assert_eq!(filled["revision"], "1.2");
    assert_eq!(filled["changes"], json!({ "changed": ["e1 value"] }));

    // The password value never shows, so only focus moved: no changes.
    let password = act(
        &mut store,
        "1.2",
        "e2",
        fill("lantern-demo"),
        page(1, "/", sign_in("dana", false), 10),
        false,
    );
    assert_eq!(password["revision"], "1.3");
    assert_eq!(password["changes"], json!({}));

    let welcome = act(
        &mut store,
        "1.3",
        "e3",
        ActKind::Click,
        page(1, "/", sign_in("dana", true), 0),
        false,
    );
    assert_eq!(welcome["revision"], "1.4");
    // The link is new to the page, so it takes e4, never the old e1.
    assert_eq!(
        welcome["snapshot"]["text"],
        "\
heading \"Lantern Depot\" [level=1]
text \"Welcome, dana\"
link \"Go to the lantern list\" [ref=e4] -> /list.html
"
    );
    // Four changes on a three line outline: the outline says it better.
    assert_eq!(welcome["changes"], json!({ "many": true }));
    // The Username field left the page; its ref is stale, not the link.
    assert!(matches!(
        store.resolve(1, "1.4", "e1"),
        Err(myarc_control::Outcome::Stale { .. })
    ));
    assert_eq!(store.resolve(1, "1.4", "e4"), Ok(33));
}

#[test]
fn filtering_the_list_keeps_the_item_refs_and_drops_the_label_echo() {
    let mut store = SnapshotStore::new();
    let first = snapshot(&mut store, page(2, "/list.html", list(""), 0));
    assert_eq!(first["revision"], "2.1");
    assert_eq!(
        first["text"],
        "\
heading \"Lanterns\" [level=1]
searchbox \"Search\" [ref=e1] value=\"\"
text \"Item 1: Copper storm lantern\"
link \"Details\" [ref=e2] -> /details.html?id=1
text \"Item 2: Paper festival lantern\"
link \"Details\" [ref=e3] -> /details.html?id=2
text \"Item 3: Brass ship lantern\"
link \"Details\" [ref=e4] -> /details.html?id=3
text \"Item 4: Glass garden lantern\"
link \"Details\" [ref=e5] -> /details.html?id=4
text \"Item 5: Tin camping lantern\"
link \"Details\" [ref=e6] -> /details.html?id=5
text \"5 lanterns\"
"
    );
    let filtered = act(
        &mut store,
        "2.1",
        "e1",
        fill("item 3"),
        page(2, "/list.html", list("item 3"), 6),
        false,
    );
    assert_eq!(filtered["revision"], "2.2");
    // Item 3 keeps e4, and the label no longer prints above the field.
    assert_eq!(
        filtered["snapshot"]["text"],
        "\
heading \"Lanterns\" [level=1]
searchbox \"Search\" [ref=e1] [focused] value=\"item 3\"
text \"Item 3: Brass ship lantern\"
link \"Details\" [ref=e4] -> /details.html?id=3
text \"1 lanterns\"
"
    );
    assert_eq!(filtered["changes"], json!({ "many": true }));
    assert_eq!(store.resolve(2, "2.2", "e4"), Ok(68));
    assert!(matches!(
        store.resolve(2, "2.2", "e2"),
        Err(myarc_control::Outcome::Stale { .. })
    ));

    // Clearing the filter brings the items back under their old refs.
    let cleared = act(
        &mut store,
        "2.2",
        "e1",
        fill(""),
        page(2, "/list.html", list(""), 6),
        false,
    );
    assert_eq!(cleared["revision"], "2.3");
    assert_eq!(store.resolve(2, "2.3", "e2"), Ok(62));
    assert_eq!(store.resolve(2, "2.3", "e6"), Ok(74));

    // Opening item 3 is a new document, numbered from e1.
    let details = vec![
        node(1, "rootWebArea", "Details", &[2, 4]),
        {
            let mut heading = node(2, "heading", ITEMS[2], &[3]);
            heading.level = 1;
            heading
        },
        text(3, ITEMS[2]),
        {
            let mut button = node(4, "button", "Delete", &[]);
            button.focusable = true;
            button
        },
    ];
    let opened = act(
        &mut store,
        "2.3",
        "e4",
        ActKind::Click,
        page(3, "/details.html?id=3", details, 0),
        true,
    );
    assert_eq!(opened["revision"], "3.1");
    assert!(opened.get("changes").is_none());
    assert_eq!(store.resolve(3, "3.1", "e1"), Ok(4));
}
