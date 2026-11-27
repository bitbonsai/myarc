chromium::import! {
    "//myarc:boosts";
    "//myarc:control";
    "//myarc:import";
    "//myarc:workspace";
}

use std::sync::{Arc, Mutex, MutexGuard, PoisonError};

pub struct Workspace(workspace::Workspace);
pub struct BoostStore(boosts::BoostStore);

// A parsed Arc export, one part per Arc profile. The Markdown export and a
// sidebar file with one profile have a single part, the main profile's.
pub struct ArcExport {
    parts: Vec<ArcPart>,
    error: String,
    // Read from Arc's sidebar file rather than its Markdown export.
    sidebar: bool,
}

// One Arc profile's part of an export, plus one browser-canonicalized URL per
// entry once `set_arc_export_canonical_urls` ran (`None` for a rejected URL),
// and what `apply_arc_import` did with it.
struct ArcPart {
    arc: import::arc_sidebar::ProfileReport,
    canonical: Vec<Option<String>>,
    applied: Option<import::arc_workspace::Preview>,
}

impl ArcPart {
    fn new(arc: import::arc_sidebar::ProfileReport) -> Self {
        Self {
            arc,
            canonical: Vec::new(),
            applied: None,
        }
    }

    // The main profile's part of a Markdown export or an unreadable file.
    fn main(report: import::arc_markdown::Report) -> Self {
        Self::new(import::arc_sidebar::ProfileReport {
            profile: import::arc_sidebar::ArcProfile::Main,
            name: None,
            report,
        })
    }
}

// One request or hello from a control client, or none once the host has
// stopped. A hello waits in `approval` for the user's answer. An interaction
// command also carries its checked action and, once the
// browser began it, the snapshot references it took. `profile` is the tag of
// the Profile it runs in, which its tab handles carry. A Space command that
// changed the workspace waits in `answered` until the change is saved.
//
// Chromium builds Rust with panic=abort, so a panic here would end the
// browser and catch_unwind can't stop it. Request handling therefore never
// unwraps, indexes or slices on request or page data.
pub struct ControlReply {
    pending: Option<control::ipc::Pending>,
    approval: Option<control::ipc::Approval>,
    action: Option<control::actions::PageAction>,
    taken: Option<control::snapshot::Taken>,
    profile: String,
    answered: Option<(control::ipc::Pending, control::Response)>,
}

// All records own their buffers. Calls are synchronous on the browser UI sequence;
// only exported records, never Workspace or native runtime IDs, cross to storage.
#[cxx::bridge(namespace = "myarc::foundation")]
mod ffi {
    struct RenameResult {
        ok: bool,
        name: String,
        error: String,
    }

    struct MutationResult {
        ok: bool,
        id: u64,
        error: String,
    }

    // Sidebar rows selected together, top to bottom, and the row a
    // Shift-click range starts from (0 for none).
    struct RowSelection {
        items: Vec<u64>,
        anchor: u64,
    }

    enum PlacementKind {
        Temporary,
        Pin,
        Favorite,
    }

    // Where a link click from a sidebar item's page opens (BC-005).
    enum PeekRoute {
        Peek,
        CurrentTab,
        NewTab,
    }

    // What the user did on an open Peek.
    enum PeekTrigger {
        ExpandButton,
        CommandO,
        SplitButton,
        CloseButton,
        CommandW,
        Escape,
        OutsideClick,
    }

    // OpenInTab turns the Peek page into a Today tab of the current Space;
    // OpenInSplit puts it in a split beside a Today copy of the source page;
    // Close dismisses it and leaves the workspace as it was.
    // The per-site ad blocking switch for a blocker level (see
    // workspace::blocker). Unknown keeps the switch disabled.
    enum BlockerSwitchState {
        Unknown,
        Off,
        On,
    }

    enum PeekActionKind {
        OpenInTab,
        OpenInSplit,
        Close,
    }

    // Where a link that another app opened goes (CONV-01). FocusLittleArc
    // brings the Little Arc at `index` of the list passed in forward.
    enum LittleArcRouteKind {
        NewLittleArc,
        FocusLittleArc,
        NewTab,
    }

    struct LittleArcRoute {
        kind: LittleArcRouteKind,
        index: usize,
    }

    // A main window that "Open in" could move a Little Arc page to.
    struct LittleArcMainWindow {
        same_profile: bool,
        workspace_ready: bool,
    }

    // A welcome tour event (onboarding.rs). StepDone and SkipStep carry
    // `step`; ImportCommitted carries the receipt in `content_hash` and
    // `space_ids`.
    enum OnboardingEventKind {
        StartBrowsing,
        ImportStarted,
        ImportCommitted,
        ImportCancelled,
        ImportFailed,
        RevealDone,
        StepDone,
        SkipStep,
        Back,
        SkipTour,
        Finish,
        Replay,
        WindowClosed,
    }

    struct OnboardingEvent {
        kind: OnboardingEventKind,
        step: u8,
        content_hash: String,
        space_ids: Vec<u64>,
    }

    // What the browser runs after it saved the new record. Show shows the
    // tour's state, animated or not; Handoff fades out a skipped tour once.
    // Imported crossfades the welcome to the theme of the tour's
    // `theme_space`, then shows the reveal.
    enum OnboardingEffect {
        None,
        Show,
        OpenImport,
        Handoff,
        Imported,
    }

    // What a tour transition does about the quick start page
    // (onboarding::QuickStart): Open it as the first Today tab, or Drop it
    // after an import. Both clear the workspace's pending flag.
    enum OnboardingQuickStart {
        Keep,
        Open,
        Drop,
    }

    // A decoded tour record. `state` is "welcome", "importing", "reveal",
    // "practice", "handoff" or "complete"; `step` is the practice step, 0
    // outside practice.
    struct OnboardingTour {
        state: String,
        step: u8,
        done: Vec<u8>,
        skipped: Vec<u8>,
        offers_import: bool,
        // The first Space the tour's import committed, or 0.
        theme_space: u64,
        // Back changes the tour, so its coach mark shows Back.
        offers_back: bool,
    }

    // The tab practice 2 points at (onboarding.rs `PinTab`).
    enum OnboardingPinTab {
        Missing,
        Other,
        Today,
        Saved,
    }

    // What practice 2 shows, or the event it feeds instead: Done feeds
    // StepDone(2), Skip feeds SkipStep(2) and Back feeds Back.
    enum OnboardingPin {
        Pin,
        Pinned,
        Done,
        Skip,
        Back,
    }

    // The record to save, before running `effect`, and what it decodes to.
    struct OnboardingTransition {
        record: String,
        tour: OnboardingTour,
        effect: OnboardingEffect,
        animate: bool,
    }

    // A command bar action (command_bar.rs). SwitchSpace carries the Space.
    enum CommandActionKind {
        PinTab,
        UnpinTab,
        NewSpace,
        NewFolder,
        ArchiveTab,
        CopyUrl,
        SplitView,
        SwitchSpace,
        ResetTab,
        ReplacePinnedUrl,
        ClearCookies,
        ClearCache,
        TogglePinsCollapsed,
        ExpandSplit,
        // A browser command, named by the offer's `command`.
        Run,
    }

    // The browser command a Run offer names (command_bar::Command); None for
    // the other kinds.
    enum CommandRun {
        None,
        NextSpace,
        PreviousSpace,
        DuplicateTab,
        ReopenTab,
        Reload,
        Back,
        Forward,
        Find,
        Print,
        SavePage,
        ZoomIn,
        ZoomOut,
        ZoomReset,
        DevTools,
        ToggleSidebar,
        FullScreen,
        NewWindow,
        NewPrivateWindow,
        LittleArc,
        MinimizeWindow,
        CloseWindow,
        History,
        Downloads,
        Extensions,
        Settings,
        PrivacyPolicy,
        ExportPins,
        DarkMode,
        LightMode,
        Quit,
    }

    // What the window's selected row is when the bar opens.
    enum CommandSelected {
        Nothing,
        Today,
        Pin,
        Favorite,
    }

    struct CommandBarContext {
        selected: CommandSelected,
        has_page: bool,
        in_split: bool,
        navigated_away: bool,
        pins_collapsed: bool,
        dark: bool,
        current_space: u64,
    }

    // A Space of the window's Profile, in footer order.
    struct CommandBarSpace {
        id: u64,
        name: String,
    }

    struct CommandOffer {
        kind: CommandActionKind,
        space: u64,
        command: CommandRun,
        title: String,
        // The text plainly names this action, so web search suggestions
        // stay out of the list, as in Arc.
        strong: bool,
    }

    // Where a dragged sidebar row lands.
    enum DropTargetKind {
        // The ungrouped Pins.
        Pins,
        Folder,
        Today,
    }

    enum SpaceIconKind {
        // The default glyph; `icon` is empty.
        Unset,
        // `icon` is one emoji.
        Emoji,
        // `icon` names a bundled icon, such as `book-open`.
        Named,
    }

    // A Space's sidebar colour. `set` false is the default tint, with zero
    // `color` and `opacity`.
    #[derive(PartialEq)]
    struct SpaceThemeRecord {
        set: bool,
        // 0xRRGGBB.
        color: u32,
        // 0 to 100 percent.
        opacity: u8,
    }

    #[derive(PartialEq)]
    struct SpaceRecord {
        id: u64,
        profile_id: u64,
        name: String,
        icon_kind: SpaceIconKind,
        icon: String,
        theme: SpaceThemeRecord,
        // The Profile's agent Space, where external agents open tabs. At most
        // one Space carries it.
        agent: bool,
    }

    #[derive(PartialEq)]
    struct ItemRecord {
        id: u64,
        placement: PlacementKind,
        owner_id: u64,
        has_url: bool,
        canonical_url: String,
        // Last known page title of a saved item; empty when unknown.
        title: String,
        // The user's name for a saved item, which shows instead of its title;
        // empty when it has none.
        name: String,
        // Browser-open clock reading when a temporary item was last seen;
        // zero when not recorded yet.
        last_viewed_secs: u64,
    }

    // A folder of a Space's Pins. `items` are Pins of `space_id` in folder
    // order; a closed folder shows only its row.
    #[derive(PartialEq)]
    struct FolderRecord {
        id: u64,
        space_id: u64,
        name: String,
        open: bool,
        items: Vec<u64>,
    }

    // Two Today tabs of one Space shown side by side; `left` is the left
    // pane.
    #[derive(PartialEq)]
    struct SplitRecord {
        left: u64,
        right: u64,
    }

    struct WindowRecord {
        id: u64,
        space_id: u64,
        // Zero means no selection; core IDs are always nonzero.
        selected_item_id: u64,
    }

    struct ArchivedTabRecord {
        id: u64,
        space_id: u64,
        url: String,
        title: String,
        archived_secs: u64,
    }

    struct AutoArchiveResult {
        // Temporary items to close now; closing archives them.
        due: Vec<u64>,
        // The clock or a last-viewed time changed, so the snapshot needs
        // saving.
        changed: bool,
    }

    struct StateSnapshot {
        version: u32,
        profile_id: u64,
        next_space_id: u64,
        next_item_id: u64,
        next_window_id: u64,
        spaces: Vec<SpaceRecord>,
        items: Vec<ItemRecord>,
        windows: Vec<WindowRecord>,
        // Oldest first, as stored.
        archived: Vec<ArchivedTabRecord>,
        // Auto Archive's browser-open clock in seconds; zero before its first
        // pass.
        open_secs: u64,
        // "Archive tabs after" in seconds. Restore reads anything but the
        // four choices as the 12-hour default.
        archive_interval_secs: u64,
        // Folders in display order within each Space. Restore drops a bad
        // folder or entry rather than failing; its Pins show ungrouped.
        folders: Vec<FolderRecord>,
        next_folder_id: u64,
        // Restore drops a bad split rather than failing; its tabs show as
        // normal rows.
        splits: Vec<SplitRecord>,
        // A fresh workspace whose welcome tour hasn't ended yet; the end may
        // open the quick start page once. Old files read false.
        quick_start_pending: bool,
    }

    // SidebarView::SetModel compares models to skip unchanged rebuilds.
    #[derive(PartialEq)]
    struct SidebarResult {
        ok: bool,
        error: String,
        window_id: u64,
        current_space: SpaceRecord,
        selected_item_id: u64,
        spaces: Vec<SpaceRecord>,
        favorites: Vec<ItemRecord>,
        pins: Vec<ItemRecord>,
        temporary: Vec<ItemRecord>,
        // Every Space's Pins in workspace order; `owner_id` is the Space.
        space_pins: Vec<ItemRecord>,
        // The current Space's folders in order. Their Pins are in `pins` too;
        // the rest of `pins` is ungrouped.
        folders: Vec<FolderRecord>,
        // The current Space's splits. Their items are in `temporary` too; the
        // sidebar draws each as one row where its left tab sits.
        splits: Vec<SplitRecord>,
        // Rows in the order the sidebar shows them, which next and previous
        // tab step through. A closed folder's Pins are left out, except the
        // selected one, and every Pin but the selected one while Pins are
        // collapsed.
        row_order: Vec<u64>,
        // Today tabs New Split's pane may take, dormant ones included, in
        // sidebar order: this Space's Today tabs in no split.
        split_candidates: Vec<u64>,
        // Collapse Pinned Tabs is on in this window: the sidebar shows only
        // the selected Pin.
        pins_collapsed: bool,
    }

    struct BridgeLimits {
        version: u32,
        spaces: usize,
        items: usize,
        windows: usize,
        archived_tabs: usize,
        name_bytes: usize,
        icon_bytes: usize,
        url_bytes: usize,
        title_bytes: usize,
        folders: usize,
        splits: usize,
    }

    // What the browser does for a control request once the core checked it.
    enum TabActionKind {
        // Answered already; nothing to run.
        Answered,
        List,
        Open,
        Activate,
        Close,
        Navigate,
        Back,
        Forward,
        Reload,
        // page.snapshot: answer with answer_snapshot_request.
        Snapshot,
        // page.screenshot: answer with answer_screenshot_request.
        Screenshot,
        // page.wait: wait for loading to finish and answer with answer_tab_request.
        Wait,
        // page.click, page.fill, page.select, page.press or page.scroll: begin
        // with begin_page_action and answer with answer_action_request.
        Act,
    }

    // Every item is a tab in the agent Space. `url` is set for Open and
    // Navigate, `space` for Open, `timeout_ms` for Wait and `items` for List.
    struct ControlPlan {
        // The workspace changed, so the caller saves and notifies.
        changed: bool,
        action: TabActionKind,
        item: u64,
        space: u64,
        timeout_ms: u64,
        url: String,
        items: Vec<u64>,
    }

    enum TabStatus {
        Done,
        // The navigation did not commit in time or ended without committing.
        NotCommitted,
        Denied,
        Stale,
        Invalid,
        // page.snapshot ran out of time.
        TimedOut,
    }

    struct TabStateRecord {
        item: u64,
        url: String,
        title: String,
        loading: bool,
        can_go_back: bool,
        can_go_forward: bool,
    }

    struct ScreenshotRecord {
        item: u64,
        url: String,
        title: String,
        mime_type: String,
        width: u32,
        height: u32,
        data_base64: String,
    }

    // One accessibility node for page.snapshot. `role` is Chromium's role
    // name. `checked` is 0 no, 1 yes, 2 mixed; `expanded` is 0 neither,
    // 1 expanded, 2 collapsed.
    struct AxNodeRecord {
        id: i32,
        role: String,
        name: String,
        value: String,
        url: String,
        children: Vec<i32>,
        level: u32,
        focusable: bool,
        clickable: bool,
        invisible: bool,
        ignored: bool,
        checked: u8,
        expanded: u8,
        disabled: bool,
        selected: bool,
        required: bool,
        is_protected: bool,
        editable: bool,
        read_only: bool,
    }

    enum PageActKind {
        Click,
        Fill,
        Select,
        Press,
        Scroll,
    }

    // A key press on a US layout. `key` and `code` are DOM key and code
    // strings, `code` empty for a character without a key of its own;
    // `text` is the keypress character or 0, `key_code` the Windows key code.
    struct KeyRecord {
        key: String,
        code: String,
        text: u16,
        shift: bool,
        key_code: u16,
    }

    // An interaction command the core accepted against the latest snapshot.
    // `node` is the element's DOMNodeId, or 0 for page.press without ref.
    // `text` is set for Fill, `value` for Select and `key` for Press. With
    // `focus_first`, page.press clicks the text field before the key.
    struct ActionStart {
        ok: bool,
        kind: PageActKind,
        node: i32,
        focus_first: bool,
        text: String,
        value: String,
        key: KeyRecord,
    }

    struct SelectOptionRecord {
        value: String,
        text: String,
        disabled: bool,
    }

    struct ChoiceResult {
        ok: bool,
        value: String,
    }

    enum ActionStatus {
        // The action ran; the nodes are the snapshot after it.
        Done,
        // The page refused it before acting, such as a hidden element.
        Refused,
        // The tab closed or navigated before the action began.
        Stale,
        // It may or may not have happened.
        Unknown,
        // Nothing ran before the time limit, such as while reading a
        // select's options.
        TimedOut,
    }

