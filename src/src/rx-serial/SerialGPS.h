#include "SerialIO.h"

typedef struct {
    // Latitude in decimal degrees
    float lat;
    // Longitude in decimal degrees
    float lon;
    // Altitude in meters
    float alt;
    // Speed in km/h
    float speed;
    // Heading in degrees, positive. 0 is north.
    float heading;
    // Number of satellites
    uint8_t satellites;
} GpsData;

class SerialGPS : public SerialIO {
public:
    explicit SerialGPS(Stream &out, Stream &in) : SerialIO(&out, &in)
    {

    }
    virtual ~SerialGPS() {}

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t* data) override;
    void sendQueuedData(uint32_t maxBytesToSend) override;
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override { return DURATION_IMMEDIATELY; }
private:
    void processBytes(uint8_t *bytes, uint16_t size) override;
    void sendTelemetryFrame();
    void processSentence(uint8_t *sentence, uint8_t size);
    GpsData gpsData = {0};
};
