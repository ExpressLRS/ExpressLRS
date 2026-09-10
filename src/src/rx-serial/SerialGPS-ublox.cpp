/**
 * @brief The UBX half of the receiver-side GPS driver: message framing, NAV-PVT decoding, and
 * the configuration messages sent back to a u-blox module.
 *
 * SerialGPS::processBytes() demultiplexes the incoming stream and hands the bytes here once a
 * sync byte has started a message. The message buffer is shared with the NMEA parser, since
 * only one message is ever in flight.
 *
 * Nothing in here decides when to configure the module, that is the detect/configure state
 * machine in SerialGPS.cpp. This unit only knows how to say it.
 */
#include "SerialGPS.h"

#include "logging.h"
#include <crsf_protocol.h>   // PACKED

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
 * @brief Offered each byte while the receiver is between messages
 * @return true if the byte started a message, in which case the UBX parser has the stream
 */
bool SerialGPS::ubxStart(uint8_t c)
{
    if (c != UBX_SYNC1)
        return false;

    rxState = grUbxSync2;
    return true;
}

/***
 * @brief Frame a message, and act on it once its Fletcher-8 checksum has been verified
 */
void SerialGPS::ubxByte(uint8_t c)
{
    switch (rxState)
    {
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

        default:
            break;
    }
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

/***
 * @brief Ask a generation 9 or later module for NAV-PVT and no NMEA
 *
 * The whole transaction is applied or rejected as a unit, so an ACK means NAV-PVT is on before
 * any NMEA message was turned off. RAM layer only: the module returns to its own defaults on
 * the next power cycle and its flash is left alone.
 */
void SerialGPS::queueCfgValset()
{
    uint8_t payload[4 + UBX_VALSET_COUNT * 5];
    uint8_t offset = valsetHeader(payload);
    for (unsigned i = 0; i < UBX_VALSET_COUNT; i++)
    {
        offset += valsetKey(&payload[offset], UBX_VALSET_ITEMS[i].key);
        payload[offset++] = UBX_VALSET_ITEMS[i].val;
    }
    queueUbx(UBX_CLASS_CFG, UBX_CFG_VALSET, payload, offset);
    expectAck(UBX_CLASS_CFG, UBX_CFG_VALSET);
}

/***
 * @brief Ask a generation 8 or earlier module, which NAKs VALSET, for NAV-PVT
 *
 * Only the enable is sent here. Waiting for its ACK before turning any NMEA message off means a
 * module too old for NAV-PVT (protocol 14 and earlier) is never left with nothing to say, which
 * VALSET gets for free by being atomic.
 */
void SerialGPS::queueCfgLegacy()
{
    queueCfgMsg(UBX_CLASS_NAV, UBX_NAV_PVT, 1);
    expectAck(UBX_CLASS_CFG, UBX_CFG_MSG);
}
