"""Compare existing receiver modes to upstream; check the SRXL2-only addition."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def check():
    source = (ROOT / "src/src/rx_main.cpp").read_text()
    # ELRS uses an unindented closing brace for this top-level function.
    setup = re.search(r"^static void setupSerial\(\)\s*\{.*?^\}", source, re.M | re.S)
    baseline = subprocess.check_output(["git", "show", "upstream/master:src/src/rx_main.cpp"], cwd=ROOT, text=True)
    upstream = re.search(r"^static void setupSerial\(\)\s*\{.*?^\}", baseline, re.M | re.S)
    common = (ROOT / "src/include/common.h").read_text()
    protocol = re.search(r"enum eSerialProtocol\s*:\s*uint8_t\s*\{.*?\};", common, re.S)
    assert setup and upstream and protocol, "Receiver factory or protocol enum not found"
    support = (ROOT / "src/include/SRXL2Config.h").read_text()
    support = support.replace('#pragma once', '').replace('#include "common.h"', '')
    # Only hardware/driver boundaries are substituted; the factory is unmodified.
    boundary = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <typeinfo>
struct Stream { virtual ~Stream() = default; };
struct NullStream : Stream {};
struct Driver {
    Driver() = default;
    Driver(Stream &, Stream &) {}
    Driver(Stream &, int8_t) {}
    virtual ~Driver() = default;
};
#define MOCK_DRIVER(name) struct name : Driver { using Driver::Driver; };
MOCK_DRIVER(SerialNOOP)
MOCK_DRIVER(SerialAirPort)
MOCK_DRIVER(SerialScorpion_TLM)
MOCK_DRIVER(SerialSBUS)
MOCK_DRIVER(SerialSUMD)
MOCK_DRIVER(SerialMavlink)
MOCK_DRIVER(SerialDisplayport)
MOCK_DRIVER(SerialGPS)
MOCK_DRIVER(SerialHoTT_TLM)
MOCK_DRIVER(SerialCRSF)
#define GPIO_PIN_RCSIGNAL_TX 1
#define UNDEF_PIN -1
#define U0TXD_GPIO_NUM 1
Stream output, input;
#define SERIAL_PROTOCOL_TX output
#define SERIAL_PROTOCOL_RX input
bool noSerialPins = false;
#define OPT_CRSF_RCVR_NO_SERIAL noSerialPins
Driver *serialIO = nullptr;
Stream *BackpackOrLogStrm = nullptr;
uint32_t serialBaud = 420000;
struct Options { bool is_airport; uint32_t uart_baud = 420000; } firmwareOptions;
"""
    config = r"""
struct Config {
    eSerialProtocol protocol;
    eSerialProtocol GetSerialProtocol() { return protocol; }
} config;
"""
    probe = r"""
int main() {
    static_assert(PROTOCOL_SCORPION_TLM == 10 && PROTOCOL_SRXL2 == 11, "Persisted protocol IDs changed");
  for (bool unavailable : {false, true}) {
    noSerialPins = unavailable;
    for (bool airport : {false, true}) {
        for (unsigned protocol = 0; protocol < 16; ++protocol) {
            firmwareOptions.is_airport = airport;
            config.protocol = static_cast<eSerialProtocol>(protocol);
            serialBaud = 420000;
            setupUpstreamSerial();
            const std::type_info &expectedType = typeid(*serialIO);
            const bool expectedAirport = firmwareOptions.is_airport;
            const uint32_t expectedBaud = serialBaud;
            delete serialIO;
            delete BackpackOrLogStrm;
            firmwareOptions.is_airport = airport;
            serialBaud = 420000;
            setupSerial();
            if (protocol == PROTOCOL_SRXL2 && !airport)
                assert(dynamic_cast<SerialNOOP *>(serialIO)); // this probe models unsupported hardware
            else
            {
                assert(typeid(*serialIO) == expectedType);
                assert(firmwareOptions.is_airport == expectedAirport);
                assert(serialBaud == expectedBaud);
            }
            delete serialIO;
            delete BackpackOrLogStrm;
        }
    }
  }
    std::puts("Serial factory: 62 existing-mode cases match upstream; 2 SRXL2-only cases pass");
}
"""
    tool_dir = ROOT / "src/.pio/tools/mingw64/bin"
    local_compiler = tool_dir / "g++.exe"
    compiler = str(local_compiler) if local_compiler.exists() else shutil.which("g++")
    assert compiler, "A native C++ compiler is required"
    environment = dict(os.environ)
    environment["PATH"] = str(tool_dir) + os.pathsep + environment.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="elrs-serial-setup-") as directory:
        cpp = Path(directory) / "probe.cpp"
        executable = Path(directory) / "probe.exe"
        cpp.write_text(boundary + '\n#include <initializer_list>\n' + protocol.group()
                       + support + config + upstream.group().replace('setupSerial()', 'setupUpstreamSerial()')
                       + setup.group() + probe)
        subprocess.run([compiler, "-std=c++11", str(cpp), "-o", str(executable)],
                       env=environment, check=True)
        subprocess.run([str(executable)], env=environment, check=True)


if __name__ == "__main__":
    check()
