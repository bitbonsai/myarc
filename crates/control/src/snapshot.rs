//! `page.snapshot`: turns a page's accessibility tree into a compact outline
//! an agent can read and act on with few tokens.
//!
//! The browser collects the tree with Chromium's own accessibility snapshot
//! and passes it here as a flat list of [`AxNode`] values. [`outline`] walks
//! it and writes one line per meaningful node:
//!
//! ```text
//! heading "Agent page one" [level=1]
//! textbox "Email" [ref=e3] value=""
//! button "Send" [ref=e4]
//! link "Docs" [ref=e5] -> /docs
//! ```
//!
//! Interactive nodes get a reference such as `e4`, numbered in output order.
//! Generic containers fold away and adjacent text joins into one line. Page
//! text is untrusted: every name, value and text sits inside double quotes
//! with quotes, backslashes and control characters escaped, so a page can't
//! end a quote, start a line or forge a reference. Output is bounded in
//! lines, bytes, name length and depth, and says what it cut.
//!
//! [`SnapshotStore`] keeps the references of the latest snapshot of each
//! document, so later commands can resolve `e4` against the revision they
//! name and refuse an old one.

use std::collections::{HashMap, HashSet, VecDeque};

use serde_json::{Value, json};

use crate::{Outcome, Request, Response, TabState, answer, tab_handle, truncate};
use myarc_workspace::MAX_TITLE_BYTES;

/// How long the browser waits for a snapshot before answering `timeout`.
/// Shorter than the host's 10 second reply timeout.
pub const SNAPSHOT_TIMEOUT_MS: u64 = 5_000;
/// Most accessibility nodes the browser asks each frame for.
pub const MAX_SNAPSHOT_NODES: usize = 5_000;
/// Most nodes [`outline`] reads; the rest are ignored and reported.
pub const MAX_INPUT_NODES: usize = 50_000;
/// Most lines in an outline.
pub const MAX_LINES: usize = 2_000;
/// Most bytes in an outline, 64 KiB.
pub const MAX_TEXT_BYTES: usize = 64 * 1024;
/// Longest name, value or text run, in characters, before it is cut.
pub const MAX_NAME_CHARS: usize = 200;
/// Longest link target shown, in characters.
pub const MAX_URL_CHARS: usize = 200;
/// Deepest indentation level; deeper nodes print at this level.
pub const MAX_INDENT: usize = 24;
/// Deepest tree level walked; deeper subtrees are left out and reported.
pub const MAX_TREE_DEPTH: usize = 256;
/// Documents whose latest references [`SnapshotStore`] keeps.
pub const MAX_STORED_DOCUMENTS: usize = 64;

/// Check state of a node.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Checked {
    #[default]
    No,
    Yes,
    Mixed,
}

/// One node of the accessibility tree as the browser collected it. `role` is
/// Chromium's role name, such as `textField` or `staticText`.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct AxNode {
    pub id: i32,
    pub role: String,
    pub name: String,
    pub value: String,
    pub url: String,
    pub children: Vec<i32>,
    /// Heading level, or 0.
    pub level: u32,
    pub focusable: bool,
    pub clickable: bool,
    pub invisible: bool,
    pub ignored: bool,
    pub checked: Checked,
    /// `Some(true)` expanded, `Some(false)` collapsed, `None` neither.
    pub expanded: Option<bool>,
    pub disabled: bool,
    pub selected: bool,
    pub required: bool,
    /// A password field; its value is never shown.
    pub protected: bool,
    /// Text can be typed into it, as into a text field or an editable root.
    pub editable: bool,
    pub read_only: bool,
}

/// Bounds for one outline. [`Limits::default`] uses the constants above.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Limits {
    pub max_lines: usize,
    pub max_bytes: usize,
    pub max_name_chars: usize,
    pub max_indent: usize,
    pub max_depth: usize,
}

