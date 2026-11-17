#!/bin/bash
# Checks the bundled blocker in background launches of out/MyarcM0:
#  1. a fresh Profile blocks the fixture's tracker request on its first page
#     load (the page is the startup URL);
#  2. "No filtering" in the blocker's popup, opened by the same code as
#     Window > Ad Blocking for This Site…, lets the request through, and
#     "Optimal" blocks it again;
#  3. a second Profile, new in the same user-data dir, blocks it again.
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
EOF

PROFILE_DIR="$PROFILE" URL="$URL" SETTLE=4 \
  ARGS="--remote-debugging-port=0 --myarc-capture-blocker-popup=14000" \
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
EOF
PROFILE_DIR="$PROFILE" URL="$URL" SETTLE=4 \
  ARGS="--remote-debugging-port=0 --profile-directory=$SECOND" \
  SCRIPT="$OUT/second.sh" "$ROOT/tools/capture_native.sh" "$OUT/4-second-profile-blocked.png" >/dev/null

rm -f "$OUT/first.sh" "$OUT/second.sh"
grep -E '"GET /(index.html|cogi-etracker.js)' "$OUT/server.log" | sed -E 's/^.*\] //' > "$OUT/requests.txt" || true
echo "$OUT"
