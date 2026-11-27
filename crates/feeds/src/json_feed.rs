//! Bounded JSON Feed 1.0 / 1.1 parser with no external dependencies.

use crate::date::format_display_date;
use crate::sanitize::{html_to_plain_text, sanitize_html};
use crate::url::safe_http_url;
use crate::{
    Feed, FeedError, FeedItem, FeedKind, MAX_AUTHORS, MAX_CATEGORIES, MAX_CONTENT_BYTES, MAX_ITEMS,
    MAX_SUMMARY_BYTES, MAX_TITLE_BYTES,
};

const MAX_JSON_DEPTH: usize = 32;

#[derive(Debug)]
enum JsonValue {
    Null,
    Bool,
    Number,
    Str(String),
    Array(Vec<JsonValue>),
    Object(Vec<(String, JsonValue)>),
}

impl JsonValue {
    fn as_object(&self) -> Option<&[(String, JsonValue)]> {
        match self {
            Self::Object(pairs) => Some(pairs),
            _ => None,
        }
    }

    fn as_array(&self) -> Option<&[JsonValue]> {
        match self {
            Self::Array(items) => Some(items),
            _ => None,
        }
    }

    fn as_str(&self) -> Option<&str> {
        match self {
            Self::Str(s) => Some(s.as_str()),
            _ => None,
        }
    }

    fn get(&self, key: &str) -> Option<&JsonValue> {
        self.as_object()?
            .iter()
            .find(|(k, _)| k == key)
            .map(|(_, v)| v)
    }

    fn get_str(&self, key: &str) -> Option<&str> {
        self.get(key)?.as_str()
    }
}

pub fn parse_json_feed(input: &str, request_url: Option<&str>) -> Result<Feed, FeedError> {
    let s = input.strip_prefix('\u{feff}').unwrap_or(input);
    let mut parser = JsonParser::new(s);
    let root = parser.parse_value(0)?;

    let version = root
        .get_str("version")
        .ok_or(FeedError::UnsupportedFormat)?;
    if !version.starts_with("https://jsonfeed.org/version/") {
        return Err(FeedError::UnsupportedFormat);
    }

    let site_url = root
        .get_str("home_page_url")
        .and_then(|u| safe_http_url(u, request_url));
    let base_url = site_url.as_deref().or(request_url);
    let feed_url = root
        .get_str("feed_url")
        .and_then(|u| safe_http_url(u, base_url))
        .or_else(|| request_url.and_then(|u| safe_http_url(u, None)));

    let title = root
        .get_str("title")
        .map(|t| html_to_plain_text(t, MAX_TITLE_BYTES))
        .filter(|t| !t.is_empty())
        .unwrap_or_else(|| {
            base_url
                .and_then(crate::url::display_host)
                .unwrap_or_else(|| "Untitled Feed".to_string())
        });

    let subtitle = root
        .get_str("description")
        .map(|d| html_to_plain_text(d, MAX_SUMMARY_BYTES))
        .filter(|d| !d.is_empty());

    let icon_url = root
        .get_str("icon")
        .or_else(|| root.get_str("favicon"))
        .and_then(|u| safe_http_url(u, base_url));

    let mut items = Vec::new();
    let mut truncated = false;

    if let Some(raw_items) = root.get("items").and_then(JsonValue::as_array) {
        for (idx, item) in raw_items.iter().enumerate() {
            if items.len() >= MAX_ITEMS {
                truncated = true;
                break;
            }
            items.push(parse_json_item(item, base_url, idx));
        }
    }

    Ok(Feed {
        kind: FeedKind::JsonFeed,
        title,
        subtitle,
        site_url,
        feed_url,
        icon_url,
        updated: None,
        items,
        truncated,
    })
}

