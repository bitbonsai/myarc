#!/bin/bash
# Rebuilds the welcome's ribbon video from the user's ribbon page in one go:
# copies it into tools/welcome_ribbon/, renders the showpiece's frames and
# encodes browser/foundation/resources/myarc_welcome_ribbon.mov.
# Usage: tools/make_welcome_ribbon.sh [source index.html]
#   (default /Users/mwolff/dev/arc-logo-3d/index.html). Rebuild after it.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${1:-/Users/mwolff/dev/arc-logo-3d/index.html}"
FRAMES=$(mktemp -d /tmp/ribbon-frames.XXXXXX)
trap 'rm -rf "$FRAMES"' EXIT
cp "$SRC" "$ROOT/tools/welcome_ribbon/index.html"
node "$ROOT/tools/render_welcome_ribbon.mjs" "$FRAMES" 360
swift "$ROOT/tools/encode_hevc_alpha.swift" "$FRAMES" "$ROOT/browser/foundation/resources/myarc_welcome_ribbon.mov" 60
