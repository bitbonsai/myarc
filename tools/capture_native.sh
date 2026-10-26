#!/bin/bash
# Capture the native myArc build window (disposable profile, by-PID cleanup).
# Usage: tools/capture_native.sh [out.png]   env: W=1440 H=900 URL=about:blank SETTLE=3
set -euo pipefail

BASE=/Users/mwolff/.t3/projects/myarc/research-private/captures
APP="$HOME/.cache/myarc-m0/chromium/src/out/MyarcM0/Chromium.app"
BIN="$APP/Contents/MacOS/Chromium"
W=${W:-1440}; H=${H:-900}; URL=${URL:-about:blank}; SETTLE=${SETTLE:-3}

if [ "${1:-}" = "--inner" ]; then
  OUT=$2
  [ -x "$BIN" ] || { echo "native binary missing: $BIN" >&2; exit 1; }
  PROFILE=$(mktemp -d "${TMPDIR:-/tmp}/myarc-native.XXXXXX")
  PID=""
  cleanup() {
    if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
      kill -TERM "$PID" 2>/dev/null || true
      for _ in $(seq 1 50); do kill -0 "$PID" 2>/dev/null || break; sleep 0.2; done
      kill -0 "$PID" 2>/dev/null && kill -KILL "$PID" 2>/dev/null || true
    fi
    # Only delete a profile carrying our marker.
    [ -f "$PROFILE/.myarc-m0-test-profile" ] && rm -rf "$PROFILE"
  }
  trap cleanup EXIT
  mkdir -p "$PROFILE/Default"
  touch "$PROFILE/.myarc-m0-test-profile"
  printf '%s\n' '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"uncollapsed_width":230,"enabled_first_time":true}}' > "$PROFILE/Default/Preferences"
  "$BIN" --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
    --enable-features=GlassFrame --disable-features=InitialWebUI \
    --myarc-visual-parity --window-size="$W,$H" --window-position=40,60 "$URL" \
    >/dev/null 2>&1 &
  PID=$!
  WID=""
  for _ in $(seq 1 100); do
    WID=$(swift - "$PID" <<'SWIFT' 2>/dev/null || true
import CoreGraphics
import Foundation
let pid = Int32(CommandLine.arguments[1])!
let list = (CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as? [[String: Any]]) ?? []
var best = (0.0, 0)
for w in list {
  guard (w[kCGWindowOwnerPID as String] as? Int32) == pid,
        (w[kCGWindowLayer as String] as? Int) == 0,
        let b = w[kCGWindowBounds as String] as? [String: Double],
        (b["Width"] ?? 0) > 400, (b["Height"] ?? 0) > 300 else { continue }
  let area = (b["Width"] ?? 0) * (b["Height"] ?? 0)
  if area > best.0 { best = (area, w[kCGWindowNumber as String] as! Int) }
}
if best.1 != 0 { print(best.1) }
SWIFT
)
    [ -n "$WID" ] && break
    kill -0 "$PID" 2>/dev/null || { echo "native process exited early" >&2; exit 1; }
    sleep 0.5
  done
  [ -n "$WID" ] || { echo "no window for PID $PID" >&2; exit 1; }
  sleep "$SETTLE"
  screencapture -x -o -l "$WID" "$OUT"
  echo "pid=$PID window=$WID"
  exit 0
fi

if [ -n "${1:-}" ]; then OUT=$1
else
  DIR="${OUTDIR:-$BASE/$(date -u +%Y%m%dT%H%M%SZ)}"
  mkdir -p "$DIR"; OUT="$DIR/native.png"
fi
mkdir -p "$(dirname "$OUT")"
# Lock held only for launch+capture; waits if a build holds it.
/usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner "$OUT"
echo "$OUT"
