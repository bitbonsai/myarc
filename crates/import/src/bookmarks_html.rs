//! Writes a workspace's Favorites and Pins as a Netscape bookmarks file, the
//! `<!DOCTYPE NETSCAPE-Bookmark-file-1>` HTML that every browser imports.
//!
//! The file starts with a "Favorites" folder holding the Profile's Favorites,
//! then has one folder per Space named after it. A Space's folder lists its
//! ungrouped Pins first and then its sidebar folders, each as a nested folder
//! with its Pins, as the sidebar shows them. Everything keeps workspace order.
//! Favorites, Spaces and sidebar folders with nothing to list are left out, so
//! an empty Agent Space adds no clutter.
//!
//! Each bookmark is titled with the item's Rename name, else its page title,
//! else its URL. Items without a saved URL are skipped. Titles, folder names
//! and URLs are HTML-escaped, and control characters in them become spaces so
//! one entry stays on one line. The output only grows with the workspace,
//! which the workspace's own limits bound.

use std::fmt::Write as _;

use myarc_workspace::{Item, Placement, WorkspaceSnapshot};

const HEADER: &str = "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n\
<!-- This is an automatically generated file.\n     \
It will be read and overwritten.\n     \
DO NOT EDIT! -->\n\
<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n\
<TITLE>Bookmarks</TITLE>\n\
<H1>Bookmarks</H1>\n";

/// Name of the folder that holds the Profile's Favorites.
pub const FAVORITES_FOLDER: &str = "Favorites";

/// Default file name for the save panel.
pub const DEFAULT_FILE_NAME: &str = "myArc Bookmarks.html";

/// The whole bookmarks file for `snapshot`.
pub fn write(snapshot: &WorkspaceSnapshot) -> String {
    let mut out = String::from(HEADER);
    out.push_str("<DL><p>\n");

    let favorites: Vec<&Item> = snapshot
        .items
        .iter()
        .filter(|item| matches!(item.placement, Placement::Favorite { .. }))
        .filter(|item| url_of(item).is_some())
        .collect();
    if !favorites.is_empty() {
        open_folder(&mut out, 1, FAVORITES_FOLDER);
        for item in favorites {
            bookmark(&mut out, 2, item);
        }
        close_folder(&mut out, 1);
    }

    for space in &snapshot.spaces {
        let pins: Vec<&Item> = snapshot
            .items
            .iter()
            .filter(|item| item.placement == Placement::Pin { space_id: space.id })
            .filter(|item| url_of(item).is_some())
            .collect();
        let folders: Vec<(&str, Vec<&Item>)> = snapshot
            .folders
            .iter()
            .filter(|folder| folder.space_id == space.id)
            .map(|folder| {
                let items = folder
                    .items
                    .iter()
                    .filter_map(|id| pins.iter().find(|item| item.id == *id).copied())
                    .collect::<Vec<_>>();
                (folder.name.as_str(), items)
            })
            .filter(|(_, items)| !items.is_empty())
            .collect();
        let ungrouped: Vec<&Item> =
            pins.iter()
                .filter(|item| {
                    !snapshot.folders.iter().any(|folder| {
                        folder.space_id == space.id && folder.items.contains(&item.id)
                    })
                })
                .copied()
                .collect();
        if ungrouped.is_empty() && folders.is_empty() {
            continue;
        }
        open_folder(&mut out, 1, &space.name);
        for item in ungrouped {
            bookmark(&mut out, 2, item);
        }
        for (name, items) in folders {
            open_folder(&mut out, 2, name);
            for item in items {
                bookmark(&mut out, 3, item);
            }
            close_folder(&mut out, 2);
        }
        close_folder(&mut out, 1);
    }

    out.push_str("</DL><p>\n");
    out
}

fn url_of(item: &Item) -> Option<&str> {
    item.canonical_url.as_deref().filter(|url| !url.is_empty())
}

fn indent(out: &mut String, depth: usize) {
    for _ in 0..depth {
        out.push_str("    ");
    }
}

