# Arc sidebar file shape

Arc keeps its sidebar in `~/Library/Application Support/Arc/StorableSidebar.json`. [StorableSidebar.json](StorableSidebar.json) is a synthetic counterpart written from public importers, mainly Firefox's [`ArcMigrationUtils.sys.mjs`](https://github.com/mozilla-firefox/firefox/blob/main/browser/components/migration/ArcMigrationUtils.sys.mjs) and Raycast's [`useArcBookmarks.ts`](https://github.com/raycast/extensions/blob/main/extensions/browser-bookmarks/src/hooks/useArcBookmarks.ts). No real Arc file was read. Hosts use reserved `.test` names.

## Shape

Arc writes Swift dictionaries as flat lists that alternate key and value.

- `sidebar.containers` alternates a kind and a container. `{"global": {}}` marks the main sidebar and `{"littleBrowser": {}}` a Little Arc window.
- A container's `items` alternates an id and an item. Each item has an `id`, a `parentID`, `childrenIds`, an optional `title` and `data`: `tab` with a `savedURL`, `list` for a folder or `splitView`.
- `topAppsContainerIDs` alternates an Arc profile marker and the id of the item holding that profile's Favorites. `{"default": {}}` is the main profile and `{"custom": {"_0": {"directoryBasename": "Profile 1"}}}` another.
- `spaces` alternates an id and a Space. Each Space has a `title`, a `profile` marker and `containerIDs`, which alternates `"pinned"` or `"unpinned"` and an item id.

## What the fixture holds

- Two main-profile Favorites and one Favorite of another profile.
- A Space "Work" with a pinned tab, a folder "Projects" holding a tab and a nested folder, a Split View of two tabs, an unknown item kind and one Today tab.
- A Space "Home" whose pinned container lists no `childrenIds`, so its two tabs are found by `parentID`; one is a `file:` URL. It has an unknown container key.
- A Little Arc container with one tab and a container of an unknown kind.

[The tests](../../../crates/import/tests/arc_sidebar.rs) check that the reader turns these into 2 Favorites, Work's 3 pins and its "Projects" folder, and Home's pins, and that it counts 2 Today tabs, 1 other-profile Favorite, 1 nested folder and 3 unknown parts. Importing twice adds nothing and the file stays unchanged.
