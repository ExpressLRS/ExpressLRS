#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

#include "CRSFRouter.h"
#include "crsf_protocol.h"

// The driver is not part of a library, pull it (and its base class) into the test translation unit
#include "../../src/rx-serial/SerialIO.cpp"
#include "../../src/rx-serial/SerialGPS.cpp"

CRSFRouter crsfRouter;

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

/// @brief A UART the test can push GPS bytes into and read driver output back out of
class MockGpsPort final : public HardwareSerial
{
public:
    std::vector<uint8_t> rx;    // bytes the GPS module sends to the receiver
    std::vector<uint8_t> tx;    // bytes the receiver sends to the GPS module
    size_t rxPos = 0;
    unsigned long baud = 0;

    int available() override { return (int)(rx.size() - rxPos); }
    int read() override { return rxPos < rx.size() ? rx[rxPos++] : -1; }
    int peek() override { return rxPos < rx.size() ? rx[rxPos] : -1; }
    void flush() override {}
    size_t write(uint8_t c) override { tx.push_back(c); return 1; }
    size_t write(const uint8_t *s, size_t l) override { tx.insert(tx.end(), s, s + l); return l; }
    void updateBaudRate(unsigned long b) override { baud = b; }

    void feed(const std::string &s) { rx.insert(rx.end(), s.begin(), s.end()); }
    void feed(const std::vector<uint8_t> &b) { rx.insert(rx.end(), b.begin(), b.end()); }
};

/// @brief Captures the telemetry frames the driver hands to the router
class MockConnector final : public CRSFConnector
{
public:
    std::vector<std::vector<uint8_t>> frames;

    MockConnector() { addDevice(CRSF_ADDRESS_RADIO_TRANSMITTER); }

    void forwardMessage(const crsf_header_t *message) override
    {
        const uint8_t *raw = (const uint8_t *)message;
        frames.emplace_back(raw, raw + message->frame_size + 2);
    }

    /// @brief The payload of the last frame of the given type, or nullptr if there wasn't one
    const uint8_t *lastPayload(crsf_frame_type_e type) const
    {
        for (auto it = frames.rbegin(); it != frames.rend(); ++it)
        {
            if ((*it)[2] == type)
                return &(*it)[3];
        }
        return nullptr;
    }

    unsigned count(crsf_frame_type_e type) const
    {
        unsigned n = 0;
        for (const auto &f : frames)
            if (f[2] == type) n++;
        return n;
    }
};

static MockGpsPort *port;
static MockConnector *connector;
static SerialGPS *gps;

static void setupDriver(int8_t txPin = 4)
{
    delete gps;
    delete port;
    delete connector;
    nativeClockMs() = 0;
    port = new MockGpsPort();
    connector = new MockConnector();
    crsfRouter.addConnector(connector);
    gps = new SerialGPS(*port, txPin);
}

/// @brief Advance the clock and give the driver its state machine tick
static void tick(uint32_t advanceMs = 0)
{
    nativeClockMs() += advanceMs;
    gps->sendRCFrame(false, false, nullptr);
    gps->sendQueuedData(128);
}

static void teardownDriver()
{
    crsfRouter.removeConnector(connector);
}

/// @brief Push everything queued in the mock port through the driver, and anything the driver
/// generated back out to the mock port
static void pump()
{
    // processSerialInput() only takes getMaxSerialReadSize() bytes at a time
    while (port->available() > 0)
        gps->processSerialInput();
    gps->sendQueuedData(128);
}

// ---------------------------------------------------------------------------
// Message builders
// ---------------------------------------------------------------------------

/// @brief Wrap a NMEA sentence body (no leading '$') in the '$', checksum and CRLF
static std::string nmea(const std::string &body)
{
    uint8_t csum = 0;
    for (const char c : body)
        csum ^= (uint8_t)c;
    char tail[8];
    snprintf(tail, sizeof(tail), "*%02X\r\n", csum);
    return "$" + body + tail;
}

static std::vector<uint8_t> ubx(uint8_t msgClass, uint8_t msgId, const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> f = { 0xb5, 0x62, msgClass, msgId, (uint8_t)(payload.size() & 0xff), (uint8_t)(payload.size() >> 8) };
    f.insert(f.end(), payload.begin(), payload.end());
    uint8_t ckA = 0;
    uint8_t ckB = 0;
    for (size_t i = 2; i < f.size(); i++)
    {
        ckA += f[i];
        ckB += ckA;
    }
    f.push_back(ckA);
    f.push_back(ckB);
    return f;
}

static void put32(std::vector<uint8_t> &v, unsigned offset, int32_t value)
{
    for (unsigned i = 0; i < 4; i++)
        v[offset + i] = (uint8_t)((uint32_t)value >> (8 * i));
}

static void put16(std::vector<uint8_t> &v, unsigned offset, uint16_t value)
{
    v[offset] = value & 0xff;
    v[offset + 1] = value >> 8;
}