    struct ActionLimits {
        action_wait_ms: u64,
        settle_cap_ms: u64,
        snapshot_timeout_ms: u64,
        max_select_options: usize,
    }

    // What importing an Arc export would change, or did change. Lists hold at
    // most `arc_import_list_limit()` lines; the counts cover everything.
    struct ArcImportPreview {
        ok: bool,
        error: String,
        // Spaces in the export, with equal names counted once.
        spaces: usize,
        new_spaces: usize,
        new_pins: usize,
        // Profile Favorites the import adds; only Arc's sidebar file has any.
        new_favorites: usize,
        // Entries already pinned in their place, left alone.
        kept_pins: usize,
        // Folders the import creates, one per new group name in a Space.
        new_folders: usize,
        // Folders it adds to that already exist.
        kept_folders: usize,
        // Ungrouped Pins it moves into the folder their group names.
        moved_pins: usize,
        skipped_count: usize,
        // "Line 12: file:///…" for entries that are not imported; Arc's
        // sidebar file has no line numbers, so its entries list only the URL.
        skipped: Vec<String>,
        unrecognized_count: usize,
        // "Line 4, indented: …" for lines the reader did not understand, or
        // "not a link: …" from Arc's sidebar file.
        unrecognized: Vec<String>,
        // After an apply, the part's first Space; otherwise 0.
        first_space: u64,
    }

    // One Arc profile in an export. The main profile, always first, imports
    // into the Profile running the import and has both names empty.
    struct ArcImportProfile {
        // The name Arc shows for the profile, such as "Work", read from
        // Arc's Local State, else its directory name, such as "Profile 1".
        arc_name: String,
        // The myArc Profile it imports into, found or created by this name:
        // Arc's name, else "Arc Profile 1".
        myarc_name: String,
        // The name imports gave the Profile before Arc's names were read,
        // such as "Arc Profile 1", when it differs from `myarc_name`. A
        // re-import goes into a Profile by that name when none has
        // `myarc_name`. Empty otherwise.
        old_myarc_name: String,
    }

    // Shown once an import has committed.
    struct ArcImportReport {
        // "Links left out, not web pages: 2" for each kind of entry the
        // applied parts left out or changed.
        lines: Vec<String>,
        // What Arc's export never carries.
        note: String,
    }

    enum BoostColorMode {
        None,
        Invert,
        Palette,
    }

    enum BoostFontPreset {
        Default,
        SystemSans,
        SystemSerif,
        SystemMono,
        Rounded,
        Geometric,
        Humanist,
        Grotesk,
        Didone,
        SlabSerif,
        ClassicSerif,
        Typewriter,
        Handwritten,
        Condensed,
        Wide,
        Comic,
    }

    enum BoostTextTransform {
        None,
        Uppercase,
        Lowercase,
        Capitalize,
    }

    struct BoostPaletteRecord {
        set: bool,
        background: u32,
        text: u32,
        accent: u32,
        contrast: u8,
        brightness: u8,
        saturation: u8,
        tint_strength: u8,
    }

    struct BoostAppearanceRecord {
        color_mode: BoostColorMode,
        palette: BoostPaletteRecord,
        font: BoostFontPreset,
        text_scale_pct: u16,
        text_transform: BoostTextTransform,
    }

    struct BoostRecord {
        id: u64,
        name: String,
        origins: Vec<String>,
        display_host: String,
        include_subdomains: bool,
        apply_to_subframes: bool,
        enabled: bool,
        version: u32,
        updated_secs: u64,
        appearance: BoostAppearanceRecord,
        hidden_selectors: Vec<String>,
        custom_css: String,
        can_undo: bool,
        has_inert_script: bool,
    }

    struct BoostCssFinding {
        line: usize,
        reason: String,
    }

    struct BoostLoadResult {
        ok: bool,
        damaged: bool,
        error: String,
    }

    struct BoostLimits {
        version: u32,
        max_boosts: usize,
        max_name_bytes: usize,
        max_origins_per_boost: usize,
        max_origin_bytes: usize,
        max_hidden_selectors: usize,
        max_selector_bytes: usize,
        max_custom_css_bytes: usize,
        max_revisions: usize,
        max_file_bytes: usize,
    }

