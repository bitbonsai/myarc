//! Single-pass O(n) CSS sanitizer, hidden-selector validator, and deterministic
//! stylesheet compiler.
//!
//! Per `docs/BOOSTS.md`, CSS can trigger network fetches or leak page data via
//! `@import`, `url()`, `src()`, and `image-set()`. This module rejects or strips
//! all remote/non-fragment resource references, including CSS hex escapes and
//! comments, while allowing local SVG fragment references (`url(#id)`).

use std::fmt::{self, Write as _};

use crate::{
    Boost, BoostAppearance, BoostsError, ColorMode, FontPreset, MAX_CUSTOM_CSS_BYTES,
    MAX_SELECTOR_BYTES, TextTransform,
};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CssBlockReason {
    AtImport,
    DisallowedUrl,
    DisallowedSrc,
    DisallowedImageSet,
    ControlCharacter,
}

impl fmt::Display for CssBlockReason {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Self::AtImport => "@import rules are blocked to prevent remote CSS fetches",
            Self::DisallowedUrl => "url() is restricted to local #fragment references",
            Self::DisallowedSrc => "src() is blocked to prevent remote resource fetches",
            Self::DisallowedImageSet => "image-set() is blocked to prevent remote resource fetches",
            Self::ControlCharacter => "CSS cannot contain control characters",
        })
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CssFinding {
    /// 1-based line number in the input stylesheet.
    pub line: usize,
    pub reason: CssBlockReason,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CssSanitizeResult {
    /// Sanitized CSS with blocked rules/calls stripped and unclosed tokens closed.
    pub sanitized: String,
    pub findings: Vec<CssFinding>,
}

/// Inspects custom CSS and returns any blocked security findings without mutating it.
pub fn inspect_css(css: &str) -> Vec<CssFinding> {
    sanitize_css(css).findings
}

/// Validates custom CSS size and security policy.
pub fn validate_custom_css(css: &str) -> Result<(), BoostsError> {
    if css.len() > MAX_CUSTOM_CSS_BYTES {
        return Err(BoostsError::LimitExceeded("custom CSS"));
    }
    let findings = inspect_css(css);
    if !findings.is_empty() {
        return Err(BoostsError::DisallowedCss(findings));
    }
    Ok(())
}

/// Validates a hidden-element CSS selector so it cannot break out into arbitrary rules.
pub fn validate_selector(raw: &str) -> Result<String, BoostsError> {
    let trimmed = raw.trim();
    if trimmed.is_empty() || trimmed.len() > MAX_SELECTOR_BYTES {
        return Err(BoostsError::InvalidSelector);
    }
    if trimmed.chars().any(char::is_control) {
        return Err(BoostsError::InvalidSelector);
    }
    if trimmed.contains("/*") || trimmed.contains("*/") {
        return Err(BoostsError::InvalidSelector);
    }

    let bytes = trimmed.as_bytes();
    let mut i = 0;
    let mut paren_depth: usize = 0;
    let mut bracket_depth: usize = 0;
    let mut in_quote: Option<u8> = None;

    while i < bytes.len() {
        let b = bytes[i];
        if let Some(q) = in_quote {
            if b == b'\\' {
                i += 1;
                if i >= bytes.len() {
                    return Err(BoostsError::InvalidSelector);
                }
                i += 1;
                continue;
            }
            if b == q {
                in_quote = None;
            }
            i += 1;
            continue;
        }

        match b {
            b'{' | b'}' | b';' | b'@' => return Err(BoostsError::InvalidSelector),
            b'"' | b'\'' => {
                in_quote = Some(b);
                i += 1;
            }
            b'\\' => {
                i += 1;
                if i >= bytes.len() {
                    return Err(BoostsError::InvalidSelector);
                }
                i += 1;
            }
            b'(' => {
                paren_depth += 1;
                i += 1;
            }
            b')' => {
                if paren_depth == 0 {
                    return Err(BoostsError::InvalidSelector);
                }
                paren_depth -= 1;
                i += 1;
            }
            b'[' => {
                bracket_depth += 1;
                i += 1;
            }
            b']' => {
                if bracket_depth == 0 {
                    return Err(BoostsError::InvalidSelector);
                }
                bracket_depth -= 1;
                i += 1;
            }
            _ => i += 1,
        }
    }

    if in_quote.is_some() || paren_depth != 0 || bracket_depth != 0 {
        return Err(BoostsError::InvalidSelector);
    }

    Ok(trimmed.to_owned())
}

/// Single-pass O(n) CSS sanitizer.
///
/// Strips `@import` rules and disallowed `url()` / `src()` / `image-set()` calls
/// (including CSS hex escapes and embedded comments), records structured findings,
/// and closes any unclosed comments, strings, or braces so concatenated Boosts
/// cannot corrupt each other.
pub fn sanitize_css(input: &str) -> CssSanitizeResult {
    let mut out = String::with_capacity(input.len());
    let mut findings = Vec::new();
    let bytes = input.as_bytes();
    let mut i = 0;
    let mut line: usize = 1;
    let mut brace_depth: usize = 0;

    while i < bytes.len() {
        let b = bytes[i];

        // Control characters (other than \n, \r, \t).
        if is_disallowed_control(b) {
            findings.push(CssFinding {
                line,
                reason: CssBlockReason::ControlCharacter,
            });
            i += 1;
            continue;
        }

        // Line tracking.
        if b == b'\n' {
            line += 1;
            out.push('\n');
            i += 1;
            continue;
        }

        // Comments: /* ... */
        if b == b'/' && i + 1 < bytes.len() && bytes[i + 1] == b'*' {
            let start = i;
            i += 2;
            let mut closed = false;
            while i < bytes.len() {
                if bytes[i] == b'\n' {
                    line += 1;
                }
                if bytes[i] == b'*' && i + 1 < bytes.len() && bytes[i + 1] == b'/' {
                    i += 2;
                    closed = true;
                    break;
                }
                i += 1;
            }
            push_sanitized_slice(&mut out, &input[start..i], &mut findings, line);
            if !closed {
                out.push_str(" */");
            }
            continue;
        }

        // Strings: "..." or '...'
        if b == b'"' || b == b'\'' {
            let quote = b;
            let start = i;
            i += 1;
            let mut closed = false;
            while i < bytes.len() {
                let c = bytes[i];
                if c == b'\n' {
                    line += 1;
                    // Unescaped newline terminates a CSS string as bad-string;
                    // close it so nothing leaks out.
                    break;
                }
                if c == b'\\' {
                    i += 1;
                    if i < bytes.len() {
                        if bytes[i] == b'\n' {
                            line += 1;
                        } else if bytes[i] == b'\r' && i + 1 < bytes.len() && bytes[i + 1] == b'\n'
                        {
                            line += 1;
                            i += 1;
                        }
                        i += 1;
                    }
                    continue;
                }
                if c == quote {
                    i += 1;
                    closed = true;
                    break;
                }
                i += 1;
            }
            push_sanitized_slice(&mut out, &input[start..i], &mut findings, line);
            if !closed {
                out.push(quote as char);
            }
            continue;
        }

        // At-keyword: @import (including CSS escapes like @\69 mport).
        if b == b'@' {
            let at_start = i;
            let at_line = line;
            i += 1;
            let (ident, next_i, next_line) = scan_css_ident(input, i, line);
            if ident.eq_ignore_ascii_case("import") {
                findings.push(CssFinding {
                    line: at_line,
                    reason: CssBlockReason::AtImport,
                });
                let (after_rule, after_line) = skip_at_rule(input, next_i, next_line);
                i = after_rule;
                line = after_line;
                continue;
            }
            out.push_str(&input[at_start..next_i]);
            i = next_i;
            line = next_line;
            continue;
        }

        // Identifier / function call: url(...), src(...), image-set(...), -webkit-image-set(...).
        if is_ident_start(input, i) {
            let ident_start = i;
            let ident_line = line;
            let (ident, next_i, next_line) = scan_css_ident(input, i, line);
            if next_i < bytes.len() && bytes[next_i] == b'(' {
                let lower = ident.to_ascii_lowercase();
                if lower == "url"
                    || lower == "src"
                    || lower == "image-set"
                    || lower == "-webkit-image-set"
                {
                    let (inner, after_paren, after_line) =
                        consume_balanced_parens(input, next_i + 1, next_line);
                    if lower == "url" {
                        if let Some(fragment) = extract_safe_fragment(inner) {
                            out.push_str("url(");
                            out.push_str(&fragment);
                            out.push(')');
                        } else {
                            findings.push(CssFinding {
                                line: ident_line,
                                reason: CssBlockReason::DisallowedUrl,
                            });
                            out.push_str("none");
                        }
                    } else if lower == "src" {
                        findings.push(CssFinding {
                            line: ident_line,
                            reason: CssBlockReason::DisallowedSrc,
                        });
                        out.push_str("none");
                    } else {
                        findings.push(CssFinding {
                            line: ident_line,
                            reason: CssBlockReason::DisallowedImageSet,
                        });
                        out.push_str("none");
                    }
                    i = after_paren;
                    line = after_line;
                    continue;
                }
            }
            out.push_str(&input[ident_start..next_i]);
            i = next_i;
            line = next_line;
            continue;
        }

        if b == b'{' {
            brace_depth += 1;
            out.push('{');
            i += 1;
            continue;
        }
        if b == b'}' {
            brace_depth = brace_depth.saturating_sub(1);
            out.push('}');
            i += 1;
            continue;
        }

        // Regular UTF-8 character.
        let ch = input[i..].chars().next().unwrap_or(' ');
        if ch.is_control() && !matches!(ch, '\n' | '\r' | '\t') {
            findings.push(CssFinding {
                line,
                reason: CssBlockReason::ControlCharacter,
            });
        } else {
            out.push(ch);
        }
        i += ch.len_utf8();
    }

    // Close any unclosed blocks so concatenated Boosts stay isolated.
    for _ in 0..brace_depth {
        out.push('}');
    }

    CssSanitizeResult {
        sanitized: out,
        findings,
    }
}

/// Compiles a single Boost into a bounded, deterministic, pure-CSS stylesheet.
///
/// Always applies defense-in-depth sanitization even if `boost` was constructed
/// without prior validation.
pub fn compile_boost_css(boost: &Boost) -> String {
    let mut css = String::new();
    emit_appearance_css(&boost.appearance, &mut css);

    for selector in &boost.hidden_selectors {
        if let Ok(valid) = validate_selector(selector) {
            let _ = writeln!(css, "{valid} {{ display: none !important; }}");
        }
    }

    if !boost.custom_css.is_empty() {
        let bounded = truncate_utf8(&boost.custom_css, MAX_CUSTOM_CSS_BYTES);
        let sanitized = sanitize_css(bounded).sanitized;
        let trimmed = sanitized.trim();
        if !trimmed.is_empty() {
            if !css.is_empty() && !css.ends_with('\n') {
                css.push('\n');
            }
            css.push_str(trimmed);
            css.push('\n');
        }
    }

    css
}

/// Compiles and concatenates multiple Boosts in slice order into one stylesheet.
pub fn compile_boosts_css(boosts: &[&Boost]) -> String {
    let mut combined = String::new();
    for boost in boosts {
        let part = compile_boost_css(boost);
        if !part.is_empty() {
            if !combined.is_empty() && !combined.ends_with('\n') {
                combined.push('\n');
            }
            combined.push_str(&part);
        }
    }
    combined
}

/// Local system font stack for each preset (zero remote webfonts).
pub const fn font_stack(preset: FontPreset) -> Option<&'static str> {
    match preset {
        FontPreset::Default => None,
        FontPreset::SystemSans => {
            Some("-apple-system, BlinkMacSystemFont, 'SF Pro Text', system-ui, sans-serif")
        }
        FontPreset::SystemSerif => Some("ui-serif, 'New York', Georgia, 'Times New Roman', serif"),
        FontPreset::SystemMono => {
            Some("ui-monospace, 'SF Mono', Menlo, Monaco, Consolas, monospace")
        }
        FontPreset::Rounded => {
            Some("ui-rounded, 'SF Pro Rounded', 'Hiragino Maru Gothic ProN', sans-serif")
        }
        FontPreset::Geometric => {
            Some("'Avenir Next', Avenir, Futura, 'Century Gothic', sans-serif")
        }
        FontPreset::Humanist => Some("'Gill Sans', Optima, 'Segoe UI', sans-serif"),
        FontPreset::Grotesk => Some("'Helvetica Neue', Helvetica, Arial, sans-serif"),
        FontPreset::Didone => Some("Didot, 'Bodoni MT', 'Times New Roman', serif"),
        FontPreset::SlabSerif => Some("Rockwell, 'Courier New', Georgia, serif"),
        FontPreset::ClassicSerif => Some("Baskerville, 'Palatino Linotype', Palatino, serif"),
        FontPreset::Typewriter => Some("'American Typewriter', 'Courier New', monospace"),
        FontPreset::Handwritten => Some("'Marker Felt', 'Bradley Hand', cursive"),
        FontPreset::Condensed => Some("'Arial Narrow', 'Helvetica Neue', sans-serif"),
        FontPreset::Wide => Some("Verdana, Geneva, Tahoma, sans-serif"),
        FontPreset::Comic => Some("'Chalkboard SE', 'Comic Sans MS', cursive"),
    }
}

