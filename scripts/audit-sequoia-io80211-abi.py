#!/usr/bin/env python3
"""Audit Sequoia IO80211 virtual declarations in caller-supplied reference headers.

No Apple/Airport headers are vendored. Point --header-root at a local macOS 15
ABI header tree (for example, a separately obtained Airport SDK header set).
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

EXPECTED = {
    "IO80211Controller": (87, 12, "IO80211ControllerV2.h"),
    "IO80211SkywalkInterface": (134, 1, "IO80211SkywalkInterface.h"),
    "IO80211InfraInterface": (56, 0, "IO80211InfraInterface.h"),
    "IO80211InfraProtocol": (190, 190, "IO80211InfraProtocol.h"),
}


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def class_body(source: str, name: str) -> str:
    clean = strip_comments(source)
    match = re.search(r"\bclass\s+" + re.escape(name) + r"\b[^;{]*\{", clean)
    if not match:
        raise ValueError(f"class {name} not found")
    start = match.end()
    depth = 1
    for pos in range(start, len(clean)):
        if clean[pos] == "{":
            depth += 1
        elif clean[pos] == "}":
            depth -= 1
            if depth == 0:
                return clean[start:pos]
    raise ValueError(f"class {name} has unbalanced braces")


def count_virtuals(body: str) -> tuple[int, int]:
    declarations = re.findall(r"\bvirtual\b[^;{}]*;", body, flags=re.S)
    pure = sum(bool(re.search(r"=\s*0\s*;", decl)) for decl in declarations)
    return len(declarations), pure


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--header-root", type=Path, required=True,
                        help="directory containing the macOS15 header files")
    parser.add_argument("--no-pinned-counts", action="store_true",
                        help="report counts without enforcing the pinned 15.2 inventory")
    args = parser.parse_args()

    failed = False
    for class_name, (want_virtual, want_pure, filename) in EXPECTED.items():
        path = args.header_root / filename
        if not path.is_file():
            print(f"ERROR {class_name}: missing {path}", file=sys.stderr)
            failed = True
            continue
        try:
            body = class_body(path.read_text(encoding="utf-8"), class_name)
            got_virtual, got_pure = count_virtuals(body)
        except (OSError, UnicodeError, ValueError) as error:
            print(f"ERROR {class_name}: {error}", file=sys.stderr)
            failed = True
            continue

        status = "OK"
        if not args.no_pinned_counts and (got_virtual, got_pure) != (want_virtual, want_pure):
            status = "MISMATCH"
            failed = True
        print(f"{status:8} {class_name:28} virtual={got_virtual:3} pure={got_pure:3}"
              f" expected={want_virtual:3}/{want_pure:3}")

    if failed:
        print("ABI inventory check failed; do not generate stubs or change superclass.", file=sys.stderr)
        return 1
    print("ABI declaration inventory matches the pinned Sequoia 15.2 counts.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
