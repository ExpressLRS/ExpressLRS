#pragma once

#include "SerialIO.h"

#include "device.h"

typedef struct
{
    int32_t lat;        // Latitude in decimal degrees * 1e7
    int32_t lon;        // Longitude in decimal degrees * 1e7
    int32_t alt;        // Altitude in cm
    int32_t speed;      // Speed in km/h * 100
    uint16_t heading;   // Heading in degrees * 100, positive. 0 is north.
    uint8_t satellites; // Number of satellites
    bool fixValid;      // The module reports a usable 2D/3D position

    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t millisecond;
} GpsData;

/**
 * @brief Receiver-side GPS driver.
 *
 * Accepts NMEA 0183 and UBX on the same port and auto-detects the baud rate, so a module can be
 * wired up at its factory settings without the user configuring anything. If the module can be
 * talked to (a TX pin is wired) and turns out to be a u-blox, it is asked to emit UBX-NAV-PVT
 * instead of NMEA, which is both cheaper on the wire and carries the fix quality.
 */
class SerialGPS final : public SerialIO {
public:
    /**
     * @param port UART the GPS is attached to. The driver changes the baud rate on this port while
     * it is probing, so it needs the concrete port and not just a Stream.
     * @param txPin pin driving the GPS RX line, or UNDEF_PIN if the module cannot be talked to,
     * in which case auto-configuration is skipped and the module is only listened to.
     */
    explicit SerialGPS(HardwareSerial &port, int8_t txPin)
        : SerialIO(&port, &port), _port(&port), _txPin(txPin) { beginProbe(); }
    ~SerialGPS() override = default;

    typedef void (*gpsFieldParser_t)(SerialGPS *ctx, uint8_t fieldIdx, char *field);

    /// @brief No RC data is sent to a GPS, this is used as the tick for the detect/configure state machine
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    enum gpsState_e : uint8_t
    {
        gsProbing,      // sweeping the baud rate candidates looking for valid frames
        gsCfgValset,    // UBX-CFG-VALSET sent, waiting for the ACK (M9/M10/F9)
        gsCfgLegacy,    // UBX-CFG-MSG sent, waiting for the ACK (M6/M7/M8)
        gsCfgBaud,      // baud rate change sent, waiting to see if the module followed
        gsRunning,      // baud rate locked, parsing
    };

    enum gpsAck_e : uint8_t
    {
        gaIdle,
        gaPending,
        gaAck,
        gaNak,
    };

    enum gpsRx_e : uint8_t
    {
        grIdle,
        grNmea,
        grUbxSync2,
        grUbxHead,
        grUbxPayload,
        grUbxCkA,
        grUbxCkB,
    };

    void processBytes(uint8_t *bytes, uint16_t size) override;
    void sendTelemetryFrame();
    void sendGpsTimeTelemetryFrame();

    // NMEA
    bool isValidChecksum(char *sentence, uint8_t size);
    void processSentence(char *sentence, uint8_t size);
    void splitSentenceFields(char *sentence, uint8_t size, gpsFieldParser_t callback);
    static void fieldParseGGA(SerialGPS *ctx, uint8_t fieldIdx, char *field);
    static void fieldParseVTG(SerialGPS *ctx, uint8_t fieldIdx, char *field);
    static void fieldParseRMC(SerialGPS *ctx, uint8_t fieldIdx, char *field);

    // UBX
    void processUbxMessage();
    void processNavPvt();
    void queueUbx(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint8_t len);
    void queueCfgMsg(uint8_t msgClass, uint8_t msgId, uint8_t rate);
    void queueNavRate(uint16_t measRateMs);
    void queueBaudChange(uint32_t baud);
    void expectAck(uint8_t msgClass, uint8_t msgId);
    void ubxAccumulate(uint8_t c) { ubxCkA += c; ubxCkB += ubxCkA; }

    // Detect/configure
    uint32_t currentBaud() const;
    void setBaud(uint32_t baud);
    void frameReceived();
    void beginProbe();
    void beginConfigure();
    void configComplete();
    void disableNmeaOutput();

    HardwareSerial *_port;
    int8_t _txPin;

    GpsData gpsData = {};

    // NMEA 0183 has a maximum 82 byte sentence including the \r\n, UBX-NAV-PVT is 92 bytes.
    // Only one message is ever in flight so both protocols share the buffer.
    uint8_t buffer[92] = {};
    uint8_t bufferIndex = 0;

    gpsRx_e rxState = grIdle;
    uint8_t ubxClass = 0;
    uint8_t ubxId = 0;
    uint16_t ubxLen = 0;
    uint8_t ubxCkA = 0;
    uint8_t ubxCkB = 0;
    uint8_t ubxCkARx = 0;

    gpsState_e state = gsProbing;
    uint8_t baudIndex = 0;
    uint8_t previousBaudIndex = 0;
    uint8_t goodFrames = 0;
    uint8_t configAttempts = 0;
    bool configApplied = false;
    bool usedValset = false;
    bool baudRaiseDone = false;
    bool baudSwitched = false;
    bool pvtSeen = false;
    bool timeFrameSent = false;
    uint32_t stateChangedMs = 0;
    uint32_t lastFrameMs = 0;
    uint32_t lastPvtMs = 0;
    uint32_t lastTimeFrameMs = 0;

    gpsAck_e ackState = gaIdle;
    uint8_t ackClass = 0;
    uint8_t ackId = 0;
};
