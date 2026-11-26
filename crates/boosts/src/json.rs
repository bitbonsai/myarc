//! Dependency-free, bounded JSON encoder and decoder for `BoostsSnapshot` and
//! single-Boost export/import.
//!
//! Enforces `MAX_FILE_BYTES` (8 MiB) and `MAX_JSON_DEPTH` (10) before allocating
//! owned structures, and never panics on malformed or adversarial input.

use std::fmt::Write as _;

use crate::{
    Boost, BoostAppearance, BoostPalette, BoostRevision, BoostsError, BoostsSnapshot, ColorMode,
    DEFAULT_TEXT_SCALE_PCT, FontPreset, MAX_FILE_BYTES, MAX_JSON_DEPTH, SNAPSHOT_VERSION,
    TextTransform,
};

/// Encodes a `BoostsSnapshot` as compact, deterministic UTF-8 JSON.
pub fn encode_snapshot(snapshot: &BoostsSnapshot) -> String {
    let mut out = String::new();
    out.push('{');
    let _ = write!(
        out,
        "\"version\":{},\"global_enabled\":{},\"next_boost_id\":{},\"boosts\":[",
        snapshot.version, snapshot.global_enabled, snapshot.next_boost_id
    );
    for (idx, boost) in snapshot.boosts.iter().enumerate() {
        if idx > 0 {
            out.push(',');
        }
        write_boost(&mut out, boost);
    }
    out.push_str("]}");
    out
}

/// Encodes a single `Boost` as UTF-8 JSON for local export.
pub fn encode_boost(boost: &Boost) -> String {
    let mut out = String::new();
    write_boost(&mut out, boost);
    out
}

/// Decodes a `BoostsSnapshot` from UTF-8 JSON bytes, enforcing file size and depth limits.
pub fn decode_snapshot(bytes: &[u8]) -> Result<BoostsSnapshot, BoostsError> {
    if bytes.len() > MAX_FILE_BYTES {
        return Err(BoostsError::InvalidSnapshot("JSON exceeds 8 MiB limit"));
    }
    let text = std::str::from_utf8(bytes)
        .map_err(|_| BoostsError::InvalidSnapshot("JSON is not valid UTF-8"))?;
    let mut parser = Parser::new(text);
    let snapshot = parser.parse_snapshot()?;
    parser.skip_ws();
    if !parser.is_eof() {
        return Err(BoostsError::InvalidSnapshot("trailing bytes after JSON"));
    }
    Ok(snapshot)
}

/// Decodes a single `Boost` from UTF-8 JSON bytes, enforcing file size and depth limits.
pub fn decode_boost(bytes: &[u8]) -> Result<Boost, BoostsError> {
    if bytes.len() > MAX_FILE_BYTES {
        return Err(BoostsError::InvalidSnapshot("JSON exceeds 8 MiB limit"));
    }
    let text = std::str::from_utf8(bytes)
        .map_err(|_| BoostsError::InvalidSnapshot("JSON is not valid UTF-8"))?;
    let mut parser = Parser::new(text);
    let boost = parser.parse_boost()?;
    parser.skip_ws();
    if !parser.is_eof() {
        return Err(BoostsError::InvalidSnapshot("trailing bytes after JSON"));
    }
    Ok(boost)
}

fn write_boost(out: &mut String, boost: &Boost) {
    out.push('{');
    let _ = write!(out, "\"id\":{},\"name\":", boost.id);
    write_json_str(out, &boost.name);
    out.push_str(",\"origins\":[");
    for (i, origin) in boost.origins.iter().enumerate() {
        if i > 0 {
            out.push(',');
        }
        write_json_str(out, origin);
    }
    let _ = write!(
        out,
        "],\"include_subdomains\":{},\"apply_to_subframes\":{},\"enabled\":{},\"version\":{},\"updated_secs\":{},\"appearance\":",
        boost.include_subdomains,
        boost.apply_to_subframes,
        boost.enabled,
        boost.version,
        boost.updated_secs
    );
    write_appearance(out, &boost.appearance);
    out.push_str(",\"hidden_selectors\":[");
    for (i, selector) in boost.hidden_selectors.iter().enumerate() {
        if i > 0 {
            out.push(',');
        }
        write_json_str(out, selector);
    }
    out.push_str("],\"custom_css\":");
    write_json_str(out, &boost.custom_css);
    out.push_str(",\"script_payload\":");
    match &boost.script_payload {
        Some(script) => write_json_str(out, script),
        None => out.push_str("null"),
    }
    out.push_str(",\"revisions\":[");
    for (i, rev) in boost.revisions.iter().enumerate() {
        if i > 0 {
            out.push(',');
        }
        write_revision(out, rev);
    }
    out.push_str("]}");
}

