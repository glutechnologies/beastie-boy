#!/bin/bash
# Validate that the system libmemif installation exposes the API expected by beastie.
set -euo pipefail

LIBMEMIF_HEADER="${LIBMEMIF_HEADER:-/usr/include/libmemif.h}"
LIBMEMIF_LIBRARY="${LIBMEMIF_LIBRARY:-/lib/libmemif.so}"

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

require_file "$LIBMEMIF_LIBRARY"
require_file "$LIBMEMIF_HEADER"

require_symbol 'int memif_create \(memif_conn_handle_t \*conn, memif_conn_args_t \*args,'
require_symbol 'int memif_create_socket \(memif_socket_handle_t \*sock,'
require_symbol 'int memif_poll_event \(memif_socket_handle_t sock, int timeout\);'
require_symbol 'int memif_cancel_poll_event \(memif_socket_handle_t sock\);'

echo "libmemif system check passed"
