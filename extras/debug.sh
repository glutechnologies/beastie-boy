#!/bin/bash
# Print a build/debug summary without modifying the workspace.
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LIBMEMIF_DIR="$PROJECT_ROOT/lib/libmemif"
LIBMEMIF_BUILD_DIR="$LIBMEMIF_DIR/build"
LIBMEMIF_LIB="$LIBMEMIF_BUILD_DIR/lib/libmemif.so"
REQUIRED_LIBMEMIF_REF="${1:-v26.02}"

print_kv() {
  printf "%-24s %s\n" "$1" "$2"
}

command_status() {
  local cmd="$1"
  if command -v "$cmd" >/dev/null 2>&1; then
    echo "yes ($(command -v "$cmd"))"
  else
    echo "no"
  fi
}

file_status() {
  local path="$1"
  if [ -e "$path" ]; then
    echo "yes"
  else
    echo "no"
  fi
}

echo "Beastie-Boy debug summary"
echo "========================="
print_kv "project root" "$PROJECT_ROOT"
print_kv "required libmemif ref" "$REQUIRED_LIBMEMIF_REF"
echo

echo "Tools"
echo "-----"
print_kv "git" "$(command_status git)"
print_kv "cmake" "$(command_status cmake)"
print_kv "make" "$(command_status make)"
print_kv "gcc" "$(command_status gcc)"
echo

echo "System headers"
echo "--------------"
print_kv "pcap/pcap.h" "$(file_status /usr/include/pcap/pcap.h)"
echo

echo "Vendored libmemif"
echo "-----------------"
print_kv "directory present" "$(file_status "$LIBMEMIF_DIR")"
print_kv "CMakeLists.txt" "$(file_status "$LIBMEMIF_DIR/CMakeLists.txt")"
print_kv "git metadata" "$(file_status "$LIBMEMIF_DIR/.git")"
print_kv "header" "$(file_status "$LIBMEMIF_DIR/src/libmemif.h")"
print_kv "shared library" "$(file_status "$LIBMEMIF_LIB")"

if [ -d "$LIBMEMIF_DIR/.git" ]; then
  actual_ref="$(git -C "$LIBMEMIF_DIR" describe --tags --match 'v*' 2>/dev/null || true)"
  print_kv "detected ref" "${actual_ref:-unavailable}"
else
  print_kv "detected ref" "unavailable"
fi

echo
echo "Compatibility check"
echo "-------------------"
if bash "$SCRIPT_DIR/check_libmemif.sh" "$REQUIRED_LIBMEMIF_REF" >/tmp/beastie-boy-check-libmemif.out 2>/tmp/beastie-boy-check-libmemif.err; then
  print_kv "status" "ok"
  sed 's/^/  /' /tmp/beastie-boy-check-libmemif.out
else
  print_kv "status" "failed"
  if [ -s /tmp/beastie-boy-check-libmemif.err ]; then
    sed 's/^/  /' /tmp/beastie-boy-check-libmemif.err
  fi
fi

rm -f /tmp/beastie-boy-check-libmemif.out /tmp/beastie-boy-check-libmemif.err
