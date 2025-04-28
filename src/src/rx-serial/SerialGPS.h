#pragma once

#include "SerialIO.h"

#include "device.h"

typedef struct {
    // Latitude in decimal degrees
    uint32_t lat;
    // Longitude in decimal degrees
    uint32_t lon;
    // Altitude in meters
    uint32_t alt;
    // Speed in km/h
    uint32_t speed;
    // Heading in degrees, positive. 0 is north.
    uint32_t heading;
    // Number of satellites
    uint8_t satellites;
} GpsData;

class SerialGPS : public SerialIO {
public:
    explicit SerialGPS(Stream &out, Stream &in) : SerialIO(&out, &in) {}
    ~SerialGPS() override = default;

    void queueLinkStatisticsPacket() override {}
    void sendQueuedData(uint32_t maxBytesToSend) override;
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override { return DURATION_IMMEDIATELY; }
private:
    void processBytes(uint8_t *bytes, uint16_t size) override;
    void sendTelemetryFrame();
    void processSentence(uint8_t *sentence, uint8_t size);
    GpsData gpsData = {0};
};