fn append_palette_filters(palette: Option<crate::BoostPalette>, out: &mut String) {
    let Some(p) = palette else {
        return;
    };
    if p.contrast != 50 {
        let _ = write!(out, " contrast({}%)", 50 + u16::from(p.contrast));
    }
    if p.brightness != 50 {
        let _ = write!(out, " brightness({}%)", 50 + u16::from(p.brightness));
    }
    if p.saturation != 50 {
        let _ = write!(out, " saturate({}%)", u16::from(p.saturation) * 2);
    }
}

fn emit_appearance_css(appearance: &BoostAppearance, css: &mut String) {
    match appearance.color_mode {
        ColorMode::None => {}
        ColorMode::Invert => {
            // Blink's native Auto Dark (web_prefs->force_dark_mode_enabled)
            // handles smart color inversion while preserving photos and fixed
            // layout. Only emit optional contrast/brightness/saturation tweaks.
            let mut extra = String::new();
            append_palette_filters(appearance.palette, &mut extra);
            if !extra.is_empty() {
                let _ = writeln!(css, "html {{ filter: {} !important; }}", extra.trim_start());
            }
        }
        ColorMode::Palette => {
            if let Some(p) = appearance.palette {
                let bg = p.background & 0xFF_FFFF;
                let text = p.text & 0xFF_FFFF;
                let accent = p.accent & 0xFF_FFFF;
                let _ = writeln!(
                    css,
                    ":root {{ --myarc-boost-bg: #{bg:06x}; --myarc-boost-text: #{text:06x}; --myarc-boost-accent: #{accent:06x}; --myarc-boost-tint: {}%; }}",
                    p.tint_strength
                );
                let _ = writeln!(
                    css,
                    "html, body {{ background-color: #{bg:06x} !important; color: #{text:06x} !important; }}"
                );
                let _ = writeln!(css, "a {{ color: #{accent:06x} !important; }}");
                let mut filters = String::new();
                append_palette_filters(Some(p), &mut filters);
                if !filters.is_empty() {
                    let _ = writeln!(
                        css,
                        "html {{ filter: {} !important; }}",
                        filters.trim_start()
                    );
                }
            }
        }
    }

    if let Some(stack) = font_stack(appearance.font) {
        let _ = writeln!(
            css,
            "html, body, input, button, textarea, select {{ font-family: {stack} !important; }}"
        );
    }

    if appearance.text_scale_pct != 100 {
        let _ = writeln!(
            css,
            "html {{ font-size: {}% !important; }}",
            appearance.text_scale_pct
        );
    }

    match appearance.text_transform {
        TextTransform::None => {}
        TextTransform::Uppercase => {
            let _ = writeln!(css, "body {{ text-transform: uppercase !important; }}");
        }
        TextTransform::Lowercase => {
            let _ = writeln!(css, "body {{ text-transform: lowercase !important; }}");
        }
        TextTransform::Capitalize => {
            let _ = writeln!(css, "body {{ text-transform: capitalize !important; }}");
        }
    }
}

