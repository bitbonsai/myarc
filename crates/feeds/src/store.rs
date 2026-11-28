//! Profile-scoped, dependency-free RSS/Atom/JSON Feed subscription store
//! with bounded JSON snapshot persistence.

use std::fmt;

use crate::url::safe_http_url;
use crate::{Feed, MAX_TITLE_BYTES};

pub const SNAPSHOT_VERSION: u32 = 1;
pub const MAX_SUBSCRIPTIONS: usize = 256;
pub const MAX_SEEN_IDS_PER_FEED: usize = 500;
pub const MAX_READ_IDS_PER_FEED: usize = 500;
pub const MAX_STORE_BYTES: usize = 4 * 1024 * 1024;
pub const MAX_ID_BYTES: usize = 512;
pub const MAX_HEADER_BYTES: usize = 256;

pub type SubscriptionId = u64;

fn normalize_feed_url(raw: &str) -> Option<String> {
    let valid = safe_http_url(raw, None)?;
    let without_ref = valid.split('#').next().unwrap_or(&valid);
    Some(without_ref.to_string())
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Subscription {
    pub id: SubscriptionId,
    pub feed_url: String,
    pub site_url: Option<String>,
    pub title: String,
    pub space_id: u64,
    pub etag: Option<String>,
    pub last_modified: Option<String>,
    pub last_fetched_secs: u64,
    pub unread_count: u32,
    pub seen_ids: Vec<String>,
    pub read_ids: Vec<String>,
}

#[derive(Debug, PartialEq, Eq)]
pub enum StoreError {
    InvalidFeedUrl,
    LimitExceeded(&'static str),
    UnknownSubscription(SubscriptionId),
    UnsupportedVersion(u32),
    InvalidSnapshot(&'static str),
}

impl fmt::Display for StoreError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidFeedUrl => {
                f.write_str("Feed URL must be a valid http:// or https:// address")
            }
            Self::LimitExceeded(what) => write!(f, "Feed store limit exceeded: {what}"),
            Self::UnknownSubscription(id) => write!(f, "Subscription {id} does not exist"),
            Self::UnsupportedVersion(v) => {
                write!(f, "Unsupported feed store snapshot version {v}")
            }
            Self::InvalidSnapshot(msg) => write!(f, "Invalid feed store snapshot: {msg}"),
        }
    }
}

impl std::error::Error for StoreError {}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct FeedStore {
    next_id: SubscriptionId,
    subscriptions: Vec<Subscription>,
}

impl FeedStore {
    pub fn new() -> Self {
        Self {
            next_id: 1,
            subscriptions: Vec::new(),
        }
    }

    pub fn subscriptions(&self) -> &[Subscription] {
        &self.subscriptions
    }

    pub fn subscriptions_for_space(&self, space_id: u64) -> Vec<&Subscription> {
        self.subscriptions
            .iter()
            .filter(|s| s.space_id == space_id)
            .collect()
    }

    pub fn find_by_url(&self, feed_url: &str) -> Option<&Subscription> {
        self.position_by_url(feed_url)
            .map(|index| &self.subscriptions[index])
    }

    fn position_by_url(&self, feed_url: &str) -> Option<usize> {
        let normalized = normalize_feed_url(feed_url)?;
        self.subscriptions.iter().position(|s| {
            normalize_feed_url(&s.feed_url)
                .map(|u| u.eq_ignore_ascii_case(&normalized))
                .unwrap_or_else(|| s.feed_url.eq_ignore_ascii_case(&normalized))
        })
    }

    /// Unread items of the subscription at `feed_url`, or 0 when there is none.
    pub fn unread_count_by_url(&self, feed_url: &str) -> u32 {
        self.find_by_url(feed_url).map_or(0, |s| s.unread_count)
    }

    /// `record_fetch` for the subscription at `feed_url`, or None when there
    /// is none.
    pub fn record_fetch_by_url(
        &mut self,
        feed_url: &str,
        feed: &Feed,
        now_secs: u64,
    ) -> Option<u32> {
        let id = self.find_by_url(feed_url)?.id;
        self.record_fetch(id, feed, None, None, now_secs).ok()
    }

    /// Marks the subscription at `feed_url` read. Returns whether its unread
    /// count changed.
    pub fn mark_all_read_by_url(&mut self, feed_url: &str) -> bool {
        let Some(sub) = self.find_by_url(feed_url) else {
            return false;
        };
        if sub.unread_count == 0 {
            return false;
        }
        let id = sub.id;
        self.mark_all_read(id).is_ok()
    }

