"""Verify an ER6 build includes startup/telemetry without diagnostic hooks."""
from pathlib import Path
import subprocess
import sys


def check(elf):
    nm = Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm.exe"
    symbols = subprocess.check_output([str(nm), "-C", "--defined-only", str(elf)], text=True)
    startup = [line for line in symbols.splitlines() if line.endswith(" initVariant")]
    assert len(startup) == 1 and startup[0].split()[1] == "T", (
        f"Production must run the early SRXL2 listener; got {startup}"
    )
    assert "SerialSRXL2::publishTelemetry(" in symbols
    for hook in ("requestSRXL2NeutralProbe(", "setSRXL2ProbeAddress(",
                 "getSRXL2LiveDiagnostics(", "initSmartEdgeCounter("):
        assert hook not in symbols, f"Diagnostic hook in production: {hook}"
    print("Production: early startup and telemetry enabled; diagnostic hooks absent")


if __name__ == "__main__":
    check(Path(sys.argv[1]))
