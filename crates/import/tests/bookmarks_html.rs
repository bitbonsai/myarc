#![cfg(feature = "arc-sidebar")]

//! Round trip: the synthetic Arc sidebar file imported into a workspace and
//! exported as bookmarks reads back with the same folders, order and URLs.

use myarc_import::arc_markdown::{Entry, Report, Scheme};
use myarc_import::arc_sidebar::parse;
use myarc_import::arc_workspace::{apply, as_written};
use myarc_import::bookmarks_html::{FAVORITES_FOLDER, write, write_profiles};
use myarc_workspace::Workspace;

const FIXTURE_PATH: &str = concat!(
    env!("CARGO_MANIFEST_DIR"),
    "/../../tests/fixtures/arc-sidebar/StorableSidebar.json"
);

#[derive(Debug, PartialEq, Eq)]
enum Node {
    Folder(String, Vec<Node>),
    Link(String),
}

fn unescape(text: &str) -> String {
    text.replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&#39;", "'")
        .replace("&amp;", "&")
}

/// A small reader for the Netscape format: `<DT><H3>` names the folder whose
/// `<DL>` follows, `<DT><A HREF>` is a bookmark, `</DL>` closes a folder.
fn read_bookmarks(html: &str) -> Vec<Node> {
    assert!(html.starts_with("<!DOCTYPE NETSCAPE-Bookmark-file-1>"));
    // Folders still open, each with its name; the root has none.
    let mut stack: Vec<(Option<String>, Vec<Node>)> = Vec::new();
    let mut pending: Option<String> = None;
    let mut rest = html;
    while let Some(start) = rest.find('<') {
        rest = &rest[start..];
        let upper = rest.get(..8).unwrap_or(rest).to_ascii_uppercase();
        if upper.starts_with("<H3") {
            let open = rest.find('>').unwrap() + 1;
            let close = rest.find("</H3>").unwrap();
            pending = Some(unescape(&rest[open..close]));
            rest = &rest[close + 5..];
        } else if upper.starts_with("<DL>") {
            stack.push((pending.take(), Vec::new()));
            rest = &rest[4..];
        } else if upper.starts_with("</DL>") {
            let (name, children) = stack.pop().expect("balanced lists");
            match (name, stack.last_mut()) {
                (Some(name), Some((_, parent))) => parent.push(Node::Folder(name, children)),
                (None, None) => return children,
                _ => panic!("a folder list without a name"),
            }
            rest = &rest[5..];
        } else if upper.starts_with("<A HREF=") {
            let url_start = "<A HREF=\"".len();
            let url_end = url_start + rest[url_start..].find('"').unwrap();
            let link = unescape(&rest[url_start..url_end]);
            stack
                .last_mut()
                .expect("inside a list")
                .1
                .push(Node::Link(link));
            rest = &rest[url_end..];
        } else {
            rest = &rest[1..];
        }
    }
    panic!("the root list never closed");
}

fn links(entries: &[Entry]) -> Vec<Node> {
    entries
        .iter()
        .filter(|entry| matches!(entry.scheme, Scheme::Http | Scheme::Https))
        .map(|entry| Node::Link(entry.url.clone()))
        .collect()
}

fn expected_group(group: &myarc_import::arc_markdown::Group) -> Option<Node> {
    let mut children = links(&group.entries);
    for child in &group.children {
        if let Some(node) = expected_group(child) {
            children.push(node);
        }
    }
    (!children.is_empty()).then(|| Node::Folder(group.label.clone(), children))
}

/// What the export should hold, built from Arc's report rather than from the
/// workspace: Favorites, then each Space's Pins and its folders, in Arc's
/// order, leaving out empty ones.
fn expected(report: &Report) -> Vec<Node> {
    let mut nodes = Vec::new();
    let favorites = links(&report.favorites);
    if !favorites.is_empty() {
        nodes.push(Node::Folder(FAVORITES_FOLDER.into(), favorites));
    }
    for space in &report.spaces {
        let mut children = links(&space.pins);
        for group in &space.groups {
            if let Some(node) = expected_group(group) {
                children.push(node);
            }
        }
        if !children.is_empty() {
            nodes.push(Node::Folder(space.label.clone(), children));
        }
    }
    nodes
}

fn imported() -> (Report, Workspace) {
    let report = parse(&std::fs::read(FIXTURE_PATH).unwrap()).expect("fixture parses");
    let mut workspace = Workspace::default();
    apply(&report, &as_written(&report), &mut workspace).expect("import succeeds");
    (report, workspace)
}

#[test]
fn arc_sidebar_import_exports_with_the_same_folders_order_and_urls() {
    let (report, workspace) = imported();
    let read = read_bookmarks(&write(&workspace.snapshot()));
    assert_eq!(read, expected(&report));
    // The fixture has Favorites, more than one Space and a sidebar folder, so
    // each part of the layout is exercised.
    assert!(matches!(&read[0], Node::Folder(name, _) if name == FAVORITES_FOLDER));
    assert!(read.len() >= 3);
    assert!(read.iter().any(|node| matches!(
        node,
        Node::Folder(_, children) if children.iter().any(|child| matches!(child, Node::Folder(..)))
    )));
}

#[test]
fn several_profiles_read_back_under_their_names() {
    let (report, workspace) = imported();
    let snapshot = workspace.snapshot();
    let read = read_bookmarks(&write_profiles(&[("Work", &snapshot), ("Home", &snapshot)]));
    assert_eq!(
        read,
        [
            Node::Folder("Work".into(), expected(&report)),
            Node::Folder("Home".into(), expected(&report)),
        ]
    );
}
