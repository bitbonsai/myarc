# Native late window restoration

## Bounded journey

On stack through0022, a fresh marked disposable Profile used two native windows on different Spaces. Each selected its real pinned copy of one logical Favorite. The original window also held a hidden Temporary page in Reading. The existing strict two-window checkpoint validated native URLs/pins before closing the original window.

Native Cmd+Shift+T then reopened that actual closed window while the survivor and browser process remained running. Both logical window IDs, Spaces/selections and saved ID/owner/URL stayed exact. The closed Temporary row was already removed; native history restored its real page under fresh monotonic item `4`, retaining Reading owner `2`. A second close/reopen produced fresh item `5`, retaining the same saved/window identity. Historical IDs were not guessed back into the model.

The second restoration was invoked from an owned focused palette in the surviving window. Native deactivation closed that palette. The survivor kept its actual target ID, URL, time origin and window position across restoration. Both completed cycles passed five samples of the unchanged two-window projection/pin validator.

## Evidence boundaries

[Structured evidence](../evidence/workspace-native-late-restore.json) records this same-process Undo Closed Window scope. This is not a process-crash run, concurrent two-restorer overlap or durability acceptance. An initial checkpoint failed on a genuinely loading page; readiness was awaited without weakening its check. Only developer checkpoint files were archived/recreated. Product metadata was never edited and no default-context target was created.

Existing `BrowserLiveTabContext::Create` starts the native restore hold before window creation, then forwards historical window hints. The existing consumer lifetime/restore barriers sufficed here; no new source patch was needed. [Earlier late-binding repair](native-late-workspace-bindings.md) and [palette repair](native-palette-intent.md) have separate implementation and source-only fixture evidence.

Private run `/tmp/myarc-m0-runtime/late-window-20261026T023208Z/` retains both real Cmd+Shift+T actions, metadata stages, pin/URL observations and the survivor roundtrip. Owned browsers/helpers/server closed. General abort/shutdown/overlap, native C++ suites and product CLI/MCP remain open. Renderer stays off by default.
