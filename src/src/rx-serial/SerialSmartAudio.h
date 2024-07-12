#include "SerialIO.h"
#include "crc.h"

#define SMARTAUDIO_MAX_FRAME_SIZE 32
#define SMARTAUDIO_HEADER_DUMMY 0x00 // Page 2: "The SmartAudio line need to be low before a frame is sent. If the host MCU can’t handle this it can be done by sending a 0x00 dummy byte in front of the actual frame."
#define SMARTAUDIO_HEADER_1 0xAA
#define SMARTAUDIO_HEADER_2 0x55
#define SMARTAUDIO_CRC_POLY 0xD5
#define SMARTAUDIO_RESPONSE_DELAY_MS 150

// check value for MSP_SET_VTX_CONFIG to determine if value is encoded
// band/channel or frequency in MHz (3 bits for band and 3 bits for channel)
#define VTXCOMMON_MSP_BANDCHAN_CHKVAL ((uint16_t)((7 << 3) + 7))

class SerialSmartAudio : public SerialIO {
public:
    explicit SerialSmartAudio(Stream &out, Stream &in) : SerialIO(&out, &in) { }
    virtual ~SerialSmartAudio() {}

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t* data) override;
    void sendQueuedData(uint32_t maxBytesToSend) override;
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override { return DURATION_IMMEDIATELY; }

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {};
};
