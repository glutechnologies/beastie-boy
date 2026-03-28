#!/bin/bash
# Validate that vendored libmemif matches the version and API expected by beastie/boy.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LIBMEMIF_DIR="$PROJECT_ROOT/lib/libmemif"
LIBMEMIF_HEADER="$LIBMEMIF_DIR/src/libmemif.h"
REQUIRED_REF="${1:-v26.02}"

require_file() {
  local path="$1"

  if [ ! -e "$path" ]; then
    echo "Missing required file: $path" >&2
    exit 1
  fi
}

require_symbol() {
  local symbol="$1"

  if ! grep -Eq "$symbol" "$LIBMEMIF_HEADER"; then
    echo "libmemif API check failed: could not find '$symbol' in $LIBMEMIF_HEADER" >&2
    exit 1
  fi
}

require_file "$LIBMEMIF_DIR/CMakeLists.txt"
require_file "$LIBMEMIF_HEADER"

if [ ! -d "$LIBMEMIF_DIR/.git" ]; then
  echo "libmemif Git metadata is missing under $LIBMEMIF_DIR" >&2
  exit 1
fi

actual_ref="$(git -C "$LIBMEMIF_DIR" describe --tags --match 'v*' 2>/dev/null || true)"
if [ -z "$actual_ref" ]; then
  echo "Could not determine libmemif version from Git tags in $LIBMEMIF_DIR" >&2
  exit 1
fi

if [ "$actual_ref" != "$REQUIRED_REF" ]; then
  echo "Unsupported libmemif version: found $actual_ref, expected $REQUIRED_REF" >&2
  exit 1
fi

require_symbol 'int memif_create \(memif_conn_handle_t \*conn, memif_conn_args_t \*args,'
require_symbol 'int memif_create_socket \(memif_socket_handle_t \*sock,'
require_symbol 'int memif_poll_event \(memif_socket_handle_t sock, int timeout\);'
require_symbol 'int memif_cancel_poll_event \(memif_socket_handle_t sock\);'

echo "libmemif check passed for $actual_ref"
