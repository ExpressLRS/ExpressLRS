#include "SerialGPS.h"

#include "CRSFRouter.h"
#include <crsf_protocol.h>

SerialGPS *SerialGPS::_active = nullptr;

// UBX-NAV-PVT, truncated after pDOP. Protocol 15+ sends a 92 byte message and protocol 14 sends
// 84 bytes, both have an identical first 78 bytes so only those are described here.
typedef struct
{
    uint32_t iTOW;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t valid;
    uint32_t tAcc;
    int32_t nano;
    uint8_t fixType;
    uint8_t flags;
    uint8_t flags2;
    uint8_t numSV;
    int32_t lon;
    int32_t lat;
    int32_t height;
    int32_t hMSL;
    uint32_t hAcc;
    uint32_t vAcc;
    int32_t velN;
    int32_t velE;
    int32_t velD;
    int32_t gSpeed;
    int32_t headMot;
    uint32_t sAcc;
    uint32_t headAcc;
    uint16_t pDOP;
} PACKED ubx_nav_pvt_t;

#define UBX_SYNC1           0xb5
#define UBX_SYNC2           0x62

#define UBX_CLASS_NAV       0x01
#define UBX_NAV_PVT         0x07
#define UBX_CLASS_ACK       0x05
#define UBX_ACK_NAK         0x00
#define UBX_ACK_ACK         0x01
#define UBX_CLASS_CFG       0x06
#define UBX_CFG_PRT         0x00
#define UBX_CFG_MSG         0x01
#define UBX_CFG_RATE        0x08
#define UBX_CFG_VALSET      0x8a
#define UBX_CLASS_NMEA      0xf0

#define UBX_KEY_UART1_BAUD  0x40520001
#define UBX_KEY_RATE_MEAS   0x30210001

#define UBX_PVT_VALID_DATE  0x01
#define UBX_PVT_VALID_TIME  0x02
#define UBX_PVT_FLAGS_FIXOK 0x01

// Largest UBX message we accept before deciding we have sync'd on garbage
#define UBX_MAX_PAYLOAD     512

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

// Configuration keys for UBX-CFG-VALSET (u-blox generation 9 and later). All of these are
// single byte values, which keeps the frame builder trivial.
static const struct {
    uint32_t key;
    uint8_t val;
} UBX_VALSET_ITEMS[] = {
    { 0x20910007, 1 },  // CFG-MSGOUT-UBX_NAV_PVT_UART1  - everything we need, once per fix
    { 0x209100bb, 0 },  // CFG-MSGOUT-NMEA_ID_GGA_UART1
    { 0x209100ca, 0 },  // CFG-MSGOUT-NMEA_ID_GLL_UART1
    { 0x209100c0, 0 },  // CFG-MSGOUT-NMEA_ID_GSA_UART1
    { 0x209100c5, 0 },  // CFG-MSGOUT-NMEA_ID_GSV_UART1  - the bandwidth hog at 9600
    { 0x209100ac, 0 },  // CFG-MSGOUT-NMEA_ID_RMC_UART1
    { 0x209100b1, 0 },  // CFG-MSGOUT-NMEA_ID_VTG_UART1
};
static constexpr uint8_t UBX_VALSET_COUNT = sizeof(UBX_VALSET_ITEMS) / sizeof(UBX_VALSET_ITEMS[0]);

// NMEA standard message IDs, all in message class 0xf0, for the legacy UBX-CFG-MSG
static const uint8_t UBX_NMEA_MSGIDS[] = {
    0x00, // GGA
    0x01, // GLL
    0x02, // GSA
    0x03, // GSV
    0x04, // RMC
    0x05, // VTG
};

/***
* @brief Parses a decimal string with optional decimal point and returns the value scaled by the given factor as an integer
*   Ex: "0.442" with scale 100 returns 44
*   Ex: "123.456" with scale 1000 returns 123456
*/
static int32_t parseDecimalToScaled(const char* str, int32_t scale) {
    char *end;
    int32_t whole = strtol(str, &end, 10);
    int32_t result = whole * scale;

    if (*end == '.') {
        const char* dec = end + 1;
        int32_t divisor = 1;
        int32_t decimalPart = 0;

        // Count decimal places in scale
        int32_t scaleDecimals = 0;
        int32_t tempScale = scale;
        while (tempScale > 1) {
            scaleDecimals++;
            tempScale /= 10;
        }

        // Process up to scaleDecimals digits
        for (int i = 0; i < scaleDecimals && dec[i] != '\0'; i++) {
            decimalPart = decimalPart * 10 + (dec[i] - '0');
            divisor *= 10;
        }

        // Scale the decimal part
        if (divisor > 1) {
            while (divisor < scale) {
                decimalPart *= 10;
                divisor *= 10;
            }
            result += decimalPart;
        }
    }
    return result;
}

