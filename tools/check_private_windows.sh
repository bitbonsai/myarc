#!/bin/bash
# Private window check (CORE-01): no title or address from a private window
# reaches myArc's files. Serves synthetic pages with one-off titles and paths
# from a local fixture server. A background launch opens a regular page in
# the regular window, and --myarc-test-private-window opens the private pages
# in a private window beside it and pins one there. Each private page renames
# itself and pushes a new path. The browser then ends two ways: a clean quit
# (SIGTERM) and, in a second profile, a SIGKILL. Each time it greps the workspace file, its
# backups, Local State and every file under the user-data dir whose name
# mentions myarc for the private token, and checks that the regular page did
# reach the workspace file, so the file was being written. Other Chromium
# files that hold the private token are listed too, as a warning.
# Needs a build with myarc_test_hooks.
# Usage: tools/check_private_windows.sh   env: APP=<myArc.app> KEEP=1
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export APP="${APP:-$HOME/.cache/myarc-m0/chromium/src/out/MyarcRelease/myArc.app}"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/myarc-private.XXXXXX")
SERVER=""
cleanup() {
  [ -n "$SERVER" ] && kill "$SERVER" 2>/dev/null || true
  [ -n "${KEEP:-}" ] || rm -rf "$WORK"
}
trap cleanup EXIT
TOKEN="mpw$(od -An -N6 -tx1 /dev/urandom | tr -d ' \n')"
REGULAR="mrw$(od -An -N6 -tx1 /dev/urandom | tr -d ' \n')"
SITE="$WORK/site"
mkdir -p "$SITE"
for n in 1 2; do
  cat > "$SITE/private-$TOKEN-$n.html" <<EOF
<!doctype html>
<title>Private $TOKEN title $n</title>
<p>Private page $n.</p>
<script>
setTimeout(() => {
  history.pushState({}, '', '/pushed-$TOKEN-$n')
  document.title = 'Private $TOKEN retitled $n'
}, 1000)
</script>
EOF
done
printf '<!doctype html>\n<title>Regular %s</title>\n<p>Regular page.</p>\n' "$REGULAR" > "$SITE/regular-$REGULAR.html"
PORT=$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$SITE" > "$WORK/server.log" 2>&1 &
SERVER=$!
for _ in $(seq 1 40); do
  curl -sf "http://127.0.0.1:$PORT/private-$TOKEN-1.html" > /dev/null && break
  sleep 0.25
done
PRIVATE="http://127.0.0.1:$PORT/private-$TOKEN-1.html,http://127.0.0.1:$PORT/private-$TOKEN-2.html"
printf 'kill -KILL "$PID"\nsleep 1\n' > "$WORK/kill.sh"
printf 'sleep 4\n' > "$WORK/wait.sh"
FAILED=0

check() {
  local name=$1 profile=$2
  grep -q "GET /private-$TOKEN-1.html" "$WORK/server.log" || { echo "FAIL $name: the private page never loaded" >&2; FAILED=1; return; }
  grep -q "Test private window: pin ok" "$profile/chrome.log" || { echo "FAIL $name: the private tab wasn't pinned" >&2; FAILED=1; return; }
  grep -q -F "$REGULAR" "$profile/Default/myarc-workspace-v1.json" 2>/dev/null || { echo "FAIL $name: the regular page isn't in the workspace file, so it wasn't being written" >&2; FAILED=1; return; }
  local files=()
  while IFS= read -r -d '' file; do files+=("$file"); done < <(
    find "$profile" -type f \( -iname '*myarc*' -o -name 'Local State' \) -print0)
  local leaks
  leaks=$(grep -l -a -F "$TOKEN" "${files[@]}" 2>/dev/null || true)
  if [ -n "$leaks" ]; then
    echo "FAIL $name: private page in" $leaks >&2
    FAILED=1
  else
    echo "ok $name: ${#files[@]} myArc files, no private title or address"
  fi
  local other
  other=$(grep -r -l -a -F "$TOKEN" "$profile" 2>/dev/null || true)
  [ -z "$other" ] || echo "warn $name: Chromium files hold the token:" $other >&2
  : > "$WORK/server.log"
}

for mode in quit kill; do
  profile="$WORK/profile-$mode"
  mkdir -p "$profile"
  touch "$profile/.myarc-m0-test-profile"
  script="$WORK/wait.sh"
  [ "$mode" = kill ] && script="$WORK/kill.sh"
  # capture_native.sh quits with SIGTERM, Chromium's clean shutdown; the kill
  # run SIGKILLs first. A locked screen fails the capture, not the check.
  PROFILE_DIR="$profile" URL="http://127.0.0.1:$PORT/regular-$REGULAR.html" SETTLE=10 SCRIPT="$script" \
    ARGS="--myarc-test-private-window=$PRIVATE --enable-logging --log-file=$profile/chrome.log" \
    "$ROOT/tools/capture_native.sh" "$WORK/$mode.png" > "$WORK/$mode.txt" 2>&1 || true
  sleep 1
  check "$mode" "$profile"
done
exit "$FAILED"
