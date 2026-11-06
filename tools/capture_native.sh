#!/bin/bash
# Capture the native myArc build window (disposable profile, by-PID cleanup).
# Usage: tools/capture_native.sh [out.png]   env: W=1440 H=900 URL=about:blank SETTLE=3
#   APP=<path to Chromium.app> captures another build, e.g. a checkout copy.
#   URL may list several space-separated URLs; URL=none passes none, so the
#   window opens the way a new window does. KEYS=cmd+t (or PALETTE=1) opens the
#   command palette via --myarc-capture-open-palette and waits KEY_SETTLE (default 2).
#   PALETTE_TEXT="claude plans" pre-fills the palette input to show its results.
#   COPY_URL=1 copies the page URL via --myarc-capture-copy-url and holds the toast.
#   LIBRARY=1 (or media/downloads/boosts/archive) opens the Library via
#   --myarc-capture-open-library, on Spaces unless a section is named.
#   TOOLTIP=library (or space/create) shows that sidebar footer tooltip via
#   --myarc-capture-footer-tooltip and keeps it up.
#   MENU=1 opens the sidebar's "+" creation menu via
#   --myarc-capture-create-menu and keeps it up.
#   CREATE_SPACE=1 opens the Create a Space panel via
#   --myarc-capture-create-space. CREATE_SPACE=emoji or CREATE_SPACE=icon also
#   opens its icon picker on that tab and keeps it up; CREATE_SPACE=profile
#   opens the Profile menu, CREATE_SPACE=profile-new the Profile name field
#   and CREATE_SPACE=theme the theme popover with pink picked at 60%.
#   STACK=1 shows the downloads stack above the Library button via
#   --myarc-capture-downloads-stack and keeps it up. file:// URLs of files
#   Chromium can't show download and seed the stack.
#   WORKSPACE=<file> seeds the profile's myarc-workspace-v1.json, for example
#   with Archived Tabs.
#   LOCAL_STATE=<file> seeds the user-data dir's Local State, for example
#   with other Profiles and their Spaces index.
#   ACTIVATE=<item ID> opens that seeded row once the workspace loads, via
#   --myarc-capture-activate; pair it with URL=none.
#   SETTINGS=1 (or general/profiles/advanced/more) opens the native Settings window via
#   --myarc-capture-open-settings and captures only that window; "more" presses
#   More Settings… and captures the browser window instead.
#   Every capture downloads into the disposable profile's Downloads folder
#   without asking.
#   The browser launches in the background (open -g) and is captured with
#   ScreenCaptureKit (tools/bgcapture.swift), so it never takes focus.
set -euo pipefail

BASE="$(cd "$(dirname "$0")/.." && pwd)/research-private/captures"
APP="${APP:-$HOME/.cache/myarc-m0/chromium/src/out/MyarcM0/Chromium.app}"
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
  printf '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true},"download":{"default_directory":"%s","prompt_for_download":false}}\n' "$PROFILE/Downloads" > "$PROFILE/Default/Preferences"
  [ -n "${WORKSPACE:-}" ] && cp "$WORKSPACE" "$PROFILE/Default/myarc-workspace-v1.json"
  [ -n "${LOCAL_STATE:-}" ] && cp "$LOCAL_STATE" "$PROFILE/Local State"
  URLS=()
  [ "$URL" = none ] || read -r -a URLS <<< "$URL"
  EXTRA=()
  { [ -n "$KEYS" ] || [ -n "${PALETTE:-}" ]; } && EXTRA+=(--myarc-capture-open-palette)
  [ -n "${PALETTE_TEXT:-}" ] && EXTRA+=(--myarc-capture-palette-text="$PALETTE_TEXT")
  [ -n "${COPY_URL:-}" ] && EXTRA+=(--myarc-capture-copy-url)
  case "${LIBRARY:-}" in
    "") ;;
    1) EXTRA+=(--myarc-capture-open-library) ;;
    *) EXTRA+=(--myarc-capture-open-library="$LIBRARY") ;;
  esac
  [ -n "${TOOLTIP:-}" ] && EXTRA+=(--myarc-capture-footer-tooltip="$TOOLTIP")
  [ -n "${STACK:-}" ] && EXTRA+=(--myarc-capture-downloads-stack)
  [ -n "${MENU:-}" ] && EXTRA+=(--myarc-capture-create-menu)
  case "${CREATE_SPACE:-}" in
    '') ;;
    emoji | icon | profile | profile-new | theme) EXTRA+=(--myarc-capture-create-space="$CREATE_SPACE") ;;
    *) EXTRA+=(--myarc-capture-create-space) ;;
  esac
  [ -n "${ACTIVATE:-}" ] && EXTRA+=(--myarc-capture-activate="$ACTIVATE")
  SHOT_TITLE=()
  case "${SETTINGS:-}" in
    "") ;;
    more) EXTRA+=(--myarc-capture-open-settings=more); KEY_SETTLE=$((KEY_SETTLE + 2)) ;;
    profiles) EXTRA+=(--myarc-capture-open-settings=profiles); SHOT_TITLE=(Profiles) ;;
    advanced) EXTRA+=(--myarc-capture-open-settings=advanced); SHOT_TITLE=(Advanced) ;;
    *) EXTRA+=(--myarc-capture-open-settings); SHOT_TITLE=(General) ;;
  esac
  # -g: launch without activating, so the user's frontmost app keeps focus.
  open -g -n -a "$APP" --args --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
    --enable-features=GlassFrame --disable-features=InitialWebUI \
    --disable-backgrounding-occluded-windows --myarc-background-launch ${EXTRA[@]+"${EXTRA[@]}"} \
    --myarc-visual-parity --myarc-workspace-sidebar --window-size="$W,$H" --window-position=40,60 ${URLS[@]+"${URLS[@]}"}
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
  swift "$(dirname "$0")/bgcapture.swift" shot "$PID" "$OUT" ${SHOT_TITLE[@]+"${SHOT_TITLE[@]}"}
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
