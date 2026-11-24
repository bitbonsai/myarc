#!/bin/bash
# Account independence & offline request audit (CORE-01 / SCORECARD):
# Launches a fresh disposable Profile with external DNS blocked
# (--host-resolver-rules="MAP * ~NOTFOUND, EXCLUDE 127.0.0.1") and
# --log-net-log enabled, verifies the browser reaches a usable workspace
# offline (loads a local synthetic page, pins it with Cmd+D, persists it to
# myarc-workspace-v1.json), then parses the NetLog to audit every outbound
# URL_REQUEST host.
#
# Usage:
#   tools/check_offline_audit.sh
# Env:
#   APP=<myArc.app>   default: /Applications/myArc.app
#   OUTDIR=<dir>      default: /tmp/myarc-offline-audit-<timestamp>
#   KEEP=1            keeps the disposable profile directory
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:-/Applications/myArc.app}"
BIN="$APP/Contents/MacOS/$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$APP/Contents/Info.plist" 2>/dev/null || echo myArc)"

if [ "${1:-}" = "--inner" ]; then
  [ -x "$BIN" ] || { echo "native binary missing: $BIN" >&2; exit 1; }
  OUTDIR="${OUTDIR:-/tmp/myarc-offline-audit-$(date -u +%Y%m%dT%H%M%SZ)}"
  mkdir -p "$OUTDIR"

  WORK=$(mktemp -d /tmp/myarc-offline.XXXXXX)
  printf 'Owned synthetic offline audit profile\n' > "$WORK/.myarc-m0-test-profile"
  PROFILE="$WORK/profile"
  mkdir -p "$PROFILE/Default"
  printf 'Owned synthetic offline audit profile\n' > "$PROFILE/.myarc-m0-test-profile"
  # Skip first-run onboarding modal so the fresh workspace opens directly.
  cat > "$PROFILE/Default/Preferences" <<'JSON'
{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}
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
PAGE = b"""<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><title>Offline Workspace Check</title></head>
<body><main><h1>Offline Workspace Check</h1><p>No account and no external network.</p></main></body>
</html>
"""
class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *args): pass
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)
srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
with open(port_file, "w") as f: f.write(str(srv.server_address[1]))
srv.serve_forever()
PY
  SERVER_PID=$!
  for _ in $(seq 1 40); do [ -s "$PORT_FILE" ] && break; sleep 0.1; done
  PORT=$(cat "$PORT_FILE")
  URL="http://127.0.0.1:$PORT/offline.html"
  NETLOG="$OUTDIR/netlog.json"
  echo "fixture server: $URL"

  # Fresh Profile, external DNS blocked, NetLog recording all requests.
  open -g -n -a "$APP" --args \
    --user-data-dir="$PROFILE" \
    --no-first-run --no-default-browser-check --use-mock-keychain \
    --disable-backgrounding-occluded-windows \
    --host-resolver-rules="MAP * ~NOTFOUND, EXCLUDE 127.0.0.1" \
    --log-net-log="$NETLOG" \
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

  # Wait for fresh welcome intro animation to finish, then dismiss with Escape.
  sleep 6.5
  shot "$OUTDIR/1-fresh-launch.png"
  key escape
  sleep 2
  # Pin the loaded local page with Cmd+D to prove usable workspace mutations offline.
  key cmd+d
  sleep 1.5
  shot "$OUTDIR/2-offline-pinned.png"

  # Clean quit so NetLog and workspace file flush completely.
  kill -TERM "$PID" 2>/dev/null || true
  for _ in $(seq 1 80); do kill -0 "$PID" 2>/dev/null || break; sleep 0.25; done
  kill -0 "$PID" 2>/dev/null && kill -KILL "$PID" 2>/dev/null || true

  # Verify workspace file persisted the offline page/pin.
  /usr/bin/python3 - "$PROFILE/Default/myarc-workspace-v1.json" "$URL" <<'PY'
import json, sys
ws_path, url = sys.argv[1:]
ws = json.load(open(ws_path))
items = ws.get("items", [])
found = [it for it in items if it.get("canonical_url") == url]
assert found, f"Offline URL {url} not found in workspace items: {items}"
print(f"workspace offline ok: spaces={len(ws.get('spaces', []))} items={len(items)} matched_item={found[0]}")
PY

  # Parse NetLog and classify every URL_REQUEST.
  /usr/bin/python3 - "$NETLOG" "$OUTDIR/audit-summary.txt" <<'PY'
import collections, json, sys, urllib.parse

netlog_path, summary_path = sys.argv[1:]
data = json.load(open(netlog_path))
constants = data.get("constants", {})
event_types = {v: k for k, v in constants.get("logEventTypes", {}).items()}

urls = []
for ev in data.get("events", []):
    et = event_types.get(ev.get("type"))
    if et == "URL_REQUEST_START_JOB":
        u = ev.get("params", {}).get("url")
        if u:
            urls.append(u)

by_host = collections.Counter()
myarc_urls = []
for u in urls:
    p = urllib.parse.urlparse(u)
    if p.scheme in ("http", "https"):
        host = p.hostname or ""
        by_host[host] += 1
        if "myarc" in host.lower():
            myarc_urls.append(u)

lines = [
    f"Total HTTP(S) URL_REQUEST_START_JOB entries: {sum(by_host.values())}",
    "Hosts observed:",
]
for host, count in by_host.most_common():
    lines.append(f"  {host}: {count}")
lines.append("myarc-domain URLs:")
if myarc_urls:
    for u in sorted(set(myarc_urls)):
        lines.append(f"  {u}")
else:
    lines.append("  (none)")

summary = "\n".join(lines) + "\n"
with open(summary_path, "w") as f:
    f.write(summary)
print(summary)

# Fail if any myarc.dev call is outside the expected quick-start/appcast set.
disallowed = [
    u for u in myarc_urls
    if not (u.startswith("https://myarc.dev/welcome") or u.startswith("https://myarc.dev/appcast.xml"))
]
assert not disallowed, f"Unexpected myarc service calls: {disallowed}"
PY

  echo "OFFLINE AUDIT PASSED! Evidence in: $OUTDIR"
  exit 0
fi

exec /usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner "$@"
