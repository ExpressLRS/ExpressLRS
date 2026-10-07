"""Compare existing receiver modes to upstream; check the SRXL2-only addition."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def check_selection(compiler, environment, directory):
    source = (ROOT / 'src/src/rx_main.cpp').read_text()
    helpers = re.search(r'^static int8_t getSerial1Pin\(.*?^#endif', source, re.M | re.S)
    assert helpers, 'Secondary pin/Smart selection helpers missing'
    common = (ROOT / 'src/include/common.h').read_text()
    enums = '\n'.join(re.findall(r'enum e(?:Serial(?:1)?Protocol|ServoOutputMode)\s*:\s*uint8_t\s*\{.*?\};', common, re.S))
    support = (ROOT / 'src/include/SRXL2Config.h').read_text().replace('#pragma once', '').replace('#include "common.h"', '').replace('#include "soc/soc_caps.h"', '')
    boundary = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#define TARGET_RX
#define PLATFORM_ESP32
#define CONFIG_IDF_TARGET_ESP32
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (0xFFFFFFFFULL & ~(1ULL << 6))
#define UNDEF_PIN -1
int8_t primaryTx = 1, primaryRx = 3, secondaryTx = -1, secondaryRx = -1;
const int8_t pins[] = {14, 15};
bool noSerialPins = false;
#define GPIO_PIN_RCSIGNAL_TX primaryTx
#define GPIO_PIN_RCSIGNAL_RX primaryRx
#define GPIO_PIN_SERIAL1_TX secondaryTx
#define GPIO_PIN_SERIAL1_RX secondaryRx
#define GPIO_PIN_PWM_OUTPUTS pins
#define GPIO_PIN_PWM_OUTPUTS_COUNT 2
#define OPT_CRSF_RCVR_NO_SERIAL noSerialPins
struct Options { bool is_airport = false; } firmwareOptions;
'''
    config = r'''
struct Pwm { struct { eServoOutputMode mode = som50Hz; } val; };
struct Config {
    eSerialProtocol primary = PROTOCOL_CRSF;
    eSerial1Protocol secondary = PROTOCOL_SERIAL1_SRXL2;
    Pwm pwm[2];
    eSerialProtocol GetSerialProtocol() const { return primary; }
    eSerial1Protocol GetSerial1Protocol() const { return secondary; }
    Pwm *GetPwmChannel(unsigned index) { return &pwm[index]; }
} config;
'''
    probe = r'''
int main() {
    assert(getSerial1TxPin() == -1 && getSRXL2Port() == -1);
    config.pwm[0].val.mode = somSerial1RX;
    assert(getSerial1RxPin() == 14 && getSRXL2Port() == -1);
    config.pwm[0].val.mode = somSerial1TX;
    assert(getSerial1TxPin() == 14 && getSRXL2Port() == 1);
    secondaryTx = 15;
    assert(getSerial1TxPin() == 15 && getSRXL2Port() == 1);
    secondaryTx = primaryRx;
    assert(getSRXL2Port() == -1);
    secondaryTx = 6; // output-disabled GPIO boundary
    assert(getSRXL2Port() == -1);
    secondaryTx = 14;
    noSerialPins = true;
    assert(getSRXL2Port() == 1);
    noSerialPins = false;
    firmwareOptions.is_airport = true;
    assert(getSRXL2Port() == 1);
    config.primary = PROTOCOL_SRXL2;
    assert(getSRXL2Port() == 1); // AirPort keeps its existing primary precedence.
    firmwareOptions.is_airport = false;
    assert(getSRXL2Port() == 0); // Corrupt both-Smart settings still choose one bus.
    config.secondary = PROTOCOL_SERIAL1_CRSF;
    secondaryRx = primaryTx;
    assert(getSRXL2Port() == -1);
    secondaryRx = -1;
    assert(getSRXL2Port() == 0);
    std::puts("Secondary pin resolution, coexistence and one-Smart-bus selection passed");
}
'''
    cpp = Path(directory) / 'selection.cpp'
    executable = Path(directory) / 'selection.exe'
    cpp.write_text(boundary + enums + '\n' + support + config + helpers[0].removesuffix('#endif') + probe)
    subprocess.run([compiler, '-std=c++11', str(cpp), '-o', str(executable)], env=environment, check=True)
    subprocess.run([str(executable)], env=environment, check=True)


def check_teamrace(compiler, environment, directory):
    directory = Path(directory)
    mock_config = directory / "teamrace-config.h"
    mock_config.write_text("""#pragma once
