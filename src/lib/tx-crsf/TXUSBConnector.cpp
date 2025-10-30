#include "TXUSBConnector.h"

#include "config.h"

extern Stream *BackpackOrLogStrm;
extern Stream *TxUSB;

void TXUSBConnector::forwardMessage(const crsf_header_t *message)
{
    if (TxUSB != BackpackOrLogStrm && config.GetLinkMode() != TX_MAVLINK_MODE)
    {
        const uint8_t length = message->frame_size + CRSF_FRAME_NOT_COUNTED_BYTES;
        TxUSB->write((uint8_t *)message, length);
    }
}
