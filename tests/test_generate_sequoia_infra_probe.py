#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    script = Path(__file__).resolve().parents[1] / "scripts" / "generate-sequoia-infra-abstractness-probe.py"
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        infra = root / "IO80211InfraProtocol.h"
        skywalk = root / "IO80211SkywalkInterface.h"
        output = root / "GeneratedProbe.hpp"
        infra.write_text(
            "class IO80211InfraProtocol { public:\n"
            " virtual IOReturn getOne(One*) = 0;\n"
            " virtual IOReturn setTwo(Two*) = 0;\n"
            "};\n",
            encoding="utf-8",
        )
        skywalk.write_text(
            "class IO80211SkywalkInterface { public:\n"
            " virtual IOReturn setMacAddress(ether_addr&) = 0;\n"
            "};\n",
            encoding="utf-8",
        )
        command = [
            sys.executable, str(script),
            "--infra-header", str(infra),
            "--skywalk-header", str(skywalk),
            "--output", str(output),
            "--expected-infra", "2",
            "--expected-skywalk", "1",
        ]
        good = subprocess.run(command, capture_output=True, text=True)
        if good.returncode != 0:
            raise SystemExit(f"generator rejected valid fixture:\n{good.stderr}")
        generated = output.read_text(encoding="utf-8")
        for method in ("getOne", "setTwo", "setMacAddress"):
            if method not in generated:
                raise SystemExit(f"generated probe missing {method}")
        if generated.count("kIOReturnUnsupported") != 3:
            raise SystemExit("generated probe did not emit exactly three unsupported stubs")

        bad_command = command.copy()
        bad_command[bad_command.index("--expected-infra") + 1] = "3"
        bad = subprocess.run(bad_command, capture_output=True, text=True)
        if bad.returncode == 0:
            raise SystemExit("generator accepted an incorrect pure-virtual count")

    print("Sequoia abstractness generator fixture tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