    extern "Rust" {
        type Workspace;
        fn new_workspace() -> Box<Workspace>;
        fn current_name(workspace: &Workspace) -> String;
        fn rename_space(workspace: &mut Workspace, id: u64, name: &str) -> RenameResult;
        fn bridge_limits() -> BridgeLimits;
        // The workspace file (crates/workspace/src/store.rs). Each returns an
        // error, or an empty string on success. Writing replaces the file at
        // `path` atomically and keeps the one before as the backup; it is
        // fast enough for the UI thread and survives a process kill once it
        // returns. Syncing flushes the file and its directory to the disk and
        // is slow, so it runs off the UI thread.
        fn write_workspace_file(path: &str, bytes: &[u8]) -> String;
        fn sync_workspace_file(path: &str) -> String;
        // Where write_workspace_file keeps the generation before the current.
        fn workspace_backup_path(path: &str) -> String;
        // Moves a file that failed to decode out of the way and returns where
        // it went, or an empty string when it could not.
        fn set_aside_workspace_file(path: &str) -> String;
        fn create_space(workspace: &mut Workspace, name: &str) -> MutationResult;
        // Create a Space panel: like create_space, with an icon unless
        // `icon_kind` is Unset, in which case `icon` must be empty, and a
        // theme when `theme.set`.
        fn create_styled_space(
            workspace: &mut Workspace,
            name: &str,
            icon_kind: SpaceIconKind,
            icon: &str,
            theme: SpaceThemeRecord,
        ) -> MutationResult;
        // Like create_styled_space, for a new Space in a fresh Profile: it
        // takes the place of an untouched default Space, keeping its ID.
        fn create_first_space(
            workspace: &mut Workspace,
            name: &str,
            icon_kind: SpaceIconKind,
            icon: &str,
            theme: SpaceThemeRecord,
        ) -> MutationResult;
        // The agent Space's ID, creating it named "Agent" with the "bot" icon
        // when the Profile has none. Never changes a window's Space.
        fn ensure_agent_space(workspace: &mut Workspace) -> MutationResult;
        // Unset restores the default glyph. `id` is 1 when the icon changed,
        // else 0.
        fn set_space_icon(
            workspace: &mut Workspace,
            space_id: u64,
            icon_kind: SpaceIconKind,
            icon: &str,
        ) -> MutationResult;
        // Unset `theme` restores the default tint. `id` is 1 when the theme
        // changed, else 0.
        fn set_space_theme(
            workspace: &mut Workspace,
            space_id: u64,
            theme: SpaceThemeRecord,
        ) -> MutationResult;
        fn create_window(workspace: &mut Workspace, space_id: u64) -> MutationResult;
        fn close_window(workspace: &mut Workspace, window_id: u64) -> MutationResult;
        fn select_space(workspace: &mut Workspace, window_id: u64, space_id: u64)
        -> MutationResult;
        fn add_temporary_item(workspace: &mut Workspace, space_id: u64) -> MutationResult;
        fn select_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> MutationResult;
        fn live_tab_closed(workspace: &mut Workspace, item_id: u64) -> MutationResult;
        // Like live_tab_closed, and archives a temporary item. `id` is 1 when
        // archived, else 0. Never call it for off-the-record Profiles or when
        // the Profile saves no history.
        fn archive_closed_tab(
            workspace: &mut Workspace,
            item_id: u64,
            url: &str,
            title: &str,
            now_secs: u64,
        ) -> MutationResult;
        // Unexpired entries, newest first.
        fn archived_tabs(workspace: &Workspace, now_secs: u64) -> Vec<ArchivedTabRecord>;
        fn remove_archived_tab(workspace: &mut Workspace, id: u64) -> MutationResult;
        // One Auto Archive pass, `elapsed_secs` of monotonic time after the
        // previous one (zero for the first of a launch): advances the
        // browser-open clock, records views of `viewed` (visible tabs),
        // selected and new temporary items, then names the idle ones among
        // `closable` (items whose pages can close and archive) that are due.
        // `media` lists items playing or capturing media.
        fn auto_archive(
            workspace: &mut Workspace,
            viewed: &[u64],
            closable: &[u64],
            media: &[u64],
            elapsed_secs: u64,
        ) -> AutoArchiveResult;
        // "Archive tabs after" in seconds: 12 hours, 24 hours, 7 days or 30
        // days.
        fn archive_interval_secs(workspace: &Workspace) -> u64;
        // True for a fresh workspace until its welcome tour ends.
        fn quick_start_pending(workspace: &Workspace) -> bool;
        fn clear_quick_start(workspace: &mut Workspace);
        // Rejects any other value.
        fn set_archive_interval(workspace: &mut Workspace, secs: u64) -> MutationResult;
        // Clear Archive: drops every entry; `id` is how many went.
        fn clear_archived_tabs(workspace: &mut Workspace) -> MutationResult;
        // Drops expired entries; `id` is how many went.
        fn prune_archived_tabs(workspace: &mut Workspace, now_secs: u64) -> MutationResult;
        // History deletion: drops entries whose URL is in `urls`, and with
        // `has_range` those archived in begin..=end seconds. `id` is how many
        // went.
        fn forget_archived_tabs(
            workspace: &mut Workspace,
            urls: Vec<String>,
            has_range: bool,
            begin_secs: u64,
            end_secs: u64,
        ) -> MutationResult;
        fn set_placement(
            workspace: &mut Workspace,
            item_id: u64,
            placement: PlacementKind,
            owner_id: u64,
            has_url: bool,
            canonical_url: &str,
        ) -> MutationResult;
        fn remove_saved_item(workspace: &mut Workspace, item_id: u64) -> MutationResult;
        // Saved items only. Navigation never calls this implicitly.
        fn set_canonical_url(workspace: &mut Workspace, item_id: u64, url: &str) -> MutationResult;
        // Saved items only. `id` is 1 when the stored title changed, else 0.
        fn set_title(workspace: &mut Workspace, item_id: u64, title: &str) -> MutationResult;
        // Saved items only. An empty name clears it. `id` is 1 when the
        // stored name changed, else 0.
        fn rename_item(workspace: &mut Workspace, item_id: u64, name: &str) -> MutationResult;
        // Drops a Pin or Today tab of the window's Space on `target`: zero
        // `folder_id` unless it is a folder, zero `before` for the end. With
        // `has_url`, pinning a Today tab saves `page_url`, its live page.
        // Nothing changes on failure.
        fn drop_item(
            workspace: &mut Workspace,
            window_id: u64,
            item_id: u64,
            target: DropTargetKind,
            folder_id: u64,
            before: u64,
            has_url: bool,
            page_url: &str,
        ) -> MutationResult;
        // Temporary items only: the page a dormant row reopens. `id` is 1 when
        // the stored URL or title changed, else 0.
        fn set_temporary_page(
            workspace: &mut Workspace,
            item_id: u64,
            url: &str,
            title: &str,
        ) -> MutationResult;
        // A dormant temporary row of `space_id` remembering `url` (ignoring
        // the fragment) and not in `live`; zero when none.
        fn dormant_temporary_item(
            workspace: &Workspace,
            space_id: u64,
            url: &str,
            live: &[u64],
        ) -> u64;
        // Saved items only. `id` is 1 when current_url differs from the saved URL, else 0.
        fn differs_from_canonical(
            workspace: &Workspace,
            item_id: u64,
            current_url: &str,
        ) -> MutationResult;
        // `id` is the Space that windows showing the deleted one now show. The caller
        // confirms first and closes live pages of the deleted Space's items. With
        // `archive`, its pages go to Archived Tabs at `now_secs` first.
        fn delete_space(
            workspace: &mut Workspace,
            space_id: u64,
            archive: bool,
            now_secs: u64,
        ) -> MutationResult;
        fn reveal_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> MutationResult;
        // Selects the item now when the window shows its Space, else when the
        // window next selects that Space.
        fn select_item_in_its_space(
            workspace: &mut Workspace,
            window_id: u64,
            item_id: u64,
        ) -> MutationResult;
        // Zero destination means no live page; zero before means append.
        fn unsave_item(workspace: &mut Workspace, item_id: u64, live_space: u64) -> MutationResult;
        fn move_item_before(workspace: &mut Workspace, item_id: u64, before: u64)
        -> MutationResult;
        // Zero before means move to the end.
        fn move_space_before(
            workspace: &mut Workspace,
            space_id: u64,
            before: u64,
        ) -> MutationResult;
        fn reconcile_live_items(workspace: &mut Workspace, live: &[u64]) -> MutationResult;
        // A new open folder after the Space's others; `id` is the folder.
        fn create_folder(workspace: &mut Workspace, space_id: u64, name: &str) -> MutationResult;
        // Creates a new open folder named after `source_space_id` inside
        // `target_space_id` and copies all of the source Space's Pins (loose
        // and folder items) into it; `id` is the new folder.
        fn export_space_into_folder(
            workspace: &mut Workspace,
            source_space_id: u64,
            target_space_id: u64,
        ) -> MutationResult;
        fn rename_folder(workspace: &mut Workspace, folder_id: u64, name: &str) -> MutationResult;
        // `id` is 1 when the state changed, else 0.
        fn set_folder_open(workspace: &mut Workspace, folder_id: u64, open: bool)
        -> MutationResult;
        // Zero before means the end of the Space's folders.
        fn move_folder_before(
            workspace: &mut Workspace,
            folder_id: u64,
            before: u64,
        ) -> MutationResult;
        // Zero folder takes the Pin out of its folder; zero before appends.
        fn move_pin_to_folder(
            workspace: &mut Workspace,
            item_id: u64,
            folder_id: u64,
            before: u64,
        ) -> MutationResult;
        // Deletes the folder and its Pins. With `archive`, each Pin goes to
        // Archived Tabs at `now_secs`; never for off-the-record Profiles or
        // when the Profile saves no history. `id` is how many were archived.
        // The caller confirms first and closes the Pins' live pages.
        fn delete_folder(
            workspace: &mut Workspace,
            folder_id: u64,
            archive: bool,
            now_secs: u64,
        ) -> MutationResult;
        // Shows two Today tabs of one Space side by side. `id` is 1 when the
        // split changed, else 0.
        fn create_split(workspace: &mut Workspace, left: u64, right: u64) -> MutationResult;
        // Ends the split holding the item, keeping both tabs. `id` is the
        // other pane, or 0 when the item was in no split.
        fn separate_split(workspace: &mut Workspace, item_id: u64) -> MutationResult;
        // Collapse Pinned Tabs, or expand them, in one window. Runtime only.
        fn set_pins_collapsed(
            workspace: &mut Workspace,
            window_id: u64,
            collapsed: bool,
        ) -> MutationResult;
        fn sidebar_snapshot(workspace: &Workspace, window_id: u64) -> SidebarResult;
        fn export_snapshot(workspace: &Workspace) -> StateSnapshot;
        fn restore_snapshot(workspace: &mut Workspace, snapshot: StateSnapshot) -> MutationResult;

        // Browser control (MA-011). The host serves the control endpoint on
        // a thread of its own; a background thread takes each request with
        // next_control_request and the UI thread answers it.
        type ControlReply;
        // UI thread: allows one start and returns its epoch, or 0 while a
        // host is already serving. A later stop or enable makes that epoch
        // stale.
        fn enable_control_host() -> u64;
        // Background thread: binds the endpoint in $MYARC_CONTROL_DIR or the
        // default directory and starts serving. Each client's hello waits
        // for decide_control_hello before its requests come. Empty on success
        // or when `epoch` is stale, else the reason it could not start.
        fn start_control_host(epoch: u64) -> String;
        // Background thread: blocks until a request or a hello arrives. The
        // reply holds neither once the host stopped or `epoch` is stale.
        fn next_control_request(epoch: u64) -> Box<ControlReply>;
        fn has_control_request(reply: &ControlReply) -> bool;
        // False when the client already got timeout or cancelled, or its
        // hello was refused; drop it.
        fn control_request_live(reply: &ControlReply) -> bool;
        // True when `reply` holds a hello rather than a request.
        fn is_control_hello(reply: &ControlReply) -> bool;
        // The hello's client name and version, untrusted and at most 64
        // bytes each. Show them through control_client_label.
        fn control_hello_name(reply: &ControlReply) -> String;
        fn control_hello_version(reply: &ControlReply) -> String;
        // UI thread: welcomes the hello's client, or refuses it with
        // "denied: the user declined".
        fn decide_control_hello(reply: &mut ControlReply, allow: bool);
        // Milliseconds a hello waits for decide_control_hello before the
        // client gets "denied: waiting for approval timed out".
        fn control_approval_timeout_ms() -> u64;
        // Untrusted client text as the browser may show it: invisible and
        // control characters dropped, at most 40 characters.
        fn control_client_label(text: &str) -> String;
        // The approved client connected now or within the last 30 seconds,
        // the latest one if several; empty when none or no host serves.
        fn active_control_client() -> String;
        // Disconnects the client called `name`, answering its waiting
        // request with cancelled.
        fn revoke_control_client(name: &str);
        // UI thread: runs the request against `workspace` as seen from
        // `window_id`, in the Profile whose directory is `profile`, such as
        // "Default" or "Profile 1". A Space command is answered at once and leaves `reply`
        // empty, unless it changed the workspace: then the answer waits for
        // send_control_answer, which the caller runs once the change is saved,
        // so a client never hears of a change a crash can lose. A tab command that passed its checks keeps the request in
        // `reply` and names the action; the caller runs it and answers with
        // answer_tab_request, now or later.
        fn answer_control_request(
            reply: &mut ControlReply,
            workspace: &mut Workspace,
            window_id: u64,
            profile: &str,
        ) -> ControlPlan;
        // Sends the answer answer_control_request kept, if any.
        fn send_control_answer(reply: &mut ControlReply);
        // Answers a tab command kept by answer_control_request. `tabs` holds
        // the tab afterwards, or each live tab for List.
        fn answer_tab_request(
            reply: &mut ControlReply,
            status: TabStatus,
            reason: &str,
            tabs: Vec<TabStateRecord>,
        );
        // Milliseconds to wait for a navigation to commit.
        fn control_navigation_wait_ms() -> u64;
        // Answers page.snapshot. With Done, formats the accessibility tree
        // `nodes` under `root` for `tab`, whose main document the browser
        // numbered `document`, and keeps its references for later commands.
        // `source_truncated` says Chromium stopped at the node limit.
        fn answer_snapshot_request(
            reply: &mut ControlReply,
            status: TabStatus,
            reason: &str,
            tab: TabStateRecord,
            document: u64,
            root: i32,
            focus: i32,
            source_truncated: bool,
            nodes: Vec<AxNodeRecord>,
        );
        // Answers page.screenshot with an encoded PNG or JPEG payload.
        fn answer_screenshot_request(
            reply: &mut ControlReply,
            status: TabStatus,
            reason: &str,
            shot: ScreenshotRecord,
        );
        // Milliseconds to wait for a snapshot before answering timeout.
        fn control_snapshot_timeout_ms() -> u64;
        // Checks an Act request against the latest snapshot of `document`,
        // the tab's main document now, and takes its references. When `ok`
        // is false the request is answered already.
        fn begin_page_action(reply: &mut ControlReply, document: u64) -> ActionStart;
        // page.select: the value of the option the request names among the
        // select's `options`. When `ok` is false the request is answered.
        fn choose_select_option(
            reply: &mut ControlReply,
            options: Vec<SelectOptionRecord>,
        ) -> ChoiceResult;
        // Answers an Act request begun with begin_page_action. With Done,
        // formats the snapshot taken after the action like
        // answer_snapshot_request does.
        fn answer_action_request(
            reply: &mut ControlReply,
            status: ActionStatus,
            reason: &str,
            tab: TabStateRecord,
            navigated: bool,
            settled: bool,
            document: u64,
            root: i32,
            focus: i32,
            source_truncated: bool,
            nodes: Vec<AxNodeRecord>,
        );
        fn control_action_limits() -> ActionLimits;
        // Most accessibility nodes to ask each frame for.
        fn control_snapshot_max_nodes() -> usize;
        // Answers denied without running the request, or refuses a hello
        // with denied and `reason`.
        fn refuse_control_request(reply: &mut ControlReply, reason: &str);
        // UI thread: stops serving and removes the endpoint before returning.
        fn stop_control_host();

        // The href of the best non-SVG `<link rel="icon">` in the head of
        // untrusted `html`, unresolved, or empty. Reads at most
        // `site_icon_scan_bytes()` bytes.
        fn declared_site_icon(html: &[u8]) -> String;
        fn site_icon_scan_bytes() -> usize;
        // Row label for a saved URL whose page has no title yet: host plus
        // path, without scheme, `www.` or trailing slash.
        fn url_label(url: &str) -> String;
        // Whether a canonical URL host is a local development server:
        // localhost, *.localhost, *.local, 127.0.0.0/8, [::1] or 0.0.0.0.
        fn is_local_dev_host(host: &str) -> bool;
        // Where a link from a `source` item's page on site `from_site` to
        // `to_site` opens. Sites are registrable domains (or the host when
        // there is none); empty means unknown and never Peeks. `new_tab` is
        // an explicit Cmd-click.
        fn route_peek_navigation(
            source: PlacementKind,
            from_scheme: &str,
            from_site: &str,
            to_scheme: &str,
            to_site: &str,
            new_tab: bool,
            peek_enabled: bool,
        ) -> PeekRoute;
        fn peek_action(trigger: PeekTrigger) -> PeekActionKind;
        // How far a sidebar drag at `pointer_y` scrolls rows shown from 0 to
        // `height` per tick: negative up, 0 in the middle.
        fn drag_autoscroll_step(pointer_y: i32, height: i32) -> i32;
        // What the blocker switch shows for a host at blocker `level`.
        fn blocker_switch_state(level: i32) -> BlockerSwitchState;
        // The blocker level to set when the switch goes `on`, given the
        // Profile's default level.
        fn blocker_switch_level(on: bool, default_level: i32) -> i32;
        // Expand Current Split: the first pane's share after expanding pane
        // `focused` (0 or 1) of a split at `ratio`.
        fn expand_split_ratio(ratio: f64, focused: usize) -> f64;
        // The actions the command bar offers for `query`, closest first.
        // A leading "/" lists every action that applies.
        fn command_bar_offers(
            query: &str,
            context: &CommandBarContext,
            spaces: &[CommandBarSpace],
        ) -> Vec<CommandOffer>;
        // Where a link another app opened goes. `url` is the canonical spec
        // and `open_little_arcs` what each Little Arc shows, most recently
        // used first.
        fn route_link_from_other_app(
            little_arc_enabled: bool,
            scheme: &str,
            url: &str,
            open_little_arcs: Vec<String>,
        ) -> LittleArcRoute;
        // The index in `windows`, most recently used first, of the main
        // window "Open in" moves a Little Arc page to, or -1 for none.
        fn little_arc_move_target(windows: &[LittleArcMainWindow]) -> i64;
        // The welcome tour once a Profile's workspace loaded. `record` is the
        // saved pref, read only when `has_record`; `first_run` means the load
        // found no workspace file and no backup.
        fn onboarding_on_load(
            has_record: bool,
            record: &str,
            first_run: bool,
        ) -> OnboardingTransition;
        // The tour after `event`, from the saved `record`.
        fn onboarding_event(record: &str, event: &OnboardingEvent) -> OnboardingTransition;
        // The saved `record` decoded. An unreadable one reads as complete.
        fn onboarding_decode(record: &str) -> OnboardingTour;
        // The quick start rule for a transition from record `before` to
        // record `after`, with the workspace's `pending` flag.
        fn onboarding_quick_start(before: &str, after: &str, pending: bool)
        -> OnboardingQuickStart;
        // Practice 2 for `tab`. `shown` is what its card shows, unless
        // `starting`, as the step shows; `by_back` says Back brought the tour
        // there.
        fn onboarding_pin_practice(
            starting: bool,
            shown: OnboardingPin,
            tab: OnboardingPinTab,
            by_back: bool,
        ) -> OnboardingPin;
        // The Ctrl+number key practice 3 teaches for `space_count` Spaces, the
        // current one at `current`, or 0 for Create a Space.
        fn onboarding_space_key(space_count: usize, current: usize) -> u8;
        // True once the window shows another Space, or there are more Spaces,
        // than when practice 3 showed.
        fn onboarding_space_done(
            start_space: u64,
            start_count: usize,
            space: u64,
            count: usize,
        ) -> bool;

        type ArcExport;
        // Largest export accepted, in bytes.
        fn arc_export_max_bytes() -> usize;
        fn arc_import_list_limit() -> usize;
        // Parses untrusted bytes. Never fails; a file it can't read carries an
        // error that previews report.
        fn parse_arc_export(bytes: &[u8]) -> Box<ArcExport>;
        // Largest sidebar file accepted, in bytes.
        fn arc_sidebar_max_bytes() -> usize;
        // Largest Arc `Local State` file read for profile names, in bytes.
        fn arc_local_state_max_bytes() -> usize;
        // The same for Arc's sidebar file, StorableSidebar.json. `local_state`
        // is Arc's `User Data/Local State`, or empty; it names the Arc
        // profiles, and anything unreadable in it is ignored.
        fn parse_arc_sidebar(bytes: &[u8], local_state: &[u8]) -> Box<ArcExport>;
        // Every entry's URL as written, for the browser to canonicalize.
        fn arc_export_urls(arc: &ArcExport) -> Vec<String>;
        // One URL per `arc_export_urls` entry, canonicalized; empty rejects it.
        fn set_arc_export_canonical_urls(arc: &mut ArcExport, urls: Vec<String>);
        // The export's Arc profiles; `part` below indexes this list.
        fn arc_export_profiles(arc: &ArcExport) -> Vec<ArcImportProfile>;
        // What `apply_arc_import` would do now, without changing anything.
        fn preview_arc_import(
            workspace: &Workspace,
            arc: &ArcExport,
            part: usize,
        ) -> ArcImportPreview;
        // Adds the part's Spaces and Pins that are missing, all or nothing.
        // `new_profile` marks a Profile made for this import, whose empty
        // default Space gives way to the first imported one.
        fn apply_arc_import(
            workspace: &mut Workspace,
            arc: &mut ArcExport,
            part: usize,
            new_profile: bool,
        ) -> ArcImportPreview;
        // Everything the applied parts left out or changed.
        fn arc_import_report(arc: &ArcExport) -> ArcImportReport;
        // Export Pins and Favorites…: collects each Profile's workspace, in
        // display order, then writes them all as a Netscape bookmarks file
        // (import::bookmarks_html). Building it touches no live workspace, so
        // it runs on any sequence.
        type BookmarksExport;
        fn new_bookmarks_export() -> Box<BookmarksExport>;
        // Adds Profile `name`; false when `snapshot` doesn't restore, and then
        // the Profile is left out.
        fn add_bookmarks_profile(
            bookmarks: &mut BookmarksExport,
            name: &str,
            snapshot: StateSnapshot,
        ) -> bool;
        fn bookmarks_export_html(bookmarks: &BookmarksExport) -> String;
        // The save panel's default file name.
        fn bookmarks_html_file_name() -> String;
        // Export Space…: writes a single Space's Pins and folders as Netscape
        // bookmarks HTML, or portable JSON when `as_json` is true. Empty string
        // if `snapshot` fails to restore or `space_id` does not exist.
        fn export_space_bookmarks(snapshot: StateSnapshot, space_id: u64, as_json: bool) -> String;
        // Safe default file name for exporting one Space as HTML.
        fn space_export_file_name(space_name: &str) -> String;

        // Arc's Ctrl+Tab over one window's most recently shown items (BC-008).
        type RecentTabs;
        fn new_recent_tabs() -> Box<RecentTabs>;
        // The window showed `item`. Ends a cycle in progress.
        fn touch_recent_tab(recent: &mut RecentTabs, item: u64);
        // Steps `step` places through the five most recent of `shown`, newest
        // first, and returns the target, or 0 when there is none. Ids missing
        // from `shown` stay in the list for when their Space is shown again.
        fn cycle_recent_tab(recent: &mut RecentTabs, shown: &[u64], step: i32) -> u64;

        // The sidebar rows selected with Cmd-click and Shift-click in one
        // window, kept by the sidebar between calls. `order` is the rows
        // shown, top to bottom; `active` the row on screen, or 0; `anchor`
        // the row a range starts from, or 0. See workspace::selection.
        // Cmd-click toggles `clicked`; Shift-click (`range`) selects from the
        // anchor to it.
        fn click_row_selection(
            order: &[u64],
            selected: &[u64],
            anchor: u64,
            active: u64,
            clicked: u64,
            range: bool,
        ) -> RowSelection;
        // Drops rows missing from `order`, after the rows rebuilt.
        fn retain_row_selection(order: &[u64], selected: &[u64], anchor: u64) -> RowSelection;
        // The rows a command from `row`'s menu applies to.
        fn row_selection_menu_targets(selected: &[u64], row: u64) -> Vec<u64>;
        // The row to show before `closing` all close: the first of `live`
        // not closing, or 0. See workspace::selection::row_after_closing.
        fn row_after_closing(live: &[u64], closing: &[u64]) -> u64;

        // The one closed Peek a window can bring back with Cmd+Shift+T
        // (BC-005). See workspace::peek::PeekReopen.
        type PeekReopen;
        fn new_peek_reopen() -> Box<PeekReopen>;
        // A Peek over `item` in `space` ended; `by_user` with the trigger the
        // user pressed, otherwise something else ended it.
        fn peek_closed(
            reopen: &mut PeekReopen,
            by_user: bool,
            trigger: PeekTrigger,
            space: u64,
            item: u64,
        );
        // A tab closed, or the Peek moved, after the last close.
        fn forget_closed_peek(reopen: &mut PeekReopen);
        fn can_reopen_peek(reopen: &PeekReopen) -> bool;
        // Cmd+Shift+T over `item` in `space`: true reopens the Peek, false
        // restores a tab. Uses the closed Peek up either way.
        fn take_closed_peek(reopen: &mut PeekReopen, space: u64, item: u64) -> bool;

        // Profile-scoped Boosts store (crates/boosts). Pure CSS, bounded,
        // no panics across FFI.
        type BoostStore;
        fn boosts_limits() -> BoostLimits;
        fn new_boost_store() -> Box<BoostStore>;
        fn boosts_encode_json(store: &BoostStore) -> String;
        fn boosts_restore_json(store: &mut BoostStore, bytes: &[u8]) -> BoostLoadResult;
        fn boosts_global_enabled(store: &BoostStore) -> bool;
        fn boosts_set_global_enabled(store: &mut BoostStore, enabled: bool) -> bool;
        fn boosts_list(store: &BoostStore, query: &str) -> Vec<BoostRecord>;
        fn boosts_get(store: &BoostStore, id: u64) -> Vec<BoostRecord>;
        fn boosts_matching(
            store: &BoostStore,
            page_origin: &str,
            is_subframe: bool,
        ) -> Vec<BoostRecord>;
        fn boosts_create(
            store: &mut BoostStore,
            name: &str,
            origins: Vec<String>,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_rename(
            store: &mut BoostStore,
            id: u64,
            name: &str,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_set_enabled(
            store: &mut BoostStore,
            id: u64,
            enabled: bool,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_set_scope(
            store: &mut BoostStore,
            id: u64,
            origins: Vec<String>,
            include_subdomains: bool,
            apply_to_subframes: bool,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_set_appearance(
            store: &mut BoostStore,
            id: u64,
            appearance: BoostAppearanceRecord,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_add_hidden_selector(
            store: &mut BoostStore,
            id: u64,
            selector: &str,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_remove_hidden_selector(
            store: &mut BoostStore,
            id: u64,
            selector: &str,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_clear_hidden_selectors(
            store: &mut BoostStore,
            id: u64,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_set_custom_css(
            store: &mut BoostStore,
            id: u64,
            custom_css: &str,
            now_secs: u64,
        ) -> MutationResult;
        fn boosts_reset(store: &mut BoostStore, id: u64, now_secs: u64) -> MutationResult;
        fn boosts_undo(store: &mut BoostStore, id: u64, now_secs: u64) -> MutationResult;
        fn boosts_delete(store: &mut BoostStore, id: u64) -> MutationResult;
        fn boosts_compile_for_origin(
            store: &BoostStore,
            page_origin: &str,
            is_subframe: bool,
        ) -> String;
        fn boosts_compile_preview(
            appearance: BoostAppearanceRecord,
            hidden_selectors: Vec<String>,
            custom_css: &str,
        ) -> String;
        fn boosts_inspect_css(css: &str) -> Vec<BoostCssFinding>;
        fn boosts_normalize_origin(raw: &str) -> String;
    }
}

pub fn arc_export_max_bytes() -> usize {
    import::arc_markdown::MAX_INPUT_BYTES
}

const ARC_IMPORT_LIST_LIMIT: usize = 50;

pub fn arc_import_list_limit() -> usize {
    ARC_IMPORT_LIST_LIMIT
}

pub fn parse_arc_export(bytes: &[u8]) -> Box<ArcExport> {
    let (report, error) = match import::arc_markdown::parse(bytes) {
        Ok(report) => (report, String::new()),
        Err(error) => (Default::default(), error.to_string()),
    };
    Box::new(ArcExport {
        parts: vec![ArcPart::main(report)],
        error,
        sidebar: false,
    })
}

pub fn arc_sidebar_max_bytes() -> usize {
    import::arc_sidebar::MAX_INPUT_BYTES
}

pub fn arc_local_state_max_bytes() -> usize {
    import::arc_sidebar::MAX_LOCAL_STATE_BYTES
}

pub fn parse_arc_sidebar(bytes: &[u8], local_state: &[u8]) -> Box<ArcExport> {
    let (parts, error) = match import::arc_sidebar::parse_profiles(bytes) {
        Ok(mut profiles) => {
            let names = import::arc_sidebar::profile_names(local_state);
            import::arc_sidebar::name_profiles(&mut profiles, &names);
            (
                profiles.into_iter().map(ArcPart::new).collect(),
                String::new(),
            )
        }
        Err(error) => (vec![ArcPart::main(Default::default())], error.to_string()),
    };
    Box::new(ArcExport {
        parts,
        error,
        sidebar: true,
    })
}

pub fn arc_export_urls(export: &ArcExport) -> Vec<String> {
    export
        .parts
        .iter()
        .flat_map(|part| import::arc_workspace::entry_urls(&part.arc.report))
        .map(str::to_owned)
        .collect()
}

pub fn set_arc_export_canonical_urls(export: &mut ArcExport, urls: Vec<String>) {
    let mut urls = urls.into_iter().map(|url| (!url.is_empty()).then_some(url));
    // Each part takes its share in order. A short list leaves later parts
    // short too, which their preview reports as a mismatch.
    for part in &mut export.parts {
        part.canonical = urls.by_ref().take(part.arc.report.entry_count()).collect();
    }
}

pub fn arc_export_profiles(export: &ArcExport) -> Vec<ffi::ArcImportProfile> {
    export
        .parts
        .iter()
        .map(|part| {
            let myarc_name = part.arc.myarc_name().unwrap_or_default();
            let old_name = part.arc.profile.myarc_name().unwrap_or_default();
            ffi::ArcImportProfile {
                arc_name: part.arc.arc_label().unwrap_or_default().to_owned(),
                old_myarc_name: if old_name == myarc_name {
                    String::new()
                } else {
                    old_name
                },
                myarc_name,
            }
        })
        .collect()
}

pub struct BookmarksExport(Vec<(String, workspace::WorkspaceSnapshot)>);

pub fn new_bookmarks_export() -> Box<BookmarksExport> {
    Box::new(BookmarksExport(Vec::new()))
}

pub fn add_bookmarks_profile(
    bookmarks: &mut BookmarksExport,
    name: &str,
    snapshot: ffi::StateSnapshot,
) -> bool {
    let mut restored = Workspace(workspace::Workspace::default());
    if !restore_snapshot(&mut restored, snapshot).ok {
        return false;
    }
    bookmarks.0.push((name.to_owned(), restored.0.snapshot()));
    true
}

pub fn bookmarks_export_html(bookmarks: &BookmarksExport) -> String {
    let profiles: Vec<(&str, &workspace::WorkspaceSnapshot)> = bookmarks
        .0
        .iter()
        .map(|(name, snapshot)| (name.as_str(), snapshot))
        .collect();
    import::bookmarks_html::write_profiles(&profiles)
}

pub fn bookmarks_html_file_name() -> String {
    import::bookmarks_html::DEFAULT_FILE_NAME.to_owned()
}

pub fn export_space_bookmarks(
    snapshot: ffi::StateSnapshot,
    space_id: u64,
    as_json: bool,
) -> String {
    let mut restored = Workspace(workspace::Workspace::default());
    if !restore_snapshot(&mut restored, snapshot).ok {
        return String::new();
    }
    let snap = restored.0.snapshot();
    if as_json {
        import::bookmarks_html::write_space_json(&snap, space_id).unwrap_or_default()
    } else {
        import::bookmarks_html::write_space(&snap, space_id).unwrap_or_default()
    }
}

pub fn space_export_file_name(space_name: &str) -> String {
    import::bookmarks_html::space_file_name(space_name, "html")
}

pub fn preview_arc_import(
    workspace: &Workspace,
    export: &ArcExport,
    part: usize,
) -> ffi::ArcImportPreview {
    let Some(chosen) = export.parts.get(part) else {
        return arc_import_failure("No such part of the export.");
    };
    arc_import_result(
        export,
        part,
        import::arc_workspace::preview(&chosen.arc.report, &chosen.canonical, &workspace.0),
    )
}

pub fn apply_arc_import(
    workspace: &mut Workspace,
    export: &mut ArcExport,
    part: usize,
    new_profile: bool,
) -> ffi::ArcImportPreview {
    let Some(chosen) = export.parts.get(part) else {
        return arc_import_failure("No such part of the export.");
    };
    let apply = if new_profile {
        import::arc_workspace::apply_to_new_profile
    } else {
        import::arc_workspace::apply
    };
    let result = apply(&chosen.arc.report, &chosen.canonical, &mut workspace.0);
    let first = result
        .as_ref()
        .ok()
        .and_then(|plan| plan.spaces.first())
        .and_then(|first| {
            first.existing.or_else(|| {
                let state = workspace.0.snapshot();
                state
                    .spaces
                    .iter()
                    .rev()
                    .find(|space| space.name == first.name && !space.agent)
                    .map(|space| space.id)
            })
        });
    let applied = result.as_ref().ok().cloned();
    let mut preview = arc_import_result(export, part, result);
    preview.first_space = first.unwrap_or(0);
    if preview.ok {
        export.parts[part].applied = applied;
    }
    preview
}

pub fn arc_import_report(export: &ArcExport) -> ffi::ArcImportReport {
    let mut counts = std::collections::BTreeMap::new();
    for part in &export.parts {
        let Some(plan) = &part.applied else { continue };
        for line in import::arc_report::lines(&part.arc.report, &part.canonical, plan) {
            *counts.entry(line.kind).or_insert(0) += line.count;
        }
    }
    ffi::ArcImportReport {
        lines: counts
            .into_iter()
            .map(|(kind, count)| import::arc_report::Line { kind, count }.text())
            .collect(),
        note: if export.sidebar {
            import::arc_report::NOT_FROM_SIDEBAR
        } else {
            import::arc_report::NOT_IN_EXPORT
        }
        .to_owned(),
    }
}

pub struct RecentTabs(workspace::tab_ops::RecentTabs);

pub fn new_recent_tabs() -> Box<RecentTabs> {
    Box::new(RecentTabs(workspace::tab_ops::RecentTabs::new()))
}

pub fn touch_recent_tab(recent: &mut RecentTabs, item: u64) {
    recent.0.touch(item);
}

pub fn cycle_recent_tab(recent: &mut RecentTabs, shown: &[u64], step: i32) -> u64 {
    recent
        .0
        .cycle_within(step, |id| shown.contains(&id))
        .unwrap_or(0)
}

fn row_selection(order: &[u64], selected: &[u64], anchor: u64) -> workspace::selection::Selection {
    let mut selection =
        workspace::selection::Selection::from_parts(selected, (anchor != 0).then_some(anchor));
    selection.retain(order);
    selection
}

fn row_selection_result(selection: &workspace::selection::Selection) -> ffi::RowSelection {
    ffi::RowSelection {
        items: selection.items().to_vec(),
        anchor: selection.anchor().unwrap_or(0),
    }
}

pub fn click_row_selection(
    order: &[u64],
    selected: &[u64],
    anchor: u64,
    active: u64,
    clicked: u64,
    range: bool,
) -> ffi::RowSelection {
    let mut selection = row_selection(order, selected, anchor);
    let active = (active != 0).then_some(active);
    if range {
        selection.extend(order, active, clicked);
    } else {
        selection.toggle(order, active, clicked);
    }
    row_selection_result(&selection)
}

pub fn retain_row_selection(order: &[u64], selected: &[u64], anchor: u64) -> ffi::RowSelection {
    row_selection_result(&row_selection(order, selected, anchor))
}

pub fn row_selection_menu_targets(selected: &[u64], row: u64) -> Vec<u64> {
    workspace::selection::Selection::from_parts(selected, None).menu_targets(row)
}

pub fn row_after_closing(live: &[u64], closing: &[u64]) -> u64 {
    workspace::selection::row_after_closing(live, closing).unwrap_or(0)
}

fn arc_import_failure(error: &str) -> ffi::ArcImportPreview {
    arc_import_result(
        &ArcExport {
            parts: Vec::new(),
            error: error.to_owned(),
            sidebar: false,
        },
        0,
        Ok(Default::default()),
    )
}

// "Line 12" for a listed entry of the Markdown export. Arc's sidebar file has
// no line numbers and its entries carry line 0, so they get none.
fn arc_import_line(export: &ArcExport, line: usize) -> Option<String> {
    (!export.sidebar && line != 0).then(|| format!("Line {line}"))
}

fn arc_import_result(
    export: &ArcExport,
    part: usize,
    result: Result<import::arc_workspace::Preview, import::arc_workspace::ImportError>,
) -> ffi::ArcImportPreview {
    let unrecognized: &[import::arc_markdown::Unrecognized] = export
        .parts
        .get(part)
        .map_or(&[], |part| &part.arc.report.unrecognized);
    let mut preview = ffi::ArcImportPreview {
        ok: false,
        error: export.error.clone(),
        spaces: 0,
        new_spaces: 0,
        new_pins: 0,
        new_favorites: 0,
        kept_pins: 0,
        new_folders: 0,
        kept_folders: 0,
        moved_pins: 0,
        skipped_count: 0,
        skipped: Vec::new(),
        unrecognized_count: unrecognized.len(),
        first_space: 0,
        unrecognized: unrecognized
            .iter()
            .take(ARC_IMPORT_LIST_LIMIT)
            .map(|line| {
                let text = format!("{}: {}", line.reason.label(), line.excerpt);
                match arc_import_line(export, line.line) {
                    Some(number) => format!("{number}, {text}"),
                    None => text,
                }
            })
            .collect(),
    };
    if !export.error.is_empty() {
        return preview;
    }
    match result {
        Ok(plan) => {
            preview.ok = true;
            preview.spaces = plan.spaces.len();
            preview.new_spaces = plan.new_space_count();
            preview.new_pins = plan.new_pin_count();
            preview.new_favorites = plan.new_favorite_count();
            preview.kept_pins = plan.kept_pin_count();
            preview.new_folders = plan.new_folder_count();
            preview.kept_folders = plan
                .spaces
                .iter()
                .flat_map(|space| &space.folders)
                .filter(|folder| folder.existing.is_some())
                .count();
            preview.moved_pins = plan.moved_pin_count();
            preview.skipped_count = plan.skipped.len();
            preview.skipped = plan
                .skipped
                .iter()
                .take(ARC_IMPORT_LIST_LIMIT)
                .map(|entry| match arc_import_line(export, entry.line) {
                    Some(number) => format!("{number}: {}", entry.url),
                    None => entry.url.clone(),
                })
                .collect();
        }
        Err(error) => preview.error = error.to_string(),
    }
    preview
}

pub fn declared_site_icon(html: &[u8]) -> String {
    workspace::site_icon::declared_icon(html).unwrap_or_default()
}

pub fn site_icon_scan_bytes() -> usize {
    workspace::site_icon::MAX_SCAN_BYTES
}

pub fn url_label(url: &str) -> String {
    workspace::url_label::url_label(url)
}

pub fn is_local_dev_host(host: &str) -> bool {
    workspace::local_dev::is_local_dev_host(host)
}

pub fn route_peek_navigation(
    source: ffi::PlacementKind,
    from_scheme: &str,
    from_site: &str,
    to_scheme: &str,
    to_site: &str,
    new_tab: bool,
    peek_enabled: bool,
) -> ffi::PeekRoute {
    use workspace::peek::{
        ClickModifier, Disposition, NavigationRequest, PeekSettings, SiteRef, SourceKind,
    };
    let source = match source {
        ffi::PlacementKind::Pin => SourceKind::Pinned,
        ffi::PlacementKind::Favorite => SourceKind::Favorite,
        _ => SourceKind::Temporary,
    };
    let request = NavigationRequest {
        source,
        from: SiteRef {
            scheme: from_scheme,
            site: from_site,
        },
        to: SiteRef {
            scheme: to_scheme,
            site: to_site,
        },
        modifier: if new_tab {
            ClickModifier::NewTab
        } else {
            ClickModifier::None
        },
    };
    match workspace::peek::route_navigation(&request, &PeekSettings { peek_enabled }) {
        Disposition::Peek => ffi::PeekRoute::Peek,
        Disposition::CurrentTab => ffi::PeekRoute::CurrentTab,
        Disposition::NewTab => ffi::PeekRoute::NewTab,
    }
}

fn peek_trigger(trigger: ffi::PeekTrigger) -> workspace::peek::PeekTrigger {
    use workspace::peek::PeekTrigger;
    match trigger {
        ffi::PeekTrigger::ExpandButton => PeekTrigger::ExpandButton,
        ffi::PeekTrigger::CommandO => PeekTrigger::CommandO,
        ffi::PeekTrigger::SplitButton => PeekTrigger::SplitButton,
        ffi::PeekTrigger::CloseButton => PeekTrigger::CloseButton,
        ffi::PeekTrigger::CommandW => PeekTrigger::CommandW,
        ffi::PeekTrigger::OutsideClick => PeekTrigger::OutsideClick,
        // Escape, and any value C++ should never send, closes.
        _ => PeekTrigger::Escape,
    }
}

pub fn peek_action(trigger: ffi::PeekTrigger) -> ffi::PeekActionKind {
    use workspace::peek::PeekAction;
    match workspace::peek::peek_action(peek_trigger(trigger)) {
        PeekAction::OpenInTab => ffi::PeekActionKind::OpenInTab,
        PeekAction::OpenInSplit => ffi::PeekActionKind::OpenInSplit,
        PeekAction::Close => ffi::PeekActionKind::Close,
    }
}

pub struct PeekReopen(workspace::peek::PeekReopen);

pub fn new_peek_reopen() -> Box<PeekReopen> {
    Box::new(PeekReopen(workspace::peek::PeekReopen::new()))
}

pub fn peek_closed(
    reopen: &mut PeekReopen,
    by_user: bool,
    trigger: ffi::PeekTrigger,
    space: u64,
    item: u64,
) {
    reopen
        .0
        .closed(by_user.then(|| peek_trigger(trigger)), space, item);
}

pub fn forget_closed_peek(reopen: &mut PeekReopen) {
    reopen.0.forget();
}

pub fn can_reopen_peek(reopen: &PeekReopen) -> bool {
    reopen.0.can_reopen()
}

pub fn take_closed_peek(reopen: &mut PeekReopen, space: u64, item: u64) -> bool {
    reopen.0.take(space, item)
}

pub fn route_link_from_other_app(
    little_arc_enabled: bool,
    scheme: &str,
    url: &str,
    open_little_arcs: Vec<String>,
) -> ffi::LittleArcRoute {
    use workspace::little_arc::{ExternalLinkRoute, route_external_link};
    let open: Vec<&str> = open_little_arcs.iter().map(String::as_str).collect();
    match route_external_link(little_arc_enabled, scheme, url, &open) {
        ExternalLinkRoute::NewLittleArc => ffi::LittleArcRoute {
            kind: ffi::LittleArcRouteKind::NewLittleArc,
            index: 0,
        },
        ExternalLinkRoute::FocusLittleArc(index) => ffi::LittleArcRoute {
            kind: ffi::LittleArcRouteKind::FocusLittleArc,
            index,
        },
        ExternalLinkRoute::NewTab => ffi::LittleArcRoute {
            kind: ffi::LittleArcRouteKind::NewTab,
            index: 0,
        },
    }
}

fn onboarding_tour(record: &workspace::onboarding::TourRecord) -> ffi::OnboardingTour {
    use workspace::onboarding::{PRACTICE_STEPS, TourState, state_name};
    let steps = |of: fn(&workspace::onboarding::TourRecord, u8) -> bool| {
        (1..=PRACTICE_STEPS)
            .filter(|step| of(record, *step))
            .collect()
    };
    ffi::OnboardingTour {
        state: state_name(record.state).to_owned(),
        step: match record.state {
            TourState::Practice(step) => step,
            _ => 0,
        },
        done: steps(workspace::onboarding::TourRecord::is_done),
        skipped: steps(workspace::onboarding::TourRecord::is_skipped),
        offers_import: record.offers_import(),
        theme_space: record.theme_space().unwrap_or(0),
        offers_back: record.offers_back(),
    }
}

fn onboarding_transition(
    transition: workspace::onboarding::Transition,
) -> ffi::OnboardingTransition {
    use workspace::onboarding::{TourEffect, encode};
    let (effect, animate) = match transition.effect {
        TourEffect::None => (ffi::OnboardingEffect::None, false),
        TourEffect::Show { animate } => (ffi::OnboardingEffect::Show, animate),
        TourEffect::OpenImport => (ffi::OnboardingEffect::OpenImport, false),
        TourEffect::Handoff => (ffi::OnboardingEffect::Handoff, true),
        TourEffect::Imported => (ffi::OnboardingEffect::Imported, true),
    };
    ffi::OnboardingTransition {
        record: encode(&transition.record),
        tour: onboarding_tour(&transition.record),
        effect,
        animate,
    }
}

pub fn onboarding_on_load(
    has_record: bool,
    record: &str,
    first_run: bool,
) -> ffi::OnboardingTransition {
    onboarding_transition(workspace::onboarding::on_load(
        has_record.then_some(record),
        first_run,
    ))
}

pub fn onboarding_event(record: &str, event: &ffi::OnboardingEvent) -> ffi::OnboardingTransition {
    use workspace::onboarding::{ImportReceipt, TourEvent, apply, decode};
    let event = match event.kind {
        ffi::OnboardingEventKind::StartBrowsing => TourEvent::StartBrowsing,
        ffi::OnboardingEventKind::ImportStarted => TourEvent::ImportStarted,
        ffi::OnboardingEventKind::ImportCommitted => TourEvent::ImportCommitted(ImportReceipt {
            content_hash: event.content_hash.clone(),
            space_ids: event.space_ids.clone(),
        }),
        ffi::OnboardingEventKind::ImportCancelled => TourEvent::ImportCancelled,
        ffi::OnboardingEventKind::ImportFailed => TourEvent::ImportFailed,
        ffi::OnboardingEventKind::RevealDone => TourEvent::RevealDone,
        ffi::OnboardingEventKind::StepDone => TourEvent::StepDone(event.step),
        ffi::OnboardingEventKind::SkipStep => TourEvent::SkipStep(event.step),
        ffi::OnboardingEventKind::Back => TourEvent::Back,
        ffi::OnboardingEventKind::SkipTour => TourEvent::SkipTour,
        ffi::OnboardingEventKind::Finish => TourEvent::Finish,
        ffi::OnboardingEventKind::Replay => TourEvent::Replay,
        // WindowClosed, and any value C++ should never send, changes at most
        // an open import back to the welcome.
        _ => TourEvent::WindowClosed,
    };
    onboarding_transition(apply(&decode(record), event))
}

pub fn onboarding_decode(record: &str) -> ffi::OnboardingTour {
    onboarding_tour(&workspace::onboarding::decode(record))
}

pub fn onboarding_quick_start(
    before: &str,
    after: &str,
    pending: bool,
) -> ffi::OnboardingQuickStart {
    use workspace::onboarding::{QuickStart, decode, quick_start};
    match quick_start(&decode(before), &decode(after), pending) {
        QuickStart::Keep => ffi::OnboardingQuickStart::Keep,
        QuickStart::Open => ffi::OnboardingQuickStart::Open,
        QuickStart::Drop => ffi::OnboardingQuickStart::Drop,
    }
}

pub fn onboarding_pin_practice(
    starting: bool,
    shown: ffi::OnboardingPin,
    tab: ffi::OnboardingPinTab,
    by_back: bool,
) -> ffi::OnboardingPin {
    use workspace::onboarding::{PinPractice, PinTab, pin_practice};
    let shown = match shown {
        _ if starting => None,
        ffi::OnboardingPin::Pinned => Some(PinPractice::Pinned),
        ffi::OnboardingPin::Done => Some(PinPractice::Done),
        ffi::OnboardingPin::Skip => Some(PinPractice::Skip),
        ffi::OnboardingPin::Back => Some(PinPractice::Back),
        _ => Some(PinPractice::Pin),
    };
    let tab = match tab {
        ffi::OnboardingPinTab::Other => PinTab::Other,
        ffi::OnboardingPinTab::Today => PinTab::Today,
        ffi::OnboardingPinTab::Saved => PinTab::Saved,
        _ => PinTab::Missing,
    };
    match pin_practice(shown, tab, by_back) {
        PinPractice::Pin => ffi::OnboardingPin::Pin,
        PinPractice::Pinned => ffi::OnboardingPin::Pinned,
        PinPractice::Done => ffi::OnboardingPin::Done,
        PinPractice::Skip => ffi::OnboardingPin::Skip,
        PinPractice::Back => ffi::OnboardingPin::Back,
    }
}

pub fn onboarding_space_key(space_count: usize, current: usize) -> u8 {
    use workspace::onboarding::{SpacePractice, space_practice};
    match space_practice(space_count, current) {
        SpacePractice::Switch { key } => key,
        SpacePractice::Create => 0,
    }
}

pub fn onboarding_space_done(
    start_space: u64,
    start_count: usize,
    space: u64,
    count: usize,
) -> bool {
    workspace::onboarding::space_step_done((start_space, start_count), (space, count))
}

pub fn little_arc_move_target(windows: &[ffi::LittleArcMainWindow]) -> i64 {
    let windows: Vec<workspace::little_arc::MainWindow> = windows
        .iter()
        .map(|window| workspace::little_arc::MainWindow {
            same_profile: window.same_profile,
            workspace_ready: window.workspace_ready,
        })
        .collect();
    workspace::little_arc::move_target(&windows)
        .and_then(|index| i64::try_from(index).ok())
        .unwrap_or(-1)
}

pub fn new_workspace() -> Box<Workspace> {
    Box::new(Workspace(workspace::Workspace::default()))
}

pub fn current_name(workspace: &Workspace) -> String {
    workspace.0.space_name().to_owned()
}

pub fn rename_space(workspace: &mut Workspace, id: u64, name: &str) -> ffi::RenameResult {
    let result = workspace.0.rename_space(id, name);
    ffi::RenameResult {
        ok: result.is_ok(),
        name: workspace.0.space_name_of(id).unwrap_or_default().to_owned(),
        error: result
            .err()
            .map(|error| error.to_string())
            .unwrap_or_default(),
    }
}

pub fn write_workspace_file(path: &str, bytes: &[u8]) -> String {
    workspace::store::write(std::path::Path::new(path), bytes)
        .err()
        .map_or_else(String::new, |error| error.to_string())
}

pub fn sync_workspace_file(path: &str) -> String {
    workspace::store::sync(std::path::Path::new(path))
        .err()
        .map_or_else(String::new, |error| error.to_string())
}

pub fn workspace_backup_path(path: &str) -> String {
    workspace::store::backup_path(std::path::Path::new(path))
        .to_string_lossy()
        .into_owned()
}

pub fn set_aside_workspace_file(path: &str) -> String {
    workspace::store::set_aside(std::path::Path::new(path))
        .map(|aside| aside.to_string_lossy().into_owned())
        .unwrap_or_default()
}

pub fn bridge_limits() -> ffi::BridgeLimits {
    ffi::BridgeLimits {
        version: workspace::SNAPSHOT_VERSION,
        spaces: workspace::MAX_SPACES,
        items: workspace::MAX_ITEMS,
        windows: workspace::MAX_WINDOWS,
        archived_tabs: workspace::MAX_ARCHIVED_TABS,
        name_bytes: workspace::MAX_SPACE_NAME_BYTES,
        icon_bytes: workspace::MAX_SPACE_ICON_BYTES,
        url_bytes: workspace::MAX_URL_BYTES,
        title_bytes: workspace::MAX_TITLE_BYTES,
        folders: workspace::MAX_FOLDERS,
        splits: workspace::MAX_SPLITS,
    }
}

pub fn expand_split_ratio(ratio: f64, focused: usize) -> f64 {
    workspace::split::expand_ratio(ratio, focused)
}

pub fn set_pins_collapsed(
    workspace: &mut Workspace,
    window_id: u64,
    collapsed: bool,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .set_pins_collapsed(window_id, collapsed)
            .map(|()| window_id),
    )
}

pub fn blocker_switch_state(level: i32) -> ffi::BlockerSwitchState {
    use workspace::blocker::SwitchState;
    match workspace::blocker::switch_state(level) {
        SwitchState::Unknown => ffi::BlockerSwitchState::Unknown,
        SwitchState::Off => ffi::BlockerSwitchState::Off,
        SwitchState::On => ffi::BlockerSwitchState::On,
    }
}

pub fn blocker_switch_level(on: bool, default_level: i32) -> i32 {
    workspace::blocker::level_for(on, default_level)
}

pub fn drag_autoscroll_step(pointer_y: i32, height: i32) -> i32 {
    workspace::drag::autoscroll_step(pointer_y, height)
}

pub fn command_bar_offers(
    query: &str,
    context: &ffi::CommandBarContext,
    spaces: &[ffi::CommandBarSpace],
) -> Vec<ffi::CommandOffer> {
    use workspace::command_bar::{Action, Command, Context, Selected};
    let selected = match context.selected {
        ffi::CommandSelected::Today => Selected::Today,
        ffi::CommandSelected::Pin => Selected::Pin,
        ffi::CommandSelected::Favorite => Selected::Favorite,
        _ => Selected::Nothing,
    };
    let context_rule = Context {
        selected,
        has_page: context.has_page,
        in_split: context.in_split,
        navigated_away: context.navigated_away,
        pins_collapsed: context.pins_collapsed,
        dark: context.dark,
    };
    let names: Vec<(u64, &str)> = spaces
        .iter()
        .map(|space| (space.id, space.name.as_str()))
        .collect();
    workspace::command_bar::offers(query, &context_rule, &names, context.current_space)
        .into_iter()
        .map(|offer| {
            let mut command = ffi::CommandRun::None;
            let (kind, space) = match offer.action {
                Action::PinTab => (ffi::CommandActionKind::PinTab, 0),
                Action::UnpinTab => (ffi::CommandActionKind::UnpinTab, 0),
                Action::NewSpace => (ffi::CommandActionKind::NewSpace, 0),
                Action::NewFolder => (ffi::CommandActionKind::NewFolder, 0),
                Action::ArchiveTab => (ffi::CommandActionKind::ArchiveTab, 0),
                Action::CopyUrl => (ffi::CommandActionKind::CopyUrl, 0),
                Action::SplitView => (ffi::CommandActionKind::SplitView, 0),
                Action::SwitchSpace(id) => (ffi::CommandActionKind::SwitchSpace, id),
                Action::ResetTab => (ffi::CommandActionKind::ResetTab, 0),
                Action::ReplacePinnedUrl => (ffi::CommandActionKind::ReplacePinnedUrl, 0),
                Action::ClearCookies => (ffi::CommandActionKind::ClearCookies, 0),
                Action::ClearCache => (ffi::CommandActionKind::ClearCache, 0),
                Action::TogglePinsCollapsed => (ffi::CommandActionKind::TogglePinsCollapsed, 0),
                Action::ExpandSplit => (ffi::CommandActionKind::ExpandSplit, 0),
                Action::Run(run) => {
                    command = match run {
                        Command::NextSpace => ffi::CommandRun::NextSpace,
                        Command::PreviousSpace => ffi::CommandRun::PreviousSpace,
                        Command::DuplicateTab => ffi::CommandRun::DuplicateTab,
                        Command::ReopenTab => ffi::CommandRun::ReopenTab,
                        Command::Reload => ffi::CommandRun::Reload,
                        Command::Back => ffi::CommandRun::Back,
                        Command::Forward => ffi::CommandRun::Forward,
                        Command::Find => ffi::CommandRun::Find,
                        Command::Print => ffi::CommandRun::Print,
                        Command::SavePage => ffi::CommandRun::SavePage,
                        Command::ZoomIn => ffi::CommandRun::ZoomIn,
                        Command::ZoomOut => ffi::CommandRun::ZoomOut,
                        Command::ZoomReset => ffi::CommandRun::ZoomReset,
                        Command::DevTools => ffi::CommandRun::DevTools,
                        Command::ToggleSidebar => ffi::CommandRun::ToggleSidebar,
                        Command::FullScreen => ffi::CommandRun::FullScreen,
                        Command::NewWindow => ffi::CommandRun::NewWindow,
                        Command::NewPrivateWindow => ffi::CommandRun::NewPrivateWindow,
                        Command::LittleArc => ffi::CommandRun::LittleArc,
                        Command::MinimizeWindow => ffi::CommandRun::MinimizeWindow,
                        Command::CloseWindow => ffi::CommandRun::CloseWindow,
                        Command::History => ffi::CommandRun::History,
                        Command::Downloads => ffi::CommandRun::Downloads,
                        Command::Extensions => ffi::CommandRun::Extensions,
                        Command::Settings => ffi::CommandRun::Settings,
                        Command::PrivacyPolicy => ffi::CommandRun::PrivacyPolicy,
                        Command::ExportPins => ffi::CommandRun::ExportPins,
                        Command::DarkMode => ffi::CommandRun::DarkMode,
                        Command::LightMode => ffi::CommandRun::LightMode,
                        Command::Quit => ffi::CommandRun::Quit,
                    };
                    (ffi::CommandActionKind::Run, 0)
                }
            };
            ffi::CommandOffer {
                kind,
                space,
                command,
                title: offer.title,
                strong: offer.strong,
            }
        })
        .collect()
}

fn outcome<E: std::fmt::Display>(result: Result<u64, E>) -> ffi::MutationResult {
    match result {
        Ok(id) => ffi::MutationResult {
            ok: true,
            id,
            error: String::new(),
        },
        Err(error) => ffi::MutationResult {
            ok: false,
            id: 0,
            error: error.to_string(),
        },
    }
}

fn reject(message: &str) -> ffi::MutationResult {
    outcome(Err::<u64, _>(message))
}

pub fn create_space(workspace: &mut Workspace, name: &str) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .create_space(workspace::DEFAULT_PROFILE_ID, name),
    )
}

fn space_icon(
    kind: ffi::SpaceIconKind,
    icon: &str,
) -> Result<Option<workspace::SpaceIcon>, &'static str> {
    match kind {
        ffi::SpaceIconKind::Unset if icon.is_empty() => Ok(None),
        ffi::SpaceIconKind::Unset => Err("Icon supplied without an icon kind"),
        ffi::SpaceIconKind::Emoji => Ok(Some(workspace::SpaceIcon::Emoji(icon.to_owned()))),
        ffi::SpaceIconKind::Named => Ok(Some(workspace::SpaceIcon::Named(icon.to_owned()))),
        _ => Err("Unknown Space icon kind"),
    }
}

fn space_theme(theme: &ffi::SpaceThemeRecord) -> Option<workspace::SpaceTheme> {
    theme.set.then_some(workspace::SpaceTheme {
        color: theme.color,
        opacity: theme.opacity,
    })
}

pub fn create_styled_space(
    workspace: &mut Workspace,
    name: &str,
    icon_kind: ffi::SpaceIconKind,
    icon: &str,
    theme: ffi::SpaceThemeRecord,
) -> ffi::MutationResult {
    match space_icon(icon_kind, icon) {
        Ok(icon) => outcome(workspace.0.create_styled_space(
            workspace::DEFAULT_PROFILE_ID,
            name,
            icon,
            space_theme(&theme),
        )),
        Err(error) => reject(error),
    }
}

pub fn create_first_space(
    workspace: &mut Workspace,
    name: &str,
    icon_kind: ffi::SpaceIconKind,
    icon: &str,
    theme: ffi::SpaceThemeRecord,
) -> ffi::MutationResult {
    match space_icon(icon_kind, icon) {
        Ok(icon) => outcome(workspace.0.create_first_space(
            workspace::DEFAULT_PROFILE_ID,
            name,
            icon,
            space_theme(&theme),
        )),
        Err(error) => reject(error),
    }
}

pub fn ensure_agent_space(workspace: &mut Workspace) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .ensure_agent_space(workspace::DEFAULT_PROFILE_ID),
    )
}

pub fn set_space_theme(
    workspace: &mut Workspace,
    space_id: u64,
    theme: ffi::SpaceThemeRecord,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .set_space_theme(space_id, space_theme(&theme))
            .map(u64::from),
    )
}

fn theme_record(theme: Option<workspace::SpaceTheme>) -> ffi::SpaceThemeRecord {
    match theme {
        Some(theme) => ffi::SpaceThemeRecord {
            set: true,
            color: theme.color,
            opacity: theme.opacity,
        },
        None => ffi::SpaceThemeRecord {
            set: false,
            color: 0,
            opacity: 0,
        },
    }
}

pub fn set_space_icon(
    workspace: &mut Workspace,
    space_id: u64,
    icon_kind: ffi::SpaceIconKind,
    icon: &str,
) -> ffi::MutationResult {
    match space_icon(icon_kind, icon) {
        Ok(icon) => outcome(workspace.0.set_space_icon(space_id, icon).map(u64::from)),
        Err(error) => reject(error),
    }
}

pub fn create_window(workspace: &mut Workspace, space_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.create_window(space_id))
}