impl Default for Limits {
    fn default() -> Self {
        Self {
            max_lines: MAX_LINES,
            max_bytes: MAX_TEXT_BYTES,
            max_name_chars: MAX_NAME_CHARS,
            max_indent: MAX_INDENT,
            max_depth: MAX_TREE_DEPTH,
        }
    }
}

/// An outline and the node behind each reference: `refs[0]` is the node of
/// `e1`, and `targets[0]` what the interaction commands need to know about
/// it. `cut` says what was left out or shortened, empty when nothing was.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Outline {
    pub text: String,
    pub refs: Vec<i32>,
    pub targets: Vec<RefTarget>,
    pub lines: usize,
    pub cut: Vec<String>,
}

/// A referenced node as a snapshot showed it. `node` is its accessibility
/// ID, which is the page's DOM node ID when positive. `head` is its line up
/// to the reference and `tail` the rest, so lines compare across snapshots
/// whatever their reference numbers.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct RefTarget {
    pub node: i32,
    /// Chromium's role name, such as `textField`.
    pub role: String,
    pub head: String,
    pub tail: String,
    pub editable: bool,
    pub read_only: bool,
    pub disabled: bool,
    pub focused: bool,
}

impl RefTarget {
    pub(crate) fn bare(node: i32) -> Self {
        Self {
            node,
            ..Self::default()
        }
    }

    /// The line as printed with reference `number`.
    pub fn line(&self, number: usize) -> String {
        format!("{} [ref=e{number}]{}", self.head, self.tail)
    }
}

impl Outline {
    pub fn truncated(&self) -> bool {
        !self.cut.is_empty()
    }
}

