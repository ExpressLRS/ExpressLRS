# ER6 Firma Smart/SRXL2 design

Status: user-approved and implemented. Software tests and receiver builds pass; physical ER6/Firma commissioning remains pending. Final image provenance and delivery evidence are recorded in `artifacts/manifest.json` and `artifacts/validation.md` after the source commit.

## Intended outcome

Add a selectable, compiled-in Spektrum Smart/SRXL2 serial protocol to ExpressLRS 4.1.0 for a RadioMaster ER6 controlling a Firma 85A Smart ESC. The receiver supplies digital throttle and reads ESC/battery telemetry through its dedicated serial connector. Existing CRSF telemetry carries the decoded readings to an unmodified ELRS transmitter. All six ER6 PWM outputs remain available.

The user requested a Superpowers plan, a new environment/folder, and implementation. This checkout is isolated from the user's modified Remote repository. Base: ExpressLRS tag `4.1.0`, commit `a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6`; feature branch `feat/firma-smart-srxl2`.

## Chosen approach

Implement an ordinary `SerialIO` driver and a small native-testable protocol core, following ELRS's existing HoTT integration and CRSF router. Add the primary serial selector value `PROTOCOL_SRXL2 = 10`, labeled **Spektrum Smart**. Preserve every existing protocol ID, the packed configuration layout, and the dynamically derived WebUI AirPort option. There is no runtime plugin loader to extend.

An external MSRC converter remains a useful protocol reference but does not meet the requested receiver-only outcome. Transmitter-side decoding would still require a receiver bus endpoint and additional raw-data transport. Neither alternative is part of this implementation.

## Scope and configuration

- Supported hardware: the standard RadioMaster ER6, ESP32, primary UART. Its dedicated UART TX GPIO1 and RX GPIO3 do not overlap PWM GPIOs 14, 12, 15, 2, 4, and 9.
- Use the existing ESP32 GPIO-matrix half-duplex pattern, with bidirectional traffic on the dedicated serial TX signal. Do not join the RX and TX leads by assumption. Confirm the physical cable's pinout and signal voltage during commissioning.
- Use ELRS channel 3 for throttle, mapped to SRXL2 channel 1. Keep this explicit in the setup guide so the user's mixer can assign the desired control to CH3. No new persistent throttle configuration is needed.
- Firma neutral is centered. Map nominal CRSF minimum/center/maximum (172/992/1811) to the specification's nominal Spektrum -100%/center/+100% (`0x2AA0`/`0x8000`/`0xD554`), clamp out-of-range input, and preserve centered neutral exactly. The full SRXL2 encoding range is 0 through `0xFFFC`; its lower two reserved bits must be zero. Document ESC endpoint calibration and transmitter travel/reversing adjustments. Surface reverse and brake use the throttle channel and the ESC's configured running mode; do not add an aircraft reverse-switch channel.
- Primary UART support only. Unsupported platforms must not expose an operational Smart option or transmit a fallback protocol after a stale/invalid saved selection. Preserve existing ESP8266 behavior.
- Use the existing PlatformIO toolchain and Unity tests. Add no new runtime dependency, plugin framework, or generic telemetry abstraction.

## Protocol core and safety

Create `src/lib/SRXL2/SRXL2.h` and `.cpp` with bounded packet parsing, packet encoding, a bus-master state machine, and decoded telemetry state. It must compile without Arduino or ESP32 headers for the existing native test environment.

- Follow Spektrum's published SRXL2 packet definitions, byte order, and CRC-16 polynomial `0x1021`, initial value zero, checksum transmitted high byte first. Packet storage is fixed-size: total lengths 5 through 80 bytes are legal; reject lengths outside that range before accessing payloads.
- Start at 115200 baud, 8N1, with receiver device ID `0x21`, ESC device ID `0x40`, receiver priority 10, and a per-device UID. Advertise only the baud rate and telemetry capabilities actually implemented. Do not advertise Spektrum Forward Programming.
- Handle discovery/handshake, the handshake completion broadcast required by the specification, incoming handshake requests, and re-handshake requests. Reject malformed packets, bad CRC, impossible lengths, and telemetry addressed to another master. Recover from noise, partial packets, back-to-back packets, and expired partial frames.
- Schedule channel packets at 10 ms intervals; request ESC telemetry at 100 ms intervals. Keep transmission, turnaround, and response windows distinct. Never send while a previous transmission or granted response window owns the bus.
- UART transmit completion must be observed before switching to receive; do not copy HoTT's fixed millisecond turnaround delay or block the ELRS loop while waiting.
- Start and reconnect at neutral. Require a fresh neutral throttle sample before accepting motion following startup, RF loss, output inhibition, or an ESC reconnect. The neutral release band is CRSF 976 through 1008 inclusive (992 +/- 16); output exactly `0x8000` for the release sample. Send neutral and the defined SRXL2 failsafe indication whenever control is disallowed.
- Control permission includes ELRS connection/failsafe, model match, team-race output inhibition, and freshness of an authorized throttle sample. No authorized sample for 100 ms means neutral. A missing frame between normal RF updates is not itself an immediate failsafe.
- An ESC reset/re-handshake or 2 s without a valid ESC handshake/telemetry reply must restart discovery safely and clear the neutral-release state. A CRC-valid addressed telemetry reply counts even if its optional measurement fields are unavailable. Local transmit echo and an absent Smart battery do not count as ESC replies or disconnection respectively. Do not replay a cached non-neutral command into a freshly initialized ESC.

## Telemetry

