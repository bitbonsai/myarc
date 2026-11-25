//! Strict HTTP(S) origin validation, normalization, and exact/subdomain matching.
//!
//! Boosts never grant wildcard or all-site access by default, and never match
//! non-HTTP(S) or privileged browser schemes.

use crate::{BoostsError, MAX_ORIGIN_BYTES};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Scheme {
    Http,
    Https,
}

impl Scheme {
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Http => "http",
            Self::Https => "https",
        }
    }

    pub const fn default_port(self) -> u16 {
        match self {
            Self::Http => 80,
            Self::Https => 443,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ParsedOrigin {
    pub scheme: Scheme,
    /// Lowercase ASCII host (including brackets for IPv6 literals).
    pub host: String,
    /// Effective port (80 for http, 443 for https when omitted).
    pub port: u16,
    /// True when `host` is an IPv4 or IPv6 literal, which never accepts subdomains.
    pub is_ip: bool,
}

impl ParsedOrigin {
    /// Parses and normalizes an HTTP(S) origin string (`scheme://host[:port]`).
    ///
    /// Rejects non-HTTP(S) schemes, credentials, paths, queries, fragments,
    /// wildcards, backslashes, whitespace, control characters, and invalid hosts/ports.
    pub fn parse(raw: &str) -> Result<Self, BoostsError> {
        if raw.is_empty()
            || raw.len() > MAX_ORIGIN_BYTES
            || raw.chars().any(|ch| ch.is_control() || ch.is_whitespace())
            || raw.contains(['@', '?', '#', '\\', '*'])
        {
            return Err(BoostsError::InvalidOrigin);
        }

        let (scheme_raw, authority) = raw.split_once("://").ok_or(BoostsError::InvalidOrigin)?;
        let scheme = if scheme_raw.eq_ignore_ascii_case("https") {
            Scheme::Https
        } else if scheme_raw.eq_ignore_ascii_case("http") {
            Scheme::Http
        } else {
            return Err(BoostsError::InvalidOrigin);
        };

        // Origins must not contain a path (not even a trailing slash).
        if authority.is_empty() || authority.contains('/') {
            return Err(BoostsError::InvalidOrigin);
        }

        let (host_raw, port_raw, is_ip) = if authority.starts_with('[') {
            let end = authority.find(']').ok_or(BoostsError::InvalidOrigin)?;
            let bracketed = &authority[..=end];
            let inside = &authority[1..end];
            if !is_valid_ipv6_literal(inside) {
                return Err(BoostsError::InvalidOrigin);
            }
            let rest = &authority[end + 1..];
            let port_part = if rest.is_empty() {
                None
            } else if let Some(p) = rest.strip_prefix(':') {
                Some(p)
            } else {
                return Err(BoostsError::InvalidOrigin);
            };
            (bracketed, port_part, true)
        } else {
            let mut parts = authority.split(':');
            let host_part = parts.next().ok_or(BoostsError::InvalidOrigin)?;
            let port_part = parts.next();
            if parts.next().is_some() {
                return Err(BoostsError::InvalidOrigin);
            }
            if !is_valid_host(host_part) {
                return Err(BoostsError::InvalidOrigin);
            }
            let is_ipv4 = is_ipv4_literal(host_part);
            (host_part, port_part, is_ipv4)
        };

        let port = match port_raw {
            None => scheme.default_port(),
            Some(p) => {
                if p.is_empty() || !p.bytes().all(|b| b.is_ascii_digit()) {
                    return Err(BoostsError::InvalidOrigin);
                }
                let parsed: u16 = p.parse().map_err(|_| BoostsError::InvalidOrigin)?;
                if parsed == 0 {
                    return Err(BoostsError::InvalidOrigin);
                }
                parsed
            }
        };

        let host = host_raw.to_ascii_lowercase();
        let origin = Self {
            scheme,
            host,
            port,
            is_ip,
        };
        if origin.to_normalized_string().len() > MAX_ORIGIN_BYTES {
            return Err(BoostsError::InvalidOrigin);
        }
        Ok(origin)
    }

    /// Canonical string form, omitting the default port for `http` (80) and `https` (443).
    pub fn to_normalized_string(&self) -> String {
        if self.port == self.scheme.default_port() {
            format!("{}://{}", self.scheme.as_str(), self.host)
        } else {
            format!("{}://{}:{}", self.scheme.as_str(), self.host, self.port)
        }
    }

    /// Host label suitable for Library Boost rows (e.g. `fly.io`, `www.npmjs.com`,
    /// or `127.0.0.1:8080` when a non-default port is used).
    pub fn display_host(&self) -> String {
        if self.port == self.scheme.default_port() {
            self.host.clone()
        } else {
            format!("{}:{}", self.host, self.port)
        }
    }

    /// Returns true if `page` matches `self` under exact or subdomain rules.
    pub fn matches(&self, page: &Self, include_subdomains: bool) -> bool {
        if self.scheme != page.scheme || self.port != page.port {
            return false;
        }
        if self.host == page.host {
            return true;
        }
        if !include_subdomains || self.is_ip || page.is_ip {
            return false;
        }
        // Must be strictly `<label>.<self.host>` (dot boundary prevents look-alike hosts).
        let suffix_len = self.host.len();
        page.host.len() > suffix_len + 1
            && page.host.ends_with(&self.host)
            && page.host.as_bytes()[page.host.len() - suffix_len - 1] == b'.'
    }
}

/// Validates and normalizes an HTTP(S) origin string.
pub fn normalize_origin(raw: &str) -> Result<String, BoostsError> {
    ParsedOrigin::parse(raw).map(|origin| origin.to_normalized_string())
}

/// Returns true when `page_origin` is a valid HTTP(S) origin matching `allowed_origin`.
pub fn origin_matches(allowed_origin: &str, page_origin: &str, include_subdomains: bool) -> bool {
    let Ok(allowed) = ParsedOrigin::parse(allowed_origin) else {
        return false;
    };
    let Ok(page) = ParsedOrigin::parse(page_origin) else {
        return false;
    };
    allowed.matches(&page, include_subdomains)
}

/// Returns the display host label for an origin (e.g. `fly.io` or `localhost:3000`).
pub fn display_host(origin: &str) -> Option<String> {
    ParsedOrigin::parse(origin).ok().map(|o| o.display_host())
}

fn is_valid_host(host: &str) -> bool {
    if host.is_empty() || host.len() > 253 || !host.is_ascii() {
        return false;
    }
    if host.starts_with('.') || host.ends_with('.') {
        return false;
    }
    for label in host.split('.') {
        if label.is_empty() || label.len() > 63 {
            return false;
        }
        if label.starts_with('-') || label.ends_with('-') {
            return false;
        }
        if !label
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b == b'-')
        {
            return false;
        }
    }
    true
}

fn is_ipv4_literal(host: &str) -> bool {
    let mut count = 0usize;
    for part in host.split('.') {
        if part.is_empty() || !part.bytes().all(|b| b.is_ascii_digit()) {
            return false;
        }
        if part.parse::<u8>().is_err() {
            return false;
        }
        count += 1;
    }
    count == 4
}

fn is_valid_ipv6_literal(inside: &str) -> bool {
    !inside.is_empty()
        && inside.len() <= 45
        && inside.contains(':')
        && inside
            .bytes()
            .all(|b| b.is_ascii_hexdigit() || b == b':' || b == b'.')
}