fn is_disallowed_control(b: u8) -> bool {
    (b < 0x20 && !matches!(b, b'\n' | b'\r' | b'\t')) || b == 0x7F
}

fn push_sanitized_slice(
    out: &mut String,
    slice: &str,
    findings: &mut Vec<CssFinding>,
    line: usize,
) {
    for ch in slice.chars() {
        if ch.is_control() && !matches!(ch, '\n' | '\r' | '\t') {
            findings.push(CssFinding {
                line,
                reason: CssBlockReason::ControlCharacter,
            });
        } else {
            out.push(ch);
        }
    }
}

fn is_ident_start(input: &str, idx: usize) -> bool {
    let bytes = input.as_bytes();
    if idx >= bytes.len() {
        return false;
    }
    let b = bytes[idx];
    b.is_ascii_alphabetic() || b == b'_' || b == b'-' || b == b'\\' || b >= 0x80
}

fn is_ident_continue(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'_' || b == b'-' || b >= 0x80
}

/// Scans a CSS identifier starting at `idx`, decoding CSS escapes (`\XX ` or `\c`).
fn scan_css_ident(input: &str, mut idx: usize, mut line: usize) -> (String, usize, usize) {
    let bytes = input.as_bytes();
    let mut decoded = String::new();
    while idx < bytes.len() {
        let b = bytes[idx];
        if b == b'\\' {
            if let Some((ch, next_i, next_line)) = decode_css_escape(input, idx, line) {
                decoded.push(ch);
                idx = next_i;
                line = next_line;
                continue;
            }
            break;
        }
        if is_ident_continue(b) {
            let ch = input[idx..].chars().next().unwrap_or(' ');
            decoded.push(ch);
            idx += ch.len_utf8();
        } else {
            break;
        }
    }
    (decoded, idx, line)
}