/// How a role prints.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Kind {
    /// Left out with its subtree, such as inline text boxes and list markers.
    Skip,
    /// Folds away; text on either side may join.
    Inline,
    /// Folds away, but text does not join across it, such as a paragraph.
    Block,
    Text,
    Break,
    /// A label element: folds away, and its text is dropped when a control
    /// already carries it as its name.
    Label,
    /// Prints under this name. When the name comes from its content, text
    /// inside it that repeats the name is dropped.
    Node(&'static str),
    /// Prints under this name only when it has one, else folds as a block.
    Named(&'static str),
}

fn kind_of(role: &str) -> Kind {
    use Kind::*;
    match role {
        "staticText" => Text,
        "lineBreak" => Break,
        "inlineTextBox" | "listMarker" | "caret" | "scrollBar" | "titleBar" | "imeCandidate"
        | "menuItemSeparator" | "splitter" => Skip,
        "none" | "abbr" | "emphasis" | "strong" | "mark" | "code" | "subscript" | "superscript"
        | "time" | "ruby" | "rubyAnnotation" | "contentDeletion" | "contentInsertion" | "term"
        | "definition" | "keyboard" | "legend" | "menuListPopup" | "rowGroup"
        | "layoutTableRow" | "layoutTableCell" => Inline,
        "labelText" => Label,
        "heading" => Node("heading"),
        "link" | "docBackLink" | "docBiblioRef" | "docGlossref" | "docNoteRef" => Node("link"),
        "button"
        | "toggleButton"
        | "popUpButton"
        | "disclosureTriangle"
        | "disclosureTriangleGrouped"
        | "colorWell"
        | "pdfActionableHighlight" => Node("button"),
        "checkBox" => Node("checkbox"),
        "radioButton" => Node("radio"),
        "switch" => Node("switch"),
        "textField" | "date" | "dateTime" | "inputTime" => Node("textbox"),
        "searchBox" => Node("searchbox"),
        "comboBoxSelect" | "comboBoxMenuButton" | "textFieldWithComboBox" => Node("combobox"),
        "listBox" => Node("listbox"),
        "listBoxOption" | "menuListOption" => Node("option"),
        "slider" => Node("slider"),
        "spinButton" => Node("spinbutton"),
        "meter" => Node("meter"),
        "progressIndicator" => Node("progressbar"),
        "menu" => Node("menu"),
        "menuBar" => Node("menubar"),
        "menuItem" => Node("menuitem"),
        "menuItemCheckBox" => Node("menuitemcheckbox"),
        "menuItemRadio" => Node("menuitemradio"),
        "tab" => Node("tab"),
        "tabList" => Node("tablist"),
        "tabPanel" => Node("tabpanel"),
        "tree" | "treeGrid" => Node("tree"),
        "treeItem" => Node("treeitem"),
        "toolbar" => Node("toolbar"),
        "radioGroup" => Node("radiogroup"),
        "table" | "grid" | "listGrid" => Node("table"),
        "row" => Node("row"),
        "cell" | "gridCell" => Node("cell"),
        "columnHeader" => Node("columnheader"),
        "rowHeader" => Node("rowheader"),
        "main" => Node("main"),
        "navigation" => Node("navigation"),
        "banner" => Node("banner"),
        "contentInfo" => Node("contentinfo"),
        "complementary" => Node("complementary"),
        "search" => Node("search"),
        "form" => Node("form"),
        "dialog" => Node("dialog"),
        "alertDialog" => Node("alertdialog"),
        "alert" => Node("alert"),
        "status" => Node("status"),
        "iframe" | "iframePresentational" => Node("iframe"),
        "math" | "mathMLMath" => Node("math"),
        "image" | "svgRoot" | "graphicsDocument" | "graphicsObject" | "graphicsSymbol"
        | "canvas" => Named("img"),
        "region" => Named("region"),
        "group" | "figure" => Named("group"),
        _ => Block,
    }
}

/// Roles that always get a reference, even when the page made them
/// unfocusable, such as a disabled button.
fn is_control(name: &str) -> bool {
    matches!(
        name,
        "link"
            | "button"
            | "checkbox"
            | "radio"
            | "switch"
            | "textbox"
            | "searchbox"
            | "combobox"
            | "slider"
            | "spinbutton"
            | "option"
            | "menuitem"
            | "menuitemcheckbox"
            | "menuitemradio"
            | "tab"
            | "treeitem"
    )
}

/// Roles whose name usually comes from the text inside them, so that text
/// would repeat the name. Containers such as a dialog keep their text.
fn named_by_content(name: &str) -> bool {
    is_control(name)
        || matches!(
            name,
            "heading" | "cell" | "columnheader" | "rowheader" | "img" | "row" | "tooltip"
        )
}

/// Roles that show their value.
fn shows_value(name: &str) -> bool {
    matches!(
        name,
        "textbox" | "searchbox" | "combobox" | "slider" | "spinbutton"
    )
}

fn is_root(role: &str) -> bool {
    matches!(role, "rootWebArea" | "webView" | "pdfRoot")
}

/// The role as printed for a node that has no role of its own here, such as
/// a clickable `div`. Unexpected characters fall back to `generic`.
fn fallback_role(role: &str) -> String {
    if role == "genericContainer" || role.is_empty() || role.len() > 32 {
        return String::from("generic");
    }
    if !role.bytes().all(|byte| byte.is_ascii_alphabetic()) {
        return String::from("generic");
    }
    role.to_ascii_lowercase()
}

/// Joins whitespace runs into single spaces, trims both ends and keeps at
/// most `max` characters. The flag is set when it cut.
pub fn collapse(text: &str, max: usize) -> (String, bool) {
    let mut out = String::new();
    let mut count = 0;
    let mut space = false;
    for c in text.chars() {
        if c.is_whitespace() {
            space = !out.is_empty();
            continue;
        }
        let needed = if space { 2 } else { 1 };
        if count + needed > max {
            return (out, true);
        }
        if space {
            out.push(' ');
            count += 1;
            space = false;
        }
        out.push(c);
        count += 1;
    }
    (out, false)
}

/// Writes `text` inside double quotes. Quotes and backslashes get a
/// backslash, and control, line separator and bidirectional formatting
/// characters become `\n`, `\t` or `\u{…}`, so nothing a page writes can end
/// the quote or start a new line.
pub fn quote(text: &str, cut: bool) -> String {
    let mut out = String::with_capacity(text.len() + 2);
    out.push('"');
    for c in text.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if needs_escape(c) => out.push_str(&format!("\\u{{{:x}}}", c as u32)),
            c => out.push(c),
        }
    }
    if cut {
        out.push('…');
    }
    out.push('"');
    out
}

