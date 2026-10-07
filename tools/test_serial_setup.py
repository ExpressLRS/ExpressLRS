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
int8_t pins[] = {14, 15};
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
    assert(getSerial1TxPin() == 15 && getSRXL2Port() == -1); // still a PWM output
    config.pwm[1].val.mode = somSerial1TX;
    assert(getSRXL2Port() == 1);
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
    cpp.write_text(boundary + enums + '\n' + support + config + 'int8_t getSerial1TxPin(const uint32_t *pwm = nullptr);\nint8_t getSerial1RxPin(const uint32_t *pwm = nullptr);\nbool isSecondarySmartPinUsable(const uint32_t *pwm = nullptr);\n' + helpers[0].removesuffix('#endif') + probe)
    subprocess.run([compiler, '-std=c++11', str(cpp), '-o', str(executable)], env=environment, check=True)
    subprocess.run([str(executable)], env=environment, check=True)



def check_reconfiguration(compiler, environment, directory):
    directory = Path(directory)
    base = (directory / 'selection.cpp').read_text().split('\nint main() {')[0]
    base = base.replace('bool is_airport = false;', 'bool is_airport = false; uint32_t uart_baud = 420000;')
    receiver = (ROOT / 'src/src/rx_main.cpp').read_text()
    names = ('setupSerial', 'setupSerial1', 'serialShutdown', 'serial1Shutdown', 'reconfigureSerial', 'reconfigureSerial1', 'reconfigureSerialPorts', 'updateSerialPorts')
    functions = {}
    for name in names:
        match = re.search(r'^(?:static )?void ' + name + r'\([^\n]*\)\s*\{.*?^\}', receiver, re.M | re.S)
        assert match, name
        functions[name] = match[0]
    boundary = r"""
#include <atomic>
#include <vector>
#include <string>
#include <algorithm>
struct Stream { virtual ~Stream() = default; };
struct NullStream : Stream {};
struct Uart : Stream {
    unsigned begins = 0, ends = 0;
    void begin(uint32_t, uint32_t = 0, int8_t = -1, int8_t = -1, bool = false) { ++begins; }
    void end() { ++ends; }
} Serial, Serial1;
#define SERIAL_8N1 0
#define SERIAL_8E2 1
#define SERIAL_8N2 2
#define SERIAL_PROTOCOL_TX Serial
#define SERIAL_PROTOCOL_RX Serial
#define SERIAL1_PROTOCOL_TX Serial1
#define SERIAL1_PROTOCOL_RX Serial1
#define U0TXD_GPIO_NUM 1
struct SerialIO { virtual ~SerialIO() = default; SerialIO() = default; SerialIO(Stream &, Stream &) {} SerialIO(Stream &, int8_t) {} SerialIO(Stream &, Stream &, int8_t) {} };
#define MOCK_DRIVER(name) struct name : SerialIO { using SerialIO::SerialIO; };
MOCK_DRIVER(SerialNOOP)
MOCK_DRIVER(SerialCRSF)
MOCK_DRIVER(SerialAirPort)
MOCK_DRIVER(SerialSBUS)
MOCK_DRIVER(SerialSUMD)
MOCK_DRIVER(SerialMavlink)
MOCK_DRIVER(SerialDisplayport)
MOCK_DRIVER(SerialGPS)
MOCK_DRIVER(SerialHoTT_TLM)
MOCK_DRIVER(SerialScorpion_TLM)
MOCK_DRIVER(SerialTramp)
MOCK_DRIVER(SerialSmartAudio)
static bool hardwareReady = false;
static unsigned live = 0, maxLive = 0;
std::vector<std::string> lifecycle;
struct SerialSRXL2 : SerialIO {
    uint8_t port; int8_t pin;
    SerialSRXL2(Stream *, Stream *, int8_t p, uint8_t slot = 0) : port(slot), pin(p) {
        ++live; maxLive = std::max(live, maxLive); lifecycle.push_back("start" + std::to_string(port));
    }
    ~SerialSRXL2() { --live; lifecycle.push_back("stop" + std::to_string(port)); }
    uint8_t getPort() const { return port; }
    int8_t getPin() const { return pin; }
    bool readyForShutdown(uint32_t) { return hardwareReady; }
};
SerialIO *serialIO = nullptr, *serial1IO = nullptr;
SerialSRXL2 *srxl2IO = nullptr;
std::atomic<uint8_t> pendingSerialChanges{0};
Stream *BackpackOrLogStrm = nullptr;
uint32_t serialBaud = 420000;
uint32_t micros() { return 100000; }
void reconfigureServoOutput() {}
constexpr uint32_t EVENT_CONFIG_SERIAL_CHANGE = 1 << 17;
unsigned serialEvents = 0;
void devicesTriggerEvent(uint32_t events) { assert(events == EVENT_CONFIG_SERIAL_CHANGE); ++serialEvents; }
void reconfigureSerialPorts(bool, bool);
"""
    probe = r"""
int main() {
    secondaryTx = 14;
    config.pwm[0].val.mode = somSerial1TX;
    config.primary = PROTOCOL_CRSF;
    config.secondary = PROTOCOL_SERIAL1_SRXL2;
    setupSerial(); setupSerial1();
    assert(dynamic_cast<SerialCRSF *>(serialIO));
    assert(srxl2IO && srxl2IO->getPort() == 1);
    assert(Serial.begins == 1 && Serial1.begins == 0); // driver owns UART1 initialization
    config.secondary = PROTOCOL_SERIAL1_OFF;
    reconfigureSerial1(); updateSerialPorts();
    assert(srxl2IO && live == 1 && serialEvents == 0); // outgoing TX/grant still owns its resources
    hardwareReady = true; updateSerialPorts();
    assert(!srxl2IO && live == 0 && Serial.begins == 1 && serialEvents == 1);
    config.secondary = PROTOCOL_SERIAL1_SRXL2;
    reconfigureSerial1(); updateSerialPorts();
    assert(srxl2IO && srxl2IO->getPort() == 1 && Serial.begins == 1);
    hardwareReady = false;
    config.primary = PROTOCOL_SRXL2;
    config.secondary = PROTOCOL_SERIAL1_OFF;
    reconfigureSerialPorts(true, true); updateSerialPorts();
    assert(srxl2IO->getPort() == 1);
    hardwareReady = true; updateSerialPorts();
    assert(srxl2IO && srxl2IO->getPort() == 0 && live == 1 && maxLive == 1);
    assert(lifecycle[lifecycle.size()-2] == "stop1" && lifecycle.back() == "start0");
    serialShutdown(); serial1Shutdown();
    config.primary = PROTOCOL_SRXL2; config.secondary = PROTOCOL_SERIAL1_SRXL2;
    setupSerial(); setupSerial1();
    assert(srxl2IO && srxl2IO->getPort() == 0 && dynamic_cast<SerialNOOP *>(serial1IO));
    serialShutdown(); serial1Shutdown();
    config.primary = PROTOCOL_CRSF; config.secondary = PROTOCOL_SERIAL1_SRXL2;
    noSerialPins = true;
    setupSerial(); setupSerial1();
    assert(dynamic_cast<SerialNOOP *>(serialIO) && srxl2IO && srxl2IO->getPort() == 1);
    serialShutdown(); serial1Shutdown();
    std::puts("Real factories/coordinator: secondary Smart, unchanged other UART, safe ownership migration and corrupt-settings fallback passed");
}
"""
    cpp = directory / 'reconfiguration.cpp'
    cpp.write_text(base + boundary + '\n' + '\n'.join(functions[name] for name in names) + probe)
    executable = directory / 'reconfiguration.exe'
    subprocess.run([compiler, '-std=c++11', str(cpp), '-o', str(executable)], env=environment, check=True)
    subprocess.run([str(executable)], env=environment, check=True)



