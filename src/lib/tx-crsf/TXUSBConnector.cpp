#include "TXUSBConnector.h"

#include "common.h"
#include "config.h"

extern Stream *BackpackOrLogStrm;
extern Stream *TxUSB;

void TXUSBConnector::forwardMessage(const crsf_header_t *message)
{
    if (TxUSB != BackpackOrLogStrm && config.GetLinkMode() != TX_MAVLINK_MODE)
    {
        const size_t length = message->frame_size + CRSF_FRAME_NOT_COUNTED_BYTES;
        TxUSB->write((uint8_t *)message, length);
    }
}

uint8_t TXUSBConnector::GetMaxPacketBytes() const
{
#if defined(PLATFORM_ESP32_S3)
#if ESP_ARDUINO_VERSION <= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // Arduino < 3.0.0 & S3 has real issues sending 64 bytes!
    return CRSF_MAX_PACKET_LEN >= 64 ? 63 : CRSF_MAX_PACKET_LEN;
#else
    return CRSF_MAX_PACKET_LEN;
#endif
#else
    return CRSF_MAX_PACKET_LEN;
#endif
}