    pub fn is_subscribed(&self, feed_url: &str) -> bool {
        self.find_by_url(feed_url).is_some()
    }

    /// Subscribes to `feed_url` in `space_id`. Idempotent: if already
    /// subscribed, returns the existing subscription's ID without duplicating.
    pub fn subscribe(
        &mut self,
        feed_url: &str,
        title: &str,
        site_url: Option<&str>,
        space_id: u64,
        now_secs: u64,
    ) -> Result<SubscriptionId, StoreError> {
        let canonical = normalize_feed_url(feed_url).ok_or(StoreError::InvalidFeedUrl)?;
        if let Some(existing) = self.subscriptions.iter().find(|s| {
            normalize_feed_url(&s.feed_url)
                .map(|u| u.eq_ignore_ascii_case(&canonical))
                .unwrap_or_else(|| s.feed_url.eq_ignore_ascii_case(&canonical))
        }) {
            return Ok(existing.id);
        }
        if self.subscriptions.len() >= MAX_SUBSCRIPTIONS {
            return Err(StoreError::LimitExceeded("subscriptions"));
        }
        let id = self.next_id.max(1);
        self.next_id = id.checked_add(1).ok_or(StoreError::LimitExceeded("ids"))?;

        let clean_title = sanitize_short_text(title, MAX_TITLE_BYTES).unwrap_or_else(|| {
            crate::url::display_host(&canonical).unwrap_or_else(|| "Feed".to_string())
        });
        let clean_site = site_url.and_then(|u| safe_http_url(u, Some(&canonical)));

        self.subscriptions.push(Subscription {
            id,
            feed_url: canonical,
            site_url: clean_site,
            title: clean_title,
            space_id,
            etag: None,
            last_modified: None,
            last_fetched_secs: now_secs,
            unread_count: 0,
            seen_ids: Vec::new(),
            read_ids: Vec::new(),
        });
        Ok(id)
    }

    pub fn unsubscribe(&mut self, id: SubscriptionId) -> Result<(), StoreError> {
        let Some(pos) = self.subscriptions.iter().position(|s| s.id == id) else {
            return Err(StoreError::UnknownSubscription(id));
        };
        self.subscriptions.remove(pos);
        Ok(())
    }

    pub fn unsubscribe_by_url(&mut self, feed_url: &str) -> bool {
        let Some(canonical) = normalize_feed_url(feed_url) else {
            return false;
        };
        let before = self.subscriptions.len();
        self.subscriptions.retain(|s| {
            let sub_url = normalize_feed_url(&s.feed_url);
            !sub_url
                .as_deref()
                .unwrap_or(&s.feed_url)
                .eq_ignore_ascii_case(&canonical)
        });
        self.subscriptions.len() != before
    }

    /// Updates a subscription from a freshly fetched & parsed `Feed`,
    /// returning the updated unread count.
    pub fn record_fetch(
        &mut self,
        id: SubscriptionId,
        feed: &Feed,
        etag: Option<&str>,
        last_modified: Option<&str>,
        now_secs: u64,
    ) -> Result<u32, StoreError> {
        let Some(sub) = self.subscriptions.iter_mut().find(|s| s.id == id) else {
            return Err(StoreError::UnknownSubscription(id));
        };
        if let Some(t) = sanitize_short_text(&feed.title, MAX_TITLE_BYTES) {
            sub.title = t;
        }
        if let Some(site) = &feed.site_url {
            sub.site_url = Some(site.clone());
        }
        if let Some(e) = etag.and_then(|v| sanitize_short_text(v, MAX_HEADER_BYTES)) {
            sub.etag = Some(e);
        }
        if let Some(lm) = last_modified.and_then(|v| sanitize_short_text(v, MAX_HEADER_BYTES)) {
            sub.last_modified = Some(lm);
        }
        sub.last_fetched_secs = now_secs;

        let mut unread: u32 = 0;
        let mut new_seen = Vec::with_capacity(feed.items.len().min(MAX_SEEN_IDS_PER_FEED));
        for item in feed.items.iter().take(MAX_SEEN_IDS_PER_FEED) {
            let item_id = truncate_utf8(&item.id, MAX_ID_BYTES).to_string();
            if !sub.read_ids.iter().any(|r| r == &item_id) {
                unread = unread.saturating_add(1);
            }
            new_seen.push(item_id);
        }
        sub.seen_ids = new_seen;
        sub.unread_count = unread;
        Ok(unread)
    }

