/**
 * @brief The receiver-side GPS driver proper: baud rate auto-detection, the configure state
 * machine, and the CRSF telemetry frames.
 *
 * The two wire protocols live next door in SerialGPS-nmea.cpp and SerialGPS-ublox.cpp. This
 * unit decides what to say and when, they know how to say it and how to read the replies.
 */
#include "SerialGPS.h"

#include "CRSFRouter.h"
#include <crsf_protocol.h>

SerialGPS *SerialGPS::_active = nullptr;

// Baud rates to probe, most likely first. 115200 leads so a receiver that already works today
// (the driver was hard-coded to 115200) locks on to the first candidate and nothing regresses.
// 9600 is the u-blox M8 factory default and 38400 is the M9/M10 factory default.
static const uint32_t GPS_BAUD_CANDIDATES[] = { 115200, 9600, 38400, 57600, 230400, 19200 };
static constexpr uint8_t GPS_BAUD_COUNT = sizeof(GPS_BAUD_CANDIDATES) / sizeof(GPS_BAUD_CANDIDATES[0]);

// The baud rate a module gets moved to when its own is too slow for the target navigation rate.
// It is one of the candidates above, so a module that resets back to its factory defaults is
// still found by a re-probe.
static constexpr uint8_t GPS_FAST_BAUD_INDEX = 3;    // 57600

// A module configured for UBX only may emit a single message per second, so the dwell has to be
// long enough to see two of them
static constexpr uint32_t GPS_PROBE_DWELL_MS = 2500;
static constexpr uint8_t GPS_PROBE_FRAMES = 2;
static constexpr uint32_t GPS_SILENCE_MS = 5000;
// A u-blox does not guarantee a prompt CFG ACK: it may defer the ACK-ACK until the end of its
// current navigation epoch, which at the 1Hz factory default is up to a second away. Time out
// short of that and a slow module gets bounced to the legacy path (and, on an M9/M10 that rejects
// the legacy CFG-MSG, never gets its rate raised). u-blox's own tooling and the common SparkFun
// library wait 1100ms for this reason.
static constexpr uint32_t GPS_ACK_TIMEOUT_MS = 1100;
static constexpr uint8_t GPS_MAX_CONFIG_ATTEMPTS = 3;
static constexpr uint32_t GPS_TICK_MS = 100;

// Target navigation rate. A NAV-PVT message is 100 bytes on the wire, so 10Hz needs 1000 B/s,
// which is 10000bps of UART and more than a 9600 baud module can carry.
static constexpr uint16_t GPS_TARGET_RATE_MS = 100;
// Baud rate at or above which the target rate uses less than a third of the UART
static constexpr uint32_t GPS_FAST_RATE_BAUD = 38400;
// Time for the baud rate change request, and anything the module was mid-message on, to get out
static constexpr uint32_t GPS_BAUD_SETTLE_MS = 150;
// How long to wait for the module to start talking at the new baud rate before giving up on it
static constexpr uint32_t GPS_BAUD_VERIFY_MS = 2000;
// The date and time do not need the navigation rate
static constexpr uint32_t GPS_TIME_INTERVAL_MS = 1000;

/***
 * @brief Fastest navigation rate whose NAV-PVT output fits comfortably in the given baud rate
 * @return the measurement interval in milliseconds
 */
static uint16_t navRateForBaud(uint32_t baud)
{
    if (baud >= GPS_FAST_RATE_BAUD)
        return GPS_TARGET_RATE_MS;  // 10Hz, 26% of 38400
    if (baud >= 19200)
        return 200;                 // 5Hz, 26% of 19200
    return 500;                     // 2Hz, 21% of 9600
}

void SerialGPS::processBytes(uint8_t *bytes, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++)
    {
        const uint8_t c = bytes[i];

        // Both protocols are accepted on the same port, and the first byte of a message decides
        // which of the two parsers gets the stream until that message is complete
        if (rxState == grIdle)
        {
            if (!nmeaStart(c))
                ubxStart(c);
        }
        else if (rxState == grNmea)
        {
            nmeaByte(c);
        }
        else
        {
            ubxByte(c);
        }
    }
}

void SerialGPS::sendGpsTimeTelemetryFrame()
{
    // Don't send if there hasn't been an update to the year field
    if (gpsData.year == 0)
        return;

    // The wall clock does not need the navigation rate, and the position frames are the ones
    // worth spending the telemetry link on
    const uint32_t now = millis();
    if (timeFrameSent && now - lastTimeFrameMs < GPS_TIME_INTERVAL_MS)
        return;
    timeFrameSent = true;
    lastTimeFrameMs = now;

    CRSF_MK_FRAME_T(crsf_sensor_gps_time_t) crsftime{};
    crsftime.p.year = htobe16(gpsData.year);
    crsftime.p.month = gpsData.month;
    crsftime.p.day = gpsData.day;
    crsftime.p.hour = gpsData.hour;
    crsftime.p.minute = gpsData.minute;
    crsftime.p.second = gpsData.second;
    crsftime.p.millisecond = htobe16(gpsData.millisecond);
    crsfRouter.SetHeaderAndCrc(&crsftime.h, CRSF_FRAMETYPE_GPS_TIME, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_time_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsftime.h);

    gpsData.year = 0;
}

