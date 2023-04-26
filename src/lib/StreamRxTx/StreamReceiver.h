#pragma once

#include <cstdint>
#include "OTA.h"
#include "FIFO_GENERIC.h"
#include "StreamBase.h"

class StreamReceiver : StreamBase
{
public:    
    FIFO_GENERIC<96> stream1Fifo;
    FIFO_GENERIC<96> stream2Fifo;
    ackState ack;
    StreamBase::CmdType cmd;
    uint8_t cmdArgsLen;
    uint8_t cmdArgs[ELRS8_TELEMETRY_BYTES_PER_CALL - 1];

    StreamReceiver(uint8_t crsfSync) : ack(ackState::UNKNOWN)
    {
        this->crsfSync = crsfSync;
    };

    ICACHE_RAM_ATTR StreamBase::CmdType ReceiveOtaPacket(OTA_Packet_s * const otaPktPtr);
    bool PopCrsfPacket(uint8_t * CRSFBuffer);
    bool isStreamStart(OTA_Packet_s * const otaPktPtr);
    bool DEBUG_gotsomething(OTA_Packet_s * const otaPktPtr);
    void DEBUG_decodePacket(OTA_Packet_s * const otaPktPtr);
};
