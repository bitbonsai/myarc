# Library Media and Archived Tabs Filter Implementation Plan

Date: 2026-11-25
Status: Proposed for review before implementation
Scope: `Library -> Media` (`Section::kMedia`) and `Library -> Archived Tabs` filter (`Section::kArchive`) in [browser/foundation/library_view.cc](../browser/foundation/library_view.cc)

## Goal

Enable the `Media` section in `LibraryView` with explicit per-Profile opt-in, local-only macOS media discovery, a 2-column thumbnail grid with floating filename hover pills, native file drag-out, and a lightbox modal with `Open in Preview`. Add the `Filter` button beside `Search Archive...` in `Section::kArchive` to narrow archived tabs by Space and time range.

## 1. Opt-In Local Media Discovery and Privacy Model (macOS)

### Explicit Per-Profile Opt-In

Media scanning stays off until the user clicks `Show Media` in the Library.

- **Preference**: Add `browser/foundation/media_prefs.{h,cc}` defining `kMediaLibraryEnabledPref = "myarc.library.media_enabled"` (boolean, default `false`), registered in `chrome/browser/ui/browser_ui_prefs.cc` inside `RegisterBrowserUserPrefs` beside `myarc::RegisterBlockerProfilePrefs`.
- **Profile isolation**: Each Profile stores its own opt-in flag. Off-the-record Profiles (Incognito and Guest) default to `false` and never inherit the regular Profile's opt-in; opting in during a private session stays in memory only or remains disabled.
- **Revocation**: Users can turn off Media from the lightbox `...` menu (`Hide Media in Library`) or the `Media` rail item context menu, which sets the pref back to `false`, stops directory watchers, and clears in-memory thumbnail caches immediately.
- **Zero network access**: Discovery, metadata reads, and thumbnail generation run strictly on local file paths. No file bytes, filenames, or thumbnails ever leave the machine or cross a network socket.

### Local Directory Sources (`browser/foundation/media_library_mac.mm`)

A dedicated `MediaLibraryService` (`browser/foundation/media_library_service.{h,cc}`, `base::SupportsUserData::Data` keyed on `Profile&`, UI-sequence API) coordinates discovery on a background `base::ThreadPool` sequenced task runner (`MayBlock()`, `TaskPriority::USER_VISIBLE`):

1. **macOS Screenshot folder**: Read `com.apple.screencapture` `location` via `CFPreferencesCopyAppValue` (expanding `~` if present). Fall back to `~/Desktop` when unset or invalid.
2. **Downloads folder**: Resolve via `Profile`'s `DownloadPrefs::DownloadPath()` (falling back to `base::DIR_USER_DOWNLOADS` / `~/Downloads`).
3. **Deduplication**: Canonicalize resolved directory paths (`base::MakeAbsoluteFilePath`) so that if Screenshots is set to `~/Downloads`, it is scanned once.
4. **Shallow, symlink-safe scan**: Enumerate immediate files (depth 1, skip hidden dotfiles, skip symlinks pointing outside the watched folder, skip directories and package bundles) to keep scans fast and predictable.
5. **Allowed extensions**:
   - Images: `.png`, `.jpg`, `.jpeg`, `.webp`, `.gif`, `.heic`, `.heif`, `.tiff`, `.tif`, `.bmp`
   - Videos: `.mov`, `.mp4`, `.m4v`, `.webm`
6. **Hard bounds**:
   - `kMaxScanEntries = 2000` files inspected per directory before sorting by `mtime` descending.
   - `kMaxMediaItems = 120` items retained in memory and exposed to `LibraryView`.
   - `kMaxPreviewDecodeBytes = 64 * 1024 * 1024` (64 MiB) file size guard before full-resolution lightbox decode.
