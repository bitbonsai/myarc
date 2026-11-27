#!/bin/bash
# Launch the local MyarcRelease build with its own isolated profile directory
# so it can run side by side with production /Applications/myArc.app without
# colliding on user data or Chromium's ProcessSingleton lock.
set -euo pipefail

APP="${APP:-$HOME/.cache/myarc-m0/chromium/src/out/MyarcRelease/myArc.app}"
DEV_PROFILE="${DEV_PROFILE:-$HOME/Library/Application Support/myArc Dev}"

if [ ! -d "$APP" ]; then
  echo "missing local build: $APP" >&2
  exit 1
fi

mkdir -p "$DEV_PROFILE"

# Launch in background (-g) as a separate instance (-n) with its own data dir.
exec /usr/bin/lockf -k /tmp/myarc-build.lock \
  open -g -n -a "$APP" --args \
  --user-data-dir="$DEV_PROFILE" \
  --no-first-run \
  --no-default-browser-check \
  --myarc-visual-parity \
  --myarc-workspace-sidebar \
  "$@"
