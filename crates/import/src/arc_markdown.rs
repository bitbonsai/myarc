//! Arc's built-in Markdown export (`Arc > Settings > Advanced > Export Data`).
//!
//! The reader accepts only the observed structure, documented in
//! `tests/fixtures/arc-markdown/README.md`:
//!
//! ```text
//! # Space label
//!
//! ## Group label
//! - https://example.test/page
//! ```
//!
//! A `# ` line starts a Space, a `## ` line starts a group inside the current Space,
//! and a `- ` line adds one URL to the current group. Blank lines separate groups.
//! Every other line becomes an [`Unrecognized`] record with its line number and a
//! reason, so nothing is dropped silently. Indented lines, deeper headings and other
//! list markers are reported rather than guessed at, because nesting has not been
//! observed in a real export yet.
//!
//! Labels and URLs are kept as written. `Favorites` is an ordinary group label: the
//! format does not say whether it means Profile favorites or Space pins. The export
//! carries no Profile identity and no titles, so the report has neither. URLs are
//! never fetched or canonicalized; the reader only checks that an entry looks like
//! `scheme:rest` and classifies the scheme.

use std::collections::BTreeSet;
use std::fmt;

/// Largest input accepted, in bytes. Larger files are rejected before parsing.
pub const MAX_INPUT_BYTES: usize = 8 * 1024 * 1024;
/// Longest Space or group label kept, in bytes.
pub const MAX_LABEL_BYTES: usize = 1024;
/// Longest URL kept, in bytes. Matches the workspace model's URL bound.
pub const MAX_URL_BYTES: usize = 8192;
/// Unrecognized lines keep at most this many bytes of their text.
pub const MAX_EXCERPT_BYTES: usize = 256;

const UTF8_BOM: &str = "\u{feff}";

/// Parsed export: Spaces in document order plus every line the reader skipped.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Report {
    pub spaces: Vec<Space>,
    pub unrecognized: Vec<Unrecognized>,
}

/// A first-level heading and the groups under it.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Space {
    pub label: String,
    /// 1-based line of the heading.
    pub line: usize,
    pub groups: Vec<Group>,
}

/// A second-level heading and its URLs in order. A group may be empty.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Group {
    pub label: String,
    /// 1-based line of the heading.
    pub line: usize,
    pub entries: Vec<Entry>,
}

/// One list item. Repeated URLs stay as separate entries.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Entry {
    /// The URL exactly as written.
    pub url: String,
    pub scheme: Scheme,
    /// 1-based line of the list item.
    pub line: usize,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Scheme {
    Http,
    Https,
    /// Any other syntactically valid scheme, lowercased (for example `file`).
    Other(String),
}

/// A line the reader did not turn into a Space, group or entry.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Unrecognized {
    /// 1-based line number.
    pub line: usize,
    pub reason: Reason,
    /// The line's text, cut to [`MAX_EXCERPT_BYTES`] at a character boundary.
    pub excerpt: String,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Reason {
    /// Starts with a space or tab. May be nesting, which is unobserved.
    Indented,
    /// `###` or deeper. May be a folder, which is unobserved.
    DeeperHeading,
    /// A heading with no label after the marker.
    EmptyLabel,
    /// A label longer than [`MAX_LABEL_BYTES`].
    LabelTooLong,
    /// A label containing control characters.
    ControlCharacter,
    /// A `## ` group before any Space.
    GroupOutsideSpace,
    /// A `- ` item before any group.
    EntryOutsideGroup,
    /// A `- ` item whose text is not a single `scheme:rest` token.
    NotUrl,
    /// A URL longer than [`MAX_URL_BYTES`].
    UrlTooLong,
    /// Any other text, including other Markdown syntax.
    Other,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ParseError {
    /// Input exceeds [`MAX_INPUT_BYTES`]. Nothing was parsed.
    TooLarge { len: usize, max: usize },
    /// Input is not UTF-8. `line` is the 1-based line of the first bad byte.
    NotUtf8 { line: usize },
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::TooLarge { len, max } => {
                write!(f, "export is {len} bytes; the limit is {max} bytes")
            }
            Self::NotUtf8 { line } => write!(f, "export is not UTF-8 text (line {line})"),
        }
    }
}

impl std::error::Error for ParseError {}

impl Report {
    pub fn group_count(&self) -> usize {
        self.spaces.iter().map(|space| space.groups.len()).sum()
    }

    pub fn entry_count(&self) -> usize {
        self.entries().count()
    }

    /// Number of distinct URL strings, compared byte for byte.
    pub fn distinct_url_count(&self) -> usize {
        self.entries()
            .map(|entry| entry.url.as_str())
            .collect::<BTreeSet<_>>()
            .len()
    }

    fn entries(&self) -> impl Iterator<Item = &Entry> {
        self.spaces
            .iter()
            .flat_map(|space| &space.groups)
            .flat_map(|group| &group.entries)
    }
}

