#!/bin/bash
# Package the nightly build as a locally signed DMG outside the repository.
# Signs with a Developer ID or Apple Development identity when the keychain has one, then
# "myArc Local Signing", then ad-hoc. SIGN_ID overrides the choice.
# Builds the myarc CLI in release mode and bundles it as Contents/Helpers/myarc,
# with a myarcctl symlink to it as a deprecated alias for one release.
# Usage: tools/package_nightly.sh
#   env: APP_SRC=<built myArc.app> DATE=YYYY-MM-DD VERSION=<label> TAG=<release tag>
#        OUT=<output dir, default ~/.cache/myarc-m0/release/DATE>
# Prints the DMG path, size, SHA-256 and the site/release.json values.
set -euo pipefail

REPO=https://github.com/bitbonsai/myarc
ROOT=$(cd "$(dirname "$0")/.." && pwd)
APP_SRC=${APP_SRC:-$HOME/.cache/myarc-m0/chromium/src/out/MyarcNightly/myArc.app}
DATE=${DATE:-$(date +%F)}
VERSION=${VERSION:-${DATE//-/.}}
TAG=${TAG:-nightly-$VERSION}
OUT=${OUT:-$HOME/.cache/myarc-m0/release/$DATE}
NAME="myArc-Nightly-$VERSION-arm64.dmg"
STAGE="$OUT/stage"
APP="$STAGE/myArc Nightly.app"

[ -d "$APP_SRC" ] || { echo "built app missing: $APP_SRC" >&2; exit 1; }
case "$OUT" in "$ROOT"*) echo "OUT must be outside the repository" >&2; exit 1;; esac

mkdir -p "$OUT"
rm -rf "$STAGE"
mkdir -p "$STAGE"
ditto "$APP_SRC" "$APP"
xattr -cr "$APP"

# The CLI and MCP server ships inside the bundle and gets the app's signature.
cargo build --manifest-path "$ROOT/Cargo.toml" --release --locked -p myarc-cli --target aarch64-apple-darwin
CTL="$APP/Contents/Helpers/myarc"
mkdir -p "$APP/Contents/Helpers"
cp "$ROOT/target/aarch64-apple-darwin/release/myarc" "$CTL"
ln -sf myarc "$APP/Contents/Helpers/myarcctl"
"$CTL" --help >/dev/null
"$APP/Contents/Helpers/myarcctl" --version >/dev/null

# The app menu shows this label above About. Chromium's own version stays.
PLIST="$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c 'Delete :MyarcVersion' "$PLIST" 2>/dev/null || true
/usr/libexec/PlistBuddy -c "Add :MyarcVersion string $VERSION" "$PLIST"
echo "MyarcVersion: $(/usr/libexec/PlistBuddy -c 'Print :MyarcVersion' "$PLIST")"

# The bundle must not load anything from the build tree.
BIN="$APP/Contents/MacOS/myArc"
FRAMEWORK=$(find "$APP/Contents/Frameworks/myArc Framework.framework/Versions" -maxdepth 2 -type f -name 'myArc Framework' | head -1)
[ -n "$FRAMEWORK" ] || { echo "framework binary missing in $APP" >&2; exit 1; }
if otool -XL "$BIN" "$FRAMEWORK" "$CTL" | grep -E '/out/|/Users/' ; then
  echo "binary references the build tree" >&2; exit 1
fi
echo "otool: no build-tree references in $(basename "$BIN"), $(basename "$FRAMEWORK") or myarc"

# The Keychain pins Safe Storage access to the signer's Team ID when the cert
# has one, so an Apple Development cert keeps Always Allow across builds. A
# self-signed or ad-hoc signature gets pinned to each build's hash instead,
# and every new build asks for the password again.
if [ -z "${SIGN_ID:-}" ]; then
  IDS=$(security find-identity -v -p codesigning)
  SIGN_ID=$(sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' <<<"$IDS" | head -1)
  [ -n "$SIGN_ID" ] || SIGN_ID=$(sed -n 's/.*"\(Apple Development: [^"]*\)".*/\1/p' <<<"$IDS" | head -1)
  SIGN_ID=${SIGN_ID:-myArc Local Signing}
fi
security find-identity -p codesigning | grep -qF "\"$SIGN_ID\"" || SIGN_ID=-
echo "signing with: $SIGN_ID"
codesign --force --sign "$SIGN_ID" "$CTL"
codesign --force --deep --sign "$SIGN_ID" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
codesign --verify --strict "$CTL"
echo "myarc: $(codesign -dvv "$CTL" 2>&1 | sed -n 's/^Authority=//p' | head -1)"

ln -s /Applications "$STAGE/Applications"
rm -f "$OUT/$NAME"
hdiutil create -quiet -volname "myArc Nightly" -srcfolder "$STAGE" -format ULFO "$OUT/$NAME"
rm "$STAGE/Applications"

SIZE=$(stat -f %z "$OUT/$NAME")
SHA=$(shasum -a 256 "$OUT/$NAME" | cut -d' ' -f1)
echo "$SHA  $NAME" > "$OUT/$NAME.sha256"
echo "dmg: $OUT/$NAME"
echo "size: $SIZE"
echo "sha256: $SHA"
echo "bundle id: $(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP/Contents/Info.plist")"
echo "profile dir: ~/Library/Application Support/$(/usr/libexec/PlistBuddy -c 'Print :CrProductDirName' "$APP/Contents/Info.plist")"
echo "site/release.json (set published after the upload):"
cat <<EOF
{
  "published": true,
  "version": "$VERSION",
  "date": "$DATE",
  "dmg_url": "$REPO/releases/download/$TAG/$NAME",
  "size_bytes": $SIZE,
  "sha256": "$SHA",
  "release_url": "$REPO/releases/tag/$TAG",
  "min_macos": "26",
  "proprietary_codecs": true
}
EOF
