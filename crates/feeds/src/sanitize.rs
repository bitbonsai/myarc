//! Entity decoding, HTML escaping, plain-text extraction, and bounded HTML
//! sanitization for untrusted feed content.

use crate::url::safe_http_url;

/// Escapes text for safe insertion into HTML text nodes or double-quoted attributes.
pub fn escape_html(input: &str) -> String {
    let mut out = String::with_capacity(input.len());
    for ch in input.chars() {
        match ch {
            '&' => out.push_str("&amp;"),
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '"' => out.push_str("&quot;"),
            '\'' => out.push_str("&#39;"),
            _ => out.push(ch),
        }
    }
    out
}

/// Decodes XML and common HTML character references (`&amp;`, `&lt;`, `&gt;`,
/// `&quot;`, `&apos;`, `&nbsp;`, `&mdash;`, `&ndash;`, `&hellip;`, `&rsquo;`,
/// `&lsquo;`, `&rdquo;`, `&ldquo;`, and numeric `&#...;` / `&#x...;`).
pub fn decode_entities(input: &str) -> String {
    if !input.contains('&') {
        return input.to_string();
    }
    let bytes = input.as_bytes();
    let mut out = String::with_capacity(input.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'&'
            && let Some((decoded, next_i)) = decode_one_entity(input, i)
        {
            out.push_str(decoded.as_str());
            i = next_i;
            continue;
        }
        let ch = input[i..].chars().next().unwrap();
        out.push(ch);
        i += ch.len_utf8();
    }
    out
}

fn decode_one_entity(input: &str, start: usize) -> Option<(String, usize)> {
    let rest = &input[start + 1..];
    let semi = rest.find(';')?;
    if semi == 0 || semi > 12 {
        return None;
    }
    let name = &rest[..semi];
    let next_i = start + 1 + semi + 1;

    if let Some(num) = name.strip_prefix('#') {
        let code = if let Some(hex) = num.strip_prefix('x').or_else(|| num.strip_prefix('X')) {
            u32::from_str_radix(hex, 16).ok()?
        } else {
            num.parse::<u32>().ok()?
        };
        if code == 0 {
            return None;
        }
        let ch = char::from_u32(code)?;
        return Some((ch.to_string(), next_i));
    }

    let replacement = match name {
        "amp" => "&",
        "lt" => "<",
        "gt" => ">",
        "quot" => "\"",
        "apos" => "'",
        "nbsp" => " ",
        "mdash" => "\u{2014}",
        "ndash" => "\u{2013}",
        "hellip" => "\u{2026}",
        "lsquo" => "\u{2018}",
        "rsquo" => "\u{2019}",
        "ldquo" => "\u{201C}",
        "rdquo" => "\u{201D}",
        "copy" => "\u{00A9}",
        "middot" => "\u{00B7}",
        _ => return None,
    };
    Some((replacement.to_string(), next_i))
}

/// Converts HTML or entity-encoded text into a clean, single-spaced plain-text
/// excerpt bounded to `max_bytes`.
pub fn html_to_plain_text(input: &str, max_bytes: usize) -> String {
    let mut raw_text = String::with_capacity(input.len().min(max_bytes));
    let mut i = 0;
    let bytes = input.as_bytes();
    let mut skip_until_tag: Option<&'static str> = None;

    while i < bytes.len() {
        if bytes[i] == b'<' {
            if let Some(close_rel) = input[i..].find('>') {
                let tag_inner = &input[i + 1..i + close_rel];
                let tag_name = extract_tag_name(tag_inner);
                if let Some(expected) = skip_until_tag {
                    if tag_inner.trim_start().starts_with('/')
                        && tag_name.eq_ignore_ascii_case(expected)
                    {
                        skip_until_tag = None;
                    }
                } else if !tag_inner.trim_start().starts_with('/') {
                    if let Some(blocked) = drop_body_tag(&tag_name) {
                        if !tag_inner.trim_end().ends_with('/') {
                            skip_until_tag = Some(blocked);
                        }
                    } else if is_block_tag(&tag_name) {
                        raw_text.push(' ');
                    }
                }
                i += close_rel + 1;
                continue;
            }
        }
        if skip_until_tag.is_some() {
            i += 1;
            continue;
        }
        let ch = input[i..].chars().next().unwrap();
        raw_text.push(ch);
        i += ch.len_utf8();
        if raw_text.len() > max_bytes.saturating_mul(2) {
            break;
        }
    }

    let decoded = decode_entities(&raw_text);
    collapse_whitespace(&decoded, max_bytes)
}

