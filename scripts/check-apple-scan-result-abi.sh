#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
MKSDK="${MKSDK:-$ROOT/MacKernelSDK}"
SDK="$(xcrun --sdk macosx --show-sdk-path)"

if [ ! -f "$MKSDK/Headers/IOKit/80211/apple80211_var.h" ]; then
    echo "ERROR: MacKernelSDK apple80211_var.h missing: $MKSDK/Headers/IOKit/80211/apple80211_var.h" >&2
    echo "Run: git submodule update --init --recursive" >&2
    exit 1
fi

compile_probe() {
    if [ -n "${CXX:-}" ]; then
        "$CXX" "$@"
    else
        xcrun --sdk macosx clang++ "$@"
    fi
}

compile_probe -std=c++17 -arch x86_64 -fsyntax-only \
    -D__IO80211_TARGET=__MAC_15_2 \
    -I"$MKSDK/Headers" \
    -isysroot "$SDK" \
    "$ROOT/tests/test_apple_scan_result_abi.cpp"

echo "Apple scan-result ABI assertions passed for the selected SDK headers."