pub fn close_window(workspace: &mut Workspace, window_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.close_window(window_id).map(|()| 0))
}

pub fn select_space(
    workspace: &mut Workspace,
    window_id: u64,
    space_id: u64,
) -> ffi::MutationResult {
    outcome(workspace.0.select_space(window_id, space_id).map(|()| 0))
}

pub fn add_temporary_item(workspace: &mut Workspace, space_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.add_temporary_item(space_id))
}

pub fn select_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.select_item(window_id, item_id).map(|()| 0))
}

pub fn live_tab_closed(workspace: &mut Workspace, item_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.live_tab_closed(item_id).map(|()| 0))
}

pub fn archive_closed_tab(
    workspace: &mut Workspace,
    item_id: u64,
    url: &str,
    title: &str,
    now_secs: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .archive_closed_tab(item_id, url, title, now_secs)
            .map(u64::from),
    )
}

fn archived_record(tab: workspace::ArchivedTab) -> ffi::ArchivedTabRecord {
    ffi::ArchivedTabRecord {
        id: tab.id,
        space_id: tab.space_id,
        url: tab.url,
        title: tab.title,
        archived_secs: tab.archived_secs,
    }
}

pub fn archived_tabs(workspace: &Workspace, now_secs: u64) -> Vec<ffi::ArchivedTabRecord> {
    workspace
        .0
        .archived_tabs(now_secs)
        .into_iter()
        .map(archived_record)
        .collect()
}

