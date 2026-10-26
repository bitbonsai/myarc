#!/bin/bash
# Capture the native myArc build window (disposable profile, by-PID cleanup).
# Usage: tools/capture_native.sh [out.png]   env: W=1440 H=900 URL=about:blank SETTLE=3
#   URL may list several space-separated URLs. KEYS=cmd+t (or PALETTE=1) opens the
#   command palette via --myarc-capture-open-palette and waits KEY_SETTLE (default 2).
#   The browser launches in the background (open -g) and is captured with
#   ScreenCaptureKit (tools/bgcapture.swift), so it never takes focus.
set -euo pipefail

BASE="$(cd "$(dirname "$0")/.." && pwd)/research-private/captures"
APP="$HOME/.cache/myarc-m0/chromium/src/out/MyarcM0/Chromium.app"
BIN="$APP/Contents/MacOS/Chromium"
W=${W:-1440}; H=${H:-900}; URL=${URL:-about:blank}; SETTLE=${SETTLE:-5}
KEYS=${KEYS:-}; KEY_SETTLE=${KEY_SETTLE:-2}

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
  printf '%s\n' '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}' > "$PROFILE/Default/Preferences"
  read -r -a URLS <<< "$URL"
  EXTRA=()
  { [ -n "$KEYS" ] || [ -n "${PALETTE:-}" ]; } && EXTRA+=(--myarc-capture-open-palette)
  # -g: launch without activating, so the user's frontmost app keeps focus.
  open -g -n -a "$APP" --args --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
    --enable-features=GlassFrame --disable-features=InitialWebUI \
    --disable-backgrounding-occluded-windows ${EXTRA[@]+"${EXTRA[@]}"} \
    --myarc-visual-parity --myarc-workspace-sidebar --window-size="$W,$H" --window-position=40,60 "${URLS[@]}"
  for _ in $(seq 1 60); do
    for p in $(pgrep -f -- "--user-data-dir=$PROFILE" || true); do
      ps -o command= -p "$p" | grep -q -- '--type=' || { PID=$p; break; }
    done
    [ -n "$PID" ] && break
    sleep 0.25
  done
  [ -n "$PID" ] || { echo "native process did not start" >&2; exit 1; }
  sleep "$SETTLE"
  [ -n "${EXTRA[*]-}" ] && sleep "$KEY_SETTLE"
  kill -0 "$PID" 2>/dev/null || { echo "native process exited early" >&2; exit 1; }
  swift "$(dirname "$0")/bgcapture.swift" shot "$PID" "$OUT"
  WID=bg; RECT=window
  echo "pid=$PID window=$WID rect=$RECT"
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
