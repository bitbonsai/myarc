//! Whether a page's host is a local development server, for the quiet marker the
//! browser draws on such pages: `localhost` and `*.localhost`, `*.local`, the IPv4
//! loopback block 127.0.0.0/8, IPv6 `[::1]` and `0.0.0.0`.
//!
//! Callers pass the host of a canonical URL (Chromium's `GURL::host()`), so IPv4 is
//! already dotted-quad and IPv6 keeps its brackets. Matching ignores ASCII case and
//! one trailing dot. Anything else, including an empty host, is not local.

use std::net::{Ipv4Addr, Ipv6Addr};

/// True when `host` names a local development server.
pub fn is_local_dev_host(host: &str) -> bool {
    let host = host.strip_suffix('.').unwrap_or(host);
    if let Some(v6) = host.strip_prefix('[').and_then(|h| h.strip_suffix(']')) {
        return v6.parse::<Ipv6Addr>().is_ok_and(|ip| ip.is_loopback());
    }
    if let Ok(ip) = host.parse::<Ipv4Addr>() {
        return ip.is_loopback() || ip.is_unspecified();
    }
    is_name_or_subdomain(host, "localhost") || has_suffix(host, ".local")
}

fn is_name_or_subdomain(host: &str, name: &str) -> bool {
    host.eq_ignore_ascii_case(name) || has_suffix(host, &format!(".{name}"))
}

// `suffix` starts with a dot, and something must come before it.
fn has_suffix(host: &str, suffix: &str) -> bool {
    host.len() > suffix.len()
        && host
            .get(host.len() - suffix.len()..)
            .is_some_and(|tail| tail.eq_ignore_ascii_case(suffix))
}

#[cfg(test)]
mod tests {
    use super::is_local_dev_host;

    #[test]
    fn localhost_names_are_local() {
        for host in [
            "localhost",
            "LocalHost",
            "localhost.",
            "app.localhost",
            "a.b.localhost",
        ] {
            assert!(is_local_dev_host(host), "{host}");
        }
    }

    #[test]
    fn dot_local_names_are_local() {
        for host in ["printer.local", "my-mac.LOCAL", "a.b.local."] {
            assert!(is_local_dev_host(host), "{host}");
        }
    }

    #[test]
    fn loopback_and_unspecified_addresses_are_local() {
        for host in [
            "127.0.0.1",
            "127.1.2.3",
            "127.255.255.255",
            "0.0.0.0",
            "[::1]",
        ] {
            assert!(is_local_dev_host(host), "{host}");
        }
    }

    #[test]
    fn other_hosts_are_not_local() {
        for host in [
            "",
            ".",
            "local",
            ".local",
            "example.com",
            "localhost.example.com",
            "notlocalhost",
            "mylocal",
            "example.local.test",
            "128.0.0.1",
            "126.255.255.255",
            "10.0.0.1",
            "192.168.1.10",
            "[::]",
            "[::2]",
            "[fe80::1]",
            "::1",
            "[::1",
        ] {
            assert!(!is_local_dev_host(host), "{host}");
        }
    }

    #[test]
    fn multibyte_hosts_do_not_panic() {
        assert!(!is_local_dev_host("é"));
        assert!(!is_local_dev_host("ééééééé"));
    }
}
