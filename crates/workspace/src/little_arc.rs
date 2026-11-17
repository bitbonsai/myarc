//! Pure Little Arc rules (CONV-01). Decides where a link opened by another app goes, and
//! which main window "Open in" moves a Little Arc page to. Holds no state and never
//! touches live pages.
//!
//! Callers pass already-canonical URL specs from the browser's own parser; this module
//! only compares strings. It does not parse URLs.

/// Where a link that another app asked the browser to open goes.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ExternalLinkRoute {
    /// Open a new Little Arc window showing the link.
    NewLittleArc,
    /// A Little Arc window already shows this link; bring that one forward. The index is
    /// into the list the caller passed.
    FocusLittleArc(usize),
    /// Open the link as a normal tab in a main window.
    NewTab,
}

fn is_web(scheme: &str) -> bool {
    scheme.eq_ignore_ascii_case("http") || scheme.eq_ignore_ascii_case("https")
}

/// Routes a link from another app. With the Little Arc setting on, a web link opens in
/// Little Arc, and a second request for the link one already shows brings that one
/// forward instead of opening another. Anything else, or every link with the setting off,
/// opens as a normal tab. `open_little_arcs` lists what each open Little Arc window shows,
/// most recently used first, so the first match wins.
pub fn route_external_link(
    little_arc_enabled: bool,
    scheme: &str,
    url: &str,
    open_little_arcs: &[&str],
) -> ExternalLinkRoute {
    if !little_arc_enabled || !is_web(scheme) || url.is_empty() {
        return ExternalLinkRoute::NewTab;
    }
    match open_little_arcs.iter().position(|shown| *shown == url) {
        Some(index) => ExternalLinkRoute::FocusLittleArc(index),
        None => ExternalLinkRoute::NewLittleArc,
    }
}

/// A main window that could take a Little Arc page, as the browser sees it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MainWindow {
    /// Uses the Little Arc window's Profile. A page can't move between Profiles.
    pub same_profile: bool,
    /// Its workspace has loaded, so it has a current Space to file the page in.
    pub workspace_ready: bool,
}

/// Which main window "Open in" moves a Little Arc page to: the most recently used one with
/// the same Profile and a loaded workspace. `windows` is in most recently used order.
/// `None` means there is nowhere to move it, and the control should say so.
pub fn move_target(windows: &[MainWindow]) -> Option<usize> {
    windows
        .iter()
        .position(|window| window.same_profile && window.workspace_ready)
}

#[cfg(test)]
mod tests {
    use super::*;

    const LINK: &str = "https://example.com/a";

    #[test]
    fn web_link_opens_a_new_little_arc() {
        assert_eq!(
            route_external_link(true, "https", LINK, &[]),
            ExternalLinkRoute::NewLittleArc
        );
        assert_eq!(
            route_external_link(
                true,
                "HTTP",
                "http://example.com/",
                &["https://other.test/"]
            ),
            ExternalLinkRoute::NewLittleArc
        );
    }

    #[test]
    fn second_open_of_a_shown_link_focuses_that_window() {
        let open = ["https://other.test/", LINK, LINK];
        assert_eq!(
            route_external_link(true, "https", LINK, &open),
            ExternalLinkRoute::FocusLittleArc(1)
        );
    }

    #[test]
    fn match_is_exact() {
        assert_eq!(
            route_external_link(true, "https", LINK, &["https://example.com/a#b"]),
            ExternalLinkRoute::NewLittleArc
        );
    }

    #[test]
    fn setting_off_opens_a_tab() {
        assert_eq!(
            route_external_link(false, "https", LINK, &[LINK]),
            ExternalLinkRoute::NewTab
        );
    }

    #[test]
    fn non_web_links_open_a_tab() {
        for scheme in ["file", "chrome", "about", "mailto", ""] {
            assert_eq!(
                route_external_link(true, scheme, "file:///tmp/a.html", &[]),
                ExternalLinkRoute::NewTab,
                "{scheme}"
            );
        }
        assert_eq!(
            route_external_link(true, "https", "", &[""]),
            ExternalLinkRoute::NewTab
        );
    }

    #[test]
    fn move_goes_to_most_recent_ready_window_of_the_profile() {
        let other_profile = MainWindow {
            same_profile: false,
            workspace_ready: true,
        };
        let loading = MainWindow {
            same_profile: true,
            workspace_ready: false,
        };
        let ready = MainWindow {
            same_profile: true,
            workspace_ready: true,
        };
        assert_eq!(
            move_target(&[other_profile, loading, ready, ready]),
            Some(2)
        );
        assert_eq!(move_target(&[ready]), Some(0));
    }

    #[test]
    fn move_has_no_target_without_a_ready_window_of_the_profile() {
        let other_profile = MainWindow {
            same_profile: false,
            workspace_ready: true,
        };
        let loading = MainWindow {
            same_profile: true,
            workspace_ready: false,
        };
        assert_eq!(move_target(&[]), None);
        assert_eq!(move_target(&[other_profile, loading]), None);
    }
}
