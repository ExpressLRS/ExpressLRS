"""Verify a receiver build includes startup/telemetry without diagnostic hooks."""
from pathlib import Path
import subprocess
import sys


def check(elf, port=0, nm=None):
    nm = nm or Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm.exe"
    symbols = subprocess.check_output([str(nm), "-C", "--defined-only", str(elf)], text=True)
    startup = [line for line in symbols.splitlines() if line.endswith(" initVariant")]
    assert len(startup) == 1 and startup[0].split()[1] == "T", (
        f"Production must run the early SRXL2 listener; got {startup}"
    )
    assert "SerialSRXL2::publishTelemetry(" in symbols
    early_receive = '__esp_system_init_fn_srxl2EarlyReceive()' in symbols
    assert early_receive == (port == 1), f'Unexpected early UART1 preparation for port {port}'
    for hook in ("requestSRXL2NeutralProbe(", "setSRXL2ProbeAddress(",
                 "getSRXL2LiveDiagnostics(", "initSmartEdgeCounter("):
        assert hook not in symbols, f"Diagnostic hook in production: {hook}"
    print(f"Production port {port}: early startup and telemetry enabled; diagnostic hooks absent")


if __name__ == "__main__":
    check(Path(sys.argv[1]), int(sys.argv[2]) if len(sys.argv) > 2 else 0,
          Path(sys.argv[3]) if len(sys.argv) > 3 else None)