pub fn remove_archived_tab(workspace: &mut Workspace, id: u64) -> ffi::MutationResult {
    outcome(workspace.0.remove_archived_tab(id).map(|tab| tab.id))
}

pub fn auto_archive(
    workspace: &mut Workspace,
    viewed: &[u64],
    closable: &[u64],
    media: &[u64],
    elapsed_secs: u64,
) -> ffi::AutoArchiveResult {
    let pass = workspace
        .0
        .auto_archive(viewed, closable, media, elapsed_secs);
    ffi::AutoArchiveResult {
        due: pass.due,
        changed: pass.changed,
    }
}

pub fn archive_interval_secs(workspace: &Workspace) -> u64 {
    workspace.0.archive_interval().seconds()
}

pub fn quick_start_pending(workspace: &Workspace) -> bool {
    workspace.0.quick_start_pending()
}

pub fn clear_quick_start(workspace: &mut Workspace) {
    workspace.0.clear_quick_start();
}

pub fn set_archive_interval(workspace: &mut Workspace, secs: u64) -> ffi::MutationResult {
    match workspace::archive::ArchiveInterval::from_seconds(secs) {
        Some(interval) => {
            workspace.0.set_archive_interval(interval);
            ffi::MutationResult {
                ok: true,
                id: secs,
                error: String::new(),
            }
        }
        None => reject("Choose 12 hours, 24 hours, 7 days or 30 days"),
    }
}