fn needs_escape(c: char) -> bool {
    c.is_control()
        || matches!(
            c,
            '\u{2028}'
                | '\u{2029}'
                | '\u{200e}'
                | '\u{200f}'
                | '\u{202a}'..='\u{202e}'
                | '\u{2066}'..='\u{2069}'
                | '\u{feff}'
        )
}

/// `scheme://host[:port]` of an absolute URL, or `None`.
fn origin_of(url: &str) -> Option<&str> {
    let start = url.find("://")? + 3;
    let end = url[start..]
        .find(['/', '?', '#'])
        .map_or(url.len(), |end| start + end);
    (end > start).then(|| &url[..end])
}

/// A link target as printed: a path when it shares the page's origin, bare
/// when it is plain visible ASCII, quoted otherwise. The flag is set when
/// it was cut.
fn link_target(url: &str, page_url: &str) -> (String, bool) {
    let mut shown = url;
    if let (Some(page), Some(link)) = (origin_of(page_url), origin_of(url))
        && page == link
    {
        shown = &url[link.len()..];
        if shown.is_empty() {
            shown = "/";
        }
    }
    let (shown, cut) = match shown.char_indices().nth(MAX_URL_CHARS) {
        Some((end, _)) => (&shown[..end], true),
        None => (shown, false),
    };
    let plain = !shown.is_empty()
        && shown
            .bytes()
            .all(|byte| byte.is_ascii_graphic() && byte != b'"' && byte != b'\\');
    if plain && !cut {
        (shown.to_owned(), false)
    } else {
        (quote(shown, cut), cut)
    }
}

struct Walk<'a> {
    nodes: &'a [AxNode],
    index: HashMap<i32, usize>,
    seen: Vec<bool>,
    focus: i32,
    page_url: &'a str,
    limits: Limits,
    /// Names of the controls, to drop label text that repeats one.
    control_names: HashSet<String>,
    text: String,
    lines: usize,
    refs: Vec<i32>,
    targets: Vec<RefTarget>,
    /// Text waiting to print, with its depth, so neighbors can join it.
    pending: Option<(usize, String, bool)>,
    full: bool,
    hidden: usize,
    names_cut: usize,
    too_deep: usize,
}

