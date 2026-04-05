#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ABI="${1:-arm64-v8a}"
SRC_SO="$ROOT_DIR/build-android/libmain.so"
DST_DIR="$ROOT_DIR/android-dummy/app/src/main/jniLibs/$ABI"
DST_SO="$DST_DIR/libmain.so"

if [[ ! -f "$SRC_SO" ]]; then
    echo "Missing native library: $SRC_SO"
    echo "Run: make -f makefile.android android-build"
    exit 1
fi

mkdir -p "$DST_DIR"
cp "$SRC_SO" "$DST_SO"

echo "Copied: $SRC_SO"
echo "To:     $DST_SO"