/***
 * @brief Parse NMEA Decimal Degrees Minutes value and return Decimal Degrees * 1e7.
 * @param field Points to beginning of DDMM[M] string, must not be blank!
 */
static int32_t nmeaDdmToDd(const char *field)
{
    // Latitude is DDMM.MMMMM, Longitude is DDDMM.MMMM
    // Start by getting the part before the decimal, and divide by 100 to remove the MM part
    int32_t degrees = atoi(field) / 100;

    // Minutes is always two digits. Find the decimal and look 2 characters before it
    // (const, because the C++ overload of strchr returns a const char* for a const argument)
    const char *minutes = strchr(field, '.');
    if (minutes == nullptr)
        return 0;
    int32_t minutesPart = parseDecimalToScaled(minutes - 2, 10000000) / 60;

    return degrees * 10000000 + minutesPart;
}

bool SerialGPS::isValidChecksum(char *sentence, uint8_t size)
{
    // Could also check for the \r\n but we know it at least has the \n to get here
    if (size < 6 || sentence[0] != '$' || sentence[size-5] != '*')
    {
        DBGLN("NMEA invalid");
        return false;
    }

    // Checksum in a NMEA packet starts after the $ and stops before the *XX and is a simple XOR
    uint8_t csumCalculated = 0;
    for (unsigned b=1; b<size-5; ++b)
    {
        csumCalculated ^= sentence[b];
    }

    uint8_t csumSentence = strtol((char *)&sentence[size-4], nullptr, 16);
    if (csumCalculated != csumSentence)
    {
        DBGLN("NMEA csum");
        return false;
    }

    return true;
}

void SerialGPS::splitSentenceFields(char *sentence, uint8_t size, gpsFieldParser_t callback)
{
    uint8_t fieldIdx = 0;
    char *fieldStart = sentence;

    //sentence[size] = 0; DBG(sentence);
    for (unsigned i=0; i<size; ++i)
    {
        if (sentence[i] == ',' || sentence[i] == '*')
        {
            sentence[i] = 0;
            callback(this, fieldIdx, fieldStart);
            fieldStart = &sentence[i+1];
            ++fieldIdx;
        }
    }
}

void SerialGPS::fieldParseGGA(SerialGPS *ctx, uint8_t fieldIdx, char *field)
{
    const bool blank = (field[0] == '\0');

    switch (fieldIdx)
    {
        case 2:
            ctx->gpsData.lat = (blank) ? 0 : nmeaDdmToDd(field);
            break;
        case 3:
            if (field[0] == 'S')
                ctx->gpsData.lat = -ctx->gpsData.lat;
            break;
        case 4:
            ctx->gpsData.lon = (blank) ? 0 : nmeaDdmToDd(field);
            break;
        case 5:
            if (field[0] == 'W')
                ctx->gpsData.lon = -ctx->gpsData.lon;
            break;
        case 6:
            // Fix quality, 0 means the position that came with it is meaningless. GGA does not
            // distinguish 2D from 3D, so any fix is reported as 3D for the status page.
            ctx->gpsData.fixValid = !blank && field[0] != '0';
            ctx->fixType = ctx->gpsData.fixValid ? 3 : 0;
            break;
        case 7:
            ctx->gpsData.satellites = atoi(field);
            break;
        case 9:
            ctx->gpsData.alt = (blank) ? 0 : parseDecimalToScaled(field, 100);;
            break;

    }
}

void SerialGPS::fieldParseVTG(SerialGPS *ctx, uint8_t fieldIdx, char *field)
{
    const bool blank = (field[0] == '\0');

    switch (fieldIdx)
    {
        case 1:
            ctx->gpsData.heading = (blank) ? 0 : parseDecimalToScaled(field, 100);
            break;
        case 7:
            ctx->gpsData.speed = (blank) ? 0 : parseDecimalToScaled(field, 100);
            break;
    }
}

void SerialGPS::fieldParseRMC(SerialGPS *ctx, uint8_t fieldIdx, char *field)
{
    const bool blank = (field[0] == '\0');
    if (blank) return;

    switch (fieldIdx) {
        case 1: // Time: HHMMSS.ss
        {
            uint32_t time_ms = parseDecimalToScaled(field, 1000);
            ctx->gpsData.millisecond = time_ms % 1000;
            time_ms /= 1000;
            ctx->gpsData.second = time_ms % 100;
            time_ms /= 100;
            ctx->gpsData.minute = time_ms % 100;
            time_ms /= 100;
            ctx->gpsData.hour = time_ms % 100;
            break;
        }
        case 9: // Date: DDMMYY
        {
            uint32_t date = atoi(field);
            ctx->gpsData.year = 2000 + date % 100;
            date /= 100;
            ctx->gpsData.month = date % 100;
            date /= 100;
            ctx->gpsData.day = date % 100;
            break;
        }
    }
}