impl Walk<'_> {
    fn node(&self, id: i32) -> Option<usize> {
        self.index.get(&id).copied()
    }

    fn emit(&mut self, depth: usize, line: &str) -> bool {
        if self.full {
            self.hidden += 1;
            return false;
        }
        let indent = depth.min(self.limits.max_indent) * 2;
        if self.lines + 1 > self.limits.max_lines
            || self.text.len() + indent + line.len() + 1 > self.limits.max_bytes
        {
            self.full = true;
            self.hidden += 1;
            return false;
        }
        self.text.extend(std::iter::repeat_n(' ', indent));
        self.text.push_str(line);
        self.text.push('\n');
        self.lines += 1;
        true
    }

    fn flush(&mut self) {
        if let Some((depth, text, cut)) = self.pending.take() {
            if cut {
                self.names_cut += 1;
            }
            let line = format!("text {}", quote(&text, cut));
            self.emit(depth, &line);
        }
    }

    fn add_text(&mut self, depth: usize, text: &str) {
        if let Some((pending_depth, pending, cut)) = &mut self.pending
            && *pending_depth == depth
        {
            if *cut {
                return;
            }
            let (joined, now_cut) =
                collapse(&format!("{pending} {text}"), self.limits.max_name_chars);
            *pending = joined;
            *cut = now_cut;
            return;
        }
        self.flush();
        let (text, cut) = collapse(text, self.limits.max_name_chars);
        self.pending = Some((depth, text, cut));
    }

    /// Text of a subtree, joined and collapsed, up to the name limit.
    fn text_of(&self, index: usize, out: &mut String, budget: &mut usize) {
        if *budget == 0 {
            return;
        }
        *budget -= 1;
        let node = &self.nodes[index];
        if node.role == "staticText" {
            out.push(' ');
            out.push_str(&node.name);
            return;
        }
        for child in &node.children {
            if out.len() > self.limits.max_name_chars * 4 {
                return;
            }
            if let Some(child) = self.node(*child) {
                self.text_of(child, out, budget);
            }
        }
    }

    fn visit(&mut self, index: usize, depth: usize, level: usize, repeats: &[String]) {
        if self.seen[index] {
            return;
        }
        self.seen[index] = true;
        if level > self.limits.max_depth {
            self.too_deep += 1;
            return;
        }
        let node = &self.nodes[index];
        let kind = kind_of(&node.role);
        if kind == Kind::Skip {
            return;
        }
        // Chromium marks the options of a closed native select invisible,
        // but an agent needs them to choose one.
        let invisible = node.invisible && node.role != "menuListOption";
        if node.ignored || invisible || is_root(&node.role) {
            self.children(index, depth, level, repeats);
            return;
        }
        let (name, name_cut) = collapse(&node.name, self.limits.max_name_chars);
        let printed = match kind {
            Kind::Text => {
                if !name.is_empty() && !repeats.iter().any(|seen| seen.contains(&name)) {
                    self.add_text(depth, &name);
                }
                return;
            }
            Kind::Break => {
                if let Some((pending_depth, _, _)) = &self.pending
                    && *pending_depth == depth
                {
                    self.flush();
                }
                return;
            }
            Kind::Node(printed) => Some(printed),
            Kind::Named(printed) if !name.is_empty() => Some(printed),
            _ => None,
        };
        // Options of a native select are chosen through the select itself,
        // and a label passes clicks to its control.
        let interactive = kind != Kind::Label
            && node.role != "menuListOption"
            && (printed.is_some_and(is_control) || node.focusable || node.clickable);
        if printed.is_none() && !interactive {
            if kind == Kind::Label {
                let mut text = String::new();
                let mut budget = 64;
                self.text_of(index, &mut text, &mut budget);
                let (text, _) = collapse(&text, self.limits.max_name_chars);
                if !text.is_empty() && self.control_names.contains(&text) {
                    let mut repeats = repeats.to_vec();
                    repeats.push(text);
                    self.children(index, depth, level, &repeats);
                    return;
                }
            }
            if kind != Kind::Inline {
                self.flush();
            }
            self.children(index, depth, level, repeats);
            if kind != Kind::Inline {
                self.flush();
            }
            return;
        }
        self.flush();
        let role = match printed {
            Some(printed) => printed.to_owned(),
            None => fallback_role(&node.role),
        };
        let mut head = role.clone();
        // A row's name repeats its cells.
        let (name, name_cut) = if role == "row" {
            (String::new(), false)
        } else {
            (name, name_cut)
        };
        if !name.is_empty() {
            head.push(' ');
            head.push_str(&quote(&name, name_cut));
        }
        let reference = interactive.then(|| self.refs.len() + 1);
        let mut tail = String::new();
        if role == "heading" && node.level > 0 {
            tail.push_str(&format!(" [level={}]", node.level.min(9)));
        }
        match node.checked {
            Checked::Yes => tail.push_str(" [checked]"),
            Checked::Mixed => tail.push_str(" [checked=mixed]"),
            Checked::No => {}
        }
        match node.expanded {
            Some(true) => tail.push_str(" [expanded]"),
            Some(false) => tail.push_str(" [collapsed]"),
            None => {}
        }
        for (on, state) in [
            (node.selected, " [selected]"),
            (node.required, " [required]"),
            (node.disabled, " [disabled]"),
            (node.id == self.focus, " [focused]"),
        ] {
            if on {
                tail.push_str(state);
            }
        }
        let mut value_cut = false;
        if shows_value(&role) && !node.protected {
            let (value, cut) = collapse(&node.value, self.limits.max_name_chars);
            if !value.is_empty() || role == "textbox" || role == "searchbox" {
                tail.push_str(" value=");
                tail.push_str(&quote(&value, cut));
                value_cut = cut;
            }
        }
        let mut url_cut = false;
        if role == "link" && !node.url.is_empty() {
            let (target, cut) = link_target(&node.url, self.page_url);
            tail.push_str(" -> ");
            tail.push_str(&target);
            url_cut = cut;
        }
        let line = match reference {
            Some(number) => format!("{head} [ref=e{number}]{tail}"),
            None => format!("{head}{tail}"),
        };
        if self.emit(depth, &line) {
            if let Some(number) = reference {
                debug_assert_eq!(number, self.refs.len() + 1);
                self.refs.push(node.id);
                self.targets.push(RefTarget {
                    node: node.id,
                    role: node.role.clone(),
                    head,
                    tail,
                    editable: node.editable,
                    read_only: node.read_only,
                    disabled: node.disabled,
                    focused: node.id == self.focus,
                });
            }
            self.names_cut += usize::from(name_cut) + usize::from(value_cut) + usize::from(url_cut);
        }
        let mut repeats = repeats.to_vec();
        if !name.is_empty() && named_by_content(&role) {
            repeats.push(name);
        }
        if shows_value(&role) && !node.value.is_empty() {
            repeats.push(collapse(&node.value, self.limits.max_name_chars).0);
        }
        self.children(index, depth + 1, level, &repeats);
        self.flush();
    }

    fn children(&mut self, index: usize, depth: usize, level: usize, repeats: &[String]) {
        let children = &self.nodes[index].children;
        for child in children {
            if let Some(child) = self.node(*child) {
                self.visit(child, depth, level + 1, repeats);
            }
        }
    }
}

