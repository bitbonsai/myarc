#!/bin/bash
# Welcome tour interruption check (step 10 of .plans/2026-11-19-welcome-tour.md):
# a kill at any tour state relaunches where the Rust relaunch rules say, with
# no duplicate import and no lost choice. Each case starts the native build in
# the background on a fresh synthetic /tmp profile, drives the tour to one
# state through the test hooks, waits for the log line that state writes and
# kills the browser with SIGKILL. It then checks the tour record in Local
# State and the workspace file, relaunches the profile with logging only,
# checks the decision the load made, quits and checks both files again.
#
# Cases and what the relaunch must show (crates/workspace/src/onboarding.rs):
#   welcome    killed at the finished welcome sheet  -> welcome, no intro
#   intro      killed about 500 ms into the intro    -> welcome, no intro
#   import     killed once the import's receipt is   -> practice 1, receipt
#              saved, before the reveal                 kept, nothing imported twice
#   reveal     killed as Start browsing starts it    -> practice 1
#   practice1  killed once the reveal ends           -> practice 1
#   practice2  killed once practice 1 is done        -> practice 2, done [1]; or
#                                                       practice 3 with 2 skipped
#                                                       when no page is left to pin
#   practice3  killed once practice 2's pin is done  -> practice 3, done [2], pin kept
#   handoff    killed once practice 3's switch is    -> handoff, done [3]
#              done
#   skip       Skip tour on the welcome, which closes -> complete
#              the sheet; the browser must outlive it
#   retire     Skip tour on practice 1's card, which  -> complete
#              fades it out; the browser must outlive it
#   foreground launched the way a user launches it, not in the background,
#              so the app may activate: once the intro is done the welcome
#              window must be fully opaque over the dim, and once Skip tour
#              closes it the dim must be gone. No relaunch.
# Needs a build with the GN arg myarc_test_hooks = true, as browser/args.gn sets.
# Usage: tools/check_onboarding.sh [build]   build is an out dir or a .app
#   (default ~/.cache/myarc-m0/chromium/src/out/MyarcM0).
#   env: CASES="welcome import" runs only those. KEEP=1 keeps the profiles.
#   OUTDIR (default research-private/captures/onboarding-<time>) gets each
#   case's logs, files and a shot of the relaunched window.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$HOME/.cache/myarc-m0/chromium/src/out/MyarcM0}"
case "$BUILD" in *.app | *.app/) APP=${BUILD%/} ;; *) APP="$BUILD/Chromium.app" ;; esac
APP="$(cd "$APP" 2>/dev/null && pwd || echo "$APP")"
BIN="$APP/Contents/MacOS/$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$APP/Contents/Info.plist" 2>/dev/null || echo Chromium)"
EXPORT="$ROOT/tests/fixtures/arc-markdown/workspace.md"
TWO_SPACES="$ROOT/tests/fixtures/durability/workspace.json"
FILE=myarc-workspace-v1.json
PORT=8796
PAGE="http://127.0.0.1:$PORT/"