fn write_appearance(out: &mut String, appearance: &BoostAppearance) {
    out.push_str("{\"color_mode\":");
    write_json_str(out, appearance.color_mode.as_str());
    out.push_str(",\"palette\":");
    match appearance.palette {
        Some(p) => {
            let _ = write!(
                out,
                "{{\"background\":{},\"text\":{},\"accent\":{},\"contrast\":{},\"brightness\":{},\"saturation\":{},\"tint_strength\":{}}}",
                p.background,
                p.text,
                p.accent,
                p.contrast,
                p.brightness,
                p.saturation,
                p.tint_strength
            );
        }
        None => out.push_str("null"),
    }
    out.push_str(",\"font\":");
    write_json_str(out, appearance.font.as_str());
    let _ = write!(out, ",\"text_scale_pct\":{}", appearance.text_scale_pct);
    out.push_str(",\"text_transform\":");
    write_json_str(out, appearance.text_transform.as_str());
    out.push('}');
}

fn write_revision(out: &mut String, rev: &BoostRevision) {
    let _ = write!(
        out,
        "{{\"version\":{},\"updated_secs\":{},\"appearance\":",
        rev.version, rev.updated_secs
    );
    write_appearance(out, &rev.appearance);
    out.push_str(",\"hidden_selectors\":[");
    for (i, selector) in rev.hidden_selectors.iter().enumerate() {
        if i > 0 {
            out.push(',');
        }
        write_json_str(out, selector);
    }
    out.push_str("],\"custom_css\":");
    write_json_str(out, &rev.custom_css);
    out.push('}');
}

fn write_json_str(out: &mut String, s: &str) {
    out.push('"');
    for ch in s.chars() {
        match ch {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\x08' => out.push_str("\\b"),
            '\x0C' => out.push_str("\\f"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => {
                let _ = write!(out, "\\u{:04x}", c as u32);
            }
            c => out.push(c),
        }
    }
    out.push('"');
}

struct Parser<'a> {
    src: &'a str,
    bytes: &'a [u8],
    pos: usize,
    depth: usize,
}

impl<'a> Parser<'a> {
    fn new(src: &'a str) -> Self {
        Self {
            src,
            bytes: src.as_bytes(),
            pos: 0,
            depth: 0,
        }
    }

    fn is_eof(&self) -> bool {
        self.pos >= self.bytes.len()
    }

    fn peek(&self) -> Option<u8> {
        self.bytes.get(self.pos).copied()
    }

    fn skip_ws(&mut self) {
        while let Some(b) = self.peek() {
            if matches!(b, b' ' | b'\t' | b'\n' | b'\r') {
                self.pos += 1;
            } else {
                break;
            }
        }
    }

    fn enter(&mut self) -> Result<(), BoostsError> {
        self.depth += 1;
        if self.depth > MAX_JSON_DEPTH {
            return Err(BoostsError::InvalidSnapshot("JSON nesting depth exceeded"));
        }
        Ok(())
    }

    fn leave(&mut self) {
        self.depth = self.depth.saturating_sub(1);
    }

    fn expect_byte(&mut self, expected: u8) -> Result<(), BoostsError> {
        self.skip_ws();
        if self.peek() == Some(expected) {
            self.pos += 1;
            Ok(())
        } else {
            Err(BoostsError::InvalidSnapshot("unexpected JSON token"))
        }
    }

    fn parse_object<F>(&mut self, mut on_field: F) -> Result<(), BoostsError>
    where
        F: FnMut(&mut Self, &str) -> Result<(), BoostsError>,
    {
        self.enter()?;
        self.expect_byte(b'{')?;
        self.skip_ws();
        if self.peek() == Some(b'}') {
            self.pos += 1;
            self.leave();
            return Ok(());
        }
        loop {
            let key = self.parse_string()?;
            self.expect_byte(b':')?;
            on_field(self, &key)?;
            self.skip_ws();
            match self.peek() {
                Some(b',') => {
                    self.pos += 1;
                }
                Some(b'}') => {
                    self.pos += 1;
                    self.leave();
                    return Ok(());
                }
                _ => {
                    return Err(BoostsError::InvalidSnapshot(
                        "expected ',' or '}' in object",
                    ));
                }
            }
        }
    }

