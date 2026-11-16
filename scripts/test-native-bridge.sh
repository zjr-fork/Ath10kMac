#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/ath10k-native-abi.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

CXX=$(xcrun --find clang++)
CC=$(xcrun --find clang)
"$CXX" -std=c++17 -Wall -Wextra -Werror -fsyntax-only "$ROOT/tests/test_native_bridge.cpp"
"$CC" -std=c11 -Wall -Wextra -Werror -fsyntax-only "$ROOT/tests/test_native_bridge.c"
"$CXX" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_native_bridge.cpp" -o "$TMP/test-cxx"
"$CC" -std=c11 -Wall -Wextra -Werror "$ROOT/tests/test_native_bridge.c" -o "$TMP/test-c"
"$TMP/test-cxx"
"$TMP/test-c"
"$CXX" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_native_scan_cursor.cpp" -o "$TMP/test-scan-cursor"
"$TMP/test-scan-cursor"
"$CXX" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_wcl_scan_result.cpp" -o "$TMP/test-wcl-scan-result"
"$TMP/test-wcl-scan-result"
make -C "$ROOT" check-native-scan-builder
echo "Native bridge ABI, scan cursor, WCL result serializer, and native WMI scan builder checks: PASS"
