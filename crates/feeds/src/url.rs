//! Safe URL validation and resolution for untrusted feed links.

use crate::MAX_URL_BYTES;

/// Validates and normalizes an `http://` or `https://` URL, resolving
/// root-relative or scheme-relative references against `base_url` when given.
/// Rejects all other schemes (`javascript:`, `data:`, `file:`, `chrome:`, etc.)
/// and any URL containing ASCII control characters or embedded credentials.
pub fn safe_http_url(raw: &str, base_url: Option<&str>) -> Option<String> {
    let trimmed = raw.trim();
    if trimmed.is_empty() || trimmed.len() > MAX_URL_BYTES {
        return None;
    }
    if trimmed.bytes().any(|b| b < 0x20 || b == 0x7f) {
        return None;
    }

    let resolved = if has_http_scheme(trimmed) {
        trimmed.to_string()
    } else if let Some(rest) = trimmed.strip_prefix("//") {
        let scheme = base_url.and_then(url_scheme).unwrap_or("https");
        format!("{scheme}://{rest}")
    } else if trimmed.starts_with('/') {
        let origin = base_url.and_then(url_origin)?;
        format!("{origin}{trimmed}")
    } else if !trimmed.contains(':') {
        let base_dir = base_url.and_then(url_base_dir)?;
        format!("{base_dir}{trimmed}")
    } else {
        return None;
    };

    if resolved.len() > MAX_URL_BYTES || !is_valid_http_url(&resolved) {
        return None;
    }
    Some(resolved)
}

fn has_http_scheme(url: &str) -> bool {
    let lower_prefix: String = url
        .chars()
        .take(8)
        .map(|c| c.to_ascii_lowercase())
        .collect();
    lower_prefix.starts_with("http://") || lower_prefix.starts_with("https://")
}

fn url_scheme(url: &str) -> Option<&'static str> {
    let lower_prefix: String = url
        .chars()
        .take(8)
        .map(|c| c.to_ascii_lowercase())
        .collect();
    if lower_prefix.starts_with("https://") {
        Some("https")
    } else if lower_prefix.starts_with("http://") {
        Some("http")
    } else {
        None
    }
}

fn is_valid_http_url(url: &str) -> bool {
    let Some(scheme) = url_scheme(url) else {
        return false;
    };
    let rest = &url[scheme.len() + 3..];
    let authority_end = rest.find(['/', '?', '#']).unwrap_or(rest.len());
    let authority = &rest[..authority_end];
    if authority.is_empty() || authority.contains('@') {
        return false;
    }
    if authority.bytes().any(|b| b <= 0x20 || b == 0x7f) {
        return false;
    }
    true
}

fn url_origin(url: &str) -> Option<&str> {
    let scheme = url_scheme(url)?;
    let prefix_len = scheme.len() + 3;
    let rest = &url[prefix_len..];
    let authority_end = rest.find(['/', '?', '#']).unwrap_or(rest.len());
    if authority_end == 0 {
        return None;
    }
    Some(&url[..prefix_len + authority_end])
}

fn url_base_dir(url: &str) -> Option<String> {
    let origin = url_origin(url)?;
    let after_origin = &url[origin.len()..];
    let path_end = after_origin.find(['?', '#']).unwrap_or(after_origin.len());
    let path = &after_origin[..path_end];
    match path.rfind('/') {
        Some(idx) => Some(format!("{origin}{}", &path[..=idx])),
        None => Some(format!("{origin}/")),
    }
}

/// Extracts a clean display host (without `www.`) for UI metadata.
pub fn display_host(url: &str) -> Option<String> {
    let scheme = url_scheme(url)?;
    let rest = &url[scheme.len() + 3..];
    let authority_end = rest.find(['/', '?', '#']).unwrap_or(rest.len());
    let authority = &rest[..authority_end];
    let host = if authority.starts_with('[') {
        authority
    } else {
        authority.split(':').next().unwrap_or(authority)
    };
    if host.is_empty() {
        return None;
    }
    let lower = host.to_ascii_lowercase();
    Some(lower.strip_prefix("www.").unwrap_or(&lower).to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn accepts_http_and_https() {
        assert_eq!(
            safe_http_url("https://mauriciowolff.com/blog/test/", None).as_deref(),
            Some("https://mauriciowolff.com/blog/test/")
        );
        assert_eq!(
            safe_http_url("http://example.com/feed", None).as_deref(),
            Some("http://example.com/feed")
        );
    }

    #[test]
    fn rejects_unsafe_schemes_and_credentials() {
        assert_eq!(safe_http_url("javascript:alert(1)", None), None);
        assert_eq!(safe_http_url("data:text/html,<h1>x</h1>", None), None);
        assert_eq!(safe_http_url("file:///etc/passwd", None), None);
        assert_eq!(safe_http_url("chrome://settings", None), None);
        assert_eq!(safe_http_url("https://user:pass@example.com/", None), None);
    }

    #[test]
    fn resolves_relative_links() {
        let base = Some("https://example.com/blog/rss.xml");
        assert_eq!(
            safe_http_url("/posts/1", base).as_deref(),
            Some("https://example.com/posts/1")
        );
        assert_eq!(
            safe_http_url("post-2.html", base).as_deref(),
            Some("https://example.com/blog/post-2.html")
        );
        assert_eq!(
            safe_http_url("//cdn.example.com/img.png", base).as_deref(),
            Some("https://cdn.example.com/img.png")
        );
    }
}