    fn parse_array<F>(&mut self, mut on_item: F) -> Result<(), BoostsError>
    where
        F: FnMut(&mut Self) -> Result<(), BoostsError>,
    {
        self.enter()?;
        self.expect_byte(b'[')?;
        self.skip_ws();
        if self.peek() == Some(b']') {
            self.pos += 1;
            self.leave();
            return Ok(());
        }
        loop {
            on_item(self)?;
            self.skip_ws();
            match self.peek() {
                Some(b',') => {
                    self.pos += 1;
                }
                Some(b']') => {
                    self.pos += 1;
                    self.leave();
                    return Ok(());
                }
                _ => return Err(BoostsError::InvalidSnapshot("expected ',' or ']' in array")),
            }
        }
    }

    fn parse_bool(&mut self) -> Result<bool, BoostsError> {
        self.skip_ws();
        if self.src[self.pos..].starts_with("true") {
            self.pos += 4;
            Ok(true)
        } else if self.src[self.pos..].starts_with("false") {
            self.pos += 5;
            Ok(false)
        } else {
            Err(BoostsError::InvalidSnapshot("expected JSON boolean"))
        }
    }

    fn parse_null(&mut self) -> Result<bool, BoostsError> {
        self.skip_ws();
        if self.src[self.pos..].starts_with("null") {
            self.pos += 4;
            Ok(true)
        } else {
            Ok(false)
        }
    }

    fn parse_u64(&mut self) -> Result<u64, BoostsError> {
        self.skip_ws();
        let start = self.pos;
        if self.peek() == Some(b'0') {
            self.pos += 1;
            if self.peek().is_some_and(|b| b.is_ascii_digit()) {
                return Err(BoostsError::InvalidSnapshot("leading zero in JSON number"));
            }
        } else {
            while self.peek().is_some_and(|b| b.is_ascii_digit()) {
                self.pos += 1;
            }
        }
        if self.pos == start {
            return Err(BoostsError::InvalidSnapshot("expected unsigned integer"));
        }
        self.src[start..self.pos]
            .parse::<u64>()
            .map_err(|_| BoostsError::InvalidSnapshot("integer out of range"))
    }

    fn parse_u32(&mut self) -> Result<u32, BoostsError> {
        let v = self.parse_u64()?;
        u32::try_from(v).map_err(|_| BoostsError::InvalidSnapshot("u32 out of range"))
    }

    fn parse_u16(&mut self) -> Result<u16, BoostsError> {
        let v = self.parse_u64()?;
        u16::try_from(v).map_err(|_| BoostsError::InvalidSnapshot("u16 out of range"))
    }

    fn parse_u8(&mut self) -> Result<u8, BoostsError> {
        let v = self.parse_u64()?;
        u8::try_from(v).map_err(|_| BoostsError::InvalidSnapshot("u8 out of range"))
    }

