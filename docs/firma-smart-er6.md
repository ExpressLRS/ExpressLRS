# Firma Smart telemetry on RadioMaster ER6

This custom ExpressLRS 4.1.0 receiver build adds **Spektrum Smart** to the primary serial protocol selector. It controls a Firma 85A through SRXL2 and forwards telemetry over existing CRSF. The ELRS transmitter remains stock 4.1; all six ER6 PWM signal outputs remain available. No RP2040/MSRC converter is required.

Software tests and builds have been performed. **The ER6/Firma hardware combination has not been bench-tested.** Do the commissioning checks below before driving the model. Use this image only for the standard RadioMaster ER6, not ER6G/ER6GV or another receiver.

## Installation and wiring

1. Record your binding, model-match and PWM settings. Disconnect the ESC signal and unload/disconnect the motor while installing.
2. Upload `artifacts/ELRS-4.1.0-ER6-FirmaSmart.bin` through the receiver's ordinary ELRS Wi-Fi firmware-update page. The manifest contains its SHA-256 and embedded ER6 identity. Keep an official ER6 4.1 image for rollback.
3. Bind/check the radio link as for an ordinary ELRS update. Set the model mixer so **ELRS CH3 is throttle**, centered at neutral. Other channels can drive the six PWM outputs through normal output mapping.
4. Select **Spektrum Smart** in the receiver WebUI's primary Serial Protocol field, or the receiver's Lua **Protocol** field. Secondary serial settings remain independent. Power off before connecting the ESC.
5. Connect the Firma's throttle **signal** to the ER6 dedicated serial connector's **TX signal** using its supplied pigtail. Leave the serial RX lead unused. Identify connector pins from the ER6 diagram/labels; do not infer pin order from wire color.
6. The ESC BEC positive and ground feed the ER6's normal power/servo rail. A power-only Y lead can share a servo connection without consuming a PWM signal output. Establish a common ground with the ESC. Leave the UART pigtail's power lead unused when powering through that rail. Do not connect BEC voltage to a GPIO signal.

The ER6 supply rating is 4.5-8.4 V; Firma 85A BEC choices are 6/7.4 V. Use settings suitable for your servo. The serial GPIO is a 3.3 V signal. Confirm the ESC bus's logic level before connecting; neither supply ratings nor software tests establish the signal voltage.

The firmware uses UART0 at 115200 8N1, GPIO1 as a shared open-drain TX/RX signal, and an internal pull-up. Confirm rise time and actual bus turnaround on the assembled cable; add a suitable pull-up to the **3.3 V logic supply** only if measurements require it. GPIO3 is the unused dedicated RX lead. The six PWM GPIOs are 14, 12, 15, 2, 4 and 9.

## Throttle and failsafe

ELRS CH3 maps to SRXL2 CH1. Nominal -100/0/+100% becomes Spektrum -100/center/+100%; center is exactly `0x8000`. Adjust transmitter travel/reversing and calibrate ESC neutral/full-forward/full-brake following its model manual. Reverse/brake behavior comes from the Firma running-mode setting, not an aircraft reverse-switch channel.

Startup, RF loss, model mismatch, team-race inhibition and ESC rediscovery revoke motion permission. A fresh neutral sample (CRSF 976-1008 inclusive) is required before motion is accepted again. No authorized throttle sample for 100 ms sends neutral with the SRXL2 failsafe command. A valid ESC reply must arrive within 2 s or discovery restarts. The ESC ultimately determines what it does with a failsafe packet; test that behavior physically.

RF resynchronization also invalidates the serial input generation and pending frame latch, including transitions that occur entirely between main-loop callbacks. Missing CH3 values remain missing rather than becoming a minimum-throttle command. UART RX FIFO/FSM activity and a 174 us quiet interval prevent scheduling a control packet over a late reply.

## Telemetry

Use the radio's sensor discovery after the link and ESC handshake are established. The radio handset must support the CRSF sensor types being displayed; an older decoder may expose battery data while ignoring extended RPM/temperature/voltage groups.

| Data supplied by ESC/battery | CRSF representation |
| --- | --- |
| Pack voltage and current | Battery frame; Smart battery current is preferred when supplied |
| Pack voltage without valid current | Voltage group source 128 (`VOLT128`) |
| Electrical RPM | RPM source 0; mechanical RPM/speed require pole count and gearing calculations |
| ESC FET / BEC / battery temperature | Temperature sources 0 / 1 / 2 |
| BEC voltage | Voltage group source 129 |
| Smart battery cells | Cell group source 0; actual Firma 85A packs are 2S/3S |
| Smart battery consumed capacity | Battery capacity in mAh, when supplied |

**Unavailable capacity or percentage uses a zero placeholder in the CRSF battery frame. That does not mean the battery is empty. Do not configure alarms on an unsupported capacity/percentage reading.** The standard frame requires these slots and current stock EdgeTX does not recognize a reliable unavailable marker. This build does not estimate percentage or integrate an unverified motor-current field into consumed mAh.

ESC-reported current is forwarded as reported; it is not independently verified as battery-input current on this exact Firma. Optional BEC fields depend on firmware/model. A compatible Smart battery is required for battery-specific fields; ordinary batteries still allow ESC measurements. BEC current is decoded but has no separate stock CRSF sensor in this implementation. Cycle count and other battery metadata are not presented as invented sensor types.

Publication is limited to one CRSF frame every 100 ms, rotating among available groups. Ordinary measurements expire after 2 s and cells after 5 s. A complete valid cell group is required for publication. Missing fields are suppressed rather than converted to plausible measurements; the documented battery placeholders are the sole exception.

## Bench commissioning checklist

- With the motor unloaded, confirm receiver power and the 3.3 V bus level, idle release, 115200-baud edges and complete request/reply packets.
- Confirm handshaking and sensor discovery first. Check voltage/current/temperature against suitable instruments where available.
- Power on while throttle is non-neutral: the motor must remain stopped. Move to neutral, then test forward, brake and reverse at low demand.
- Test both configured Firma running modes as applicable; confirm neutral and travel calibration.
- At low demand, turn the transmitter off. Confirm stop/neutral, then reconnect while holding non-neutral throttle. Motion must stay inhibited until a fresh neutral sample.
- Repeat with model mismatch, team-race output inhibition, and an ESC power reset. Confirm neutral recovery and that a missing Smart battery does not trigger endless discovery.
- Recheck all servo output mappings and failsafe settings. Save observed results with ESC SKU/firmware, receiver firmware manifest and radio packet/telemetry rate.

## Build, provenance and rollback

Base source is ExpressLRS tag 4.1.0, `a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6`. Hardware targets are pinned to `c8bb70d6ad08da08381fc898adf29b8ae40eae12`. Use PlatformIO 6.1.19, its pinned Espressif platforms, and the existing WebUI lockfile. No binding phrase or Wi-Fi credentials are included in the delivered image.

From `src/html`, build the checked-in headers with `npm ci` and `npm run build:all`. Windows needs a native compiler for node-zopfli-es. This prepared workspace used an ignored Python Zopfli bridge when MSVC was unavailable; it recompressed the four original ER6 WebUI assets byte-identically. Exact local preparation is recorded in `src/.pio/environment.md`. The tracked build tooling and dependencies are unchanged.

From `src`, with native-test-only flags cleared:

```powershell
$env:PYTHONPATH = "$PWD\python\external\esptool"
Remove-Item Env:PLATFORMIO_BUILD_FLAGS -ErrorAction SilentlyContinue
pio run -c platformio.er6.ini -e Unified_ESP32_2400_RX_via_UART
```

The configuration selects the exact ER6 layout, rather than producing a bare Unified ESP32 image. Verify the packaged output from the repository root:

```powershell
python tools/verify-er6-image.py artifacts/ELRS-4.1.0-ER6-FirmaSmart.bin
```

Native verification from `src` uses the prepared local compiler:

```powershell
$env:PATH = "$PWD\.pio\tools\mingw64\bin;" + $env:PATH
$env:PYTHONPATH = "$PWD\python\external\esptool"
$env:PLATFORMIO_BUILD_FLAGS = '-DRegulatory_Domain_ISM_2400 -mno-ms-bitfields'
pio test -e native
```

Run `node scripts/test-serial-options.mjs` from `src/html` for selector compatibility. Restore the original firmware with the ordinary Wi-Fi update flow and the official **standard ER6** 4.1 image; check binding, primary protocol and PWM settings afterward. Use the upstream UART recovery procedure if Wi-Fi is unavailable. Official future firmware updates remove this custom driver unless it is carried forward or accepted upstream.

This workstation's C: volume was nearly full, so build/dependency/temp files use the task-owned cache below. Set these process-local variables before native or firmware builds to reuse this prepared environment; firmware output is then under the D: build directory. No global settings were changed.

```powershell
$env:PLATFORMIO_BUILD_DIR = 'D:\CodexBuildCache\ELRS-FirmaSmart\build'
$env:PLATFORMIO_LIBDEPS_DIR = 'D:\CodexBuildCache\ELRS-FirmaSmart\libdeps'
$env:TEMP = 'D:\CodexBuildCache\ELRS-FirmaSmart\tmp'
$env:TMP = $env:TEMP
```

The verifier requires a clean committed checkout, the embedded source revision, and the entire hardware configuration from the pinned hardware revision. Metadata/provenance regressions run with `python tools/test_verify_er6_image.py`.

Sources: [ER6 specifications](https://radiomasterrc.com/products/er6-2-4ghz-elrs-pwm-receiver), [ELRS serial wiring/update documentation](https://www.expresslrs.org/software/serial-protocols/), [Spektrum SRXL2](https://github.com/SpektrumRC/SRXL2), [Firma manual](https://www.spektrumrc.com/ProdInfo/Files/SPMXSE1085-Instruction-Sheet-EN.pdf), [MSRC Smart decoding](https://github.com/dgatf/msrc/blob/v1.10/board/project/sensor/smart_esc.c).
