# ER6 Firma Smart/SRXL2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add selectable native Smart/SRXL2 throttle and telemetry support to an ER6 running custom ELRS 4.1 firmware, with a stock transmitter.

**Architecture:** A bounded native-testable SRXL2 core handles wire packets, bus state, throttle permission, and measurements. A `SerialIO` adapter handles ESP32 half-duplex UART and forwards existing CRSF telemetry; existing WebUI/Lua selector storage activates it.

**Tech Stack:** ExpressLRS 4.1.0, C++11, ESP32 Arduino, PlatformIO, upstream Unity native tests, existing WebUI build tooling.

**Spec:** `docs/superpowers/specs/2026-10-05-firma-smart-srxl2-design.md`

## Global Constraints

- Base: ExpressLRS tag `4.1.0`, commit `a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6`; branch `feat/firma-smart-srxl2`.
- Supported hardware: standard RadioMaster ER6, primary UART; preserve all six PWM outputs and stock ELRS RF/transmitter behavior.
- ELRS CH3 maps to SRXL2 CH1; nominal CRSF 172/992/1811 maps to SRXL2 `0x2AA0`/`0x8000`/`0xD554`, clamped and with the two reserved low bits zero. Startup/reconnect requires a fresh sample in the inclusive neutral band 976-1008; that release sample outputs exact center. All disallowed or 100 ms stale control is neutral.
- UART: 115200 baud, 8N1, receiver ID `0x21`, ESC ID `0x40`, priority 10; advertise no unsupported baud rate or Forward Programming capability.
- Control period 10 ms; telemetry requests/publication 100 ms; measurement expiry 2 s, cell-group expiry 5 s.
- An ESC reset/re-handshake or 2 s without a valid ESC handshake/telemetry reply revokes motion and restarts discovery; unavailable optional fields and local TX echo must not falsely determine connectivity.
- CRSF battery frames require valid fresh voltage/current; unavailable capacity/percentage are documented zero placeholders. Voltage-only data uses 0x0E source 128; BEC voltage source 129; RPM source 0; temperatures FET/BEC/battery sources 0/1/2; cells source 0.
- Primary protocol ID 10; preserve old IDs 0-9, config layout/version, secondary choices, and UI-only AirPort behavior.
- No new runtime dependency, plugin loader, proprietary CRSF sensor type, or aircraft reverse-switch channel.
- Software tests/builds do not certify the physical ESC. Deliver the bench procedure and label physical validation pending.

## Review Focus

1. Model mismatch/team-race inhibition with an apparently connected RF link must immediately revoke motion permission.
2. ESC or RF reconnect while throttle is held away from neutral must not replay cached motion.
3. UART transmission/telemetry response ownership must prevent overlap despite delayed main-loop callbacks and timer wrap.
4. Invalid/sentinel or stale telemetry must never become plausible fresh zero readings or overflowed CRSF values.
5. Old saved protocol IDs and AirPort must keep their meaning; unsupported hardware must not silently use another protocol.

## File Responsibilities

- Create `src/lib/SRXL2/SRXL2.h`, `.cpp`: fixed-buffer protocol/control/telemetry core, independent of Arduino.
- Create `src/test/test_srxl2/test_srxl2.cpp`: upstream Unity tests for packets, parser, state, throttle and measurements.
- Create `src/src/rx-serial/SerialSRXL2.h`, `.cpp`: ESP32 `SerialIO` adapter and CRSF publication.
- Modify `src/src/rx_main.cpp`: initialize the new primary UART mode and construct its adapter.
- Modify `src/include/common.h`, `src/lib/rx-crsf/RXParameters.cpp`, `src/html/src/utils/globals.js`, `src/lib/WIFI/devWIFI.cpp` and `src/html/src/pages/serial-panel.js`: selector integration and capability/configuration validation.
- Regenerate affected `src/html` generated headers using the upstream build command; do not hand-edit generated assets.
- Create `docs/firma-smart-er6.md`: installation, mapping, telemetry meanings, bench commissioning, rollback and known limits.
- Create a reproducible build helper and ignored artifact directory only if upstream build commands cannot produce a deterministic ER6-configured image by themselves.

---

### Task 1: Native protocol, control safety and telemetry core

