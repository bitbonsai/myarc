#!/bin/bash
# Profile isolation check (BC-001): launches the native build in the background
# with a marked, disposable user-data dir, the synthetic fixture server and the
# test extension's path, runs the profile-isolation scenario, relaunches, runs
# profile-isolation-restart, then relaunches with only the ordered-out
# Profile set to continue where it left off and runs profile-isolation-group. Needs a build with the GN arg
# myarc_test_hooks = true, as browser/args.gn sets.
# Each phase quits the browser and then reads the Cookies and History files.
# Usage: tools/check_profile_isolation.sh   env: APP=<Chromium.app> KEEP=1 keeps the dir
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:-$HOME/.cache/myarc-m0/chromium/src/out/MyarcM0/Chromium.app}"

if [ "${1:-}" = "--inner" ]; then
  [ -x "$APP/Contents/MacOS/Chromium" ] || { echo "native binary missing: $APP" >&2; exit 1; }
  # The title requests exist only in builds with the GN arg myarc_test_hooks.
  ARGS="$(dirname "$APP")/args.gn"
  if [ -f "$ARGS" ] && ! grep -Eq '^myarc_test_hooks *= *true' "$ARGS"; then
    echo "build lacks myarc_test_hooks = true in $ARGS; see browser/README.md" >&2; exit 1
  fi
  PROFILE=$(mktemp -d "${TMPDIR:-/tmp}/myarc-isolation.XXXXXX")
  SERVER=""
  PID=""
  cleanup() {
    if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
      kill -TERM "$PID" 2>/dev/null || true
      for _ in $(seq 1 50); do kill -0 "$PID" 2>/dev/null || break; sleep 0.2; done
      kill -0 "$PID" 2>/dev/null && kill -KILL "$PID" 2>/dev/null || true
    fi
    [ -n "$SERVER" ] && kill "$SERVER" 2>/dev/null || true
    if [ -z "${KEEP:-}" ] && [ -f "$PROFILE/.myarc-m0-test-profile" ]; then rm -rf "$PROFILE"
    else echo "kept $PROFILE"; fi
  }
  trap cleanup EXIT
  mkdir -p "$PROFILE/Default"
  printf 'Owned synthetic M0 browser test profile\n' > "$PROFILE/.myarc-m0-test-profile"
  printf '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}\n' > "$PROFILE/Default/Preferences"
  if ! curl -sf -o /dev/null http://127.0.0.1:8766/; then
    python3 -m http.server 8766 --bind 127.0.0.1 --directory "$ROOT/tests/fixtures/browser" >/dev/null 2>&1 &
    SERVER=$!
    for _ in $(seq 1 40); do curl -sf -o /dev/null http://127.0.0.1:8766/ && break; sleep 0.25; done
  fi
  launch() {
    rm -f "$PROFILE/DevToolsActivePort"
    # -g: launch without activating, so the user's frontmost app keeps focus.
    open -g -n -a "$APP" --args --user-data-dir="$PROFILE" --no-first-run --no-default-browser-check \
      --use-mock-keychain --enable-logging --log-file="$PROFILE/myarc-test.log" \
      --remote-debugging-address=127.0.0.1 --remote-debugging-port=0 \
      --enable-features=GlassFrame --disable-features=InitialWebUI \
      --disable-backgrounding-occluded-windows --myarc-background-launch --myarc-capture-profile-swap=cdp \
      --myarc-test-extension="$ROOT/tests/fixtures/browser/isolation-extension" \
      --myarc-visual-parity --myarc-workspace-sidebar --window-size=1280,800 --window-position=40,60 "$@"
    PID=""
    for _ in $(seq 1 80); do
      for p in $(pgrep -f -- "--user-data-dir=$PROFILE" || true); do
        ps -o command= -p "$p" | grep -q -- '--type=' || { PID=$p; break; }
      done
      [ -n "$PID" ] && [ -s "$PROFILE/DevToolsActivePort" ] && break
      sleep 0.25
    done
    [ -n "$PID" ] && [ -s "$PROFILE/DevToolsActivePort" ] || { echo "native process did not start" >&2; exit 1; }
    sleep 4
  }
  launch http://127.0.0.1:8766/
  node "$ROOT/tools/check_browser_foundation.mjs" "$PROFILE" "$PID" profile-isolation
  launch --restore-last-session
  node "$ROOT/tools/check_browser_foundation.mjs" "$PROFILE" "$PID" profile-isolation-restart
  # Without --restore-last-session only the Profile ordered out at quit
  # restores; the shown one is set to open a new window. The restored window
  # must come on screen by itself.
  launch
  node "$ROOT/tools/check_browser_foundation.mjs" "$PROFILE" "$PID" profile-isolation-group
  exit 0
fi

# Lock held for both launches; waits if a build holds it.
/usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner
