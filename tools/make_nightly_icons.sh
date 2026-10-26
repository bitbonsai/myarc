#!/bin/bash
# Regenerate the nightly app icon (app.icns + Assets.car) from the matte mark.
# Usage: tools/make_nightly_icons.sh   Needs Python 3 with Pillow, iconutil, actool.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/design/brand/assets/myarc-matte.png"
OUT="$ROOT/browser/foundation/nightly"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/myarc-icons.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

# The mark sits on Apple's 1024 grid at 824 px, centered on transparency.
python3 - "$SRC" "$WORK/icon-1024.png" <<'PY'
import sys
from PIL import Image
mark = Image.open(sys.argv[1]).convert('RGBA').resize((824, 824), Image.LANCZOS)
canvas = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
canvas.paste(mark, (100, 100), mark)
canvas.save(sys.argv[2])
PY

ICONSET="$WORK/app.iconset"
CATALOG="$WORK/Assets.xcassets/AppIcon.appiconset"
mkdir -p "$ICONSET" "$CATALOG"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" "$WORK/icon-1024.png" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
  sips -z $((size * 2)) $((size * 2)) "$WORK/icon-1024.png" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$OUT/app.icns"

IMAGES=""
for size in 16 32 128 256 512; do
  cp "$ICONSET/icon_${size}x${size}.png" "$ICONSET/icon_${size}x${size}@2x.png" "$CATALOG/"
  IMAGES+="{\"idiom\":\"mac\",\"size\":\"${size}x${size}\",\"scale\":\"1x\",\"filename\":\"icon_${size}x${size}.png\"},"
  IMAGES+="{\"idiom\":\"mac\",\"size\":\"${size}x${size}\",\"scale\":\"2x\",\"filename\":\"icon_${size}x${size}@2x.png\"},"
done
printf '{"images":[%s],"info":{"version":1,"author":"xcode"}}\n' "${IMAGES%,}" > "$CATALOG/Contents.json"
printf '{"info":{"version":1,"author":"xcode"}}\n' > "$WORK/Assets.xcassets/Contents.json"
mkdir -p "$WORK/car"
xcrun actool --compile "$WORK/car" --platform macosx --minimum-deployment-target 12.0 \
  --app-icon AppIcon --output-partial-info-plist "$WORK/partial.plist" \
  --output-format human-readable-text "$WORK/Assets.xcassets" >/dev/null
cp "$WORK/car/Assets.car" "$OUT/Assets.car"
echo "$OUT/app.icns"
echo "$OUT/Assets.car"
