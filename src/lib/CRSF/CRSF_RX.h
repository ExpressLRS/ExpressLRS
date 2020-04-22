#ifndef CRSF_RX_H_
#define CRSF_RX_H_

#include "CRSF.h"

class CRSF_RX : public CRSF
{
public:
    CRSF_RX(HwSerial *dev) : CRSF(dev) {}
    CRSF_RX(HwSerial &dev) : CRSF(&dev) {}

    void handleUartIn(void);

    void sendRCFrameToFC();
    void LinkStatisticsSend();

    // Received channel data
    crsf_channels_t ChannelsPacked = {0};

private:
    void sendFrameToFC(uint8_t *buff, uint8_t size);
    void processPacket(uint8_t const *data);
};

#endif /* CRSF_RX_H_ */