/// @brief Build a 92 byte UBX-NAV-PVT payload, laid out by the offsets in the u-blox interface
/// description rather than by the driver's own struct
static std::vector<uint8_t> navPvt(uint8_t fixType, uint8_t flags, uint8_t numSV,
                                   int32_t lon, int32_t lat, int32_t hMSL,
                                   int32_t gSpeed, int32_t headMot, uint8_t valid)
{
    std::vector<uint8_t> p(92, 0);
    put32(p, 0, 0x1234);        // iTOW
    put16(p, 4, 2025);          // year
    p[6] = 7;                   // month
    p[7] = 27;                  // day
    p[8] = 12;                  // hour
    p[9] = 34;                  // min
    p[10] = 56;                 // sec
    p[11] = valid;
    put32(p, 16, 250000000);    // nano, 250ms
    p[20] = fixType;
    p[21] = flags;
    p[23] = numSV;
    put32(p, 24, lon);
    put32(p, 28, lat);
    put32(p, 32, hMSL + 4000);  // height above ellipsoid, unused
    put32(p, 36, hMSL);
    put32(p, 60, gSpeed);
    put32(p, 64, headMot);
    return p;
}

static uint16_t be16(const uint8_t *p) { return ((uint16_t)p[0] << 8) | p[1]; }
static int32_t be32(const uint8_t *p)
{
    return ((int32_t)p[0] << 24) | ((int32_t)p[1] << 16) | ((int32_t)p[2] << 8) | p[3];
}

// The two reference fixes below are the same position expressed in both protocols:
// 48 07.038' N, 011 31.000' E at 545.4m, 10.2 km/h on a heading of 54.7 degrees
static constexpr int32_t REF_LAT = 481173000;
static constexpr int32_t REF_LON = 115166666;
static constexpr uint16_t REF_ALT_CRSF = 1545;      // 545m + 1000m offset
static constexpr uint16_t REF_HEADING_CRSF = 5470;  // 54.70 degrees

// ---------------------------------------------------------------------------
// NMEA
// ---------------------------------------------------------------------------

static void test_nmea_position(void)
{
    setupDriver();
    port->feed(nmea("GPVTG,054.7,T,034.4,M,005.5,N,010.2,K"));
    port->feed(nmea("GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    pump();

    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(p));
    TEST_ASSERT_EQUAL_INT32(REF_LON, be32(p + 4));
    TEST_ASSERT_EQUAL_UINT16(102, be16(p + 8));         // 10.2 km/h in 0.1 km/h units
    TEST_ASSERT_EQUAL_UINT16(REF_HEADING_CRSF, be16(p + 10));
    TEST_ASSERT_EQUAL_UINT16(REF_ALT_CRSF, be16(p + 12));
    TEST_ASSERT_EQUAL_UINT8(8, p[14]);
    // VTG must not generate a frame of its own, only GGA does
    TEST_ASSERT_EQUAL_UINT32(1, connector->count(CRSF_FRAMETYPE_GPS));
    teardownDriver();
}

static void test_nmea_southwest_is_negative(void)
{
    setupDriver();
    port->feed(nmea("GPGGA,123519.00,4807.038,S,01131.000,W,1,08,0.9,545.4,M,46.9,M,,"));
    pump();

    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32(-REF_LAT, be32(p));
    TEST_ASSERT_EQUAL_INT32(-REF_LON, be32(p + 4));
    teardownDriver();
}

static void test_nmea_no_fix_zeroes_position(void)
{
    setupDriver();
    // Fix quality 0, but the module is still reporting the last position it knew
    port->feed(nmea("GPGGA,123519.00,4807.038,N,01131.000,E,0,04,0.9,545.4,M,46.9,M,,"));
    pump();

    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32(0, be32(p));
    TEST_ASSERT_EQUAL_INT32(0, be32(p + 4));
    TEST_ASSERT_EQUAL_UINT16(1000, be16(p + 12));   // 0m, which is the 1000m offset
    // ... but the satellite count still gets through so the user can watch it climb
    TEST_ASSERT_EQUAL_UINT8(4, p[14]);
    teardownDriver();
}

static void test_nmea_time(void)
{
    setupDriver();
    port->feed(nmea("GPRMC,123519.25,A,4807.038,N,01131.000,E,022.4,084.4,270725,003.1,W"));
    pump();

    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS_TIME);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT16(2025, be16(p));
    TEST_ASSERT_EQUAL_UINT8(7, p[2]);
    TEST_ASSERT_EQUAL_UINT8(27, p[3]);
    TEST_ASSERT_EQUAL_UINT8(12, p[4]);
    TEST_ASSERT_EQUAL_UINT8(35, p[5]);
    TEST_ASSERT_EQUAL_UINT8(19, p[6]);
    TEST_ASSERT_EQUAL_UINT16(250, be16(p + 7));
    teardownDriver();
}