fn parse_json_item(item: &JsonValue, base_url: Option<&str>, index: usize) -> FeedItem {
    let title = item
        .get_str("title")
        .map(|t| html_to_plain_text(t, MAX_TITLE_BYTES))
        .filter(|t| !t.is_empty())
        .unwrap_or_else(|| "(Untitled)".to_string());

    let link = item
        .get_str("url")
        .or_else(|| item.get_str("external_url"))
        .and_then(|u| safe_http_url(u, base_url));

    let id = item
        .get_str("id")
        .map(|s| s.to_string())
        .or_else(|| link.clone())
        .unwrap_or_else(|| format!("json-item-{index}"));

    let published = item
        .get_str("date_published")
        .or_else(|| item.get_str("date_modified"))
        .map(|s| s.to_string());
    let published_display = published.as_deref().and_then(format_display_date);

    let author = item
        .get("authors")
        .and_then(JsonValue::as_array)
        .and_then(|arr| {
            let names: Vec<String> = arr
                .iter()
                .take(MAX_AUTHORS)
                .filter_map(|a| a.get_str("name"))
                .map(|n| html_to_plain_text(n, MAX_TITLE_BYTES))
                .filter(|n| !n.is_empty())
                .collect();
            if names.is_empty() {
                None
            } else {
                Some(names.join(", "))
            }
        })
        .or_else(|| {
            item.get("author")
                .and_then(|a| a.get_str("name"))
                .map(|n| html_to_plain_text(n, MAX_TITLE_BYTES))
                .filter(|n| !n.is_empty())
        });

    let categories: Vec<String> = item
        .get("tags")
        .and_then(JsonValue::as_array)
        .map(|tags| {
            tags.iter()
                .filter_map(JsonValue::as_str)
                .map(|t| html_to_plain_text(t, MAX_TITLE_BYTES))
                .filter(|t| !t.is_empty())
                .take(MAX_CATEGORIES)
                .collect()
        })
        .unwrap_or_default();

    let summary_text = item
        .get_str("summary")
        .or_else(|| item.get_str("content_text"))
        .or_else(|| item.get_str("content_html"))
        .map(|s| html_to_plain_text(s, MAX_SUMMARY_BYTES))
        .filter(|s| !s.is_empty());

    let content_html = item
        .get_str("content_html")
        .map(|h| sanitize_html(h, base_url, MAX_CONTENT_BYTES))
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

struct JsonParser<'a> {
    s: &'a str,
    bytes: &'a [u8],
    pos: usize,
}

impl<'a> JsonParser<'a> {
    fn new(s: &'a str) -> Self {
        Self {
            s,
            bytes: s.as_bytes(),
            pos: 0,
        }
    }

    fn skip_ws(&mut self) {
        while self.pos < self.bytes.len() && self.bytes[self.pos].is_ascii_whitespace() {
            self.pos += 1;
        }
    }

    fn parse_value(&mut self, depth: usize) -> Result<JsonValue, FeedError> {
        if depth > MAX_JSON_DEPTH {
            return Err(FeedError::NestingTooDeep);
        }
        self.skip_ws();
        let b = *self
            .bytes
            .get(self.pos)
            .ok_or(FeedError::MalformedJson("unexpected end of JSON"))?;
        match b {
            b'{' => self.parse_object(depth + 1),
            b'[' => self.parse_array(depth + 1),
            b'"' => Ok(JsonValue::Str(self.parse_string()?)),
            b't' => self.expect_literal("true", JsonValue::Bool),
            b'f' => self.expect_literal("false", JsonValue::Bool),
            b'n' => self.expect_literal("null", JsonValue::Null),
            b'-' | b'0'..=b'9' => self.parse_number(),
            _ => Err(FeedError::MalformedJson("unexpected token in JSON")),
        }
    }

    fn expect_literal(&mut self, lit: &str, val: JsonValue) -> Result<JsonValue, FeedError> {
        if self.s[self.pos..].starts_with(lit) {
            self.pos += lit.len();
            Ok(val)
        } else {
            Err(FeedError::MalformedJson("invalid JSON literal"))
        }
    }

    fn parse_number(&mut self) -> Result<JsonValue, FeedError> {
        let start = self.pos;
        if self.bytes.get(self.pos) == Some(&b'-') {
            self.pos += 1;
        }
        while self.pos < self.bytes.len()
            && matches!(
                self.bytes[self.pos],
                b'0'..=b'9' | b'.' | b'e' | b'E' | b'+' | b'-'
            )
        {
            self.pos += 1;
        }
        if self.pos == start {
            return Err(FeedError::MalformedJson("invalid JSON number"));
        }
        Ok(JsonValue::Number)
    }

