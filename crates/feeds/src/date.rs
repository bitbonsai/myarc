//! Lightweight RFC 2822 and ISO 8601 / RFC 3339 date formatting for feeds.

const MONTHS: [&str; 12] = [
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
];

/// Parses common RSS (RFC 2822) and Atom/JSON Feed (ISO 8601) dates into a
/// clean display string such as `23 Nov 2026`. Falls back to a trimmed slice
/// of the raw input if the format is unrecognized.
pub fn format_display_date(raw: &str) -> Option<String> {
    let trimmed = raw.trim();
    if trimmed.is_empty() {
        return None;
    }
    if let Some((y, m, d)) = parse_iso8601_date(trimmed).or_else(|| parse_rfc2822_date(trimmed)) {
        let month_name = MONTHS[(m - 1) as usize];
        return Some(format!("{d} {month_name} {y}"));
    }
    let clean: String = trimmed.chars().take(48).collect();
    Some(clean)
}

/// Parses `YYYY-MM-DD...` (ISO 8601 / RFC 3339).
fn parse_iso8601_date(s: &str) -> Option<(u16, u8, u8)> {
    let bytes = s.as_bytes();
    if bytes.len() < 10 || bytes[4] != b'-' || bytes[7] != b'-' {
        return None;
    }
    let y: u16 = s[0..4].parse().ok()?;
    let m: u8 = s[5..7].parse().ok()?;
    let d: u8 = s[8..10].parse().ok()?;
    if (1900..=2999).contains(&y) && (1..=12).contains(&m) && (1..=31).contains(&d) {
        Some((y, m, d))
    } else {
        None
    }
}

/// Parses `[Day,] DD Mon YYYY ...` (RFC 2822 / RFC 822).
fn parse_rfc2822_date(s: &str) -> Option<(u16, u8, u8)> {
    let after_comma = match s.find(',') {
        Some(idx) => s[idx + 1..].trim_start(),
        None => s,
    };
    let mut parts = after_comma.split_whitespace();
    let day_str = parts.next()?;
    let month_str = parts.next()?;
    let year_str = parts.next()?;

    let d: u8 = day_str.parse().ok()?;
    let m = parse_month_name(month_str)?;
    let mut y: u16 = year_str.parse().ok()?;
    if y < 100 {
        y += if y >= 70 { 1900 } else { 2000 };
    }
    if (1900..=2999).contains(&y) && (1..=31).contains(&d) {
        Some((y, m, d))
    } else {
        None
    }
}

fn parse_month_name(s: &str) -> Option<u8> {
    if s.len() < 3 {
        return None;
    }
    let prefix = &s[..3];
    for (idx, name) in MONTHS.iter().enumerate() {
        if prefix.eq_ignore_ascii_case(name) {
            return Some((idx + 1) as u8);
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn formats_rfc2822_dates() {
        assert_eq!(
            format_display_date("Mon, 23 Nov 2026 00:00:00 GMT").as_deref(),
            Some("23 Nov 2026")
        );
        assert_eq!(
            format_display_date("5 Oct 2026 12:30:00 +0200").as_deref(),
            Some("5 Oct 2026")
        );
    }

    #[test]
    fn formats_iso8601_dates() {
        assert_eq!(
            format_display_date("2026-11-23T14:30:00Z").as_deref(),
            Some("23 Nov 2026")
        );
        assert_eq!(
            format_display_date("2025-02-19").as_deref(),
            Some("19 Feb 2025")
        );
    }
}
