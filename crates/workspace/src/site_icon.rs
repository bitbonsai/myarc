//! Finds the icon a page declares with `<link rel="icon">` in the start of its HTML.
//!
//! Input is untrusted page bytes. The scan reads at most [`MAX_SCAN_BYTES`], stops at
//! `<body` or `</head>`, skips comments, scripts and styles, and never allocates more
//! than one href. It does not resolve or validate the href; the browser does that with
//! its own URL parser and decides whether to fetch it. SVG icons are skipped because
//! the browser's image decoder can't read them.

/// Most HTML bytes scanned. Callers should fetch no more than this.
pub const MAX_SCAN_BYTES: usize = 256 * 1024;
/// Longest href returned.
pub const MAX_HREF_BYTES: usize = 2048;

/// The href of the best non-SVG icon link in the document's head, or `None`.
/// Among several, the one declaring the largest size up to 256 px wins; ties keep
/// the first.
pub fn declared_icon(html: &[u8]) -> Option<String> {
    let html = &html[..html.len().min(MAX_SCAN_BYTES)];
    let mut best: Option<(u32, &[u8])> = None;
    let mut at = 0;
    while let Some(open) = find_byte(html, at, b'<') {
        let start = open + 1;
        if html[start..].starts_with(b"!--") {
            match find_seq(html, start + 3, b"-->") {
                Some(end) => at = end + 3,
                None => break,
            }
            continue;
        }
        let end_tag = html.get(start) == Some(&b'/');
        let name_start = start + usize::from(end_tag);
        let name_end = name_start
            + html[name_start..]
                .iter()
                .take_while(|byte| byte.is_ascii_alphanumeric())
                .count();
        let name = &html[name_start..name_end];
        at = name_end;
        if name.is_empty() {
            continue;
        }
        if name.eq_ignore_ascii_case(b"body") || (end_tag && name.eq_ignore_ascii_case(b"head")) {
            break;
        }
        if end_tag {
            continue;
        }
        if name.eq_ignore_ascii_case(b"script") || name.eq_ignore_ascii_case(b"style") {
            let close: &[u8] = if name.eq_ignore_ascii_case(b"script") {
                b"</script"
            } else {
                b"</style"
            };
            match find_seq_ignore_case(html, at, close) {
                Some(end) => at = end + close.len(),
                None => break,
            }
            continue;
        }
        let Some((attributes, next)) = attributes(html, at) else {
            break;
        };
        at = next;
        if !name.eq_ignore_ascii_case(b"link") {
            continue;
        }
        if let Some((score, href)) = icon_candidate(&attributes) {
            if best.is_none_or(|(best_score, _)| score > best_score) {
                best = Some((score, href));
            }
        }
    }
    best.map(|(_, href)| decode_href(href))
}

struct Attributes<'a> {
    rel: Option<&'a [u8]>,
    href: Option<&'a [u8]>,
    kind: Option<&'a [u8]>,
    sizes: Option<&'a [u8]>,
}

// Reads a start tag's attributes from `at` through its `>`. `None` when the tag is cut
// off by the end of the input.
fn attributes(html: &[u8], mut at: usize) -> Option<(Attributes<'_>, usize)> {
    let mut found = Attributes {
        rel: None,
        href: None,
        kind: None,
        sizes: None,
    };
    loop {
        while html
            .get(at)
            .is_some_and(|byte| byte.is_ascii_whitespace() || *byte == b'/')
        {
            at += 1;
        }
        match html.get(at) {
            None => return None,
            Some(b'>') => return Some((found, at + 1)),
            Some(_) => {}
        }
        let name_start = at;
        while html
            .get(at)
            .is_some_and(|byte| !byte.is_ascii_whitespace() && !b"=>/".contains(byte))
        {
            at += 1;
        }
        if at == name_start {
            // A stray `=`.
            at += 1;
            continue;
        }
        let name = &html[name_start..at];
        while html.get(at).is_some_and(u8::is_ascii_whitespace) {
            at += 1;
        }
        let mut value: &[u8] = b"";
        if html.get(at) == Some(&b'=') {
            at += 1;
            while html.get(at).is_some_and(u8::is_ascii_whitespace) {
                at += 1;
            }
            match html.get(at) {
                None => return None,
                Some(&quote @ (b'"' | b'\'')) => {
                    let end = find_byte(html, at + 1, quote)?;
                    value = &html[at + 1..end];
                    at = end + 1;
                }
                Some(_) => {
                    let start = at;
                    while html
                        .get(at)
                        .is_some_and(|byte| !byte.is_ascii_whitespace() && *byte != b'>')
                    {
                        at += 1;
                    }
                    value = &html[start..at];
                }
            }
        }
        // The first of a repeated attribute wins, as in HTML.
        let slot = if name.eq_ignore_ascii_case(b"rel") {
            &mut found.rel
        } else if name.eq_ignore_ascii_case(b"href") {
            &mut found.href
        } else if name.eq_ignore_ascii_case(b"type") {
            &mut found.kind
        } else if name.eq_ignore_ascii_case(b"sizes") {
            &mut found.sizes
        } else {
            continue;
        };
        slot.get_or_insert(value);
    }
}