void SerialGPS::processSentence(char *sentence, uint8_t size)
{
    detectedProtocol = 1;   // NMEA
    if (sentence[3] == 'G' && sentence[4] == 'G' && sentence[5] == 'A') {
        splitSentenceFields(sentence, size, &fieldParseGGA);
        if (!gpsData.fixValid)
        {
            // Some modules keep reporting the last known position with a fix quality of 0,
            // don't pass that off as the current position
            gpsData.lat = 0;
            gpsData.lon = 0;
            gpsData.alt = 0;
        }
        sendTelemetryFrame();
    }
    else if (sentence[3] == 'V' && sentence[4] == 'T' && sentence[5] == 'G') {
        splitSentenceFields(sentence, size, &fieldParseVTG);
        // VTG usually comes before GGA, only generate one telemetry frame with both combined
        //sendTelemetryFrame();
    }
    else if (sentence[3] == 'R' && sentence[4] == 'M' && sentence[5] == 'C') {
        splitSentenceFields(sentence, size, &fieldParseRMC);
        // Snapshot the wall clock for the status page before sendGpsTimeTelemetryFrame() clears it
        if (gpsData.year != 0)
        {
            utcValid = true;
            utcYear = gpsData.year;
            utcMonth = gpsData.month;
            utcDay = gpsData.day;
            utcHour = gpsData.hour;
            utcMinute = gpsData.minute;
            utcSecond = gpsData.second;
        }
        sendGpsTimeTelemetryFrame();
    }
    // Maybe we need to think about ZDA as well so we can adjust UTC to local time!
}

void SerialGPS::processNavPvt()
{
    const auto pvt = (ubx_nav_pvt_t *)buffer;

    lastPvtMs = millis();
    pvtSeen = true;
    detectedProtocol = 2;   // UBX
    fixType = pvt->fixType;
    // The module is doing what we asked of it, so any configuration retry budget is unspent
    configAttempts = 0;

    gpsData.satellites = pvt->numSV;
    gpsData.fixValid = pvt->fixType >= 2 && (pvt->flags & UBX_PVT_FLAGS_FIXOK);
    if (gpsData.fixValid)
    {
        // NAV-PVT is already in the 1e-7 degree units CRSF wants
        gpsData.lat = pvt->lat;
        gpsData.lon = pvt->lon;
        gpsData.alt = pvt->hMSL / 10;                // mm to cm
        gpsData.speed = (pvt->gSpeed * 36) / 100;    // mm/s to km/h * 100
        int32_t heading = pvt->headMot / 1000;       // 1e-5 degrees to 1e-2 degrees
        if (heading < 0)
            heading += 36000;
        gpsData.heading = heading;
    }
    else
    {
        gpsData.lat = 0;
        gpsData.lon = 0;
        gpsData.alt = 0;
        gpsData.speed = 0;
        gpsData.heading = 0;
    }
    sendTelemetryFrame();

    // Date and time become valid before there is a position fix, but only send them once the
    // module says they are resolved
    if ((pvt->valid & (UBX_PVT_VALID_DATE | UBX_PVT_VALID_TIME)) == (UBX_PVT_VALID_DATE | UBX_PVT_VALID_TIME))
    {
        gpsData.year = pvt->year;
        gpsData.month = pvt->month;
        gpsData.day = pvt->day;
        gpsData.hour = pvt->hour;
        gpsData.minute = pvt->min;
        gpsData.second = pvt->sec;
        // nano is the correction to apply to the second above, and can be negative
        gpsData.millisecond = (pvt->nano > 0) ? pvt->nano / 1000000 : 0;
        // Snapshot for the status page before sendGpsTimeTelemetryFrame() clears gpsData.year
        utcValid = true;
        utcYear = gpsData.year;
        utcMonth = gpsData.month;
        utcDay = gpsData.day;
        utcHour = gpsData.hour;
        utcMinute = gpsData.minute;
        utcSecond = gpsData.second;
        sendGpsTimeTelemetryFrame();
    }
}

