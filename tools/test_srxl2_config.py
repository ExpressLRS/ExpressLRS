"""Run the real receiver HTTP/Lua admission paths with fake configuration/hardware."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def block(source, token):
    start = source.index(token)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def check():
    wifi = (ROOT / 'src/lib/WIFI/devWIFI.cpp').read_text()
    lua = (ROOT / 'src/lib/rx-crsf/RXParameters.cpp').read_text()
    common = (ROOT / 'src/include/common.h').read_text()
    enums = '\n'.join(re.findall(r'enum (?:eSerial(?:1)?Protocol|eServoOutputMode|eFailsafeMode)\s*:\s*uint8_t\s*\{.*?\};', common, re.S))
    config_header = (ROOT / 'src/lib/CONFIG/config.h').read_text()
    pwm = re.search(r'typedef union \{\s*struct \{\s*uint32_t failsafe:11,.*?\} rx_config_pwm_t;', config_header, re.S).group()
    support = (ROOT / 'src/include/SRXL2Config.h').read_text()
    support = support.replace('#pragma once', '').replace('#include "common.h"', '').replace('#include "soc/soc_caps.h"', '')
    http = block(wifi[wifi.index('static void JsonUidToConfig'):], 'static void UpdateConfiguration')
    lua_helpers = '\n'.join(block(lua, token) for token in [
        'static uint8_t sanitizePwmMode', 'static void configureSerialPin', 'static void luaparamMappingOutputMode'
    ] if token in lua)
    callbacks = '\n'.join(block(lua, f'registerParameter(&{name},') + ');' for name in ['luaSerialProtocol', 'luaSerial1Protocol'])
    selections = '\n'.join(block(lua, f'static selectionParameter {name} =') + ';'
                           for name in ['luaSerialProtocol', 'luaSerial1Protocol'])
    capability = re.search(r'if \([^\n]*\)\s*\{\s*cfg\["serial1-protocol"\].*?\}', wifi, re.S).group()
    fixed_capability = re.search(r'^\s*settings\["has_serial1_pins"\].*?;', wifi, re.M)
    boundary = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <vector>
#include <string>
#include <ArduinoJson.h>
#define TARGET_RX
#define PLATFORM_ESP32
#define CONFIG_IDF_TARGET_ESP32
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK 0xFFFFFFFFULL
#define UNDEF_PIN -1
#define U0TXD_GPIO_NUM 1
#define U0RXD_GPIO_NUM 3
#define RX1 15
#define TX1 16
#define UID_LEN 6
#define STR_EMPTYSPACE " "
#define CRSF_TEXT_SELECTION 9
#define UNUSED(value) (void)(value)
#define constrain(value, low, high) std::min<size_t>(std::max<size_t>(value, low), high)
int8_t primaryRx = 3, primaryTx = 1, secondaryRx = -1, secondaryTx = 14;
int16_t pwmPins[] = {1, 3, 14, 15};
uint8_t pinCount = 4;
bool noSerialPins = false, pwmOnly = false;
#define GPIO_PIN_RCSIGNAL_RX primaryRx
#define GPIO_PIN_RCSIGNAL_TX primaryTx
#define GPIO_PIN_SERIAL1_RX secondaryRx
#define GPIO_PIN_SERIAL1_TX secondaryTx
#define GPIO_PIN_PWM_OUTPUTS pwmPins
#define GPIO_PIN_PWM_OUTPUTS_COUNT pinCount
#define OPT_HAS_SERVO_OUTPUT (pinCount != 0)
#define OPT_CRSF_RCVR_NO_SERIAL noSerialPins
#define OPT_PWM_OUT_ONLY pwmOnly
#define RX_HAS_SERIAL1 (secondaryTx != -1 || OPT_HAS_SERVO_OUTPUT)
uint8_t UID[UID_LEN] = {};
struct { bool is_airport = false; } firmwareOptions;
enum rx_config_bindstorage_t { BINDSTORAGE_PERSISTENT };
struct AsyncWebServerRequest {
    int status = 0;
    void send(int code, const char *, const char *) { status = code; }
};
'''
    fake_config = r'''
struct Config {
    eSerialProtocol primary = PROTOCOL_CRSF;
    eSerial1Protocol secondary = PROTOCOL_SERIAL1_OFF;
    rx_config_pwm_t channels[4] = {};
    uint8_t uid[UID_LEN] = {}, model = 7;
    unsigned writes = 0, commits = 0;
    eSerialProtocol GetSerialProtocol() const { return primary; }
    eSerial1Protocol GetSerial1Protocol() const { return secondary; }
    const rx_config_pwm_t *GetPwmChannel(uint8_t ch) const { assert(ch < 4); return &channels[ch]; }
    void SetSerialProtocol(eSerialProtocol value) { primary = value; ++writes; }
    void SetSerial1Protocol(eSerial1Protocol value) { secondary = value; ++writes; }
    void SetPwmChannelRaw(uint8_t ch, uint32_t value) { assert(ch < 4); channels[ch].raw = value; ++writes; }
    void SetFailsafeMode(eFailsafeMode) { ++writes; }
    void SetModelId(uint8_t value) { model = value; ++writes; }
    void SetForceTlmOff(bool) { ++writes; }
    void SetBindStorage(rx_config_bindstorage_t) { ++writes; }
    const uint8_t *GetUID() const { return uid; }
    void SetUID(const uint8_t *value) { memcpy(uid, value, UID_LEN); ++writes; }
    void Commit() { ++commits; }
    bool IsModified() const { return writes != 0; }
} config;
struct propertiesCommon { const char *name; int type; };
struct selectionParameter { propertiesCommon common; uint8_t value; const char *options; const char *unit; };
struct { struct { struct { uint8_t value = 1; } u; } properties; } luaMappingChannelOut;
selectionParameter luaMappingOutputMode = {};
unsigned reconfigured = 0, reconfigured1 = 0;
std::vector<std::function<void()>> deferred;
void deferExecutionMillis(unsigned delay, std::function<void()> callback) {
    assert(delay == 100); deferred.push_back(callback);
}
void reconfigureSerial() { ++reconfigured; }
void reconfigureSerial1() { ++reconfigured1; }
std::function<void(propertiesCommon *, uint8_t)> changePrimary, changeSecondary;
void registerParameter(selectionParameter *parameter, std::function<void(propertiesCommon *, uint8_t)> callback) {
    if (!strcmp(parameter->common.name, "Protocol")) changePrimary = callback;
    else changeSecondary = callback;
}
'''
    probe = r'''
static void reset() {
    config = Config();
    primaryRx = 3; primaryTx = 1; secondaryRx = -1; secondaryTx = 14;
    pinCount = 4; noSerialPins = pwmOnly = firmwareOptions.is_airport = false;
    config.channels[0].val.mode = config.channels[1].val.mode = somSerial;
    config.channels[2].val.mode = somSerial1TX;
    deferred.clear(); reconfigured = reconfigured1 = 0;
}
static void runDefers() {
    auto callbacks = deferred; deferred.clear();
    for (auto callback : callbacks) callback();
}
static void submit(JsonDocument &document, unsigned expectedStatus) {
    AsyncWebServerRequest request;
    JsonVariant json = document.as<JsonVariant>();
    UpdateConfiguration(&request, json);
    if (request.status != int(expectedStatus))
        std::fprintf(stderr, "HTTP %d/%d: expected %u, received %d\n", int(json["serial-protocol"]), int(json["serial1-protocol"]), expectedStatus, request.status);
    assert(request.status == int(expectedStatus));
}
static void assertUnchanged(eSerialProtocol primary, eSerial1Protocol secondary) {
    assert(config.primary == primary && config.secondary == secondary);
    assert(config.model == 7 && config.uid[0] == 0);
    assert(config.writes == 0 && config.commits == 0 && deferred.empty());
}
static void checkHttp() {
    // Final pair validation permits a complete move in either setter order.
    for (bool reverse : {false, true}) {
        reset();
        config.primary = reverse ? PROTOCOL_CRSF : PROTOCOL_SRXL2;
        config.secondary = reverse ? PROTOCOL_SERIAL1_SRXL2 : PROTOCOL_SERIAL1_CRSF;
        JsonDocument document;
        document["serial-protocol"] = reverse ? 11 : 0;
        document["serial1-protocol"] = reverse ? 1 : 13;
        submit(document, 200);
        assert(config.primary == (reverse ? 11 : 0) && config.secondary == (reverse ? 1 : 13));
        assert(config.commits == 1);
    }
    reset(); primaryRx = primaryTx = -1; noSerialPins = true;
    config.channels[0].val.mode = config.channels[1].val.mode = som50Hz;
    config.channels[2].val.mode = som50Hz;
    JsonDocument proposal;
    proposal["serial-protocol"] = 0; proposal["serial1-protocol"] = 13;
    JsonArray mapping = proposal["pwm"].to<JsonArray>();
    mapping.add(0); mapping.add(0); mapping.add(uint32_t(15) << 16); mapping.add(0);
    submit(proposal, 200); // PWM TX and Smart selected in one request.
    assert(config.secondary == 13 && config.channels[2].val.mode == somSerial1TX);
    reset(); primaryRx = primaryTx = -1; noSerialPins = true;
    config.channels[0].val.mode = config.channels[1].val.mode = som50Hz;
    proposal.clear(); proposal["serial-protocol"] = 11; proposal["serial1-protocol"] = 0;
    mapping = proposal["pwm"].to<JsonArray>();
    mapping.add(uint32_t(10) << 16); mapping.add(uint32_t(10) << 16); mapping.add(0); mapping.add(0);
    submit(proposal, 200); // Proposed primary default UART pins, running primary disabled.
    for (bool airport : {false, true}) {
      for (unsigned reason = 0; reason < 8; ++reason) {
        reset(); firmwareOptions.is_airport = airport;
        proposal.clear();
        proposal["serial-protocol"] = 11; proposal["serial1-protocol"] = 13;
        proposal["modelid"] = 42; proposal["uid"].to<JsonArray>().add(99);
        if (reason == 1) { proposal["serial-protocol"] = 0; secondaryTx = -1; config.channels[2].val.mode = somSerial1RX; }
        if (reason == 2) { proposal["serial-protocol"] = 0; secondaryTx = 34; }
        if (reason == 3) { proposal["serial-protocol"] = 0; primaryTx = 14; }
        if (reason == 4) { proposal["serial-protocol"] = 0; config.channels[2].val.mode = som50Hz; }
        if (reason == 5) { proposal["serial-protocol"] = 12; proposal["serial1-protocol"] = 0; }
        if (reason == 6) { proposal["serial1-protocol"] = 0; config.channels[0].val.mode = som50Hz; }
        if (reason == 7) { proposal["serial-protocol"] = 267; proposal["serial1-protocol"] = 0; }
        submit(proposal, 400);
        assertUnchanged(PROTOCOL_CRSF, PROTOCOL_SERIAL1_OFF);
      }
    }
    reset(); firmwareOptions.is_airport = true;
    proposal.clear(); proposal["serial-protocol"] = 0; proposal["serial1-protocol"] = 13;
    submit(proposal, 200); // Current WebUI stores primary CRSF when selecting AirPort.
    reset(); proposal["serial1-protocol"] = 1;
    submit(proposal, 200); // Duplicate ordinary protocols remain allowed.
    reset(); pwmOnly = true; proposal["serial1-protocol"] = 13;
    mapping = proposal["pwm"].to<JsonArray>();
    mapping.add(uint32_t(10) << 16); mapping.add(uint32_t(10) << 16);
    mapping.add(uint32_t(15) << 16); mapping.add(0);
    submit(proposal, 400); assertUnchanged(PROTOCOL_CRSF, PROTOCOL_SERIAL1_OFF);
    for (unsigned protocol : {1u, 3u}) {
        reset(); pinCount = 0; primaryTx = 16; secondaryTx = secondaryRx = -1;
        proposal.clear(); proposal["serial-protocol"] = 11; proposal["serial1-protocol"] = protocol;
        submit(proposal, 400); assertUnchanged(PROTOCOL_CRSF, PROTOCOL_SERIAL1_OFF);
        if (protocol == 3) {
            secondaryRx = 5; // TX-only setup ignores RX, so it still invokes Arduino defaults.
            submit(proposal, 400); assertUnchanged(PROTOCOL_CRSF, PROTOCOL_SERIAL1_OFF);
        }
    }
    reset(); pinCount = 0; primaryTx = 15; secondaryTx = secondaryRx = -1;
    proposal["serial1-protocol"] = 3;
    submit(proposal, 400); assertUnchanged(PROTOCOL_CRSF, PROTOCOL_SERIAL1_OFF); // Inferred RX is owned too.
    reset(); pinCount = 0; primaryTx = 16; secondaryTx = secondaryRx = -1;
    proposal["serial1-protocol"] = 11;
    submit(proposal, 200); // GPS with no RX remains inactive and cannot conflict.
    reset(); pinCount = 0; primaryTx = 16; secondaryTx = 17; secondaryRx = -1;
    proposal["serial1-protocol"] = 3;
    submit(proposal, 200); // An explicit distinct TX avoids defaults.
    std::puts("HTTP: final-pair moves, candidate PWM, disabled primary, AirPort, and pre-mutation rejection passed");
}
static void checkLua() {
    reset(); registerProtocols(); config.primary = PROTOCOL_SRXL2;
    changeSecondary(&luaSerial1Protocol.common, 13);
    assertUnchanged(PROTOCOL_SRXL2, PROTOCOL_SERIAL1_OFF);
    assert(luaSerial1Protocol.value == 0); // CRSF writes call the callback without replacing the parameter value.
    changePrimary(&luaSerialProtocol.common, 0); assert(deferred.size() == 1);
    runDefers(); assert(reconfigured == 1);
    changeSecondary(&luaSerial1Protocol.common, 13); assert(deferred.size() == 1);
    runDefers(); assert(reconfigured1 == 1 && config.secondary == 13);
    config.writes = 0;
    changePrimary(&luaSerialProtocol.common, 11);
    assertUnchanged(PROTOCOL_CRSF, PROTOCOL_SERIAL1_SRXL2);
    changeSecondary(&luaSerial1Protocol.common, 0); runDefers();
    changePrimary(&luaSerialProtocol.common, 11); runDefers();
    assert(config.primary == 11 && config.secondary == 0 && reconfigured == 2);
    reset(); secondaryTx = -1; config.channels[2].val.mode = somSerial1RX;
    changeSecondary(&luaSerial1Protocol.common, 13);
    assertUnchanged(PROTOCOL_CRSF, PROTOCOL_SERIAL1_OFF);
    reset(); config.primary = PROTOCOL_SRXL2;
    luaMappingChannelOut.properties.u.value = 2; // Primary RX GPIO3.
    luaMappingOutputMode.value = somSerial;
    const uint32_t tx = config.channels[0].raw, rx = config.channels[1].raw;
    luaparamMappingOutputMode(nullptr, som50Hz);
    assertUnchanged(PROTOCOL_SRXL2, PROTOCOL_SERIAL1_OFF);
    assert(config.channels[0].raw == tx && config.channels[1].raw == rx && luaMappingOutputMode.value == somSerial);
    reset(); config.secondary = PROTOCOL_SERIAL1_SRXL2;
    luaMappingChannelOut.properties.u.value = 3;
    luaparamMappingOutputMode(nullptr, som50Hz);
    assertUnchanged(PROTOCOL_CRSF, PROTOCOL_SERIAL1_SRXL2);
    assert(config.channels[2].val.mode == somSerial1TX);
    reset(); luaMappingChannelOut.properties.u.value = 2;
    luaparamMappingOutputMode(nullptr, som50Hz);
    assert(config.channels[0].val.mode == som50Hz && config.channels[1].val.mode == som50Hz);
    assert(deferred.size() == 1); // Ordinary stock sibling behavior retained.
    std::puts("Lua: rejection/refresh, deferred sequential moves, and indirect sibling-mode rejection passed");
}
static void checkVisibility() {
    reset(); pinCount = 0;
    JsonDocument document;
    exportSecondary(document["config"].to<JsonObject>(), document["settings"].to<JsonObject>());
    assert(document["config"]["serial1-protocol"].is<uint8_t>());
    assert(document["settings"]["has_serial1_pins"] == true); // Fixed TX-only, no primary capability gate.
    std::string options(luaSerial1Protocol.options);
    assert(std::count(options.begin(), options.end(), ';') == 13);
    assert(options.substr(options.rfind(';') + 1) == "Spektrum Smart");
    std::puts("Capability: TX-only secondary export and Lua Smart at ID 13 passed");
}
int main(int argc, char **argv) {
    const int suite = argc > 1 ? std::atoi(argv[1]) : 0;
    if (!suite || suite == 1) checkHttp();
    if (!suite || suite == 2) checkLua();
    if (!suite || suite == 3) checkVisibility();
}
'''
    tool_dir = ROOT / 'src/.pio/tools/mingw64/bin'
    compiler = str(tool_dir / 'g++.exe') if (tool_dir / 'g++.exe').exists() else shutil.which('g++')
    assert compiler, 'A native C++ compiler is required'
    includes = ROOT / 'src/.pio/libdeps/Unified_ESP32_2400_RX_via_UART/ArduinoJson/src'
    assert includes.exists(), 'Use the existing receiver build to install ArduinoJson'
    environment = dict(os.environ)
    environment['PATH'] = str(tool_dir) + os.pathsep + environment.get('PATH', '')
    with tempfile.TemporaryDirectory(prefix='elrs-srxl2-config-') as directory:
        cpp = Path(directory) / 'config.cpp'
        executable = Path(directory) / 'config.exe'
        cpp.write_text(boundary + enums + pwm + support + fake_config + selections
                       + block(wifi, 'static void JsonUidToConfig') + http + lua_helpers
                       + '\nstatic void registerProtocols() {\n' + callbacks + '\n}\n'
                       + '\nstatic void exportSecondary(JsonObject cfg, JsonObject settings) {\n' + capability
                       + (fixed_capability.group() if fixed_capability else '') + '\n}\n' + probe)
        subprocess.run([compiler, '-std=c++17', '-mno-ms-bitfields', '-I' + str(includes), str(cpp), '-o', str(executable)],
                       env=environment, check=True)
        subprocess.run([str(executable), *sys.argv[1:]], env=environment, check=True)


if __name__ == '__main__':
    check()
