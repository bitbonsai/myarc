# Alpha live check

Run this 10-minute pass on an installed nightly build (`2026.11.22.2` or newer) with a real pointer and keyboard. Each section closes the remaining unchecked boxes under **Required for alpha** in [ALPHA.md](ALPHA.md).

## 1. Little Arc from another app

Closes: **Open with Cmd+Opt+N**

1. Check **Settings → Links** and confirm **Links from other apps open in Little Arc** is on.
2. If Raycast (or another launcher) holds `⌥⌘N`, disable or pause that shortcut temporarily.
3. Click into another app (Finder, Terminal, or Notes) so myArc is not in front.
4. Press `⌥⌘N`.
   - **Expected:** A small Little Arc window opens with no sidebar and the command bar focused, ready to type a URL or search.
5. Open a page (e.g. `https://myarc.dev`), then press `⌘O` (or click **Open in …**).
   - **Expected:** The page moves into your current Space as a Today tab and the Little Arc window closes.

## 2. One-click Arc import

Closes: **Arc import**

1. Start from a clean Profile (or use **Import from Arc** on the welcome screen) on a Mac with Arc installed.
2. Click **Import from Arc** (welcome screen or **Settings → Advanced → Import**).
   - **Expected:** No file picker appears. myArc reads `~/Library/Application Support/Arc/StorableSidebar.json` on its own and opens the preview immediately, showing your Arc profile names, Favorites, Spaces, folders, pinned tabs, and any skipped items.
3. Confirm **Import**.
   - **Expected:** Favorites land in the Favorites grid, Spaces/folders/pins appear with their saved names and titles, extra Arc profiles map to named myArc Profiles, and the post-import report lists what was added and skipped.
4. Run **Settings → Advanced → Import** a second time on the same Arc data.
   - **Expected:** Zero duplicates are added, and Arc's `StorableSidebar.json` is untouched.

## 3. Welcome tour, Back, and Reduce Motion

Closes: **Animated onboarding in the native browser** and **Skip, Back and restart in onboarding**

1. Press `⌃⌥⌘A` (or **Settings → Advanced → Replay**).
   - **Expected:** The desktop dims, the color blob expands into the welcome window, the 3D ribbon plays once with sound (if macOS UI sound effects are on), and **Import from Arc** + **Let's go** appear.
2. Click **Let's go** and step through the coach marks with real keys:
   - Press `⌘T` and open a site.
   - Press `⌘D` to pin it.
   - Click **Back** on a coach mark, confirm it returns to the previous step cleanly, then continue forward.
   - Press `⌃2` to switch Spaces and complete the handoff.
   - **Expected:** Keys reach the browser without coach marks stealing focus; the tour finishes cleanly without opening a second quick-start tab on a replay.
3. Turn on **System Settings → Accessibility → Display → Reduce Motion**, then press `⌃⌥⌘A` again.
   - **Expected:** The welcome fades in without motion scaling; press `Esc` or **Skip tour** to dismiss cleanly.

## 4. Chrome Web Store extension install and restart

Closes: **Extensions**

1. Open the Chrome Web Store page for **Bitwarden** (or another password manager / content blocker).
2. Click **Add to Chrome** and confirm the install dialog.
   - **Expected:** The dialog anchors cleanly, Bitwarden installs, and its icon pins itself automatically.
3. Open a regular page (`https://myarc.dev`) and hover the sidebar URL pill.
   - **Expected:** The Bitwarden icon appears at the right end of the URL pill next to **Copy URL**; bundled uBlock Origin Lite stays unpinned.
4. Click the Bitwarden icon.
   - **Expected:** Its popup opens and the icon stays visible until the popup closes.
5. Quit myArc (`⌘Q`) and relaunch.
   - **Expected:** Bitwarden is still enabled, pinned in the URL pill on hover, and its popup still opens.

## After the pass

Once all four sections pass, check off the five remaining boxes in [ALPHA.md](ALPHA.md) and decide whether to tag and announce the alpha release.
