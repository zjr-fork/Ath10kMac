#!/usr/bin/env python3
"""Generate a local compile-only concrete-subclass probe from supplied Sequoia headers.

The input headers are never copied into the repository. The generated output is
written only to the caller-selected path and should normally live in a temp dir.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def strip_comments(source: str) -> str:
    # Preserve newlines so diagnostics and declaration boundaries remain readable.
    source = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def class_body(source: str, class_name: str) -> str:
    match = re.search(r"\bclass\s+" + re.escape(class_name) + r"\b[^\{]*\{", source)
    if not match:
        raise ValueError(f"class {class_name} not found")
    start = match.end()
    depth = 1
    for index in range(start, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index]
    raise ValueError(f"unterminated class {class_name}")


def pure_virtuals(header: Path, class_name: str) -> list[str]:
    source = strip_comments(header.read_text(encoding="utf-8"))
    body = class_body(source, class_name)
    declarations = re.findall(r"\bvirtual\s+([^;{}]+?)\s*=\s*0\s*;", body, flags=re.S)
    result = []
    for declaration in declarations:
        declaration = " ".join(declaration.split()).strip()
        if not declaration.startswith("IOReturn "):
            raise ValueError(
                f"{class_name} has unsupported pure-virtual return/signature: {declaration}"
            )
        result.append(declaration)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--infra-header", required=True, type=Path)
    parser.add_argument("--skywalk-header", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--expected-infra", type=int, default=190)
    parser.add_argument("--expected-skywalk", type=int, default=1)
    args = parser.parse_args()

    try:
        infra = pure_virtuals(args.infra_header, "IO80211InfraProtocol")
        skywalk = pure_virtuals(args.skywalk_header, "IO80211SkywalkInterface")
        if len(infra) != args.expected_infra:
            raise ValueError(
                f"IO80211InfraProtocol pure-virtual count {len(infra)} "
                f"!= expected {args.expected_infra}"
            )
        if len(skywalk) != args.expected_skywalk:
            raise ValueError(
                f"IO80211SkywalkInterface pure-virtual count {len(skywalk)} "
                f"!= expected {args.expected_skywalk}"
            )

        declarations = skywalk + infra
        names = []
        methods = []
        for declaration in declarations:
            match = re.search(r"\b([A-Za-z_]\w*)\s*\(", declaration)
            if not match:
                raise ValueError(f"cannot parse pure-virtual method name: {declaration}")
            names.append(match.group(1))
            methods.append(
                "    virtual " + declaration + " override { return kIOReturnUnsupported; }"
            )
        if len(set(names)) != len(names):
            raise ValueError("duplicate pure-virtual method names; refusing ambiguous probe")

        output = [
            "// Generated locally from caller-supplied Sequoia headers; do not commit.",
            "#pragma once",
            "#include <type_traits>",
            "#include <IOKit/IOService.h>",
            "#include <net/ethernet.h>",
            "// Load the public Apple80211 ABI first: it defines IO80211LinkState,",
            "// apple80211_scan_data/channel types, ether_addr and ether_header.",
            '#include "apple80211_ioctl.h"',
            "// packet_info_tag is only used through pointers in these declarations.",
            "struct packet_info_tag;",
            "// Private Sequoia pointer-only dependencies need no imported layout.",
            "struct ifnet_init_eparams;",
            "typedef struct ifnet_init_eparams ifnet_init_eparams;",
            "struct nicproxy_info_s;",
            "typedef struct nicproxy_info_s nicproxy_info_s;",
            "// InfraInterface derives from SkywalkInterface; establish the base first.",
            '#include "IO80211SkywalkInterface.h"',
            '#include "IO80211InfraProtocol.h"',
            "",
            "class QcaAirportInfraProtocolCompileProbe : public IO80211InfraProtocol {",
            "public:",
            *methods,
            "};",
            "",
            "static_assert(!std::is_abstract<QcaAirportInfraProtocolCompileProbe>::value,",
            '              "Sequoia infra protocol subclass still has pure virtual methods");',
            "",
        ]
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text("\n".join(output), encoding="utf-8")
        print(
            f"generated {len(infra)} infra + {len(skywalk)} inherited pure-virtual "
            f"stubs at {args.output}"
        )
        return 0
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
