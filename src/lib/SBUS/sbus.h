#pragma once

#include "RXModule.h"

#define SBUS_PACKET_SIZE 25

#define SBUS_HEADER 0x0F
#define SBUS_FOOTER 0x00

class SbusRxModule : public RXModule
{
public:
    void sendRCFrameToFC(Channels* chan) override;
};

extern SbusRxModule sbusRxModule;
