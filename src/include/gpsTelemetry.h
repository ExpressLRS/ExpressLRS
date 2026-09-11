#pragma once

#include <stdint.h>

/***
 * A snapshot of the receiver-side GPS driver's live state, shared between the driver
 * (rx-serial/SerialGPS) and any observer such as the WebUI status page. Deliberately a plain POD
 * with no dependency on the serial or web stacks so it can live in the global include path.
 */
typedef struct
{
    uint8_t  state;             // gpsState_e: what the detect/configure state machine is doing
    uint32_t baud;              // baud rate currently locked on to
    bool     canConfigure;      // a TX line is wired, so the module can be talked to
    uint8_t  protocol;          // last message type parsed: 0 unknown, 1 NMEA, 2 UBX
    bool     ubxConfigured;     // the module accepted the UBX auto-configuration
    bool     usedValset;        // config method, only meaningful when ubxConfigured: true=VALSET, false=CFG-MSG
    uint16_t navIntervalMs;     // navigation interval the module was asked for, 0 if not yet configured
    uint16_t updateIntervalMs;  // measured interval between position frames, 0 until two have arrived
    uint8_t  satellites;        // satellites reported in the last fix
    uint8_t  fixType;           // 0/1 no fix, 2 = 2D, 3 = 3D (NMEA reports 3 for any fix)
    bool     fixValid;          // the module reports a usable position
    int32_t  lat;               // latitude in decimal degrees * 1e7
    int32_t  lon;               // longitude in decimal degrees * 1e7
    int32_t  altCm;             // altitude in cm
    int32_t  speedKmh100;       // ground speed in km/h * 100
    uint16_t heading100;        // heading in degrees * 100
    bool     timeValid;         // a resolved UTC date and time is available
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint32_t ageMs;             // time since the last valid frame from the module, 0xffffffff if none yet
} gps_telemetry_t;