/// Sanitizes untrusted HTML from a feed item into an allowlisted subset of safe
/// formatting tags, stripping all scripts, styles, event handlers, and unsafe URLs.
pub fn sanitize_html(input: &str, base_url: Option<&str>, max_bytes: usize) -> String {
    let mut out = String::with_capacity(input.len().min(max_bytes));
    let bytes = input.as_bytes();
    let mut i = 0;
    let mut skip_until_tag: Option<&'static str> = None;
    let mut open_tags: Vec<&'static str> = Vec::new();

    while i < bytes.len() && out.len() < max_bytes {
        if bytes[i] == b'<' {
            // Skip HTML comments.
            if input[i..].starts_with("<!--") {
                if let Some(end_rel) = input[i + 4..].find("-->") {
                    i += 4 + end_rel + 3;
                    continue;
                }
                break;
            }
            if let Some(close_rel) = input[i..].find('>') {
                let tag_inner = input[i + 1..i + close_rel].trim();
                let is_closing = tag_inner.starts_with('/');
                let is_self_closing = tag_inner.ends_with('/');
                let tag_name = extract_tag_name(tag_inner);

                if let Some(expected) = skip_until_tag {
                    if is_closing && tag_name.eq_ignore_ascii_case(expected) {
                        skip_until_tag = None;
                    }
                    i += close_rel + 1;
                    continue;
                }

                if !is_closing && let Some(blocked) = drop_body_tag(&tag_name) {
                    if !is_self_closing {
                        skip_until_tag = Some(blocked);
                    }
                    i += close_rel + 1;
                    continue;
                }

                if let Some(allowed) = allowed_tag(&tag_name) {
                    if is_closing {
                        if let Some(pos) = open_tags.iter().rposition(|&t| t == allowed) {
                            while open_tags.len() > pos {
                                let closing = open_tags.pop().unwrap();
                                out.push_str("</");
                                out.push_str(closing);
                                out.push('>');
                            }
                        }
                    } else if is_void_tag(allowed) {
                        write_opening_tag(&mut out, allowed, tag_inner, base_url);
                    } else {
                        write_opening_tag(&mut out, allowed, tag_inner, base_url);
                        if !is_self_closing && open_tags.len() < 32 {
                            open_tags.push(allowed);
                        }
                    }
                }
                i += close_rel + 1;
                continue;
            }
        }

        if skip_until_tag.is_some() {
            i += 1;
            continue;
        }

        let ch = input[i..].chars().next().unwrap();
        match ch {
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '&' => {
                if let Some((decoded, next_i)) = decode_one_entity(input, i) {
                    out.push_str(&escape_html(&decoded));
                    i = next_i;
                    continue;
                }
                out.push_str("&amp;");
            }
            _ => out.push(ch),
        }
        i += ch.len_utf8();
    }

    while let Some(tag) = open_tags.pop() {
        out.push_str("</");
        out.push_str(tag);
        out.push('>');
    }
    out
}

fn write_opening_tag(out: &mut String, tag: &'static str, tag_inner: &str, base_url: Option<&str>) {
    match tag {
        "a" => {
            if let Some(href) = extract_attr(tag_inner, "href")
                .and_then(|v| safe_http_url(&decode_entities(&v), base_url))
            {
                out.push_str("<a href=\"");
                out.push_str(&escape_html(&href));
                out.push_str("\" rel=\"noopener noreferrer\">");
            } else {
                out.push_str("<a>");
            }
        }
        "img" => {
            if let Some(src) = extract_attr(tag_inner, "src")
                .and_then(|v| safe_http_url(&decode_entities(&v), base_url))
            {
                out.push_str("<img src=\"");
                out.push_str(&escape_html(&src));
                out.push('"');
                if let Some(alt) = extract_attr(tag_inner, "alt") {
                    out.push_str(" alt=\"");
                    out.push_str(&escape_html(&decode_entities(&alt)));
                    out.push('"');
                }
                out.push_str(" loading=\"lazy\">");
            }
        }
        _ => {
            out.push('<');
            out.push_str(tag);
            out.push('>');
        }
    }
}

fn extract_tag_name(tag_inner: &str) -> String {
    let trimmed = tag_inner.trim_start_matches(['/', ' ', '\t', '\n', '\r']);
    trimmed
        .chars()
        .take_while(|c| c.is_ascii_alphanumeric() || *c == ':' || *c == '-')
        .map(|c| c.to_ascii_lowercase())
        .collect()
}

