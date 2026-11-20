#!/bin/bash
# Package the nightly build as a signed, optionally notarized DMG outside the repository.
# Signs with a Developer ID or Apple Development identity when the keychain has one, then
# "myArc Local Signing", then ad-hoc. SIGN_ID overrides the choice.
# Builds the myarc CLI in release mode and bundles it as Contents/Helpers/myarc.
# It reports VERSION through MYARC_VERSION, so the approval card shows the
# nightly label.
# Signs inside out with the hardened runtime, a secure timestamp and Chromium's
# own entitlements from chrome/app (JIT for the renderer and GPU helpers).
# With a Developer ID and the notarytool keychain profile NOTARY_PROFILE, it
# notarizes and staples the app, then the DMG. NOTARIZE=0 skips that.
# Usage: tools/package_nightly.sh
#   env: APP_SRC=<built myArc.app> DATE=YYYY-MM-DD VERSION=<label> TAG=<release tag>
#        OUT=<output dir, default ~/.cache/myarc-m0/release/DATE>
#        NOTARIZE=0|1 NOTARY_PROFILE=<keychain profile, default myarc-notary>
#        ENTITLEMENTS=<dir with Chromium's entitlement plists>
#        CHROMIUM_SRC=<checkout> ALLOW_STALE=1 (skip the stale build check)
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
ENTITLEMENTS=${ENTITLEMENTS:-$HOME/.cache/myarc-m0/chromium/src/chrome/app}
NOTARY_PROFILE=${NOTARY_PROFILE:-myarc-notary}
STAGE="$OUT/stage"
APP="$STAGE/myArc Nightly.app"

[ -d "$APP_SRC" ] || { echo "built app missing: $APP_SRC" >&2; exit 1; }
for plist in app helper-renderer helper-gpu; do
  [ -f "$ENTITLEMENTS/$plist-entitlements.plist" ] || { echo "entitlements missing: $ENTITLEMENTS/$plist-entitlements.plist" >&2; exit 1; }
done
case "$OUT" in "$ROOT"*) echo "OUT must be outside the repository" >&2; exit 1;; esac

# A failed build leaves the previous app in place. Refuse a framework older than
# the Chromium checkout's last commit. ALLOW_STALE=1 skips this check.
CHROMIUM_SRC=${CHROMIUM_SRC:-$HOME/.cache/myarc-m0/chromium/src}
if [ "${ALLOW_STALE:-0}" != 1 ] && git -C "$CHROMIUM_SRC" rev-parse -q --verify HEAD >/dev/null 2>&1; then
  FRAMEWORK=$(find "$APP_SRC/Contents/Frameworks" -maxdepth 4 -type f -name '* Framework' | head -1)
  BUILT=$(stat -f %m "$FRAMEWORK")
  HEAD_TIME=$(git -C "$CHROMIUM_SRC" log -1 --format=%ct)
  if [ "$BUILT" -lt "$HEAD_TIME" ]; then
    echo "built app is older than Chromium HEAD $(git -C "$CHROMIUM_SRC" log -1 --format=%h); did the build fail?" >&2
    exit 1
  fi
fi

mkdir -p "$OUT"
rm -rf "$STAGE"
mkdir -p "$STAGE"
ditto "$APP_SRC" "$APP"
xattr -cr "$APP"

# The CLI and MCP server ships inside the bundle and gets the app's signature.
MYARC_VERSION="$VERSION" cargo build --manifest-path "$ROOT/Cargo.toml" --release --locked -p myarc-cli --target aarch64-apple-darwin
CTL="$APP/Contents/Helpers/myarc"
mkdir -p "$APP/Contents/Helpers"
cp "$ROOT/target/aarch64-apple-darwin/release/myarc" "$CTL"
"$CTL" --help >/dev/null
case "$("$CTL" --version)" in "myarc $VERSION "*) ;; *) echo "myarc does not report $VERSION" >&2; exit 1;; esac

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

