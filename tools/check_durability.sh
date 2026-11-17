#!/bin/bash
# Workspace durability check (REL-01): a forced quit right after a change
# leaves that change in place. Each case starts the native build in the
# background with a fresh /tmp profile seeded from
# tests/fixtures/durability/workspace.json, makes one change and kills the
# browser with SIGKILL. It then checks the workspace file, relaunches the
# profile, captures the window, quits it and checks the file again.
#
# Hook cases make a pin, rename, move or Space delete through
# --myarc-test-durability, which kills the browser itself DELAYS milliseconds
# after the change returns (0 kills it in the same task). Control cases rename
# a Space with `myarc run space.rename` and kill the browser from here DELAYS
# milliseconds after the command answers. The damaged-file case tears the file
# and checks that the backup loads. Needs a build with the GN arg
# myarc_test_hooks = true, as browser/args.gn sets.
# Usage: tools/check_durability.sh   env: APP=<Chromium.app> DELAYS="0 1 5 25 100"
#   REPEAT=1 runs the whole set that many times. CLI=<myarc binary> (default
#   target/debug/myarc, built if missing). KEEP=1 keeps the profiles.
#   Captures go to OUTDIR (default research-private/captures/durability-<time>);
#   a window that never comes on screen is listed at the end, not failed.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:-$HOME/.cache/myarc-m0/chromium/src/out/MyarcM0/Chromium.app}"
BIN="$APP/Contents/MacOS/$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$APP/Contents/Info.plist" 2>/dev/null || echo Chromium)"
FIXTURE="$ROOT/tests/fixtures/durability/workspace.json"
FILE=myarc-workspace-v1.json