/// Decodes a CSS escape sequence starting at `\` (`bytes[idx] == b'\\'`).
fn decode_css_escape(input: &str, idx: usize, mut line: usize) -> Option<(char, usize, usize)> {
    let bytes = input.as_bytes();
    if idx + 1 >= bytes.len() {
        return None;
    }
    let next = bytes[idx + 1];
    if next == b'\n' || next == b'\r' {
        return None;
    }
    if next.is_ascii_hexdigit() {
        let mut val: u32 = 0;
        let mut pos = idx + 1;
        let mut digits = 0;
        while pos < bytes.len() && digits < 6 && bytes[pos].is_ascii_hexdigit() {
            val = (val << 4) | hex_val(bytes[pos]);
            pos += 1;
            digits += 1;
        }
        // Optional single trailing whitespace after hex escape.
        if pos < bytes.len() {
            match bytes[pos] {
                b' ' | b'\t' | 0x0C => pos += 1,
                b'\n' => {
                    line += 1;
                    pos += 1;
                }
                b'\r' => {
                    line += 1;
                    pos += 1;
                    if pos < bytes.len() && bytes[pos] == b'\n' {
                        pos += 1;
                    }
                }
                _ => {}
            }
        }
        let ch = if val == 0 || val > 0x10_FFFF {
            '\u{FFFD}'
        } else {
            char::from_u32(val).unwrap_or('\u{FFFD}')
        };
        Some((ch, pos, line))
    } else {
        let ch = input[idx + 1..].chars().next()?;
        Some((ch, idx + 1 + ch.len_utf8(), line))
    }
}

fn hex_val(b: u8) -> u32 {
    match b {
        b'0'..=b'9' => u32::from(b - b'0'),
        b'a'..=b'f' => u32::from(b - b'a' + 10),
        b'A'..=b'F' => u32::from(b - b'A' + 10),
        _ => 0,
    }
}

/// Skips an `@import ...;` at-rule up to its terminating `;`, balanced `{...}` block, or EOF.
fn skip_at_rule(input: &str, mut i: usize, mut line: usize) -> (usize, usize) {
    let bytes = input.as_bytes();
    let mut paren_depth: usize = 0;
    let mut brace_depth: usize = 0;

    while i < bytes.len() {
        let b = bytes[i];
        if b == b'\n' {
            line += 1;
            i += 1;
            continue;
        }
        if b == b'/' && i + 1 < bytes.len() && bytes[i + 1] == b'*' {
            i += 2;
            while i < bytes.len() {
                if bytes[i] == b'\n' {
                    line += 1;
                }
                if bytes[i] == b'*' && i + 1 < bytes.len() && bytes[i + 1] == b'/' {
                    i += 2;
                    break;
                }
                i += 1;
            }
            continue;
        }
        if b == b'"' || b == b'\'' {
            let quote = b;
            i += 1;
            while i < bytes.len() {
                if bytes[i] == b'\n' {
                    line += 1;
                    break;
                }
                if bytes[i] == b'\\' {
                    i += 1;
                    if i < bytes.len() {
                        if bytes[i] == b'\n' {
                            line += 1;
                        }
                        i += 1;
                    }
                    continue;
                }
                if bytes[i] == quote {
                    i += 1;
                    break;
                }
                i += 1;
            }
            continue;
        }
        match b {
            b'(' => paren_depth += 1,
            b')' => paren_depth = paren_depth.saturating_sub(1),
            b'{' => brace_depth += 1,
            b'}' => {
                if brace_depth <= 1 {
                    return (i + 1, line);
                }
                brace_depth -= 1;
            }
            b';' if paren_depth == 0 && brace_depth == 0 => return (i + 1, line),
            _ => {}
        }
        i += 1;
    }
    (i, line)
}

/// Consumes a parenthesized call starting right after `(` and returns
/// `(inner_slice, index_after_closing_paren, updated_line)`.
fn consume_balanced_parens(input: &str, start: usize, mut line: usize) -> (&str, usize, usize) {
    let bytes = input.as_bytes();
    let mut i = start;
    let mut depth: usize = 1;

    while i < bytes.len() {
        let b = bytes[i];
        if b == b'\n' {
            line += 1;
            i += 1;
            continue;
        }
        if b == b'/' && i + 1 < bytes.len() && bytes[i + 1] == b'*' {
            i += 2;
            while i < bytes.len() {
                if bytes[i] == b'\n' {
                    line += 1;
                }
                if bytes[i] == b'*' && i + 1 < bytes.len() && bytes[i + 1] == b'/' {
                    i += 2;
                    break;
                }
                i += 1;
            }
            continue;
        }
        if b == b'"' || b == b'\'' {
            let quote = b;
            i += 1;
            while i < bytes.len() {
                if bytes[i] == b'\n' {
                    line += 1;
                    break;
                }
                if bytes[i] == b'\\' {
                    i += 1;
                    if i < bytes.len() {
                        if bytes[i] == b'\n' {
                            line += 1;
                        }
                        i += 1;
                    }
                    continue;
                }
                if bytes[i] == quote {
                    i += 1;
                    break;
                }
                i += 1;
            }
            continue;
        }
        if b == b'\\' {
            i += 2;
            continue;
        }
        if b == b'(' {
            depth += 1;
        } else if b == b')' {
            depth -= 1;
            if depth == 0 {
                return (&input[start..i], i + 1, line);
            }
        }
        i += 1;
    }
    (&input[start..i], i, line)
}

