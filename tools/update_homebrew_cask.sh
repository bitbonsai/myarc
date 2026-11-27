#!/bin/bash
# Update Casks/myarc.rb in bitbonsai/homebrew-tap from site/release.json.
# Usage: tools/update_homebrew_cask.sh [--dry-run]
#   env: TAP_REPO (default https://github.com/bitbonsai/homebrew-tap.git)
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RELEASE_JSON="$ROOT/site/release.json"
TAP_REPO=${TAP_REPO:-https://github.com/bitbonsai/homebrew-tap.git}
DRY_RUN=0
if [ "${1:-}" = "--dry-run" ]; then
  DRY_RUN=1
fi

read -r VERSION SHA256 < <(python3 - "$RELEASE_JSON" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
if not data.get('published'):
    raise SystemExit('site/release.json is not published')
print(data['version'], data['sha256'])
PY
)

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

git clone --depth 1 "$TAP_REPO" "$TMP/tap"
CASK="$TMP/tap/Casks/myarc.rb"
[ -f "$CASK" ] || { echo "missing $CASK" >&2; exit 1; }

python3 - "$CASK" "$VERSION" "$SHA256" <<'PY'
from pathlib import Path
import re, sys

path, version, sha256 = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
text = path.read_text()
text, n_ver = re.subn(r'^(\s*version\s+)"[^"]+"', rf'\1"{version}"', text, count=1, flags=re.M)
text, n_sha = re.subn(r'^(\s*sha256\s+)"[^"]+"', rf'\1"{sha256}"', text, count=1, flags=re.M)
if n_ver != 1 or n_sha != 1:
    raise SystemExit(f'unexpected cask format (version={n_ver}, sha256={n_sha})')
path.write_text(text)
PY

if git -C "$TMP/tap" diff --quiet; then
  echo "homebrew-tap already at v$VERSION"
  exit 0
fi

git -C "$TMP/tap" diff
if [ "$DRY_RUN" = 1 ]; then
  echo "dry-run: skipping commit and push"
  exit 0
fi

git -C "$TMP/tap" config user.name "bitbonsai"
git -C "$TMP/tap" config user.email "44185+bitbonsai@users.noreply.github.com"
git -C "$TMP/tap" add Casks/myarc.rb
git -C "$TMP/tap" commit -m "Update myarc cask to v$VERSION"
git -C "$TMP/tap" push origin main
echo "updated homebrew-tap to v$VERSION"