    fn parse_string(&mut self) -> Result<String, FeedError> {
        if self.bytes.get(self.pos) != Some(&b'"') {
            return Err(FeedError::MalformedJson("expected string"));
        }
        self.pos += 1;
        let mut out = String::new();
        while self.pos < self.bytes.len() {
            let b = self.bytes[self.pos];
            if b == b'"' {
                self.pos += 1;
                return Ok(out);
            }
            if b == b'\\' {
                self.pos += 1;
                let esc = *self
                    .bytes
                    .get(self.pos)
                    .ok_or(FeedError::MalformedJson("unterminated escape"))?;
                self.pos += 1;
                match esc {
                    b'"' => out.push('"'),
                    b'\\' => out.push('\\'),
                    b'/' => out.push('/'),
                    b'b' => out.push('\u{0008}'),
                    b'f' => out.push('\u{000C}'),
                    b'n' => out.push('\n'),
                    b'r' => out.push('\r'),
                    b't' => out.push('\t'),
                    b'u' => {
                        let code = self.parse_hex4()?;
                        if (0xD800..=0xDBFF).contains(&code)
                            && self.s[self.pos..].starts_with("\\u")
                        {
                            self.pos += 2;
                            let low = self.parse_hex4()?;
                            if (0xDC00..=0xDFFF).contains(&low) {
                                let combined = 0x10000
                                    + (((u32::from(code) - 0xD800) << 10)
                                        | (u32::from(low) - 0xDC00));
                                if let Some(ch) = char::from_u32(combined) {
                                    out.push(ch);
                                }
                            }
                        } else if let Some(ch) = char::from_u32(u32::from(code)) {
                            out.push(ch);
                        }
                    }
                    _ => return Err(FeedError::MalformedJson("invalid escape character")),
                }
            } else {
                let ch = self.s[self.pos..].chars().next().unwrap();
                out.push(ch);
                self.pos += ch.len_utf8();
            }
        }
        Err(FeedError::MalformedJson("unterminated JSON string"))
    }

    fn parse_hex4(&mut self) -> Result<u16, FeedError> {
        if self.pos + 4 > self.bytes.len() {
            return Err(FeedError::MalformedJson("truncated \\u escape"));
        }
        let hex = &self.s[self.pos..self.pos + 4];
        self.pos += 4;
        u16::from_str_radix(hex, 16)
            .map_err(|_| FeedError::MalformedJson("invalid hex in \\u escape"))
    }

    fn parse_array(&mut self, depth: usize) -> Result<JsonValue, FeedError> {
        self.pos += 1; // '['
        let mut items = Vec::new();
        self.skip_ws();
        if self.bytes.get(self.pos) == Some(&b']') {
            self.pos += 1;
            return Ok(JsonValue::Array(items));
        }
        loop {
            items.push(self.parse_value(depth)?);
            self.skip_ws();
            match self.bytes.get(self.pos) {
                Some(&b',') => self.pos += 1,
                Some(&b']') => {
                    self.pos += 1;
                    return Ok(JsonValue::Array(items));
                }
                _ => return Err(FeedError::MalformedJson("expected ',' or ']' in array")),
            }
        }
    }

    fn parse_object(&mut self, depth: usize) -> Result<JsonValue, FeedError> {
        self.pos += 1; // '{'
        let mut pairs = Vec::new();
        self.skip_ws();
        if self.bytes.get(self.pos) == Some(&b'}') {
            self.pos += 1;
            return Ok(JsonValue::Object(pairs));
        }
        loop {
            self.skip_ws();
            let key = self.parse_string()?;
            self.skip_ws();
            if self.bytes.get(self.pos) != Some(&b':') {
                return Err(FeedError::MalformedJson("expected ':' in object"));
            }
            self.pos += 1;
            let val = self.parse_value(depth)?;
            pairs.push((key, val));
            self.skip_ws();
            match self.bytes.get(self.pos) {
                Some(&b',') => self.pos += 1,
                Some(&b'}') => {
                    self.pos += 1;
                    return Ok(JsonValue::Object(pairs));
                }
                _ => return Err(FeedError::MalformedJson("expected ',' or '}' in object")),
            }
        }
    }
}
