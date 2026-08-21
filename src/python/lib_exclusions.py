"""PlatformIO pre-script that adds target-aware `lib_ignore` entries.

PlatformIO's default LDF does not evaluate preprocessor branches, so it
discovers target-incompatible libraries behind inactive `#if`s. `chain+` or
`deep+` follows those branches but breaks external-library dependency
resolution. Keep the default mode and explicitly exclude incompatible
libraries here.

`lib_ignore` is read by PlatformIO's Library Dependency Finder, so mutate the
project config rather than `env["LIB_IGNORE"]`. Entries are exact library
names from PlatformIO's dependency graph and may identify project, global, or
`lib_deps` libraries.

Set "*" for every MCU of a TX/RX target type; its entries are combined with
that target type's MCU-specific entries. An ignored library is not selected or
built; its target must not include headers or symbols it provides.
"""

LIBRARY_EXCLUSIONS = {
    # target type -> "*" (all target MCUs) or MCU -> library names
    "TX": {
        "*": ("AnalogVbat","Baro","GYRO","MSPVTX","rx-crsf","ServoOutput","VTXSPI"),
        "esp32c3": ("GFX Library for Arduino","U8g2","GSENSOR","SCREEN","THERMAL"),
        "esp8285": ("GSENSOR","SCREEN","THERMAL"),
    },
    "RX": {
        "*": ("ADC","Backpack","BLE","GSENSOR","Handset","POWER_DETECT","SCREEN","tx-crsf","VTX"),
        "esp8285": ("MSPVTX","VTXSPI"),
    }
}


def get_target_type(target_name):
    target_parts = target_name.upper().split("_")
    return next((target for target in ("RX", "TX") if target in target_parts), None)


def get_excluded_libraries(target_type, mcu):
    target_exclusions = LIBRARY_EXCLUSIONS.get(target_type, {})
    return target_exclusions.get("*", ()) + target_exclusions.get(mcu, ())

Import("env")

target_type = get_target_type(env["PIOENV"])
excluded_libraries = ()
if target_type:
    mcu = env.BoardConfig().get("build.mcu", "").lower()
    excluded_libraries = get_excluded_libraries(target_type, mcu)

if excluded_libraries:
    config = env.GetProjectConfig()
    ignored_libraries = config.get("env:" + env["PIOENV"], "lib_ignore", [])
    config.set("env:" + env["PIOENV"], "lib_ignore", list(dict.fromkeys(ignored_libraries + list(excluded_libraries))))
    print("Ignoring libraries for %s/%s: %s" % (target_type, mcu, ", ".join(excluded_libraries)))