**Interfaces:** Define `SRXL2::Packet { uint8_t bytes[80]; uint8_t length; }` and `SRXL2::Reading { int32_t value; uint32_t updatedUs; bool valid; }`. `SRXL2::Telemetry` contains readings named `rpm`, `voltage`, `current`, `temperatureFet`, `temperatureBec`, `voltageBec`, `currentBec`, `batteryCurrent`, `consumption`, `batteryTemperature`, and `cells[18]`, plus `uint8_t cellCount`. Use RPM, millivolts, milliamps, deci-degrees Celsius and mAh respectively. Reject values that cannot fit the documented destination; no signed overflow.

Define `SRXL2::Link` with `void reset(uint32_t uid, uint32_t nowUs)`, `void setControlPermission(bool permitted)`, `void setThrottle(uint16_t crsfValue, uint32_t nowUs)`, `void receive(uint8_t byte, uint32_t nowUs)`, `bool nextPacket(uint32_t nowUs, Packet &packet)`, `void transmitted(uint32_t nowUs)`, `bool connected() const`, and `Telemetry telemetry(uint32_t nowUs) const`. `nextPacket` reserves TX ownership on success; `transmitted` releases TX after the actual final stop bit. Only `setThrottle` refreshes authorized sample time, and only call it for a fresh authorized RF frame. Permission checks never refresh a cached command. Wire encoding/state transitions must match the specification.

Expose `uint16_t SRXL2::encodeThrottle(uint16_t crsfValue)` for endpoint/neutral checks:

```cpp
TEST_ASSERT_EQUAL_HEX16(0x2AA0, SRXL2::encodeThrottle(172));
TEST_ASSERT_EQUAL_HEX16(0x8000, SRXL2::encodeThrottle(992));
TEST_ASSERT_EQUAL_HEX16(0xD554, SRXL2::encodeThrottle(1811));
TEST_ASSERT_EQUAL_HEX16(0x2AA0, SRXL2::encodeThrottle(0));
TEST_ASSERT_EQUAL_HEX16(0xD554, SRXL2::encodeThrottle(2047));
```

- [ ] Read the complete SRXL2 specification and inspect manufacturer telemetry definitions plus MSRC/GroundFlight Smart battery byte order. Record fixed packet sizes, timeout/handshake rules and golden packet vectors in the tests.
- [ ] Establish clean baseline with `pio test -e native` from `src`, using `PLATFORMIO_BUILD_FLAGS=-DRegulatory_Domain_ISM_2400`. On Windows select an existing compatible native compiler; document the exact command. Diagnose baseline failures before attributing them to this change.
- [ ] Write failing Unity tests in `src/test/test_srxl2/test_srxl2.cpp`: CRC known vector; handshake/control golden bytes; fragmented/concatenated input; malformed length/CRC/resynchronization; handshake/re-handshake; timer wrap; transmission-completion and response-window exclusion. Run `pio test -e native -f test_srxl2` and confirm failure is caused by the missing new implementation.
- [ ] Implement the bounded encoder/parser and bus state in `src/lib/SRXL2/SRXL2.h` and `.cpp`; use fixed storage and existing CRC facilities where compatible. Pass those tests.
- [ ] Add failing assertions for the specified nominal endpoints, lower/upper clamp, center `0x8000`, reserved low bits zero, inclusive neutral-band release, no motion before a fresh neutral sample, disconnect, missing authorized samples for 100 ms, model mismatch, team-race inhibition, and ESC/RF reconnect with non-neutral throttle. Assert rediscovery at 2 s without a valid reply, no rediscovery solely for sentinel optional fields, and no liveness from local TX echo. Implement explicit control permission/release state and pass the focused tests.
- [ ] Add failing measurement tests: ESC field endianness/units and `0xFFFF`/`0xFF` sentinels; Smart battery subtypes/current/capacity/cells; per-field/group expiration; invalid replacement values; reconnect clearing; no fabricated consumption/percentage. Implement decoding and validity timestamps, then pass focused tests.
- [ ] Commit the tested core and tests after a focused diff review.

### Task 2: ER6 UART adapter and existing CRSF telemetry

