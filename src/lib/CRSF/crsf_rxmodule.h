#pragma once

#include <stdint.h>
#include "RXModule.h"

struct Channels;
class TransportLayer;

class CRSF_RXModule : public RXModule
{
public:
    CRSF_RXModule() {}

    void sendRCFrameToFC(Channels* chan) override;
    void sendLinkStatisticsToFC(Channels* chan) override;
    void sendMSPFrameToFC(uint8_t* data) override;
};

extern CRSF_RXModule crsfRx;
