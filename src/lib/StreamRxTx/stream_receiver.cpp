#include <cstdint>
#include <algorithm>
#include <cstring>
#include "stream_receiver.h"
#include "crsf_protocol.h"
#include "CRSF.h"

uint32_t DEBUG_rxdatacnt = 0;

ICACHE_RAM_ATTR StreamTxRx::CmdType StreamReceiver::ReceiveOtaPacket(OTA_Packet_s * const otaPktPtr)
{
    StreamTxRx::CmdType cmd = StreamTxRx::CmdType::NOP;

    _PartBegin(otaPktPtr);

    ack = (_streamPtr->hdrFirst.ack == 1 ? ackState::ACK : ackState::NACK) ;
    //uint8_t seq = streamPtr->hdrFirst.seq; //TODO

    uint8_t partDataPos;
    uint8_t partLen;
    uint8_t partType;

    while (_PartPop(&partType, &partDataPos, &partLen))
    {
        switch(partType) {
        case OTA_STREAMTYPE_CMD:
            cmd = (StreamTxRx::CmdType) _streamPtr->data[partDataPos];
            cmdArgsLen = partLen - 1;
            memcpy(cmdArgs, &_streamPtr->data[partDataPos + 1], partLen - 1);
            break;
        case OTA_STREAMTYPE_DATA:
            data.pushBytes(&_streamPtr->data[partDataPos], partLen);
            break;
        case OTA_STREAMTYPE_DATA2:
            data2.pushBytes(&_streamPtr->data[partDataPos], partLen);
            break;                        
        }
    }

    return cmd;  
}

bool StreamReceiver::PopCrsfPacket(uint8_t * CRSFBuffer) {
    while (data.size() >= 4) //minimum package length is 4: {sync len type crc}
    {
        if (data.peek() != crsfSync) 
        {
            data.pop();
            continue;
        }
        uint8_t len = data.peekPos(CRSF_TELEMETRY_LENGTH_INDEX);
        if (len < 2 || len > 63) 
        {
            data.pop();
            continue;
        }
        if (data.size() < len + CRSF_FRAME_NOT_COUNTED_BYTES)
        {
            break;
        }
        //TODO: check crc without popping data first -> improved recovery from missed OTA package
        data.popBytes(CRSFBuffer, len + CRSF_FRAME_NOT_COUNTED_BYTES);
        uint8_t crc = crsf_crc.calc(CRSFBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, len - CRSF_TELEMETRY_CRC_LENGTH);
        if (crc == CRSFBuffer[len + CRSF_FRAME_NOT_COUNTED_BYTES - CRSF_TELEMETRY_CRC_LENGTH]) 
        {
            return true;
        }
    }
    return false;
}
