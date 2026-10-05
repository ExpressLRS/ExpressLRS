"""Compile the actual receiver factory and check its driver/mode invariant."""
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
    common = (ROOT / "src/include/common.h").read_text()
    protocol = re.search(r"enum eSerialProtocol\s*:\s*uint8_t\s*\{.*?\};", common, re.S)
    assert setup and protocol, "Receiver factory or protocol enum not found"
    support = (ROOT / "src/include/SRXL2Config.h").read_text()
    support = support.replace('#pragma once', '').replace('#include "common.h"', '')
    # Only hardware/driver boundaries are substituted; the factory is unmodified.
    boundary = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
struct Stream { virtual ~Stream() = default; };
struct NullStream : Stream {};
struct Driver {
    Driver() = default;
    Driver(Stream &, Stream &) {}
    Driver(Stream &, int8_t) {}
    virtual ~Driver() = default;
};
struct SerialNOOP : Driver { using Driver::Driver; };
struct SerialAirPort : Driver { using Driver::Driver; };
struct SerialScorpion_TLM : Driver { using Driver::Driver; };
using SerialSBUS = Driver;
using SerialSUMD = Driver;
using SerialMavlink = Driver;
using SerialDisplayport = Driver;
using SerialGPS = Driver;
using SerialHoTT_TLM = Driver;
using SerialCRSF = Driver;
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
            setupSerial();
            assert(firmwareOptions.is_airport == (airport && !unavailable));
            if (unavailable) assert(dynamic_cast<SerialNOOP *>(serialIO));
            else if (airport) assert(dynamic_cast<SerialAirPort *>(serialIO));
            else if (protocol >= PROTOCOL_SRXL2) assert(dynamic_cast<SerialNOOP *>(serialIO));
            else if (protocol == PROTOCOL_SCORPION_TLM) assert(dynamic_cast<SerialScorpion_TLM *>(serialIO));
            else assert(!dynamic_cast<SerialNOOP *>(serialIO));
            delete serialIO;
            delete BackpackOrLogStrm;
        }
    }
  }
    std::puts("Serial factory: all 64 protocol/AirPort/pin-availability combinations passed");
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
                       + support + config + setup.group() + probe)
        subprocess.run([compiler, "-std=c++11", str(cpp), "-o", str(executable)],
                       env=environment, check=True)
        subprocess.run([str(executable)], env=environment, check=True)


if __name__ == "__main__":
    check()
