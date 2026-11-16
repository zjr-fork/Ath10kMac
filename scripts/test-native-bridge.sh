#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/ath10k-native-abi.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

run_test() {
    name="$1"
    shift
    if "$@"; then
        echo "PASS: $name"
    else
        status=$?
        echo "FAIL: $name (exit $status)" >&2
        return "$status"
    fi
}

CXX=$(xcrun --sdk macosx --find clang++)
CC=$(xcrun --sdk macosx --find clang)
SDK=$(xcrun --sdk macosx --show-sdk-path)
if [ ! -f "$SDK/usr/lib/libc++.tbd" ] && [ ! -f "$SDK/usr/lib/libc++.dylib" ]; then
    echo "ERROR: macOS SDK does not contain libc++: $SDK/usr/lib" >&2
    echo "Install/update Xcode Command Line Tools, then run: xcode-select --switch /Library/Developer/CommandLineTools" >&2
    exit 1
fi
"$CXX" -isysroot "$SDK" -std=c++17 -Wall -Wextra -Werror -fsyntax-only "$ROOT/tests/test_native_bridge.cpp"
"$CC" -isysroot "$SDK" -std=c11 -Wall -Wextra -Werror -fsyntax-only "$ROOT/tests/test_native_bridge.c"
"$CXX" -isysroot "$SDK" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_native_bridge.cpp" -o "$TMP/test-cxx"
"$CC" -isysroot "$SDK" -std=c11 -Wall -Wextra -Werror "$ROOT/tests/test_native_bridge.c" -o "$TMP/test-c"
"$TMP/test-cxx"
"$TMP/test-c"
"$CXX" -isysroot "$SDK" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_native_scan_cursor.cpp" -o "$TMP/test-scan-cursor"
"$TMP/test-scan-cursor"
"$CXX" -isysroot "$SDK" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_wcl_scan_result.cpp" -o "$TMP/test-wcl-scan-result"
"$TMP/test-wcl-scan-result"
"$CXX" -isysroot "$SDK" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_wcl_scan_lifecycle.cpp" -o "$TMP/test-wcl-scan-lifecycle"
"$CXX" -isysroot "$SDK" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_wcl_join_lifecycle.cpp" -o "$TMP/test-wcl-join-lifecycle"
"$CXX" -isysroot "$SDK" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_native_join_observer.cpp" -o "$TMP/test-native-join-observer"
"$TMP/test-native-join-observer"
"$CXX" -isysroot "$SDK" -std=c++17 -Wall -Wextra -Werror "$ROOT/tests/test_native_scan_observer.cpp" -o "$TMP/test-native-scan-observer"
"$TMP/test-native-scan-observer"
"$TMP/test-wcl-join-lifecycle"
"$TMP/test-wcl-scan-lifecycle"
make -C "$ROOT" check-native-scan-builder
echo "Native bridge ABI, scan cursor, WCL result serializer/scan+join lifecycle/Join+Scan snapshot observers, and native WMI scan builder checks: PASS"
