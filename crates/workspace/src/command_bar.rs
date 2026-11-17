//! Actions typed in the command bar, as in Arc. Typing an action's name, or a
//! word it goes by, offers it above the page results; a leading "/" lists only
//! actions. Each action runs a command the menus already run, so this module
//! only decides which actions to offer and in what order. It holds no state
//! and never touches live pages.

use crate::SpaceId;

/// The most query characters matched; longer text matches nothing.
pub const MAX_QUERY_CHARS: usize = 256;

/// The fewest characters that offer an action without the "/" prefix, so a
/// single typed letter keeps the bar for pages.
pub const MIN_TYPED_CHARS: usize = 2;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Action {
    PinTab,
    UnpinTab,
    NewSpace,
    ArchiveTab,
    CopyUrl,
    SplitView,
    /// Shows another Space of this Profile.
    SwitchSpace(SpaceId),
    /// Back to the selected Pin's or Favorite's saved URL (BC-003).
    ResetTab,
    ReplacePinnedUrl,
    /// Arc's View menu: clear the current site's cookies, or its cache, and
    /// reload the page.
    ClearCookies,
    ClearCache,
    /// Collapse Pinned Tabs, or Expand Pinned Tabs while they are collapsed.
    TogglePinsCollapsed,
    /// Expand Current Split: the focused pane takes most of the window.
    ExpandSplit,
}

/// What the selected row is.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Selected {
    /// No row, or an empty page.
    Nothing,
    Today,
    Pin,
    Favorite,
}

/// The window's state when the bar opens, which decides which actions apply.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Context {
    pub selected: Selected,
    /// The selected row has a live page in this window.
    pub has_page: bool,
    /// The selected page sits in a split.
    pub in_split: bool,
    /// The selected Pin's or Favorite's page left its saved URL.
    pub navigated_away: bool,
    /// The window's Pins are collapsed.
    pub pins_collapsed: bool,
}

/// One offered action and the title its row shows.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Offer {
    pub action: Action,
    pub title: String,
}

struct Entry {
    action: Action,
    title: String,
    /// Other words the action answers to, beyond its title's.
    aliases: &'static [&'static str],
}

/// The actions that apply in `context`, in the order "/" lists them. Spaces
/// are `(id, name)` in footer order; the current one is left out.
fn entries(context: &Context, spaces: &[(SpaceId, &str)], current: SpaceId) -> Vec<Entry> {
    let page = context.has_page && context.selected != Selected::Nothing;
    let saved = matches!(context.selected, Selected::Pin | Selected::Favorite);
    let mut entries = Vec::new();
    let mut add = |on: bool, action, title: &str, aliases| {
        if on {
            entries.push(Entry {
                action,
                title: title.to_owned(),
                aliases,
            });
        }
    };
    add(
        page && context.selected == Selected::Today,
        Action::PinTab,
        "Pin Tab",
        &["save"],
    );
    add(
        context.selected == Selected::Pin,
        Action::UnpinTab,
        "Unpin Tab",
        &["pin", "remove"],
    );
    add(
        saved && context.navigated_away,
        Action::ResetTab,
        "Reset Tab",
        &["back", "pinned", "url", "saved"],
    );
    add(
        saved && context.navigated_away,
        Action::ReplacePinnedUrl,
        "Replace Pinned URL with Current",
        &["update", "save"],
    );
    add(page, Action::ArchiveTab, "Archive Tab", &["close", "tab"]);
    add(page, Action::CopyUrl, "Copy URL", &["link", "address"]);
    add(
        page && context.selected == Selected::Today && !context.in_split,
        Action::SplitView,
        "Add Split View",
        &["split"],
    );
    add(
        page && context.in_split,
        Action::ExpandSplit,
        "Expand Current Split",
        &["maximize", "focus"],
    );
    add(
        page,
        Action::ClearCookies,
        "Clear Cookies and Refresh",
        &["site", "data", "reload"],
    );
    add(
        page,
        Action::ClearCache,
        "Clear Cache and Refresh",
        &["site", "reload"],
    );
    if context.pins_collapsed {
        add(
            true,
            Action::TogglePinsCollapsed,
            "Expand Pinned Tabs",
            &["show", "pins"],
        );
    } else {
        add(
            true,
            Action::TogglePinsCollapsed,
            "Collapse Pinned Tabs",
            &["hide", "pins"],
        );
    }
    add(true, Action::NewSpace, "New Space", &["create", "add"]);
    for &(id, name) in spaces {
        if id != current {
            entries.push(Entry {
                action: Action::SwitchSpace(id),
                title: format!("Switch to {name}"),
                aliases: &["space", "go"],
            });
        }
    }
    entries
}