# Same parts and options as Chromium's chrome/installer/mac/signing/parts.py.
# Library validation needs a Team ID, and the JIT helpers can't have it.
TIMESTAMP=--timestamp
FULL=runtime,restrict,kill
case "$SIGN_ID" in
  "Developer ID Application: "*|"Apple Development: "*) FULL=$FULL,library;;
  *) TIMESTAMP=--timestamp=none;;
esac
JIT=runtime,restrict,kill
sign() { codesign --force "$TIMESTAMP" --sign "$SIGN_ID" "$@"; }
FW="$APP/Contents/Frameworks/myArc Framework.framework"
FWV=$(dirname "$FRAMEWORK")
for lib in "$FWV"/Libraries/*.dylib; do sign "$lib"; done
for tool in chrome_crashpad_handler app_mode_loader web_app_shortcut_copier; do
  if [ -e "$FWV/Helpers/$tool" ]; then sign --options "$FULL" "$FWV/Helpers/$tool"; fi
done
for helper in "$FWV"/Helpers/*.app; do
  case "$helper" in
    *Renderer\).app) sign --options "$JIT" --entitlements "$ENTITLEMENTS/helper-renderer-entitlements.plist" "$helper";;
    *GPU\).app) sign --options "$JIT" --entitlements "$ENTITLEMENTS/helper-gpu-entitlements.plist" "$helper";;
    *) sign --options "$FULL" "$helper";;
  esac
done
sign "$FW"
sign --options "$FULL" "$CTL"
sign --options "$FULL" --entitlements "$ENTITLEMENTS/app-entitlements.plist" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
codesign --verify --strict "$CTL"
echo "myarc: $(codesign -dvv "$CTL" 2>&1 | sed -n 's/^Authority=//p' | head -1)"

# Notarize only with a Developer ID and a working keychain profile.
if [ -z "${NOTARIZE:-}" ]; then
  NOTARIZE=0
  case "$SIGN_ID" in "Developer ID Application: "*)
    xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1 && NOTARIZE=1;;
  esac
fi
notarize() {
  local out id
  out=$(xcrun notarytool submit "$1" --keychain-profile "$NOTARY_PROFILE" --wait --output-format plist || true)
  id=$(plutil -extract id raw -o - - <<<"$out" 2>/dev/null || true)
  [ -n "$id" ] || { echo "notarytool submit failed for $(basename "$1")" >&2; exit 1; }
  if [ "$(plutil -extract status raw -o - - <<<"$out")" != Accepted ]; then
    echo "notarization failed for $(basename "$1"), submission $id:" >&2
    xcrun notarytool log "$id" --keychain-profile "$NOTARY_PROFILE" >&2
    exit 1
  fi
  echo "notarized: $(basename "$1") ($id)"
}
if [ "$NOTARIZE" = 1 ]; then
  # The app gets its own ticket so it opens offline after leaving the DMG.
  ZIP="$OUT/notarize-app.zip"
  rm -f "$ZIP"
  ditto -c -k --keepParent "$APP" "$ZIP"
  notarize "$ZIP"
  rm "$ZIP"
  xcrun stapler staple "$APP"
  xcrun stapler validate "$APP"
else
  echo "notarization skipped"
fi

ln -s /Applications "$STAGE/Applications"
rm -f "$OUT/$NAME"
hdiutil create -quiet -volname "myArc Nightly" -srcfolder "$STAGE" -format ULFO "$OUT/$NAME"
rm "$STAGE/Applications"
if [ "$NOTARIZE" = 1 ]; then
  sign "$OUT/$NAME"
  notarize "$OUT/$NAME"
  xcrun stapler staple "$OUT/$NAME"
  xcrun stapler validate "$OUT/$NAME"
  spctl -a -vvv -t install "$OUT/$NAME"
  spctl -a -vvv "$APP"
fi

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
