---
title: Import from Arc
summary: "Export your Spaces from Arc, preview what myArc will add, and import them as pinned tabs. Importing the same file twice adds nothing."
area: Getting started
order: 2
---

myArc reads the file Arc exports and adds your Spaces and their links to the sidebar. Arc stays untouched, and so does the export file.

## Export from Arc

In Arc, open Settings, go to Advanced and choose Export Data. Arc saves a Markdown file that lists each Space by name, with the links under it.

## Import into myArc

1. Press Cmd+, to open Settings and go to the Advanced tab.
2. Click Import from Arc and pick the exported file.
3. Read the preview, then click Import. Cancel closes the preview and writes nothing.

The preview counts the Spaces and pinned tabs the import adds and the pinned tabs already in myArc. It also lists links it leaves out and lines it couldn't read. Nothing loads while you import, and each pinned tab opens its page the first time you click it.

## What comes over

- Each Space in the export becomes a Space with the same name, in Arc's order. If a Space with that name already exists, its links go there.
- Every link under a Space becomes a pinned tab in that Space, in the export's order.
- Arc Favorites and folders become pinned tabs too. The export doesn't mark which links were Favorites, and myArc has no folders yet, so folder names are dropped.
- Only http and https links come over. Anything else shows up under the links left out.

The export holds only Space names and links. Tab titles, Profiles, temporary tabs, archived tabs, themes, history, passwords, cookies, extensions and Boosts aren't in it, so they don't come over. Imported Spaces join the Profile you're using. [Spaces and Profiles](../spaces-and-profiles/) explains the difference.

myArc refuses files over 8 MB.

## Import again

Importing the same file a second time adds nothing. myArc reuses Spaces by name and keeps the pinned tabs a Space already has. If you export from Arc again later, the next import adds only the new Spaces and links.

An import never removes anything. If it fails partway, nothing changes, because myArc tries every import on a copy of your workspace before it writes.
