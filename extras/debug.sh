#!/bin/bash
# Print a build/debug summary without modifying the workspace.
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LIBMEMIF_HEADER="/usr/include/libmemif.h"
LIBMEMIF_LIB="/lib/libmemif.so"

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
print_kv "libmemif.h" "$(file_status "$LIBMEMIF_HEADER")"
print_kv "vapi/vapi.h" "$(file_status /usr/include/vapi/vapi.h)"
print_kv "vapi/vpe.api.vapi.h" "$(file_status /usr/include/vapi/vpe.api.vapi.h)"
echo

echo "System libraries"
echo "----------------"
print_kv "libmemif.so" "$(file_status "$LIBMEMIF_LIB")"
print_kv "libvapiclient.so" "$(file_status /lib/x86_64-linux-gnu/libvapiclient.so)"
echo

echo "Compatibility check"
echo "-------------------"
if bash "$SCRIPT_DIR/check_libmemif.sh" >/tmp/beastie-boy-check-libmemif.out 2>/tmp/beastie-boy-check-libmemif.err; then
  print_kv "status" "ok"
  sed 's/^/  /' /tmp/beastie-boy-check-libmemif.out
else
  print_kv "status" "failed"
  if [ -s /tmp/beastie-boy-check-libmemif.err ]; then
    sed 's/^/  /' /tmp/beastie-boy-check-libmemif.err
  fi
fi

rm -f /tmp/beastie-boy-check-libmemif.out /tmp/beastie-boy-check-libmemif.err
