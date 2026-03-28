#!/bin/bash
# Download and build libmemif into lib/libmemif
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LIB_DIR="$PROJECT_ROOT/lib"
LIBMEMIF_DIR="$LIB_DIR/libmemif"
LIBMEMIF_BUILD_DIR="$LIBMEMIF_DIR/build"
TMP_VPP_DIR="$LIB_DIR/vpp-tmp"
VPP_REF="${VPP_REF:-v26.02}"

bootstrap_libmemif() {
  rm -rf "$TMP_VPP_DIR"
  git clone --depth 1 --branch "$VPP_REF" https://github.com/FDio/vpp.git "$TMP_VPP_DIR"
  rm -rf "$LIBMEMIF_DIR"
  cp -r "$TMP_VPP_DIR/extras/libmemif" "$LIBMEMIF_DIR"
  rm -rf "$TMP_VPP_DIR"
}

prepare_git_metadata() {
  if [ ! -d "$LIBMEMIF_DIR/.git" ]; then
    git -C "$LIBMEMIF_DIR" init -q
    git -C "$LIBMEMIF_DIR" add .
    git -C "$LIBMEMIF_DIR" \
      -c user.name="beastie-boy" \
      -c user.email="noreply@example.invalid" \
      commit -q -m "Import libmemif $VPP_REF"
  fi

  git -C "$LIBMEMIF_DIR" \
    -c user.name="beastie-boy" \
    -c user.email="noreply@example.invalid" \
    tag -fa "$VPP_REF" -m "libmemif $VPP_REF"
}

prepare_include_tree() {
  local header

  mkdir -p "$LIBMEMIF_DIR/include/memif"

  if [ -f "$LIBMEMIF_DIR/libmemif.h" ]; then
    header="../../libmemif.h"
  elif [ -f "$LIBMEMIF_DIR/src/libmemif.h" ]; then
    header="../../src/libmemif.h"
  else
    echo "Could not find libmemif.h under $LIBMEMIF_DIR" >&2
    exit 1
  fi

  ln -sfn "$header" "$LIBMEMIF_DIR/include/memif/libmemif.h"
}

mkdir -p "$LIB_DIR"

if [ ! -f "$LIBMEMIF_DIR/CMakeLists.txt" ]; then
  bootstrap_libmemif
fi

prepare_include_tree
prepare_git_metadata

cmake -S "$LIBMEMIF_DIR" -B "$LIBMEMIF_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -Wno-dev
cmake --build "$LIBMEMIF_BUILD_DIR" --target memif

echo "libmemif downloaded and built in lib/libmemif/build"
