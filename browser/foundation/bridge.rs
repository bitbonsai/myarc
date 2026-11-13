chromium::import! {
    "//myarc:control";
    "//myarc:import";
    "//myarc:workspace";
}

use std::sync::{Arc, Mutex, MutexGuard, PoisonError};

pub struct Workspace(workspace::Workspace);

// A parsed Arc Markdown export, plus one browser-canonicalized URL per entry
// once `set_arc_export_canonical_urls` ran (`None` for a rejected URL).
pub struct ArcExport {
    report: import::arc_markdown::Report,
    error: String,
    canonical: Vec<Option<String>>,
}

// One request or hello from a control client, or none once the host has
// stopped. A hello waits in `approval` for the user's answer. An interaction
// command also carries its checked action and, once the
// browser began it, the snapshot references it took. `profile` is the tag of
// the Profile it runs in, which its tab handles carry.
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

    enum PlacementKind {
        Temporary,
        Pin,
        Favorite,
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
    struct SpaceThemeRecord {
        set: bool,
        // 0xRRGGBB.
        color: u32,
        // 0 to 100 percent.
        opacity: u8,
    }

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

    struct ItemRecord {
        id: u64,
        placement: PlacementKind,
        owner_id: u64,
        has_url: bool,
        canonical_url: String,
        // Last known page title of a saved item; empty when unknown.
        title: String,
        // Browser-open clock reading when a temporary item was last seen;
        // zero when not recorded yet.
        last_viewed_secs: u64,
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
    }

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
        // page.click, page.fill, page.select or page.press: begin with
        // begin_page_action and answer with answer_action_request.
        Act,
    }

    // Every item is a tab in the agent Space. `url` is set for Open and
    // Navigate, `space` for Open and `items` for List.
    struct ControlPlan {
        // The workspace changed, so the caller saves and notifies.
        changed: bool,
        action: TabActionKind,
        item: u64,
        space: u64,
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
        // Entries already pinned in their Space, left alone.
        kept_pins: usize,
        // Groups whose label is dropped, since Pins have no folders yet.
        flattened_groups: usize,
        skipped_count: usize,
        // "Line 12: file:///…" for entries that are not imported.
        skipped: Vec<String>,
        unrecognized_count: usize,
        // "Line 4, indented: …" for lines the reader did not understand.
        unrecognized: Vec<String>,
        // After an apply, the export's first Space; otherwise 0.
        first_space: u64,
    }

    extern "Rust" {
        type Workspace;
        fn new_workspace() -> Box<Workspace>;
        fn current_name(workspace: &Workspace) -> String;
        fn rename_space(workspace: &mut Workspace, id: u64, name: &str) -> RenameResult;
        fn bridge_limits() -> BridgeLimits;
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
        // confirms first and closes live pages of the deleted Space's items.
        fn delete_space(workspace: &mut Workspace, space_id: u64) -> MutationResult;
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
        // empty. A tab command that passed its checks keeps the request in
        // `reply` and names the action; the caller runs it and answers with
        // answer_tab_request, now or later.
        fn answer_control_request(
            reply: &mut ControlReply,
            workspace: &mut Workspace,
            window_id: u64,
            profile: &str,
        ) -> ControlPlan;
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

        type ArcExport;
        // Largest export accepted, in bytes.
        fn arc_export_max_bytes() -> usize;
        fn arc_import_list_limit() -> usize;
        // Parses untrusted bytes. Never fails; a file it can't read carries an
        // error that previews report.
        fn parse_arc_export(bytes: &[u8]) -> Box<ArcExport>;
        // Every entry's URL as written, for the browser to canonicalize.
        fn arc_export_urls(arc: &ArcExport) -> Vec<String>;
        // One URL per `arc_export_urls` entry, canonicalized; empty rejects it.
        fn set_arc_export_canonical_urls(arc: &mut ArcExport, urls: Vec<String>);
        // What `apply_arc_import` would do now, without changing anything.
        fn preview_arc_import(workspace: &Workspace, arc: &ArcExport) -> ArcImportPreview;
        // Adds the export's Spaces and Pins that are missing, all or nothing.
        fn apply_arc_import(workspace: &mut Workspace, arc: &ArcExport) -> ArcImportPreview;
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
        report,
        error,
        canonical: Vec::new(),
    })
}