static void test_nmea_bad_checksum_ignored(void)
{
    setupDriver();
    std::string sentence = nmea("GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    sentence[10] = (sentence[10] == '0') ? '1' : '0';   // corrupt the body, not the checksum
    port->feed(sentence);
    pump();

    TEST_ASSERT_EQUAL_UINT32(0, connector->frames.size());
    teardownDriver();
}

// ---------------------------------------------------------------------------
// UBX
// ---------------------------------------------------------------------------

static void test_ubx_navpvt_position(void)
{
    setupDriver();
    // 2833 mm/s is 10.1988 km/h, the same fix as the NMEA test above
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    pump();

    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(p));
    TEST_ASSERT_EQUAL_INT32(REF_LON, be32(p + 4));
    TEST_ASSERT_EQUAL_UINT16(101, be16(p + 8));
    TEST_ASSERT_EQUAL_UINT16(REF_HEADING_CRSF, be16(p + 10));
    TEST_ASSERT_EQUAL_UINT16(REF_ALT_CRSF, be16(p + 12));
    TEST_ASSERT_EQUAL_UINT8(11, p[14]);

    const uint8_t *t = connector->lastPayload(CRSF_FRAMETYPE_GPS_TIME);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL_UINT16(2025, be16(t));
    TEST_ASSERT_EQUAL_UINT8(7, t[2]);
    TEST_ASSERT_EQUAL_UINT8(27, t[3]);
    TEST_ASSERT_EQUAL_UINT8(12, t[4]);
    TEST_ASSERT_EQUAL_UINT8(34, t[5]);
    TEST_ASSERT_EQUAL_UINT8(56, t[6]);
    TEST_ASSERT_EQUAL_UINT16(250, be16(t + 7));
    teardownDriver();
}

static void test_ubx_navpvt_negative_heading_wraps(void)
{
    setupDriver();
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 0, -9000000, 0x03)));
    pump();

    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT16(27000, be16(p + 10));   // -90 degrees is 270 degrees
    teardownDriver();
}

static void test_ubx_navpvt_no_fix(void)
{
    setupDriver();
    // Fix type 0 with the position fields still populated, and the time not yet resolved
    port->feed(ubx(0x01, 0x07, navPvt(0, 0x00, 5, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x00)));
    pump();

    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32(0, be32(p));
    TEST_ASSERT_EQUAL_INT32(0, be32(p + 4));
    TEST_ASSERT_EQUAL_UINT16(0, be16(p + 8));
    TEST_ASSERT_EQUAL_UINT16(1000, be16(p + 12));
    TEST_ASSERT_EQUAL_UINT8(5, p[14]);
    TEST_ASSERT_EQUAL_UINT32(0, connector->count(CRSF_FRAMETYPE_GPS_TIME));
    teardownDriver();
}

static void test_ubx_bad_checksum_ignored(void)
{
    setupDriver();
    std::vector<uint8_t> f = ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03));
    f[f.size() - 1] ^= 0xff;
    port->feed(f);
    pump();

    TEST_ASSERT_EQUAL_UINT32(0, connector->frames.size());
    teardownDriver();
}

static void test_ubx_oversized_message_does_not_desync(void)
{
    setupDriver();
    // MON-VER is far larger than the driver's buffer, it has to be consumed rather than skipped
    port->feed(ubx(0x0a, 0x04, std::vector<uint8_t>(220, 0x41)));
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    pump();

    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(p));
    teardownDriver();
}

