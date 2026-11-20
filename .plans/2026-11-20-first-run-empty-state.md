# First-run empty state

After the welcome on a fresh start, Arc shows three things: a Favorites drop card, a starter folder, and the command bar open over the empty Space. The user saw it on 2026-11-20 and chose all three for myArc. The copy and the pages are myArc's own.

## What the user sees

This happens once, when the tour ends through Start fresh or Skip tour on a new profile.

1. **Favorites drop card.** A dashed card at the top of the sidebar, where Favorites go, with a star icon, "Drag to add Favorites" and one short line of our own. It has a close ×. Dropping a tab on it makes that tab a Favorite. The card goes away for good once there's a Favorite or after the × is pressed.
2. **Starter folder.** A "myArc Basics" folder in the first Space with five dormant tabs from myarc.dev:
   - `/docs/` (getting started)
   - `/docs/peek/`
   - `/docs/spaces-and-profiles/`
   - `/docs/shortcuts/`
   - `/docs/import-from-arc/`

   Nothing loads until a tab is opened, and the folder deletes like any other.
3. **Command bar open.** The command bar opens on the empty Space with an empty query, listing the starter tabs with "Switch to Tab" and the first one selected. Esc closes it as usual.

After Import from Arc, the folder and the command bar are left out, because the user already has their own tabs. The card shows only if the import brought no Favorites.

## State

The first-run flags live in the workspace state, so a relaunch doesn't show the empty state again. There's one flag for "seeded" and one for "card dismissed". Replaying the tour doesn't reseed.

## Checks

- Rust tests for the seeding rules: fresh start, skip, import with and without Favorites, a relaunch, and a replay.
- Background captures of the sidebar card, the folder and the open command bar, in light and dark.
- A drop on the card that adds a Favorite, using a test hook.
