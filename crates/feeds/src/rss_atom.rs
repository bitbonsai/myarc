//! RSS 2.0, RSS 1.0 (RDF), and Atom 1.0 feed extraction.

use crate::date::format_display_date;
use crate::sanitize::{decode_entities, html_to_plain_text, sanitize_html};
use crate::url::safe_http_url;
use crate::xml::{XmlElement, parse_xml};
use crate::{
    Feed, FeedError, FeedItem, FeedKind, MAX_CATEGORIES, MAX_CONTENT_BYTES, MAX_ITEMS,
    MAX_SUMMARY_BYTES, MAX_TITLE_BYTES,
};

pub fn parse_xml_feed(input: &str, request_url: Option<&str>) -> Result<Feed, FeedError> {
    let root = parse_xml(input)?;
    let root_local = root.local_name().to_ascii_lowercase();
    match root_local.as_str() {
        "rss" => parse_rss2(&root, request_url),
        "rdf" => parse_rss1(&root, request_url),
        "feed" => parse_atom(&root, request_url),
        _ => Err(FeedError::UnsupportedFormat),
    }
}

fn parse_rss2(root: &XmlElement, request_url: Option<&str>) -> Result<Feed, FeedError> {
    let channel = root
        .child("channel")
        .ok_or(FeedError::MalformedXml("RSS feed missing <channel>"))?;

    let raw_site_url = channel
        .children
        .iter()
        .find(|c| c.name.eq_ignore_ascii_case("link") && !c.text.trim().is_empty())
        .map(|c| c.text.trim());
    let site_url = raw_site_url.and_then(|u| safe_http_url(u, request_url));
    let base_url = site_url.as_deref().or(request_url);

    let self_url = channel
        .children
        .iter()
        .find(|c| {
            c.local_name().eq_ignore_ascii_case("link")
                && c.attr("rel")
                    .is_some_and(|r| r.eq_ignore_ascii_case("self"))
        })
        .and_then(|c| c.attr("href"))
        .and_then(|u| safe_http_url(u, base_url))
        .or_else(|| request_url.and_then(|u| safe_http_url(u, None)));

    let title = channel
        .child_text("title")
        .map(clean_title)
        .filter(|t| !t.is_empty())
        .unwrap_or_else(|| fallback_title(base_url));

    let subtitle = channel
        .child_text("description")
        .map(|d| html_to_plain_text(d, MAX_SUMMARY_BYTES))
        .filter(|d| !d.is_empty());

    let icon_url = channel
        .child("image")
        .and_then(|img| img.child_text("url"))
        .and_then(|u| safe_http_url(u, base_url));

    let updated = channel
        .child_text("lastBuildDate")
        .or_else(|| channel.child_text("pubDate"))
        .and_then(format_display_date);

    let mut items = Vec::new();
    let mut truncated = false;

    for child in &channel.children {
        if !child.matches_name("item") {
            continue;
        }
        if items.len() >= MAX_ITEMS {
            truncated = true;
            break;
        }
        items.push(parse_rss_item(child, base_url, items.len()));
    }

    Ok(Feed {
        kind: FeedKind::Rss,
        title,
        subtitle,
        site_url,
        feed_url: self_url,
        icon_url,
        updated,
        items,
        truncated,
    })
}

fn parse_rss1(root: &XmlElement, request_url: Option<&str>) -> Result<Feed, FeedError> {
    let channel = root
        .child("channel")
        .ok_or(FeedError::MalformedXml("RDF feed missing <channel>"))?;

    let site_url = channel
        .child_text("link")
        .and_then(|u| safe_http_url(u, request_url));
    let base_url = site_url.as_deref().or(request_url);
    let feed_url = request_url.and_then(|u| safe_http_url(u, None));

    let title = channel
        .child_text("title")
        .map(clean_title)
        .filter(|t| !t.is_empty())
        .unwrap_or_else(|| fallback_title(base_url));

    let subtitle = channel
        .child_text("description")
        .map(|d| html_to_plain_text(d, MAX_SUMMARY_BYTES))
        .filter(|d| !d.is_empty());

    let mut items = Vec::new();
    let mut truncated = false;
    for child in &root.children {
        if !child.matches_name("item") {
            continue;
        }
        if items.len() >= MAX_ITEMS {
            truncated = true;
            break;
        }
        items.push(parse_rss_item(child, base_url, items.len()));
    }

    Ok(Feed {
        kind: FeedKind::Rss,
        title,
        subtitle,
        site_url,
        feed_url,
        icon_url: None,
        updated: None,
        items,
        truncated,
    })
}