    fn parse_string(&mut self) -> Result<String, BoostsError> {
        self.skip_ws();
        if self.peek() != Some(b'"') {
            return Err(BoostsError::InvalidSnapshot("expected JSON string"));
        }
        self.pos += 1;
        let mut out = String::new();
        while let Some(b) = self.peek() {
            match b {
                b'"' => {
                    self.pos += 1;
                    return Ok(out);
                }
                b'\\' => {
                    self.pos += 1;
                    let esc = self
                        .peek()
                        .ok_or(BoostsError::InvalidSnapshot("unterminated escape"))?;
                    self.pos += 1;
                    match esc {
                        b'"' => out.push('"'),
                        b'\\' => out.push('\\'),
                        b'/' => out.push('/'),
                        b'b' => out.push('\x08'),
                        b'f' => out.push('\x0C'),
                        b'n' => out.push('\n'),
                        b'r' => out.push('\r'),
                        b't' => out.push('\t'),
                        b'u' => {
                            let code = self.parse_hex4()?;
                            if (0xD800..=0xDBFF).contains(&code) {
                                // High surrogate: must be followed by \uXXXX low surrogate.
                                if !self.src[self.pos..].starts_with("\\u") {
                                    return Err(BoostsError::InvalidSnapshot(
                                        "unpaired UTF-16 high surrogate",
                                    ));
                                }
                                self.pos += 2;
                                let low = self.parse_hex4()?;
                                if !(0xDC00..=0xDFFF).contains(&low) {
                                    return Err(BoostsError::InvalidSnapshot(
                                        "invalid UTF-16 low surrogate",
                                    ));
                                }
                                let combined = 0x10000
                                    + ((u32::from(code) - 0xD800) << 10)
                                    + (u32::from(low) - 0xDC00);
                                let ch = char::from_u32(combined).ok_or(
                                    BoostsError::InvalidSnapshot("invalid Unicode scalar"),
                                )?;
                                out.push(ch);
                            } else if (0xDC00..=0xDFFF).contains(&code) {
                                return Err(BoostsError::InvalidSnapshot(
                                    "unpaired UTF-16 low surrogate",
                                ));
                            } else {
                                let ch = char::from_u32(u32::from(code)).ok_or(
                                    BoostsError::InvalidSnapshot("invalid Unicode scalar"),
                                )?;
                                out.push(ch);
                            }
                        }
                        _ => return Err(BoostsError::InvalidSnapshot("invalid JSON escape")),
                    }
                }
                c if c < 0x20 => {
                    return Err(BoostsError::InvalidSnapshot(
                        "unescaped control character in JSON string",
                    ));
                }
                _ => {
                    let ch = self.src[self.pos..]
                        .chars()
                        .next()
                        .ok_or(BoostsError::InvalidSnapshot("unexpected EOF in string"))?;
                    out.push(ch);
                    self.pos += ch.len_utf8();
                }
            }
        }
        Err(BoostsError::InvalidSnapshot("unterminated JSON string"))
    }

    fn parse_hex4(&mut self) -> Result<u16, BoostsError> {
        if self.pos + 4 > self.bytes.len() {
            return Err(BoostsError::InvalidSnapshot("truncated \\uXXXX escape"));
        }
        let slice = &self.src[self.pos..self.pos + 4];
        if !slice.bytes().all(|b| b.is_ascii_hexdigit()) {
            return Err(BoostsError::InvalidSnapshot("invalid \\uXXXX escape"));
        }
        self.pos += 4;
        u16::from_str_radix(slice, 16)
            .map_err(|_| BoostsError::InvalidSnapshot("invalid \\uXXXX escape"))
    }

    fn skip_value(&mut self) -> Result<(), BoostsError> {
        self.skip_ws();
        match self.peek() {
            Some(b'{') => self.parse_object(|p, _| p.skip_value()),
            Some(b'[') => self.parse_array(Self::skip_value),
            Some(b'"') => self.parse_string().map(|_| ()),
            Some(b't') => self.parse_bool().map(|_| ()),
            Some(b'f') => self.parse_bool().map(|_| ()),
            Some(b'n') => {
                if self.parse_null()? {
                    Ok(())
                } else {
                    Err(BoostsError::InvalidSnapshot("invalid literal"))
                }
            }
            Some(b'-' | b'0'..=b'9') => self.skip_number(),
            _ => Err(BoostsError::InvalidSnapshot("unexpected JSON value")),
        }
    }

    fn skip_number(&mut self) -> Result<(), BoostsError> {
        if self.peek() == Some(b'-') {
            self.pos += 1;
        }
        let start = self.pos;
        while self.peek().is_some_and(|b| b.is_ascii_digit()) {
            self.pos += 1;
        }
        if self.pos == start {
            return Err(BoostsError::InvalidSnapshot("invalid JSON number"));
        }
        if self.peek() == Some(b'.') {
            self.pos += 1;
            let frac = self.pos;
            while self.peek().is_some_and(|b| b.is_ascii_digit()) {
                self.pos += 1;
            }
            if self.pos == frac {
                return Err(BoostsError::InvalidSnapshot("invalid JSON fraction"));
            }
        }
        if matches!(self.peek(), Some(b'e' | b'E')) {
            self.pos += 1;
            if matches!(self.peek(), Some(b'+' | b'-')) {
                self.pos += 1;
            }
            let exp = self.pos;
            while self.peek().is_some_and(|b| b.is_ascii_digit()) {
                self.pos += 1;
            }
            if self.pos == exp {
                return Err(BoostsError::InvalidSnapshot("invalid JSON exponent"));
            }
        }
        Ok(())
    }

