# SRXL2 tests and verification tools

Run these commands from `src/test`. The `test_srxl2` suite includes both the
protocol and receiver-adapter tests.

## Native tests

Requires PlatformIO and a native C++ compiler. On Linux/macOS:

```sh
export PYTHONPATH="$(pwd)/../python/external/esptool"
export PLATFORMIO_BUILD_FLAGS="-DRegulatory_Domain_ISM_2400"
pio test --project-dir .. -e native -f test_srxl2
```

On Windows PowerShell, use a current MinGW compiler on PATH:

```powershell
$env:PYTHONPATH = (Resolve-Path ../python/external/esptool).Path
$env:PLATFORMIO_BUILD_FLAGS = '-DRegulatory_Domain_ISM_2400 -mno-ms-bitfields'
pio test --project-dir .. -e native -f test_srxl2
```

If this checkout has a compiler in `src/.pio/tools/mingw64/bin`, add it first
with `$env:PATH = "$(Resolve-Path ../.pio/tools/mingw64/bin);$env:PATH"`.
Omit `-f test_srxl2` to run all native suites, including `test_binary_stream`.

## Regression checks

```sh
python test_serial_setup.py
python test_srxl2_config.py
python test_srxl2_platforms.py
python -m unittest test_verify_er6_image -v
node test-serial-options.mjs
```

The C++ probes use the checkout's MinGW compiler when available, otherwise g++
on PATH. The configuration check needs ArduinoJson from an existing ESP32
receiver build in `src/.pio/libdeps`. The platform check needs the installed
Arduino ESP32 SDK in `PLATFORMIO_CORE_DIR` (default: `~/.platformio`). Image
tests require `src/hardware`; the frontend test uses dependencies installed in
`src/html/node_modules` from the existing package lockfile.

## Firmware verification

```sh
python verify-er6-image.py /path/to/firmware.bin
python verify-srxl2-production.py /path/to/firmware.elf [port] [nm-path]
```

The ER6 verifier requires a clean checkout matching the image's embedded source
revision and pinned hardware revision. The production verifier defaults to
UART0 and the Windows ESP32 nm executable under `~/.platformio`; pass port 1
for UART1 and the appropriate nm executable for other targets or systems.