    /// Marks one item (by ID or URL) as read and decrements `unread_count`.
    pub fn mark_item_read(&mut self, feed_url: &str, item_id: &str) -> bool {
        let Some(index) = self.position_by_url(feed_url) else {
            return false;
        };
        let sub = &mut self.subscriptions[index];
        let id = truncate_utf8(item_id.trim(), MAX_ID_BYTES);
        if id.is_empty() || sub.read_ids.iter().any(|r| r == id) {
            return false;
        }
        if sub.read_ids.len() >= MAX_READ_IDS_PER_FEED {
            sub.read_ids.remove(0);
        }
        sub.read_ids.push(id.to_string());
        sub.unread_count = sub
            .seen_ids
            .iter()
            .filter(|s| !sub.read_ids.iter().any(|r| r == *s))
            .count() as u32;
        true
    }

    /// Marks every currently seen item in subscription `id` as read.
    pub fn mark_all_read(&mut self, id: SubscriptionId) -> Result<(), StoreError> {
        let Some(sub) = self.subscriptions.iter_mut().find(|s| s.id == id) else {
            return Err(StoreError::UnknownSubscription(id));
        };
        for seen in &sub.seen_ids {
            if !sub.read_ids.iter().any(|r| r == seen) {
                if sub.read_ids.len() >= MAX_READ_IDS_PER_FEED {
                    sub.read_ids.remove(0);
                }
                sub.read_ids.push(seen.clone());
            }
        }
        sub.unread_count = 0;
        Ok(())
    }

    pub fn encode_snapshot(&self) -> String {
        let mut out = String::with_capacity(256 + self.subscriptions.len() * 256);
        out.push_str("{\"version\":");
        out.push_str(&SNAPSHOT_VERSION.to_string());
        out.push_str(",\"next_id\":");
        out.push_str(&self.next_id.max(1).to_string());
        out.push_str(",\"subscriptions\":[");
        for (idx, sub) in self.subscriptions.iter().enumerate() {
            if idx > 0 {
                out.push(',');
            }
            out.push_str("{\"id\":");
            out.push_str(&sub.id.to_string());
            out.push_str(",\"feed_url\":");
            push_json_string(&mut out, &sub.feed_url);
            if let Some(site) = &sub.site_url {
                out.push_str(",\"site_url\":");
                push_json_string(&mut out, site);
            }
            out.push_str(",\"title\":");
            push_json_string(&mut out, &sub.title);
            out.push_str(",\"space_id\":");
            out.push_str(&sub.space_id.to_string());
            if let Some(etag) = &sub.etag {
                out.push_str(",\"etag\":");
                push_json_string(&mut out, etag);
            }
            if let Some(lm) = &sub.last_modified {
                out.push_str(",\"last_modified\":");
                push_json_string(&mut out, lm);
            }
            out.push_str(",\"last_fetched_secs\":");
            out.push_str(&sub.last_fetched_secs.to_string());
            out.push_str(",\"unread_count\":");
            out.push_str(&sub.unread_count.to_string());
            out.push_str(",\"seen_ids\":[");
            for (i, s) in sub.seen_ids.iter().enumerate() {
                if i > 0 {
                    out.push(',');
                }
                push_json_string(&mut out, s);
            }
            out.push_str("],\"read_ids\":[");
            for (i, r) in sub.read_ids.iter().enumerate() {
                if i > 0 {
                    out.push(',');
                }
                push_json_string(&mut out, r);
            }
            out.push_str("]}");
        }
        out.push_str("]}");
        out
    }

