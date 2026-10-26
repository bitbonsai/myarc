#!/bin/bash
# Usage: tools/compare_captures.sh <a.png> <b.png> <outdir>
# Normalizes both images to the size of <a>, writes side-by-side.png and diff.png.
set -euo pipefail
[ $# -eq 3 ] || { echo "usage: $0 <a.png> <b.png> <outdir>" >&2; exit 2; }
A=$1; B=$2; OUT=$3
mkdir -p "$OUT"
SIZE=$(magick identify -format '%wx%h' "$A")
magick "$A" -alpha off "$OUT/a.png"
magick "$B" -alpha off -resize "${SIZE}!" "$OUT/b.png"
magick "$OUT/a.png" "$OUT/b.png" +append "$OUT/side-by-side.png"
# AE with fuzz = count of differing pixels; RMSE = normalized error.
DIFFPX=$(compare -metric AE -fuzz 5% "$OUT/a.png" "$OUT/b.png" "$OUT/diff.png" 2>&1 || true)
RMSE=$(compare -metric RMSE "$OUT/a.png" "$OUT/b.png" null: 2>&1 || true)
TOTAL=$(magick identify -format '%[fx:w*h]' "$OUT/a.png")
PCT=$(awk -v d="$DIFFPX" -v t="$TOTAL" 'BEGIN{printf "%.2f", 100*d/t}')
echo "size=$SIZE differing_pixels=$DIFFPX (${PCT}%, fuzz 5%) rmse=$RMSE"
echo "$OUT/side-by-side.png"
echo "$OUT/diff.png"