def check_configuration(compiler, environment, directory):
    directory = Path(directory)
    base = (directory / 'selection.cpp').read_text().split('\nint main() {')[0]
    start = base.index('struct Pwm')
    end = base.index('} config;', start) + len('} config;')
    config = r"""
#define OPT_PWM_OUT_ONLY false
#define PWM_MAX_CHANNELS 2
#define DBGLN(...) ((void)0)
using rx_config_bindstorage_t = uint8_t;
using eFailsafeMode = uint8_t;
union rx_config_pwm_t { uint32_t raw; struct { unsigned lower:16, mode:4, upper:12; } val; rx_config_pwm_t() : raw(0) {} };
struct Config {
    eSerialProtocol primary = PROTOCOL_CRSF;
    eSerial1Protocol secondary = PROTOCOL_SERIAL1_SRXL2;
    rx_config_pwm_t pwm[2];
    unsigned writes = 0, commits = 0;
    eSerialProtocol GetSerialProtocol() { return primary; }
    eSerial1Protocol GetSerial1Protocol() { return secondary; }
    rx_config_pwm_t *GetPwmChannel(unsigned ch) { return &pwm[ch]; }
    void SetSerialProtocol(eSerialProtocol value) { ++writes; primary = value; }
    void SetSerial1Protocol(eSerial1Protocol value) { ++writes; secondary = value; }
    void SetPwmChannelRaw(unsigned ch, uint32_t value) { ++writes; pwm[ch].raw = value; }
    void SetFailsafeMode(uint8_t) { ++writes; }
    void SetModelId(long) { ++writes; }
    void SetForceTlmOff(bool) { ++writes; }
    void SetBindStorage(uint8_t) { ++writes; }
    void Commit() { ++commits; }
} config;
"""
    base = base[:start] + config + base[end:]
    wifi = (ROOT / 'src/lib/WIFI/devWIFI.cpp').read_text()
    funcs = re.findall(r'^static void UpdateConfiguration\(AsyncWebServerRequest \*request, JsonVariant &json\)\s*\{.*?^\}', wifi, re.M | re.S)
    assert funcs
    boundary = r"""
#include <ArduinoJson.h>
struct AsyncWebServerRequest { unsigned status = 0; void send(unsigned code, const char *, const char *) { status = code; } };
void JsonUidToConfig(JsonVariant &) {}
void reconfigureSerialPorts(bool, bool) {}
"""
    probe = r"""
int main() {
    secondaryTx = 14;
    AsyncWebServerRequest request;
    JsonDocument doc;
    doc["serial-protocol"] = 11; doc["serial1-protocol"] = 13;
    JsonVariant json = doc.as<JsonVariant>();
    UpdateConfiguration(&request, json);
    assert(request.status == 400 && config.writes == 0 && config.commits == 0);
    doc.clear(); doc["serial1-protocol"] = 14; json = doc.as<JsonVariant>();
    UpdateConfiguration(&request, json);
    assert(request.status == 400 && config.writes == 0);
    doc.clear(); doc["serial1-protocol"] = -1; json = doc.as<JsonVariant>();
    UpdateConfiguration(&request, json);
    assert(request.status == 400 && config.writes == 0);
    secondaryTx = -1;
    doc.clear(); doc["serial1-protocol"] = 13;
    doc["pwm"].to<JsonArray>().add(uint32_t(somSerial1TX) << 16); json = doc.as<JsonVariant>();
    UpdateConfiguration(&request, json);
    assert(request.status == 200 && config.secondary == PROTOCOL_SERIAL1_SRXL2 && config.primary == PROTOCOL_CRSF);
    const unsigned previous = config.writes;
    doc.clear(); doc["pwm"].to<JsonArray>().add(uint32_t(som50Hz) << 16); json = doc.as<JsonVariant>();
    UpdateConfiguration(&request, json);
    assert(request.status == 400 && config.writes == previous && config.pwm[0].val.mode == somSerial1TX);
    std::puts("Real configuration handler: atomic pair rejection, missing-field preservation and prospective PWM validation passed");
}
"""
    cpp = directory / 'configuration.cpp'
    cpp.write_text(base + boundary + funcs[-1] + probe)
    executable = directory / 'configuration.exe'
    libdeps = Path(os.environ.get('PLATFORMIO_LIBDEPS_DIR', ROOT / 'src/.pio/libdeps'))
    json_headers = list(libdeps.glob('*/ArduinoJson/src/ArduinoJson.h'))
    assert json_headers, 'Install the existing ArduinoJson firmware dependency before this check'
    json_include = json_headers[0].parent
    subprocess.run([compiler, '-std=c++11', '-I' + str(json_include), str(cpp), '-o', str(executable)], env=environment, check=True)
    subprocess.run([str(executable)], env=environment, check=True)



