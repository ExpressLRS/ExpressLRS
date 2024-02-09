#include "SerialIO.h"

class SerialSatellite : public SerialIO
{
public:
    SerialSatellite(Stream &out, Stream &in)
        : SerialIO(&out, &in) {}

    ~SerialSatellite() override = default;

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t *data) override {}
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    static constexpr uint16_t SATELLITE_MAX_CHANNELS = 12;
    static constexpr uint8_t SATELLITE_CHANNELS_PER_PACKET = 7;

    void processBytes(uint8_t *bytes, uint16_t size) override{};

    uint32_t prevChannelData[SATELLITE_CHANNELS_PER_PACKET] = {0};

    uint8_t fadeCount{0};
    uint8_t roundRobinIndex{0};
    bool sendPackets{false};
};