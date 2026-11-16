#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
HEADER_ROOT="${SEQUOIA_HEADER_ROOT:-}"
if [ -z "$HEADER_ROOT" ]; then
    echo "ERROR: set SEQUOIA_HEADER_ROOT to the directory containing IO80211ControllerV2.h" >&2
    exit 1
fi
HEADER_ROOT="$(cd "$HEADER_ROOT" && pwd)"
INCLUDE_ROOT="${SEQUOIA_INCLUDE_ROOT:-$(cd "$HEADER_ROOT/../.." && pwd)}"
AIRPORT_INCLUDE_ROOT="${SEQUOIA_AIRPORT_INCLUDE_ROOT:-$INCLUDE_ROOT/Airport}"
MKSDK="${MKSDK:-$ROOT/MacKernelSDK}"
SDK="$(xcrun --sdk macosx --show-sdk-path)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

for path in "$HEADER_ROOT/IO80211ControllerV2.h" "$AIRPORT_INCLUDE_ROOT/apple80211_ioctl.h" "$MKSDK/Headers"; do
    if [ ! -e "$path" ]; then
        echo "ERROR: missing required Sequoia compile input: $path" >&2
        exit 1
    fi
done

python3 "$ROOT/scripts/generate-sequoia-controller-abstractness-probe.py" \
    --controller-header "$HEADER_ROOT/IO80211ControllerV2.h" \
    --output "$TMP_DIR/QcaAirportControllerCompileProbe.hpp"

if [ -n "${CXX:-}" ]; then
    "$CXX" -std=c++17 -arch x86_64 -fsyntax-only -DKERNEL -DAIRPORT_WCL \
        -D__IO80211_TARGET=__MAC_15_2 \
        -I"$INCLUDE_ROOT" -I"$AIRPORT_INCLUDE_ROOT" -I"$HEADER_ROOT" -I"$MKSDK/Headers" \
        -isysroot "$SDK" "$TMP_DIR/QcaAirportControllerCompileProbe.hpp"
else
    xcrun --sdk macosx clang++ -std=c++17 -arch x86_64 -fsyntax-only -DKERNEL -DAIRPORT_WCL \
        -D__IO80211_TARGET=__MAC_15_2 \
        -I"$INCLUDE_ROOT" -I"$AIRPORT_INCLUDE_ROOT" -I"$HEADER_ROOT" -I"$MKSDK/Headers" \
        -isysroot "$SDK" "$TMP_DIR/QcaAirportControllerCompileProbe.hpp"
fi

echo "Sequoia controller abstractness compile probe passed."
