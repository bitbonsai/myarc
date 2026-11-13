---
title: Your data
summary: "Where myArc keeps your workspace on disk, how to back it up, and what it sends over the network. There is no account."
area: Your workspace
order: 3
---

myArc has no account and nothing to sign in to. Everything it saves stays on your Mac.

## Where it lives

myArc keeps its data in this folder:

```text
~/Library/Application Support/myArc Nightly/
```

Inside it, each Profile has its own folder. The first is called `Default`, later ones `Profile 1`, `Profile 2` and so on. A Profile folder holds Chromium's usual data, such as history, cookies and extensions, plus one myArc file, `myarc-workspace-v1.json`. That file is your workspace, meaning the Spaces of that Profile with their names, icons and colors, your pinned tabs and Favorites, your temporary tabs and Archived Tabs. It's plain JSON you can open in a text editor.

When myArc saves the workspace, it writes a new file and swaps it in, so a save that gets interrupted leaves the previous version in place.

Saved passwords and cookies stay in Chromium's own stores. Chromium encrypts them with a key it keeps in your macOS Keychain, under the name myArc Nightly Safe Storage.

## Back it up

There is no export command yet. To back up everything, quit myArc and copy the whole `myArc Nightly` folder somewhere safe. Quitting first means nothing is halfway through a save while you copy.

## What goes over the network

myArc adds no analytics and no usage tracking. It makes one kind of request of its own. When a pinned tab or Favorite has no icon yet, myArc asks that site for its icon. The request goes only to that site's domain, carries no cookies and never sends the path of the page you saved. No Google icon service or other third party is involved.

Every other request comes from Chromium itself. myArc turns off a few of the ones that go to Google:

- Chromium no longer asks Google's servers for the current time.
- Domain Reliability reports, which Chromium sends Google when a connection to one of its sites fails, are off.
- WebRTC text logs, which video calls can upload for debugging, are off by default.

Crash report uploads are off too. Crash reports stay on your Mac. [Known limitations](../known-limitations/) says where to find them.

## Agent control

Agents and scripts can't touch myArc until you turn on Allow agents to control myArc in Settings > Advanced. Until then myArc writes nothing to its control folder. [CLI and MCP](../cli-and-mcp/) covers how it works.