void SerialGPS::sendTelemetryFrame()
{
    // Measure how often a position frame actually goes out, which is the update rate the handset
    // sees. Smoothed with a light EWMA (alpha 1/4) so a single late frame does not swing it.
    const uint32_t nowMs = millis();
    if (lastPosFrameMs != 0)
    {
        const uint32_t dt = nowMs - lastPosFrameMs;
        posIntervalMs = (posIntervalMs == 0) ? (uint16_t)dt : (uint16_t)((posIntervalMs * 3 + dt) / 4);
    }
    lastPosFrameMs = nowMs;

    // CRSF altitude is metres with a 1000m offset, so it can represent -1000m to 64535m
    int32_t altitude = gpsData.alt / 100 + 1000;
    if (altitude < 0)
        altitude = 0;
    else if (altitude > UINT16_MAX)
        altitude = UINT16_MAX;

    // CRSF groundspeed is km/h * 10
    int32_t groundspeed = gpsData.speed / 10;
    if (groundspeed < 0)
        groundspeed = 0;
    else if (groundspeed > UINT16_MAX)
        groundspeed = UINT16_MAX;

    CRSF_MK_FRAME_T(crsf_sensor_gps_t) crsfgps{};
    crsfgps.p.latitude = htobe32(gpsData.lat);
    crsfgps.p.longitude = htobe32(gpsData.lon);
    crsfgps.p.altitude = htobe16((uint16_t)altitude);
    crsfgps.p.groundspeed = htobe16((uint16_t)groundspeed);
    crsfgps.p.satellites_in_use = gpsData.satellites;
    crsfgps.p.gps_heading = htobe16(gpsData.heading);
    crsfRouter.SetHeaderAndCrc(&crsfgps.h, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfgps.h);
}

uint32_t SerialGPS::currentBaud() const
{
    return GPS_BAUD_CANDIDATES[baudIndex];
}

void SerialGPS::setBaud(uint32_t baud)
{
    _port->updateBaudRate(baud);
    // Whatever was mid-flight through the old divisor is garbage
    while (_port->available())
        _port->read();
    _fifo.flush();
    rxState = grIdle;
    bufferIndex = 0;
    ackState = gaIdle;
    pvtSeen = false;
    // The measured update rate belonged to the old baud rate, don't carry it across
    lastPosFrameMs = 0;
    posIntervalMs = 0;
}

void SerialGPS::frameReceived()
{
    lastFrameMs = millis();
    if (state == gsProbing && goodFrames < GPS_PROBE_FRAMES)
        goodFrames++;
}

void SerialGPS::beginProbe()
{
    state = gsProbing;
    goodFrames = 0;
    // Nothing is known about the module again until it starts talking
    detectedProtocol = 0;
    fixType = 0;
    // A module that stopped talking may have reset back to its own defaults, in which case it
    // needs moving up to a usable baud rate again
    baudRaiseDone = false;
    stateChangedMs = millis();
    setBaud(currentBaud());
}

void SerialGPS::configComplete()
{
    configApplied = true;
    lastPvtMs = millis();
    // Ask for everything this baud rate can carry. If the baud rate can be raised below, this
    // gets superseded once the module is confirmed to have followed us up.
    queueNavRate(navRateForBaud(currentBaud()));

    if (currentBaud() < GPS_FAST_RATE_BAUD && !baudRaiseDone)
    {
        baudRaiseDone = true;
        DBGLN("GPS raising baud to %u", GPS_BAUD_CANDIDATES[GPS_FAST_BAUD_INDEX]);
        queueBaudChange(GPS_BAUD_CANDIDATES[GPS_FAST_BAUD_INDEX]);
        baudSwitched = false;
        state = gsCfgBaud;
    }
    else
    {
        state = gsRunning;
    }
    stateChangedMs = millis();
}

void SerialGPS::beginConfigure()
{
    // Without a TX line, or after repeatedly failing, just listen to whatever the module sends
    if (_txPin == UNDEF_PIN || configAttempts >= GPS_MAX_CONFIG_ATTEMPTS)
    {
        state = gsRunning;
        stateChangedMs = millis();
        return;
    }
    configAttempts++;

    // Start with the generation 9 and later message. Anything older NAKs it and gets the legacy
    // ladder from sendRCFrame() instead.
    queueCfgValset();

    state = gsCfgValset;
    stateChangedMs = millis();
}