/// Writes the outline of the tree under `root`. `focus` is the focused node,
/// and `page_url` turns same-origin link targets into paths. Never panics on
/// malformed input: unknown children, repeated IDs and cycles are skipped.
pub fn outline(
    nodes: &[AxNode],
    root: i32,
    focus: i32,
    page_url: &str,
    limits: &Limits,
) -> Outline {
    let ignored = nodes.len().saturating_sub(MAX_INPUT_NODES);
    let nodes = &nodes[..nodes.len().min(MAX_INPUT_NODES)];
    let mut index = HashMap::with_capacity(nodes.len());
    for (position, node) in nodes.iter().enumerate() {
        index.entry(node.id).or_insert(position);
    }
    let control_names = nodes
        .iter()
        .filter(|node| matches!(kind_of(&node.role), Kind::Node(role) if is_control(role)))
        .map(|node| collapse(&node.name, limits.max_name_chars).0)
        .filter(|name| !name.is_empty())
        .collect();
    let mut walk = Walk {
        nodes,
        index,
        seen: vec![false; nodes.len()],
        focus,
        page_url,
        limits: *limits,
        control_names,
        text: String::new(),
        lines: 0,
        refs: Vec::new(),
        targets: Vec::new(),
        pending: None,
        full: false,
        hidden: 0,
        names_cut: 0,
        too_deep: 0,
    };
    if let Some(root) = walk.node(root) {
        walk.visit(root, 0, 0, &[]);
        walk.flush();
    }
    let mut cut = Vec::new();
    if walk.full {
        cut.push(format!(
            "stopped at {} lines or {} KiB; {} more nodes not shown",
            limits.max_lines,
            limits.max_bytes / 1024,
            walk.hidden
        ));
    }
    if walk.names_cut > 0 {
        cut.push(format!(
            "{} names, values, texts or links cut to {} characters",
            walk.names_cut, limits.max_name_chars
        ));
    }
    if walk.too_deep > 0 {
        cut.push(format!(
            "{} subtrees deeper than {} levels left out",
            walk.too_deep, limits.max_depth
        ));
    }
    if ignored > 0 {
        cut.push(format!(
            "{ignored} nodes past the first {MAX_INPUT_NODES} ignored"
        ));
    }
    Outline {
        text: walk.text,
        refs: walk.refs,
        targets: walk.targets,
        lines: walk.lines,
        cut,
    }
}

