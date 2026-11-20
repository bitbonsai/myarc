#!/bin/bash
# Checks the crash report prompt in a test-hooks build, in the background.
# Usage: tools/check_crash_report.sh   env: APP=<myArc.app> OUTDIR=<dir>
#   1. Launches a disposable profile and SIGKILLs it, so Chromium records a
#      crashed session.
#   2. Relaunches with the synthetic crash log in tests/fixtures/crash-report:
#      the prompt must show, and its issue URL must carry the versions and
#      the log's top lines without the Mac's identifiers. Captures it.
#   3. Relaunches after that clean quit: no prompt.
#   4. SIGKILLs again and relaunches: the prompt shows without the log it
#      already offered.
#   5. Relaunches after a SIGKILL with --myarc-test-crash-report-press and
#      captures the GitHub tab Report opens.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export APP="${APP:-$HOME/.cache/myarc-m0/chromium/src/out/MyarcNightly/myArc.app}"
OUT="${OUTDIR:-$ROOT/research-private/captures/crash-report-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$OUT"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/myarc-crash.XXXXXX")
export PROFILE_DIR="$WORK/profile"
LOGS="$WORK/logs"
mkdir -p "$PROFILE_DIR" "$LOGS"
touch "$PROFILE_DIR/.myarc-m0-test-profile"
trap 'rm -rf "$WORK"' EXIT
# A fresh copy, so the log reads as minutes old.
cp "$ROOT/tests/fixtures/crash-report/"*.ips "$LOGS/"
touch "$LOGS/"*.ips
KILL="$WORK/kill.sh"
printf 'kill -KILL "$PID"\nsleep 1\n' > "$KILL"
# The tour is seeded complete so the welcome doesn't cover the card.
HOOKS="--myarc-test-crash-logs=$LOGS --myarc-test-crash-report-url=$WORK/url.txt --myarc-test-onboarding=complete"

crash() {
  # Chromium saves the session as running a few seconds after launch, so the
  # kill waits for that. The capture after the kill finds no window.
  ARGS=--myarc-test-onboarding=complete SETTLE=15 SCRIPT="$KILL" "$ROOT/tools/capture_native.sh" "$WORK/killed.png" > /dev/null 2>&1 || true
}
run() {
  rm -f "$WORK/url.txt"
  # A locked screen has no display to capture; the checks still run.
  ARGS="$HOOKS ${1:-}" SETTLE="${2:-6}" "$ROOT/tools/capture_native.sh" "$3" > "$WORK/run.txt" 2>&1 ||
    echo "capture failed: $(grep -m1 -E 'no on-screen|exited early|Terminating' "$WORK/run.txt" || tail -1 "$WORK/run.txt")" >&2
  sleep 1
}
fail() { echo "FAIL: $*" >&2; exit 1; }
decode() { python3 -c 'import sys, urllib.parse; print(urllib.parse.unquote(open(sys.argv[1]).read()))' "$1"; }

crash
run "" 6 "$OUT/prompt.png"
[ -s "$WORK/url.txt" ] || fail "no prompt after a crash"
decode "$WORK/url.txt" > "$OUT/issue-1.txt"
grep -q '^https://github.com/bitbonsai/myarc/issues/new?title=Crash: myArc ' "$OUT/issue-1.txt" || fail "issue URL"
grep -q '\*\*macOS:\*\* [0-9]' "$OUT/issue-1.txt" || fail "macOS version"
grep -q 'Chromium [0-9.]*)' "$OUT/issue-1.txt" || fail "Chromium version"
grep -q 'myArc-2026-11-20-150000.ips' "$OUT/issue-1.txt" || fail "log name"
grep -q 'EXC_BAD_ACCESS' "$OUT/issue-1.txt" || fail "exception"
grep -q 'myArc Framework  myarc::SyntheticFixtureCrash()' "$OUT/issue-1.txt" || fail "crashed frames"
grep -q -E 'crashReporterKey|deviceIdentifierForVendor|"userID"|"incident"|0000-000000000002' "$OUT/issue-1.txt" && fail "identifier kept"
[ "$(grep -c '^```' "$OUT/issue-1.txt")" = 2 ] || fail "code fence"
echo "ok: prompt after a crash, with the log"

run "" 6 "$OUT/clean.png"
[ -s "$WORK/url.txt" ] && fail "prompt after a clean quit"
echo "ok: no prompt after a clean quit"

crash
run "" 6 "$OUT/again.png"
[ -s "$WORK/url.txt" ] || fail "no prompt after the second crash"
decode "$WORK/url.txt" > "$OUT/issue-2.txt"
grep -q 'myArc-2026-11-20-150000.ips' "$OUT/issue-2.txt" && fail "same log offered twice"
grep -q "macOS didn't write one" "$OUT/issue-2.txt" || fail "no-log text"
echo "ok: second crash prompts without the log already offered"

crash
run "--myarc-test-crash-report-press" 10 "$OUT/github.png"
[ -s "$WORK/url.txt" ] || fail "no prompt before Report"
echo "ok: Report pressed; see $OUT/github.png"
echo "$OUT"
