//! Iterative, bounded XML parser for RSS and Atom feeds.
//!
//! Deliberately skips DTD entity expansion (immune to entity expansion bombs)
//! and caps nesting depth and total node count.

use crate::sanitize::decode_entities;
use crate::{FeedError, MAX_XML_DEPTH, MAX_XML_NODES};

#[derive(Debug, Clone)]
pub struct XmlElement {
    pub name: String,
    pub attrs: Vec<(String, String)>,
    pub children: Vec<XmlElement>,
    pub text: String,
}

impl XmlElement {
    /// Local name without namespace prefix (e.g. `content:encoded` -> `encoded`).
    pub fn local_name(&self) -> &str {
        match self.name.rsplit_once(':') {
            Some((_, local)) => local,
            None => &self.name,
        }
    }

    /// Case-insensitive attribute lookup by full or local name.
    pub fn attr(&self, target: &str) -> Option<&str> {
        self.attrs
            .iter()
            .find(|(k, _)| {
                k.eq_ignore_ascii_case(target)
                    || k.rsplit_once(':')
                        .is_some_and(|(_, local)| local.eq_ignore_ascii_case(target))
            })
            .map(|(_, v)| v.as_str())
    }

    /// First child matching `name` (full name or local name, case-insensitive).
    pub fn child(&self, name: &str) -> Option<&XmlElement> {
        self.children.iter().find(|c| c.matches_name(name))
    }

    /// Text of the first matching child, trimmed.
    pub fn child_text(&self, name: &str) -> Option<&str> {
        self.child(name)
            .map(|c| c.text.trim())
            .filter(|t| !t.is_empty())
    }

    pub fn matches_name(&self, target: &str) -> bool {
        if target.contains(':') {
            self.name.eq_ignore_ascii_case(target)
        } else {
            self.local_name().eq_ignore_ascii_case(target)
        }
    }
}

/// Parses an XML document and returns its root element.
pub fn parse_xml(input: &str) -> Result<XmlElement, FeedError> {
    let s = input.strip_prefix('\u{feff}').unwrap_or(input);
    let bytes = s.as_bytes();
    let mut pos = 0;
    let mut stack: Vec<XmlElement> = Vec::new();
    let mut root: Option<XmlElement> = None;
    let mut node_count: usize = 0;

    while pos < bytes.len() {
        if bytes[pos] == b'<' {
            // Comment: <!-- ... -->
            if s[pos..].starts_with("<!--") {
                let end = s[pos + 4..]
                    .find("-->")
                    .ok_or(FeedError::MalformedXml("unterminated XML comment"))?;
                pos += 4 + end + 3;
                continue;
            }
            // CDATA: <![CDATA[ ... ]]>
            if s[pos..].starts_with("<![CDATA[") {
                let end = s[pos + 9..]
                    .find("]]>")
                    .ok_or(FeedError::MalformedXml("unterminated CDATA section"))?;
                if let Some(current) = stack.last_mut() {
                    current.text.push_str(&s[pos + 9..pos + 9 + end]);
                }
                pos += 9 + end + 3;
                continue;
            }
            // Processing instruction: <? ... ?>
            if s[pos..].starts_with("<?") {
                let end = s[pos + 2..].find("?>").ok_or(FeedError::MalformedXml(
                    "unterminated processing instruction",
                ))?;
                pos += 2 + end + 2;
                continue;
            }
            // DOCTYPE or other declaration: <! ... > (supports internal [...] subset)
            if s[pos..].starts_with("<!") {
                pos = skip_declaration(s, pos)?;
                continue;
            }
            // Closing tag: </name>
            if s[pos..].starts_with("</") {
                let close = s[pos + 2..]
                    .find('>')
                    .ok_or(FeedError::MalformedXml("unterminated closing tag"))?;
                let close_name = s[pos + 2..pos + 2 + close].trim();
                pos += 2 + close + 1;

                // Pop matching element (tolerant of minor unclosed tags in wild RSS feeds).
                if let Some(match_idx) = stack
                    .iter()
                    .rposition(|el| el.name.eq_ignore_ascii_case(close_name))
                {
                    while stack.len() > match_idx {
                        let finished = stack.pop().unwrap();
                        attach_element(finished, &mut stack, &mut root);
                    }
                }
                continue;
            }

            // Opening or self-closing tag.
            let open = parse_open_tag(s, pos)?;
            pos = open.end_pos;

            node_count += 1;
            if node_count > MAX_XML_NODES {
                return Err(FeedError::TooManyNodes);
            }
            if stack.len() >= MAX_XML_DEPTH {
                return Err(FeedError::NestingTooDeep);
            }

            let element = XmlElement {
                name: open.name,
                attrs: open.attrs,
                children: Vec::new(),
                text: String::new(),
            };
            if open.self_closing {
                attach_element(element, &mut stack, &mut root);
            } else {
                stack.push(element);
            }
        } else {
            let next_lt = s[pos..]
                .find('<')
                .map(|rel| pos + rel)
                .unwrap_or(bytes.len());
            if let Some(current) = stack.last_mut() {
                let chunk = &s[pos..next_lt];
                if !chunk.is_empty() {
                    current.text.push_str(&decode_entities(chunk));
                }
            }
            pos = next_lt;
        }
    }

    // Close any unclosed trailing elements cleanly.
    while let Some(finished) = stack.pop() {
        attach_element(finished, &mut stack, &mut root);
    }

    root.ok_or(FeedError::MalformedXml("no root XML element found"))
}