#include <cstdint>
struct Config {
    uint8_t position = 1, channel = 10;
    uint8_t GetTeamracePosition() const { return position; }
    uint8_t GetTeamraceChannel() const { return channel; }
};
extern Config config;
""")
    source = directory / "devSerialIO.cpp"
    source.write_text((ROOT / "src/src/rx-serial/devSerialIO.cpp").read_text().replace(
        '#include "config.h"', '#include "teamrace-config.h"'))
    probe = directory / "teamrace.cpp"
    probe.write_text(r"""
#include <cassert>
#include <cstdio>
#include "targets.h"
#include "common.h"
#include "SerialIO.h"
#include "crsf_protocol.h"
#include "devSerialIO.h"
#include "teamrace-config.h"
Config config;
connectionState_e connectionState = connected;
bool connectionHasModelMatch = true, teamraceHasModelMatch = true;
uint32_t ChannelData[CRSF_NUM_CHANNELS] = {};
class Probe : public SerialIO {
public:
    Probe() : SerialIO(nullptr, nullptr) {}
    uint32_t sendRCFrame(bool available, bool, uint32_t *) override {
        passed = available;
        return 1;
    }
    bool passed = false;
    void processBytes(uint8_t *, uint16_t) override {}
};
void SerialIO::setFailsafe(bool value) { failsafe = value; }
void SerialIO::processSerialInput() {}
void SerialIO::sendQueuedData(uint32_t) {}
Probe probe;
SerialIO *serialIO = &probe;
static void frame() { crsfRCFrameAvailable(); Serial0_device.timeout(); }
int main() {
    Serial0_device.start();
    ChannelData[config.channel] = 1811; // Wrong team-race switch position.
    frame(); frame(); frame();
    assert(!teamraceHasModelMatch && !probe.passed);
    config.position = 0; // Lua can disable team race without rebooting.
    connectionState = disconnected; Serial0_device.event();
    connectionState = connected; Serial0_device.event();
    connectionHasModelMatch = false;
    frame();
    assert(!probe.passed && !teamraceHasModelMatch); // Model mismatch still rejects the frame.
    connectionHasModelMatch = true;
    ChannelData[2] = 992;
    for (unsigned i = 0; i < 10; ++i) frame();
    assert(probe.passed && teamraceHasModelMatch); // SRXL2's permission gate can recover.
    std::puts("Team-race Off: live inhibition clears after reconnect; model mismatch remains blocked");
}
""")
    executable = directory / "teamrace.exe"
    includes = ["-I" + str(ROOT / "src/include"), "-I" + str(ROOT / "src/src/rx-serial")]
    includes += ["-I" + str(path) for path in (ROOT / "src/lib").iterdir() if path.is_dir()]
    subprocess.run([compiler, "-std=gnu++17", "-mno-ms-bitfields", "-DTARGET_NATIVE", "-DUNIT_TEST",
                    "-DTARGET_RX", "-DRegulatory_Domain_ISM_2400", *includes,
                    str(source), str(probe), "-o", str(executable)], env=environment, check=True)
    subprocess.run([str(executable)], env=environment, check=True)


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
        check_teamrace(compiler, environment, directory)
        check_selection(compiler, environment, directory)


if __name__ == "__main__":
    check()