// The link's size score and raw href when it is a usable icon.
fn icon_candidate<'a>(link: &Attributes<'a>) -> Option<(u32, &'a [u8])> {
    let rel = link.rel?;
    if !rel
        .split(u8::is_ascii_whitespace)
        .any(|token| token.eq_ignore_ascii_case(b"icon"))
    {
        return None;
    }
    let href = link.href?.trim_ascii();
    if href.is_empty() || href.len() > MAX_HREF_BYTES {
        return None;
    }
    let path = href
        .split(|byte| b"?#".contains(byte))
        .next()
        .unwrap_or(href);
    if link
        .kind
        .is_some_and(|kind| contains_ignore_case(kind, b"svg"))
        || (path.len() >= 4 && path[path.len() - 4..].eq_ignore_ascii_case(b".svg"))
    {
        return None;
    }
    Some((size_score(link.sizes.unwrap_or_default()), href))
}

// The largest declared side up to 256 px. Larger icons score 1, so any sized icon
// beats an unsized one but a small exact icon beats a huge one.
fn size_score(sizes: &[u8]) -> u32 {
    sizes
        .split(u8::is_ascii_whitespace)
        .filter_map(|size| {
            let split = size.iter().position(|byte| matches!(byte, b'x' | b'X'))?;
            let width = parse_side(&size[..split])?;
            let height = parse_side(&size[split + 1..])?;
            let side = width.max(height);
            Some(if side > 256 { 1 } else { side })
        })
        .max()
        .unwrap_or(0)
}

fn parse_side(digits: &[u8]) -> Option<u32> {
    if digits.is_empty() || digits.len() > 5 || !digits.iter().all(u8::is_ascii_digit) {
        return None;
    }
    std::str::from_utf8(digits).ok()?.parse().ok()
}

// Undoes the one entity common in hrefs, `&amp;`.
fn decode_href(href: &[u8]) -> String {
    let mut out = Vec::with_capacity(href.len());
    let mut at = 0;
    while at < href.len() {
        if href[at..].len() >= 5 && href[at..at + 5].eq_ignore_ascii_case(b"&amp;") {
            out.push(b'&');
            at += 5;
        } else {
            out.push(href[at]);
            at += 1;
        }
    }
    String::from_utf8_lossy(&out).into_owned()
}

fn find_byte(html: &[u8], from: usize, byte: u8) -> Option<usize> {
    html.get(from..)?
        .iter()
        .position(|candidate| *candidate == byte)
        .map(|offset| from + offset)
}

fn find_seq(html: &[u8], from: usize, needle: &[u8]) -> Option<usize> {
    html.get(from..)?
        .windows(needle.len())
        .position(|window| window == needle)
        .map(|offset| from + offset)
}

fn find_seq_ignore_case(html: &[u8], from: usize, needle: &[u8]) -> Option<usize> {
    html.get(from..)?
        .windows(needle.len())
        .position(|window| window.eq_ignore_ascii_case(needle))
        .map(|offset| from + offset)
}

