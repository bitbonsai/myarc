---
title: Import from Arc
summary: "Import your Spaces, folders, pinned tabs and Favorites from Arc's sidebar file or its export, preview what myArc will add, and import again without duplicates."
area: Getting started
order: 2
---

myArc reads one of two Arc files and adds your Spaces and their links to the sidebar. Arc stays untouched, and so does the file.

- Arc's sidebar file, `StorableSidebar.json`, keeps your Favorites apart from your pinned tabs, so they arrive as Favorites. Arc writes it on its own, in `~/Library/Application Support/Arc/`. Use this one if Arc is on your Mac.
- Arc's export is a Markdown file that lists each Space by name, with the links under it. To make one, open Settings in Arc, go to Advanced and choose Export Data.

## Import into myArc

1. Press Cmd+, to open Settings and go to the Advanced tab, or click Import from Arc… on the welcome screen.
2. Click Import from Arc…. When Arc's sidebar file is on your Mac, myArc reads it automatically and opens the preview directly. Only when Arc isn't found does myArc show a file picker so you can choose an exported file or sidebar file manually.
3. Read the preview, then click Import. Cancel closes the preview and writes nothing.

The preview counts the Profiles, Favorites, Spaces and pinned tabs the import adds and the pinned tabs already in myArc, and shows how each Arc profile maps to a myArc Profile. It also lists links it leaves out and lines it couldn't read. Nothing loads while you import, and each pinned tab opens its page the first time you click it. When the import finishes, a report counts what was added and each kind of thing it left out or changed.

## What comes over

- Each Space becomes a Space with the same name, in Arc's order. If a Space with that name already exists, its links go there.
- Each folder in a Space becomes a folder with the same name, and its links become pinned tabs inside it, in Arc's order. myArc folders don't nest, so in the sidebar file the links of a folder inside a folder go into the outer one.
- From the sidebar file, Arc's main profile goes into the Profile you're using, and each other Arc profile maps to a myArc Profile with its Arc name (read from Arc's `User Data/Local State`, or named `Arc Profile 1`, `Arc Profile 2` and so on if no name is found), creating that Profile if needed. Favorites of Arc's main profile become Favorites of the Profile you're using; other Arc profiles' Favorites are counted in the report. Each tab keeps its custom name and saved page title.
- From the export, Arc Favorites become pinned tabs at the top of their Space, and every imported Space joins the Profile you're using, because the Markdown export doesn't record Favorites, Profiles or tab titles.
- Only http and https links come over. Anything else shows up under the links left out.

Temporary tabs, archived tabs, themes, history, passwords, cookies, extensions and Boosts don't come over. [Spaces and Profiles](../spaces-and-profiles/) explains how Spaces and Profiles work in myArc.

myArc refuses a sidebar file over 32 MB and an export over 8 MB.

## Import again

Importing the same file a second time adds nothing. myArc reuses Spaces and folders by name and keeps the pinned tabs they already have. Pinned tabs from an import made before myArc had folders move into their folder. If you export from Arc again later, the next import adds only the new Spaces and links.

An import never removes anything. If it fails partway, nothing changes, because myArc tries every import on a copy of your workspace before it writes.