static void test_ubx_and_nmea_interleaved(void)
{
    setupDriver();
    port->feed(nmea("GPRMC,123519.25,A,4807.038,N,01131.000,E,022.4,084.4,270725,003.1,W"));
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    port->feed(nmea("GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    pump();

    TEST_ASSERT_EQUAL_UINT32(2, connector->count(CRSF_FRAMETYPE_GPS));
    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(p));
    teardownDriver();
}

// ---------------------------------------------------------------------------
// Baud detection and auto-configuration
// ---------------------------------------------------------------------------

static const char *GGA = "GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,";

/// @brief Feed enough valid traffic for the driver to lock on to the baud rate, then tick it
static void lockOn()
{
    port->feed(nmea(GGA));
    port->feed(nmea(GGA));
    pump();
    tick();
}

/// @brief Let the probe sweep on to a given baud rate candidate, then lock on there
static void lockOnAt(uint32_t baud)
{
    for (unsigned i = 0; i < GPS_BAUD_COUNT && port->baud != baud; i++)
        tick(GPS_PROBE_DWELL_MS + 100);
    TEST_ASSERT_EQUAL_UINT32(baud, port->baud);
    lockOn();
}

/// @brief Verify a CFG-VALSET frame carrying exactly one key, and return the value it sets
static uint32_t valsetValue(const std::vector<uint8_t> &f, uint32_t expectedKey, uint8_t valueSize)
{
    TEST_ASSERT_EQUAL_UINT8(0x06, f[2]);
    TEST_ASSERT_EQUAL_UINT8(0x8a, f[3]);
    TEST_ASSERT_EQUAL_UINT8(4 + 4 + valueSize, f[4]);
    TEST_ASSERT_EQUAL_UINT8(0x01, f[7]);        // RAM layer
    for (unsigned i = 0; i < 4; i++)
        TEST_ASSERT_EQUAL_UINT8((expectedKey >> (8 * i)) & 0xff, f[10 + i]);
    uint32_t value = 0;
    for (unsigned i = 0; i < valueSize; i++)
        value |= (uint32_t)f[14 + i] << (8 * i);
    return value;
}

/// @brief Split the driver's output into UBX frames, verifying the framing and checksum of each
static std::vector<std::vector<uint8_t>> parseTx()
{
    std::vector<std::vector<uint8_t>> out;
    const std::vector<uint8_t> &tx = port->tx;
    size_t i = 0;
    while (i + 8 <= tx.size())
    {
        TEST_ASSERT_EQUAL_UINT8(0xb5, tx[i]);
        TEST_ASSERT_EQUAL_UINT8(0x62, tx[i + 1]);
        const uint16_t len = tx[i + 4] | ((uint16_t)tx[i + 5] << 8);
        TEST_ASSERT_TRUE(i + 8 + len <= tx.size());
        uint8_t ckA = 0;
        uint8_t ckB = 0;
        for (size_t b = i + 2; b < i + 6 + len; b++)
        {
            ckA += tx[b];
            ckB += ckA;
        }
        TEST_ASSERT_EQUAL_UINT8(ckA, tx[i + 6 + len]);
        TEST_ASSERT_EQUAL_UINT8(ckB, tx[i + 7 + len]);
        out.emplace_back(tx.begin() + i, tx.begin() + i + 8 + len);
        i += 8 + len;
    }
    TEST_ASSERT_EQUAL_UINT32(tx.size(), i);
    return out;
}

static void test_nothing_is_sent_before_lock(void)
{
    setupDriver();
    // One good sentence is not enough to be sure of the baud rate
    port->feed(nmea(GGA));
    pump();
    tick();

    TEST_ASSERT_EQUAL_UINT32(0, port->tx.size());
    teardownDriver();
}

static void test_probe_sweeps_baud_rates(void)
{
    setupDriver();
    TEST_ASSERT_EQUAL_UINT32(115200, port->baud);
    tick(GPS_PROBE_DWELL_MS + 100);
    TEST_ASSERT_EQUAL_UINT32(9600, port->baud);
    tick(GPS_PROBE_DWELL_MS + 100);
    TEST_ASSERT_EQUAL_UINT32(38400, port->baud);
    // and it does not move on once something valid is heard
    lockOn();
    tick(GPS_PROBE_DWELL_MS + 100);
    TEST_ASSERT_EQUAL_UINT32(38400, port->baud);
    teardownDriver();
}

static void test_valset_sent_after_lock(void)
{
    setupDriver();
    lockOn();

    const auto frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    const auto &f = frames[0];
    TEST_ASSERT_EQUAL_UINT8(0x06, f[2]);            // CFG
    TEST_ASSERT_EQUAL_UINT8(0x8a, f[3]);            // VALSET
    TEST_ASSERT_EQUAL_UINT8(4 + 7 * 5, f[4]);       // header plus seven single byte keys
    TEST_ASSERT_EQUAL_UINT8(0x00, f[6]);            // version
    TEST_ASSERT_EQUAL_UINT8(0x01, f[7]);            // RAM layer only, never flash
    // First key must be CFG-MSGOUT-UBX_NAV_PVT_UART1 = 1
    TEST_ASSERT_EQUAL_UINT8(0x07, f[10]);
    TEST_ASSERT_EQUAL_UINT8(0x00, f[11]);
    TEST_ASSERT_EQUAL_UINT8(0x91, f[12]);
    TEST_ASSERT_EQUAL_UINT8(0x20, f[13]);
    TEST_ASSERT_EQUAL_UINT8(1, f[14]);
    // Every remaining key turns a NMEA sentence off
    for (unsigned k = 1; k < 7; k++)
        TEST_ASSERT_EQUAL_UINT8(0, f[14 + k * 5]);
    teardownDriver();
}

static void test_valset_ack_requests_10hz(void)
{
    setupDriver();
    lockOn();
    port->tx.clear();

    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));    // ACK-ACK of CFG-VALSET
    pump();
    tick();

    // 115200 carries 10Hz, so the rate is the only thing left to ask for
    const auto frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_EQUAL_UINT32(100, valsetValue(frames[0], 0x30210001, 2));
    teardownDriver();
}

