#ifndef TX_USB_CONNECTOR_H
#define TX_USB_CONNECTOR_H
#include "CRSFConnector.h"

class TXUSBConnector final : public CRSFConnector
{
public:
    void forwardMessage(const crsf_header_t *message) override;
    uint8_t GetMaxPacketBytes() const override;
};

#endif //TX_USB_CONNECTOR_H