    pub fn decode_snapshot(input: &[u8]) -> Result<Self, StoreError> {
        if input.len() > MAX_STORE_BYTES {
            return Err(StoreError::LimitExceeded("snapshot bytes"));
        }
        let text =
            std::str::from_utf8(input).map_err(|_| StoreError::InvalidSnapshot("invalid UTF-8"))?;
        let mut p = SnapshotParser::new(text);
        let root = p.parse_object()?;

        let version = root
            .get_u64("version")
            .ok_or(StoreError::InvalidSnapshot("missing version"))? as u32;
        if version != SNAPSHOT_VERSION {
            return Err(StoreError::UnsupportedVersion(version));
        }

        let mut next_id = root.get_u64("next_id").unwrap_or(1).max(1);
        let mut subscriptions = Vec::new();
        if let Some(arr) = root.get_array("subscriptions") {
            for item in arr.iter().take(MAX_SUBSCRIPTIONS) {
                let Some(obj) = item.as_object() else {
                    continue;
                };
                let Some(raw_url) = obj.get_str("feed_url") else {
                    continue;
                };
                let Some(feed_url) = safe_http_url(raw_url, None) else {
                    continue;
                };
                let id = obj.get_u64("id").unwrap_or(next_id).max(1);
                if id >= next_id {
                    next_id = id.saturating_add(1);
                }
                let title = obj
                    .get_str("title")
                    .and_then(|t| sanitize_short_text(t, MAX_TITLE_BYTES))
                    .unwrap_or_else(|| {
                        crate::url::display_host(&feed_url).unwrap_or_else(|| "Feed".to_string())
                    });
                let site_url = obj
                    .get_str("site_url")
                    .and_then(|u| safe_http_url(u, Some(&feed_url)));
                let space_id = obj.get_u64("space_id").unwrap_or(0);
                let etag = obj
                    .get_str("etag")
                    .and_then(|v| sanitize_short_text(v, MAX_HEADER_BYTES));
                let last_modified = obj
                    .get_str("last_modified")
                    .and_then(|v| sanitize_short_text(v, MAX_HEADER_BYTES));
                let last_fetched_secs = obj.get_u64("last_fetched_secs").unwrap_or(0);
                let unread_count = obj.get_u64("unread_count").unwrap_or(0) as u32;
                let seen_ids = obj.get_string_list("seen_ids", MAX_SEEN_IDS_PER_FEED);
                let read_ids = obj.get_string_list("read_ids", MAX_READ_IDS_PER_FEED);

                if subscriptions
                    .iter()
                    .any(|s: &Subscription| s.feed_url.eq_ignore_ascii_case(&feed_url))
                {
                    continue;
                }
                subscriptions.push(Subscription {
                    id,
                    feed_url,
                    site_url,
                    title,
                    space_id,
                    etag,
                    last_modified,
                    last_fetched_secs,
                    unread_count,
                    seen_ids,
                    read_ids,
                });
            }
        }
        Ok(Self {
            next_id,
            subscriptions,
        })
    }
}

fn sanitize_short_text(raw: &str, max_bytes: usize) -> Option<String> {
    let cleaned: String = raw.trim().chars().filter(|c| !c.is_control()).collect();
    let truncated = truncate_utf8(&cleaned, max_bytes);
    if truncated.is_empty() {
        None
    } else {
        Some(truncated.to_string())
    }
}

fn truncate_utf8(s: &str, max_bytes: usize) -> &str {
    if s.len() <= max_bytes {
        return s;
    }
    let mut end = max_bytes;
    while end > 0 && !s.is_char_boundary(end) {
        end -= 1;
    }
    &s[..end]
}

fn push_json_string(out: &mut String, s: &str) {
    out.push('"');
    for ch in s.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => {
                use std::fmt::Write;
                let _ = write!(out, "\\u{:04x}", c as u32);
            }
            c => out.push(c),
        }
    }
    out.push('"');
}

// Minimal bounded JSON parser for FeedStore snapshots.
enum JVal {
    Null,
    Bool,
    U64(u64),
    Str(String),
    Arr(Vec<JVal>),
    Obj(JObj),
}

struct JObj(Vec<(String, JVal)>);

impl JVal {
    fn as_object(&self) -> Option<&JObj> {
        match self {
            Self::Obj(o) => Some(o),
            _ => None,
        }
    }
}

impl JObj {
    fn get(&self, key: &str) -> Option<&JVal> {
        self.0.iter().find(|(k, _)| k == key).map(|(_, v)| v)
    }
    fn get_u64(&self, key: &str) -> Option<u64> {
        match self.get(key)? {
            JVal::U64(n) => Some(*n),
            _ => None,
        }
    }
    fn get_str(&self, key: &str) -> Option<&str> {
        match self.get(key)? {
            JVal::Str(s) => Some(s.as_str()),
            _ => None,
        }
    }
    fn get_array(&self, key: &str) -> Option<&[JVal]> {
        match self.get(key)? {
            JVal::Arr(a) => Some(a.as_slice()),
            _ => None,
        }
    }
    fn get_string_list(&self, key: &str, cap: usize) -> Vec<String> {
        self.get_array(key)
            .map(|arr| {
                arr.iter()
                    .take(cap)
                    .filter_map(|v| match v {
                        JVal::Str(s) => Some(truncate_utf8(s, MAX_ID_BYTES).to_string()),
                        _ => None,
                    })
                    .collect()
            })
            .unwrap_or_default()
    }
}

