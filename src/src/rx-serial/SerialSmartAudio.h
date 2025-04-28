#pragma once

#include "SerialIO.h"
#include "CRSFEndpoint.h"
#include "device.h"

#define SMARTAUDIO_MAX_FRAME_SIZE 32
#define SMARTAUDIO_HEADER_DUMMY 0x00 // Page 2: "The SmartAudio line need to be low before a frame is sent. If the host MCU can’t handle this it can be done by sending a 0x00 dummy byte in front of the actual frame."
#define SMARTAUDIO_HEADER_1 0xAA
#define SMARTAUDIO_HEADER_2 0x55
#define SMARTAUDIO_CRC_POLY 0xD5
#define SMARTAUDIO_RESPONSE_DELAY_MS 200

// check value for MSP_SET_VTX_CONFIG to determine if value is encoded
// band/channel or frequency in MHz (3 bits for band and 3 bits for channel)
#define VTXCOMMON_MSP_BANDCHAN_CHKVAL ((uint16_t)((7 << 3) + 7))

class SerialSmartAudio final : public SerialIO, public CRSFConnector {
public:
    explicit SerialSmartAudio(Stream &out, Stream &in, int8_t serial1TXpin) : SerialIO(&out, &in) {
#if defined(PLATFORM_ESP32)
        // we are on UART1, use Serial1 TX assigned pin for half duplex
        UTXDoutIdx = U1TXD_OUT_IDX;
        URXDinIdx = U1RXD_IN_IDX;
        halfDuplexPin = serial1TXpin;
#endif
        setRXMode();
        crsfEndpoint->addConnector(this);
    }
    ~SerialSmartAudio() override = default;

    void queueLinkStatisticsPacket() override {}
    void sendQueuedData(uint32_t maxBytesToSend) override;
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override { return DURATION_IMMEDIATELY; }

    void forwardMessage(const crsf_header_t *message) override;

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {};
    void setTXMode();
    void setRXMode();
#if defined(PLATFORM_ESP32)
    int8_t halfDuplexPin;
    uint8_t UTXDoutIdx;
    uint8_t URXDinIdx;
#endif
};
