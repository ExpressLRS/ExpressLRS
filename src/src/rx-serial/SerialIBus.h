#include "SerialIO.h"

#define IBUS_NUM_CHANS 14
#define IBUS_HEADER ((uint16_t) 0x4020)
#define IBUS_CHECKSUM_START ((uint16_t) 0xFFFF)

#define IBUS_CHANNEL_VALUE_MIN 1000
#define IBUS_CHANNEL_VALUE_MAX 2000

#define IBUS_CHAN_PACKET_INTERVAL_MS 7

typedef struct ibus_channels_packet_s {
    uint16_t header; // Always 0x20, 0x40 (LSB first)
    uint16_t channels[IBUS_NUM_CHANS]; // uint16_t in range [1000, 2000]
    uint16_t checksum; // 0xFFFF - all previous bytes

} __attribute__((packed)) ibus_channels_packet_t;

class SerialIBus : public SerialIO {
public:
    explicit SerialIBus(Stream &out, Stream &in) : SerialIO(&out, &in)
    {
        streamOut = &out;
    }

    ~SerialIBus() override = default;

    void queueLinkStatisticsPacket() override {} // No link stats in IBus
    void queueMSPFrameTransmission(uint8_t* data) override {} // No MSP over IBus
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {};

    void setChecksum(ibus_channels_packet_t *packet);

    Stream *streamOut;
};