/// Returns `Some("#id")` if the inner contents of `url(...)` represent a safe,
/// fragment-only local SVG reference (no remote/relative/data/blob/file URLs).
fn extract_safe_fragment(inner: &str) -> Option<String> {
    let stripped = strip_comments_and_trim(inner)?;
    if stripped.is_empty() {
        return None;
    }

    let decoded = if (stripped.starts_with('"') && stripped.ends_with('"') && stripped.len() >= 2)
        || (stripped.starts_with('\'') && stripped.ends_with('\'') && stripped.len() >= 2)
    {
        let quote = stripped.as_bytes()[0];
        let body = &stripped[1..stripped.len() - 1];
        // Reject extra unescaped quotes inside the string (e.g. `url("#a" "http://evil")`).
        decode_css_string_strict(body, quote)?
    } else {
        // Unquoted URL: decode escapes and reject any whitespace/quotes/parens.
        decode_css_unquoted_strict(&stripped)?
    };

    let fragment = decoded.trim();
    if !fragment.starts_with('#') || fragment.len() < 2 {
        return None;
    }
    if fragment[1..].chars().any(|ch| {
        ch.is_control()
            || ch.is_whitespace()
            || matches!(ch, '"' | '\'' | '(' | ')' | '\\' | '/' | ':' | '#')
    }) {
        return None;
    }
    Some(fragment.to_owned())
}

fn strip_comments_and_trim(input: &str) -> Option<String> {
    let bytes = input.as_bytes();
    let mut out = String::with_capacity(input.len());
    let mut i = 0;
    let mut in_quote: Option<u8> = None;

    while i < bytes.len() {
        let b = bytes[i];
        if let Some(q) = in_quote {
            if b == b'\\' && i + 1 < bytes.len() {
                out.push('\\');
                let ch = input[i + 1..].chars().next()?;
                out.push(ch);
                i += 1 + ch.len_utf8();
                continue;
            }
            if b == q {
                in_quote = None;
            }
            let ch = input[i..].chars().next()?;
            out.push(ch);
            i += ch.len_utf8();
            continue;
        }
        if b == b'/' && i + 1 < bytes.len() && bytes[i + 1] == b'*' {
            i += 2;
            let mut closed = false;
            while i < bytes.len() {
                if bytes[i] == b'*' && i + 1 < bytes.len() && bytes[i + 1] == b'/' {
                    i += 2;
                    closed = true;
                    break;
                }
                i += 1;
            }
            if !closed {
                return None;
            }
            continue;
        }
        if b == b'"' || b == b'\'' {
            in_quote = Some(b);
            out.push(b as char);
            i += 1;
            continue;
        }
        let ch = input[i..].chars().next()?;
        out.push(ch);
        i += ch.len_utf8();
    }
    if in_quote.is_some() {
        return None;
    }
    Some(out.trim().to_owned())
}

fn decode_css_string_strict(body: &str, quote: u8) -> Option<String> {
    let bytes = body.as_bytes();
    let mut out = String::with_capacity(body.len());
    let mut i = 0;
    while i < bytes.len() {
        let b = bytes[i];
        if b == quote {
            return None;
        }
        if b == b'\\' {
            if i + 1 < bytes.len() && matches!(bytes[i + 1], b'\n' | b'\r') {
                i += 2;
                continue;
            }
            let (ch, next_i, _) = decode_css_escape(body, i, 1)?;
            out.push(ch);
            i = next_i;
            continue;
        }
        let ch = body[i..].chars().next()?;
        out.push(ch);
        i += ch.len_utf8();
    }
    Some(out)
}

fn decode_css_unquoted_strict(body: &str) -> Option<String> {
    let bytes = body.as_bytes();
    let mut out = String::with_capacity(body.len());
    let mut i = 0;
    while i < bytes.len() {
        let b = bytes[i];
        if b == b'\\' {
            let (ch, next_i, _) = decode_css_escape(body, i, 1)?;
            out.push(ch);
            i = next_i;
            continue;
        }
        if matches!(b, b'"' | b'\'' | b'(' | b')') || b.is_ascii_whitespace() {
            return None;
        }
        let ch = body[i..].chars().next()?;
        out.push(ch);
        i += ch.len_utf8();
    }
    Some(out)
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
