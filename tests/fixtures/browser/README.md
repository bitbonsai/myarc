# Native Chromium regression fixture

[index.html](index.html) is an original local form that writes one invented note to browser localStorage. It implements no myarc workspace rules. The [developer test client](../../../tools/check_browser_foundation.mjs) exercises real browser behavior, separate from production CLI/MCP control.

## Run

Requires Node 22+, Python 3 and the compiled Chromium candidate from [the foundation recipe](../../../browser/README.md). Use a fresh, disposable profile and keep an ordinary test tab open. The client checks the candidate version and expected browser PID before navigation. A marker prevents accidental use of an unmarked profile; these privileged checks remain developer tooling, not an authorization system.

From the repository root, with `WORK` and `SRC` from that recipe:

```sh
PROFILE=$(mktemp -d "$WORK/runtime-profile.XXXXXX")
printf 'Owned synthetic M0 browser test profile\n' > "$PROFILE/.myarc-m0-test-profile"
python3 -m http.server 8766 --bind 127.0.0.1 --directory tests/fixtures/browser &
SERVER_PID=$!
"$SRC/out/MyarcM0/Chromium.app/Contents/MacOS/Chromium" \
  --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
  --myarc-foundation --remote-debugging-address=127.0.0.1 \
  --remote-debugging-port=0 http://127.0.0.1:8766/ &
BROWSER_PID=$!
```

Wait for the native window. Install uBlock Origin Lite `2026.1021.1354`, ID `ddkjiahejlhfcafbddmgiahcphecmpfh`, through Chrome Web Store in that test profile. The tested configuration grants its website access. Neither the extension nor its code is bundled here. Keep personal browsing out of this profile.

```sh
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" extension
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" runtime
```

The extension scenario temporarily disables and re-enables the named extension. It visits the public example.com page and fetches an advertising resource without executing it or sending credentials. This negative control requires internet access. Enabled filtering must return an extension-local replacement or a blocked-client error; disabled filtering must produce a response from the expected advertising origin/path. A resolved fetch alone proves neither network access nor blocking.

Quit only the test Chromium instance normally, then repeat its launch command with the same `PROFILE` and flags. Set `BROWSER_PID=$!` to the new process before running:

```sh
node tools/check_browser_foundation.mjs "$PROFILE" "$BROWSER_PID" restart
```

The extension scenario saves its browser PID in the marked profile. The restart scenario rejects the unchanged PID and verifies the enabled state and filtering effect. It leaves the extension installed. Stop the owned fixture server afterward with `kill "$SERVER_PID"`, and quit the test browser to close its developer debugging endpoint.

## Scope and limits

- Renderer check deliberately crashes the selected renderer, reloads it and checks acknowledged localStorage, with the browser PID unchanged. It does not prove automatic workspace recovery.
- Off-the-record checks verify localStorage separation from the normal context and absence in a newly created private context after disposal. Cookie/history isolation, disk forensics and production workspace private-data handling remain untested.
- Native toolbar activation is a separate macOS accessibility/keyboard check. The Node client covers neither full UI accessibility nor product CLI/MCP grants.
- The candidate aborted once when default-context `Target.createTarget` entered synchronous Profile creation on the browser UI thread. The client reuses an existing ordinary tab and creates additional targets only inside explicit off-the-record contexts. The default-profile failure remains unresolved; upstream-versus-patch origin has not been isolated.
- Extension version-changing updates, removal and native messaging remain untested. These checks run manually with the external browser build; GitHub's standalone Rust workflow does not run them.
