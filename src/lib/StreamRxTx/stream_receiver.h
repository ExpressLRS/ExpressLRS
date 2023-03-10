#pragma once

#include <cstdint>
#include "OTA.h"
#include "FIFO_GENERIC.h"
#include "stream_sender.h"

class StreamReceiver
{
public:
    StreamReceiver(uint8_t crsfSync) : ack(ackState::UNKNOWN) 
    {
        this->crsfSync = crsfSync;
    };
    ICACHE_RAM_ATTR StreamTxRx::CmdType ReceiveOtaPacket(OTA_Packet_s const * const otaPktPtr);
    bool PopCrsfPacket(uint8_t * CRSFBuffer);
    FIFO_GENERIC<96> data;
    ackState ack;

    StreamTxRx::CmdType cmd;
    uint8_t cmdLen;
    uint8_t cmdData[ELRS8_TELEMETRY_BYTES_PER_CALL - 1];    
private:
    uint8_t crsfSync;
};
