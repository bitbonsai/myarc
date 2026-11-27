//! Bounded HTML `<head>` feed discovery (`<link rel="alternate" ...>`).

use crate::sanitize::decode_entities;
use crate::url::safe_http_url;
use crate::{FeedKind, MAX_TITLE_BYTES};

pub const MAX_DISCOVER_BYTES: usize = 128 * 1024;
pub const MAX_DISCOVERED_FEEDS: usize = 8;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DiscoveredFeed {
    pub url: String,
    pub title: Option<String>,
    pub kind: FeedKind,
}

/// Scans an HTML document prefix for `<link rel="alternate">` RSS, Atom, or
/// JSON Feed declarations. Stops at `</head>` or `MAX_DISCOVER_BYTES`.
pub fn discover_feeds_in_html(html: &str, page_url: &str) -> Vec<DiscoveredFeed> {
    let limit = html.len().min(MAX_DISCOVER_BYTES);
    let mut end = limit;
    while end > 0 && !html.is_char_boundary(end) {
        end -= 1;
    }
    let slice = &html[..end];
    let bytes = slice.as_bytes();
    let mut i = 0;
    let mut found: Vec<DiscoveredFeed> = Vec::new();

    while i < bytes.len() && found.len() < MAX_DISCOVERED_FEEDS {
        let Some(lt_rel) = slice[i..].find('<') else {
            break;
        };
        i += lt_rel;
        if slice[i..].starts_with("<!--") {
            match slice[i + 4..].find("-->") {
                Some(close) => {
                    i += 4 + close + 3;
                    continue;
                }
                None => break,
            }
        }
        let Some(gt_rel) = slice[i..].find('>') else {
            break;
        };
        let tag = &slice[i + 1..i + gt_rel];
        i += gt_rel + 1;

        let trimmed = tag.trim_start();
        if let Some(stripped) = trimmed.strip_prefix('/') {
            let close_name: String = stripped
                .chars()
                .take_while(|c| c.is_ascii_alphabetic())
                .map(|c| c.to_ascii_lowercase())
                .collect();
            if close_name == "head" {
                break;
            }
            continue;
        }

        let name: String = trimmed
            .chars()
            .take_while(|c| c.is_ascii_alphabetic())
            .map(|c| c.to_ascii_lowercase())
            .collect();
        if name == "body" {
            break;
        }
        if name != "link" {
            continue;
        }

        let Some(rel) = find_attr(trimmed, "rel") else {
            continue;
        };
        if !rel
            .split_whitespace()
            .any(|tok| tok.eq_ignore_ascii_case("alternate"))
        {
            continue;
        }
        let Some(ty) = find_attr(trimmed, "type") else {
            continue;
        };
        let kind = match ty.trim().to_ascii_lowercase().as_str() {
            "application/rss+xml" | "application/rdf+xml" | "text/rss+xml" => FeedKind::Rss,
            "application/atom+xml" | "text/atom+xml" => FeedKind::Atom,
            "application/feed+json" | "application/json" => FeedKind::JsonFeed,
            _ => continue,
        };
        let Some(raw_href) = find_attr(trimmed, "href") else {
            continue;
        };
        let decoded_href = decode_entities(&raw_href);
        let Some(url) = safe_http_url(&decoded_href, Some(page_url)) else {
            continue;
        };
        if found.iter().any(|f| f.url.eq_ignore_ascii_case(&url)) {
            continue;
        }
        let title = find_attr(trimmed, "title")
            .map(|t| {
                decode_entities(&t)
                    .trim()
                    .chars()
                    .take(MAX_TITLE_BYTES)
                    .collect::<String>()
            })
            .filter(|t| !t.is_empty());

        found.push(DiscoveredFeed { url, title, kind });
    }
    found
}

fn find_attr(tag: &str, wanted: &str) -> Option<String> {
    let bytes = tag.as_bytes();
    let mut i = 0;
    while i < bytes.len() && !bytes[i].is_ascii_whitespace() {
        i += 1;
    }
    while i < bytes.len() {
        while i < bytes.len() && (bytes[i].is_ascii_whitespace() || bytes[i] == b'/') {
            i += 1;
        }
        let name_start = i;
        while i < bytes.len()
            && !bytes[i].is_ascii_whitespace()
            && bytes[i] != b'='
            && bytes[i] != b'/'
        {
            i += 1;
        }
        let name = &tag[name_start..i];
        while i < bytes.len() && bytes[i].is_ascii_whitespace() {
            i += 1;
        }
        if i >= bytes.len() || bytes[i] != b'=' {
            continue;
        }
        i += 1;
        while i < bytes.len() && bytes[i].is_ascii_whitespace() {
            i += 1;
        }
        if i >= bytes.len() {
            break;
        }
        let val = if bytes[i] == b'"' || bytes[i] == b'\'' {
            let q = bytes[i];
            i += 1;
            let val_start = i;
            while i < bytes.len() && bytes[i] != q {
                i += 1;
            }
            let slice = &tag[val_start..i];
            if i < bytes.len() {
                i += 1;
            }
            slice
        } else {
            let val_start = i;
            while i < bytes.len() && !bytes[i].is_ascii_whitespace() && bytes[i] != b'/' {
                i += 1;
            }
            &tag[val_start..i]
        };
        if name.eq_ignore_ascii_case(wanted) {
            return Some(val.to_string());
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn discovers_rss_and_atom_links_in_head() {
        let html = r#"<!doctype html><html><head>
            <link rel="alternate" type="application/rss+xml" title="Main &amp; Blog" href="/rss.xml">
            <link rel="alternate" type="application/atom+xml" href="https://example.test/atom.xml">
            <link rel="stylesheet" href="/style.css">
        </head><body><link rel="alternate" type="application/rss+xml" href="/ignored.xml"></body></html>"#;
        let feeds = discover_feeds_in_html(html, "https://example.test/blog/post");
        assert_eq!(feeds.len(), 2);
        assert_eq!(feeds[0].url, "https://example.test/rss.xml");
        assert_eq!(feeds[0].title.as_deref(), Some("Main & Blog"));
        assert_eq!(feeds[0].kind, FeedKind::Rss);
        assert_eq!(feeds[1].url, "https://example.test/atom.xml");
        assert_eq!(feeds[1].kind, FeedKind::Atom);
    }
}
