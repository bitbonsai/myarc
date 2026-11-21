# Arc sidebar file shape

Arc keeps its sidebar in `~/Library/Application Support/Arc/StorableSidebar.json`. [StorableSidebar.json](StorableSidebar.json) is a synthetic counterpart written from public importers, mainly Firefox's [`ArcMigrationUtils.sys.mjs`](https://github.com/mozilla-firefox/firefox/blob/main/browser/components/migration/ArcMigrationUtils.sys.mjs) and Raycast's [`useArcBookmarks.ts`](https://github.com/raycast/extensions/blob/main/extensions/browser-bookmarks/src/hooks/useArcBookmarks.ts). No real Arc file was read. Hosts use reserved `.test` names.

## Shape

Arc writes Swift dictionaries as flat lists that alternate key and value.

- `sidebar.containers` alternates a kind and a container. `{"global": {}}` marks the main sidebar and `{"littleBrowser": {}}` a Little Arc window.
- A container's `items` alternates an id and an item. Each item has an `id`, a `parentID`, `childrenIds`, an optional `title` and `data`: `tab` with a `savedURL` and an optional `savedTitle`, `list` for a folder or `splitView`. A tab's `title` is the name the user gave it in Arc, and `savedTitle` is the page title Arc saved.
- `topAppsContainerIDs` alternates an Arc profile marker and the id of the item holding that profile's Favorites. `{"default": {}}` is the main profile and `{"custom": {"_0": {"directoryBasename": "Profile 1"}}}` another.
- `spaces` alternates an id and a Space. Each Space has a `title`, a `profile` marker and `containerIDs`, which alternates `"pinned"` or `"unpinned"` and an item id.

Arc is built on Chromium, so its `User Data/Local State`, next to the sidebar file, names each profile as Chromium does: `profile.info_cache` maps a profile's directory, such as `Profile 1`, to an entry whose `name` is the name Arc shows. Chromium writes it in `chrome/browser/profiles/profile_attributes_storage.cc`. [User Data/Local State](User%20Data/Local%20State) is a synthetic counterpart.

## What the fixture holds

- Two main-profile Favorites, one with a page title. One Favorite of "Profile 1", and an empty Favorites container for "Profile 2".
- A main-profile Space "Work" with a pinned tab that has a name and a page title, a folder "Projects" holding a titled tab and a nested folder, a Split View of two tabs, an unknown item kind and one Today tab.
- A main-profile Space "Home" whose pinned container lists no `childrenIds`, so its two tabs are found by `parentID`; one is a `file:` URL. It has an unknown container key.
- A "Profile 1" Space "Side Project" with a named, titled tab and a folder "Reading" holding a tab and a nested folder "Later".
- A Little Arc container with one tab and a container of an unknown kind.
- In `User Data/Local State`, the names "Personal" for `Default`, "Side Work" for `Profile 1` and "Empty" for `Profile 2`.

[The tests](../../../crates/import/tests/arc_sidebar.rs) check that the reader splits the file into the main profile and "Profile 1", leaving out "Profile 2", which has nothing. The main profile gets 2 Favorites, Work's 3 pins and its "Projects" folder, and Home's pins; "Profile 1" gets its Favorite and "Side Project" with its "Reading" folder. Names and page titles reach the imported items. With the `Local State`, "Profile 1" maps to a myArc Profile named "Side Work", and without a usable name it keeps "Arc Profile 1". The reader counts 2 Today tabs, 2 nested folders, whose tabs join the outer folder, and 3 unknown parts. "Profile 1" imported into a new Profile holds only its own Space. Importing twice adds nothing and the file stays unchanged.
