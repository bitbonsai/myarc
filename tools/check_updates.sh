#!/bin/bash
# Checks Sparkle updates end to end without publishing anything. Serves a local
# appcast on 127.0.0.1 and launches the older app in the background with a
# disposable profile and --myarc-test-update-feed, three times:
#   older: the appcast lists an older nightly, so no update is offered.
#   tampered: the newer DMG with another file's signature, so Sparkle refuses it.
#   good: the newer DMG with its own signature. Sparkle downloads and extracts
#     it, checks the signature without aborting, and installs it over the test
#     copy when the app quits.
# Both apps come from tools/package_nightly.sh on a build with
# myarc_test_hooks = true, which compiles the feed switch. Needs the site venv
# from `mise run site-check`. Takes the build lock.
# Usage: tools/check_updates.sh <old myArc Nightly.app> <new DMG> <new DMG's ed_signature> <new VERSION>
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
if [ "${1:-}" != --inner ]; then
  exec /usr/bin/lockf -k /tmp/myarc-build.lock "$0" --inner "$@"
fi
shift
OLD_APP=$1 NEW_DMG=$2 SIGNATURE=$3 NEW_VERSION=$4
PORT=${PORT:-8797}
DOMAIN=dev.myarc.nightly
WORK=$(mktemp -d "${TMPDIR:-/tmp}/myarc-updates.XXXXXX")
SPARKLE=$("$ROOT/tools/fetch_sparkle.sh")

# Sparkle saves its check times in the app's defaults, which the installed
# nightly shares. Remember which SU keys exist so cleanup removes only new ones.
BEFORE=$(defaults read "$DOMAIN" 2>/dev/null | sed -n 's/^ *\(SU[A-Za-z]*\) = .*/\1/p' || true)
SERVER=""
cleanup() {
  [ -n "$SERVER" ] && kill "$SERVER" 2>/dev/null || true
  for key in $(defaults read "$DOMAIN" 2>/dev/null | sed -n 's/^ *\(SU[A-Za-z]*\) = .*/\1/p'); do
    grep -qx "$key" <<<"$BEFORE" || defaults delete "$DOMAIN" "$key"
  done
  rm -rf "$HOME/Library/Caches/$DOMAIN/org.sparkle-project.Sparkle" "$WORK"
}
trap cleanup EXIT

mkdir -p "$WORK/feed"
cp "$NEW_DMG" "$WORK/feed/new.dmg"
OTHER_SIGNATURE=$("$SPARKLE/bin/sign_update" --account myarc-sparkle -p "$OLD_APP/Contents/Info.plist")
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$WORK/feed" >"$WORK/server.log" 2>&1 &
SERVER=$!

# Writes the appcast with site/download.py, pointed at the local DMG.
feed() {
  "$ROOT/.venv/bin/python" - "$ROOT/site" "$WORK/feed/$1.xml" "$2" "$3" "$(stat -f %z "$NEW_DMG")" "http://127.0.0.1:$PORT/new.dmg" <<'EOF'
import sys
sys.path.insert(0, sys.argv[1])
from download import appcast
path, version, signature, size, url = sys.argv[2:]
release = {'version': version, 'date': '2026-11-20', 'release_url': url, 'min_macos': '26',
           'dmg_url': url, 'size_bytes': int(size), 'ed_signature': signature}
open(path, 'w').write(appcast(release))
EOF
}
feed older 2026.11.19 "$SIGNATURE"
feed tampered "$NEW_VERSION" "$OTHER_SIGNATURE"
feed good "$NEW_VERSION" "$SIGNATURE"

# Runs one case and waits for one of the expected log lines.
run() {
  local name=$1 expect=$2 app="$WORK/$1/myArc Nightly.app" profile="$WORK/$1/profile" pid=""
  mkdir -p "$profile"
  ditto "$OLD_APP" "$app"
  open -g -n -a "$app" --args --user-data-dir="$profile" --no-first-run --no-default-browser-check \
    --use-mock-keychain --myarc-background-launch --myarc-visual-parity --myarc-workspace-sidebar \
    --enable-logging --log-file="$profile/chrome.log" \
    --myarc-test-update-feed="http://127.0.0.1:$PORT/$name.xml"
  for _ in $(seq 1 240); do
    grep -Eq "myarc-update: ($expect)" "$profile/chrome.log" 2>/dev/null && break
    sleep 0.5
  done
  # The signature check follows extraction; give it time to abort.
  [ "$name" = good ] && sleep 15
  for p in $(pgrep -f -- "--user-data-dir=$profile" || true); do
    ps -o command= -p "$p" | grep -q -- '--type=' || pid=$p
  done
  echo "$name:"
  grep -o 'myarc-update: .*' "$profile/chrome.log" | sed 's/^/  /' || echo "  no updater lines"
  [ -n "$pid" ] && kill -TERM "$pid" 2>/dev/null || true
  for _ in $(seq 1 100); do pgrep -f -- "--user-data-dir=$profile" >/dev/null || break; sleep 0.2; done
  grep -Eq "myarc-update: ($expect)" "$profile/chrome.log"
}

FAIL=0
run older none || FAIL=1
run tampered aborted || FAIL=1
if grep -q 'myarc-update: ready' "$WORK/tampered/profile/chrome.log"; then
  echo "tampered: Sparkle accepted a DMG with the wrong signature" >&2; FAIL=1
fi
run good extracted || FAIL=1
if grep -q 'myarc-update: aborted' "$WORK/good/profile/chrome.log"; then
  echo "good: Sparkle refused the correctly signed DMG" >&2; FAIL=1
fi
# Sparkle installs over the quit app's copy a moment after it exits.
for _ in $(seq 1 120); do
  WANT=$(/usr/libexec/PlistBuddy -c 'Print :MyarcVersion' "$WORK/good/myArc Nightly.app/Contents/Info.plist" 2>/dev/null || true)
  [ "$WANT" = "$NEW_VERSION" ] && break
  sleep 0.5
done
echo "good: installed copy reports $WANT"
[ "$WANT" = "$NEW_VERSION" ] || FAIL=1
codesign --verify --deep --strict "$WORK/good/myArc Nightly.app" && echo "good: installed copy passes codesign" || FAIL=1
[ "$FAIL" = 0 ] && echo "updates: all cases passed" || { echo "updates: a case failed" >&2; exit 1; }