fn extract_attr(tag_inner: &str, attr_name: &str) -> Option<String> {
    let bytes = tag_inner.as_bytes();
    let mut i = 0;
    // Skip tag name.
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
        let name = &tag_inner[name_start..i];
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
        let value = if bytes[i] == b'"' || bytes[i] == b'\'' {
            let quote = bytes[i];
            i += 1;
            let val_start = i;
            while i < bytes.len() && bytes[i] != quote {
                i += 1;
            }
            let val = &tag_inner[val_start..i];
            if i < bytes.len() {
                i += 1;
            }
            val
        } else {
            let val_start = i;
            while i < bytes.len() && !bytes[i].is_ascii_whitespace() && bytes[i] != b'/' {
                i += 1;
            }
            &tag_inner[val_start..i]
        };
        if name.eq_ignore_ascii_case(attr_name) {
            return Some(value.to_string());
        }
    }
    None
}

fn drop_body_tag(name: &str) -> Option<&'static str> {
    match name {
        "script" => Some("script"),
        "style" => Some("style"),
        "iframe" => Some("iframe"),
        "object" => Some("object"),
        "svg" => Some("svg"),
        "math" => Some("math"),
        "noscript" => Some("noscript"),
        _ => None,
    }
}

fn allowed_tag(name: &str) -> Option<&'static str> {
    match name {
        "p" => Some("p"),
        "br" => Some("br"),
        "a" => Some("a"),
        "strong" => Some("strong"),
        "b" => Some("b"),
        "em" => Some("em"),
        "i" => Some("i"),
        "code" => Some("code"),
        "pre" => Some("pre"),
        "blockquote" => Some("blockquote"),
        "ul" => Some("ul"),
        "ol" => Some("ol"),
        "li" => Some("li"),
        "h1" => Some("h1"),
        "h2" => Some("h2"),
        "h3" => Some("h3"),
        "h4" => Some("h4"),
        "h5" => Some("h5"),
        "h6" => Some("h6"),
        "hr" => Some("hr"),
        "img" => Some("img"),
        "figure" => Some("figure"),
        "figcaption" => Some("figcaption"),
        "table" => Some("table"),
        "thead" => Some("thead"),
        "tbody" => Some("tbody"),
        "tr" => Some("tr"),
        "th" => Some("th"),
        "td" => Some("td"),
        "del" => Some("del"),
        "s" => Some("s"),
        "sup" => Some("sup"),
        "sub" => Some("sub"),
        _ => None,
    }
}

fn is_void_tag(tag: &str) -> bool {
    matches!(tag, "br" | "hr" | "img")
}

fn is_block_tag(tag: &str) -> bool {
    matches!(
        tag,
        "p" | "br" | "div" | "li" | "blockquote" | "h1" | "h2" | "h3" | "h4" | "h5" | "h6" | "tr"
    )
}

fn collapse_whitespace(input: &str, max_bytes: usize) -> String {
    let mut out = String::with_capacity(input.len().min(max_bytes));
    let mut last_was_space = true;
    for ch in input.chars() {
        if ch.is_whitespace() {
            if !last_was_space {
                if out.len() + 1 > max_bytes {
                    break;
                }
                out.push(' ');
                last_was_space = true;
            }
        } else {
            if out.len() + ch.len_utf8() > max_bytes {
                break;
            }
            out.push(ch);
            last_was_space = false;
        }
    }
    if out.ends_with(' ') {
        out.pop();
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn decodes_numeric_and_named_entities() {
        assert_eq!(
            decode_entities("Arc&#39;s &amp; Slirc&#x27;s &mdash; test"),
            "Arc's & Slirc's \u{2014} test"
        );
    }

    #[test]
    fn sanitizer_strips_scripts_and_javascript_urls() {
        let dirty = r#"<p onclick="evil()">Hello <script>alert(1)</script><a href="javascript:alert(2)">bad</a> <a href="https://myarc.dev">good</a></p>"#;
        let clean = sanitize_html(dirty, None, 4096);
        assert!(!clean.contains("script"));
        assert!(!clean.contains("onclick"));
        assert!(!clean.contains("javascript:"));
        assert!(
            clean.contains(r#"<a href="https://myarc.dev" rel="noopener noreferrer">good</a>"#)
        );
    }

    #[test]
    fn plain_text_strips_tags_and_collapses_whitespace() {
        let html = "<p>First line</p><p>Second &amp; <b>bold</b></p><script>x()</script>";
        assert_eq!(html_to_plain_text(html, 256), "First line Second & bold");
    }
}
