#!/bin/bash
# Checks the bundled blocker in background launches of out/MyarcM0:
#  1. a fresh Profile blocks the fixture's tracker request on its first page
#     load (the page is the startup URL);
#  2. "No filtering" in the blocker's popup, opened by the same code as
#     Window > Ad Blocking for This Site…, lets the request through, and
#     "Optimal" blocks it again;
#  3. myArc's own switch, "Block ads on this site" in the site info popover
#     that Window > Ad Blocking for This Site… opens, turned off lets the
#     request through and turned on blocks it again; each time the blocker's
#     popup shows the matching mode. It's left off for the next step;
#  4. a second Profile, new in the same user-data dir, blocks it on its first
#     load and its switch reads on.
# Steps 3 and 4 need a build with myarc_test_hooks, for
# --myarc-test-blocker-hook.
# Usage: tools/check_blocker.sh [outdir]
# Writes shots, page titles and the fixture server's request log to outdir.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/research-private/captures/blocker-$(date -u +%Y%m%dT%H%M%SZ)}"
EXT=aefnaoagnnooajnoebfnnnpdngplgemc
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

PORT=$(/usr/bin/python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
/usr/bin/python3 -m http.server --bind 127.0.0.1 "$PORT" \
  --directory "$ROOT/tests/fixtures/blocker" 2> "$OUT/server.log" &
SERVER=$!
PROFILE=$(mktemp -d "${TMPDIR:-/tmp}/myarc-blocker.XXXXXX")
touch "$PROFILE/.myarc-m0-test-profile"
cleanup() {
  kill "$SERVER" 2>/dev/null || true
  [ -f "$PROFILE/.myarc-m0-test-profile" ] && rm -rf "$PROFILE"
  true
}
trap cleanup EXIT
URL="http://127.0.0.1:$PORT/index.html"
hits() { grep -c 'GET /cogi-etracker.js' "$OUT/server.log" || true; }
HOOK="$PROFILE/blocker-hook"

# Shared by both launch scripts: hook <word> runs one test hook and waits for
# the browser to take it; switch_state <pattern> [label] waits up to 15 s for
# a new line of the switch's log to match and adds it to result.txt, after
# <label> if given, or "-" to leave it out.
cat > "$OUT/hooks.sh" <<EOF
hook() {
  echo "\$1" > '$HOOK'
  for _ in \$(seq 1 40); do [ -f '$HOOK' ] || return 0; sleep 0.25; done
  echo "hook \$1 not taken" >&2; return 1
}
front() { lsappinfo info -only name "\$(lsappinfo front)" | sed -E 's/.*="(.*)"/\1/'; }
seen=0
switch_state() {
  for _ in \$(seq 1 60); do
    line=\$(tail -n +\$((seen + 1)) '$HOOK.log' 2>/dev/null | grep -m1 -E "\$1" || true)
    if [ -n "\$line" ]; then
      seen=\$(wc -l < '$HOOK.log')
      [ "\${2:-}" = - ] || echo "\${2:+\$2 }switch: \$line" | tee -a '$OUT/result.txt'
      return 0
    fi
    sleep 0.25
  done
  echo "\${2:+\$2 }switch: no line matching \$1" | sed 's/^- //' | tee -a '$OUT/result.txt'; return 1
}
EOF

cat > "$OUT/first.sh" <<EOF
set -u
echo "first load: \$(node '$ROOT/tools/check_blocker.mjs' title '$PROFILE' '$URL')" | tee "$OUT/result.txt"
echo "tracker requests at the server: \$(grep -c 'GET /cogi-etracker.js' '$OUT/server.log')" | tee -a "$OUT/result.txt"
swift '$ROOT/tools/bgcapture.swift' shot "\$PID" '$OUT/1-first-load-blocked.png'
until curl -s "http://127.0.0.1:\$(head -1 '$PROFILE/DevToolsActivePort')/json/list" | grep -q 'chrome-extension://$EXT/popup.html'; do sleep 0.5; done
sleep 1.5
swift '$ROOT/tools/bgcapture.swift' shot "\$PID" '$OUT/2-popup.png'
node '$ROOT/tools/check_blocker.mjs' set-level '$PROFILE' '$EXT' 0 | tee -a "$OUT/result.txt"
sleep 2
echo "after No filtering: \$(node '$ROOT/tools/check_blocker.mjs' title '$PROFILE' '$URL' loaded)" | tee -a "$OUT/result.txt"
echo "tracker requests at the server: \$(grep -c 'GET /cogi-etracker.js' '$OUT/server.log')" | tee -a "$OUT/result.txt"
swift '$ROOT/tools/bgcapture.swift' shot "\$PID" '$OUT/3-site-switched-off.png'
node '$ROOT/tools/check_blocker.mjs' set-level '$PROFILE' '$EXT' 2 | tee -a "$OUT/result.txt"
sleep 2
echo "after Optimal: \$(node '$ROOT/tools/check_blocker.mjs' title '$PROFILE' '$URL' blocked)" | tee -a "$OUT/result.txt"
echo "tracker requests at the server: \$(grep -c 'GET /cogi-etracker.js' '$OUT/server.log')" | tee -a "$OUT/result.txt"
swift '$ROOT/tools/bgcapture.swift' shot "\$PID" '$OUT/3c-site-switched-on.png'

. '$OUT/hooks.sh'
echo "frontmost app before the switch steps: \$(front)" | tee -a "$OUT/result.txt"
hook switch
switch_state 'level=2 on'
sleep 1
swift '$ROOT/tools/bgcapture.swift' shot "\$PID" '$OUT/5-switch-on.png'
hook toggle
switch_state 'level=0 off'
echo "after the switch went off: \$(node '$ROOT/tools/check_blocker.mjs' title '$PROFILE' '$URL' loaded)" | tee -a "$OUT/result.txt"
echo "tracker requests at the server: \$(grep -c 'GET /cogi-etracker.js' '$OUT/server.log')" | tee -a "$OUT/result.txt"
hook switch
switch_state 'level=0 off'
sleep 1
swift '$ROOT/tools/bgcapture.swift' shot "\$PID" '$OUT/6-switch-off.png'
hook popup
node '$ROOT/tools/check_blocker.mjs' popup-mode '$PROFILE' '$EXT' 'no filtering' | tee -a "$OUT/result.txt"
swift '$ROOT/tools/bgcapture.swift' shot "\$PID" '$OUT/7-popup-off.png'
hook switch
switch_state 'level=0 off' -
hook toggle
switch_state 'level=2 on'
echo "after the switch went on: \$(node '$ROOT/tools/check_blocker.mjs' title '$PROFILE' '$URL' blocked)" | tee -a "$OUT/result.txt"
echo "tracker requests at the server: \$(grep -c 'GET /cogi-etracker.js' '$OUT/server.log')" | tee -a "$OUT/result.txt"
hook popup
node '$ROOT/tools/check_blocker.mjs' popup-mode '$PROFILE' '$EXT' optimal | tee -a "$OUT/result.txt"
# Off again, so the second Profile runs while the first leaves the site alone.
hook switch
switch_state 'level=2 on' -
hook toggle
switch_state 'level=0 off' left
node '$ROOT/tools/check_blocker.mjs' title '$PROFILE' '$URL' loaded >/dev/null
echo "frontmost app after them: \$(front)" | tee -a "$OUT/result.txt"
EOF

PROFILE_DIR="$PROFILE" URL="$URL" SETTLE=4 \
  ARGS="--remote-debugging-port=0 --myarc-capture-blocker-popup=14000 --myarc-test-blocker-hook=$HOOK" \
  SCRIPT="$OUT/first.sh" "$ROOT/tools/capture_native.sh" "$OUT/3b-end-default.png" >/dev/null

# A second Profile created later in the same user-data dir.
SECOND=Profile2
mkdir -p "$PROFILE/$SECOND"
printf '{"vertical_tabs":{"enabled":true,"collapsed_state":false,"enabled_first_time":true}}\n' \
  > "$PROFILE/$SECOND/Preferences"
rm -f "$PROFILE/DevToolsActivePort"
BEFORE=$(hits)
cat > "$OUT/second.sh" <<EOF
set -u
echo "second Profile first load: \$(node '$ROOT/tools/check_blocker.mjs' title '$PROFILE' '$URL')" | tee -a "$OUT/result.txt"
echo "tracker requests at the server: \$(grep -c 'GET /cogi-etracker.js' '$OUT/server.log') (was $BEFORE)" | tee -a "$OUT/result.txt"
. '$OUT/hooks.sh'
seen=\$(wc -l < '$HOOK.log')
hook switch
switch_state 'level=2 on' 'second Profile'
sleep 1
swift '$ROOT/tools/bgcapture.swift' shot "\$PID" '$OUT/8-second-profile-switch-on.png'
EOF
PROFILE_DIR="$PROFILE" URL="$URL" SETTLE=4 \
  ARGS="--remote-debugging-port=0 --profile-directory=$SECOND --myarc-test-blocker-hook=$HOOK" \
  SCRIPT="$OUT/second.sh" "$ROOT/tools/capture_native.sh" "$OUT/4-second-profile-blocked.png" >/dev/null

rm -f "$OUT/first.sh" "$OUT/second.sh" "$OUT/hooks.sh"
grep -E '"GET /(index.html|cogi-etracker.js)' "$OUT/server.log" | sed -E 's/^.*\] //' > "$OUT/requests.txt" || true
echo "$OUT"
