#!/bin/bash
# Prepare and validate the local build environment.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
REQUIRED_LIBMEMIF_REF="${1:-v26.02}"

require_command() {
  local cmd="$1"

  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Missing required command: $cmd" >&2
    exit 1
  fi
}

require_header() {
  local header="$1"

  if [ ! -f "$header" ]; then
    echo "Missing required header: $header" >&2
    exit 1
  fi
}

echo "Checking build tools..."
require_command git
require_command cmake
require_command make
require_command gcc

echo "Checking system headers..."
require_header /usr/include/pcap/pcap.h

echo "Preparing vendored libmemif..."
bash "$SCRIPT_DIR/get_libmemif.sh"

echo "Validating libmemif compatibility..."
bash "$SCRIPT_DIR/check_libmemif.sh" "$REQUIRED_LIBMEMIF_REF"

echo "Configuration completed successfully."
