#!/usr/bin/env python3
import importlib.util
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
        private_spi_include = generated.index('#include "apple_private_spi.h"')
        apple80211_include = generated.index('#include "apple80211_ioctl.h"')
        skywalk_include = generated.index('#include "IO80211SkywalkInterface.h"')
        infra_include = generated.index('#include "IO80211InfraProtocol.h"')
        if not (private_spi_include < apple80211_include < skywalk_include < infra_include):
            raise SystemExit("Apple80211 ABI, Skywalk base, and InfraProtocol headers are out of order")
        if '#include <net/ethernet.h>' not in generated:
            raise SystemExit("missing Ethernet ABI header before Skywalk declarations")
        if "struct nicproxy_info_s;" not in generated:
            raise SystemExit("missing nicproxy_info_s pointer forward declaration")
        if "struct apple80211_chip_stats;" not in generated:
            raise SystemExit("missing opaque apple80211_chip_stats forward declaration")

        if "sizeof(IO80211SkywalkInterface) == 0x118" not in generated:
            raise SystemExit("infra probe omitted the pinned Skywalk base-size assertion")
        if "sizeof(IO80211InfraProtocol) == 0x120" not in generated:
            raise SystemExit("infra probe omitted the pinned InfraProtocol base-size assertion")

        # Exercise the exact 190-slot audit: a same-count reorder must fail.
        spec = importlib.util.spec_from_file_location("infra_probe_generator", script)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        audited = root / "AuditedInfraProtocol.h"
        declarations = [
            f" virtual IOReturn {name}(void*) = 0;"
            for name in module.EXPECTED_INFRA_METHOD_ORDER
        ]
        audited.write_text(
            "class IO80211InfraProtocol { public:\n"
            + "\n".join(declarations) + "\n};\n",
            encoding="utf-8",
        )
        audited_command = command.copy()
        audited_command[audited_command.index("--infra-header") + 1] = str(audited)
        audited_command[audited_command.index("--expected-infra") + 1] = "190"
        good_audit = subprocess.run(audited_command, capture_output=True, text=True)
        if good_audit.returncode:
            raise SystemExit(f"generator rejected the audited 190-slot fixture:\n{good_audit.stderr}")
        declarations[0], declarations[1] = declarations[1], declarations[0]
        audited.write_text(
            "class IO80211InfraProtocol { public:\n"
            + "\n".join(declarations) + "\n};\n",
            encoding="utf-8",
        )
        bad_order = subprocess.run(audited_command, capture_output=True, text=True)
        if bad_order.returncode == 0:
            raise SystemExit("generator accepted reordered infra virtual slots")

        bad_command = command.copy()
        bad_command[bad_command.index("--expected-infra") + 1] = "3"
        bad = subprocess.run(bad_command, capture_output=True, text=True)
        if bad.returncode == 0:
            raise SystemExit("generator accepted an incorrect pure-virtual count")

    print("Sequoia abstractness generator fixture tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
