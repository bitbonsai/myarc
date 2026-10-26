#!/bin/bash
# Capture a NEW Arc window only; existing windows/tabs/Spaces are never touched.
# Usage: tools/capture_arc.sh [out.png]   env: W=1440 H=900 SETTLE=3
set -euo pipefail

BASE=/Users/mwolff/.t3/projects/myarc/research-private/captures
W=${W:-1440}; H=${H:-900}; SETTLE=${SETTLE:-3}
X=40; Y=60

if [ -n "${1:-}" ]; then OUT=$1
else
  DIR="${OUTDIR:-$BASE/$(date -u +%Y%m%dT%H%M%SZ)}"
  mkdir -p "$DIR"; OUT="$DIR/arc.png"
fi
mkdir -p "$(dirname "$OUT")"

PID=$(pgrep -x Arc | head -1 || true)
[ -n "$PID" ] || { echo "Arc is not running; start it first (script does not launch it)" >&2; exit 1; }

# Prints CGWindowIDs of Arc's normal-layer windows, one per line.
arc_ids() {
  swift - "$PID" <<'SWIFT' 2>/dev/null
import CoreGraphics
import Foundation
let pid = Int32(CommandLine.arguments[1])!
let list = (CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as? [[String: Any]]) ?? []
for w in list {
  guard (w[kCGWindowOwnerPID as String] as? Int32) == pid,
        (w[kCGWindowLayer as String] as? Int) == 0,
        let b = w[kCGWindowBounds as String] as? [String: Double],
        (b["Width"] ?? 0) > 200, (b["Height"] ?? 0) > 200 else { continue }
  print(w[kCGWindowNumber as String] as! Int)
}
SWIFT
}

BEFORE=$(arc_ids | sort)
UUID=""
cleanup() {
  if [ -n "$UUID" ]; then
    osascript -e "tell application \"Arc\" to close (window id \"$UUID\")" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

BEFORE_UUIDS=$(osascript -e 'tell application "Arc" to get id of every window' | tr ',' '\n' | tr -d ' ' | sort)
osascript -e 'tell application "Arc" to make new window' >/dev/null \
  || { echo "AppleScript failed (see error above)" >&2; exit 1; }
for _ in $(seq 1 20); do
  UUID=$(comm -13 <(echo "$BEFORE_UUIDS") <(osascript -e 'tell application "Arc" to get id of every window' | tr ',' '\n' | tr -d ' ' | sort) | head -1)
  [ -n "$UUID" ] && break
  sleep 0.25
done
[ -n "$UUID" ] || { echo "new Arc window id not found" >&2; exit 1; }

WID=""
for _ in $(seq 1 40); do
  sleep 0.5
  WID=$(comm -13 <(echo "$BEFORE") <(arc_ids | sort) | head -1)
  [ -n "$WID" ] && break
done
[ -n "$WID" ] || { echo "new Arc window not found in CGWindowList" >&2; exit 1; }

# Arc has no bounds property: resize via Accessibility. The new window is AX window 1
# (frontmost); only touch it if its geometry matches the new CGWindowID.
CGB=$(swift - "$PID" "$WID" <<'SWIFT' 2>/dev/null
import CoreGraphics
import Foundation
let wid = Int(CommandLine.arguments[2])!
for w in (CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as? [[String: Any]]) ?? []
  where (w[kCGWindowNumber as String] as? Int) == wid {
  let b = w[kCGWindowBounds as String] as! [String: Double]
  print(Int(b["X"]!), Int(b["Y"]!), Int(b["Width"]!), Int(b["Height"]!))
}
SWIFT
)
osascript <<OSA || { echo "Accessibility resize failed (see error above)" >&2; exit 1; }
tell application "System Events" to tell process "Arc"
  set {p, s} to {position, size} of window 1
  set g to (item 1 of p as text) & " " & (item 2 of p as text) & " " & (item 1 of s as text) & " " & (item 2 of s as text)
  if g is not "$CGB" then error "AX window 1 ($CGB expected, got " & g & ") is not the new window; refusing to resize"
  set size of window 1 to {$W, $H}
  set position of window 1 to {$X, $Y}
end tell
OSA
sleep "$SETTLE"
screencapture -x -o -l "$WID" "$OUT"
echo "window=$WID uuid=$UUID"
echo "$OUT"