if [ "${1:-}" = "--inner" ]; then
  [ -x "$BIN" ] || { echo "native binary missing: $BIN" >&2; exit 1; }
  DELAYS=${DELAYS:-"0 1 5 25 100"}
  REPEAT=${REPEAT:-1}
  OUTDIR=${OUTDIR:-$ROOT/research-private/captures/durability-$(date -u +%Y%m%dT%H%M%SZ)}
  CLI=${CLI:-$ROOT/target/debug/myarc}
  mkdir -p "$OUTDIR"
  if [ ! -x "$CLI" ]; then
    (cd "$ROOT" && cargo build -q -p myarc-cli --locked)
  fi
  PID=""
  # Every profile lives under WORK, which carries the test marker. /tmp, not
  # TMPDIR, keeps the control socket's path short enough.
  WORK=$(mktemp -d /tmp/myarc-durability.XXXXXX)
  printf 'Owned synthetic M0 browser test profiles\n' > "$WORK/.myarc-m0-test-profile"
  cleanup() {
    pkill -KILL -f -- "--user-data-dir=$WORK/" 2>/dev/null || true
    if [ -z "${KEEP:-}" ] && [ -f "$WORK/.myarc-m0-test-profile" ]; then rm -rf "$WORK"
    else echo "kept $WORK"; fi
  }
  trap cleanup EXIT

  # A fresh marked profile seeded with the fixture; prints its path.
  fresh() {
    local p
    p=$(mktemp -d "$WORK/profile.XXXXXX")
    mkdir -p "$p/Default" "$p/control"
    chmod 700 "$p/control"
    printf 'Owned synthetic M0 browser test profile\n' > "$p/.myarc-m0-test-profile"
    printf '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}\n' > "$p/Default/Preferences"
    cp "$FIXTURE" "$p/Default/$FILE"
    echo "$p"
  }

  # launch <profile> [switches...]: starts the browser in the background and
  # sets PID to its browser process.
  launch() {
    local p=$1; shift
    # -g: launch without activating, so the user's frontmost app keeps focus.
    open -g -n --env "MYARC_CONTROL_DIR=$p/control" -a "$APP" --args --user-data-dir="$p" \
      --no-first-run --no-default-browser-check --use-mock-keychain \
      --enable-logging --log-file="$p/chrome.log" --vmodule=workspace_service=1 \
      --enable-features=GlassFrame --disable-features=InitialWebUI \
      --disable-backgrounding-occluded-windows --myarc-background-launch \
      --myarc-visual-parity --myarc-workspace-sidebar --window-size=1280,800 --window-position=40,60 "$@"
    PID=""
    for _ in $(seq 1 80); do
      # The browser itself: not a helper, and not `open`, whose arguments
      # name the profile too while it runs.
      for q in $(pgrep -f -- "--user-data-dir=$p" || true); do
        ps -o command= -p "$q" | grep -- '--type=' > /dev/null && continue
        [[ "$(ps -o command= -p "$q")" == "$BIN "* ]] && { PID=$q; break; }
      done
      [ -n "$PID" ] && break
      sleep 0.25
    done
    [ -n "$PID" ] || { echo "native process did not start" >&2; exit 1; }
  }

  # Waits up to <seconds> for PID to exit; fails when it is still running.
  wait_exit() {
    for _ in $(seq 1 $(($1 * 10))); do kill -0 "$PID" 2>/dev/null || return 0; sleep 0.1; done
    return 1
  }

  # Fails when a browser process of <profile> still runs after the kill.
  gone() {
    local q
    for q in $(pgrep -f -- "--user-data-dir=$1" || true); do
      if [[ "$(ps -o command= -p "$q")" == "$BIN "* ]] && ! ps -o command= -p "$q" | grep -- '--type=' > /dev/null; then
        echo "browser $q of $1 survived the kill" >&2; return 1
      fi
    done
  }

  # verify <profile> <change> <stage>: checks that the file holds the change.
  verify() {
    /usr/bin/python3 - "$1/Default/$FILE" "$2" "$3" <<'PY'
import json, sys
path, change, stage = sys.argv[1:]
try:
    with open(path) as f:
        state = json.load(f)
except Exception as error:
    sys.exit(f"{stage}: {path} unreadable: {error}")
items = {item["id"]: item for item in state["items"]}
spaces = {space["id"]: space for space in state["spaces"]}
kind, id_, *rest = change.split(":")
pins = [item["id"] for item in state["items"] if item["placement"] == 1 and item["owner_id"] == "1"]
if kind == "pin":
    ok = items.get(id_, {}).get("placement") == 1
elif kind == "rename":
    ok = items.get(id_, {}).get("name") == rest[0]
elif kind == "move":
    ok = id_ in pins and rest[0] in pins and pins.index(id_) < pins.index(rest[0])
elif kind == "delete-space":
    ok = id_ not in spaces and not any(item["owner_id"] == id_ for item in state["items"])
elif kind == "space-name":
    ok = spaces.get(id_, {}).get("name") == rest[0]
else:
    sys.exit(f"unknown change {change}")
if not ok:
    sys.exit(f"{stage}: {change} missing from {path}: spaces {list(spaces.values())}, Space 1 Pins {pins}, items {list(items.values())}")
PY
  }

  # Captures PID's window, waiting up to 15 s for it to come on screen. A
  # window that never does, for example one on another desktop, is listed in
  # $WORK/missed and reported at the end; the file checks still decide.
  capture() {
    for _ in $(seq 1 15); do
      swift "$ROOT/tools/bgcapture.swift" shot "$PID" "$1" > /dev/null 2>&1 && [ -s "$1" ] && return 0
      sleep 1
    done
    echo "$1" >> "$WORK/missed"
  }

  # Relaunches the killed profile, checks that it loaded cleanly, captures
  # the window, quits and checks the file. The relaunch's new tab makes it
  # save, so the file then holds what it loaded.
  relaunch_and_check() {
    local p=$1 change=$2 shot=$3
    mv "$p/chrome.log" "$p/chrome-killed.log"
    launch "$p" about:blank
    sleep 5
    if grep -E "Workspace (metadata is corrupt|metadata write failed|metadata read failed|file .* is damaged)" "$p/chrome.log"; then
      echo "relaunch reported a workspace storage error" >&2; return 1
    fi
    grep -q "Workspace saved in" "$p/chrome.log" || { echo "relaunch saved nothing" >&2; return 1; }
    capture "$shot"
    kill -TERM "$PID"
    wait_exit 20 || { echo "relaunched browser did not quit" >&2; return 1; }
    PID=""
    verify "$p" "$change" "after relaunch"
  }

  RUNS=0
  FAILS=0

  hook_case() {
    local change=$1 delay=$2 name=$3 p
    p=$(fresh)
    launch "$p" "--myarc-test-durability=$change" "--myarc-test-kill-after-ms=$delay" about:blank
    if ! wait_exit 40; then
      echo "browser still running: hook missing or change failed" >&2
      grep -E "Test durability|ERROR" "$p/chrome.log" | tail -5 >&2 || true
      return 1
    fi
    PID=""
    gone "$p"
    grep -q "Test durability: $change returned" "$p/chrome.log" || { echo "no acknowledgement in the log" >&2; return 1; }
    grep -q "Test durability: SIGKILL" "$p/chrome.log" || { echo "browser exited without the hook's SIGKILL" >&2; return 1; }
    verify "$p" "$change" "after kill"
    relaunch_and_check "$p" "$change" "$OUTDIR/$name.png"
  }

  control_case() {
    local delay=$1 name=$2 label=$3 p approve
    p=$(fresh)
    launch "$p" --myarc-enable-control about:blank
    for _ in $(seq 1 80); do [ -f "$p/control/control.json" ] && break; sleep 0.25; done
    # The first command waits for the approval card; the test hook presses Allow.
    MYARC_CONTROL_DIR="$p/control" "$CLI" run space.list > /dev/null &
    approve=$!
    for _ in $(seq 1 60); do
      kill -0 "$approve" 2>/dev/null || break
      echo allow > "$p/control/test-hook"
      sleep 0.5
    done
    wait "$approve" || { echo "control approval failed" >&2; return 1; }
    MYARC_CONTROL_DIR="$p/control" "$CLI" run space.rename "{\"space_id\":1,\"name\":\"$label\"}" > /dev/null
    [ "$delay" = 0 ] || sleep "$(printf '0.%03d' "$delay")"
    kill -KILL "$PID"
    wait_exit 10 || { echo "browser survived SIGKILL" >&2; return 1; }
    PID=""
    gone "$p"
    verify "$p" "space-name:1:$label" "after kill"
    relaunch_and_check "$p" "space-name:1:$label" "$OUTDIR/$name.png"
  }

  damaged_case() {
    local p size
    p=$(fresh)
    # The backup holds the good generation; the file is torn halfway.
    /usr/bin/python3 - "$FIXTURE" "$p/Default/$FILE.bak" <<'PY'
import json, sys
state = json.load(open(sys.argv[1]))
state["spaces"][0]["name"] = "From backup"
json.dump(state, open(sys.argv[2], "w"))
PY
    size=$(wc -c < "$FIXTURE")
    head -c $((size / 2)) "$FIXTURE" > "$p/Default/$FILE"
    launch "$p" about:blank
    sleep 5
    grep -q "is damaged; loaded the last good copy" "$p/chrome.log" || { echo "no fallback in the log" >&2; return 1; }
    capture "$OUTDIR/damaged-file.png"
    kill -TERM "$PID"
    wait_exit 20 || { echo "browser did not quit" >&2; return 1; }
    PID=""
    verify "$p" "space-name:1:From backup" "after fallback"
    cmp -s <(head -c $((size / 2)) "$FIXTURE") "$p/Default/$FILE.corrupt" || { echo "damaged file not kept aside" >&2; return 1; }
  }

  # Each case runs in a subshell that kills its browser if it stops early.
  run_case() {
    local label=$1 status; shift
    set +e
    ( set -e; trap '[ -z "$PID" ] || kill -KILL "$PID" 2>/dev/null; true' EXIT; "$@" )
    status=$?
    set -e
    RUNS=$((RUNS + 1))
    if [ "$status" = 0 ]; then echo "pass  $label"
    else FAILS=$((FAILS + 1)); echo "FAIL  $label"; fi
  }

  for round in $(seq 1 "$REPEAT"); do
    for delay in $DELAYS; do
      for change in "pin:5" "rename:1:Renamed $round-$delay" "move:3:1" "delete-space:2"; do
        kind=${change%%:*}
        run_case "hook $change, kill ${delay} ms after" hook_case "$change" "$delay" "r$round-$kind-${delay}ms"
      done
      run_case "control space.rename, kill ${delay} ms after the answer" control_case "$delay" "r$round-control-${delay}ms" "Control $round-$delay"
    done
    run_case "damaged file falls back to the backup" damaged_case
  done
  echo "$RUNS runs, $FAILS failed; captures in $OUTDIR"
  if [ -s "$WORK/missed" ]; then
    echo "$(wc -l < "$WORK/missed" | tr -d ' ') windows never came on screen to capture:"
    cat "$WORK/missed"
  fi
  [ "$FAILS" = 0 ]
  exit
fi

# Lock held for every launch; waits if a build holds it.
/usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner
