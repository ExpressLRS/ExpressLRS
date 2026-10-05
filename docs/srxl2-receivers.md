# Spektrum Smart on ESP32, ESP32-C3, and ESP32-S3 receivers

This fork supports the **Spektrum Smart** primary serial protocol on these three MCU families when the receiver has a configured serial TX pin. It uses SRXL2 to control the Firma ESC from ELRS channel 3 and return supported ESC/Smart battery telemetry. On PWM receivers, steering and other servos continue to use their normal PWM outputs. XR1/XR2/XR3 have no built-in servo connectors; steering requires a separate serial-to-servo decoder/controller.

ESP8266/ESP8285, unknown ESP32 SoCs, and receivers without an available serial TX pin remain unsupported. The secondary UART does not offer Spektrum Smart.

## Receiver targets

| Receiver | MCU | Hardware target | PlatformIO environment | Primary TX / RX GPIO |
|---|---|---|---|---|
| RadioMaster XR1 | ESP32-C3 | `radiomaster.rx_dual.xr1` | `Unified_ESP32C3_LR1121_RX_via_UART` | 21 / 20 |
| RadioMaster XR2 | ESP32-C3 | `radiomaster.rx_2400.xr2` | `Unified_ESP32C3_LR1121_RX_via_UART` | 21 / 20 |
| RadioMaster XR3 | ESP32-C3 | `radiomaster.rx_dual.xr3` | `Unified_ESP32C3_LR1121_RX_via_UART` | 21 / 20 |
| RadioMaster ER12 | ESP32-S3 | `radiomaster.rx_dual.er12` | `Unified_ESP32S3_LR1121_RX_via_UART` | 43 / 44 |
| RadioMaster ER16 | ESP32-S3 | `radiomaster.rx_dual.er16` | `Unified_ESP32S3_LR1121_RX_via_UART` | 43 / 44 |

XR1/XR2/XR3 expose UART pads. ER12/ER16 use their dedicated UART/CRSF connector. Identify TX, ground, and supply from the receiver's manual; GPIO numbers are MCU pin assignments, not connector positions. Build and flash the image for the exact receiver. An ER6 image is not an ER12/ER16 image.

## Build and software verification

From the repository root, using the existing PlatformIO installation:

First select the appropriate RF region in `src/user_defines.txt`. The LR1121 environments require a domain even for the XR2: US validation builds use `-DRegulatory_Domain_FCC_915`. Match the transmitter's region; keep any binding phrase and Wi-Fi credentials out of shared firmware images.

```powershell
$env:ELRS_UNIFIED_CONFIG = 'radiomaster.rx_dual.xr1'
pio run -d src -e Unified_ESP32C3_LR1121_RX_via_UART
```

For another receiver, substitute both the hardware target and environment from the table. Use the normal `src/platformio.ini`; the `platformio.er6.ini` overlay hardcodes the ER6 target. The firmware appears in `src/.pio/build/<environment>/firmware.bin`. Copy it before building another receiver in the same environment, because that build replaces the embedded hardware definition. Remove `ELRS_UNIFIED_CONFIG` from the shell after building if subsequent builds should use interactive selection:

```powershell
Remove-Item Env:ELRS_UNIFIED_CONFIG
```

On Windows, a very long framework/package path can make the S3 compiler report `CreateProcess: No such file or directory` when its expanded command exceeds the system limit. A temporary short package-directory alias with `PLATFORMIO_PACKAGES_DIR` can resolve that build-environment issue; no firmware change is required.

The capability checks recognize only the three supported ESP32 SoCs. The driver reads the RX FIFO, physical bus level, and receive state machine before permitting traffic. ESP32 uses `status.st_urx_out`; C3/S3 use `fsm_status.st_urx_out`. All retain the 174 us idle guard and the existing UART transmit-idle check, which includes both the TX FIFO and shifter.

Run the platform probe after the ESP32 framework is installed:

```powershell
python tools/test_srxl2_platforms.py
python tools/test_serial_setup.py
pio test -d src -e native
node src/html/scripts/test-serial-options.mjs
npm --prefix src/html run check:syntax
npm --prefix src/html run check:lint
```

Native tests require a C++ compiler on PATH; this checkout also supports the existing `src/.pio/tools/mingw64/bin/g++.exe`. The platform probe uses actual register structures from the installed ESP32 SDK, exercising idle/busy behavior on all three chips and unsupported/missing-pin capability cases. The native protocol and adapter suites cover packet parsing, reply windows, pending/partial receives, telemetry, RF reset, neutral release, and output inhibition. These checks cannot measure a physical receiver's waveform.

Verified build artifacts and their target, chip ID, complete hardware layout, SHA-256, source revision, and hardware revision are recorded in `artifacts/srxl2-receivers/manifest.json`. Each image's physical validation status remains pending until the bench checks below are performed on that receiver.

## Wiring and configuration

1. Disconnect the motor or unload the drivetrain before commissioning. Power the receiver within its documented voltage range.
2. Connect the ESC's Smart signal to the receiver's **primary TX** pin and share ground. The driver receives and transmits on that one pin using open-drain output and UART0 GPIO routing. Leave the receiver's separate RX signal unconnected; do not join TX and RX.
3. Verify that the Smart signal's electrical levels are compatible with the receiver's 3.3 V GPIO. Scope the bus if its interface or pull-up voltage is uncertain; use appropriate level interfacing when required.
4. Configure the receiver's primary protocol as **Spektrum Smart**, with AirPort disabled. UART speed is set to 115200, 8N1 by the driver.
5. Map throttle to ELRS channel 3 and steering to the intended PWM output or external servo decoder. Start at neutral. A fresh neutral sample is required after startup, RF loss, inhibition, or ESC reconnect before motion is permitted again.
6. Use a transmitter running the matching ELRS major version. Bind and discover telemetry using the normal ELRS/radio workflow. Optional telemetry depends on the ESC and battery; absence of a Smart battery must not stop ESC control.

Battery capacity/percentage fields may be numeric zero placeholders when the ESC/battery does not provide them. Do not enable alarms based on those placeholders. No estimated consumed capacity is synthesized from motor current.

## Bench commissioning for each receiver

- Confirm pin identity and bus voltage before connecting. Check idle-high behavior and rise time with the actual harness; provide a suitable 3.3 V pull-up if measurements require one.
- With a logic analyzer, confirm discovery, the ESC handshake, and handshake completion at 115200 baud. Confirm the receiver releases the bus at idle and receives an immediate ESC reply on TX.
- Verify that transmission stops while a received byte is incomplete or the bus is low. Confirm the reply window starts after the final stop bit and that master/ESC traffic does not overlap.
- With the motor disconnected or drivetrain unloaded, verify exact neutral at startup, controlled forward/brake/reverse behavior, and channel 3 mapping. An already non-neutral input must not release motion after startup or reconnect.
- Compare available voltage, current, temperature, RPM, and Smart battery readings with expected values. Disconnect optional battery telemetry and confirm the ESC remains controllable; check stale readings expire.
- Test transmitter loss, model-match rejection, and any enabled team-race inhibition. The output must become neutral/failsafe. Reconnect while holding non-neutral throttle and verify motion remains blocked until a fresh neutral sample arrives.
- Reset/power-cycle the ESC independently, then repeat the reconnect/neutral test. Switch the receiver to another protocol and back, and confirm old throttle commands are not replayed.

Complete this on at least one C3 and one S3 unit to validate platform timing, and check each receiver model's electrical path before operating it. Record the receiver/ESC versions and results alongside its firmware checksum.

## Rollback

Keep the previous receiver firmware and configuration. To restore normal CRSF/PWM operation, select the previous primary serial protocol and reconnect the ESC/servos for that protocol, or flash the correct stock firmware for that exact receiver. Never force-flash another receiver's target to bypass a mismatch.