static void test_valset_nak_falls_back_to_legacy(void)
{
    setupDriver();
    lockOn();
    port->tx.clear();

    port->feed(ubx(0x05, 0x00, { 0x06, 0x8a }));    // ACK-NAK of CFG-VALSET
    pump();
    tick();

    // NAV-PVT is enabled on its own first and nothing is turned off yet
    auto frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_EQUAL_UINT8(0x06, frames[0][2]);    // CFG
    TEST_ASSERT_EQUAL_UINT8(0x01, frames[0][3]);    // MSG
    TEST_ASSERT_EQUAL_UINT8(0x01, frames[0][6]);    // NAV
    TEST_ASSERT_EQUAL_UINT8(0x07, frames[0][7]);    // PVT
    TEST_ASSERT_EQUAL_UINT8(0x01, frames[0][8]);    // once per solution

    // Only once that is acknowledged are the NMEA sentences turned off and the rate raised
    port->tx.clear();
    port->feed(ubx(0x05, 0x01, { 0x06, 0x01 }));
    pump();
    tick();

    frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(7, frames.size());
    for (unsigned i = 0; i < 6; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0x06, frames[i][2]);
        TEST_ASSERT_EQUAL_UINT8(0x01, frames[i][3]);    // CFG-MSG
        TEST_ASSERT_EQUAL_UINT8(0xf0, frames[i][6]);    // NMEA message class
        TEST_ASSERT_EQUAL_UINT8(0x00, frames[i][8]);    // rate 0, off
    }
    // CFG-RATE, 100ms between measurements, one solution each, aligned to GPS time
    TEST_ASSERT_EQUAL_UINT8(0x06, frames[6][2]);
    TEST_ASSERT_EQUAL_UINT8(0x08, frames[6][3]);
    TEST_ASSERT_EQUAL_UINT8(6, frames[6][4]);
    TEST_ASSERT_EQUAL_UINT16(100, frames[6][6] | (frames[6][7] << 8));
    TEST_ASSERT_EQUAL_UINT8(1, frames[6][8]);
    TEST_ASSERT_EQUAL_UINT8(1, frames[6][10]);
    teardownDriver();
}

static void test_slow_module_is_moved_to_a_faster_baud(void)
{
    setupDriver();
    lockOnAt(9600);
    port->tx.clear();

    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));
    pump();
    tick();

    // 9600 cannot carry 10Hz, so it gets what fits now plus a request to move up
    auto frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(2, frames.size());
    TEST_ASSERT_EQUAL_UINT32(500, valsetValue(frames[0], 0x30210001, 2));
    TEST_ASSERT_EQUAL_UINT32(57600, valsetValue(frames[1], 0x40520001, 4));

    // The receiver only moves once the request has had time to get out of the UART
    TEST_ASSERT_EQUAL_UINT32(9600, port->baud);
    tick(GPS_BAUD_SETTLE_MS + 10);
    TEST_ASSERT_EQUAL_UINT32(57600, port->baud);

    // Once the module is heard at the new rate, the full rate is requested
    port->tx.clear();
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    pump();
    tick();

    frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_EQUAL_UINT32(100, valsetValue(frames[0], 0x30210001, 2));
    teardownDriver();
}

static void test_baud_change_is_reverted_if_module_does_not_follow(void)
{
    setupDriver();
    lockOnAt(9600);
    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));
    pump();
    tick();
    tick(GPS_BAUD_SETTLE_MS + 10);
    TEST_ASSERT_EQUAL_UINT32(57600, port->baud);
    port->tx.clear();

    // Nothing is heard at the new baud rate, so the receiver goes back to where it was
    tick(GPS_BAUD_VERIFY_MS + 100);
    TEST_ASSERT_EQUAL_UINT32(9600, port->baud);
    TEST_ASSERT_EQUAL_UINT32(0, port->tx.size());

    // and the module is still understood there, at the rate 9600 can carry
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    pump();
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(connector->lastPayload(CRSF_FRAMETYPE_GPS)));
    teardownDriver();
}

static void test_legacy_baud_change_uses_cfg_prt(void)
{
    setupDriver();
    lockOnAt(9600);
    port->feed(ubx(0x05, 0x00, { 0x06, 0x8a }));    // NAK the VALSET
    pump();
    tick();
    port->tx.clear();
    port->feed(ubx(0x05, 0x01, { 0x06, 0x01 }));    // ACK the legacy NAV-PVT enable
    pump();
    tick();

    const auto frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(8, frames.size());     // six NMEA off, CFG-RATE, CFG-PRT
    TEST_ASSERT_EQUAL_UINT16(500, frames[6][6] | (frames[6][7] << 8));
    const auto &prt = frames[7];
    TEST_ASSERT_EQUAL_UINT8(0x06, prt[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, prt[3]);          // CFG-PRT
    TEST_ASSERT_EQUAL_UINT8(20, prt[4]);
    TEST_ASSERT_EQUAL_UINT8(0x01, prt[6]);          // UART1
    TEST_ASSERT_EQUAL_UINT8(0xd0, prt[10]);         // 8N1
    TEST_ASSERT_EQUAL_UINT8(0x08, prt[11]);
    uint32_t baud = 0;
    for (unsigned i = 0; i < 4; i++)
        baud |= (uint32_t)prt[14 + i] << (8 * i);
    TEST_ASSERT_EQUAL_UINT32(57600, baud);
    TEST_ASSERT_EQUAL_UINT8(0x07, prt[18]);         // still accepts UBX and NMEA in
    TEST_ASSERT_EQUAL_UINT8(0x03, prt[20]);         // and out
    teardownDriver();
}

static void test_gps_time_is_rate_limited(void)
{
    setupDriver();
    const auto pvt = ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03));
    for (unsigned i = 0; i < 10; i++)
    {
        nativeClockMs() += 100;
        port->feed(pvt);
        pump();
    }

    // Position at the full rate, but the wall clock only once a second
    TEST_ASSERT_EQUAL_UINT32(10, connector->count(CRSF_FRAMETYPE_GPS));
    TEST_ASSERT_EQUAL_UINT32(1, connector->count(CRSF_FRAMETYPE_GPS_TIME));
    nativeClockMs() += 1000;
    port->feed(pvt);
    pump();
    TEST_ASSERT_EQUAL_UINT32(2, connector->count(CRSF_FRAMETYPE_GPS_TIME));
    teardownDriver();
}