def check_servo_handoff(compiler, environment, directory):
    directory = Path(directory)
    source = (ROOT / 'src/lib/ServoOutput/devServoOutput.cpp').read_text()
    func = re.search(r'^void reconfigureServoOutput\(\)\s*\{.*?^\}', source, re.M | re.S)
    assert func
    cpp=directory/'servo-handoff.cpp'
    cpp.write_text(r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#define PLATFORM_ESP32
#define GPIO_PIN_PWM_OUTPUTS_COUNT 2
bool outputsConfigured = true, initialized = true;
uint8_t outputModes[] = {0,0};
int pwmChannels[] = {0,1};
unsigned starts = 0, events = 0, releases = 0;
struct Pwm { struct { uint8_t mode = 0; } val; };
struct Config { Pwm channels[2]; Pwm *GetPwmChannel(unsigned i) { return &channels[i]; } } config;
struct PWMClass { void release(int i) { assert(i>=0); ++releases; } } PWM;
struct DShot {};
DShot *dshotInstances[2] = {nullptr,nullptr};
bool initialize() { ++starts; for(unsigned i=0;i<2;++i) outputModes[i]=config.channels[i].val.mode; return true; }
int event() { ++events; return 0; }
""" + func[0] + r"""
int main() {
    reconfigureServoOutput(); assert(starts==0 && releases==0);
    config.channels[0].val.mode=15;
    reconfigureServoOutput(); assert(starts==1 && releases==2 && events==1 && !initialized);
    reconfigureServoOutput(); assert(starts==1 && releases==2);
    std::puts("Servo/UART pin handoff: changed modes release old PWM resources exactly once");
}
""")
    executable=directory/'servo-handoff.exe'
    subprocess.run([compiler,'-std=c++11',str(cpp),'-o',str(executable)],env=environment,check=True)
    subprocess.run([str(executable)],env=environment,check=True)


def check_lua_configuration(compiler, environment, directory):
    directory = Path(directory)
    base = (directory / 'configuration.cpp').read_text().split('#include <ArduinoJson.h>')[0]
    base = base.replace('void Commit() {', 'bool IsModified() { return writes != 0; }\n    void Commit() {')
    source = (ROOT / 'src/lib/rx-crsf/RXParameters.cpp').read_text()
    functions = '\n'.join(re.search(r'^static void ' + name + r'\([^\n]*\)\s*\{.*?^\}', source, re.M | re.S)[0]
                          for name in ('configureSerialPin', 'luaparamMappingOutputMode'))
    callback = re.search(r'registerParameter\(&luaSerial1Protocol, (\[\]\(propertiesCommon\* item, uint8_t arg\)\{.*?^    \})\);', source, re.M | re.S)
    assert callback
    cpp = directory / 'lua-configuration.cpp'
    cpp.write_text(base + r'''
struct propertiesCommon {};
#define UNUSED(x) ((void)(x))
struct { struct { struct { uint8_t value = 1; } u; } properties; } luaMappingChannelOut;
unsigned deferred = 0;
template<class F> void deferExecutionMillis(unsigned, F) { ++deferred; }
void reconfigureSerial() {}
void reconfigureSerial1() {}
uint8_t sanitizePwmMode(uint8_t mode) { return mode; }
''' + functions + '\nauto selectSecondary = ' + callback[1] + r''';
int main() {
    config.secondary = PROTOCOL_SERIAL1_OFF;
    selectSecondary(nullptr, PROTOCOL_SERIAL1_SRXL2);
    assert(config.writes == 0 && deferred == 0); // no TX assigned
    secondaryTx = primaryRx;
    selectSecondary(nullptr, PROTOCOL_SERIAL1_SRXL2);
    assert(config.writes == 0 && deferred == 0); // shared with primary UART
    secondaryTx = -1;
    config.pwm[0].val.mode = somSerial1TX;
    selectSecondary(nullptr, PROTOCOL_SERIAL1_SRXL2);
    assert(config.secondary == PROTOCOL_SERIAL1_SRXL2 && config.writes == 1 && deferred == 1);
    config.writes = deferred = 0;
    luaparamMappingOutputMode(nullptr, som50Hz);
    assert(config.writes == 0 && deferred == 0 && config.pwm[0].val.mode == somSerial1TX);
    // Primary UART sibling adjustments must also be checked before either write.
    pins[0] = 1; pins[1] = 3; noSerialPins = true;
    config.pwm[0].val.mode = som50Hz; config.pwm[1].val.mode = somSerial1TX;
    luaparamMappingOutputMode(nullptr, somSerial);
    assert(config.writes == 0 && deferred == 0);
    assert(config.pwm[0].val.mode == som50Hz && config.pwm[1].val.mode == somSerial1TX);
    std::puts("Real Lua callbacks: invalid/shared Smart TX and prospective sibling mapping rejected before writes");
}
''')
    executable = directory / 'lua-configuration.exe'
    subprocess.run([compiler, '-std=c++11', '-mno-ms-bitfields', str(cpp), '-o', str(executable)], env=environment, check=True)
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
        '#include "config.h"', '#include "teamrace-config.h"').replace(
        '#define NO_SERIALIO_INTERVAL', '#define PLATFORM_ESP32\n#define NO_SERIALIO_INTERVAL'))
    scheduler = directory / 'device.cpp'
    scheduler.write_text('#include <algorithm>\n' + (ROOT / 'src/lib/DEVICE/device.cpp').read_text())
    probe = directory / "teamrace.cpp"
    probe.write_text(r"""
#include <cassert>
#include <cstdio>
#include "targets.h"
#include "common.h"
#include "SerialIO.h"
#include "crsf_protocol.h"
#define PLATFORM_ESP32
#include "devSerialIO.h"
#include "teamrace-config.h"
Config config;
connectionState_e connectionState = connected;
bool connectionHasModelMatch = true, teamraceHasModelMatch = true;
uint32_t ChannelData[CRSF_NUM_CHANNELS] = {};
class Probe : public SerialIO {
public:
    Probe() : SerialIO(nullptr, nullptr) {}
    uint32_t sendRCFrame(bool available, bool, uint32_t *channels) override {
        passed = available;
        throttle = channels[2];
        ++frames;
        return 1;
    }
    bool passed = false;
    unsigned frames = 0;
    uint32_t throttle = 0;
    void processBytes(uint8_t *, uint16_t) override {}
};
class Immediate : public Probe { bool sendImmediateRC() override { return true; } } immediate;
unsigned long &schedulerNow = nativeClockMs();
void SerialIO::setFailsafe(bool value) { failsafe = value; }
void SerialIO::processSerialInput() {}
void SerialIO::sendQueuedData(uint32_t) {}
Probe probe;
SerialIO *serialIO = &probe;
SerialIO *serial1IO = &immediate;
static void frame() { crsfRCFrameAvailable(); Serial0_device.timeout(); }
int main() {
    schedulerNow = 100;
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
    serialIO = &immediate;
    device_affinity_t devices[] = {{&Serial0_device, 1}, {&Serial1_device, 1}};
    devicesRegister(devices, 2); devicesStart(); devicesUpdate(schedulerNow);
    // CRSF's immediate-send setting has now parked both timeouts at NEVER.
    serial1IO = &probe; probe.passed = false; probe.frames = 0;
    ChannelData[2] = 992; // fresh neutral
    crsfRCFrameAvailable(); devicesTriggerEvent(EVENT_CONFIG_SERIAL_CHANGE);
    devicesUpdate(++schedulerNow);
    assert(probe.passed && probe.frames == 1 && probe.throttle == 992);
    ChannelData[2] = 1500;
    crsfRCFrameAvailable(); devicesUpdate(++schedulerNow);
    assert(probe.passed && probe.frames == 2 && probe.throttle == 1500 && immediate.frames == 0);
    std::puts("Team-race Off: live inhibition clears after reconnect; model mismatch remains blocked");
    std::puts("Serial scheduler: secondary CRSF-to-scheduled-driver switch resumes RC frames without reboot");
}
""")
    executable = directory / "teamrace.exe"
    includes = ["-I" + str(ROOT / "src/include"), "-I" + str(ROOT / "src/src/rx-serial")]
    includes += ["-I" + str(path) for path in (ROOT / "src/lib").iterdir() if path.is_dir()]
    subprocess.run([compiler, "-std=gnu++17", "-mno-ms-bitfields", "-DTARGET_NATIVE", "-DUNIT_TEST",
                    "-DTARGET_RX", "-DRegulatory_Domain_ISM_2400", *includes,
                    '-DRADIO_SX128X=1', str(source), str(scheduler), str(probe), "-o", str(executable)], env=environment, check=True)
    subprocess.run([str(executable)], env=environment, check=True)


def check():
    source = (ROOT / "src/src/rx_main.cpp").read_text()
    secondary_setup = re.search(r'^static void setupSerial1\(\).*?^\}', source, re.M | re.S)
    assert secondary_setup and 'case PROTOCOL_SERIAL1_SRXL2:' in secondary_setup[0], 'Secondary factory must construct the Smart driver at ID 13'
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
        check_reconfiguration(compiler, environment, directory)
        check_configuration(compiler, environment, directory)
        check_lua_configuration(compiler, environment, directory)
        check_servo_handoff(compiler, environment, directory)


if __name__ == "__main__":
    check()
