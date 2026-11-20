#!/bin/bash
# Downloads the pinned Sparkle 2 release once, checks its SHA-256 and prints
# the directory holding Sparkle.framework, bin/sign_update and bin/generate_keys.
# tools/package_nightly.sh embeds the framework and signs the DMG with it.
# Usage: tools/fetch_sparkle.sh   env: SPARKLE_DIR=<cache dir>
set -euo pipefail

VERSION=2.10.1
SHA256=1a856d2a9edb8e628de88121d7be04cf0e2167f8641650b78fca00b14244cf27
URL=https://github.com/sparkle-project/Sparkle/releases/download/$VERSION/Sparkle-$VERSION.tar.xz
DIR=${SPARKLE_DIR:-$HOME/.cache/myarc-m0/sparkle/$VERSION}

if [ ! -f "$DIR/.verified" ]; then
  rm -rf "$DIR"
  mkdir -p "$DIR"
  TMP=$(mktemp -d)
  trap 'rm -rf "$TMP"' EXIT
  curl -fsSL -o "$TMP/sparkle.tar.xz" "$URL"
  if [ "$(shasum -a 256 "$TMP/sparkle.tar.xz" | cut -d' ' -f1)" != "$SHA256" ]; then
    echo "Sparkle $VERSION does not match its pinned SHA-256" >&2
    exit 1
  fi
  tar -xJf "$TMP/sparkle.tar.xz" -C "$DIR" ./Sparkle.framework ./bin/sign_update ./bin/generate_keys ./LICENSE
  touch "$DIR/.verified"
fi
echo "$DIR"
