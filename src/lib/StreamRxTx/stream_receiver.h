#pragma once

#include <cstdint>
#include "OTA.h"
#include "FIFO_GENERIC.h"
#include "stream_sender.h"

class StreamReceiver : StreamTxRx
{
public:
    bool debug_gotsomething(OTA_Packet_s * const otaPktPtr);
    void debug_decodePacket(OTA_Packet_s * const otaPktPtr);
    
    StreamReceiver(uint8_t crsfSync) : ack(ackState::UNKNOWN) 
    {
        this->crsfSync = crsfSync;
    };
    ICACHE_RAM_ATTR StreamTxRx::CmdType ReceiveOtaPacket(OTA_Packet_s * const otaPktPtr);
    bool PopCrsfPacket(uint8_t * CRSFBuffer);
    FIFO_GENERIC<96> stream1Fifo;
    FIFO_GENERIC<96> stream2Fifo;   
    ackState ack;

    StreamTxRx::CmdType cmd;
    uint8_t cmdArgsLen;
    uint8_t cmdArgs[ELRS8_TELEMETRY_BYTES_PER_CALL - 1];    
private:
    uint8_t crsfSync;
};