struct SnapshotParser<'a> {
    s: &'a str,
    b: &'a [u8],
    pos: usize,
}

impl<'a> SnapshotParser<'a> {
    fn new(s: &'a str) -> Self {
        Self {
            s,
            b: s.as_bytes(),
            pos: 0,
        }
    }
    fn ws(&mut self) {
        while self.pos < self.b.len() && self.b[self.pos].is_ascii_whitespace() {
            self.pos += 1;
        }
    }
    fn parse_object(&mut self) -> Result<JObj, StoreError> {
        self.ws();
        match self.parse_val(0)? {
            JVal::Obj(o) => Ok(o),
            _ => Err(StoreError::InvalidSnapshot("expected root object")),
        }
    }
    fn parse_val(&mut self, depth: usize) -> Result<JVal, StoreError> {
        if depth > 10 {
            return Err(StoreError::InvalidSnapshot("nesting too deep"));
        }
        self.ws();
        let ch = *self
            .b
            .get(self.pos)
            .ok_or(StoreError::InvalidSnapshot("unexpected end of JSON"))?;
        match ch {
            b'{' => {
                self.pos += 1;
                let mut pairs = Vec::new();
                self.ws();
                if self.b.get(self.pos) == Some(&b'}') {
                    self.pos += 1;
                    return Ok(JVal::Obj(JObj(pairs)));
                }
                loop {
                    self.ws();
                    let k = self.parse_str()?;
                    self.ws();
                    if self.b.get(self.pos) != Some(&b':') {
                        return Err(StoreError::InvalidSnapshot("expected ':'"));
                    }
                    self.pos += 1;
                    let v = self.parse_val(depth + 1)?;
                    pairs.push((k, v));
                    self.ws();
                    match self.b.get(self.pos) {
                        Some(&b',') => self.pos += 1,
                        Some(&b'}') => {
                            self.pos += 1;
                            return Ok(JVal::Obj(JObj(pairs)));
                        }
                        _ => return Err(StoreError::InvalidSnapshot("expected ',' or '}'")),
                    }
                }
            }
            b'[' => {
                self.pos += 1;
                let mut items = Vec::new();
                self.ws();
                if self.b.get(self.pos) == Some(&b']') {
                    self.pos += 1;
                    return Ok(JVal::Arr(items));
                }
                loop {
                    items.push(self.parse_val(depth + 1)?);
                    self.ws();
                    match self.b.get(self.pos) {
                        Some(&b',') => self.pos += 1,
                        Some(&b']') => {
                            self.pos += 1;
                            return Ok(JVal::Arr(items));
                        }
                        _ => return Err(StoreError::InvalidSnapshot("expected ',' or ']'")),
                    }
                }
            }
            b'"' => Ok(JVal::Str(self.parse_str()?)),
            b'0'..=b'9' => {
                let start = self.pos;
                while self.pos < self.b.len() && self.b[self.pos].is_ascii_digit() {
                    self.pos += 1;
                }
                let n = self.s[start..self.pos]
                    .parse::<u64>()
                    .map_err(|_| StoreError::InvalidSnapshot("invalid integer"))?;
                Ok(JVal::U64(n))
            }
            b't' if self.s[self.pos..].starts_with("true") => {
                self.pos += 4;
                Ok(JVal::Bool)
            }
            b'f' if self.s[self.pos..].starts_with("false") => {
                self.pos += 5;
                Ok(JVal::Bool)
            }
            b'n' if self.s[self.pos..].starts_with("null") => {
                self.pos += 4;
                Ok(JVal::Null)
            }
            _ => Err(StoreError::InvalidSnapshot("unexpected token")),
        }
    }
    fn parse_str(&mut self) -> Result<String, StoreError> {
        if self.b.get(self.pos) != Some(&b'"') {
            return Err(StoreError::InvalidSnapshot("expected string"));
        }
        self.pos += 1;
        let mut out = String::new();
        while self.pos < self.b.len() {
            let b = self.b[self.pos];
            if b == b'"' {
                self.pos += 1;
                return Ok(out);
            }
            if b == b'\\' {
                self.pos += 1;
                let esc = *self
                    .b
                    .get(self.pos)
                    .ok_or(StoreError::InvalidSnapshot("unterminated escape"))?;
                self.pos += 1;
                match esc {
                    b'"' => out.push('"'),
                    b'\\' => out.push('\\'),
                    b'/' => out.push('/'),
                    b'n' => out.push('\n'),
                    b'r' => out.push('\r'),
                    b't' => out.push('\t'),
                    b'u' if self.pos + 4 <= self.b.len() => {
                        let code = u16::from_str_radix(&self.s[self.pos..self.pos + 4], 16)
                            .map_err(|_| StoreError::InvalidSnapshot("bad \\u escape"))?;
                        self.pos += 4;
                        if let Some(ch) = char::from_u32(u32::from(code)) {
                            out.push(ch);
                        }
                    }
                    _ => return Err(StoreError::InvalidSnapshot("invalid escape")),
                }
            } else {
                let ch = self.s[self.pos..].chars().next().unwrap();
                out.push(ch);
                self.pos += ch.len_utf8();
            }
        }
        Err(StoreError::InvalidSnapshot("unterminated string"))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::parse_feed;

    #[test]
    fn subscribe_fetch_read_and_snapshot_roundtrip() {
        let mut store = FeedStore::new();
        let id = store
            .subscribe(
                "https://example.test/rss.xml",
                "Example Blog",
                Some("https://example.test/"),
                1,
                1000,
            )
            .unwrap();
        // Idempotent re-subscribe returns same ID.
        assert_eq!(
            store
                .subscribe("https://example.test/rss.xml", "Other", None, 1, 1001)
                .unwrap(),
            id
        );
        assert!(store.is_subscribed("https://example.test/rss.xml"));

        let rss = br#"<rss version="2.0"><channel><title>Example Blog</title><item><guid>p1</guid><title>One</title></item><item><guid>p2</guid><title>Two</title></item></channel></rss>"#;
        let parsed = parse_feed(rss, Some("https://example.test/rss.xml")).unwrap();
        let unread = store
            .record_fetch(id, &parsed, Some("\"v1\""), None, 2000)
            .unwrap();
        assert_eq!(unread, 2);

        assert!(store.mark_item_read("https://example.test/rss.xml", "p1"));
        assert_eq!(store.subscriptions()[0].unread_count, 1);

        let encoded = store.encode_snapshot();
        let decoded = FeedStore::decode_snapshot(encoded.as_bytes()).unwrap();
        assert_eq!(decoded, store);

        store.mark_all_read(id).unwrap();
        assert_eq!(store.subscriptions()[0].unread_count, 0);

        assert!(store.unsubscribe_by_url("https://example.test/rss.xml"));
        assert!(!store.is_subscribed("https://example.test/rss.xml"));
    }

    #[test]
    fn unread_by_url_ignores_fragment_and_marks_read() {
        let mut store = FeedStore::new();
        store
            .subscribe("https://example.test/rss.xml", "Blog", None, 1, 1000)
            .unwrap();
        let rss = br#"<rss version="2.0"><channel><title>Blog</title><item><guid>p1</guid><title>One</title></item><item><guid>p2</guid><title>Two</title></item></channel></rss>"#;
        let parsed = parse_feed(rss, Some("https://example.test/rss.xml")).unwrap();
        assert_eq!(
            store.record_fetch_by_url("https://example.test/rss.xml#top", &parsed, 2000),
            Some(2)
        );
        assert_eq!(store.unread_count_by_url("https://example.test/rss.xml"), 2);
        assert_eq!(store.unread_count_by_url("https://other.test/rss.xml"), 0);
        assert!(store.mark_item_read("https://example.test/rss.xml#top", "p2"));
        assert_eq!(store.unread_count_by_url("https://example.test/rss.xml"), 1);
        assert!(store.mark_all_read_by_url("https://example.test/rss.xml"));
        assert!(!store.mark_all_read_by_url("https://example.test/rss.xml"));
        assert_eq!(store.unread_count_by_url("https://example.test/rss.xml"), 0);
        assert_eq!(
            store.record_fetch_by_url("https://other.test/rss.xml", &parsed, 3000),
            None
        );
    }
}
