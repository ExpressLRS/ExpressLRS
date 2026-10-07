"""Exercise SRXL2 capability and receive-busy checks against ESP-IDF registers."""
import os
from itertools import count
from pathlib import Path
import shutil
import subprocess
import tempfile
import re

ROOT = Path(__file__).resolve().parents[1]


def check():
    common = (ROOT / "src/include/common.h").read_text()
    protocols = '\n'.join(re.findall(r'enum eSerial(?:1)?Protocol\s*:\s*uint8_t\s*\{.*?\};', common, re.S))
    secondary = re.search(r'enum eSerial1Protocol\s*:\s*uint8_t\s*\{(.*?)\};', common, re.S)
    names = re.findall(r'PROTOCOL_SERIAL1_\w+', secondary[1])
    assert len(names) == 14 and names[12:] == ['PROTOCOL_SERIAL1_SCORPION_TLM', 'PROTOCOL_SERIAL1_SRXL2'], 'Append secondary Smart at ID 13 without renumbering legacy IDs'
    support = (ROOT / "src/include/SRXL2Config.h").read_text()
    support = support.replace("#pragma once", "").replace('#include "common.h"', "")
    support = support.replace('#include "soc/soc_caps.h"', '')
    source = (ROOT / "src/src/rx-serial/SerialSRXL2.cpp").read_text()
    busy = source[source.index("#ifndef SRXL2_HARDWARE_RX_BUSY"):
                  source.index("#ifndef SRXL2_MODE_ACTIVE")]
    sdk = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    sdk /= "packages/framework-arduinoespressif32/tools/sdk"
    tool_dir = ROOT / "src/.pio/tools/mingw64/bin"
    compiler = str(tool_dir / "g++.exe") if (tool_dir / "g++.exe").exists() else shutil.which("g++")
    assert compiler, "A native C++ compiler is required"
    environment = dict(os.environ)
    environment["PATH"] = str(tool_dir) + os.pathsep + environment.get("PATH", "")
    boundary = r"""
#include <cassert>
#include <cstdint>
#include <initializer_list>
int txPin = 1;
bool noSerialPins = false;
#define GPIO_PIN_RCSIGNAL_TX txPin
#define OPT_CRSF_RCVR_NO_SERIAL noSerialPins
#define UNDEF_PIN -1
"""
    capability_probe = r"""
int main() {
    static_assert(PROTOCOL_SRXL2 == 11 && PROTOCOL_SERIAL1_SCORPION_TLM == 12 && PROTOCOL_SERIAL1_SRXL2 == 13, "Protocol IDs changed");
    assert(isValidSerialProtocolPair(11, 0));
#if defined(PLATFORM_ESP32)
    assert(isValidSerialProtocolPair(0, 13));
#else
    assert(!isValidSerialProtocolPair(0, 13));
#endif
    assert(!isValidSerialProtocolPair(11, 13));
    assert(!isValidSerialProtocolPair(12, 0));
    assert(!isValidSerialProtocolPair(0, 14));
    for (int pin : {-1, 0, 1, 21, 34, 43, 63, 64}) {
        txPin = pin;
        for (bool unavailable : {false, true}) {
            noSerialPins = unavailable;
            const bool valid = pin >= 0 && pin < 64 && ((SOC_GPIO_VALID_OUTPUT_GPIO_MASK >> pin) & 1);
            assert(supportsSRXL2() == (EXPECTED_SUPPORT && valid && !unavailable));
            assert(supportsSRXL2(0, pin) == supportsSRXL2());
            assert(supportsSRXL2(1, pin) == (EXPECTED_SUPPORT && valid));
            assert(!supportsSRXL2(2, pin));
        }
    }
}
"""
    hardware_probe = r"""
#include "soc/uart_struct.h"
uart_dev_t uart{};
static uart_dev_t *getUart(int index) { assert(index == 0); return &uart; }
#define UART_LL_GET_HW(index) getUart(index)
static unsigned uart_ll_get_rxfifo_len(uart_dev_t *hw) { return hw->status.rxfifo_cnt; }
using gpio_num_t = int;
int busLevel = 1;
static int gpio_get_level(gpio_num_t pin) { assert(pin == txPin); return busLevel; }
"""
    busy_probe = r"""
int main() {
    const int pin = txPin = BUS_PIN;
    for (unsigned fifo : {0u, 1u}) {
        uart.status.rxfifo_cnt = fifo;
        for (unsigned state : {0u, 1u, 2u, 11u, 13u}) {
#if defined(CONFIG_IDF_TARGET_ESP32)
            uart.status.st_urx_out = state;
#else
            uart.fsm_status.st_urx_out = state;
#endif
            for (int level : {0, 1}) {
                busLevel = level;
                assert(SRXL2_HARDWARE_RX_BUSY() == (fifo != 0 || state != 0 || level == 0));
            }
        }
    }
}
"""
    with tempfile.TemporaryDirectory(prefix="elrs-srxl2-platforms-") as directory:
        cpp = Path(directory) / "probe.cpp"
        probe_numbers = count()

        def run(code, flags, includes=()):
            executable = Path(directory) / f"probe-{next(probe_numbers)}.exe"
            cpp.write_text(boundary + protocols + '\n' + code)
            subprocess.run([compiler, "-std=c++11", *flags,
                            *("-I" + str(path) for path in includes), str(cpp), "-o", str(executable)],
                           env=environment, check=True)
            subprocess.run([str(executable)], env=environment, check=True)

        platforms = (
            ("esp32", (), 1),
            ("esp32c3", ("-DPLATFORM_ESP32_C3",), 21),
            ("esp32s3", ("-DPLATFORM_ESP32_S3",), 43),
        )
        for mcu, extra, pin in platforms:
            flags = ["-DTARGET_RX", "-DPLATFORM_ESP32", "-DCONFIG_IDF_TARGET_" + mcu.upper(), *extra]
            caps = '#include "soc/soc_caps.h"\n' + '\n'.join(f'#define BIT{i} (1ULL << {i})' for i in range(64)) + '\n'
            run(caps + support + capability_probe, [*flags, "-DEXPECTED_SUPPORT=1"],
                [sdk / mcu / "include/soc" / mcu / "include"])
            run(hardware_probe + busy + busy_probe, [*flags, "-DBUS_PIN=" + str(pin)],
                [sdk / mcu / "include/soc" / mcu / "include"])
            print(f"{mcu}: capability and 20 FIFO/state/bus-level combinations passed", flush=True)
        for flags in (
            ["-DTARGET_RX", "-DPLATFORM_ESP32", "-DCONFIG_IDF_TARGET_ESP32C6"],
            ["-DTARGET_RX", "-DPLATFORM_ESP8266"],
            ["-DPLATFORM_ESP32", "-DCONFIG_IDF_TARGET_ESP32"],
        ):
            run('#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (~0ULL)\n' + support + capability_probe, [*flags, "-DEXPECTED_SUPPORT=0"])
        run(busy + "int main() { assert(SRXL2_HARDWARE_RX_BUSY()); }",
            ["-DPLATFORM_ESP32", "-DCONFIG_IDF_TARGET_ESP32C6"])
    print("Unsupported SoCs, transmitters, and missing serial pins remain disabled")


if __name__ == "__main__":
    check()