**Consumes:** Task 1's `SRXL2::Link` API and measurements. **Produces:** `SerialSRXL2(Stream *output, Stream *input, int8_t txPin)` implementing `sendRCFrame(bool, bool, uint32_t *)`, `processBytes(uint8_t *, uint16_t)`, `sendQueuedData(uint32_t)`, and safe destruction/reconfiguration.

- [ ] Add failing checks that exercise actual adapter control gating and CRSF payload generation through the smallest runnable native harness/stubs compatible with existing `test_msp/mock_serial.h` and CRSF test patterns. Assert neutral on startup/mismatch/team-race/stale inputs, specified frame source IDs/endian values, source-sentinel suppression, voltage-only fallback, battery zero placeholders solely for unavailable capacity/percentage, and the 100 ms publication budget.
- [ ] Implement `SerialSRXL2.h/.cpp`: use CH3 snapshots only when authorized; check current model/team-race/failsafe status before using cached input. Keep transmission and RX turnaround nonblocking in the main loop; observe actual UART transmit completion. Reuse existing half-duplex GPIO mapping and CRSF framing/router patterns. Disable debug/log output on the ESC UART.
- [ ] Add primary `setupSerial()` selection in `rx_main.cpp` at 115200 8N1. Preserve GPIO/PWM assignments and existing adapter lifecycle. Unsupported builds must reject or safely disable Smart mode explicitly.
- [ ] Pass focused core/adapter tests. Compile `pio run -e Unified_ESP32_2400_RX_via_UART` with `PLATFORMIO_BUILD_FLAGS=-DRegulatory_Domain_ISM_2400`; inspect size and warnings. A generic successful image is only build evidence at this stage.
- [ ] Commit adapter and integration after reviewing every path that constructs, reconfigures or destroys the primary serial driver.

### Task 3: Selectable settings, complete firmware and delivery

**Consumes:** The working adapter. **Produces:** **Spektrum Smart** protocol selection, an ER6-configured firmware image, passing regression evidence, committed instructions and a rollback path.

- [ ] Add a focused configuration regression check: old protocol IDs remain 0-9, Smart is 10 and fits four-bit storage, existing config version/layout is unchanged, WebUI AirPort remains the final UI-only option, and Lua/WebUI enumerate matching protocol names. Exercise unsupported-platform selection validation.
- [ ] Append the primary protocol enum and Lua string, insert the WebUI option before AirPort, and add capability/configuration validation without altering secondary serial settings. Pass the configuration check.
- [ ] In `src/html`, run `npm ci` and `npm run build:all` using the repository's pinned tooling; include regenerated headers. Confirm the new option and old options in the actual built output.
- [ ] Run the full native regression suite and focused adapter/configuration checks. Compile the ER6 ESP32 receiver and one existing ESP8266 receiver target to detect unsupported-platform regressions. Repeat only checks affected by subsequent fixes.
- [ ] Pin/record the `ExpressLRS/targets` revision and build with `board_config = radiomaster.rx_2400.er6` using upstream unified configuration. Verify the resulting binary embeds the ER6 product identity, correct serial GPIOs and all six PWM GPIOs. Record the source/hardware revisions, build command, file size and SHA-256 in the delivery manifest. Do not include binding phrases or Wi-Fi secrets.
- [ ] Write `docs/firma-smart-er6.md`: custom-image installation; primary protocol selection; CH3 throttle mixer and six-PWM mapping; cable pinout/voltage checks; ESC calibration; Smart-battery-dependent readings; electrical RPM and reported current meanings; unsupported metadata; explicit capacity/percentage zero-placeholder and alarm limitations; restoring official ER6 4.1 firmware. Include bench checks for neutral/startup, forward/brake/reverse, ESC reset, RF loss, model mismatch and reconnect. Label these physical checks pending.
- [ ] Request independent whole-change correctness review, including the five Review Focus cases. Fix confirmed findings and rerun affected verification.
- [ ] Run `git diff --check`, inspect the final intended diff and artifact manifest, commit the verified source/docs, and report links to the plan, guide, firmware and validation results. Do not flash hardware or publish a PR unless requested.

## Execution Recommendation

Native execution in this session with a final independent reviewer is recommended: the protocol core and UART adapter share close timing/safety contracts, and keeping one implementer avoids inconsistent assumptions. Read-only protocol and build investigations may run concurrently. A subagent-driven execution remains available if selected during written-plan review.
