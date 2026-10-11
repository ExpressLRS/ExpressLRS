"""Compare existing receiver modes to upstream; check the SRXL2-only addition."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STOCK = "15c78990e7eac43c110fd43cd9823ec70fa8220d"


def check_dual_factories(compiler, environment, directory):
    source = (ROOT / "src/src/rx_main.cpp").read_text()
    stock = subprocess.check_output(["git", "show", f"{STOCK}:src/src/rx_main.cpp"], cwd=ROOT, text=True)
    common = (ROOT / "src/include/common.h").read_text()
    enums = '\n'.join(re.findall(r'enum (?:eSerial(?:1)?Protocol|eServoOutputMode)\s*:\s*uint8_t\s*\{.*?\};', common, re.S))
    support = (ROOT / "src/include/SRXL2Config.h").read_text()
    support = support.replace('#pragma once', '').replace('#include "common.h"', '').replace('#include "soc/soc_caps.h"', '')
    boundary = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <typeinfo>
#include <initializer_list>
#define TARGET_RX
#define PLATFORM_ESP32
#define CONFIG_IDF_TARGET_ESP32
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK 0xFFFFFFFFULL
#define UNDEF_PIN -1
#define U0TXD_GPIO_NUM 1
#define U0RXD_GPIO_NUM 3
#define RX1 9
#define TX1 10
int8_t primaryRx = 3, primaryTx = 1, secondaryRx = -1, secondaryTx = 14;
int16_t pwmPins[] = {1, 3, 14, 15};
unsigned pinCount = 0;
bool noSerialPins = false;
#define GPIO_PIN_RCSIGNAL_RX primaryRx
#define GPIO_PIN_RCSIGNAL_TX primaryTx
#define GPIO_PIN_SERIAL1_RX secondaryRx
#define GPIO_PIN_SERIAL1_TX secondaryTx
#define GPIO_PIN_PWM_OUTPUTS pwmPins
#define GPIO_PIN_PWM_OUTPUTS_COUNT pinCount
#define OPT_HAS_SERVO_OUTPUT (pinCount != 0)
#define OPT_CRSF_RCVR_NO_SERIAL noSerialPins
#define SERIAL_8N1 1
#define SERIAL_8E2 2
#define SERIAL_8N2 3
struct Stream { virtual ~Stream() = default; };
struct NullStream : Stream {};
struct Uart {
    uint8_t port = 0;
    unsigned begins = 0, ends = 0, baud = 0, format = 0;
    int8_t rx = -1, tx = -1;
    bool inverted = false;
    void begin(unsigned b, unsigned f, int8_t r, int8_t t, bool i) {
        if (r < 0 && t < 0) { r = port == 1 ? RX1 : U0RXD_GPIO_NUM; t = port == 1 ? TX1 : U0TXD_GPIO_NUM; }
        ++begins; baud = b; format = f; rx = r; tx = t; inverted = i;
    }
    void end() { ++ends; }
} Serial, Serial1;
struct Driver {
    Driver() = default;
    Driver(Stream &, Stream &) {}
    Driver(Stream &, int8_t) {}
    Driver(Stream &, Stream &, int8_t) {}
    virtual ~Driver() = default;
};
#define MOCK_DRIVER(name) struct name : Driver { using Driver::Driver; };
MOCK_DRIVER(SerialNOOP) MOCK_DRIVER(SerialAirPort) MOCK_DRIVER(SerialScorpion_TLM)
MOCK_DRIVER(SerialSBUS) MOCK_DRIVER(SerialSUMD) MOCK_DRIVER(SerialMavlink)
MOCK_DRIVER(SerialDisplayport) MOCK_DRIVER(SerialGPS) MOCK_DRIVER(SerialHoTT_TLM)
MOCK_DRIVER(SerialCRSF) MOCK_DRIVER(SerialTramp) MOCK_DRIVER(SerialSmartAudio)
Stream output, input, output1, input1;
#define SERIAL_PROTOCOL_TX output
#define SERIAL_PROTOCOL_RX input
#define SERIAL1_PROTOCOL_TX output1
#define SERIAL1_PROTOCOL_RX input1
struct SerialSRXL2 : Driver {
    Stream *out, *in;
    int8_t pin;
    uint8_t port;
    SerialSRXL2(Stream *o, Stream *i, int8_t p, uint8_t n = 0) : out(o), in(i), pin(p), port(n) {}
};
Driver *serialIO = nullptr, *serial1IO = nullptr;
Stream *BackpackOrLogStrm = nullptr;
uint32_t serialBaud = 420000;
struct { bool is_airport = false; uint32_t uart_baud = 420000; } firmwareOptions;
"""
    config = r"""
struct Pwm { struct { uint8_t mode = 0; } val; } pwm[4];
struct {
    eSerialProtocol primary = PROTOCOL_CRSF;
    eSerial1Protocol secondary = PROTOCOL_SERIAL1_OFF;
    eSerialProtocol GetSerialProtocol() const { return primary; }
    eSerial1Protocol GetSerial1Protocol() const { return secondary; }
    Pwm *GetPwmChannel(uint8_t ch) { return &pwm[ch]; }
} config;
"""
    probe = r"""
static void clear() {
    delete serialIO; serialIO = nullptr;
    delete serial1IO; serial1IO = nullptr;
    delete BackpackOrLogStrm; BackpackOrLogStrm = nullptr;
    Serial = Uart(); Serial1 = Uart(); Serial1.port = 1;
}
int main() {
    config.secondary = PROTOCOL_SERIAL1_SRXL2;
    setupSerial(); setupSerial1();
    auto smart = dynamic_cast<SerialSRXL2 *>(serial1IO);
    assert(dynamic_cast<SerialCRSF *>(serialIO) && smart);
    assert(smart->port == 1 && smart->pin == 14 && smart->out == &output1 && smart->in == &input1);
    assert(Serial1.begins == 0); // The Smart constructor alone initializes its shared wire.
    clear();
    config.primary = PROTOCOL_SRXL2; config.secondary = PROTOCOL_SERIAL1_CRSF;
    setupSerial(); setupSerial1();
    smart = dynamic_cast<SerialSRXL2 *>(serialIO);
    assert(smart && smart->port == 0 && smart->pin == 1 && Serial.begins == 0);
    assert(dynamic_cast<SerialCRSF *>(serial1IO) && Serial1.begins == 1);
    clear();
    config.secondary = PROTOCOL_SERIAL1_SRXL2;
    setupSerial(); setupSerial1();
    assert(dynamic_cast<SerialSRXL2 *>(serialIO) && !serial1IO); // Boot recovery favors eligible primary.
    clear();
    firmwareOptions.is_airport = true;
    setupSerial(); setupSerial1();
    assert(dynamic_cast<SerialAirPort *>(serialIO) && dynamic_cast<SerialSRXL2 *>(serial1IO));
    clear(); firmwareOptions.is_airport = false;
    config.primary = PROTOCOL_CRSF;
    primaryRx = primaryTx = -1; noSerialPins = true;
    setupSerial(); setupSerial1();
    assert(dynamic_cast<SerialNOOP *>(serialIO) && dynamic_cast<SerialSRXL2 *>(serial1IO));
    clear();
    config.primary = PROTOCOL_SRXL2; config.secondary = PROTOCOL_SERIAL1_OFF;
    pinCount = 4; pwm[0].val.mode = pwm[1].val.mode = somSerial; noSerialPins = false;
    setupSerial();
    smart = dynamic_cast<SerialSRXL2 *>(serialIO);
    assert(smart && smart->pin == 1); // Exact stock UART0 fallback.
    clear(); primaryRx = 3;
    setupSerial(); assert(dynamic_cast<SerialNOOP *>(serialIO)); // No inferred TX with fixed RX alone.
    clear();
    primaryTx = 1; config.primary = PROTOCOL_CRSF; config.secondary = PROTOCOL_SERIAL1_SRXL2;
    pwm[2].val.mode = pwm[3].val.mode = somSerial1TX;
    setupSerial1(); smart = dynamic_cast<SerialSRXL2 *>(serial1IO);
    assert(smart && smart->pin == 14); // Fixed TX wins over conflicting fallback candidates.
    clear(); secondaryTx = -1;
    setupSerial1(); smart = dynamic_cast<SerialSRXL2 *>(serial1IO);
    assert(smart && smart->pin == 15); // Stock fallback is the last matching output.
    clear(); pwm[2].val.mode = pwm[3].val.mode = somSerial1RX;
    setupSerial1(); assert(!serial1IO && Serial1.begins == 0); // RX cannot replace missing Smart TX.
    clear(); secondaryTx = 14; pwm[2].val.mode = som50Hz;
    setupSerial1(); assert(!serial1IO); // No PWM detachment in the Smart factory.
    clear(); pwm[2].val.mode = somSerial1TX; primaryTx = 14;
    setupSerial1(); assert(!serial1IO); // Primary overlap rejected.
    clear(); primaryTx = 1; secondaryTx = 14; pinCount = 0;
    primaryTx = 10; secondaryTx = -1; secondaryRx = 5;
    config.primary = PROTOCOL_SRXL2; config.secondary = PROTOCOL_SERIAL1_SBUS;
    setupSerial(); setupSerial1();
    assert(dynamic_cast<SerialNOOP *>(serialIO) && dynamic_cast<SerialSBUS *>(serial1IO));
    assert(Serial1.rx == 9 && Serial1.tx == 10); // Stock UART defaults conflict, so Smart alone stays inactive.
    clear(); secondaryRx = -1; config.secondary = PROTOCOL_SERIAL1_GPS;
    setupSerial(); setupSerial1();
    assert(dynamic_cast<SerialSRXL2 *>(serialIO) && !serial1IO && Serial1.begins == 0);
    clear(); config.primary = PROTOCOL_CRSF; primaryTx = 1; secondaryTx = 14;
    // Compare every ordinary factory case and UART initialization to pinned stock.
    for (unsigned p = 0; p <= 12; ++p) {
        config.secondary = static_cast<eSerial1Protocol>(p);
        setupUpstreamSerial1();
        const auto *expected = serial1IO ? &typeid(*serial1IO) : nullptr;
        const Uart uart = Serial1;
        clear(); setupSerial1();
        assert((serial1IO ? &typeid(*serial1IO) : nullptr) == expected);
        assert(Serial1.begins == uart.begins && Serial1.baud == uart.baud && Serial1.format == uart.format);
        assert(Serial1.rx == uart.rx && Serial1.tx == uart.tx && Serial1.inverted == uart.inverted);
        clear();
    }
    config.primary = PROTOCOL_CRSF; config.secondary = PROTOCOL_SERIAL1_CRSF;
    setupSerial(); setupSerial1();
    assert(dynamic_cast<SerialCRSF *>(serialIO) && dynamic_cast<SerialCRSF *>(serial1IO));
    clear();
    std::puts("Dual factories: stock protocols, Smart on either port, PWM fallback, overlap and boot recovery passed");
}
"""
    def function(text, name):
        match = re.search(rf'^static void {name}\(\)\s*\{{.*?^\}}', text, re.M | re.S)
        assert match, name
        return match.group()

    cpp = Path(directory) / "dual.cpp"
    executable = Path(directory) / "dual.exe"
    cpp.write_text(boundary + enums + support + config
                   + function(stock, 'setupSerial1').replace('setupSerial1()', 'setupUpstreamSerial1()')
                   + function(source, 'setupSerial') + function(source, 'setupSerial1') + probe)
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
    baseline = subprocess.check_output(["git", "show", f"{STOCK}:src/src/rx_main.cpp"], cwd=ROOT, text=True)
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
        check_dual_factories(compiler, environment, directory)


if __name__ == "__main__":
    check()