fn open_folder(out: &mut String, depth: usize, name: &str) {
    indent(out, depth);
    let _ = writeln!(out, "<DT><H3>{}</H3>", escape(name));
    indent(out, depth);
    out.push_str("<DL><p>\n");
}

fn close_folder(out: &mut String, depth: usize) {
    indent(out, depth);
    out.push_str("</DL><p>\n");
}

fn bookmark(out: &mut String, depth: usize, item: &Item) {
    let Some(url) = url_of(item) else {
        return;
    };
    let title = [item.name.as_str(), item.title.as_str()]
        .into_iter()
        .find(|text| !text.trim().is_empty())
        .unwrap_or(url);
    indent(out, depth);
    let _ = writeln!(out, "<DT><A HREF=\"{}\">{}</A>", escape(url), escape(title));
}

/// HTML-escapes `text` for an attribute or element body.
fn escape(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    for c in text.chars() {
        match c {
            '&' => out.push_str("&amp;"),
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '"' => out.push_str("&quot;"),
            '\'' => out.push_str("&#39;"),
            c if c.is_control() => out.push(' '),
            c => out.push(c),
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use myarc_workspace::{Folder, Space, Workspace};

    use super::*;

    fn space(id: u64, name: &str) -> Space {
        Space {
            id,
            profile_id: 1,
            name: name.into(),
            icon: None,
            theme: None,
            agent: false,
        }
    }

    fn item(id: u64, placement: Placement, url: &str, title: &str) -> Item {
        Item {
            id,
            placement,
            canonical_url: Some(url.into()),
            title: title.into(),
            name: String::new(),
            last_viewed_secs: 0,
        }
    }

    fn pin(id: u64, space_id: u64, url: &str, title: &str) -> Item {
        item(id, Placement::Pin { space_id }, url, title)
    }

    fn favorite(id: u64, url: &str, title: &str) -> Item {
        item(id, Placement::Favorite { profile_id: 1 }, url, title)
    }

    fn folder(id: u64, space_id: u64, name: &str, items: &[u64]) -> Folder {
        Folder {
            id,
            space_id,
            name: name.into(),
            open: true,
            items: items.to_vec(),
        }
    }

    fn body(html: &str) -> &str {
        html.strip_prefix(HEADER).expect("header")
    }

    #[test]
    fn empty_workspace_writes_an_empty_list() {
        let html = write(&Workspace::default().snapshot());
        assert!(html.starts_with("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n"));
        assert_eq!(body(&html), "<DL><p>\n</DL><p>\n");
    }

    #[test]
    fn favorites_come_first_then_each_space_with_folders_after_loose_pins() {
        let mut snapshot = Workspace::default().snapshot();
        snapshot.spaces = vec![space(1, "Work"), space(2, "Home")];
        snapshot.items = vec![
            pin(10, 2, "https://home.example/", "Home"),
            pin(1, 1, "https://a.example/", "A"),
            favorite(2, "https://mail.example/", "Mail"),
            pin(3, 1, "https://b.example/", "B"),
            pin(4, 1, "https://c.example/", "C"),
            pin(5, 1, "https://d.example/", "D"),
            favorite(6, "https://cal.example/", "Calendar"),
        ];
        snapshot.folders = vec![folder(1, 1, "Docs", &[5, 3]), folder(2, 1, "Empty", &[])];
        assert_eq!(
            body(&write(&snapshot)),
            "<DL><p>
    <DT><H3>Favorites</H3>
    <DL><p>
        <DT><A HREF=\"https://mail.example/\">Mail</A>
        <DT><A HREF=\"https://cal.example/\">Calendar</A>
    </DL><p>
    <DT><H3>Work</H3>
    <DL><p>
        <DT><A HREF=\"https://a.example/\">A</A>
        <DT><A HREF=\"https://c.example/\">C</A>
        <DT><H3>Docs</H3>
        <DL><p>
            <DT><A HREF=\"https://d.example/\">D</A>
            <DT><A HREF=\"https://b.example/\">B</A>
        </DL><p>
    </DL><p>
    <DT><H3>Home</H3>
    <DL><p>
        <DT><A HREF=\"https://home.example/\">Home</A>
    </DL><p>
</DL><p>
"
        );
    }

    #[test]
    fn spaces_without_pins_and_temporary_tabs_are_left_out() {
        let mut snapshot = Workspace::default().snapshot();
        snapshot.spaces = vec![space(1, "Everyday"), space(2, "Agent"), space(3, "Kept")];
        snapshot.items = vec![
            item(
                1,
                Placement::Temporary { space_id: 1 },
                "https://today.example/",
                "Today",
            ),
            pin(2, 3, "https://kept.example/", "Kept"),
        ];
        snapshot.folders = vec![folder(1, 2, "Nothing", &[])];
        let html = write(&snapshot);
        assert!(!html.contains("Everyday"));
        assert!(!html.contains("Agent"));
        assert!(!html.contains("Favorites"));
        assert!(!html.contains("today.example"));
        assert!(html.contains("<DT><H3>Kept</H3>"));
    }

    #[test]
    fn titles_fall_back_from_name_to_title_to_url() {
        let mut snapshot = Workspace::default().snapshot();
        let mut renamed = favorite(1, "https://a.example/", "Page title");
        renamed.name = "My name".into();
        let mut no_url = favorite(4, "", "Gone");
        no_url.canonical_url = None;
        snapshot.items = vec![
            renamed,
            favorite(2, "https://b.example/", "Page title"),
            favorite(3, "https://c.example/", "  "),
            no_url,
        ];
        let html = write(&snapshot);
        assert!(html.contains("<A HREF=\"https://a.example/\">My name</A>"));
        assert!(html.contains("<A HREF=\"https://b.example/\">Page title</A>"));
        assert!(html.contains("<A HREF=\"https://c.example/\">https://c.example/</A>"));
        assert!(!html.contains("Gone"));
    }

    #[test]
    fn titles_urls_and_names_are_escaped() {
        let mut snapshot = Workspace::default().snapshot();
        snapshot.spaces = vec![space(1, "R&D <team>")];
        snapshot.items = vec![
            pin(
                1,
                1,
                "https://x.example/?a=1&b=\"2\"",
                "<script>alert('x')</script>\nnext",
            ),
            pin(2, 1, "https://y.example/", "Y"),
        ];
        snapshot.folders = vec![folder(1, 1, "\"Q&A\"", &[2])];
        let html = write(&snapshot);
        assert!(html.contains("<DT><H3>R&amp;D &lt;team&gt;</H3>"));
        assert!(html.contains(
            "<A HREF=\"https://x.example/?a=1&amp;b=&quot;2&quot;\">\
             &lt;script&gt;alert(&#39;x&#39;)&lt;/script&gt; next</A>"
        ));
        assert!(html.contains("<DT><H3>&quot;Q&amp;A&quot;</H3>"));
        assert!(!html.contains("<script>"));
    }

    #[test]
    fn folder_entries_that_are_not_pins_of_the_space_are_skipped() {
        let mut snapshot = Workspace::default().snapshot();
        snapshot.spaces = vec![space(1, "One"), space(2, "Two")];
        snapshot.items = vec![
            pin(1, 1, "https://one.example/", "One"),
            pin(2, 2, "https://two.example/", "Two"),
        ];
        // A damaged folder naming another Space's Pin and a missing item.
        snapshot.folders = vec![folder(1, 1, "Mixed", &[2, 99, 1])];
        let html = write(&snapshot);
        assert_eq!(html.matches("https://two.example/").count(), 1);
        assert!(html.contains(
            "        <DT><H3>Mixed</H3>
        <DL><p>
            <DT><A HREF=\"https://one.example/\">One</A>
        </DL><p>
"
        ));
    }
}
