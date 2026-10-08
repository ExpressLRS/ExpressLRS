"""Exercise SRXL2 capability and receive-busy checks against ESP-IDF registers."""
import os
from itertools import count
from pathlib import Path
import shutil
import subprocess
import tempfile
import re

ROOT = Path(__file__).resolve().parents[1]


def check_early_teardown(compiler, environment, directory):
    source = (ROOT / "src/src/rx-serial/SerialSRXL2.cpp").read_text()
    prepare = re.search(r'ESP_SYSTEM_INIT_FN\(srxl2EarlyReceive, BIT0\)\s*\{.*?^\}', source, re.M | re.S).group()
    variant = re.search(r'extern "C" void initVariant\(\)\s*\{.*?^\}', source, re.M | re.S).group()
    boundary = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define ESP_SYSTEM_INIT_FN(name, cores) void earlyReceive()
#define BIT0 1
enum { PERIPH_UART0_MODULE, PERIPH_UART1_MODULE, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY,
       INPUT_PULLUP, UART_SCLK_APB, UART_DATA_8_BITS, UART_PARITY_DISABLE,
       UART_STOP_BITS_1, UART_HW_FLOWCTRL_DISABLE, SERIAL_8N1, U0RXD_IN_IDX, U1RXD_IN_IDX };
const uint32_t UART_LL_INTR_MASK = 0xFFFFFFFF;
using gpio_num_t = int;
unsigned clocks[2] = {}, enables[2] = {}, disables[2] = {}, signalMode = 0;
bool captured = false, listened = false, ended = false;
void periph_module_enable(int module) { ++clocks[module]; ++enables[module]; }
void periph_module_disable(int module) { assert(clocks[module] > 0); --clocks[module]; ++disables[module]; }
void periph_module_reset(int module) { assert(clocks[module] > 0 && !captured); }
unsigned micros() { return 50000; }
void gpio_set_direction(int pin, int mode) { assert(pin == SRXL2_EARLY_STARTUP_PIN); signalMode = mode; }
void gpio_set_pull_mode(int pin, int) { assert(pin == SRXL2_EARLY_STARTUP_PIN); }
void pinMode(int pin, int mode) { assert(pin == SRXL2_EARLY_STARTUP_PIN); signalMode = mode; }
void pinMatrixInAttach(int pin, int signal, bool) {
    assert(pin == SRXL2_EARLY_STARTUP_PIN);
    assert(signal == (SRXL2_EARLY_STARTUP_PORT == 0 ? U0RXD_IN_IDX : U1RXD_IN_IDX));
}
int earlyHardware;
#define UART_LL_GET_HW(port) (&earlyHardware)
#define UART_STUB(name) template<typename... Args> void name(Args...) {}
UART_STUB(uart_ll_disable_intr_mask) UART_STUB(uart_ll_clr_intsts_mask)
UART_STUB(uart_ll_set_sclk) UART_STUB(uart_ll_set_baudrate) UART_STUB(uart_ll_set_data_bit_num)
UART_STUB(uart_ll_set_parity) UART_STUB(uart_ll_set_stop_bits) UART_STUB(uart_ll_set_hw_flow_ctrl)
const uint8_t fifo[] = {0xA6,0x21,14,0x40,0,10,0,0,0,0,0,1,0x38,0x4E};
unsigned uart_ll_get_rxfifo_len(int *) { assert(clocks[1] == 1); return sizeof(fifo); }
void uart_ll_read_rxfifo(int *, uint8_t *bytes, unsigned size) {
    assert(size == sizeof(fifo) && clocks[1] == 1); memcpy(bytes, fifo, size); captured = true;
}
struct Uart {
    unsigned port;
    void setTxBufferSize(unsigned size) { assert(size == 0); }
    void setRxFIFOFull(unsigned threshold) { assert(threshold == 1); }
    void begin(unsigned baud, int, int rx, int tx, bool inverted) {
        assert(baud == 115200 && rx == SRXL2_EARLY_STARTUP_PIN && tx == -1 && !inverted);
        assert(port == SRXL2_EARLY_STARTUP_PORT);
        if (port == 1) assert(captured && clocks[1] == 1);
        periph_module_enable(port); // Arduino acquires a separate clock reference.
    }
    void end() {
        assert(listened); ended = true;
        periph_module_disable(port); signalMode = 0; // Arduino's teardown detaches the pin.
    }
} Serial{0}, Serial1{1};
struct { uint64_t getEfuseMac() { return 0x12345678; } } ESP;
struct SRXL2StartupState { uint32_t enteredUs = 0; } startupState;
void listenForEarlyESC(Uart &, Uart &, uint32_t, SRXL2StartupState &, uint8_t port, int8_t pin,
                      const uint8_t *prefix, unsigned size) {
    assert(port == SRXL2_EARLY_STARTUP_PORT && pin == SRXL2_EARLY_STARTUP_PIN && !ended);
    if (port == 1) assert(clocks[1] == 2 && size == sizeof(fifo) && !memcmp(prefix, fifo, size));
    else assert(clocks[0] == 1 && size == 0);
    listened = true;
}
'''
    probe = r'''