7. **Live updates**: While the Library `Media` section is open and opted in, attach `base::FilePathWatcher` (non-recursive) to each scanned directory with a 300 ms debounce timer. Stop watchers when the Library closes or switches away from `Section::kMedia`.
8. **macOS TCC permissions**: Clicking `Show Media` triggers the first scan on Desktop/Downloads, which lets macOS show its standard folder-access prompt on user intent rather than at startup. If access is denied (`EPERM`/`EACCES`), return a structured permission-denied state with an inline `Open System Settings` action (`x-apple.systempreferences:com.apple.preference.security?Privacy_FilesAndFolders`) instead of silently showing an empty list.

### Async Thumbnail and Preview Decoding

All image and video decoding happens off the UI thread with generation tokens so stale callbacks after a refresh or section switch are discarded:

- **Grid thumbnails (280x280 px @ 2x, aspect-fit)**:
  - Use macOS `QLThumbnailGenerator` (`QuickLookThumbnailing.framework`) as the primary generator for both images and videos (handles HEIC, screenshots, and video first frames natively with hardware acceleration and bounded memory).
  - Fall back to `CGImageSourceCreateThumbnailAtIndex` (`ImageIO.framework`, `kCGImageSourceCreateThumbnailFromImageAlways`, `kCGImageSourceThumbnailMaxPixelSize = 280`, `kCGImageSourceCreateThumbnailWithTransform = true`) for raster images if QuickLook returns an error.
  - Cache decoded `gfx::ImageSkia` thumbnails in a bounded LRU cache (`kMaxCachedThumbnails = 120`, keyed by `{path, mtime, file_size}`), invalidated on file change or opt-out.
- **Lightbox full preview**:
  - Decode on demand when the lightbox opens or steps to a neighbor, capped at `kMaxLightboxDimension = 2048` px via `CGImageSourceCreateThumbnailAtIndex` (respecting EXIF orientation).
  - Pre-decode `index - 1` and `index + 1` in the background for crisp `< >` stepping.
  - For videos, render a high-resolution `AVAssetImageGenerator` frame with a centered play badge that launches the video in Preview / QuickTime via `NSWorkspace` (keeping video playback out of the browser UI process).

## 2. Delegate Contract and Data Structures

Extend [browser/foundation/sidebar_view.h](../browser/foundation/sidebar_view.h) with minimal presentation types and delegate methods:

```cpp
struct MediaItemPresentation {
  uint64_t id = 0;           // Stable hash of canonical path + mtime
  std::u16string filename;
  base::FilePath path;       // Local path for drag-out and Preview
  base::Time modified;
  bool is_video = false;
  ui::ImageModel thumbnail;  // Empty while async decode is in flight
  bool operator==(const MediaItemPresentation&) const = default;
};
```

Delegate methods on `SidebarView::Delegate` (implemented in `BrowserView` delegating to `MediaLibraryService`):

- `bool IsMediaLibraryEnabled() const`
- `void SetMediaLibraryEnabled(bool enabled)`
- `std::vector<MediaItemPresentation> GetMediaItems() const`
- `void SetMediaLibraryShown(bool shown)` (starts/stops watchers and refresh notifications, mirroring `SetDownloadsStackShown`)
- `void DragMediaItem(uint64_t id)` (initiates native file drag via `NSDraggingSession` / `DragFile` helper, matching `DragDownload`)
- `void OpenMediaInPreview(uint64_t id)` (opens file with `NSWorkspace`, preferring `com.apple.Preview` for images and default handler for videos)
- `void RevealMediaInFinder(uint64_t id)` (`activateFileViewerSelectingURLs:`)
- `void CopyMediaFile(uint64_t id)` (writes `NSURL` file reference and, for raster images, image data to `NSPasteboard`)
- `void RequestMediaPreview(uint64_t id, base::OnceCallback<void(gfx::ImageSkia)> callback)`

Add `RefreshMedia()` on `LibraryView` and `RefreshLibraryMedia()` on `SidebarView`, matching `RefreshDownloads()` / `RefreshLibraryDownloads()`.