/// The revision string of the `count`th snapshot of `document`.
fn revision(document: u64, count: u64) -> String {
    format!("{document}.{count}")
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct Stored {
    document: u64,
    count: u64,
    refs: Vec<RefTarget>,
}

/// The references of the latest snapshot of each recent document. The
/// browser numbers each main document it snapshots, so a navigation starts
/// a new entry and the old revision can't resolve on the new page. Keeps
/// [`MAX_STORED_DOCUMENTS`], dropping the oldest.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SnapshotStore {
    entries: VecDeque<Stored>,
}

/// The references an action took from the store, so its answer can compare
/// them with the next snapshot, or [`SnapshotStore::put_back`] can restore
/// them when the action never ran.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Taken {
    pub document: u64,
    count: u64,
    pub refs: Vec<RefTarget>,
}

impl SnapshotStore {
    pub const fn new() -> Self {
        Self {
            entries: VecDeque::new(),
        }
    }

    /// Keeps `refs` as the latest snapshot of `document` and returns its
    /// revision, which differs from every earlier one. The targets carry
    /// only the node IDs; see [`SnapshotStore::record_targets`].
    pub fn record(&mut self, document: u64, refs: Vec<i32>) -> String {
        self.record_targets(document, refs.into_iter().map(RefTarget::bare).collect())
    }

    /// Like [`SnapshotStore::record`], with what each reference showed.
    pub fn record_targets(&mut self, document: u64, refs: Vec<RefTarget>) -> String {
        let count = match self.entries.iter().position(|e| e.document == document) {
            Some(position) => self
                .entries
                .remove(position)
                .map_or(1, |old| old.count.saturating_add(1)),
            None => 1,
        };
        self.entries.push_back(Stored {
            document,
            count,
            refs,
        });
        while self.entries.len() > MAX_STORED_DOCUMENTS {
            self.entries.pop_front();
        }
        revision(document, count)
    }

    /// The latest snapshot of `document`, or `stale` when `revision_text`
    /// is not its revision.
    fn latest(&self, document: u64, revision_text: &str) -> Result<&Stored, Outcome> {
        let Some(entry) = self.entries.iter().find(|e| e.document == document) else {
            return Err(Outcome::Stale {
                reason: format!("The page changed since that snapshot; {AGAIN}"),
            });
        };
        if revision(entry.document, entry.count) != revision_text {
            return Err(Outcome::Stale {
                reason: format!(
                    "Revision {:?} is not the latest snapshot of this page; {AGAIN}",
                    crate::bounded(revision_text)
                ),
            });
        }
        Ok(entry)
    }

    /// The node behind `element`, such as `e4`, in the latest snapshot of
    /// `document`. `stale` when `revision` is not that snapshot's, for
    /// example after a navigation or a newer snapshot.
    pub fn resolve(
        &self,
        document: u64,
        revision_text: &str,
        element: &str,
    ) -> Result<i32, Outcome> {
        self.target(document, revision_text, element)
            .map(|target| target.node)
    }

    /// Like [`SnapshotStore::resolve`], with what the snapshot showed.
    pub fn target(
        &self,
        document: u64,
        revision_text: &str,
        element: &str,
    ) -> Result<&RefTarget, Outcome> {
        let entry = self.latest(document, revision_text)?;
        parse_ref(element)
            .and_then(|number| entry.refs.get(number - 1))
            .ok_or_else(|| Outcome::Invalid {
                reason: format!(
                    "Reference {:?} is not in snapshot {}; use a [ref=eN] from it",
                    crate::bounded(element),
                    crate::bounded(revision_text)
                ),
            })
    }

