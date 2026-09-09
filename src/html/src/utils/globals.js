export const SERIAL_OPTIONS1 = ["CRSF", "Inverted CRSF", "SBUS", "Inverted SBUS", "SUMD", "DJI RS Pro",
    // FEATURE:NOT IS_8285
    "HoTT Telemetry",
    // /FEATURE:NOT IS_8285
    // FEATURE:IS_8285
    null, // HoTT telemetry is not built for ESP8285, the placeholder keeps the values aligned with the enum
    // /FEATURE:IS_8285
    "MAVLink", "DisplayPort", "GPS", "AirPort"]
export const SERIAL_OPTIONS2 = ["Off", "CRSF", "Inverted CRSF", "SBUS", "Inverted SBUS", "SUMD", "DJI RS Pro", "HoTT Telemetry", "IRC Tramp", "TBS SmartAudio", "DisplayPort", "GPS"]
