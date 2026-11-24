#!/bin/bash
# Installs Bitwarden, as the Chrome Web Store serves it, into an installed
# myArc build and checks that it works and survives a restart, in background
# launches of a disposable user-data dir:
#  1. an external-extension file in that dir's own "External Extensions"
#     folder names the store's update server, so the browser downloads and
#     verifies the store's CRX itself. The store page's "Add to Chrome" isn't
#     used: its confirm dialog activates the app, which would take focus from
#     the user. An external install arrives off, as on any Mac, so the check
#     turns it on from chrome://extensions, as a user would;
#  2. finds its content script's world on a local page with a login form;
#  3. waits for the extension's service worker and asks it, through
#     chrome.management, whether it is enabled and how it was installed;
#  4. quits with Browser.close, checks the saved extension state, relaunches
#     the same dir and repeats 2 and 3.
# A watcher kills the test browser if it ever becomes the frontmost app.
# No account, sign-in or credentials are used. Needs the network.
# Usage: tools/check_extension_store.sh [outdir]   env: APP=<myArc.app>
# Writes result.txt, the browser log and the fixture server log to outdir.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:-/Applications/myArc.app}"
EXT=nngceckbapebfimnlniiiahkandclblb
UPDATE_URL=https://clients2.google.com/service/update2/crx

if [ "${1:-}" != --inner ]; then
  OUT="${1:-$ROOT/research-private/captures/extension-store-$(date -u +%Y%m%dT%H%M%SZ)}"
  mkdir -p "$OUT"
  OUT="$(cd "$OUT" && pwd)"
  # One lock for both launches, as tools/capture_native.sh holds it.
  /usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner "$OUT"
  echo "$OUT"
  exit 0
fi

OUT=$2
BIN="$APP/Contents/MacOS/$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$APP/Contents/Info.plist")"
[ -x "$BIN" ] || { echo "app binary missing: $BIN" >&2; exit 1; }
VERSION=$(/usr/libexec/PlistBuddy -c "Print :MyarcVersion" "$APP/Contents/Info.plist" 2>/dev/null || echo unknown)
HELPER=(node "$ROOT/tools/check_extension_store.mjs")