uint32_t SerialGPS::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    const uint32_t now = millis();

    switch (state)
    {
        case gsProbing:
            if (goodFrames >= GPS_PROBE_FRAMES)
            {
                DBGLN("GPS found at %u baud", GPS_BAUD_CANDIDATES[baudIndex]);
                beginConfigure();
            }
            else if (now - stateChangedMs > GPS_PROBE_DWELL_MS)
            {
                baudIndex = (baudIndex + 1) % GPS_BAUD_COUNT;
                beginProbe();
            }
            break;

        case gsCfgValset:
            if (ackState == gaAck)
            {
                DBGLN("GPS configured for UBX (VALSET)");
                usedValset = true;
                configComplete();
            }
            else if (ackState == gaNak || now - stateChangedMs > GPS_ACK_TIMEOUT_MS)
            {
                // Generation 8 and earlier NAK VALSET, fall back to the legacy CFG-MSG
                queueCfgLegacy();
                state = gsCfgLegacy;
                stateChangedMs = now;
            }
            break;

        case gsCfgLegacy:
            if (ackState == gaAck)
            {
                DBGLN("GPS configured for UBX (CFG-MSG)");
                usedValset = false;
                disableNmeaOutput();
                configComplete();
            }
            else if (ackState == gaNak || now - stateChangedMs > GPS_ACK_TIMEOUT_MS)
            {
                // Not a u-blox, or too old for NAV-PVT. NMEA it is.
                DBGLN("GPS not configurable, using NMEA");
                state = gsRunning;
                stateChangedMs = now;
            }
            break;

        case gsCfgBaud:
            if (!baudSwitched)
            {
                if (now - stateChangedMs > GPS_BAUD_SETTLE_MS)
                {
                    // The request has to be all the way out of the UART before the divisor moves
                    _port->flush();
                    previousBaudIndex = baudIndex;
                    baudIndex = GPS_FAST_BAUD_INDEX;
                    setBaud(currentBaud());
                    baudSwitched = true;
                    stateChangedMs = now;
                }
            }
            else if (pvtSeen)
            {
                // The module followed us up, so it can carry the full navigation rate now
                DBGLN("GPS now at %u baud", currentBaud());
                queueNavRate(GPS_TARGET_RATE_MS);
                lastPvtMs = now;
                state = gsRunning;
                stateChangedMs = now;
            }
            else if (now - stateChangedMs > GPS_BAUD_VERIFY_MS)
            {
                // It did not take the change, go back to where it was talking to us. The
                // navigation rate it was given already suits that baud rate.
                DBGLN("GPS refused baud change");
                baudIndex = previousBaudIndex;
                setBaud(currentBaud());
                lastFrameMs = now;
                lastPvtMs = now;
                state = gsRunning;
                stateChangedMs = now;
            }
            break;

        case gsRunning:
            if (now - lastFrameMs > GPS_SILENCE_MS)
            {
                // Module unplugged, reset, or we locked on to noise. Resume the sweep where it
                // left off, the same baud rate is by far the most likely one to work again.
                DBGLN("GPS lost, probing");
                configApplied = false;
                beginProbe();
            }
            else if (configApplied && now - lastPvtMs > GPS_SILENCE_MS)
            {
                // Still talking, but no longer sending NAV-PVT, so it reset back to its defaults
                configApplied = false;
                beginConfigure();
            }
            break;
    }

    return GPS_TICK_MS;
}

bool SerialGPS::getTelemetryInfo(gps_telemetry_t &out)
{
    const SerialGPS *gps = _active;
    if (gps == nullptr)
        return false;

    out.state = gps->state;
    out.baud = gps->currentBaud();
    out.canConfigure = gps->_txPin != UNDEF_PIN;
    out.protocol = gps->detectedProtocol;
    out.ubxConfigured = gps->configApplied;
    out.usedValset = gps->usedValset;
    out.navIntervalMs = gps->navIntervalMs;
    out.updateIntervalMs = gps->posIntervalMs;

    out.satellites = gps->gpsData.satellites;
    out.fixType = gps->fixType;
    out.fixValid = gps->gpsData.fixValid;
    out.lat = gps->gpsData.lat;
    out.lon = gps->gpsData.lon;
    out.altCm = gps->gpsData.alt;
    out.speedKmh100 = gps->gpsData.speed;
    out.heading100 = gps->gpsData.heading;

    out.timeValid = gps->utcValid;
    out.year = gps->utcYear;
    out.month = gps->utcMonth;
    out.day = gps->utcDay;
    out.hour = gps->utcHour;
    out.minute = gps->utcMinute;
    out.second = gps->utcSecond;

    // lastFrameMs stays 0 until the first valid frame, don't report a bogus multi-day age for that
    out.ageMs = gps->lastFrameMs == 0 ? 0xffffffff : millis() - gps->lastFrameMs;
    return true;
}