static void test_non_ublox_is_left_alone(void)
{
    setupDriver();
    lockOn();

    port->feed(ubx(0x05, 0x00, { 0x06, 0x8a }));    // NAK the VALSET
    pump();
    tick();
    port->tx.clear();

    port->feed(ubx(0x05, 0x00, { 0x06, 0x01 }));    // and NAK the legacy CFG-MSG
    pump();
    for (unsigned i = 0; i < 10; i++)
        tick();

    // Nothing more is sent, and NMEA parsing carries on working
    TEST_ASSERT_EQUAL_UINT32(0, port->tx.size());
    port->feed(nmea(GGA));
    pump();
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(connector->lastPayload(CRSF_FRAMETYPE_GPS)));
    teardownDriver();
}

static void test_no_tx_pin_never_writes(void)
{
    setupDriver(UNDEF_PIN);
    lockOn();
    for (unsigned i = 0; i < 10; i++)
        tick();

    TEST_ASSERT_EQUAL_UINT32(0, port->tx.size());
    // and it still parses whatever the module sends
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    pump();
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(connector->lastPayload(CRSF_FRAMETYPE_GPS)));
    teardownDriver();
}

// ---------------------------------------------------------------------------
// Adversarial / edge cases
// ---------------------------------------------------------------------------

/// @brief Feed a byte stream one byte per processSerialInput call, to prove the UBX/NMEA state
/// machine survives arbitrary fragmentation across reads
static void feedByteByByte(const std::vector<uint8_t> &b)
{
    for (uint8_t x : b)
    {
        port->rx.push_back(x);
        gps->processSerialInput();
    }
    gps->sendQueuedData(128);
}

static std::vector<uint8_t> navPvtLen(unsigned len)
{
    auto p = navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03);
    p.resize(len);   // truncate to a shorter protocol version's length
    return p;
}

// The driver's ubx_nav_pvt_t is truncated at pDOP (78 bytes). A real NAV-PVT is 84 bytes
// (protocol 14) or 92 (protocol 15+); both must be accepted, and everything we read lives in the
// first 68 bytes. Anything shorter than the struct must be ignored, not read out of bounds.
static void test_ubx_navpvt_84_byte_protocol14_accepted(void)
{
    setupDriver();
    port->feed(ubx(0x01, 0x07, navPvtLen(84)));
    pump();
    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(p));
    TEST_ASSERT_EQUAL_UINT16(REF_HEADING_CRSF, be16(p + 10));
    teardownDriver();
}

static void test_ubx_navpvt_78_byte_exact_accepted(void)
{
    setupDriver();
    port->feed(ubx(0x01, 0x07, navPvtLen(78)));
    pump();
    TEST_ASSERT_NOT_NULL(connector->lastPayload(CRSF_FRAMETYPE_GPS));
    teardownDriver();
}

static void test_ubx_navpvt_too_short_ignored(void)
{
    setupDriver();
    port->feed(ubx(0x01, 0x07, navPvtLen(77)));   // one below the struct size
    pump();
    // No telemetry, no crash, no out-of-bounds read
    TEST_ASSERT_EQUAL_UINT32(0, connector->count(CRSF_FRAMETYPE_GPS));
    teardownDriver();
}

static void test_ubx_fragmented_one_byte_per_read(void)
{
    setupDriver();
    feedByteByByte(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    const uint8_t *p = connector->lastPayload(CRSF_FRAMETYPE_GPS);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32(REF_LAT, be32(p));
    teardownDriver();
}

static void test_nmea_fragmented_one_byte_per_read(void)
{
    setupDriver();
    std::string s = nmea(GGA);
    feedByteByByte(std::vector<uint8_t>(s.begin(), s.end()));
    TEST_ASSERT_NOT_NULL(connector->lastPayload(CRSF_FRAMETYPE_GPS));
    teardownDriver();
}

static void test_stray_sync1_then_garbage_resyncs(void)
{
    setupDriver();
    // 0xB5 not followed by 0x62, then junk, then a real message: parser must recover
    port->feed(std::vector<uint8_t>{0xb5, 0x41, 0x42, 0xb5, 0x00, 0x11});
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    pump();
    TEST_ASSERT_NOT_NULL(connector->lastPayload(CRSF_FRAMETYPE_GPS));
    teardownDriver();
}

static void test_double_sync1_handled(void)
{
    setupDriver();
    // 0xB5 0xB5 0x62 ... — a repeated sync byte must still frame correctly
    std::vector<uint8_t> f = ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03));
    f.insert(f.begin(), 0xb5);
    port->feed(f);
    pump();
    TEST_ASSERT_NOT_NULL(connector->lastPayload(CRSF_FRAMETYPE_GPS));
    teardownDriver();
}

