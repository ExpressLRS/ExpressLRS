export const SERIAL_OPTIONS1 = ["CRSF", "Inverted CRSF", "SBUS", "Inverted SBUS", "SUMD", "DJI RS Pro", "HoTT Telemetry", "MAVLink", "DisplayPort", "GPS", "Scorpion Telemetry", "Spektrum Smart", "AirPort"]
export const SERIAL_OPTIONS2 = ["Off", "CRSF", "Inverted CRSF", "SBUS", "Inverted SBUS", "SUMD", "DJI RS Pro", "HoTT Telemetry", "IRC Tramp", "TBS SmartAudio", "DisplayPort", "GPS", "Scorpion Telemetry"]

export function primarySerialOptions(smartSupported) {
    return SERIAL_OPTIONS1.map((label, value) => ({label, value}))
        .filter(option => option.value !== 11 || smartSupported)
}