int main() {
#if SRXL2_EARLY_STARTUP_PORT == 1
    earlyReceive();
#endif
    initVariant();
    assert(ended && signalMode == INPUT_PULLUP);
    assert(clocks[0] == 0 && clocks[1] == 0);
    assert(enables[SRXL2_EARLY_STARTUP_PORT] == (SRXL2_EARLY_STARTUP_PORT == 1 ? 2u : 1u));
    assert(disables[SRXL2_EARLY_STARTUP_PORT] == enables[SRXL2_EARLY_STARTUP_PORT]);
    std::puts("Early teardown: FIFO/listener precede end; signal pull-up and clock references balanced");
}
'''
    cpp = Path(directory) / 'early.cpp'
    executable = Path(directory) / 'early.exe'
    for port in (0, 1):
        cpp.write_text(boundary + (prepare if port == 1 else '') + variant + probe)
        subprocess.run([compiler, '-std=c++11', f'-DSRXL2_EARLY_STARTUP_PORT={port}',
                        f'-DSRXL2_EARLY_STARTUP_PIN={3 if port == 0 else 14}', str(cpp), '-o', str(executable)],
                       env=environment, check=True)
        subprocess.run([str(executable)], env=environment, check=True)


def check():
    common = (ROOT / "src/include/common.h").read_text()
    protocols = '\n'.join(re.findall(r'enum (?:eSerial(?:1)?Protocol|eServoOutputMode)\s*:\s*uint8_t\s*\{.*?\};', common, re.S))
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
int rxPin = 3, secondaryTx = 14, secondaryRx = -1;
bool noSerialPins = false;
#define GPIO_PIN_RCSIGNAL_TX txPin
#define GPIO_PIN_RCSIGNAL_RX rxPin
#define GPIO_PIN_SERIAL1_TX secondaryTx
#define GPIO_PIN_SERIAL1_RX secondaryRx
#define U0TXD_GPIO_NUM 1
#define U0RXD_GPIO_NUM 3
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define RX1 15
#define TX1 16
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define RX1 18
#define TX1 19
#else
#define RX1 9
#define TX1 10
#endif
int16_t pwmPins[] = {1, 3, 14, 15};
uint8_t pwmModes[] = {0, 0, 0, 0};
#define GPIO_PIN_PWM_OUTPUTS pwmPins
#define GPIO_PIN_PWM_OUTPUTS_COUNT 4
#define OPT_HAS_SERVO_OUTPUT true
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
    assert(isValidSerialProtocolPair(0, 0));
    assert(isSRXL2Selected(0, 11, 0, false));
    assert(!isSRXL2Selected(0, 11, 0, true));
    assert(!isSRXL2Selected(0, 0, 13, false));
#if defined(PLATFORM_ESP32)
    assert(isSRXL2Selected(1, 0, 13, false));
    assert(isSRXL2Selected(1, 11, 13, true));
#endif
    assert(!isSRXL2Selected(2, 11, 13, false));
    for (int pin : {-1, 0, 1, 21, 34, 43, 63, 64}) {
        txPin = pin;
        for (bool unavailable : {false, true}) {
            noSerialPins = unavailable;
            const bool valid = pin >= 0 && pin < 64 && ((uint64_t(SOC_GPIO_VALID_OUTPUT_GPIO_MASK) >> pin) & 1);
            assert(supportsSRXL2() == (EXPECTED_SUPPORT && valid && !unavailable));
            assert(supportsSRXL2(0, pin) == supportsSRXL2());
            assert(supportsSRXL2(1, pin) == (EXPECTED_SUPPORT && valid));
            assert(!supportsSRXL2(2, pin));
            assert(supportsSRXL2(0, pin, true) == (EXPECTED_SUPPORT && valid));
        }
    }
#if defined(TARGET_RX)
    txPin = rxPin = -1;
    noSerialPins = true;
    auto modeAt = [](uint8_t ch) { return pwmModes[ch]; };
    assert(!primarySerialEnabled(modeAt));
    pwmModes[0] = pwmModes[1] = somSerial;
    assert(primarySerialEnabled(modeAt)); // Proposed mapping, even while running serial is off.
    int8_t rx, tx;
    resolvePrimarySerialPins(rx, tx, primarySerialEnabled(modeAt));
    assert(tx == 1 && rx == 3);
    noSerialPins = false;
    assert(supportsSRXL2() == bool(EXPECTED_SUPPORT));
    rxPin = 3;
    assert(!supportsSRXL2()); // A fixed RX alone never infers Smart's TX.
    assert(!isValidSRXL2Config(11, 0, false, modeAt));
    txPin = 1;
    assert(isValidSRXL2Config(11, 0, false, modeAt) == bool(EXPECTED_SUPPORT));
    pwmModes[0] = som50Hz;
    assert(!isValidSRXL2Config(11, 0, true, modeAt)); // Validate even when AirPort masks it.
#if defined(PLATFORM_ESP32)
    txPin = rxPin = -1;
    pwmModes[1] = som50Hz;
    pwmModes[2] = somSerial1TX;
    assert(isValidSRXL2Config(0, 13, false, modeAt) == bool(EXPECTED_SUPPORT));
    assert(isValidSRXL2Config(0, 13, true, modeAt) == bool(EXPECTED_SUPPORT));
    assert(!isValidSRXL2Config(11, 13, true, modeAt));
    // Fixed fields win independently over the stock last-matching-PWM fallback.
    pwmModes[3] = somSerial1TX;
    resolveSerial1Pins(rx, tx, modeAt);
    assert(tx == 14 && rx == -1);
    secondaryTx = -1;
    resolveSerial1Pins(rx, tx, modeAt);
    assert(tx == 15);
    pwmModes[2] = pwmModes[3] = somSerial1RX;
    resolveSerial1Pins(rx, tx, modeAt);
    assert(tx == -1 && rx == 15);
    assert(!isValidSRXL2Config(0, 13, false, modeAt));
    secondaryTx = 14;
    pwmModes[2] = somSerial1TX;
    txPin = 14;
    assert(!isValidSRXL2Config(0, 13, false, modeAt)); // Other active primary owns TX.
    txPin = 1;
    rxPin = 14;
    assert(!isValidSRXL2Config(0, 13, true, modeAt)); // AirPort still owns primary RX.
    txPin = rxPin = -1;
    secondaryTx = 34;
    assert(isValidSRXL2Config(0, 13, false, modeAt) ==
        bool(EXPECTED_SUPPORT && ((uint64_t(SOC_GPIO_VALID_OUTPUT_GPIO_MASK) >> 34) & 1)));
    txPin = TX1; rxPin = 3;
    secondaryTx = secondaryRx = -1;
    for (auto &mode : pwmModes) mode = som50Hz;
    assert(!isValidSRXL2Config(11, 3, false, modeAt)); // TX-only begin(-1,-1) infers UART1 defaults.
    assert(!isValidSRXL2Config(11, 1, false, modeAt)); // Bidirectional begin(-1,-1) does too.
    assert(isValidSRXL2Config(11, 11, false, modeAt) == bool(EXPECTED_SUPPORT)); // RX-less GPS never begins.
    secondaryRx = 5;
    assert(!isValidSRXL2Config(11, 3, false, modeAt)); // TX-only factory discards even a fixed RX.
    assert(isValidSRXL2Config(11, 1, false, modeAt) == bool(EXPECTED_SUPPORT)); // One explicit pin prevents inference.
#endif
#endif
}
"""
    hardware_probe = r"""
#include "soc/uart_struct.h"
uart_dev_t uarts[2]{};
static uart_dev_t *getUart(int index) { assert(index == 0 || index == 1); return &uarts[index]; }
#define UART_LL_GET_HW(index) getUart(index)
static unsigned uart_ll_get_rxfifo_len(uart_dev_t *hw) { return hw->status.rxfifo_cnt; }
using gpio_num_t = int;
int busLevel = 1;
static int gpio_get_level(gpio_num_t pin) { assert(pin == txPin); return busLevel; }
"""
    busy_probe = r"""
int main() {
    const int pin = txPin = BUS_PIN;
  for (unsigned port : {0u, 1u}) {
    auto &uart = uarts[port];
    auto &other = uarts[1 - port];
    other.status.rxfifo_cnt = 1; // Traffic on the other UART must not block this bus.
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
            print(f"{mcu}: both-port capability and 40 FIFO/state/bus-level combinations passed", flush=True)
        for flags in (
            ["-DTARGET_RX", "-DPLATFORM_ESP32", "-DCONFIG_IDF_TARGET_ESP32C6"],
            ["-DTARGET_RX", "-DPLATFORM_ESP8266"],
            ["-DPLATFORM_ESP32", "-DCONFIG_IDF_TARGET_ESP32"],
        ):
            run('#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (~0ULL)\n' + support + capability_probe, [*flags, "-DEXPECTED_SUPPORT=0"])
        run(busy + "int main() { assert(SRXL2_HARDWARE_RX_BUSY()); }",
            ["-DPLATFORM_ESP32", "-DCONFIG_IDF_TARGET_ESP32C6"])
        check_early_teardown(compiler, environment, directory)
    print("Unsupported SoCs, transmitters, and missing serial pins remain disabled")


if __name__ == "__main__":
    check()
