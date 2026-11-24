#!/bin/bash
# Core browsing smoke check (CORE-01): verifies a permission prompt, find in
# page, DevTools, and History (both chrome://history UI and SQLite History DB)
# in a background launch of myArc on a disposable /tmp profile with a local
# synthetic HTTP server.
#
# Usage:
#   tools/check_core_smoke.sh
# Env:
#   APP=<myArc.app>   default: /Applications/myArc.app
#   OUTDIR=<dir>      default: /tmp/myarc-core-smoke-<timestamp>
#   KEEP=1            keeps the disposable profile directory
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:-/Applications/myArc.app}"
BIN="$APP/Contents/MacOS/$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$APP/Contents/Info.plist" 2>/dev/null || echo myArc)"

if [ "${1:-}" = "--inner" ]; then
  [ -x "$BIN" ] || { echo "native binary missing: $BIN" >&2; exit 1; }
  OUTDIR="${OUTDIR:-/tmp/myarc-core-smoke-$(date -u +%Y%m%dT%H%M%SZ)}"
  mkdir -p "$OUTDIR"

  WORK=$(mktemp -d /tmp/myarc-core-smoke.XXXXXX)
  printf 'Owned synthetic CORE-01 smoke test profile\n' > "$WORK/.myarc-m0-test-profile"
  PROFILE="$WORK/profile"
  mkdir -p "$PROFILE/Default"
  printf 'Owned synthetic CORE-01 smoke test profile\n' > "$PROFILE/.myarc-m0-test-profile"
  cat > "$PROFILE/Default/Preferences" <<'JSON'
{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}
JSON
  cat > "$PROFILE/Default/myarc-workspace-v1.json" <<'JSON'
{
  "version": 1,
  "profile_id": "1",
  "next_space_id": "2",
  "next_item_id": "2",
  "next_window_id": "2",
  "spaces": [
    {"id": "1", "profile_id": "1", "name": "Smoke"}
  ],
  "items": [
    {"id": "1", "placement": 0, "owner_id": "1", "has_url": true, "canonical_url": "http://127.0.0.1:9/seed", "title": "Seed"}
  ],
  "windows": [
    {"id": "1", "space_id": "1", "selected_item_id": "0"}
  ]
}
JSON

  SERVER_PID=""
  PID=""
  cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
    pkill -KILL -f -- "--user-data-dir=$PROFILE" 2>/dev/null || true
    if [ -z "${KEEP:-}" ] && [ -f "$WORK/.myarc-m0-test-profile" ]; then
      rm -rf "$WORK"
    else
      echo "kept $WORK"
    fi
  }
  trap cleanup EXIT

  PORT_FILE="$WORK/server.port"
  /usr/bin/python3 - "$PORT_FILE" <<'PY' &
import http.server, sys

port_file = sys.argv[1]
PAGE = """<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><title>Core Smoke Fixture</title></head>
<body>
<main>
  <h1>Core Smoke Fixture</h1>
  <p>First match: needle-smoke-token in paragraph one.</p>
  <p>Second match: needle-smoke-token in paragraph two.</p>
  <button id="perm" autofocus onclick="Notification.requestPermission()">Enable notifications</button>
</main>
</body>
</html>
""".encode("utf-8")

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)

srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
with open(port_file, "w") as f:
    f.write(str(srv.server_address[1]))
srv.serve_forever()
PY
  SERVER_PID=$!
  for _ in $(seq 1 40); do [ -s "$PORT_FILE" ] && break; sleep 0.1; done
  PORT=$(cat "$PORT_FILE")
  URL="http://127.0.0.1:$PORT/smoke.html"
  echo "fixture server: $URL"

  open -g -n -a "$APP" --args \
    --user-data-dir="$PROFILE" \
    --no-first-run --no-default-browser-check --use-mock-keychain \
    --disable-backgrounding-occluded-windows \
    --window-size=1280,800 --window-position=40,60 \
    "$URL"

  for _ in $(seq 1 80); do
    for q in $(pgrep -f -- "--user-data-dir=$PROFILE" || true); do
      ps -o command= -p "$q" | grep -q -- '--type=' && continue
      [[ "$(ps -o command= -p "$q")" == "$BIN "* ]] && { PID=$q; break; }
    done
    [ -n "$PID" ] && break
    sleep 0.25
  done
  [ -n "$PID" ] || { echo "native process did not start" >&2; exit 1; }
  echo "browser PID=$PID"

  key() { swift "$ROOT/tools/bgcapture.swift" key "$PID" "$1"; }
  shot() { swift "$ROOT/tools/bgcapture.swift" shot "$PID" "$1"; }

  # 1. Permission prompt (user gesture on autofocus button -> Notification.requestPermission)
  sleep 3
  key space
  sleep 1.2
  shot "$OUTDIR/1-permission-prompt.png"
  echo "captured 1-permission-prompt.png"
  key escape
  sleep 0.5

  # 2. Find in page (Cmd+F -> "needle")
  key cmd+f
  sleep 0.5
  for ch in n e e d l e; do key "$ch"; done
  sleep 0.6
  shot "$OUTDIR/2-find-in-page.png"
  echo "captured 2-find-in-page.png"
  key escape
  sleep 0.5

  # 3. DevTools (Cmd+Opt+I)
  key cmd+alt+i
  sleep 2
  shot "$OUTDIR/3-devtools.png"
  echo "captured 3-devtools.png"
  key cmd+alt+i
  sleep 0.8

  # 4. History (Cmd+Y)
  key cmd+y
  sleep 2
  shot "$OUTDIR/4-history.png"
  echo "captured 4-history.png"

  # Clean quit and verify History SQLite DB recorded the synthetic URL.
  kill -TERM "$PID" 2>/dev/null || true
  for _ in $(seq 1 80); do kill -0 "$PID" 2>/dev/null || break; sleep 0.25; done
  kill -0 "$PID" 2>/dev/null && kill -KILL "$PID" 2>/dev/null || true

  /usr/bin/python3 - "$PROFILE/Default/History" "$URL" <<'PY'
import sqlite3, sys
db, url = sys.argv[1:]
con = sqlite3.connect(db)
rows = con.execute("SELECT url, title, visit_count FROM urls WHERE url = ?", (url,)).fetchall()
assert rows, f"URL {url} not found in History DB {db}"
print(f"History DB verified: {rows[0]}")
PY

  echo "CORE SMOKE PASSED! Evidence in: $OUTDIR"
  ls -la "$OUTDIR"
  exit 0
fi

exec /usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner "$@"
