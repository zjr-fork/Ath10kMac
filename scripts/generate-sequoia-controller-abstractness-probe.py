#!/usr/bin/env python3
"""Generate a temporary concrete-subclass probe for Sequoia IO80211Controller."""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


EXPECTED_CONTROLLER_METHODS = (
    "isCommandProhibited",
    "handleCardSpecific",
    "getDRIVER_VERSION",
    "getHARDWARE_VERSION",
    "getCARD_CAPABILITIES",
    "getPOWER",
    "setPOWER",
    "getCOUNTRY_CODE",
    "setCOUNTRY_CODE",
    "setGET_DEBUG_INFO",
    "getLogger",
    "getFaultReporterFromDriver",
)


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def class_body(source: str, class_name: str) -> str:
    match = re.search(r"\bclass\s+" + re.escape(class_name) + r"\b[^\{]*\{", source)
    if not match:
        raise ValueError(f"class {class_name} not found")
    start, depth = match.end(), 1
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index]
    raise ValueError(f"unterminated class {class_name}")


def pure_virtuals(header: Path, class_name: str) -> list[str]:
    body = class_body(strip_comments(header.read_text(encoding="utf-8")), class_name)
    declarations = re.findall(r"\bvirtual\s+([^;{}]+?)\s*=\s*0\s*;", body, flags=re.S)
    return [" ".join(item.split()).strip() for item in declarations]


def stub_for(declaration: str) -> str:
    match = re.search(r"^(.+?)\s+([A-Za-z_]\w*)\s*\(", declaration)
    if not match:
        raise ValueError(f"cannot parse pure-virtual method: {declaration}")
    result_type, name = match.group(1).strip(), match.group(2)
    if result_type == "bool":
        body = "return false;"
    elif result_type == "IOReturn":
        body = "return kIOReturnUnsupported;"
    elif result_type == "SInt32":
        body = "return static_cast<SInt32>(kIOReturnUnsupported);"
    elif result_type.endswith("*"):
        body = "return nullptr;"
    else:
        raise ValueError(f"unsupported pure-virtual return type for {name}: {result_type}")
    return f"    virtual {declaration} override {{ {body} }}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--controller-header", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--expected", type=int, default=12)
    args = parser.parse_args()
    try:
        methods = pure_virtuals(args.controller_header, "IO80211Controller")
        if len(methods) != args.expected:
            raise ValueError(f"IO80211Controller pure-virtual count {len(methods)} != expected {args.expected}")
        names = []
        rendered = []
        for declaration in methods:
            match = re.search(r"\b([A-Za-z_]\w*)\s*\(", declaration)
            if not match:
                raise ValueError(f"cannot parse pure-virtual method name: {declaration}")
            names.append(match.group(1))
            rendered.append(stub_for(declaration))
        if len(set(names)) != len(names):
            raise ValueError("duplicate pure-virtual method names; refusing ambiguous probe")
        if args.expected == 12 and tuple(names) != EXPECTED_CONTROLLER_METHODS:
            first = next((i for i, pair in enumerate(zip(names, EXPECTED_CONTROLLER_METHODS))
                          if pair[0] != pair[1]), min(len(names), len(EXPECTED_CONTROLLER_METHODS)))
            raise ValueError(
                "IO80211Controller pure-virtual method order differs from the audited "
                f"Sequoia ABI at slot {first}: actual={names[first:first + 1]}, "
                f"expected={list(EXPECTED_CONTROLLER_METHODS[first:first + 1])}"
            )
        output = [
            "// Generated locally from caller-supplied Sequoia headers; do not commit.",
            "#pragma once",
            "#include <type_traits>",
            '#include "apple_private_spi.h"',
            '#include "apple80211_ioctl.h"',
            '#include "IO80211ControllerV2.h"',
            "",
            "class QcaAirportControllerCompileProbe : public IO80211Controller {",
            "public:",
            *rendered,
            "};",
            "",
            "static_assert(!std::is_abstract<QcaAirportControllerCompileProbe>::value,",
            '              "Sequoia controller subclass still has pure virtual methods");',
            "",
        ]
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text("\n".join(output), encoding="utf-8")
        print(f"generated {len(methods)} controller pure-virtual stubs at {args.output}")
        return 0
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