    fn parse_snapshot(&mut self) -> Result<BoostsSnapshot, BoostsError> {
        let mut version: Option<u32> = None;
        let mut global_enabled: Option<bool> = None;
        let mut next_boost_id: Option<u64> = None;
        let mut boosts: Option<Vec<Boost>> = None;

        self.parse_object(|p, key| match key {
            "version" => {
                let v = p.parse_u32()?;
                if v != SNAPSHOT_VERSION {
                    return Err(BoostsError::UnsupportedSnapshotVersion(v));
                }
                version = Some(v);
                Ok(())
            }
            "global_enabled" => {
                global_enabled = Some(p.parse_bool()?);
                Ok(())
            }
            "next_boost_id" => {
                next_boost_id = Some(p.parse_u64()?);
                Ok(())
            }
            "boosts" => {
                let mut list = Vec::new();
                p.parse_array(|p| {
                    if list.len() >= crate::MAX_BOOSTS {
                        return Err(BoostsError::LimitExceeded("boosts"));
                    }
                    list.push(p.parse_boost()?);
                    Ok(())
                })?;
                boosts = Some(list);
                Ok(())
            }
            _ => p.skip_value(),
        })?;

        let version = version.ok_or(BoostsError::InvalidSnapshot("missing version"))?;
        if version != SNAPSHOT_VERSION {
            return Err(BoostsError::UnsupportedSnapshotVersion(version));
        }

        Ok(BoostsSnapshot {
            version,
            global_enabled: global_enabled.unwrap_or(true),
            boosts: boosts.unwrap_or_default(),
            next_boost_id: next_boost_id
                .ok_or(BoostsError::InvalidSnapshot("missing next_boost_id"))?,
        })
    }

    fn parse_boost(&mut self) -> Result<Boost, BoostsError> {
        let mut id: Option<u64> = None;
        let mut name: Option<String> = None;
        let mut origins: Option<Vec<String>> = None;
        let mut include_subdomains = false;
        let mut apply_to_subframes = false;
        let mut enabled = false;
        let mut version = 1u32;
        let mut updated_secs = 0u64;
        let mut appearance = BoostAppearance::default();
        let mut hidden_selectors = Vec::new();
        let mut custom_css = String::new();
        let mut script_payload: Option<String> = None;
        let mut revisions = Vec::new();

        self.parse_object(|p, key| match key {
            "id" => {
                id = Some(p.parse_u64()?);
                Ok(())
            }
            "name" => {
                name = Some(p.parse_string()?);
                Ok(())
            }
            "origins" => {
                let mut list = Vec::new();
                p.parse_array(|p| {
                    if list.len() >= crate::MAX_ORIGINS_PER_BOOST {
                        return Err(BoostsError::LimitExceeded("origins"));
                    }
                    list.push(p.parse_string()?);
                    Ok(())
                })?;
                origins = Some(list);
                Ok(())
            }
            "include_subdomains" => {
                include_subdomains = p.parse_bool()?;
                Ok(())
            }
            "apply_to_subframes" => {
                apply_to_subframes = p.parse_bool()?;
                Ok(())
            }
            "enabled" => {
                enabled = p.parse_bool()?;
                Ok(())
            }
            "version" => {
                version = p.parse_u32()?;
                Ok(())
            }
            "updated_secs" => {
                updated_secs = p.parse_u64()?;
                Ok(())
            }
            "appearance" => {
                appearance = p.parse_appearance()?;
                Ok(())
            }
            "hidden_selectors" => {
                hidden_selectors.clear();
                p.parse_array(|p| {
                    if hidden_selectors.len() >= crate::MAX_HIDDEN_SELECTORS {
                        return Err(BoostsError::LimitExceeded("hidden selectors"));
                    }
                    hidden_selectors.push(p.parse_string()?);
                    Ok(())
                })?;
                Ok(())
            }
            "custom_css" => {
                custom_css = p.parse_string()?;
                Ok(())
            }
            "script_payload" => {
                if p.parse_null()? {
                    script_payload = None;
                } else {
                    script_payload = Some(p.parse_string()?);
                }
                Ok(())
            }
            "revisions" => {
                revisions.clear();
                p.parse_array(|p| {
                    if revisions.len() >= crate::MAX_REVISIONS {
                        return Err(BoostsError::LimitExceeded("revisions"));
                    }
                    revisions.push(p.parse_revision()?);
                    Ok(())
                })?;
                Ok(())
            }
            _ => p.skip_value(),
        })?;

        Ok(Boost {
            id: id.unwrap_or(0),
            name: name.ok_or(BoostsError::InvalidSnapshot("missing boost name"))?,
            origins: origins.ok_or(BoostsError::InvalidSnapshot("missing boost origins"))?,
            include_subdomains,
            apply_to_subframes,
            enabled,
            version,
            updated_secs,
            appearance,
            hidden_selectors,
            custom_css,
            script_payload,
            revisions,
        })
    }