    /// Takes the references of `revision_text`, the latest snapshot of
    /// `document`, so that revision turns stale: once an action starts, the
    /// page may differ from what it showed.
    pub fn take(&mut self, document: u64, revision_text: &str) -> Result<Taken, Outcome> {
        self.latest(document, revision_text)?;
        let entry = self
            .entries
            .iter_mut()
            .find(|e| e.document == document)
            .ok_or_else(|| Outcome::Stale {
                reason: format!("The page changed since that snapshot; {AGAIN}"),
            })?;
        let taken = Taken {
            document,
            count: entry.count,
            refs: std::mem::take(&mut entry.refs),
        };
        entry.count = entry.count.saturating_add(1);
        Ok(taken)
    }

    /// Restores references taken by an action that never ran, unless a
    /// newer snapshot of the document came in since.
    pub fn put_back(&mut self, taken: Taken) {
        if let Some(entry) = self
            .entries
            .iter_mut()
            .find(|e| e.document == taken.document)
            && entry.count == taken.count.saturating_add(1)
            && entry.refs.is_empty()
        {
            entry.count = taken.count;
            entry.refs = taken.refs;
        }
    }
}

/// How stale snapshot reasons end, so clients can swap in their own wording.
pub const AGAIN: &str = "run page.snapshot again and use its references";

/// The number in a reference such as `e4`.
pub(crate) fn parse_ref(element: &str) -> Option<usize> {
    element
        .strip_prefix('e')
        .filter(|digits| {
            !digits.is_empty()
                && digits.len() <= 9
                && !digits.starts_with('0')
                && digits.bytes().all(|byte| byte.is_ascii_digit())
        })
        .and_then(|digits| digits.parse::<usize>().ok())
}

/// A page the browser snapshotted. `document` numbers its main document;
/// `source_truncated` says Chromium stopped at [`MAX_SNAPSHOT_NODES`].
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SnapshotPage {
    pub tab: TabState,
    pub document: u64,
    pub root: i32,
    pub focus: i32,
    pub source_truncated: bool,
    pub nodes: Vec<AxNode>,
}

/// How the browser finished a `page.snapshot`.
#[derive(Clone, Debug, PartialEq)]
pub enum SnapshotResult {
    Done(SnapshotPage),
    Failed(Outcome),
}

/// The answer to `page.snapshot`: `{tab, url, title, revision, loading,
/// truncated, text}`, plus `cut` naming what was left out when `truncated`.
/// Records the references in `store` under the new revision.
pub fn snapshot_response(
    req: &Request,
    result: SnapshotResult,
    store: &mut SnapshotStore,
) -> Response {
    let page = match result {
        SnapshotResult::Done(page) => page,
        SnapshotResult::Failed(outcome) => return answer(req, outcome),
    };
    let (data, _) = record_page(&page, store);
    answer(req, Outcome::Success { data })
}

/// Formats `page`, records its references in `store` and returns the
/// `page.snapshot` data with the new references.
pub(crate) fn record_page(
    page: &SnapshotPage,
    store: &mut SnapshotStore,
) -> (Value, Vec<RefTarget>) {
    let outline = outline(
        &page.nodes,
        page.root,
        page.focus,
        &page.tab.url,
        &Limits::default(),
    );
    let mut cut = outline.cut;
    if page.source_truncated {
        cut.insert(
            0,
            format!(
                "the page has more than {MAX_SNAPSHOT_NODES} accessibility nodes; Chromium stopped there"
            ),
        );
    }
    let targets = outline.targets;
    let revision = store.record_targets(page.document, targets.clone());
    let mut data = json!({
        "tab": tab_handle(page.tab.item),
        "url": truncate(&page.tab.url, crate::MAX_URL_LENGTH),
        "title": truncate(&page.tab.title, MAX_TITLE_BYTES),
        "revision": revision,
        "loading": page.tab.loading,
        "truncated": !cut.is_empty(),
        "text": outline.text,
    });
    if !cut.is_empty() {
        data["cut"] = json!(cut.join("; "));
    }
    (data, targets)
}