static void test_navrate_38400_is_10hz_no_raise(void)
{
    setupDriver();
    lockOnAt(38400);
    port->tx.clear();
    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));   // ACK the VALSET
    pump();
    tick();
    // 38400 is exactly the "fast enough" threshold: request 10Hz, and do NOT raise the baud
    auto frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_EQUAL_UINT32(100, valsetValue(frames[0], 0x30210001, 2));
    tick(GPS_BAUD_SETTLE_MS + 10);
    TEST_ASSERT_EQUAL_UINT32(38400, port->baud);   // unchanged
    teardownDriver();
}

static void test_navrate_19200_is_5hz_then_raises(void)
{
    setupDriver();
    lockOnAt(19200);
    port->tx.clear();
    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));
    pump();
    tick();
    auto frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(2, frames.size());
    TEST_ASSERT_EQUAL_UINT32(200, valsetValue(frames[0], 0x30210001, 2));   // 5Hz
    TEST_ASSERT_EQUAL_UINT32(57600, valsetValue(frames[1], 0x40520001, 4)); // raise
    teardownDriver();
}

static void test_valset_slow_ack_still_accepted(void)
{
    setupDriver();
    lockOn();
    port->tx.clear();
    // A u-blox at 1Hz can take most of a second to ACK. Advance well past the old 500ms timeout
    // but within the real window; the driver must still be waiting, not already on the legacy path.
    tick(900);
    TEST_ASSERT_EQUAL_UINT32(0, port->tx.size());   // no legacy CFG-MSG sent yet
    // Now the delayed ACK arrives -> VALSET path completes and the 10Hz rate is requested
    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));
    pump();
    tick();
    auto frames = parseTx();
    TEST_ASSERT_EQUAL_UINT32(1, frames.size());
    TEST_ASSERT_EQUAL_UINT32(100, valsetValue(frames[0], 0x30210001, 2));  // CFG-RATE-MEAS, not CFG-MSG
    teardownDriver();
}

static void test_ack_for_wrong_message_ignored(void)
{
    setupDriver();
    lockOn();
    port->tx.clear();
    // ACK for some other CFG message must NOT complete our VALSET wait
    port->feed(ubx(0x05, 0x01, { 0x06, 0x24 }));   // ACK-ACK of CFG-NAV5, not VALSET
    pump();
    tick();
    // still waiting: no rate/further config sent, no premature "configured"
    TEST_ASSERT_EQUAL_UINT32(0, port->tx.size());
    // the real VALSET ACK still works afterwards
    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));
    pump();
    tick();
    TEST_ASSERT_TRUE(port->tx.size() > 0);
    teardownDriver();
}

// ---------------------------------------------------------------------------
// Status snapshot for the WebUI
// ---------------------------------------------------------------------------

static void test_status_no_driver_returns_false(void)
{
    // No SerialGPS has been constructed in this scenario
    delete gps;
    delete port;
    delete connector;
    gps = nullptr;
    port = nullptr;
    connector = nullptr;

    gps_telemetry_t info;
    TEST_ASSERT_FALSE(SerialGPS::getTelemetryInfo(info));
}

static void test_status_reports_ubx_running(void)
{
    setupDriver();
    lockOnAt(115200);
    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));    // ACK the VALSET
    pump();
    tick();
    // 115200 already carries 10Hz, so the driver settles straight into running
    port->feed(ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03)));
    pump();

    gps_telemetry_t info;
    TEST_ASSERT_TRUE(SerialGPS::getTelemetryInfo(info));
    TEST_ASSERT_EQUAL_UINT8(4, info.state);         // gsRunning
    TEST_ASSERT_EQUAL_UINT32(115200, info.baud);
    TEST_ASSERT_TRUE(info.canConfigure);
    TEST_ASSERT_EQUAL_UINT8(2, info.protocol);      // UBX
    TEST_ASSERT_TRUE(info.ubxConfigured);
    TEST_ASSERT_TRUE(info.usedValset);
    TEST_ASSERT_EQUAL_UINT16(100, info.navIntervalMs);
    TEST_ASSERT_EQUAL_UINT8(3, info.fixType);
    TEST_ASSERT_TRUE(info.fixValid);
    TEST_ASSERT_EQUAL_UINT8(11, info.satellites);
    TEST_ASSERT_EQUAL_INT32(REF_LAT, info.lat);
    TEST_ASSERT_EQUAL_INT32(REF_LON, info.lon);
    TEST_ASSERT_EQUAL_INT32(54540, info.altCm);     // 545400mm / 10
    TEST_ASSERT_TRUE(info.timeValid);
    TEST_ASSERT_EQUAL_UINT16(2025, info.year);
    teardownDriver();
}

