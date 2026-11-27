//! OPML 1.0/2.0 import and export for portable feed subscriptions.

use crate::sanitize::escape_html;
use crate::store::{MAX_SUBSCRIPTIONS, Subscription};
use crate::url::safe_http_url;
use crate::xml::{XmlElement, parse_xml};
use crate::{FeedError, MAX_FEED_BYTES, MAX_TITLE_BYTES};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct OpmlEntry {
    pub feed_url: String,
    pub title: String,
    pub site_url: Option<String>,
    pub folder: Option<String>,
}

/// Parses an OPML document into a bounded list of validated HTTP(S) feeds.
pub fn parse_opml(input: &[u8]) -> Result<Vec<OpmlEntry>, FeedError> {
    if input.is_empty() {
        return Err(FeedError::EmptyInput);
    }
    if input.len() > MAX_FEED_BYTES {
        return Err(FeedError::TooLarge {
            limit: MAX_FEED_BYTES,
        });
    }
    let text = std::str::from_utf8(input).map_err(|_| FeedError::InvalidUtf8)?;
    let root = parse_xml(text)?;
    if !root.local_name().eq_ignore_ascii_case("opml") {
        return Err(FeedError::UnsupportedFormat);
    }
    let body = root
        .child("body")
        .ok_or(FeedError::MalformedXml("OPML missing <body>"))?;

    let mut entries = Vec::new();
    collect_outlines(body, None, &mut entries);
    Ok(entries)
}

fn collect_outlines(parent: &XmlElement, folder: Option<&str>, out: &mut Vec<OpmlEntry>) {
    for child in &parent.children {
        if out.len() >= MAX_SUBSCRIPTIONS {
            return;
        }
        if !child.matches_name("outline") {
            continue;
        }
        let label = child
            .attr("title")
            .or_else(|| child.attr("text"))
            .map(|s| s.trim())
            .filter(|s| !s.is_empty());

        if let Some(raw_feed) = child.attr("xmlUrl") {
            if let Some(feed_url) = safe_http_url(raw_feed, None) {
                let title = label
                    .map(|t| truncate_str(t, MAX_TITLE_BYTES))
                    .unwrap_or_else(|| {
                        crate::url::display_host(&feed_url).unwrap_or_else(|| "Feed".to_string())
                    });
                let site_url = child
                    .attr("htmlUrl")
                    .and_then(|u| safe_http_url(u, Some(&feed_url)));
                if !out
                    .iter()
                    .any(|e| e.feed_url.eq_ignore_ascii_case(&feed_url))
                {
                    out.push(OpmlEntry {
                        feed_url,
                        title,
                        site_url,
                        folder: folder.map(|f| truncate_str(f, MAX_TITLE_BYTES)),
                    });
                }
            }
        } else if !child.children.is_empty() {
            collect_outlines(child, label.or(folder), out);
        }
    }
}

/// Exports subscriptions as a valid OPML 2.0 XML document.
pub fn export_opml(subscriptions: &[Subscription]) -> String {
    let mut out = String::with_capacity(256 + subscriptions.len() * 160);
    out.push_str("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    out.push_str("<opml version=\"2.0\">\n<head><title>myArc Feeds</title></head>\n<body>\n");
    for sub in subscriptions {
        out.push_str("  <outline type=\"rss\" text=\"");
        out.push_str(&escape_html(&sub.title));
        out.push_str("\" title=\"");
        out.push_str(&escape_html(&sub.title));
        out.push_str("\" xmlUrl=\"");
        out.push_str(&escape_html(&sub.feed_url));
        if let Some(site) = &sub.site_url {
            out.push_str("\" htmlUrl=\"");
            out.push_str(&escape_html(site));
        }
        out.push_str("\"/>\n");
    }
    out.push_str("</body>\n</opml>\n");
    out
}

fn truncate_str(s: &str, max_bytes: usize) -> String {
    if s.len() <= max_bytes {
        return s.to_string();
    }
    let mut end = max_bytes;
    while end > 0 && !s.is_char_boundary(end) {
        end -= 1;
    }
    s[..end].to_string()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::store::FeedStore;

    #[test]
    fn opml_import_and_export_roundtrip() {
        let sample = br#"<?xml version="1.0" encoding="UTF-8"?>
        <opml version="2.0">
          <head><title>My Feeds</title></head>
          <body>
            <outline text="Design">
              <outline type="rss" text="Blog One" xmlUrl="https://one.example.test/rss.xml" htmlUrl="https://one.example.test/"/>
              <outline type="rss" text="Bad Scheme" xmlUrl="javascript:alert(1)"/>
            </outline>
          </body>
        </opml>"#;
        let entries = parse_opml(sample).unwrap();
        assert_eq!(entries.len(), 1);
        assert_eq!(entries[0].feed_url, "https://one.example.test/rss.xml");
        assert_eq!(entries[0].folder.as_deref(), Some("Design"));

        let mut store = FeedStore::new();
        store
            .subscribe(
                &entries[0].feed_url,
                &entries[0].title,
                entries[0].site_url.as_deref(),
                1,
                100,
            )
            .unwrap();
        let exported = export_opml(store.subscriptions());
        let reparsed = parse_opml(exported.as_bytes()).unwrap();
        assert_eq!(reparsed.len(), 1);
        assert_eq!(reparsed[0].feed_url, "https://one.example.test/rss.xml");
    }
}