For `ArchivedTabPresentation`, add `uint64_t space_id = 0;` alongside the existing `std::u16string space;` so the Archive Space filter matches reliably even when two Spaces share a name or a Space is renamed.

## 3. UI Architecture in `LibraryView`

### Enabling `Section::kMedia`

- In `browser/foundation/library_view.cc`, update `IsBuilt(Section)` to allow `Section::kMedia` (only `Section::kBoosts` remains gated until Boosts lands).
- Add right-click context menu support on the `Media` rail item (`Hide Media in Library`, enabled when opted in).
- Enable `search_` when `section_ == Section::kMedia` and `IsMediaLibraryEnabled()` is true; keep `search_` disabled in the opt-out empty state.

### Opt-In Empty State (Reference Screenshot 1)

When `section_ == Section::kMedia` and `!delegate_->IsMediaLibraryEnabled()`:

- Render a non-interactive skeleton list behind the card: muted section headers (`Today`, `Yesterday`, `Earlier This Week`) and rounded translucent placeholder pills (`colors.tile` at reduced alpha) filling the pane height.
- Center a floating dark opt-in card (`~248 DIP` wide, 14 DIP corner radius, dark elevated fill with a subtle periwinkle/pink radial ambient shadow behind it):
  - Top-right `×` close button (`kPeekCloseIcon`): dismisses the floating card to a minimal centered `Show Media` button in the pane (never enables scanning).
  - Duotone `kMediaActiveIcon` (36 DIP).
  - Title: `Your Media in the Library` (semibold).
  - Body: `Quickly grab screenshots, images, and videos from your computer by showing them in the myArc Library!`
  - Primary CTA button: `Show Media`, full width, 36 DIP tall, 8 DIP radius, linear horizontal gradient (`#E8A6E8` to `#5B73FF`), white semibold label. Pressing it posts `delegate_->SetMediaLibraryEnabled(true)`.
  - Privacy line at bottom: small lock icon (`kPeekLockIcon`, 12 DIP) + muted text: `Media never leaves your computer and is not viewable by anyone but you.`

### 2-Column Thumbnail Grid (Reference Screenshots 2 and 3)

When opted in and `media_items_` is non-empty (after `library::MatchesSearch(item.filename, query_)`):

- Use a vertical `views::ScrollView` inside `list_` containing a 2-column grid container (`views::TableLayout` or custom 2-column `BoxLayout` rows with 8 DIP column and row gaps inside the 289 DIP inner pane width, yielding ~140 DIP wide tiles and ~120 DIP tile height).
- Each `MediaTile` (`TileButton` subclass):
  - Rounded 10 DIP background (`colors.column` at rest, `colors.tile` on hover/focus).
  - Centered aspect-fit thumbnail framed by a 2 DIP white border and subtle shadow so dark terminal screenshots read clearly against the dark pane.
  - Skeleton shimmer/placeholder rect while `thumbnail.IsEmpty()`.
  - Centered white play triangle badge when `is_video` is true.
  - **Click**: opens `MediaLightbox` at the clicked item's index in the filtered list.
  - **Drag-out**: on `OnMouseDragged` past `views::View::ExceededDragThreshold`, calls `delegate_->DragMediaItem(item.id)` to hand the file to macOS native drag-and-drop (works with web `<input type="file">`, rich editors, Finder, and external apps).
  - **Context menu**: `Open in Preview`, `Copy`, `Reveal in Finder`.

### Floating Hover Filename Pill (Reference Screenshots 2 and 3)

