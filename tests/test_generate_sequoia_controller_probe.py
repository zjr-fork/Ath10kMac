#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    script = Path(__file__).resolve().parents[1] / "scripts" / "generate-sequoia-controller-abstractness-probe.py"
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        header = root / "IO80211ControllerV2.h"
        output = root / "GeneratedControllerProbe.hpp"
        header.write_text(
            "class IO80211Controller { public:\n"
            " virtual bool isCommandProhibited(int) = 0;\n"
            " virtual SInt32 handleCardSpecific(void*, unsigned long, void*, bool) = 0;\n"
            " virtual IOReturn getPOWER(void*) = 0;\n"
            " virtual CCLogStream * getLogger() const = 0;\n"
            " virtual CommonFaultReporter * getFaultReporterFromDriver() = 0;\n"
            "};\n",
            encoding="utf-8",
        )
        command = [
            sys.executable, str(script),
            "--controller-header", str(header),
            "--output", str(output),
            "--expected", "5",
        ]
        good = subprocess.run(command, capture_output=True, text=True)
        if good.returncode:
            raise SystemExit(f"generator rejected valid fixture:\n{good.stderr}")
        generated = output.read_text(encoding="utf-8")
        expected = (
            "isCommandProhibited", "handleCardSpecific", "getPOWER",
            "getLogger", "getFaultReporterFromDriver",
        )
        for method in expected:
            if method not in generated:
                raise SystemExit(f"generated controller probe missing {method}")
        for expression in ("return false;", "static_cast<SInt32>(kIOReturnUnsupported)",
                           "return kIOReturnUnsupported;", "return nullptr;"):
            if expression not in generated:
                raise SystemExit(f"missing safe stub body: {expression}")
        bad = command.copy()
        bad[bad.index("--expected") + 1] = "6"
        if subprocess.run(bad, capture_output=True).returncode == 0:
            raise SystemExit("generator accepted incorrect controller pure-virtual count")

        audited_names = (
            "isCommandProhibited", "handleCardSpecific", "getDRIVER_VERSION",
            "getHARDWARE_VERSION", "getCARD_CAPABILITIES", "getPOWER",
            "setPOWER", "getCOUNTRY_CODE", "setCOUNTRY_CODE",
            "setGET_DEBUG_INFO", "getLogger", "getFaultReporterFromDriver",
        )
        declarations = []
        for name in audited_names:
            if name == "isCommandProhibited":
                declarations.append(f"virtual bool {name}(int) = 0;")
            elif name == "handleCardSpecific":
                declarations.append(f"virtual SInt32 {name}(void*, unsigned long, void*, bool) = 0;")
            elif name in ("getLogger", "getFaultReporterFromDriver"):
                declarations.append(f"virtual void * {name}() = 0;")
            else:
                declarations.append(f"virtual IOReturn {name}(void*) = 0;")
        audited_header = root / "AuditedController.h"
        audited_header.write_text(
            "class IO80211Controller { public:\n"
            + "\n".join(declarations) + "\n};\n",
            encoding="utf-8",
        )
        audited_command = [
            sys.executable, str(script),
            "--controller-header", str(audited_header),
            "--output", str(output),
            "--expected", "12",
        ]
        if subprocess.run(audited_command, capture_output=True).returncode:
            raise SystemExit("generator rejected the audited 12-method controller contract")
        generated = output.read_text(encoding="utf-8")
        if "sizeof(IO80211Controller) == 0x128" not in generated:
            raise SystemExit("controller probe omitted the pinned base-size assertion")
        audited_original = declarations.copy()
        declarations[-1] = "virtual void * unexpectedLogger() = 0;"
        audited_header.write_text(
            "class IO80211Controller { public:\n"
            + "\n".join(declarations) + "\n};\n",
            encoding="utf-8",
        )
        if subprocess.run(audited_command, capture_output=True).returncode == 0:
            raise SystemExit("generator accepted a changed method set with the same pure-virtual count")

        # A pure-virtual reorder preserves the count and method set but changes
        # vtable slots. The audited Sequoia contract must reject that drift too.
        reordered = audited_original.copy()
        reordered[0], reordered[1] = reordered[1], reordered[0]
        audited_header.write_text(
            "class IO80211Controller { public:\n"
            + "\n".join(reordered) + "\n};\n",
            encoding="utf-8",
        )
        if subprocess.run(audited_command, capture_output=True).returncode == 0:
            raise SystemExit("generator accepted reordered controller virtual slots")
    print("Sequoia controller abstractness generator fixture tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