void SerialGPS::processUbxMessage()
{
    if (ubxClass == UBX_CLASS_ACK && ubxLen >= 2)
    {
        if (ackState == gaPending && buffer[0] == ackClass && buffer[1] == ackId)
        {
            ackState = (ubxId == UBX_ACK_ACK) ? gaAck : gaNak;
        }
    }
    else if (ubxClass == UBX_CLASS_NAV && ubxId == UBX_NAV_PVT && ubxLen >= sizeof(ubx_nav_pvt_t))
    {
        processNavPvt();
    }
}

void SerialGPS::processBytes(uint8_t *bytes, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++)
    {
        const uint8_t c = bytes[i];

        switch (rxState)
        {
            case grIdle:
                if (c == '$')
                {
                    buffer[0] = c;
                    bufferIndex = 1;
                    rxState = grNmea;
                }
                else if (c == UBX_SYNC1)
                {
                    rxState = grUbxSync2;
                }
                break;

            case grNmea:
                if (bufferIndex >= sizeof(buffer))
                {
                    // Overlong sentence, resync on the next '$'
                    rxState = grIdle;
                    break;
                }
                // Note that the buffer/size includes the \r\n
                buffer[bufferIndex++] = c;
                if (c == '\n')
                {
                    if (isValidChecksum((char *)buffer, bufferIndex))
                    {
                        frameReceived();
                        processSentence((char *)buffer, bufferIndex);
                    }
                    rxState = grIdle;
                }
                break;

            case grUbxSync2:
                if (c == UBX_SYNC2)
                {
                    bufferIndex = 0;
                    rxState = grUbxHead;
                }
                else if (c != UBX_SYNC1)
                {
                    rxState = grIdle;
                }
                break;

            case grUbxHead:
                switch (bufferIndex)
                {
                    case 0: ubxClass = c; break;
                    case 1: ubxId = c; break;
                    case 2: ubxLen = c; break;
                    case 3: ubxLen |= (uint16_t)c << 8; break;
                }
                if (++bufferIndex == 4)
                {
                    if (ubxLen > UBX_MAX_PAYLOAD)
                    {
                        rxState = grIdle;
                        break;
                    }
                    ubxCkA = 0;
                    ubxCkB = 0;
                    ubxAccumulate(ubxClass);
                    ubxAccumulate(ubxId);
                    ubxAccumulate(ubxLen & 0xff);
                    ubxAccumulate(ubxLen >> 8);
                    bufferIndex = 0;
                    rxState = (ubxLen == 0) ? grUbxCkA : grUbxPayload;
                }
                break;

            case grUbxPayload:
                ubxAccumulate(c);
                // Messages larger than the buffer are still consumed and checksummed so the
                // parser does not resync in the middle of one, they are just not stored
                if (bufferIndex < sizeof(buffer))
                    buffer[bufferIndex] = c;
                if (++bufferIndex >= ubxLen)
                    rxState = grUbxCkA;
                break;

            case grUbxCkA:
                ubxCkARx = c;
                rxState = grUbxCkB;
                break;

            case grUbxCkB:
                if (ubxCkARx == ubxCkA && c == ubxCkB)
                {
                    frameReceived();
                    processUbxMessage();
                }
                else
                {
                    DBGLN("UBX csum");
                }
                rxState = grIdle;
                break;
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

void SerialGPS::queueUbx(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint8_t len)
{
    // Nothing is wired to the GPS RX line, so there is no point generating the message
    if (_txPin == UNDEF_PIN)
        return;

    // Big enough for the largest message this driver generates (CFG-VALSET)
    uint8_t frame[8 + 4 + UBX_VALSET_COUNT * 5];
    if ((unsigned)len + 8 > sizeof(frame))
        return;

    frame[0] = UBX_SYNC1;
    frame[1] = UBX_SYNC2;
    frame[2] = msgClass;
    frame[3] = msgId;
    frame[4] = len;
    frame[5] = 0;
    memcpy(&frame[6], payload, len);

    // Fletcher-8 over everything but the two sync bytes and the checksum itself
    uint8_t ckA = 0;
    uint8_t ckB = 0;
    for (uint8_t i = 2; i < len + 6; i++)
    {
        ckA += frame[i];
        ckB += ckA;
    }
    frame[len + 6] = ckA;
    frame[len + 7] = ckB;

    const uint8_t total = len + 8;
    _fifo.lock();
    if (_fifo.ensure(total + 1))
    {
        _fifo.push(total);
        _fifo.pushBytes(frame, total);
    }
    _fifo.unlock();
}

void SerialGPS::queueCfgMsg(uint8_t msgClass, uint8_t msgId, uint8_t rate)
{
    // The 3 byte form of CFG-MSG applies to the port the message arrived on, which is the one
    // we are talking to
    const uint8_t payload[3] = { msgClass, msgId, rate };
    queueUbx(UBX_CLASS_CFG, UBX_CFG_MSG, payload, sizeof(payload));
}

/***
 * @brief Write the 4 byte header of a CFG-VALSET payload, RAM layer only
 */
static uint8_t valsetHeader(uint8_t *payload)
{
    payload[0] = 0x00;  // version, no transaction
    payload[1] = 0x01;  // layers, RAM only
    payload[2] = 0x00;  // reserved
    payload[3] = 0x00;
    return 4;
}

/***
 * @brief Append a little endian configuration key id to a CFG-VALSET payload
 */
static uint8_t valsetKey(uint8_t *payload, uint32_t key)
{
    payload[0] = key & 0xff;
    payload[1] = (key >> 8) & 0xff;
    payload[2] = (key >> 16) & 0xff;
    payload[3] = (key >> 24) & 0xff;
    return 4;
}

void SerialGPS::queueNavRate(uint16_t measRateMs)
{
    navIntervalMs = measRateMs;
    if (usedValset)
    {
        uint8_t payload[4 + 4 + 2];
        uint8_t offset = valsetHeader(payload);
        offset += valsetKey(&payload[offset], UBX_KEY_RATE_MEAS);
        payload[offset++] = measRateMs & 0xff;
        payload[offset++] = measRateMs >> 8;
        queueUbx(UBX_CLASS_CFG, UBX_CFG_VALSET, payload, offset);
    }
    else
    {
        const uint8_t payload[6] = {
            (uint8_t)(measRateMs & 0xff), (uint8_t)(measRateMs >> 8),
            0x01, 0x00,     // one navigation solution per measurement
            0x01, 0x00,     // aligned to GPS time
        };
        queueUbx(UBX_CLASS_CFG, UBX_CFG_RATE, payload, sizeof(payload));
    }
}

void SerialGPS::queueBaudChange(uint32_t baud)
{
    if (usedValset)
    {
        uint8_t payload[4 + 4 + 4];
        uint8_t offset = valsetHeader(payload);
        offset += valsetKey(&payload[offset], UBX_KEY_UART1_BAUD);
        for (unsigned i = 0; i < 4; i++)
            payload[offset++] = (baud >> (8 * i)) & 0xff;
        queueUbx(UBX_CLASS_CFG, UBX_CFG_VALSET, payload, offset);
    }
    else
    {
        uint8_t payload[20] = {};
        payload[0] = 0x01;      // portID, UART1
        payload[4] = 0xd0;      // mode, 8N1
        payload[5] = 0x08;
        for (unsigned i = 0; i < 4; i++)
            payload[8 + i] = (baud >> (8 * i)) & 0xff;
        payload[12] = 0x07;     // inProtoMask, UBX + NMEA + RTCM
        payload[14] = 0x03;     // outProtoMask, UBX + NMEA
        queueUbx(UBX_CLASS_CFG, UBX_CFG_PRT, payload, sizeof(payload));
    }
}

void SerialGPS::expectAck(uint8_t msgClass, uint8_t msgId)
{
    ackClass = msgClass;
    ackId = msgId;
    ackState = gaPending;
}

void SerialGPS::disableNmeaOutput()
{
    for (unsigned i = 0; i < sizeof(UBX_NMEA_MSGIDS); i++)
    {
        queueCfgMsg(UBX_CLASS_NMEA, UBX_NMEA_MSGIDS[i], 0);
    }
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

    // Generation 9 and later. The whole transaction is applied or rejected as a unit, so an ACK
    // means NAV-PVT is on before any NMEA message was turned off. RAM layer only: the module
    // returns to its own defaults on the next power cycle and its flash is left alone.
    uint8_t payload[4 + UBX_VALSET_COUNT * 5];
    uint8_t offset = valsetHeader(payload);
    for (unsigned i = 0; i < UBX_VALSET_COUNT; i++)
    {
        offset += valsetKey(&payload[offset], UBX_VALSET_ITEMS[i].key);
        payload[offset++] = UBX_VALSET_ITEMS[i].val;
    }
    queueUbx(UBX_CLASS_CFG, UBX_CFG_VALSET, payload, offset);
    expectAck(UBX_CLASS_CFG, UBX_CFG_VALSET);

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
                // Generation 8 and earlier NAK VALSET. Enable NAV-PVT on its own and wait for
                // the ACK before turning any NMEA message off, so a module that does not know
                // NAV-PVT (protocol 14 and earlier) is not left with nothing to say.
                queueCfgMsg(UBX_CLASS_NAV, UBX_NAV_PVT, 1);
                expectAck(UBX_CLASS_CFG, UBX_CFG_MSG);
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