/// Parses an export. Fails only on oversized or non-UTF-8 input; any other content
/// yields a report, with lines it could not place listed in `unrecognized`.
pub fn parse(input: &[u8]) -> Result<Report, ParseError> {
    if input.len() > MAX_INPUT_BYTES {
        return Err(ParseError::TooLarge {
            len: input.len(),
            max: MAX_INPUT_BYTES,
        });
    }
    let text = std::str::from_utf8(input).map_err(|error| ParseError::NotUtf8 {
        line: line_of(input, error.valid_up_to()),
    })?;
    let text = text.strip_prefix(UTF8_BOM).unwrap_or(text);

    let mut report = Report::default();
    for (index, raw) in text.split('\n').enumerate() {
        let line = index + 1;
        let content = raw.trim_end_matches([' ', '\t', '\r']);
        if content.is_empty() {
            continue;
        }
        if let Err(reason) = place(&mut report, content, line) {
            report.unrecognized.push(Unrecognized {
                line,
                reason,
                excerpt: excerpt(content),
            });
        }
    }
    Ok(report)
}

fn place(report: &mut Report, content: &str, line: usize) -> Result<(), Reason> {
    if content.starts_with([' ', '\t']) {
        return Err(Reason::Indented);
    }
    if content.starts_with("###") {
        return Err(Reason::DeeperHeading);
    }
    if let Some(rest) = heading(content, "##") {
        let label = label(rest)?;
        let space = report.spaces.last_mut().ok_or(Reason::GroupOutsideSpace)?;
        space.groups.push(Group {
            label,
            line,
            entries: Vec::new(),
        });
        return Ok(());
    }
    if let Some(rest) = heading(content, "#") {
        let label = label(rest)?;
        report.spaces.push(Space {
            label,
            line,
            groups: Vec::new(),
        });
        return Ok(());
    }
    if let Some(rest) = content.strip_prefix("- ") {
        let url = rest.trim_matches([' ', '\t']);
        let scheme = scheme(url)?;
        let group = report
            .spaces
            .last_mut()
            .and_then(|space| space.groups.last_mut())
            .ok_or(Reason::EntryOutsideGroup)?;
        group.entries.push(Entry {
            url: url.to_owned(),
            scheme,
            line,
        });
        return Ok(());
    }
    if content == "-" {
        return Err(Reason::NotUrl);
    }
    Err(Reason::Other)
}

/// Returns the text after `marker` when the line is exactly that heading level.
/// `#` alone (trailing whitespace already trimmed) is a heading with an empty label.
fn heading<'a>(content: &'a str, marker: &str) -> Option<&'a str> {
    let rest = content.strip_prefix(marker)?;
    if rest.is_empty() {
        return Some(rest);
    }
    rest.strip_prefix([' ', '\t'])
}

fn label(rest: &str) -> Result<String, Reason> {
    let label = rest.trim_matches([' ', '\t']);
    if label.is_empty() {
        return Err(Reason::EmptyLabel);
    }
    if label.len() > MAX_LABEL_BYTES {
        return Err(Reason::LabelTooLong);
    }
    if label.chars().any(char::is_control) {
        return Err(Reason::ControlCharacter);
    }
    Ok(label.to_owned())
}

/// Accepts one token shaped `scheme:rest` (RFC 3986 scheme syntax, non-empty rest,
/// no whitespace or control characters). Says nothing about host validity.
fn scheme(url: &str) -> Result<Scheme, Reason> {
    if url.len() > MAX_URL_BYTES {
        return Err(Reason::UrlTooLong);
    }
    if url.chars().any(|c| c.is_whitespace() || c.is_control()) {
        return Err(Reason::NotUrl);
    }
    let (scheme, rest) = url.split_once(':').ok_or(Reason::NotUrl)?;
    let mut chars = scheme.chars();
    let starts_alpha = chars.next().is_some_and(|c| c.is_ascii_alphabetic());
    let valid = chars.all(|c| c.is_ascii_alphanumeric() || matches!(c, '+' | '-' | '.'));
    if !starts_alpha || !valid || rest.is_empty() {
        return Err(Reason::NotUrl);
    }
    let scheme = scheme.to_ascii_lowercase();
    Ok(match scheme.as_str() {
        "http" => Scheme::Http,
        "https" => Scheme::Https,
        _ => Scheme::Other(scheme),
    })
}

fn line_of(input: &[u8], offset: usize) -> usize {
    let end = offset.min(input.len());
    input[..end].iter().filter(|&&byte| byte == b'\n').count() + 1
}

fn excerpt(content: &str) -> String {
    if content.len() <= MAX_EXCERPT_BYTES {
        return content.to_owned();
    }
    let mut end = MAX_EXCERPT_BYTES;
    while !content.is_char_boundary(end) {
        end -= 1;
    }
    content[..end].to_owned()
}