pub fn clear_archived_tabs(workspace: &mut Workspace) -> ffi::MutationResult {
    count(workspace.0.clear_archived_tabs())
}

fn count(removed: usize) -> ffi::MutationResult {
    ffi::MutationResult {
        ok: true,
        id: u64::try_from(removed).unwrap_or(u64::MAX),
        error: String::new(),
    }
}

pub fn prune_archived_tabs(workspace: &mut Workspace, now_secs: u64) -> ffi::MutationResult {
    count(workspace.0.prune_archived_tabs(now_secs))
}

pub fn forget_archived_tabs(
    workspace: &mut Workspace,
    urls: Vec<String>,
    has_range: bool,
    begin_secs: u64,
    end_secs: u64,
) -> ffi::MutationResult {
    count(
        workspace
            .0
            .forget_archived_tabs(&urls, has_range.then_some(begin_secs..=end_secs)),
    )
}

fn placement(
    kind: ffi::PlacementKind,
    owner_id: u64,
) -> Result<workspace::Placement, &'static str> {
    match kind {
        ffi::PlacementKind::Temporary => Ok(workspace::Placement::Temporary { space_id: owner_id }),
        ffi::PlacementKind::Pin => Ok(workspace::Placement::Pin { space_id: owner_id }),
        ffi::PlacementKind::Favorite => Ok(workspace::Placement::Favorite {
            profile_id: owner_id,
        }),
        _ => Err("Unknown item placement"),
    }
}

pub fn set_placement(
    workspace: &mut Workspace,
    item_id: u64,
    kind: ffi::PlacementKind,
    owner_id: u64,
    has_url: bool,
    canonical_url: &str,
) -> ffi::MutationResult {
    if !has_url && !canonical_url.is_empty() {
        return reject("URL supplied without has_url");
    }
    let placement = match placement(kind, owner_id) {
        Ok(value) => value,
        Err(error) => return reject(error),
    };
    outcome(
        workspace
            .0
            .set_placement(item_id, placement, has_url.then_some(canonical_url))
            .map(|()| 0),
    )
}

pub fn remove_saved_item(workspace: &mut Workspace, item_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.remove_saved_item(item_id).map(|()| 0))
}

pub fn set_canonical_url(
    workspace: &mut Workspace,
    item_id: u64,
    url: &str,
) -> ffi::MutationResult {
    outcome(workspace.0.set_canonical_url(item_id, url).map(|()| 0))
}

pub fn set_title(workspace: &mut Workspace, item_id: u64, title: &str) -> ffi::MutationResult {
    outcome(workspace.0.set_title(item_id, title).map(u64::from))
}

pub fn set_temporary_page(
    workspace: &mut Workspace,
    item_id: u64,
    url: &str,
    title: &str,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .set_temporary_page(item_id, url, title)
            .map(u64::from),
    )
}

pub fn dormant_temporary_item(
    workspace: &Workspace,
    space_id: u64,
    url: &str,
    live: &[u64],
) -> u64 {
    workspace
        .0
        .dormant_temporary_item(space_id, url, live)
        .unwrap_or(0)
}

pub fn differs_from_canonical(
    workspace: &Workspace,
    item_id: u64,
    current_url: &str,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .differs_from_canonical(item_id, current_url)
            .map(u64::from),
    )
}

pub fn delete_space(
    workspace: &mut Workspace,
    space_id: u64,
    archive: bool,
    now_secs: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .delete_space(space_id, archive.then_some(now_secs))
            .map(|deletion| deletion.fallback_space),
    )
}

pub fn reveal_item(workspace: &mut Workspace, window_id: u64, item_id: u64) -> ffi::MutationResult {
    outcome(workspace.0.reveal_item(window_id, item_id).map(|()| 0))
}

pub fn select_item_in_its_space(
    workspace: &mut Workspace,
    window_id: u64,
    item_id: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .select_item_in_its_space(window_id, item_id)
            .map(|()| 0),
    )
}

pub fn unsave_item(
    workspace: &mut Workspace,
    item_id: u64,
    live_space: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .unsave_item(item_id, (live_space != 0).then_some(live_space))
            .map(|()| 0),
    )
}

pub fn move_item_before(
    workspace: &mut Workspace,
    item_id: u64,
    before: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .move_item_before(item_id, (before != 0).then_some(before))
            .map(|()| 0),
    )
}

pub fn move_space_before(
    workspace: &mut Workspace,
    space_id: u64,
    before: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .move_space_before(space_id, (before != 0).then_some(before))
            .map(|()| 0),
    )
}

pub fn reconcile_live_items(workspace: &mut Workspace, live: &[u64]) -> ffi::MutationResult {
    outcome(workspace.0.reconcile_live_items(live).map(|()| 0))
}

pub fn create_folder(workspace: &mut Workspace, space_id: u64, name: &str) -> ffi::MutationResult {
    outcome(workspace.0.create_folder(space_id, name))
}

pub fn export_space_into_folder(
    workspace: &mut Workspace,
    source_space_id: u64,
    target_space_id: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .export_space_into_folder(source_space_id, target_space_id),
    )
}

pub fn rename_folder(workspace: &mut Workspace, folder_id: u64, name: &str) -> ffi::MutationResult {
    outcome(workspace.0.rename_folder(folder_id, name).map(|()| 0))
}

pub fn set_folder_open(
    workspace: &mut Workspace,
    folder_id: u64,
    open: bool,
) -> ffi::MutationResult {
    outcome(workspace.0.set_folder_open(folder_id, open).map(u64::from))
}

pub fn move_folder_before(
    workspace: &mut Workspace,
    folder_id: u64,
    before: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .move_folder_before(folder_id, (before != 0).then_some(before))
            .map(|()| 0),
    )
}

pub fn rename_item(workspace: &mut Workspace, item_id: u64, name: &str) -> ffi::MutationResult {
    outcome(workspace.0.rename_item(item_id, name).map(u64::from))
}

#[allow(clippy::too_many_arguments)]
pub fn drop_item(
    workspace: &mut Workspace,
    window_id: u64,
    item_id: u64,
    target: ffi::DropTargetKind,
    folder_id: u64,
    before: u64,
    has_url: bool,
    page_url: &str,
) -> ffi::MutationResult {
    let before = (before != 0).then_some(before);
    let target = match target {
        ffi::DropTargetKind::Pins => workspace::DropTarget::Pins { before },
        ffi::DropTargetKind::Folder => workspace::DropTarget::Folder {
            folder: folder_id,
            before,
        },
        ffi::DropTargetKind::Today => workspace::DropTarget::Today { before },
        _ => return reject("Unknown drop target"),
    };
    outcome(
        workspace
            .0
            .drop_item(window_id, item_id, target, has_url.then_some(page_url))
            .map(|()| 0),
    )
}

pub fn move_pin_to_folder(
    workspace: &mut Workspace,
    item_id: u64,
    folder_id: u64,
    before: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .move_pin_to_folder(
                item_id,
                (folder_id != 0).then_some(folder_id),
                (before != 0).then_some(before),
            )
            .map(|()| 0),
    )
}

pub fn delete_folder(
    workspace: &mut Workspace,
    folder_id: u64,
    archive: bool,
    now_secs: u64,
) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .delete_folder(folder_id, archive.then_some(now_secs))
            .map(|deletion| deletion.archived as u64),
    )
}

pub fn create_split(workspace: &mut Workspace, left: u64, right: u64) -> ffi::MutationResult {
    outcome(workspace.0.create_split(left, right).map(u64::from))
}

pub fn separate_split(workspace: &mut Workspace, item_id: u64) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .separate_split(item_id)
            .map(|split| split.map_or(0, |split| split.partner(item_id))),
    )
}

fn split_record(split: workspace::Split) -> ffi::SplitRecord {
    ffi::SplitRecord {
        left: split.left,
        right: split.right,
    }
}

fn folder_record(folder: workspace::Folder) -> ffi::FolderRecord {
    ffi::FolderRecord {
        id: folder.id,
        space_id: folder.space_id,
        name: folder.name,
        open: folder.open,
        items: folder.items,
    }
}

fn space_record(space: workspace::Space) -> ffi::SpaceRecord {
    let (icon_kind, icon) = match space.icon {
        None => (ffi::SpaceIconKind::Unset, String::new()),
        Some(workspace::SpaceIcon::Emoji(text)) => (ffi::SpaceIconKind::Emoji, text),
        Some(workspace::SpaceIcon::Named(name)) => (ffi::SpaceIconKind::Named, name),
    };
    ffi::SpaceRecord {
        id: space.id,
        profile_id: space.profile_id,
        name: space.name,
        icon_kind,
        icon,
        theme: theme_record(space.theme),
        agent: space.agent,
    }
}

fn item_record(item: workspace::Item) -> ffi::ItemRecord {
    let (placement, owner_id) = match item.placement {
        workspace::Placement::Temporary { space_id } => (ffi::PlacementKind::Temporary, space_id),
        workspace::Placement::Pin { space_id } => (ffi::PlacementKind::Pin, space_id),
        workspace::Placement::Favorite { profile_id } => (ffi::PlacementKind::Favorite, profile_id),
    };
    ffi::ItemRecord {
        id: item.id,
        placement,
        owner_id,
        has_url: item.canonical_url.is_some(),
        canonical_url: item.canonical_url.unwrap_or_default(),
        title: item.title,
        name: item.name,
        last_viewed_secs: item.last_viewed_secs,
    }
}

pub fn sidebar_snapshot(workspace: &Workspace, window_id: u64) -> ffi::SidebarResult {
    match workspace.0.sidebar_snapshot(window_id) {
        Ok(sidebar) => ffi::SidebarResult {
            row_order: sidebar.row_order(),
            split_candidates: sidebar.split_candidates(),
            pins_collapsed: sidebar.pins_collapsed,
            ok: true,
            error: String::new(),
            window_id: sidebar.window_id,
            current_space: space_record(sidebar.current_space),
            selected_item_id: sidebar.selected_item_id.unwrap_or(0),
            spaces: sidebar.spaces.into_iter().map(space_record).collect(),
            favorites: sidebar.favorites.into_iter().map(item_record).collect(),
            pins: sidebar.pins.into_iter().map(item_record).collect(),
            temporary: sidebar.temporary.into_iter().map(item_record).collect(),
            space_pins: sidebar.space_pins.into_iter().map(item_record).collect(),
            folders: sidebar.folders.into_iter().map(folder_record).collect(),
            splits: sidebar.splits.into_iter().map(split_record).collect(),
        },
        Err(error) => ffi::SidebarResult {
            ok: false,
            error: error.to_string(),
            window_id,
            current_space: ffi::SpaceRecord {
                id: 0,
                profile_id: 0,
                name: String::new(),
                icon_kind: ffi::SpaceIconKind::Unset,
                icon: String::new(),
                theme: theme_record(None),
                agent: false,
            },
            selected_item_id: 0,
            spaces: Vec::new(),
            favorites: Vec::new(),
            pins: Vec::new(),
            temporary: Vec::new(),
            space_pins: Vec::new(),
            folders: Vec::new(),
            splits: Vec::new(),
            row_order: Vec::new(),
            split_candidates: Vec::new(),
            pins_collapsed: false,
        },
    }
}

pub fn export_snapshot(workspace: &Workspace) -> ffi::StateSnapshot {
    let state = workspace.0.snapshot();
    ffi::StateSnapshot {
        version: state.version,
        profile_id: state.profile_id,
        next_space_id: state.next_space_id,
        next_item_id: state.next_item_id,
        next_window_id: state.next_window_id,
        spaces: state.spaces.into_iter().map(space_record).collect(),
        items: state.items.into_iter().map(item_record).collect(),
        windows: state
            .windows
            .into_iter()
            .map(|window| ffi::WindowRecord {
                id: window.id,
                space_id: window.space_id,
                selected_item_id: window.selected_item_id.unwrap_or(0),
            })
            .collect(),
        archived: state.archived.into_iter().map(archived_record).collect(),
        open_secs: state.open_secs,
        archive_interval_secs: state.archive_interval.seconds(),
        folders: state.folders.into_iter().map(folder_record).collect(),
        next_folder_id: state.next_folder_id,
        splits: state.splits.into_iter().map(split_record).collect(),
        quick_start_pending: state.quick_start_pending,
    }
}

pub fn restore_snapshot(
    workspace: &mut Workspace,
    snapshot: ffi::StateSnapshot,
) -> ffi::MutationResult {
    if snapshot.spaces.len() > workspace::MAX_SPACES
        || snapshot.items.len() > workspace::MAX_ITEMS
        || snapshot.windows.len() > workspace::MAX_WINDOWS
        || snapshot
            .spaces
            .iter()
            .any(|space| space.name.len() > workspace::MAX_SPACE_NAME_BYTES)
        || snapshot.items.iter().any(|item| {
            item.canonical_url.len() > workspace::MAX_URL_BYTES
                || item.title.len() > workspace::MAX_TITLE_BYTES
                || (!item.has_url && !item.canonical_url.is_empty())
        })
    {
        return reject("Snapshot exceeds bridge bounds or has inconsistent URL presence");
    }
    let mut items = Vec::with_capacity(snapshot.items.len());
    for item in snapshot.items {
        let placement = match placement(item.placement, item.owner_id) {
            Ok(value) => value,
            Err(error) => return reject(error),
        };
        items.push(workspace::Item {
            id: item.id,
            placement,
            canonical_url: item.has_url.then_some(item.canonical_url),
            title: item.title,
            // The core drops a bad name on restore; this only bounds it.
            name: if item.name.len() <= workspace::MAX_TITLE_BYTES {
                item.name
            } else {
                String::new()
            },
            last_viewed_secs: item.last_viewed_secs,
        });
    }
    outcome(
        workspace
            .0
            .restore(workspace::WorkspaceSnapshot {
                version: snapshot.version,
                profile_id: snapshot.profile_id,
                next_space_id: snapshot.next_space_id,
                next_item_id: snapshot.next_item_id,
                next_window_id: snapshot.next_window_id,
                spaces: snapshot
                    .spaces
                    .into_iter()
                    .map(|space| workspace::Space {
                        id: space.id,
                        profile_id: space.profile_id,
                        // An oversized or malformed icon reads as none, like
                        // any other bad icon on restore.
                        icon: if space.icon.len() > workspace::MAX_SPACE_ICON_BYTES {
                            None
                        } else {
                            space_icon(space.icon_kind, &space.icon).unwrap_or_default()
                        },
                        // The core drops a bad theme on restore.
                        theme: space_theme(&space.theme),
                        // The core keeps one mark per Profile on restore.
                        agent: space.agent,
                        name: space.name,
                    })
                    .collect(),
                items,
                windows: snapshot
                    .windows
                    .into_iter()
                    .map(|window| workspace::WindowState {
                        id: window.id,
                        space_id: window.space_id,
                        selected_item_id: (window.selected_item_id != 0)
                            .then_some(window.selected_item_id),
                    })
                    .collect(),
                // An oversized entry is dropped like any other bad one; the
                // workspace keeps the newest entries within its limit.
                archived: snapshot
                    .archived
                    .into_iter()
                    .filter(|tab| {
                        tab.url.len() <= workspace::MAX_URL_BYTES
                            && tab.title.len() <= workspace::MAX_TITLE_BYTES
                    })
                    .map(|tab| workspace::ArchivedTab {
                        id: tab.id,
                        space_id: tab.space_id,
                        url: tab.url,
                        title: tab.title,
                        archived_secs: tab.archived_secs,
                    })
                    .collect(),
                open_secs: snapshot.open_secs,
                archive_interval: workspace::archive::ArchiveInterval::from_seconds(
                    snapshot.archive_interval_secs,
                )
                .unwrap_or_default(),
                // The core drops bad folders and entries on restore.
                folders: snapshot
                    .folders
                    .into_iter()
                    .take(workspace::MAX_FOLDERS)
                    .filter(|folder| {
                        folder.name.len() <= workspace::MAX_FOLDER_NAME_BYTES
                            && folder.items.len() <= workspace::MAX_ITEMS
                    })
                    .map(|folder| workspace::Folder {
                        id: folder.id,
                        space_id: folder.space_id,
                        name: folder.name,
                        open: folder.open,
                        items: folder.items,
                    })
                    .collect(),
                next_folder_id: snapshot.next_folder_id,
                // The core drops bad splits on restore.
                splits: snapshot
                    .splits
                    .into_iter()
                    .take(workspace::MAX_SPLITS)
                    .map(|split| workspace::Split {
                        left: split.left,
                        right: split.right,
                    })
                    .collect(),
                quick_start_pending: snapshot.quick_start_pending,
            })
            .map(|()| 0),
    )
}

