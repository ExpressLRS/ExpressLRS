#pragma once

#include <stdint.h>

struct Channels;
class TransportLayer;

class CRSF_RXModule
{
public:
    CRSF_RXModule() {}

    void begin(TransportLayer* dev);

    void sendRCFrameToFC(Channels* chan);
    void sendLinkStatisticsToFC(Channels* chan);
    void sendMSPFrameToFC(uint8_t* data);

private:
    TransportLayer* _dev = nullptr;
};

extern CRSF_RXModule crsfRx;