fn contains_ignore_case(haystack: &[u8], needle: &[u8]) -> bool {
    find_seq_ignore_case(haystack, 0, needle).is_some()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn icon(html: &str) -> Option<String> {
        declared_icon(html.as_bytes())
    }

    #[test]
    fn finds_icon_link_in_head() {
        assert_eq!(
            icon(r#"<html><head><title>x</title><link rel="icon" href="/a.png"></head>"#),
            Some("/a.png".into())
        );
        assert_eq!(
            icon("<LINK REL='Shortcut Icon' HREF='/b.ico'>"),
            Some("/b.ico".into())
        );
        assert_eq!(icon("<link href=/c.png rel=icon>"), Some("/c.png".into()));
        assert_eq!(
            icon(r#"<link rel="alternate icon" type="image/png" href=" /d.png ">"#),
            Some("/d.png".into())
        );
    }

    #[test]
    fn skips_other_links_and_svg() {
        assert_eq!(icon(r#"<link rel="stylesheet" href="/s.css">"#), None);
        assert_eq!(icon(r#"<link rel="apple-touch-icon" href="/t.png">"#), None);
        assert_eq!(
            icon(r#"<link rel="icon" type="image/svg+xml" href="/i">"#),
            None
        );
        assert_eq!(icon(r#"<link rel="icon" href="/i.SVG?v=2">"#), None);
        assert_eq!(icon(r#"<link rel="icon" href="">"#), None);
        assert_eq!(icon(r#"<link rel="icon">"#), None);
        assert_eq!(
            icon(r#"<link rel="icon" href="/i.svg"><link rel="icon" href="/i.png">"#),
            Some("/i.png".into())
        );
    }

    #[test]
    fn prefers_largest_declared_size() {
        let html = r#"<link rel=icon sizes=16x16 href=/16.png>
            <link rel=icon href=/plain.ico>
            <link rel=icon sizes="32x32 48x48" href=/48.png>
            <link rel=icon sizes=512x512 href=/512.png>
            <link rel=icon sizes=48x48 href=/48b.png>"#;
        assert_eq!(icon(html), Some("/48.png".into()));
        assert_eq!(
            icon("<link rel=icon href=/a.ico><link rel=icon sizes=any href=/b.ico>"),
            Some("/a.ico".into())
        );
    }

    #[test]
    fn ignores_comments_scripts_and_body() {
        assert_eq!(icon(r#"<!-- <link rel="icon" href="/c.png"> -->"#), None);
        assert_eq!(
            icon(r#"<script>var s = '<link rel="icon" href="/s.png">'</script>"#),
            None
        );
        assert_eq!(
            icon(r#"<style>a{} </STYLE><link rel=icon href=/after.png>"#),
            Some("/after.png".into())
        );
        assert_eq!(icon(r#"<body><link rel="icon" href="/b.png">"#), None);
        assert_eq!(icon(r#"</head><link rel="icon" href="/b.png">"#), None);
    }

    #[test]
    fn handles_cut_off_and_odd_markup() {
        assert_eq!(icon(r#"<link rel="icon" href="/a.png"#), None);
        assert_eq!(icon("<link rel=icon href=/a.png"), None);
        assert_eq!(icon("<!-- unterminated <link rel=icon href=/a.png>"), None);
        assert_eq!(
            icon("< <=> <link = rel=icon href=/x.png>"),
            Some("/x.png".into())
        );
        assert_eq!(
            icon(r#"<link rel="icon" href="/a.png" rel="stylesheet" href="/b.png">"#),
            Some("/a.png".into())
        );
    }

    #[test]
    fn decodes_amp_and_bounds_href() {
        assert_eq!(
            icon(r#"<link rel="icon" href="/i.png?a=1&amp;b=2">"#),
            Some("/i.png?a=1&b=2".into())
        );
        let long = format!(
            r#"<link rel="icon" href="/{}">"#,
            "a".repeat(MAX_HREF_BYTES)
        );
        assert_eq!(icon(&long), None);
    }

    #[test]
    fn stops_at_scan_limit() {
        let mut html = "<p>".repeat(MAX_SCAN_BYTES / 3 + 1);
        html.push_str(r#"<link rel="icon" href="/late.png">"#);
        assert_eq!(icon(&html), None);
    }

    #[test]
    fn arbitrary_bytes_never_panic() {
        let alphabet = b"<>/=\"' !-linkrelhreficonsvgtypesizes0x1&amp;bodyhead";
        let mut seed: u64 = 0x5eed;
        for _ in 0..2000 {
            let len = (seed % 200) as usize;
            let mut bytes = Vec::with_capacity(len);
            for _ in 0..len {
                seed = seed
                    .wrapping_mul(6364136223846793005)
                    .wrapping_add(1442695040888963407);
                let pick = (seed >> 33) as usize;
                bytes.push(if pick % 7 == 0 {
                    (pick >> 3) as u8
                } else {
                    alphabet[pick % alphabet.len()]
                });
            }
            if let Some(href) = declared_icon(&bytes) {
                assert!(href.len() <= MAX_HREF_BYTES * 3);
            }
        }
    }
}