PORT=$(/usr/bin/python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
/usr/bin/python3 -m http.server --bind 127.0.0.1 "$PORT" \
  --directory "$ROOT/tests/fixtures/extension-login" 2> "$OUT/server.log" &
SERVER=$!
PAGE="http://127.0.0.1:$PORT/index.html"
PROFILE=$(mktemp -d /tmp/myarc-extension.XXXXXX)
touch "$PROFILE/.myarc-m0-test-profile"
mkdir -p "$PROFILE/Default" "$PROFILE/External Extensions"
printf '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}\n' \
  > "$PROFILE/Default/Preferences"
# The welcome tour is done, so its desktop dim never covers the user's apps.
printf '{"myarc":{"onboarding":{"tour":"{\\"v\\":1,\\"state\\":\\"complete\\",\\"step\\":0,\\"done\\":[],\\"skipped\\":[],\\"import\\":null}"}}}\n' \
  > "$PROFILE/Local State"
printf '{"external_update_url":"%s"}\n' "$UPDATE_URL" > "$PROFILE/External Extensions/$EXT.json"

PID=""
WATCH=""
stop() {
  [ -z "$WATCH" ] || kill "$WATCH" 2>/dev/null || true
  WATCH=""
  if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
    kill -TERM "$PID" 2>/dev/null || true
    for _ in $(seq 1 50); do kill -0 "$PID" 2>/dev/null || break; sleep 0.2; done
    kill -0 "$PID" 2>/dev/null && kill -KILL "$PID" 2>/dev/null || true
  fi
  PID=""
}
cleanup() {
  stop
  kill "$SERVER" 2>/dev/null || true
  [ -f "$PROFILE/.myarc-m0-test-profile" ] && rm -rf "$PROFILE"
  true
}
trap cleanup EXIT

note() { echo "$*" | tee -a "$OUT/result.txt"; }
FAILED=0
check() { local line; line=$("$@" 2>&1) && note "$line" || { note "FAIL $line"; FAILED=1; }; }
front_pid() { lsappinfo info -only pid "$(lsappinfo front)" 2>/dev/null | sed -E 's/[^0-9]//g'; }

launch() {
  rm -f "$PROFILE/DevToolsActivePort"
  # -g: launch without activating; -n: a separate process from any running
  # myArc, with its own user-data dir.
  open -g -n -a "$APP" --args --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
    --use-mock-keychain --lang=en-US --remote-debugging-port=0 --enable-logging --log-file="$OUT/chrome.log" --disable-backgrounding-occluded-windows \
    --enable-features=GlassFrame --disable-features=InitialWebUI --myarc-background-launch \
    --myarc-visual-parity --myarc-workspace-sidebar --window-size=1440,900 --window-position=40,60 "$@"
  for _ in $(seq 1 60); do
    for p in $(pgrep -f -- "--user-data-dir=$PROFILE" || true); do
      ps -o command= -p "$p" | grep -q -- '--type=' || { PID=$p; break; }
    done
    [ -n "$PID" ] && break
    sleep 0.25
  done
  [ -n "$PID" ] || { note "FAIL browser did not start"; exit 1; }
  ( while kill -0 "$PID" 2>/dev/null; do
      if [ "$(front_pid)" = "$PID" ]; then
        kill -KILL "$PID"; echo "FAIL the test browser became the frontmost app and was killed" >> "$OUT/result.txt"; exit
      fi
      sleep 0.2
    done ) &
  WATCH=$!
}

quit() {
  "${HELPER[@]}" quit "$PROFILE" >/dev/null 2>&1 || true
  for _ in $(seq 1 100); do kill -0 "$PID" 2>/dev/null || break; sleep 0.2; done
  if kill -0 "$PID" 2>/dev/null; then note "FAIL browser still running 20 s after Browser.close"; FAILED=1; stop
  else note "quit: browser exited after Browser.close"; PID=""; kill "$WATCH" 2>/dev/null || true; WATCH=""; fi
}

: > "$OUT/result.txt"
note "app: $APP ($VERSION), install: external_update_url $UPDATE_URL"
launch "$PAGE"
check "${HELPER[@]}" enable "$PROFILE" "$EXT"
check "${HELPER[@]}" inject "$PROFILE" "$EXT" "$PAGE"
check "${HELPER[@]}" worker "$PROFILE" "$EXT"
quit

# The saved state the relaunch reads.
check /usr/bin/python3 -I -c '
import json, sys
entry, name = None, None
for name in ("Secure Preferences", "Preferences"):
    try:
        entry = json.load(open(f"{sys.argv[1]}/Default/{name}"))["extensions"]["settings"][sys.argv[2]]
        break
    except (OSError, KeyError):
        pass
if not entry:
    sys.exit("saved state: extension missing from the profile")
reasons = entry.get("disable_reasons", [])
location, store = entry.get("location"), entry.get("from_webstore")
version = entry.get("manifest", {}).get("version", "?")
shown = reasons or "none"
print(f"saved state: {name}, location {location}, from webstore {store}, "
      f"version {version}, disable reasons {shown}")
sys.exit(1 if reasons else 0)
' "$PROFILE" "$EXT"

note "relaunch:"
launch "$PAGE"
check "${HELPER[@]}" inject "$PROFILE" "$EXT" "$PAGE"
check "${HELPER[@]}" worker "$PROFILE" "$EXT"
quit

grep -q 'became the frontmost' "$OUT/result.txt" && FAILED=1
note "fixture page loads: $(grep -c '"GET /index.html' "$OUT/server.log" || true)"
[ "$FAILED" = 0 ] && note "PASS" || { note "FAILED"; exit 1; }