Decode ESC payload `0x20` using manufacturer units and unavailable-value sentinels: electrical RPM in tens, pack voltage in 0.01 V, FET/BEC temperature in 0.1 C, ESC current in 0.01 A, BEC current in 0.1 A, and BEC voltage in 0.05 V. Do not reuse the inconsistent MSRC BEC scaling. Retain per-field validity and timestamps.

Decode available Smart battery `0x42` realtime, cell, and identification packets: reported current, consumed capacity, temperature, cell count, and up to 18 cell voltages. Confirm subtype endianness and units against reference implementations and byte-vector tests. Forward fields supported by existing CRSF battery/RPM/temperature/cell-voltage frames. Smart battery metadata without an existing CRSF sensor representation remains decoded only if needed for those frames; do not invent proprietary sensor types.

Use ELRS's existing `crsfRouter.SetHeaderAndCrc()` and `deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, ...)`. Preserve source IDs so temperatures and voltage groups remain distinguishable. Prefer valid Smart-reported battery current/capacity when present; otherwise identify the current as ESC-reported in documentation. Do not calculate consumed mAh from an unverified motor-current field.

CRSF battery frame `0x08` has mandatory current, capacity and percentage slots but no unavailable-value convention honored by current stock EdgeTX. Preserve usable current/consumption telemetry: publish `0x08` only with fresh valid voltage and current, filling unavailable capacity/percentage with numeric zero placeholders. Document prominently that an unsupported capacity/percentage slot is a placeholder, not a measured empty battery; users must not enable alarms on it. Do not send max-value sentinels or shortened frames. Mark `crsfBatterySensorDetected` only while this real voltage/current aggregate is fresh. If only voltage is valid, publish pack voltage with `0x0E` source ID 128 instead; BEC voltage uses 129. RPM uses source ID 0; temperature sources 0/1/2 identify FET/BEC/battery; cells use source ID 0. This preserves available readings without hiding the stock-radio representation limitation.

Bound publication to one CRSF frame per 100 ms, rotating through available groups. Expire stale measurements rather than indefinitely refreshing their timestamps. Ordinary readings expire after 2 s and battery cell groups after 5 s; disappearance and reconnection clear accumulated validity. Actual telemetry fields depend on the Firma hardware/firmware and battery. The current project has no attached ESC and cannot certify which optional fields this unit emits.

## Firmware and user interface

Create `src/src/rx-serial/SerialSRXL2.h` and `.cpp` implementing `sendRCFrame`, `processBytes`, and `sendQueuedData`. Use the normal main loop rather than the immediate RC interrupt callback. Integrate its UART setup and lifetime into `setupSerial()` and existing shutdown/reconfigure operations.

Add **Spektrum Smart** to the primary WebUI/Lua protocol choices with capability checks. Reuse the existing selector storage and JSON handling; validate the new option at configuration boundaries. Keep secondary serial options unchanged. Regenerate bundled WebUI headers using the upstream build process.

Deliver a firmware binary with the exact `radiomaster.rx_2400.er6` hardware definition attached, plus a checksum, base/source commit, hardware-target revision, build command, and a setup/rollback guide. A generic Unified ESP32 binary without the ER6 layout is not an acceptable deliverable. Keep stock transmitter firmware and stock RF framing.

## Verification and completion

Software delivery requires: passing focused native protocol/control/telemetry tests; existing native regression tests; successful ER6-target ESP32 build; successful existing ESP8266 receiver regression build; WebUI build and selector checks; independent code review; committed source; and inspection of the packaged ER6 binary's embedded configuration and checksum.

Native tests must cover known wire vectors and malformed input, bus scheduling/turnaround state, throttle bounds and exact neutral, startup and every control-inhibition path, reconnect behavior, field scaling/sentinels, stale-data expiry, and CRSF payload bytes. Source-level configuration checks must prove unchanged old IDs, Smart ID 10, unchanged packed storage, and AirPort behavior. These tests establish software correctness within the modeled interfaces, not physical timing.

The delivered guide must include a bench commissioning procedure for electrical wiring, negotiation, throttle/neutral/brake/reverse, sensor discovery, and RF-loss/reconnect tests with the motor unloaded. Physical commissioning is required before operating the model, but no receiver or ESC has been connected to this workstation. Report that limitation plainly; do not label the build hardware-validated.

## References

- [ExpressLRS 4.1 SerialIO extension interface](https://github.com/ExpressLRS/ExpressLRS/blob/4.1.0/src/src/rx-serial/SerialIO.h)
- [ELRS serial protocol wiring](https://www.expresslrs.org/software/serial-protocols/)
- [Spektrum SRXL2 specification and reference code](https://github.com/SpektrumRC/SRXL2)
- [Spektrum ESC telemetry definitions](https://github.com/SpektrumRC/SpektrumDocumentation/blob/master/Telemetry/spektrumTelemetrySensors.h)
- [Firma 85A original specification](https://www.spektrumrc.com/ProdInfo/Files/SPMXSE1085-Instruction-Sheet-EN.pdf)
- [Firma 85A V2 specification](https://www.spektrumrc.com/on/demandware.static/-/Sites-horizon-master/default/dwcf12f076/Manuals/SPMXSE2085_PROGRAMMING_GUIDE_EN.pdf)
- [MSRC Smart implementation](https://github.com/dgatf/msrc/blob/v1.10/board/project/sensor/smart_esc.c)
- [GroundFlight Firma surface implementation and neutral failsafe experience](https://github.com/joshperry/groundflight)