    fn parse_appearance(&mut self) -> Result<BoostAppearance, BoostsError> {
        let mut color_mode = ColorMode::None;
        let mut palette: Option<BoostPalette> = None;
        let mut font = FontPreset::Default;
        let mut text_scale_pct = DEFAULT_TEXT_SCALE_PCT;
        let mut text_transform = TextTransform::None;

        self.parse_object(|p, key| match key {
            "color_mode" => {
                let s = p.parse_string()?;
                color_mode = ColorMode::parse(&s).ok_or(BoostsError::InvalidAppearance)?;
                Ok(())
            }
            "palette" => {
                if p.parse_null()? {
                    palette = None;
                } else {
                    palette = Some(p.parse_palette()?);
                }
                Ok(())
            }
            "font" => {
                let s = p.parse_string()?;
                font = FontPreset::parse(&s).ok_or(BoostsError::InvalidAppearance)?;
                Ok(())
            }
            "text_scale_pct" => {
                text_scale_pct = p.parse_u16()?;
                Ok(())
            }
            "text_transform" => {
                let s = p.parse_string()?;
                text_transform = TextTransform::parse(&s).ok_or(BoostsError::InvalidAppearance)?;
                Ok(())
            }
            _ => p.skip_value(),
        })?;

        Ok(BoostAppearance {
            color_mode,
            palette,
            font,
            text_scale_pct,
            text_transform,
        })
    }

    fn parse_palette(&mut self) -> Result<BoostPalette, BoostsError> {
        let mut background = 0u32;
        let mut text = 0u32;
        let mut accent = 0u32;
        let mut contrast = 50u8;
        let mut brightness = 50u8;
        let mut saturation = 50u8;
        let mut tint_strength = 50u8;

        self.parse_object(|p, key| match key {
            "background" => {
                background = p.parse_u32()?;
                Ok(())
            }
            "text" => {
                text = p.parse_u32()?;
                Ok(())
            }
            "accent" => {
                accent = p.parse_u32()?;
                Ok(())
            }
            "contrast" => {
                contrast = p.parse_u8()?;
                Ok(())
            }
            "brightness" => {
                brightness = p.parse_u8()?;
                Ok(())
            }
            "saturation" => {
                saturation = p.parse_u8()?;
                Ok(())
            }
            "tint_strength" => {
                tint_strength = p.parse_u8()?;
                Ok(())
            }
            _ => p.skip_value(),
        })?;

        Ok(BoostPalette {
            background,
            text,
            accent,
            contrast,
            brightness,
            saturation,
            tint_strength,
        })
    }

    fn parse_revision(&mut self) -> Result<BoostRevision, BoostsError> {
        let mut version = 1u32;
        let mut updated_secs = 0u64;
        let mut appearance = BoostAppearance::default();
        let mut hidden_selectors = Vec::new();
        let mut custom_css = String::new();

        self.parse_object(|p, key| match key {
            "version" => {
                version = p.parse_u32()?;
                Ok(())
            }
            "updated_secs" => {
                updated_secs = p.parse_u64()?;
                Ok(())
            }
            "appearance" => {
                appearance = p.parse_appearance()?;
                Ok(())
            }
            "hidden_selectors" => {
                hidden_selectors.clear();
                p.parse_array(|p| {
                    if hidden_selectors.len() >= crate::MAX_HIDDEN_SELECTORS {
                        return Err(BoostsError::LimitExceeded("hidden selectors"));
                    }
                    hidden_selectors.push(p.parse_string()?);
                    Ok(())
                })?;
                Ok(())
            }
            "custom_css" => {
                custom_css = p.parse_string()?;
                Ok(())
            }
            _ => p.skip_value(),
        })?;

        Ok(BoostRevision {
            version,
            updated_secs,
            appearance,
            hidden_selectors,
            custom_css,
        })
    }
}