fn parse_rss_item(item: &XmlElement, base_url: Option<&str>, index: usize) -> FeedItem {
    let title = item
        .child_text("title")
        .map(clean_title)
        .filter(|t| !t.is_empty())
        .unwrap_or_else(|| "(Untitled)".to_string());

    let link = item
        .child_text("link")
        .and_then(|u| safe_http_url(u, base_url))
        .or_else(|| {
            item.child("guid")
                .filter(|g| {
                    !g.attr("isPermaLink")
                        .is_some_and(|v| v.eq_ignore_ascii_case("false"))
                })
                .and_then(|g| safe_http_url(g.text.trim(), base_url))
        });

    let id = item
        .child_text("guid")
        .map(|s| s.to_string())
        .or_else(|| link.clone())
        .unwrap_or_else(|| format!("rss-item-{index}"));

    let published = item
        .child_text("pubDate")
        .or_else(|| item.child_text("dc:date"))
        .or_else(|| item.child_text("date"))
        .map(|s| s.to_string());
    let published_display = published.as_deref().and_then(format_display_date);

    let author = item
        .child_text("dc:creator")
        .or_else(|| item.child_text("author"))
        .map(clean_title)
        .filter(|a| !a.is_empty());

    let categories: Vec<String> = item
        .children
        .iter()
        .filter(|c| c.matches_name("category"))
        .filter_map(|c| {
            let t = clean_title(c.text.trim());
            if t.is_empty() { None } else { Some(t) }
        })
        .take(MAX_CATEGORIES)
        .collect();

    let raw_desc = item.child_text("description");
    let raw_content = item.child_text("content:encoded");

    let summary_source = raw_desc.or(raw_content);
    let summary_text = summary_source
        .map(|s| html_to_plain_text(s, MAX_SUMMARY_BYTES))
        .filter(|s| !s.is_empty());

    let content_html = raw_content
        .or_else(|| raw_desc.filter(|d| looks_like_markup(d)))
        .map(|html| sanitize_html(html, base_url, MAX_CONTENT_BYTES))
        .filter(|h| !h.is_empty());

    FeedItem {
        id,
        title,
        link,
        published,
        published_display,
        author,
        categories,
        summary_text,
        content_html,
    }
}

fn parse_atom(root: &XmlElement, request_url: Option<&str>) -> Result<Feed, FeedError> {
    let site_url = find_atom_link(root, "alternate", request_url)
        .or_else(|| find_atom_link(root, "", request_url));
    let base_url = site_url.as_deref().or(request_url);

    let feed_url = find_atom_link(root, "self", base_url)
        .or_else(|| request_url.and_then(|u| safe_http_url(u, None)));

    let title = root
        .child_text("title")
        .map(clean_title)
        .filter(|t| !t.is_empty())
        .unwrap_or_else(|| fallback_title(base_url));

    let subtitle = root
        .child_text("subtitle")
        .map(|s| html_to_plain_text(s, MAX_SUMMARY_BYTES))
        .filter(|s| !s.is_empty());

    let icon_url = root
        .child_text("icon")
        .or_else(|| root.child_text("logo"))
        .and_then(|u| safe_http_url(u, base_url));

    let updated = root.child_text("updated").and_then(format_display_date);

    let mut items = Vec::new();
    let mut truncated = false;

    for child in &root.children {
        if !child.matches_name("entry") {
            continue;
        }
        if items.len() >= MAX_ITEMS {
            truncated = true;
            break;
        }
        items.push(parse_atom_entry(child, base_url, items.len()));
    }

    Ok(Feed {
        kind: FeedKind::Atom,
        title,
        subtitle,
        site_url,
        feed_url,
        icon_url,
        updated,
        items,
        truncated,
    })
}

