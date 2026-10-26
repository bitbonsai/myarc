#!/bin/bash
# Stage the blog comparison scene in the native myArc build and capture it.
# Scene (matches research-private/arc/blog-reference/arc-palette-favorites.png):
#   three Profile Favorites (bitbonsai.com, github.com, cloudflare.com), the first
#   active, command palette open. The reference window is 1862x1170 px at 1.4 px/pt
#   (traffic lights 28 px apart, 20 pt natively), so the window is 1330x836 pt with
#   a 200 pt sidebar. Dark mode follows the system.
# Favorites are seeded in the workspace's own persisted format (myarc-workspace-v1.json,
# see EncodeWorkspaceMetadata in browser/foundation/workspace_service.cc) in a marked
# disposable profile; AXPress on each tile opens it. The browser launches in the
# background (open -g), the palette opens via --myarc-capture-open-palette and
# ScreenCaptureKit captures the window, so nothing takes focus and no keystroke,
# click or menu is sent. By-PID cleanup.
# Usage: tools/capture_scene.sh [outdir]   env: W=1330 H=836 PAGE_SETTLE=10
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REF="$ROOT/research-private/arc/blog-reference/arc-palette-favorites.png"
APP="$HOME/.cache/myarc-m0/chromium/src/out/MyarcM0/Chromium.app"
BIN="$APP/Contents/MacOS/Chromium"
W=${W:-1330}; H=${H:-836}; PAGE_SETTLE=${PAGE_SETTLE:-10}

if [ "${1:-}" = "--inner" ]; then
  OUT=$2
  [ -x "$BIN" ] || { echo "native binary missing: $BIN" >&2; exit 1; }
  PROFILE=$(mktemp -d "${TMPDIR:-/tmp}/myarc-scene.XXXXXX")
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
  printf '%s\n' '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"uncollapsed_width":200,"enabled_first_time":true}}' > "$PROFILE/Default/Preferences"
  # Placement 2 = Favorite, owned by Profile 1; Window 1 selects item 1. IDs are strings.
  cat > "$PROFILE/Default/myarc-workspace-v1.json" <<'JSON'
{"version":1,"profile_id":"1","next_space_id":"2","next_item_id":"4","next_window_id":"2",
"spaces":[{"id":"1","profile_id":"1","name":"Everyday"}],
"items":[
{"id":"1","placement":2,"owner_id":"1","has_url":true,"canonical_url":"https://bitbonsai.com/"},
{"id":"2","placement":2,"owner_id":"1","has_url":true,"canonical_url":"https://github.com/"},
{"id":"3","placement":2,"owner_id":"1","has_url":true,"canonical_url":"https://www.cloudflare.com/"}],
"windows":[{"id":"1","space_id":"1","selected_item_id":"1"}]}
JSON
  # -g: launch without activating, so the user's frontmost app keeps focus.
  open -g -n -a "$APP" --args --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
    --enable-features=GlassFrame --disable-features=InitialWebUI \
    --disable-backgrounding-occluded-windows --myarc-capture-open-palette \
    --myarc-visual-parity --myarc-workspace-sidebar --window-size="$W,$H" --window-position=40,60 ${START_URL:-}
  for _ in $(seq 1 60); do
    for p in $(pgrep -f -- "--user-data-dir=$PROFILE" || true); do
      ps -o command= -p "$p" | grep -q -- '--type=' || { PID=$p; break; }
    done
    [ -n "$PID" ] && break
    sleep 0.25
  done
  [ -n "$PID" ] || { echo "native process did not start" >&2; exit 1; }
  # Saved Favorites go live only through ActivateItem. AXPress on each tile runs that
  # path without focus, keystrokes or menus; the first site is pressed last so it is
  # active. Tile titles before load are the saved host.
  cat > "$PROFILE/press.swift" <<'SWIFT'
import ApplicationServices
import Foundation
let app = AXUIElementCreateApplication(Int32(CommandLine.arguments[1])!)
func attr(_ e: AXUIElement, _ a: String) -> AnyObject? { var v: AnyObject?; AXUIElementCopyAttributeValue(e, a as CFString, &v); return v }
func find(_ e: AXUIElement, _ d: Int, _ key: String) -> AXUIElement? {
  if d > 40 { return nil }
  let role = attr(e, kAXRoleAttribute) as? String ?? ""
  let title = (attr(e, kAXTitleAttribute) as? String ?? "").lowercased()
  // "=prefix" matches a title prefix exactly (close buttons); otherwise a host substring.
  let hit = key.hasPrefix("=") ? title.hasPrefix(key.dropFirst()) : title.contains(key) && !title.hasPrefix("close")
  if role == "AXButton" && hit { return e }
  if role == "AXWebArea" || role == "AXMenuBar" { return nil }
  for c in (attr(e, kAXChildrenAttribute) as? [AXUIElement] ?? []) { if let f = find(c, d + 1, key) { return f } }
  return nil
}
for key in CommandLine.arguments.dropFirst(2) {
  var done = false
  for _ in 0..<40 {
    if let b = find(app, 0, key), AXUIElementPerformAction(b, kAXPressAction as CFString) == .success { done = true; break }
    usleep(250_000)
  }
  if !done { print("no tile for \(key)"); exit(1) }
  sleep(2)
}
SWIFT
  sleep 3
  # The startup page is a Temporary "New tab" row the reference does not have.
  swift "$PROFILE/press.swift" "$PID" github cloudflare bitbonsai "=close live page: new tab"
  sleep "$PAGE_SETTLE"
  kill -0 "$PID" 2>/dev/null || { echo "native process exited early" >&2; exit 1; }
  swift "$ROOT/tools/bgcapture.swift" shot "$PID" "$OUT"
  echo "pid=$PID"
  exit 0
fi

DIR="${1:-$ROOT/research-private/blog/$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$DIR"
# Lock held only for launch+capture; waits if a build holds it.
/usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner "$DIR/myarc-scene.png"
echo "$DIR/myarc-scene.png"
if [ -f "$REF" ]; then
  # Reference without its shadow, so both sides show only the window.
  magick "$REF" -crop 1862x1170+69+46 +repage "$DIR/reference.png"
  "$ROOT/tools/compare_captures.sh" "$DIR/reference.png" "$DIR/myarc-scene.png" "$DIR/compare"
fi
