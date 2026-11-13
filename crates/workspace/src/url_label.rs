//! Short readable label for a saved URL, shown on a row until its page has a title.
//! Arc's Markdown export carries no titles, so imported Pins start with only a URL.
//!
//! The label drops the scheme, any credentials, a leading `www.`, the query, the
//! fragment and a trailing slash, and keeps host plus path:
//! `https://www.docs.example.test/start/?a=1` becomes `docs.example.test/start`.
//! Path text stays percent-encoded, so decoding can't produce look-alike characters.
//! URLs without `//` after the scheme, such as `about:blank`, come back unchanged.
//! This is display text only. It is never stored and is not a URL parser.

/// Label for `url`. Falls back to the whole URL when nothing readable is left.
pub fn url_label(url: &str) -> String {
    let Some((_, rest)) = url.split_once("://") else {
        return url.to_owned();
    };
    let end = rest.find(['?', '#']).unwrap_or(rest.len());
    let rest = &rest[..end];
    let (authority, path) = rest.split_at(rest.find('/').unwrap_or(rest.len()));
    let host = authority
        .rsplit_once('@')
        .map_or(authority, |(_, host)| host);
    let host = host.strip_prefix("www.").unwrap_or(host);
    let path = path.trim_end_matches('/');
    let label = format!("{host}{path}");
    if label.is_empty() {
        url.to_owned()
    } else {
        label
    }
}

#[cfg(test)]
mod tests {
    use super::url_label;

    #[test]
    fn keeps_host_and_path() {
        assert_eq!(
            url_label("https://docs.example.test/start"),
            "docs.example.test/start"
        );
        assert_eq!(
            url_label("http://notes.example.test/a/b.html"),
            "notes.example.test/a/b.html"
        );
    }

    #[test]
    fn drops_www_and_trailing_slash() {
        assert_eq!(url_label("https://www.example.test/"), "example.test");
        assert_eq!(
            url_label("https://www.example.test/team/"),
            "example.test/team"
        );
        assert_eq!(url_label("https://www.example.test//"), "example.test");
        assert_eq!(url_label("https://wwwx.example.test/"), "wwwx.example.test");
    }

    #[test]
    fn drops_query_fragment_and_credentials() {
        assert_eq!(
            url_label("https://user:secret@mail.example.test/inbox?u=0#top"),
            "mail.example.test/inbox"
        );
        assert_eq!(url_label("https://example.test#top"), "example.test");
        assert_eq!(url_label("https://example.test?q=a/b"), "example.test");
    }

    #[test]
    fn keeps_port_and_encoding() {
        assert_eq!(
            url_label("http://localhost:8080/caf%C3%A9/"),
            "localhost:8080/caf%C3%A9"
        );
    }

    #[test]
    fn falls_back_to_the_url() {
        assert_eq!(url_label("about:blank"), "about:blank");
        assert_eq!(url_label("https://"), "https://");
        assert_eq!(url_label("https://?q=1"), "https://?q=1");
        assert_eq!(url_label("file:///tmp/notes.txt"), "/tmp/notes.txt");
        assert_eq!(url_label(""), "");
    }
}