fn parse_atom_entry(entry: &XmlElement, base_url: Option<&str>, index: usize) -> FeedItem {
    let title = entry
        .child_text("title")
        .map(clean_title)
        .filter(|t| !t.is_empty())
        .unwrap_or_else(|| "(Untitled)".to_string());

    let link = find_atom_link(entry, "alternate", base_url)
        .or_else(|| find_atom_link(entry, "", base_url));

    let id = entry
        .child_text("id")
        .map(|s| s.to_string())
        .or_else(|| link.clone())
        .unwrap_or_else(|| format!("atom-entry-{index}"));

    let published = entry
        .child_text("published")
        .or_else(|| entry.child_text("updated"))
        .map(|s| s.to_string());
    let published_display = published.as_deref().and_then(format_display_date);

    let author = entry
        .child("author")
        .and_then(|a| {
            a.child_text("name").or_else(|| {
                let t = a.text.trim();
                if t.is_empty() { None } else { Some(t) }
            })
        })
        .map(clean_title)
        .filter(|a| !a.is_empty());

    let categories: Vec<String> = entry
        .children
        .iter()
        .filter(|c| c.matches_name("category"))
        .filter_map(|c| {
            let raw = c
                .attr("label")
                .or_else(|| c.attr("term"))
                .unwrap_or(c.text.trim());
            let t = clean_title(raw);
            if t.is_empty() { None } else { Some(t) }
        })
        .take(MAX_CATEGORIES)
        .collect();

    let summary_el = entry.child("summary");
    let content_el = entry.child("content");

    let summary_source = summary_el
        .map(|e| e.text.trim())
        .filter(|s| !s.is_empty())
        .or_else(|| content_el.map(|e| e.text.trim()).filter(|s| !s.is_empty()));

    let summary_text = summary_source
        .map(|s| html_to_plain_text(s, MAX_SUMMARY_BYTES))
        .filter(|s| !s.is_empty());

    let content_html = content_el
        .and_then(|e| {
            let text = e.text.trim();
            if text.is_empty() {
                return None;
            }
            let ty = e.attr("type").unwrap_or("text");
            if ty.eq_ignore_ascii_case("html")
                || ty.eq_ignore_ascii_case("xhtml")
                || looks_like_markup(text)
            {
                Some(sanitize_html(text, base_url, MAX_CONTENT_BYTES))
            } else {
                None
            }
        })
        .filter(|h| !h.is_empty());

    FeedItem {
        id,
        title,
        link,
        published,
        published_display,
        author,
        categories,
        summary_text,
        content_html,
    }
}

fn find_atom_link(el: &XmlElement, target_rel: &str, base_url: Option<&str>) -> Option<String> {
    for child in &el.children {
        if !child.matches_name("link") {
            continue;
        }
        let rel = child.attr("rel").unwrap_or("");
        let matches = if target_rel.is_empty() {
            rel.is_empty()
        } else {
            rel.eq_ignore_ascii_case(target_rel)
        };
        if matches && let Some(href) = child.attr("href").and_then(|u| safe_http_url(u, base_url)) {
            return Some(href);
        }
    }
    None
}

fn clean_title(raw: &str) -> String {
    let decoded = decode_entities(raw);
    html_to_plain_text(&decoded, MAX_TITLE_BYTES)
}

fn fallback_title(base_url: Option<&str>) -> String {
    base_url
        .and_then(crate::url::display_host)
        .unwrap_or_else(|| "Untitled Feed".to_string())
}

fn looks_like_markup(s: &str) -> bool {
    s.contains('<') && s.contains('>')
}
