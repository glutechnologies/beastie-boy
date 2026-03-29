#!/bin/bash
# Prepare and validate the local build environment.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

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

require_file() {
  local path="$1"

  if [ ! -f "$path" ]; then
    echo "Missing required file: $path" >&2
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
require_header /usr/include/libmemif.h
require_header /usr/include/vapi/vapi.h
require_header /usr/include/vapi/vpe.api.vapi.h

echo "Checking system libraries..."
require_file /lib/libmemif.so
require_file /lib/x86_64-linux-gnu/libvapiclient.so

echo "Validating libmemif compatibility..."
bash "$SCRIPT_DIR/check_libmemif.sh"

echo "Configuration completed successfully."