static void test_status_measures_update_rate(void)
{
    setupDriver();
    lockOnAt(115200);
    port->feed(ubx(0x05, 0x01, { 0x06, 0x8a }));
    pump();
    tick();
    // Feed NAV-PVT frames 100ms apart; the measured interval should converge on 100ms (10Hz)
    const auto pvt = ubx(0x01, 0x07, navPvt(3, 0x01, 11, REF_LON, REF_LAT, 545400, 2833, 5470000, 0x03));
    for (unsigned i = 0; i < 10; i++)
    {
        nativeClockMs() += 100;
        port->feed(pvt);
        pump();
    }

    gps_telemetry_t info;
    TEST_ASSERT_TRUE(SerialGPS::getTelemetryInfo(info));
    TEST_ASSERT_EQUAL_UINT16(100, info.updateIntervalMs);
    TEST_ASSERT_EQUAL_UINT32(0, info.ageMs);        // the last frame is the current instant
    teardownDriver();
}

static void test_status_reports_nmea_readonly(void)
{
    // No TX line, so the module can only be listened to as NMEA
    setupDriver(UNDEF_PIN);
    lockOn();
    tick();
    port->feed(nmea("GPGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    pump();

    gps_telemetry_t info;
    TEST_ASSERT_TRUE(SerialGPS::getTelemetryInfo(info));
    TEST_ASSERT_EQUAL_UINT8(4, info.state);         // gsRunning, configuration was skipped
    TEST_ASSERT_FALSE(info.canConfigure);
    TEST_ASSERT_FALSE(info.ubxConfigured);
    TEST_ASSERT_EQUAL_UINT8(1, info.protocol);      // NMEA
    TEST_ASSERT_EQUAL_UINT8(3, info.fixType);       // GGA reports any fix as 3D
    TEST_ASSERT_TRUE(info.fixValid);
    TEST_ASSERT_EQUAL_UINT8(8, info.satellites);
    teardownDriver();
}

static void test_status_no_fix_has_no_position(void)
{
    setupDriver(UNDEF_PIN);
    lockOn();
    tick();
    // Fix quality 0: the module has no usable position yet, only a satellite count
    port->feed(nmea("GPGGA,123519.00,4807.038,N,01131.000,E,0,04,0.9,545.4,M,46.9,M,,"));
    pump();

    gps_telemetry_t info;
    TEST_ASSERT_TRUE(SerialGPS::getTelemetryInfo(info));
    TEST_ASSERT_FALSE(info.fixValid);
    TEST_ASSERT_EQUAL_UINT8(0, info.fixType);
    TEST_ASSERT_EQUAL_UINT8(4, info.satellites);    // still surfaced so it can be watched climbing
    TEST_ASSERT_EQUAL_INT32(0, info.lat);
    TEST_ASSERT_EQUAL_INT32(0, info.lon);
    teardownDriver();
}

void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_nmea_position);
    RUN_TEST(test_nmea_southwest_is_negative);
    RUN_TEST(test_nmea_no_fix_zeroes_position);
    RUN_TEST(test_nmea_time);
    RUN_TEST(test_nmea_bad_checksum_ignored);
    RUN_TEST(test_ubx_navpvt_position);
    RUN_TEST(test_ubx_navpvt_negative_heading_wraps);
    RUN_TEST(test_ubx_navpvt_no_fix);
    RUN_TEST(test_ubx_bad_checksum_ignored);
    RUN_TEST(test_ubx_oversized_message_does_not_desync);
    RUN_TEST(test_ubx_and_nmea_interleaved);
    RUN_TEST(test_nothing_is_sent_before_lock);
    RUN_TEST(test_probe_sweeps_baud_rates);
    RUN_TEST(test_valset_sent_after_lock);
    RUN_TEST(test_valset_ack_requests_10hz);
    RUN_TEST(test_valset_nak_falls_back_to_legacy);
    RUN_TEST(test_slow_module_is_moved_to_a_faster_baud);
    RUN_TEST(test_baud_change_is_reverted_if_module_does_not_follow);
    RUN_TEST(test_legacy_baud_change_uses_cfg_prt);
    RUN_TEST(test_gps_time_is_rate_limited);
    RUN_TEST(test_non_ublox_is_left_alone);
    RUN_TEST(test_no_tx_pin_never_writes);
    RUN_TEST(test_ubx_navpvt_84_byte_protocol14_accepted);
    RUN_TEST(test_ubx_navpvt_78_byte_exact_accepted);
    RUN_TEST(test_ubx_navpvt_too_short_ignored);
    RUN_TEST(test_ubx_fragmented_one_byte_per_read);
    RUN_TEST(test_nmea_fragmented_one_byte_per_read);
    RUN_TEST(test_stray_sync1_then_garbage_resyncs);
    RUN_TEST(test_double_sync1_handled);
    RUN_TEST(test_navrate_38400_is_10hz_no_raise);
    RUN_TEST(test_navrate_19200_is_5hz_then_raises);
    RUN_TEST(test_valset_slow_ack_still_accepted);
    RUN_TEST(test_ack_for_wrong_message_ignored);
    RUN_TEST(test_status_no_driver_returns_false);
    RUN_TEST(test_status_reports_ubx_running);
    RUN_TEST(test_status_measures_update_rate);
    RUN_TEST(test_status_reports_nmea_readonly);
    RUN_TEST(test_status_no_fix_has_no_position);
    return UNITY_END();
}