/// Lowercase words, split on anything that is not a letter or digit.
fn words(text: &str) -> Vec<String> {
    text.split(|c: char| !c.is_alphanumeric())
        .filter(|word| !word.is_empty())
        .map(str::to_lowercase)
        .collect()
}

/// How well `query` names `entry`: 2 when the title starts with it, 1 when
/// each query word starts a word of the title or an alias, else 0.
fn score(entry: &Entry, query: &[String], typed: &str) -> u8 {
    if entry.title.to_lowercase().starts_with(typed) {
        return 2;
    }
    let mut known = words(&entry.title);
    known.extend(entry.aliases.iter().map(|alias| (*alias).to_owned()));
    let all = query.iter().all(|word| {
        known
            .iter()
            .any(|candidate| candidate.starts_with(word.as_str()))
    });
    u8::from(all)
}

/// The actions `query` offers. With a leading "/" every applicable action
/// lists, narrowed by the text after it. Without it, text of at least
/// `MIN_TYPED_CHARS` characters offers the actions it names, the closest first.
/// Ties keep the "/" order. Space names come from the caller, bounded there.
pub fn offers(
    query: &str,
    context: &Context,
    spaces: &[(SpaceId, &str)],
    current: SpaceId,
) -> Vec<Offer> {
    if query.chars().count() > MAX_QUERY_CHARS {
        return Vec::new();
    }
    let trimmed = query.trim_start();
    let (listing, text) = match trimmed.strip_prefix('/') {
        Some(rest) => (true, rest.trim()),
        None => (false, trimmed.trim_end()),
    };
    let typed = text.to_lowercase();
    if !listing && typed.chars().count() < MIN_TYPED_CHARS {
        return Vec::new();
    }
    let query_words = words(&typed);
    if !listing && query_words.is_empty() {
        return Vec::new();
    }
    let mut scored: Vec<(u8, Entry)> = entries(context, spaces, current)
        .into_iter()
        .map(|entry| {
            let value = if query_words.is_empty() {
                1
            } else {
                score(&entry, &query_words, &typed)
            };
            (value, entry)
        })
        .filter(|(value, _)| *value > 0)
        .collect();
    // Stable, so equal scores keep the listing order.
    scored.sort_by_key(|(value, _)| std::cmp::Reverse(*value));
    scored
        .into_iter()
        .map(|(_, entry)| Offer {
            action: entry.action,
            title: entry.title,
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    const TODAY: Context = Context {
        selected: Selected::Today,
        has_page: true,
        in_split: false,
        navigated_away: false,
        pins_collapsed: false,
    };
    const SPACES: &[(SpaceId, &str)] = &[(1, "Work"), (2, "Reading"), (3, "Home")];

    fn titles(query: &str, context: &Context) -> Vec<String> {
        offers(query, context, SPACES, 1)
            .into_iter()
            .map(|offer| offer.title)
            .collect()
    }

    #[test]
    fn slash_lists_every_action_that_applies() {
        assert_eq!(
            titles("/", &TODAY),
            [
                "Pin Tab",
                "Archive Tab",
                "Copy URL",
                "Add Split View",
                "Clear Cookies and Refresh",
                "Clear Cache and Refresh",
                "Collapse Pinned Tabs",
                "New Space",
                "Switch to Reading",
                "Switch to Home",
            ]
        );
        // The current Space is not offered.
        assert!(!titles("/", &TODAY).contains(&"Switch to Work".to_owned()));
    }

    #[test]
    fn slash_narrows_by_the_text_after_it() {
        assert_eq!(
            titles("/ sp", &TODAY),
            [
                "Add Split View",
                "New Space",
                "Switch to Reading",
                "Switch to Home"
            ]
        );
        assert_eq!(titles("/copy", &TODAY), ["Copy URL"]);
        assert!(titles("/zzz", &TODAY).is_empty());
    }

    #[test]
    fn typed_names_offer_actions() {
        assert_eq!(titles("pin", &TODAY), ["Pin Tab", "Collapse Pinned Tabs"]);
        assert_eq!(titles("new space", &TODAY), ["New Space"]);
        assert_eq!(titles("archive", &TODAY), ["Archive Tab"]);
        assert_eq!(titles("close", &TODAY), ["Archive Tab"]);
        assert_eq!(titles("copy url", &TODAY), ["Copy URL"]);
        assert_eq!(titles("split", &TODAY), ["Add Split View"]);
        assert_eq!(titles("Reading", &TODAY), ["Switch to Reading"]);
        assert_eq!(titles("  HOME ", &TODAY), ["Switch to Home"]);
    }

    #[test]
    fn a_title_prefix_ranks_first() {
        // "sw" starts both Switch titles; "Add Split View" only matches a word.
        assert_eq!(
            titles("sw", &TODAY),
            ["Switch to Reading", "Switch to Home"]
        );
        let space = titles("space", &TODAY);
        assert_eq!(space.first().map(String::as_str), Some("New Space"));
        assert!(space.contains(&"Switch to Home".to_owned()));
    }

    #[test]
    fn short_or_unrelated_text_offers_nothing() {
        assert!(titles("p", &TODAY).is_empty());
        assert!(titles("", &TODAY).is_empty());
        assert!(titles("example.com", &TODAY).is_empty());
        assert!(titles("--", &TODAY).is_empty());
        assert!(titles(&"a".repeat(MAX_QUERY_CHARS + 1), &TODAY).is_empty());
        assert!(titles(&format!("/{}", "a".repeat(MAX_QUERY_CHARS)), &TODAY).is_empty());
    }

    #[test]
    fn actions_follow_the_selection() {
        let pin = Context {
            selected: Selected::Pin,
            ..TODAY
        };
        assert_eq!(titles("pin", &pin), ["Unpin Tab", "Collapse Pinned Tabs"]);
        assert!(titles("split", &pin).is_empty());
        let favorite = Context {
            selected: Selected::Favorite,
            ..TODAY
        };
        assert!(titles("unpin", &favorite).is_empty());
        assert_eq!(titles("pin tab", &favorite), ["Collapse Pinned Tabs"]);
        let split = Context {
            in_split: true,
            ..TODAY
        };
        assert_eq!(titles("split", &split), ["Expand Current Split"]);
        let empty = Context {
            selected: Selected::Nothing,
            has_page: false,
            in_split: false,
            navigated_away: false,
            pins_collapsed: false,
        };
        assert_eq!(
            titles("/", &empty),
            [
                "Collapse Pinned Tabs",
                "New Space",
                "Switch to Reading",
                "Switch to Home"
            ]
        );
    }

    #[test]
    fn reset_waits_for_a_saved_page_to_leave_its_url() {
        let pin = Context {
            selected: Selected::Pin,
            ..TODAY
        };
        assert!(titles("reset", &pin).is_empty());
        let away = Context {
            navigated_away: true,
            ..pin
        };
        assert_eq!(titles("reset", &away), ["Reset Tab"]);
        assert_eq!(
            titles("replace", &away),
            ["Replace Pinned URL with Current"]
        );
        let favorite = Context {
            selected: Selected::Favorite,
            ..away
        };
        assert_eq!(titles("reset tab", &favorite), ["Reset Tab"]);
        // A Today tab has no saved URL to go back to.
        let today = Context {
            navigated_away: true,
            ..TODAY
        };
        assert!(titles("reset", &today).is_empty());
    }

    #[test]
    fn page_menu_extras_follow_the_page_and_window() {
        assert_eq!(
            titles("clear", &TODAY),
            ["Clear Cookies and Refresh", "Clear Cache and Refresh"]
        );
        assert_eq!(titles("cookies", &TODAY), ["Clear Cookies and Refresh"]);
        assert_eq!(titles("cache", &TODAY), ["Clear Cache and Refresh"]);
        assert_eq!(titles("collapse", &TODAY), ["Collapse Pinned Tabs"]);
        let collapsed = Context {
            pins_collapsed: true,
            ..TODAY
        };
        assert_eq!(titles("expand", &collapsed), ["Expand Pinned Tabs"]);
        assert!(titles("collapse", &collapsed).is_empty());
        // No split, no Expand Current Split; no page, nothing to clear.
        assert!(titles("expand current", &TODAY).is_empty());
        let blank = Context {
            has_page: false,
            ..TODAY
        };
        assert!(titles("clear", &blank).is_empty());
        let split = Context {
            in_split: true,
            ..TODAY
        };
        assert_eq!(titles("expand", &split), ["Expand Current Split"]);
    }

    #[test]
    fn switch_carries_the_space_id() {
        let offer = offers("home", &TODAY, SPACES, 1);
        assert_eq!(offer.len(), 1);
        assert_eq!(offer[0].action, Action::SwitchSpace(3));
    }
}
