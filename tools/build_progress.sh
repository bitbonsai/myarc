#!/bin/sh
# Print one-line Chromium build/packaging progress.
# Usage: tools/build_progress.sh [out_dir]
set -eu

SRC="${CHROMIUM_SRC:-$HOME/.cache/myarc-m0/chromium/src}"
OUT="${1:-$SRC/out/MyarcNightly}"
SISO="$SRC/third_party/siso/cipd/siso"

if [ -x "$SISO" ] && line=$("$SISO" ps -C "$OUT" 2>/dev/null | grep -m1 '\[[0-9]*/[0-9]*'); then
  printf '%s\n' "$line" | awk '
    match($0, /\[[0-9]+\/[0-9]+/) {
      s = substr($0, RSTART + 1, RLENGTH - 1)
      split(s, a, "/")
      done = a[1] + 0; total = a[2] + 0
      pct = total > 0 ? (done * 100.0 / total) : 0
      printf "%.1f%% (%d/%d, %d remaining)\n", pct, done, total, total - done
    }'
  exit 0
fi

for proc in package_nightly notarytool codesign cargo; do
  if pgrep -qf "$proc"; then
    echo "Running: $proc"
    exit 0
  fi
done

echo "No build running"
