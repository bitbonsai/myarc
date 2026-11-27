#![forbid(unsafe_code)]

//! Dependency-free, bounded RSS 2.0, Atom 1.0, and JSON Feed 1.1 parser and
//! HTML viewer renderer for myArc.
//!
//! All network bytes are treated as untrusted: response size, XML depth, node
//! count, item count, and text lengths are bounded, links are restricted to
//! `http://` and `https://`, and embedded HTML is sanitized to an allowlist.

use std::fmt;

pub mod date;
pub mod discover;
pub mod json_feed;
pub mod opml;
pub mod render;
pub mod rss_atom;
pub mod sanitize;
pub mod store;
pub mod url;
pub mod xml;

pub use discover::{DiscoveredFeed, discover_feeds_in_html};
pub use opml::{OpmlEntry, export_opml, parse_opml};
pub use render::{RenderOptions, render_viewer_html};
pub use store::{FeedStore, StoreError, Subscription, SubscriptionId};

pub const MAX_FEED_BYTES: usize = 4 * 1024 * 1024;
pub const MAX_XML_DEPTH: usize = 64;
pub const MAX_XML_NODES: usize = 65_536;
pub const MAX_ITEMS: usize = 500;
pub const MAX_TITLE_BYTES: usize = 512;
pub const MAX_URL_BYTES: usize = 4096;
pub const MAX_SUMMARY_BYTES: usize = 16 * 1024;
pub const MAX_CONTENT_BYTES: usize = 128 * 1024;
pub const MAX_AUTHORS: usize = 16;
pub const MAX_CATEGORIES: usize = 16;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FeedKind {
    Rss,
    Atom,
    JsonFeed,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Feed {
    pub kind: FeedKind,
    pub title: String,
    pub subtitle: Option<String>,
    pub site_url: Option<String>,
    pub feed_url: Option<String>,
    pub icon_url: Option<String>,
    pub updated: Option<String>,
    pub items: Vec<FeedItem>,
    pub truncated: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FeedItem {
    pub id: String,
    pub title: String,
    pub link: Option<String>,
    pub published: Option<String>,
    pub published_display: Option<String>,
    pub author: Option<String>,
    pub categories: Vec<String>,
    pub summary_text: Option<String>,
    pub content_html: Option<String>,
}

#[derive(Debug, PartialEq, Eq)]
pub enum FeedError {
    EmptyInput,
    TooLarge { limit: usize },
    InvalidUtf8,
    UnsupportedFormat,
    MalformedXml(&'static str),
    MalformedJson(&'static str),
    NestingTooDeep,
    TooManyNodes,
}

impl fmt::Display for FeedError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::EmptyInput => f.write_str("Feed input is empty"),
            Self::TooLarge { limit } => write!(f, "Feed exceeds maximum size of {limit} bytes"),
            Self::InvalidUtf8 => f.write_str("Feed is not valid UTF-8"),
            Self::UnsupportedFormat => {
                f.write_str("Document is not a supported RSS, Atom, or JSON Feed")
            }
            Self::MalformedXml(msg) => write!(f, "Malformed XML feed: {msg}"),
            Self::MalformedJson(msg) => write!(f, "Malformed JSON feed: {msg}"),
            Self::NestingTooDeep => f.write_str("Feed nesting exceeds maximum depth"),
            Self::TooManyNodes => f.write_str("Feed exceeds maximum XML node limit"),
        }
    }
}

impl std::error::Error for FeedError {}

/// Checks whether a response `Content-Type` header and initial body prefix look
/// like an RSS, Atom, or JSON Feed suitable for the built-in viewer.
pub fn looks_like_feed(content_type: &str, body_prefix: &[u8]) -> Option<FeedKind> {
    let mime = content_type
        .split(';')
        .next()
        .unwrap_or("")
        .trim()
        .to_ascii_lowercase();

    match mime.as_str() {
        "application/rss+xml" | "application/rdf+xml" | "text/rss+xml" => {
            return Some(FeedKind::Rss);
        }
        "application/atom+xml" | "text/atom+xml" => return Some(FeedKind::Atom),
        "application/feed+json" => return Some(FeedKind::JsonFeed),
        _ => {}
    }

    let prefix = std::str::from_utf8(body_prefix).ok()?;
    let trimmed = prefix
        .strip_prefix('\u{feff}')
        .unwrap_or(prefix)
        .trim_start();

    let xml_mime = matches!(
        mime.as_str(),
        "application/xml" | "text/xml" | "" | "text/plain"
    );
    if xml_mime && trimmed.starts_with('<') {
        if let Some(root_tag) = xml::sniff_root_tag(trimmed) {
            let local = root_tag
                .rsplit_once(':')
                .map_or(root_tag.as_str(), |(_, l)| l);
            match local {
                "rss" | "rdf" => return Some(FeedKind::Rss),
                "feed" => return Some(FeedKind::Atom),
                _ => {}
            }
        }
    }

    let json_mime = matches!(mime.as_str(), "application/json" | "text/json" | "");
    if json_mime && trimmed.starts_with('{') && trimmed.contains("https://jsonfeed.org/version/") {
        return Some(FeedKind::JsonFeed);
    }

    None
}

/// Parses an RSS 2.0, RSS 1.0 (RDF), Atom 1.0, or JSON Feed 1.0/1.1 payload.
pub fn parse_feed(input: &[u8], request_url: Option<&str>) -> Result<Feed, FeedError> {
    if input.is_empty() {
        return Err(FeedError::EmptyInput);
    }
    if input.len() > MAX_FEED_BYTES {
        return Err(FeedError::TooLarge {
            limit: MAX_FEED_BYTES,
        });
    }
    let text = std::str::from_utf8(input).map_err(|_| FeedError::InvalidUtf8)?;
    let trimmed = text.strip_prefix('\u{feff}').unwrap_or(text).trim_start();
    if trimmed.is_empty() {
        return Err(FeedError::EmptyInput);
    }
    if trimmed.starts_with('{') {
        json_feed::parse_json_feed(trimmed, request_url)
    } else if trimmed.starts_with('<') {
        rss_atom::parse_xml_feed(trimmed, request_url)
    } else {
        Err(FeedError::UnsupportedFormat)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SAMPLE_RSS: &str = r#"<?xml version="1.0" encoding="UTF-8"?><rss version="2.0"><channel><title>Synthetic Blog</title><description>Design &amp; engineering notes.</description><link>https://example.test/</link><item><title>A thousand tabs</title><link>https://example.test/blog/tabs/</link><guid>https://example.test/blog/tabs/</guid><pubDate>Mon, 23 Nov 2026 00:00:00 GMT</pubDate><description>Testing didn&#39;t break Arc&#39;s sidebar.</description></item><item><title>Second post</title><link>/blog/second/</link><pubDate>Sun, 22 Nov 2026 00:00:00 GMT</pubDate><description>&lt;p&gt;Hello &lt;script&gt;alert(1)&lt;/script&gt;&lt;b&gt;world&lt;/b&gt;.&lt;/p&gt;</description></item></channel></rss>"#;

    const SAMPLE_ATOM: &str = r#"<?xml version="1.0" encoding="utf-8"?>
    <feed xmlns="http://www.w3.org/2005/Atom">
      <title>Synthetic Atom</title>
      <subtitle>Updates from the lab</subtitle>
      <link href="https://atom.example.test/" rel="alternate"/>
      <link href="https://atom.example.test/feed.xml" rel="self"/>
      <updated>2026-11-26T12:00:00Z</updated>
      <entry>
        <id>tag:atom.example.test,2026:1</id>
        <title>First Entry</title>
        <link href="https://atom.example.test/posts/1" rel="alternate"/>
        <published>2026-11-25T09:30:00Z</published>
        <author><name>Ada</name></author>
        <category term="rust"/>
        <summary>Short summary here.</summary>
        <content type="html">&lt;p&gt;Full &lt;a href="https://atom.example.test/a"&gt;article&lt;/a&gt; body.&lt;/p&gt;</content>
      </entry>
    </feed>"#;

    const SAMPLE_JSON_FEED: &str = r#"{
      "version": "https://jsonfeed.org/version/1.1",
      "title": "Synthetic JSON Feed",
      "home_page_url": "https://json.example.test/",
      "feed_url": "https://json.example.test/feed.json",
      "description": "Fast JSON updates.",
      "items": [
        {
          "id": "post-42",
          "url": "https://json.example.test/42",
          "title": "JSON Post",
          "summary": "Plain summary",
          "content_html": "<p>Safe <em>markup</em><script>bad()</script></p>",
          "date_published": "2026-11-24T08:00:00Z",
          "authors": [{ "name": "Grace" }],
          "tags": ["feeds", "browser"]
        }
      ]
    }"#;

    #[test]
    fn detects_feeds_by_mime_and_sniffing() {
        assert_eq!(
            looks_like_feed("application/rss+xml; charset=utf-8", b""),
            Some(FeedKind::Rss)
        );
        assert_eq!(
            looks_like_feed("application/xml", SAMPLE_RSS.as_bytes()),
            Some(FeedKind::Rss)
        );
        assert_eq!(
            looks_like_feed("text/xml", SAMPLE_ATOM.as_bytes()),
            Some(FeedKind::Atom)
        );
        assert_eq!(
            looks_like_feed("application/json", SAMPLE_JSON_FEED.as_bytes()),
            Some(FeedKind::JsonFeed)
        );
        assert_eq!(
            looks_like_feed("text/html", b"<!doctype html><html><body>Hi</body></html>"),
            None
        );
    }

    #[test]
    fn parses_rss2_and_decodes_double_entities() {
        let feed = parse_feed(SAMPLE_RSS.as_bytes(), Some("https://example.test/rss")).unwrap();
        assert_eq!(feed.kind, FeedKind::Rss);
        assert_eq!(feed.title, "Synthetic Blog");
        assert_eq!(
            feed.subtitle.as_deref(),
            Some("Design & engineering notes.")
        );
        assert_eq!(feed.items.len(), 2);
        assert_eq!(
            feed.items[0].published_display.as_deref(),
            Some("23 Nov 2026")
        );
        assert_eq!(
            feed.items[0].summary_text.as_deref(),
            Some("Testing didn't break Arc's sidebar.")
        );
        assert_eq!(
            feed.items[1].link.as_deref(),
            Some("https://example.test/blog/second/")
        );
        assert_eq!(feed.items[1].summary_text.as_deref(), Some("Hello world."));
        let html = feed.items[1].content_html.as_deref().unwrap();
        assert!(!html.contains("<script>"));
        assert!(html.contains("<b>world</b>"));
    }

    #[test]
    fn parses_atom_feed() {
        let feed = parse_feed(SAMPLE_ATOM.as_bytes(), None).unwrap();
        assert_eq!(feed.kind, FeedKind::Atom);
        assert_eq!(feed.title, "Synthetic Atom");
        assert_eq!(
            feed.feed_url.as_deref(),
            Some("https://atom.example.test/feed.xml")
        );
        assert_eq!(feed.items.len(), 1);
        assert_eq!(feed.items[0].author.as_deref(), Some("Ada"));
        assert_eq!(feed.items[0].categories, vec!["rust"]);
        assert_eq!(
            feed.items[0].published_display.as_deref(),
            Some("25 Nov 2026")
        );
    }

    #[test]
    fn parses_json_feed() {
        let feed = parse_feed(SAMPLE_JSON_FEED.as_bytes(), None).unwrap();
        assert_eq!(feed.kind, FeedKind::JsonFeed);
        assert_eq!(feed.title, "Synthetic JSON Feed");
        assert_eq!(feed.items.len(), 1);
        assert_eq!(feed.items[0].author.as_deref(), Some("Grace"));
        assert_eq!(feed.items[0].categories, vec!["feeds", "browser"]);
        let html = feed.items[0].content_html.as_deref().unwrap();
        assert!(!html.contains("script"));
        assert!(html.contains("<em>markup</em>"));
    }

    #[test]
    fn rejects_oversized_and_empty_inputs() {
        assert_eq!(parse_feed(b"", None).unwrap_err(), FeedError::EmptyInput);
        let big = vec![b' '; MAX_FEED_BYTES + 1];
        assert_eq!(
            parse_feed(&big, None).unwrap_err(),
            FeedError::TooLarge {
                limit: MAX_FEED_BYTES
            }
        );
    }

    #[test]
    fn renders_safe_viewer_html() {
        let feed = parse_feed(SAMPLE_RSS.as_bytes(), Some("https://example.test/rss")).unwrap();
        let html = render_viewer_html(&feed, SAMPLE_RSS, &RenderOptions::default());
        assert!(html.starts_with("<!doctype html>"));
        assert!(html.contains("Synthetic Blog"));
        assert!(html.contains("A thousand tabs"));
        assert!(html.contains("23 Nov 2026"));
        assert!(html.contains("Raw source"));
        assert!(!html.contains("<script>alert(1)</script>"));
    }
}
