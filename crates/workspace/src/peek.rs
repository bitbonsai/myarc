//! Pure Peek routing. Decides where a link navigation from a sidebar item opens, and
//! what the Peek overlay's actions do. Holds no state and never touches live pages.
//!
//! Callers pass already-validated parts: the scheme and a site key per URL. The site key
//! is the registrable domain (e.g. `example.com` for `docs.example.com`) computed with
//! native browser machinery; this module only compares the strings, ASCII
//! case-insensitively. It does not parse URLs or know the public suffix list.
//! An empty site key means unknown and never triggers Peek.

use crate::{ItemId, Placement, SpaceId};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SourceKind {
    Temporary,
    Pinned,
    Favorite,
}

impl From<Placement> for SourceKind {
    fn from(placement: Placement) -> Self {
        match placement {
            Placement::Temporary { .. } => Self::Temporary,
            Placement::Pin { .. } => Self::Pinned,
            Placement::Favorite { .. } => Self::Favorite,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SiteRef<'a> {
    pub scheme: &'a str,
    pub site: &'a str,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ClickModifier {
    None,
    /// Cmd-click: the user explicitly asked for a new tab.
    NewTab,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PeekSettings {
    /// The Advanced setting. When off, cross-site links from pinned/favorite items open in new tabs.
    pub peek_enabled: bool,
}

impl Default for PeekSettings {
    fn default() -> Self {
        Self { peek_enabled: true }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct NavigationRequest<'a> {
    pub source: SourceKind,
    pub from: SiteRef<'a>,
    pub to: SiteRef<'a>,
    pub modifier: ClickModifier,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Disposition {
    Peek,
    CurrentTab,
    NewTab,
}

fn is_web(scheme: &str) -> bool {
    scheme.eq_ignore_ascii_case("http") || scheme.eq_ignore_ascii_case("https")
}

fn cross_site(from: &SiteRef<'_>, to: &SiteRef<'_>) -> bool {
    !from.site.is_empty() && !to.site.is_empty() && !from.site.eq_ignore_ascii_case(to.site)
}

/// Where a navigation opens. Order: explicit modifier, then Peek eligibility, then the setting.
pub fn route_navigation(request: &NavigationRequest<'_>, settings: &PeekSettings) -> Disposition {
    if request.modifier == ClickModifier::NewTab {
        return Disposition::NewTab;
    }
    let eligible = request.source != SourceKind::Temporary
        && is_web(request.from.scheme)
        && is_web(request.to.scheme)
        && cross_site(&request.from, &request.to);
    match (eligible, settings.peek_enabled) {
        (false, _) => Disposition::CurrentTab,
        (true, true) => Disposition::Peek,
        (true, false) => Disposition::NewTab,
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PeekAction {
    /// Cmd+O: expand the Peek page into a real tab.
    OpenInTab,
    /// Outside click, X button or Cmd+W.
    Close,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PeekOutcome {
    /// Create a new Temporary item in this Space for the Peek page, select it and dismiss the overlay.
    PromoteToTemporary { space_id: SpaceId },
    /// Dismiss the overlay and reselect the item Peek was opened from.
    ReturnToSource { source_item: ItemId },
}

pub fn resolve_peek_action(
    action: PeekAction,
    source_item: ItemId,
    current_space: SpaceId,
) -> PeekOutcome {
    match action {
        PeekAction::OpenInTab => PeekOutcome::PromoteToTemporary {
            space_id: current_space,
        },
        PeekAction::Close => PeekOutcome::ReturnToSource { source_item },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const ON: PeekSettings = PeekSettings { peek_enabled: true };
    const OFF: PeekSettings = PeekSettings {
        peek_enabled: false,
    };

    fn site<'a>(scheme: &'a str, site: &'a str) -> SiteRef<'a> {
        SiteRef { scheme, site }
    }

    fn request<'a>(
        source: SourceKind,
        from: SiteRef<'a>,
        to: SiteRef<'a>,
    ) -> NavigationRequest<'a> {
        NavigationRequest {
            source,
            from,
            to,
            modifier: ClickModifier::None,
        }
    }

    fn cross<'a>(source: SourceKind) -> NavigationRequest<'a> {
        request(
            source,
            site("https", "example.com"),
            site("https", "other.org"),
        )
    }

    #[test]
    fn cross_site_from_pinned_and_favorite_opens_peek() {
        assert_eq!(
            route_navigation(&cross(SourceKind::Pinned), &ON),
            Disposition::Peek
        );
        assert_eq!(
            route_navigation(&cross(SourceKind::Favorite), &ON),
            Disposition::Peek
        );
    }

    #[test]
    fn temporary_source_stays_in_place() {
        assert_eq!(
            route_navigation(&cross(SourceKind::Temporary), &ON),
            Disposition::CurrentTab
        );
        assert_eq!(
            route_navigation(&cross(SourceKind::Temporary), &OFF),
            Disposition::CurrentTab
        );
    }

    #[test]
    fn same_site_stays_in_place() {
        let same = request(
            SourceKind::Pinned,
            site("https", "example.com"),
            site("https", "example.com"),
        );
        assert_eq!(route_navigation(&same, &ON), Disposition::CurrentTab);
        assert_eq!(route_navigation(&same, &OFF), Disposition::CurrentTab);
    }

    #[test]
    fn same_site_comparison_ignores_ascii_case_and_scheme_upgrade() {
        let upgraded = request(
            SourceKind::Pinned,
            site("http", "Example.COM"),
            site("HTTPS", "example.com"),
        );
        assert_eq!(route_navigation(&upgraded, &ON), Disposition::CurrentTab);
    }

    #[test]
    fn subdomains_share_a_site_only_when_caller_passes_registrable_domain() {
        // docs.example.com -> www.example.com, both reduced to example.com by the caller.
        let reduced = request(
            SourceKind::Pinned,
            site("https", "example.com"),
            site("https", "example.com"),
        );
        assert_eq!(route_navigation(&reduced, &ON), Disposition::CurrentTab);
        // Unreduced hosts differ as strings, so they count as cross-site.
        let raw = request(
            SourceKind::Pinned,
            site("https", "docs.example.com"),
            site("https", "www.example.com"),
        );
        assert_eq!(route_navigation(&raw, &ON), Disposition::Peek);
    }

    #[test]
    fn similar_looking_sites_are_different() {
        let lookalike = request(
            SourceKind::Pinned,
            site("https", "example.com"),
            site("https", "example.com.evil.net"),
        );
        assert_eq!(route_navigation(&lookalike, &ON), Disposition::Peek);
        let prefix = request(
            SourceKind::Pinned,
            site("https", "example.com"),
            site("https", "notexample.com"),
        );
        assert_eq!(route_navigation(&prefix, &ON), Disposition::Peek);
    }

    #[test]
    fn non_web_schemes_stay_in_place() {
        for scheme in [
            "about",
            "chrome",
            "file",
            "javascript",
            "data",
            "mailto",
            "",
        ] {
            let to = request(
                SourceKind::Pinned,
                site("https", "example.com"),
                site(scheme, "other.org"),
            );
            assert_eq!(
                route_navigation(&to, &ON),
                Disposition::CurrentTab,
                "to {scheme:?}"
            );
            let from = request(
                SourceKind::Pinned,
                site(scheme, "example.com"),
                site("https", "other.org"),
            );
            assert_eq!(
                route_navigation(&from, &ON),
                Disposition::CurrentTab,
                "from {scheme:?}"
            );
        }
    }

    #[test]
    fn unknown_site_never_peeks() {
        let empty_to = request(
            SourceKind::Pinned,
            site("https", "example.com"),
            site("https", ""),
        );
        let empty_from = request(
            SourceKind::Pinned,
            site("https", ""),
            site("https", "other.org"),
        );
        let both_empty = request(SourceKind::Pinned, site("https", ""), site("https", ""));
        for r in [empty_to, empty_from, both_empty] {
            assert_eq!(route_navigation(&r, &ON), Disposition::CurrentTab);
        }
    }

    #[test]
    fn setting_off_opens_cross_site_in_new_tab() {
        assert_eq!(
            route_navigation(&cross(SourceKind::Pinned), &OFF),
            Disposition::NewTab
        );
        assert_eq!(
            route_navigation(&cross(SourceKind::Favorite), &OFF),
            Disposition::NewTab
        );
    }

    #[test]
    fn new_tab_modifier_always_wins() {
        for source in [
            SourceKind::Temporary,
            SourceKind::Pinned,
            SourceKind::Favorite,
        ] {
            for settings in [ON, OFF] {
                let mut cross_request = cross(source);
                cross_request.modifier = ClickModifier::NewTab;
                assert_eq!(
                    route_navigation(&cross_request, &settings),
                    Disposition::NewTab
                );
                let mut same = request(source, site("https", "a.com"), site("https", "a.com"));
                same.modifier = ClickModifier::NewTab;
                assert_eq!(route_navigation(&same, &settings), Disposition::NewTab);
                let mut non_web = request(source, site("https", "a.com"), site("about", ""));
                non_web.modifier = ClickModifier::NewTab;
                assert_eq!(route_navigation(&non_web, &settings), Disposition::NewTab);
            }
        }
    }

    #[test]
    fn default_settings_enable_peek() {
        assert!(PeekSettings::default().peek_enabled);
    }

    #[test]
    fn source_kind_maps_from_placement() {
        assert_eq!(
            SourceKind::from(Placement::Temporary { space_id: 1 }),
            SourceKind::Temporary
        );
        assert_eq!(
            SourceKind::from(Placement::Pin { space_id: 1 }),
            SourceKind::Pinned
        );
        assert_eq!(
            SourceKind::from(Placement::Favorite { profile_id: 1 }),
            SourceKind::Favorite
        );
    }

    #[test]
    fn open_in_tab_promotes_to_temporary_in_current_space() {
        assert_eq!(
            resolve_peek_action(PeekAction::OpenInTab, 7, 3),
            PeekOutcome::PromoteToTemporary { space_id: 3 }
        );
    }

    #[test]
    fn close_returns_to_source_item() {
        assert_eq!(
            resolve_peek_action(PeekAction::Close, 7, 3),
            PeekOutcome::ReturnToSource { source_item: 7 }
        );
        assert_eq!(
            resolve_peek_action(PeekAction::Close, u64::MAX, 0),
            PeekOutcome::ReturnToSource {
                source_item: u64::MAX
            }
        );
    }
}