fn attach_element(el: XmlElement, stack: &mut [XmlElement], root: &mut Option<XmlElement>) {
    if let Some(parent) = stack.last_mut() {
        parent.children.push(el);
    } else if root.is_none() {
        *root = Some(el);
    }
}

fn skip_declaration(s: &str, start: usize) -> Result<usize, FeedError> {
    let bytes = s.as_bytes();
    let mut i = start + 2;
    let mut bracket_depth: usize = 0;
    let mut quote: Option<u8> = None;

    while i < bytes.len() {
        let b = bytes[i];
        if let Some(q) = quote {
            if b == q {
                quote = None;
            }
        } else {
            match b {
                b'"' | b'\'' => quote = Some(b),
                b'[' => bracket_depth += 1,
                b']' => bracket_depth = bracket_depth.saturating_sub(1),
                b'>' if bracket_depth == 0 => return Ok(i + 1),
                _ => {}
            }
        }
        i += 1;
    }
    Err(FeedError::MalformedXml("unterminated XML declaration"))
}

struct OpenTag {
    end_pos: usize,
    self_closing: bool,
    name: String,
    attrs: Vec<(String, String)>,
}

fn parse_open_tag(s: &str, start: usize) -> Result<OpenTag, FeedError> {
    let bytes = s.as_bytes();
    let mut i = start + 1;

    // Parse element name.
    let name_start = i;
    while i < bytes.len() && !bytes[i].is_ascii_whitespace() && bytes[i] != b'>' && bytes[i] != b'/'
    {
        i += 1;
    }
    if i == name_start {
        return Err(FeedError::MalformedXml("empty XML tag name"));
    }
    let name = s[name_start..i].to_string();
    let mut attrs = Vec::new();

    loop {
        while i < bytes.len() && bytes[i].is_ascii_whitespace() {
            i += 1;
        }
        if i >= bytes.len() {
            return Err(FeedError::MalformedXml("unterminated opening tag"));
        }
        if bytes[i] == b'>' {
            return Ok(OpenTag {
                end_pos: i + 1,
                self_closing: false,
                name,
                attrs,
            });
        }
        if bytes[i] == b'/' && i + 1 < bytes.len() && bytes[i + 1] == b'>' {
            return Ok(OpenTag {
                end_pos: i + 2,
                self_closing: true,
                name,
                attrs,
            });
        }

        // Attribute name.
        let attr_start = i;
        while i < bytes.len()
            && !bytes[i].is_ascii_whitespace()
            && bytes[i] != b'='
            && bytes[i] != b'>'
            && bytes[i] != b'/'
        {
            i += 1;
        }
        if i == attr_start {
            return Err(FeedError::MalformedXml("invalid attribute in tag"));
        }
        let attr_name = s[attr_start..i].to_string();

        while i < bytes.len() && bytes[i].is_ascii_whitespace() {
            i += 1;
        }
        let attr_val = if i < bytes.len() && bytes[i] == b'=' {
            i += 1;
            while i < bytes.len() && bytes[i].is_ascii_whitespace() {
                i += 1;
            }
            if i < bytes.len() && (bytes[i] == b'"' || bytes[i] == b'\'') {
                let q = bytes[i];
                i += 1;
                let val_start = i;
                while i < bytes.len() && bytes[i] != q {
                    i += 1;
                }
                if i >= bytes.len() {
                    return Err(FeedError::MalformedXml("unterminated attribute value"));
                }
                let val = decode_entities(&s[val_start..i]);
                i += 1;
                val
            } else {
                let val_start = i;
                while i < bytes.len()
                    && !bytes[i].is_ascii_whitespace()
                    && bytes[i] != b'>'
                    && bytes[i] != b'/'
                {
                    i += 1;
                }
                decode_entities(&s[val_start..i])
            }
        } else {
            String::new()
        };

        if attrs.len() < 32 {
            attrs.push((attr_name, attr_val));
        }
    }
}