if [ "${2:-}" = "--inner" ]; then
  [ -x "$BIN" ] || { echo "native binary missing: $BIN" >&2; exit 1; }
  CASES=${CASES:-"welcome intro import reveal practice1 practice2 practice3 handoff skip retire foreground"}
  OUTDIR=${OUTDIR:-$ROOT/research-private/captures/onboarding-$(date -u +%Y%m%dT%H%M%SZ)}
  mkdir -p "$OUTDIR"
  PID=""
  # Every profile lives under WORK, which carries the test marker.
  WORK=$(mktemp -d /tmp/myarc-onboarding.XXXXXX)
  printf 'Owned synthetic M0 browser test profiles\n' > "$WORK/.myarc-m0-test-profile"
  # A synthetic page for the practice steps, on loopback only.
  mkdir -p "$WORK/site"
  printf '<!doctype html><title>Field notes</title><h1>Field notes</h1><p>Synthetic page.</p>\n' > "$WORK/site/index.html"
  /usr/bin/python3 -I -m http.server "$PORT" --bind 127.0.0.1 --directory "$WORK/site" > /dev/null 2>&1 &
  SERVER=$!
  cleanup() {
    kill "$SERVER" 2>/dev/null || true
    pkill -KILL -f -- "--user-data-dir=$WORK/" 2>/dev/null || true
    if [ -z "${KEEP:-}" ] && [ -f "$WORK/.myarc-m0-test-profile" ]; then rm -rf "$WORK"
    else echo "kept $WORK"; fi
  }
  trap cleanup EXIT

  # A fresh marked profile with no workspace file; prints its path.
  fresh() {
    local p
    p=$(mktemp -d "$WORK/profile.XXXXXX")
    mkdir -p "$p/Default"
    printf 'Owned synthetic M0 browser test profile\n' > "$p/.myarc-m0-test-profile"
    printf '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}\n' > "$p/Default/Preferences"
    echo "$p"
  }

  # launch <profile> [switches...]: starts the browser in the background and
  # sets PID to its browser process. FOREGROUND=1 launches it as a user does,
  # which may activate it; only the foreground case does that.
  launch() {
    local p=$1; shift
    # -g: launch without activating, so the user's frontmost app keeps focus.
    local how=(-g) background=(--myarc-background-launch)
    [ -z "${FOREGROUND:-}" ] || { how=(); background=(); }
    open ${how[@]+"${how[@]}"} -n -a "$APP" --args --user-data-dir="$p" \
      --no-first-run --no-default-browser-check --use-mock-keychain \
      --enable-logging --log-file="$p/chrome.log" \
      --enable-features=GlassFrame --disable-features=InitialWebUI \
      --disable-backgrounding-occluded-windows ${background[@]+"${background[@]}"} \
      --myarc-visual-parity --myarc-workspace-sidebar --myarc-test-mute-sound --window-size=1280,800 --window-position=40,60 "$@"
    PID=""
    for _ in $(seq 1 80); do
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

  # wait_log <profile> <ERE> <seconds>: waits for a log line, polling every
  # 20 ms, so the kill lands within a few milliseconds of it.
  wait_log() {
    local i
    for i in $(seq 1 $(($3 * 50))); do
      grep -q -E -- "$2" "$1/chrome.log" 2>/dev/null && return 0
      kill -0 "$PID" 2>/dev/null || { echo "browser exited before: $2" >&2; return 1; }
      sleep 0.02
    done
    echo "no log line in $3 s: $2" >&2
    grep -E "Test onboarding" "$1/chrome.log" | tail -5 >&2 || true
    return 1
  }

  # alive <profile> <seconds>: fails when the browser exits within that time,
  # for example on a dangling pointer check.
  alive() {
    sleep "$2"
    kill -0 "$PID" 2>/dev/null && return 0
    echo "browser exited" >&2
    grep -m1 -E "DanglingSignature|FATAL|Check failed" "$1/chrome.log" | cut -c1-300 >&2 || true
    return 1
  }

  kill_now() {
    kill -KILL "$PID"
    wait_exit 10 || { echo "browser survived SIGKILL" >&2; return 1; }
    PID=""
  }

  # check <profile> <stage> <expected record> <workspace check> [baseline]:
  # the record in Local State must be exactly the expected one, and the
  # workspace file must pass the named check.
  check() {
    /usr/bin/python3 -I - "$@" <<'PY'
import json, os, sys
p, stage, expected, ws_check, *rest = sys.argv[1:]
try:
    with open(os.path.join(p, "Local State")) as f:
        record = json.load(f)["myarc"]["onboarding"]["tour"]
except Exception as error:
    sys.exit(f"{stage}: no tour record in Local State: {error!r}")
if record != expected:
    sys.exit(f"{stage}: tour record is {record}, expected {expected}")
path = os.path.join(p, "Default", "myarc-workspace-v1.json")
state = None
if os.path.exists(path):
    try:
        with open(path) as f:
            state = json.load(f)
    except Exception as error:
        sys.exit(f"{stage}: {path} unreadable: {error}")
def need(ok, what):
    if not ok:
        sys.exit(f"{stage}: {what}; spaces {state and state['spaces']}, items {state and state['items']}")
if ws_check == "any":
    pass
elif ws_check == "saved":
    need(state is not None, "no workspace file")
elif ws_check.startswith("imported"):
    need(state is not None, "no workspace file")
    names = [space["name"] for space in state["spaces"]]
    need(all(names.count(n) == 1 for n in ("Desk", "Research", "Reading")), "imported Spaces missing or doubled")
    receipt = json.loads(record)["import"]
    need(receipt and all(str(i) in {s["id"] for s in state["spaces"]} for i in receipt["spaces"]), "receipt names a Space the file lacks")
    urls = sorted(i.get("canonical_url", "") for i in state["items"] if "example.test" in i.get("canonical_url", ""))
    if rest:
        with open(rest[0]) as f:
            before = json.load(f)
        old = sorted(i.get("canonical_url", "") for i in before["items"] if "example.test" in i.get("canonical_url", ""))
        need(urls == old, f"imported rows changed on relaunch: {len(old)} before, {len(urls)} after")
        need(sorted(s["id"] for s in state["spaces"]) == sorted(s["id"] for s in before["spaces"]), "Spaces changed on relaunch")
    else:
        need(len(urls) > 0, "no imported rows")
elif ws_check == "pinned":
    need(state is not None, "no workspace file")
    need(any(i.get("canonical_url", "").startswith("http://127.0.0.1:8796") and i["placement"] != 0 for i in state["items"]), "practice 2's pin is missing")
else:
    sys.exit(f"unknown check {ws_check}")
PY
  }

  # Relaunches the killed profile with logging only, checks the decision,
  # captures the window, quits and checks the files again.
  relaunch() {
    local name=$1 p=$2 expected=$3 ws_check=$4 decision=$5; shift 5
    mv "$p/chrome.log" "$OUTDIR/$name-killed-log.txt"
    cp "$p/Local State" "$OUTDIR/$name-local-state-killed.json"
    [ -f "$p/Default/$FILE" ] && cp "$p/Default/$FILE" "$OUTDIR/$name-workspace-killed.json"
    launch "$p" --myarc-test-onboarding "$@" "$PAGE"
    wait_log "$p" "Test onboarding: profile=.* had_record=1 $decision" 20
    # Long enough for a coach mark or the sheet to show, and for practice 2
    # to judge the page.
    sleep 6
    swift "$ROOT/tools/bgcapture.swift" shot "$PID" "$OUTDIR/$name-relaunched.png" > /dev/null 2>&1 || echo "  no shot of $name" >&2
    kill -TERM "$PID"
    wait_exit 20 || { echo "relaunched browser did not quit" >&2; return 1; }
    PID=""
    cp "$p/chrome.log" "$OUTDIR/$name-relaunch-log.txt"
    if grep -E "effect=open-import|effect=imported" "$p/chrome.log"; then
      echo "the relaunch offered or ran an import" >&2; return 1
    fi
    cp "$p/Local State" "$OUTDIR/$name-local-state-relaunched.json"
    # Only FALLBACK, a step's own rule after the resume, may change the record.
    if grep "Test onboarding: event state=" "$p/chrome.log" | grep -v -F -- "record=${FALLBACK:-none}" | grep . >&2; then
      echo "the relaunch changed the record" >&2; return 1
    fi
    grep -q -F -- "record=${FALLBACK:-none}" "$p/chrome.log" && expected=$FALLBACK
    check "$p" "after relaunch" "$expected" "$ws_check" "$OUTDIR/$name-workspace-killed.json"
  }

  rec() {  # rec <state> <step> <done> [import]
    printf '{"v":1,"state":"%s","step":%s,"done":[%s],"skipped":[],"import":%s}' "$1" "$2" "$3" "${4:-null}"
  }

  case_welcome() {
    local p; p=$(fresh)
    launch "$p" --myarc-test-onboarding about:blank
    wait_log "$p" "first_run=1 had_record=0 state=welcome" 30
    # The arrival ends with the choice at about 5.1 s.
    sleep 6
    kill_now
    check "$p" "after kill" "$(rec welcome 0 '')" any
    relaunch welcome "$p" "$(rec welcome 0 '')" saved "state=welcome step=0 effect=show animate=0"
  }

  case_intro() {
    local p burst; p=$(fresh)
    # The sheet waits 4 s, so the frame burst is running before its intro.
    launch "$p" --myarc-test-onboarding --myarc-test-onboarding-show-delay=4000 about:blank
    wait_log "$p" "first_run=1 had_record=0 state=welcome" 30
    swift "$ROOT/tools/bgcapture.swift" burst "$PID" "$OUTDIR/intro-frames" 6 > /dev/null 2>&1 &
    burst=$!
    sleep 4.5
    kill_now
    wait "$burst" 2>/dev/null || true
    check "$p" "after kill" "$(rec welcome 0 '')" any
    relaunch intro "$p" "$(rec welcome 0 '')" saved "state=welcome step=0 effect=show animate=0"
  }

  case_import() {
    local p receipt; p=$(fresh)
    launch "$p" --myarc-test-onboarding --myarc-test-onboarding-event=import \
      "--myarc-test-onboarding-import=$EXPORT" --myarc-test-onboarding-hold about:blank
    wait_log "$p" "event state=reveal effect=imported" 40
    kill_now
    receipt=$(grep -E 'event state=reveal effect=imported' "$p/chrome.log" | sed -E 's/.*"import":(.*)\}$/\1/')
    check "$p" "after kill" "$(rec reveal 0 '' "$receipt")" imported
    # The import hook stays armed: a welcome that showed would import again.
    relaunch import "$p" "$(rec practice 1 '' "$receipt")" imported "state=practice step=1 effect=show animate=0" \
      --myarc-test-onboarding-event=import "--myarc-test-onboarding-import=$EXPORT"
  }

  case_reveal() {
    local p; p=$(fresh)
    launch "$p" --myarc-test-onboarding --myarc-test-onboarding-event=start_browsing about:blank
    wait_log "$p" 'event state=reveal effect=show' 30
    kill_now
    check "$p" "after kill" "$(rec reveal 0 '')" any
    relaunch reveal "$p" "$(rec practice 1 '')" saved "state=practice step=1 effect=show animate=0"
  }

  case_practice1() {
    local p; p=$(fresh)
    launch "$p" --myarc-test-onboarding --myarc-test-onboarding-event=start_browsing about:blank
    wait_log "$p" 'event state=practice effect=show record=.*"step":1' 30
    kill_now
    check "$p" "after kill" "$(rec practice 1 '')" any
    relaunch practice1 "$p" "$(rec practice 1 '')" saved "state=practice step=1 effect=show animate=0"
  }

  case_practice2() {
    local p; p=$(fresh)
    # The command bar opens the synthetic page, which finishes practice 1.
    launch "$p" --myarc-test-onboarding=practice1 --myarc-capture-open-palette=3000 \
      "--myarc-capture-palette-text=$PAGE" --myarc-capture-palette-enter about:blank
    wait_log "$p" 'event state=practice effect=show record=.*"step":2' 30
    kill_now
    check "$p" "after kill" "$(rec practice 2 1)" any
    # A kill this soon can beat the save of the new tab's address, and then
    # practice 2 finds no page to pin and skips itself, keeping done [1].
    FALLBACK='{"v":1,"state":"practice","step":3,"done":[1],"skipped":[2],"import":null}'
    relaunch practice2 "$p" "$(rec practice 2 1)" saved "state=practice step=2 effect=show animate=0"
  }

  case_practice3() {
    local p; p=$(fresh)
    launch "$p" --myarc-test-onboarding=practice2 --myarc-test-onboarding-event=do "$PAGE"
    wait_log "$p" 'event state=practice effect=show record=.*"step":3' 30
    kill_now
    check "$p" "after kill" "$(rec practice 3 2)" pinned
    relaunch practice3 "$p" "$(rec practice 3 2)" pinned "state=practice step=3 effect=show animate=0"
  }

  case_handoff() {
    local p; p=$(fresh)
    cp "$TWO_SPACES" "$p/Default/$FILE"
    launch "$p" --myarc-test-onboarding=practice3 --myarc-test-onboarding-event=do about:blank
    wait_log "$p" 'event state=handoff' 30
    kill_now
    check "$p" "after kill" "$(rec handoff 0 3)" saved
    relaunch handoff "$p" "$(rec handoff 0 3)" saved "state=handoff step=0 effect=show animate=0"
  }

  case_skip() {
    local p; p=$(fresh)
    launch "$p" --myarc-test-onboarding --myarc-test-onboarding-event=skip_tour about:blank
    wait_log "$p" 'event state=complete' 30
    alive "$p" 3
    kill_now
    check "$p" "after kill" "$(rec complete 0 '')" any
    relaunch skip "$p" "$(rec complete 0 '')" saved "state=complete step=0 effect=none"
  }

  case_retire() {
    local p; p=$(fresh)
    launch "$p" --myarc-test-onboarding=practice1 --myarc-test-onboarding-event=skip_tour about:blank
    wait_log "$p" 'event state=complete' 30
    # The card fades out over 120 ms, then goes.
    alive "$p" 3
    kill_now
    check "$p" "after kill" "$(rec complete 0 '')" any
    relaunch retire "$p" "$(rec complete 0 '')" saved "state=complete step=0 effect=none"
  }

  # windows <name>: the browser's on-screen windows, front to back, also kept
  # in OUTDIR as <name>.txt.
  windows() {
    swift "$ROOT/tools/bgcapture.swift" windows "$PID" list | tee "$OUTDIR/$1.txt"
  }

  case_foreground() {
    local p before dim after; p=$(fresh)
    FOREGROUND=1 launch "$p" --myarc-test-onboarding --myarc-test-onboarding-event=skip_tour \
      --myarc-test-onboarding-event-delay=6000 about:blank
    wait_log "$p" "Test onboarding: intro done" 30
    sleep 1
    before=$(windows foreground-shown)
    # A window kept at alpha 0 is the welcome that never showed.
    if grep -q -E 'layer=0 alpha=0\.([0-8]|9[0-8])' <<< "$before"; then
      echo "a window is not fully opaque after the intro" >&2; return 1
    fi
    # The dim is the largest window, and the welcome shows over it.
    dim=$(grep 'layer=0 ' <<< "$before" | sed -E 's/.*size=([0-9]+)x([0-9]+)/\1 \2 &/' \
      | sort -k1,1nr -k2,2nr | head -1 | sed -E 's/.*(size=[0-9]+x[0-9]+)$/\1/')
    [ "$(grep -c 'layer=0 ' <<< "$before")" -ge 2 ] || { echo "no welcome over the dim" >&2; return 1; }
    [ "$(grep 'layer=0 ' <<< "$before" | head -1 | grep -c -F "$dim")" = 0 ] \
      || { echo "the dim is in front of the welcome" >&2; return 1; }
    wait_log "$p" 'event state=complete' 30
    # The dim fades out over 400 ms, then its windows close.
    sleep 1.5
    after=$(windows foreground-closed)
    if grep -q -F "$dim" <<< "$after"; then
      echo "the dim is still up after the welcome closed ($dim)" >&2; return 1
    fi
    alive "$p" 1
    kill_now
    check "$p" "after kill" "$(rec complete 0 '')" any
    cp "$p/chrome.log" "$OUTDIR/foreground-log.txt"
  }

  RUNS=0
  FAILS=0
  # Each case runs in a subshell that kills its browser if it stops early.
  for name in $CASES; do
    set +e
    ( set -e; trap '[ -z "$PID" ] || kill -KILL "$PID" 2>/dev/null; true' EXIT; "case_$name" )
    status=$?
    set -e
    RUNS=$((RUNS + 1))
    if [ "$status" = 0 ]; then echo "pass  $name"
    else FAILS=$((FAILS + 1)); echo "FAIL  $name"; fi
  done
  echo "$RUNS cases, $FAILS failed; logs and shots in $OUTDIR"
  [ "$FAILS" = 0 ]
  exit
fi

# Lock held for every launch; waits if a build holds it.
/usr/bin/lockf -k /tmp/myarc-build.lock "$0" "$BUILD" --inner
