#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
HEADER_ROOT="${SEQUOIA_HEADER_ROOT:-}"
if [ -z "$HEADER_ROOT" ]; then
    echo "ERROR: set SEQUOIA_HEADER_ROOT to the directory containing the Sequoia 15 headers" >&2
    echo "Required: IO80211InfraProtocol.h and IO80211SkywalkInterface.h" >&2
    exit 1
fi
HEADER_ROOT="$(cd "$HEADER_ROOT" && pwd)"
INCLUDE_ROOT="${SEQUOIA_INCLUDE_ROOT:-$(cd "$HEADER_ROOT/../.." && pwd)}"
MKSDK="${MKSDK:-$ROOT/MacKernelSDK}"
SDK="$(xcrun --sdk macosx --show-sdk-path)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

for header in IO80211InfraProtocol.h IO80211SkywalkInterface.h; do
    if [ ! -f "$HEADER_ROOT/$header" ]; then
        echo "ERROR: missing $HEADER_ROOT/$header" >&2
        exit 1
    fi
done
if [ ! -d "$MKSDK/Headers" ]; then
    echo "ERROR: MacKernelSDK headers missing: $MKSDK/Headers" >&2
    exit 1
fi

python3 "$ROOT/scripts/generate-sequoia-infra-abstractness-probe.py" \
    --infra-header "$HEADER_ROOT/IO80211InfraProtocol.h" \
    --skywalk-header "$HEADER_ROOT/IO80211SkywalkInterface.h" \
    --output "$TMP_DIR/QcaAirportInfraProtocolCompileProbe.hpp"

if [ -n "${CXX:-}" ]; then
    "$CXX" -std=c++17 -arch x86_64 -fsyntax-only -DKERNEL \
        -D__IO80211_TARGET=__MAC_15_2 \
        -I"$INCLUDE_ROOT" -I"$HEADER_ROOT" -I"$MKSDK/Headers" \
        -isysroot "$SDK" "$TMP_DIR/QcaAirportInfraProtocolCompileProbe.hpp"
else
    xcrun --sdk macosx clang++ -std=c++17 -arch x86_64 -fsyntax-only -DKERNEL \
        -D__IO80211_TARGET=__MAC_15_2 \
        -I"$INCLUDE_ROOT" -I"$HEADER_ROOT" -I"$MKSDK/Headers" \
        -isysroot "$SDK" "$TMP_DIR/QcaAirportInfraProtocolCompileProbe.hpp"
fi

echo "Sequoia infra abstractness compile probe passed."