struct ControlSlot {
    epoch: u64,
    host: Option<Arc<control::ipc::Host>>,
}

static CONTROL: Mutex<ControlSlot> = Mutex::new(ControlSlot {
    epoch: 0,
    host: None,
});

fn control_slot() -> MutexGuard<'static, ControlSlot> {
    CONTROL.lock().unwrap_or_else(PoisonError::into_inner)
}

pub fn enable_control_host() -> u64 {
    let mut slot = control_slot();
    if slot.host.is_some() {
        return 0;
    }
    slot.epoch = slot.epoch.wrapping_add(1).max(1);
    slot.epoch
}

pub fn start_control_host(epoch: u64) -> String {
    let mut slot = control_slot();
    if slot.epoch != epoch || slot.host.is_some() {
        return String::new();
    }
    let Some(dir) = control::ipc::default_dir() else {
        return format!(
            "set HOME or {} to choose the control directory",
            control::ipc::DIR_ENV
        );
    };
    match control::ipc::Host::start_gated(
        &dir,
        control::ipc::REPLY_TIMEOUT,
        control::ipc::APPROVAL_TIMEOUT,
    ) {
        Ok(host) => {
            slot.host = Some(Arc::new(host));
            String::new()
        }
        Err(error) => format!("could not serve in {}: {error}", dir.display()),
    }
}

pub fn next_control_request(epoch: u64) -> Box<ControlReply> {
    let host = {
        let slot = control_slot();
        if slot.epoch == epoch {
            slot.host.clone()
        } else {
            None
        }
    };
    let (pending, approval) = match host.and_then(|host| host.next_incoming()) {
        Some(control::ipc::Incoming::Request(pending)) => (Some(pending), None),
        Some(control::ipc::Incoming::Hello(approval)) => (None, Some(approval)),
        None => (None, None),
    };
    Box::new(ControlReply {
        pending,
        approval,
        action: None,
        taken: None,
        profile: String::new(),
        answered: None,
    })
}

pub fn has_control_request(reply: &ControlReply) -> bool {
    reply.pending.is_some() || reply.approval.is_some()
}

pub fn control_request_live(reply: &ControlReply) -> bool {
    if let Some(approval) = &reply.approval {
        return !approval.abandoned();
    }
    reply
        .pending
        .as_ref()
        .is_some_and(|pending| !pending.abandoned())
}

pub fn is_control_hello(reply: &ControlReply) -> bool {
    reply.approval.is_some()
}

pub fn control_hello_name(reply: &ControlReply) -> String {
    reply
        .approval
        .as_ref()
        .map(|approval| approval.name().to_owned())
        .unwrap_or_default()
}

pub fn control_hello_version(reply: &ControlReply) -> String {
    reply
        .approval
        .as_ref()
        .map(|approval| approval.version().to_owned())
        .unwrap_or_default()
}

pub fn decide_control_hello(reply: &mut ControlReply, allow: bool) {
    if let Some(approval) = reply.approval.take() {
        approval.decide(allow);
    }
}

pub fn control_approval_timeout_ms() -> u64 {
    u64::try_from(control::ipc::APPROVAL_TIMEOUT.as_millis()).unwrap_or(u64::MAX)
}

pub fn control_client_label(text: &str) -> String {
    control::ipc::client_label(text)
}

fn control_host() -> Option<Arc<control::ipc::Host>> {
    control_slot().host.clone()
}

pub fn active_control_client() -> String {
    control_host()
        .and_then(|host| host.active_client())
        .unwrap_or_default()
}

pub fn revoke_control_client(name: &str) {
    if let Some(host) = control_host() {
        host.revoke(name);
    }
}

pub fn answer_control_request(
    reply: &mut ControlReply,
    workspace: &mut Workspace,
    window_id: u64,
    profile: &str,
) -> ffi::ControlPlan {
    use control::TabAction;
    use ffi::TabActionKind as Kind;
    let mut plan = ffi::ControlPlan {
        changed: false,
        action: Kind::Answered,
        item: 0,
        space: 0,
        timeout_ms: 0,
        url: String::new(),
        items: Vec::new(),
    };
    let Some(pending) = reply.pending.take() else {
        return plan;
    };
    reply.profile = control::profile_tag(profile);
    let before = workspace.0.snapshot();
    let routed = control::in_profile(&reply.profile, || {
        control::route(&mut workspace.0, window_id, pending.request())
    });
    plan.changed = workspace.0.snapshot() != before;
    let action = match routed {
        control::Routed::Answered(response) => {
            if plan.changed {
                reply.answered = Some((pending, response));
            } else {
                pending.reply(response);
            }
            return plan;
        }
        control::Routed::Tab(action) => action,
    };
    reply.pending = Some(pending);
    (plan.action, plan.item) = match action {
        TabAction::List { items } => {
            plan.items = items;
            (Kind::List, 0)
        }
        TabAction::Open { space_id, url } => {
            (plan.space, plan.url) = (space_id, url);
            (Kind::Open, 0)
        }
        TabAction::Navigate { item, url } => {
            plan.url = url;
            (Kind::Navigate, item)
        }
        TabAction::Activate { item } => (Kind::Activate, item),
        TabAction::Close { item } => (Kind::Close, item),
        TabAction::Back { item } => (Kind::Back, item),
        TabAction::Forward { item } => (Kind::Forward, item),
        TabAction::Reload { item } => (Kind::Reload, item),
        TabAction::Snapshot { item } => (Kind::Snapshot, item),
        TabAction::Screenshot { item } => (Kind::Screenshot, item),
        TabAction::Wait { item, timeout_ms } => {
            plan.timeout_ms = timeout_ms;
            (Kind::Wait, item)
        }
        TabAction::Act(action) => {
            let item = action.item;
            reply.action = Some(action);
            (Kind::Act, item)
        }
    };
    plan
}

pub fn send_control_answer(reply: &mut ControlReply) {
    if let Some((pending, response)) = reply.answered.take() {
        pending.reply(response);
    }
}

pub fn answer_tab_request(
    reply: &mut ControlReply,
    status: ffi::TabStatus,
    reason: &str,
    tabs: Vec<ffi::TabStateRecord>,
) {
    use ffi::TabStatus as Status;
    let Some(pending) = reply.pending.take() else {
        return;
    };
    let tabs: Vec<control::TabState> = tabs.into_iter().map(tab_state).collect();
    let reason = reason.to_owned();
    let result = match status {
        Status::Done => control::TabResult::Done(tabs),
        Status::NotCommitted => control::TabResult::Unknown { reason, tabs },
        other => control::TabResult::Failed(failure(other, reason)),
    };
    let response = control::in_profile(&reply.profile, || {
        control::tab_response(pending.request(), result)
    });
    pending.reply(response);
}

fn tab_state(tab: ffi::TabStateRecord) -> control::TabState {
    control::TabState {
        item: tab.item,
        url: tab.url,
        title: tab.title,
        loading: tab.loading,
        can_go_back: tab.can_go_back,
        can_go_forward: tab.can_go_forward,
    }
}

fn failure(status: ffi::TabStatus, reason: String) -> control::Outcome {
    use ffi::TabStatus as Status;
    match status {
        Status::Denied => control::Outcome::Denied { reason },
        Status::Stale => control::Outcome::Stale { reason },
        Status::TimedOut => control::Outcome::Timeout,
        _ => control::Outcome::Invalid { reason },
    }
}

pub fn control_navigation_wait_ms() -> u64 {
    control::NAVIGATION_WAIT_MS
}

/// References of the latest snapshot of each recent main document.
static SNAPSHOTS: Mutex<control::snapshot::SnapshotStore> =
    Mutex::new(control::snapshot::SnapshotStore::new());

#[allow(clippy::too_many_arguments)]
pub fn answer_snapshot_request(
    reply: &mut ControlReply,
    status: ffi::TabStatus,
    reason: &str,
    tab: ffi::TabStateRecord,
    document: u64,
    root: i32,
    focus: i32,
    source_truncated: bool,
    nodes: Vec<ffi::AxNodeRecord>,
) {
    use control::snapshot::{SnapshotPage, SnapshotResult};
    let Some(pending) = reply.pending.take() else {
        return;
    };
    let result = if status == ffi::TabStatus::Done {
        let nodes = nodes.into_iter().map(ax_node).collect();
        SnapshotResult::Done(SnapshotPage {
            tab: tab_state(tab),
            document,
            root,
            focus,
            source_truncated,
            nodes,
        })
    } else {
        SnapshotResult::Failed(failure(status, reason.to_owned()))
    };
    let mut store = SNAPSHOTS.lock().unwrap_or_else(PoisonError::into_inner);
    let response = control::in_profile(&reply.profile, || {
        control::snapshot::snapshot_response(pending.request(), result, &mut store)
    });
    drop(store);
    pending.reply(response);
}

pub fn answer_screenshot_request(
    reply: &mut ControlReply,
    status: ffi::TabStatus,
    reason: &str,
    shot: ffi::ScreenshotRecord,
) {
    use control::{ScreenshotData, ScreenshotResult};
    let Some(pending) = reply.pending.take() else {
        return;
    };
    let result = if status == ffi::TabStatus::Done {
        ScreenshotResult::Done(ScreenshotData {
            item: shot.item,
            url: shot.url,
            title: shot.title,
            mime_type: shot.mime_type,
            width: shot.width,
            height: shot.height,
            data_base64: shot.data_base64,
        })
    } else {
        ScreenshotResult::Failed(failure(status, reason.to_owned()))
    };
    let response = control::in_profile(&reply.profile, || {
        control::screenshot_response(pending.request(), result)
    });
    pending.reply(response);
}

fn ax_node(node: ffi::AxNodeRecord) -> control::snapshot::AxNode {
    use control::snapshot::{AxNode, Checked};
    AxNode {
        id: node.id,
        role: node.role,
        name: node.name,
        value: node.value,
        url: node.url,
        children: node.children,
        level: node.level,
        focusable: node.focusable,
        clickable: node.clickable,
        invisible: node.invisible,
        ignored: node.ignored,
        checked: match node.checked {
            1 => Checked::Yes,
            2 => Checked::Mixed,
            _ => Checked::No,
        },
        expanded: match node.expanded {
            1 => Some(true),
            2 => Some(false),
            _ => None,
        },
        disabled: node.disabled,
        selected: node.selected,
        required: node.required,
        protected: node.is_protected,
        editable: node.editable,
        read_only: node.read_only,
    }
}

fn key_record(key: control::actions::KeySpec) -> ffi::KeyRecord {
    ffi::KeyRecord {
        key: key.key,
        code: key.code,
        text: key.text,
        shift: key.shift,
        key_code: key.key_code,
    }
}

pub fn begin_page_action(reply: &mut ControlReply, document: u64) -> ffi::ActionStart {
    use control::actions::ActKind;
    let mut start = ffi::ActionStart {
        ok: false,
        kind: ffi::PageActKind::Click,
        node: 0,
        focus_first: false,
        text: String::new(),
        value: String::new(),
        key: key_record(control::actions::KeySpec::default()),
    };
    if reply.pending.is_none() || reply.action.is_none() {
        refuse_control_request(reply, "No page action to begin");
        return start;
    }
    let (Some(pending), Some(action)) = (reply.pending.as_ref(), reply.action.as_ref()) else {
        return start;
    };
    let mut store = SNAPSHOTS.lock().unwrap_or_else(PoisonError::into_inner);
    match control::actions::prepare(&mut store, document, action) {
        Ok(prepared) => {
            start.ok = true;
            start.node = prepared.node;
            start.focus_first = prepared.focus_first;
            reply.taken = Some(prepared.taken);
            start.kind = match &action.kind {
                ActKind::Click => ffi::PageActKind::Click,
                ActKind::Fill { text } => {
                    start.text = text.clone();
                    ffi::PageActKind::Fill
                }
                ActKind::Select { value } => {
                    start.value = value.clone();
                    ffi::PageActKind::Select
                }
                ActKind::Press { key } => {
                    start.key = key_record(key.clone());
                    ffi::PageActKind::Press
                }
                ActKind::Scroll { direction } => {
                    start.text = direction.as_str().to_owned();
                    ffi::PageActKind::Scroll
                }
            };
        }
        Err(outcome) => {
            drop(store);
            let response = control::answer_outcome(pending.request(), outcome);
            if let Some(pending) = reply.pending.take() {
                pending.reply(response);
            }
        }
    }
    start
}

pub fn choose_select_option(
    reply: &mut ControlReply,
    options: Vec<ffi::SelectOptionRecord>,
) -> ffi::ChoiceResult {
    use control::actions::{ActKind, SelectOption};
    let wanted = match reply.action.as_ref().map(|action| &action.kind) {
        Some(ActKind::Select { value }) => value.clone(),
        _ => String::new(),
    };
    let options: Vec<SelectOption> = options
        .into_iter()
        .map(|option| SelectOption {
            value: option.value,
            text: option.text,
            disabled: option.disabled,
        })
        .collect();
    match control::actions::choose_option(&options, &wanted) {
        Ok(value) => ffi::ChoiceResult { ok: true, value },
        Err(outcome) => {
            finish_action(reply, control::actions::ActionResult::Refused(outcome));
            ffi::ChoiceResult {
                ok: false,
                value: String::new(),
            }
        }
    }
}

fn finish_action(reply: &mut ControlReply, result: control::actions::ActionResult) {
    reply.action = None;
    let Some(pending) = reply.pending.take() else {
        return;
    };
    let taken = reply.taken.take().unwrap_or_default();
    let mut store = SNAPSHOTS.lock().unwrap_or_else(PoisonError::into_inner);
    let response = control::in_profile(&reply.profile, || {
        control::actions::action_response(pending.request(), result, taken, &mut store)
    });
    drop(store);
    pending.reply(response);
}

#[allow(clippy::too_many_arguments)]
pub fn answer_action_request(
    reply: &mut ControlReply,
    status: ffi::ActionStatus,
    reason: &str,
    tab: ffi::TabStateRecord,
    navigated: bool,
    settled: bool,
    document: u64,
    root: i32,
    focus: i32,
    source_truncated: bool,
    nodes: Vec<ffi::AxNodeRecord>,
) {
    use control::actions::ActionResult;
    use ffi::ActionStatus as Status;
    let reason = reason.to_owned();
    let result = match status {
        Status::Done => ActionResult::Done {
            page: control::snapshot::SnapshotPage {
                tab: tab_state(tab),
                document,
                root,
                focus,
                source_truncated,
                nodes: nodes.into_iter().map(ax_node).collect(),
            },
            navigated,
            settled,
        },
        Status::Refused => ActionResult::Refused(control::Outcome::Invalid { reason }),
        Status::Stale => ActionResult::Refused(control::Outcome::Stale { reason }),
        Status::TimedOut => ActionResult::Refused(control::Outcome::Timeout),
        _ => ActionResult::Unknown {
            reason,
            tab: tab_state(tab),
            navigated,
        },
    };
    finish_action(reply, result);
}