pub fn arc_export_urls(export: &ArcExport) -> Vec<String> {
    import::arc_workspace::entry_urls(&export.report)
        .into_iter()
        .map(str::to_owned)
        .collect()
}

pub fn set_arc_export_canonical_urls(export: &mut ArcExport, urls: Vec<String>) {
    export.canonical = urls
        .into_iter()
        .map(|url| (!url.is_empty()).then_some(url))
        .collect();
}

pub fn preview_arc_import(workspace: &Workspace, export: &ArcExport) -> ffi::ArcImportPreview {
    arc_import_result(
        export,
        import::arc_workspace::preview(&export.report, &export.canonical, &workspace.0),
    )
}

pub fn apply_arc_import(workspace: &mut Workspace, export: &ArcExport) -> ffi::ArcImportPreview {
    let result = import::arc_workspace::apply(&export.report, &export.canonical, &mut workspace.0);
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
    let mut preview = arc_import_result(export, result);
    preview.first_space = first.unwrap_or(0);
    preview
}

fn arc_import_result(
    export: &ArcExport,
    result: Result<import::arc_workspace::Preview, import::arc_workspace::ImportError>,
) -> ffi::ArcImportPreview {
    use import::arc_markdown::Reason;
    let unrecognized = &export.report.unrecognized;
    let mut preview = ffi::ArcImportPreview {
        ok: false,
        error: export.error.clone(),
        spaces: 0,
        new_spaces: 0,
        new_pins: 0,
        kept_pins: 0,
        flattened_groups: 0,
        skipped_count: 0,
        skipped: Vec::new(),
        unrecognized_count: unrecognized.len(),
        first_space: 0,
        unrecognized: unrecognized
            .iter()
            .take(ARC_IMPORT_LIST_LIMIT)
            .map(|line| {
                let reason = match line.reason {
                    Reason::Indented => "indented",
                    Reason::DeeperHeading => "nested heading",
                    Reason::EmptyLabel => "heading without a name",
                    Reason::LabelTooLong => "name too long",
                    Reason::ControlCharacter => "control character",
                    Reason::GroupOutsideSpace => "group before any Space",
                    Reason::EntryOutsideGroup => "link before any group",
                    Reason::NotUrl => "not a link",
                    Reason::UrlTooLong => "link too long",
                    Reason::Other => "unknown text",
                };
                format!("Line {}, {reason}: {}", line.line, line.excerpt)
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
            preview.kept_pins = plan.kept_pin_count();
            preview.flattened_groups = plan.flattened_groups;
            preview.skipped_count = plan.skipped.len();
            preview.skipped = plan
                .skipped
                .iter()
                .take(ARC_IMPORT_LIST_LIMIT)
                .map(|entry| format!("Line {}: {}", entry.line, entry.url))
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
    }
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

pub fn delete_space(workspace: &mut Workspace, space_id: u64) -> ffi::MutationResult {
    outcome(
        workspace
            .0
            .delete_space(space_id)
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
        last_viewed_secs: item.last_viewed_secs,
    }
}

pub fn sidebar_snapshot(workspace: &Workspace, window_id: u64) -> ffi::SidebarResult {
    match workspace.0.sidebar_snapshot(window_id) {
        Ok(sidebar) => ffi::SidebarResult {
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
            pending.reply(response);
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
        TabAction::Act(action) => {
            let item = action.item;
            reply.action = Some(action);
            (Kind::Act, item)
        }
    };
    plan
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