/// Peeks at the first XML root tag name without parsing the whole document.
pub fn sniff_root_tag(prefix: &str) -> Option<String> {
    let s = prefix.strip_prefix('\u{feff}').unwrap_or(prefix);
    let bytes = s.as_bytes();
    let mut pos = 0;

    while pos < bytes.len() {
        while pos < bytes.len() && bytes[pos].is_ascii_whitespace() {
            pos += 1;
        }
        if pos >= bytes.len() || bytes[pos] != b'<' {
            return None;
        }
        if s[pos..].starts_with("<!--") {
            let end = s[pos + 4..].find("-->")?;
            pos += 4 + end + 3;
            continue;
        }
        if s[pos..].starts_with("<?") {
            let end = s[pos + 2..].find("?>")?;
            pos += 2 + end + 2;
            continue;
        }
        if s[pos..].starts_with("<!") {
            pos = skip_declaration(s, pos).ok()?;
            continue;
        }
        let name_start = pos + 1;
        let mut i = name_start;
        while i < bytes.len()
            && !bytes[i].is_ascii_whitespace()
            && bytes[i] != b'>'
            && bytes[i] != b'/'
        {
            i += 1;
        }
        if i > name_start {
            return Some(s[name_start..i].to_ascii_lowercase());
        }
        return None;
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_rss_with_cdata_and_namespaces() {
        let doc = r#"<?xml version="1.0" encoding="UTF-8"?>
        <rss version="2.0" xmlns:content="http://purl.org/rss/1.0/modules/content/">
          <channel>
            <title>Test &amp; Blog</title>
            <item>
              <title>Hello</title>
              <content:encoded><![CDATA[<p>Body & more</p>]]></content:encoded>
            </item>
          </channel>
        </rss>"#;
        let root = parse_xml(doc).unwrap();
        assert_eq!(root.name, "rss");
        let channel = root.child("channel").unwrap();
        assert_eq!(channel.child_text("title"), Some("Test & Blog"));
        let item = channel.child("item").unwrap();
        assert_eq!(
            item.child_text("content:encoded"),
            Some("<p>Body & more</p>")
        );
    }

    #[test]
    fn enforces_nesting_depth_limit() {
        let mut deep = String::new();
        for _ in 0..=MAX_XML_DEPTH + 2 {
            deep.push_str("<a>");
        }
        assert_eq!(parse_xml(&deep).unwrap_err(), FeedError::NestingTooDeep);
    }

    #[test]
    fn sniffs_root_tag_past_prolog_and_comments() {
        let s =
            "<?xml version=\"1.0\"?><!-- comment --><feed xmlns=\"http://www.w3.org/2005/Atom\">";
        assert_eq!(sniff_root_tag(s).as_deref(), Some("feed"));
    }
}