pub fn control_action_limits() -> ffi::ActionLimits {
    use control::actions as limits;
    ffi::ActionLimits {
        action_wait_ms: limits::ACTION_WAIT_MS,
        settle_cap_ms: limits::SETTLE_CAP_MS,
        snapshot_timeout_ms: limits::ACTION_SNAPSHOT_TIMEOUT_MS,
        max_select_options: limits::MAX_SELECT_OPTIONS,
    }
}

pub fn control_snapshot_timeout_ms() -> u64 {
    control::snapshot::SNAPSHOT_TIMEOUT_MS
}

pub fn control_snapshot_max_nodes() -> usize {
    control::snapshot::MAX_SNAPSHOT_NODES
}

pub fn refuse_control_request(reply: &mut ControlReply, reason: &str) {
    if let Some(approval) = reply.approval.take() {
        approval.refuse(reason);
    }
    if let Some(pending) = reply.pending.take() {
        let response = control::deny(pending.request(), reason);
        pending.reply(response);
    }
}

pub fn stop_control_host() {
    let host = {
        let mut slot = control_slot();
        slot.epoch = slot.epoch.wrapping_add(1);
        slot.host.take()
    };
    if let Some(host) = host {
        host.stop();
    }
}

fn boost_color_mode_to_ffi(mode: boosts::ColorMode) -> ffi::BoostColorMode {
    match mode {
        boosts::ColorMode::None => ffi::BoostColorMode::None,
        boosts::ColorMode::Invert => ffi::BoostColorMode::Invert,
        boosts::ColorMode::Palette => ffi::BoostColorMode::Palette,
    }
}

fn boost_color_mode_from_ffi(mode: ffi::BoostColorMode) -> boosts::ColorMode {
    match mode {
        ffi::BoostColorMode::Invert => boosts::ColorMode::Invert,
        ffi::BoostColorMode::Palette => boosts::ColorMode::Palette,
        _ => boosts::ColorMode::None,
    }
}

fn boost_font_to_ffi(font: boosts::FontPreset) -> ffi::BoostFontPreset {
    match font {
        boosts::FontPreset::Default => ffi::BoostFontPreset::Default,
        boosts::FontPreset::SystemSans => ffi::BoostFontPreset::SystemSans,
        boosts::FontPreset::SystemSerif => ffi::BoostFontPreset::SystemSerif,
        boosts::FontPreset::SystemMono => ffi::BoostFontPreset::SystemMono,
        boosts::FontPreset::Rounded => ffi::BoostFontPreset::Rounded,
        boosts::FontPreset::Geometric => ffi::BoostFontPreset::Geometric,
        boosts::FontPreset::Humanist => ffi::BoostFontPreset::Humanist,
        boosts::FontPreset::Grotesk => ffi::BoostFontPreset::Grotesk,
        boosts::FontPreset::Didone => ffi::BoostFontPreset::Didone,
        boosts::FontPreset::SlabSerif => ffi::BoostFontPreset::SlabSerif,
        boosts::FontPreset::ClassicSerif => ffi::BoostFontPreset::ClassicSerif,
        boosts::FontPreset::Typewriter => ffi::BoostFontPreset::Typewriter,
        boosts::FontPreset::Handwritten => ffi::BoostFontPreset::Handwritten,
        boosts::FontPreset::Condensed => ffi::BoostFontPreset::Condensed,
        boosts::FontPreset::Wide => ffi::BoostFontPreset::Wide,
        boosts::FontPreset::Comic => ffi::BoostFontPreset::Comic,
    }
}

fn boost_font_from_ffi(font: ffi::BoostFontPreset) -> boosts::FontPreset {
    match font {
        ffi::BoostFontPreset::SystemSans => boosts::FontPreset::SystemSans,
        ffi::BoostFontPreset::SystemSerif => boosts::FontPreset::SystemSerif,
        ffi::BoostFontPreset::SystemMono => boosts::FontPreset::SystemMono,
        ffi::BoostFontPreset::Rounded => boosts::FontPreset::Rounded,
        ffi::BoostFontPreset::Geometric => boosts::FontPreset::Geometric,
        ffi::BoostFontPreset::Humanist => boosts::FontPreset::Humanist,
        ffi::BoostFontPreset::Grotesk => boosts::FontPreset::Grotesk,
        ffi::BoostFontPreset::Didone => boosts::FontPreset::Didone,
        ffi::BoostFontPreset::SlabSerif => boosts::FontPreset::SlabSerif,
        ffi::BoostFontPreset::ClassicSerif => boosts::FontPreset::ClassicSerif,
        ffi::BoostFontPreset::Typewriter => boosts::FontPreset::Typewriter,
        ffi::BoostFontPreset::Handwritten => boosts::FontPreset::Handwritten,
        ffi::BoostFontPreset::Condensed => boosts::FontPreset::Condensed,
        ffi::BoostFontPreset::Wide => boosts::FontPreset::Wide,
        ffi::BoostFontPreset::Comic => boosts::FontPreset::Comic,
        _ => boosts::FontPreset::Default,
    }
}

fn boost_text_transform_to_ffi(tt: boosts::TextTransform) -> ffi::BoostTextTransform {
    match tt {
        boosts::TextTransform::None => ffi::BoostTextTransform::None,
        boosts::TextTransform::Uppercase => ffi::BoostTextTransform::Uppercase,
        boosts::TextTransform::Lowercase => ffi::BoostTextTransform::Lowercase,
        boosts::TextTransform::Capitalize => ffi::BoostTextTransform::Capitalize,
    }
}

fn boost_text_transform_from_ffi(tt: ffi::BoostTextTransform) -> boosts::TextTransform {
    match tt {
        ffi::BoostTextTransform::Uppercase => boosts::TextTransform::Uppercase,
        ffi::BoostTextTransform::Lowercase => boosts::TextTransform::Lowercase,
        ffi::BoostTextTransform::Capitalize => boosts::TextTransform::Capitalize,
        _ => boosts::TextTransform::None,
    }
}

fn boost_appearance_to_ffi(appearance: &boosts::BoostAppearance) -> ffi::BoostAppearanceRecord {
    let palette = match appearance.palette {
        Some(p) => ffi::BoostPaletteRecord {
            set: true,
            background: p.background,
            text: p.text,
            accent: p.accent,
            contrast: p.contrast,
            brightness: p.brightness,
            saturation: p.saturation,
            tint_strength: p.tint_strength,
        },
        None => ffi::BoostPaletteRecord {
            set: false,
            background: 0,
            text: 0,
            accent: 0,
            contrast: 50,
            brightness: 50,
            saturation: 50,
            tint_strength: 50,
        },
    };
    ffi::BoostAppearanceRecord {
        color_mode: boost_color_mode_to_ffi(appearance.color_mode),
        palette,
        font: boost_font_to_ffi(appearance.font),
        text_scale_pct: appearance.text_scale_pct,
        text_transform: boost_text_transform_to_ffi(appearance.text_transform),
    }
}

fn boost_appearance_from_ffi(record: ffi::BoostAppearanceRecord) -> boosts::BoostAppearance {
    let palette = if record.palette.set {
        Some(boosts::BoostPalette {
            background: record.palette.background,
            text: record.palette.text,
            accent: record.palette.accent,
            contrast: record.palette.contrast,
            brightness: record.palette.brightness,
            saturation: record.palette.saturation,
            tint_strength: record.palette.tint_strength,
        })
    } else {
        None
    };
    boosts::BoostAppearance {
        color_mode: boost_color_mode_from_ffi(record.color_mode),
        palette,
        font: boost_font_from_ffi(record.font),
        text_scale_pct: record.text_scale_pct,
        text_transform: boost_text_transform_from_ffi(record.text_transform),
    }
}

fn boost_record_to_ffi(boost: &boosts::Boost) -> ffi::BoostRecord {
    let display_host = boost
        .origins
        .first()
        .and_then(|origin| boosts::display_host(origin))
        .unwrap_or_default();
    let has_inert_script = boost
        .script_payload
        .as_deref()
        .is_some_and(|s| !s.trim().is_empty());
    ffi::BoostRecord {
        id: boost.id,
        name: boost.name.clone(),
        origins: boost.origins.clone(),
        display_host,
        include_subdomains: boost.include_subdomains,
        apply_to_subframes: boost.apply_to_subframes,
        enabled: boost.enabled,
        version: boost.version,
        updated_secs: boost.updated_secs,
        appearance: boost_appearance_to_ffi(&boost.appearance),
        hidden_selectors: boost.hidden_selectors.clone(),
        custom_css: boost.custom_css.clone(),
        can_undo: !boost.revisions.is_empty(),
        has_inert_script,
    }
}

fn boost_bool_mutation(result: Result<bool, boosts::BoostsError>) -> ffi::MutationResult {
    match result {
        Ok(changed) => ffi::MutationResult {
            ok: true,
            id: u64::from(changed),
            error: String::new(),
        },
        Err(error) => ffi::MutationResult {
            ok: false,
            id: 0,
            error: error.to_string(),
        },
    }
}

pub fn boosts_limits() -> ffi::BoostLimits {
    ffi::BoostLimits {
        version: boosts::SNAPSHOT_VERSION,
        max_boosts: boosts::MAX_BOOSTS,
        max_name_bytes: boosts::MAX_NAME_BYTES,
        max_origins_per_boost: boosts::MAX_ORIGINS_PER_BOOST,
        max_origin_bytes: boosts::MAX_ORIGIN_BYTES,
        max_hidden_selectors: boosts::MAX_HIDDEN_SELECTORS,
        max_selector_bytes: boosts::MAX_SELECTOR_BYTES,
        max_custom_css_bytes: boosts::MAX_CUSTOM_CSS_BYTES,
        max_revisions: boosts::MAX_REVISIONS,
        max_file_bytes: boosts::MAX_FILE_BYTES,
    }
}

pub fn new_boost_store() -> Box<BoostStore> {
    Box::new(BoostStore(boosts::BoostStore::new()))
}

pub fn boosts_encode_json(store: &BoostStore) -> String {
    boosts::encode_snapshot(&store.0.snapshot())
}

pub fn boosts_restore_json(store: &mut BoostStore, bytes: &[u8]) -> ffi::BoostLoadResult {
    let snapshot = match boosts::decode_snapshot(bytes) {
        Ok(snapshot) => snapshot,
        Err(boosts::BoostsError::UnsupportedSnapshotVersion(v)) => {
            return ffi::BoostLoadResult {
                ok: false,
                damaged: false,
                error: boosts::BoostsError::UnsupportedSnapshotVersion(v).to_string(),
            };
        }
        Err(error) => {
            return ffi::BoostLoadResult {
                ok: false,
                damaged: true,
                error: error.to_string(),
            };
        }
    };
    match boosts::BoostStore::from_snapshot(snapshot) {
        Ok(restored) => {
            store.0 = restored;
            ffi::BoostLoadResult {
                ok: true,
                damaged: false,
                error: String::new(),
            }
        }
        Err(boosts::BoostsError::UnsupportedSnapshotVersion(v)) => ffi::BoostLoadResult {
            ok: false,
            damaged: false,
            error: boosts::BoostsError::UnsupportedSnapshotVersion(v).to_string(),
        },
        Err(error) => ffi::BoostLoadResult {
            ok: false,
            damaged: true,
            error: error.to_string(),
        },
    }
}

pub fn boosts_global_enabled(store: &BoostStore) -> bool {
    store.0.global_enabled()
}

pub fn boosts_set_global_enabled(store: &mut BoostStore, enabled: bool) -> bool {
    store.0.set_global_enabled(enabled)
}

pub fn boosts_list(store: &BoostStore, query: &str) -> Vec<ffi::BoostRecord> {
    store
        .0
        .search(query)
        .into_iter()
        .map(boost_record_to_ffi)
        .collect()
}

pub fn boosts_get(store: &BoostStore, id: u64) -> Vec<ffi::BoostRecord> {
    store
        .0
        .boost(id)
        .map(boost_record_to_ffi)
        .into_iter()
        .collect()
}

pub fn boosts_matching(
    store: &BoostStore,
    page_origin: &str,
    is_subframe: bool,
) -> Vec<ffi::BoostRecord> {
    store
        .0
        .matching_boosts(page_origin, is_subframe)
        .into_iter()
        .map(boost_record_to_ffi)
        .collect()
}

pub fn boosts_create(
    store: &mut BoostStore,
    name: &str,
    origins: Vec<String>,
    now_secs: u64,
) -> ffi::MutationResult {
    let refs: Vec<&str> = origins.iter().map(String::as_str).collect();
    match store.0.create_boost(name, &refs, now_secs) {
        Ok(id) => ffi::MutationResult {
            ok: true,
            id,
            error: String::new(),
        },
        Err(error) => ffi::MutationResult {
            ok: false,
            id: 0,
            error: error.to_string(),
        },
    }
}

pub fn boosts_rename(
    store: &mut BoostStore,
    id: u64,
    name: &str,
    now_secs: u64,
) -> ffi::MutationResult {
    boost_bool_mutation(store.0.rename_boost(id, name, now_secs))
}

pub fn boosts_set_enabled(
    store: &mut BoostStore,
    id: u64,
    enabled: bool,
    now_secs: u64,
) -> ffi::MutationResult {
    boost_bool_mutation(store.0.set_enabled(id, enabled, now_secs))
}

pub fn boosts_set_scope(
    store: &mut BoostStore,
    id: u64,
    origins: Vec<String>,
    include_subdomains: bool,
    apply_to_subframes: bool,
    now_secs: u64,
) -> ffi::MutationResult {
    let refs: Vec<&str> = origins.iter().map(String::as_str).collect();
    boost_bool_mutation(store.0.set_scope(
        id,
        &refs,
        include_subdomains,
        apply_to_subframes,
        now_secs,
    ))
}

pub fn boosts_set_appearance(
    store: &mut BoostStore,
    id: u64,
    appearance: ffi::BoostAppearanceRecord,
    now_secs: u64,
) -> ffi::MutationResult {
    boost_bool_mutation(
        store
            .0
            .set_appearance(id, boost_appearance_from_ffi(appearance), now_secs),
    )
}

pub fn boosts_add_hidden_selector(
    store: &mut BoostStore,
    id: u64,
    selector: &str,
    now_secs: u64,
) -> ffi::MutationResult {
    boost_bool_mutation(store.0.add_hidden_selector(id, selector, now_secs))
}

pub fn boosts_remove_hidden_selector(
    store: &mut BoostStore,
    id: u64,
    selector: &str,
    now_secs: u64,
) -> ffi::MutationResult {
    boost_bool_mutation(store.0.remove_hidden_selector(id, selector, now_secs))
}

pub fn boosts_clear_hidden_selectors(
    store: &mut BoostStore,
    id: u64,
    now_secs: u64,
) -> ffi::MutationResult {
    boost_bool_mutation(store.0.clear_hidden_selectors(id, now_secs))
}

pub fn boosts_set_custom_css(
    store: &mut BoostStore,
    id: u64,
    custom_css: &str,
    now_secs: u64,
) -> ffi::MutationResult {
    boost_bool_mutation(store.0.set_custom_css(id, custom_css, now_secs))
}

pub fn boosts_reset(store: &mut BoostStore, id: u64, now_secs: u64) -> ffi::MutationResult {
    boost_bool_mutation(store.0.reset_boost(id, now_secs))
}

pub fn boosts_undo(store: &mut BoostStore, id: u64, now_secs: u64) -> ffi::MutationResult {
    boost_bool_mutation(store.0.undo(id, now_secs))
}

pub fn boosts_delete(store: &mut BoostStore, id: u64) -> ffi::MutationResult {
    match store.0.delete_boost(id) {
        Ok(()) => ffi::MutationResult {
            ok: true,
            id: 1,
            error: String::new(),
        },
        Err(error) => ffi::MutationResult {
            ok: false,
            id: 0,
            error: error.to_string(),
        },
    }
}

pub fn boosts_compile_for_origin(
    store: &BoostStore,
    page_origin: &str,
    is_subframe: bool,
) -> String {
    store.0.compile_for_origin(page_origin, is_subframe)
}

pub fn boosts_compile_preview(
    appearance: ffi::BoostAppearanceRecord,
    hidden_selectors: Vec<String>,
    custom_css: &str,
) -> String {
    let preview = boosts::Boost {
        id: 0,
        name: String::new(),
        origins: Vec::new(),
        include_subdomains: false,
        apply_to_subframes: false,
        enabled: true,
        version: 1,
        updated_secs: 0,
        appearance: boost_appearance_from_ffi(appearance),
        hidden_selectors,
        custom_css: custom_css.to_owned(),
        script_payload: None,
        revisions: Vec::new(),
    };
    boosts::compile_boost_css(&preview)
}

pub fn boosts_inspect_css(css: &str) -> Vec<ffi::BoostCssFinding> {
    boosts::inspect_css(css)
        .into_iter()
        .map(|f| ffi::BoostCssFinding {
            line: f.line,
            reason: f.reason.to_string(),
        })
        .collect()
}

pub fn boosts_normalize_origin(raw: &str) -> String {
    boosts::normalize_origin(raw).unwrap_or_default()
}