- Add a lightweight `MediaFilenamePill` non-activating popup `views::Widget` owned by `LibraryView` (following `FooterTooltip` in `browser/foundation/footer_button.h` so the scroll viewport never clips it).
- Anchored to the bottom-right corner of the hovered `MediaTile` (overlapping the pane right edge, matching Arc's `04-first-login.png` / `06-omarchy-fleet.png` pill).
- Styling: deep indigo pill fill (`#23297A`), full-capsule border radius, 12pt semibold white text, 10 DIP horizontal padding, 4 DIP vertical padding, elided at 220 DIP max width.
- Hides immediately on mouse exit, scroll, drag start, lightbox open, or widget destruction.

### Media Lightbox Modal (Reference Screenshot 4)

Implement `MediaLightbox` (`browser/foundation/media_lightbox.{h,cc}`) as a window-modal child `views::Widget` (or `views::WidgetDelegateView` with dimmed `SkColorSetA(SK_ColorBLACK, 0xB8)` scrim covering the browser window bounds):

- **Header bar (44 DIP tall, elevated dark surface `#26262B`)**:
  - Left controls: `×` close button, `<` previous button, `>` next button (disabled at bounds or when 1 item), filename label (semibold white, tail-elided), relative age label (`library::AgeLabel(item.modified, now)`, muted).
  - Right controls: `...` menu button (`Copy File`, `Reveal in Finder`, separator, `Hide Media in Library`) and `Open in Preview ↗` button (rounded 6 DIP, `colors.tile` fill, calls `delegate_->OpenMediaInPreview(id)`).
- **Content canvas**:
  - Centered aspect-fit image preview from `RequestMediaPreview(id, ...)`, showing the grid thumbnail immediately as a fast placeholder until the high-res decode arrives.
  - For videos, overlays a centered play button that calls `OpenMediaInPreview(id)`.
- **Input and accessibility**:
  - `Esc` or clicking the backdrop scrim closes only the lightbox and restores focus to the originating `MediaTile`.
  - `Left` / `Right` arrow keys navigate between filtered items and update the header and preview.
  - Proper `ax::mojom::Role::kDialog` with accessible title and focus trap inside the modal.

### Archived Tabs Filter Button (Reference Screenshot 5)

In `LibraryView::BuildList()` when `section_ == Section::kArchive`:

- Wrap the search field in a horizontal header row (`spacing = 8 DIP`):
  - `Search Archive...` field (`flex = 1`, existing 36 DIP height and styling).
  - `Filter` button (`views::MdTextButton` or `TileButton`, 36 DIP tall, 10 DIP radius, `colors.tile` fill, filter vector icon + `Filter` label, fixed preferred width ~78 DIP).
- Filter state stored on `LibraryView` (reset on `SelectSection` or explicit `Reset Filters`):
  ```cpp
  struct ArchiveFilter {
    std::optional<uint64_t> space_id;  // std::nullopt = All Spaces
    library::ArchiveTimeRange range = library::ArchiveTimeRange::kAny;
  };
  ```
- `library::ArchiveTimeRange` enum in `browser/foundation/library_logic.h`: `kAny`, `kToday`, `kLast7Days`, `kLast30Days`, paired with pure helper `library::MatchesArchiveFilter(tab_space_id, archived_time, now, filter_space_id, range)`.
- Clicking `Filter` opens a `ui::SimpleMenuModel` via `views::MenuRunner` anchored below the button:
  - Radio/checked Space items: `All Spaces`, then each Space from `spaces_` (plus an `Other / Deleted Spaces` bucket when `archived_` contains tabs with `space_id == 0`).
  - Separator.
  - Radio/checked time items: `Any Time`, `Today`, `Last 7 Days`, `Last 30 Days`.
  - Separator.
  - `Reset Filters` (enabled when `space_id.has_value() || range != kAny`).
- Visual indicator: when any filter is active, tint the `Filter` button background with `SkColorSetA(kAccent, 0x38)` and brighten its icon/label so active narrowing is obvious.
- Empty state copy: when `archived_` is non-empty but `query_` or `archive_filter_` hides all rows, show `No matches` / `No archived tabs match your search or filter.` with a one-click `Clear Filters` button when filters are active.

## 4. Slice-by-Slice Implementation Order

1. **Slice 1: Pure filter logic and Archive Filter UI (`library_logic`, `library_view`, `icons.h`)**
   - Add `ArchiveTimeRange` and `MatchesArchiveFilter` to `browser/foundation/library_logic.{h,cc}` with unit assertions in `browser/foundation/bridge_check.cc`.
   - Add `space_id` to `SidebarView::ArchivedTabPresentation` and populate it in `BrowserView::GetArchivedTabs()`.
   - Add `kFilterIcon` to `browser/foundation/icons.h` and wire the `Filter` button, menu, active styling, and filtered empty state in `LibraryView`.
2. **Slice 2: Per-Profile Media pref and macOS discovery/thumbnail service (`media_prefs`, `media_library_service`, `media_library_mac.mm`)**
   - Register `myarc.library.media_enabled` in `RegisterBrowserUserPrefs` (default `false`, OTR isolated).
   - Implement background directory resolution (`com.apple.screencapture` + Downloads), bounded scan, `FilePathWatcher` with debounce, and async `QLThumbnailGenerator` / `ImageIO` thumbnail + preview decoding.
   - Add unit checks in `bridge_check.cc` for extension filtering, scan bounds, and sort order.
3. **Slice 3: `LibraryView` Media opt-in card and 2-column thumbnail grid**
   - Enable `Section::kMedia` in `LibraryView::IsBuilt()`.
   - Implement the skeleton background + centered opt-in card with gradient `Show Media` CTA and privacy copy.
   - Implement the 2-column `MediaTile` grid with white-bordered thumbnails, video play overlay, search filtering, and `MediaFilenamePill` hover tooltip.
   - Wire `SidebarView::Delegate` Media methods in `BrowserView` and native macOS file drag-out (`DragMediaItem`).
4. **Slice 4: `MediaLightbox` modal and Preview/Finder actions**
   - Build `browser/foundation/media_lightbox.{h,cc}` with scrim backdrop, `< >` stepping, keyboard navigation (`Esc`, `Left`, `Right`), `...` actions menu, and `Open in Preview ↗` (`NSWorkspace`).
   - Add capture switches (guarded by `MYARC_TEST_HOOKS`) to deterministically seed synthetic media items for background screenshot parity checks.

## 5. Verification Checks

### Automated Checks

- `python3 tools/check_docs.py`
- `cargo fmt --all -- --check`
- `rustfmt --check --edition 2024 browser/foundation/bridge.rs`
- `cargo clippy --workspace --all-targets --locked -- -D warnings`
- `cargo test --workspace --locked`
- `python3 tools/export_patches.py --check`
- Build and run `bridge_check` covering `MatchesArchiveFilter` (Space match, `Today` / `Last 7 Days` / `Last 30 Days` boundaries, DST-safe day boundaries) and media filename search/bounds.

### Native Visual and Behavioral Verification

Run background captures via `tools/capture_native.sh` (using synthetic fixture images/videos in a temp directory, never real user files) and compare side by side against the 5 Arc reference screenshots:

1. **Media opt-in empty state**: skeleton list behind centered dark card, `Show Media` gradient button, lock icon privacy caption, search field disabled, zero filesystem reads prior to clicking `Show Media`.
2. **Media grid + hover pill**: 2-column white-bordered thumbnails, video play overlay, floating indigo filename pill extending cleanly past the pane edge without clipping.
3. **Media lightbox**: dimmed backdrop, header with `×`, `< >`, filename, relative age, `...` menu, and `Open in Preview ↗`; verify `Esc` closes only the lightbox and `Left`/`Right` step cleanly.
4. **Drag-out and Preview**: drag a thumbnail onto a local HTML `<input type="file">` test page and confirm the file attaches; click `Open in Preview ↗` and confirm macOS Preview opens the file.
5. **Archived Tabs filter**: verify `Filter` button sits beside `Search Archive...`, filters by Space and time range in combination with search text, highlights when active, and resets cleanly.
6. **Privacy and OTR check**: opt in on Profile A; confirm Profile B and an Incognito window remain opted out and show the opt-in card.
