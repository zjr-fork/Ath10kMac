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
echo "Native bridge ABI C/C++ layout tests: PASS"
