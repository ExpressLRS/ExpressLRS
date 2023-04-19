#include <cstdint>
#include <algorithm>
#include <cstring>
#include "stream_receiver.h"
#include "crsf_protocol.h"
#include "CRSF.h"

uint32_t DEBUG_rxdatacnt = 0;

bool StreamReceiver::debug_gotsomething(OTA_Packet_s * const otaPktPtr)
{
    _PartBegin(otaPktPtr);

    ack = (_streamPtr->hdrFirst.ack == 1 ? ackState::ACK : ackState::NACK);
    //uint8_t seq = streamPtr->hdrFirst.seq; //TODO

    uint8_t partDataPos;
    uint8_t partLen;
    uint8_t partType;

    while (_PartPop(&partType, &partDataPos, &partLen))
    {
        switch(partType) {
        case OTA_STREAMTYPE_CMD:
            if (_streamPtr->data[partDataPos] >= 2 ) return true;
            break;
        case OTA_STREAMTYPE_STREAM1:
            return true;
            break;
        case OTA_STREAMTYPE_STREAM2:
            return true;
            break;
        }
    }
    return false;
} 

void StreamReceiver::debug_decodePacket(OTA_Packet_s * const otaPktPtr)
{
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
            if (_streamPtr->data[partDataPos] >= 2)
            {
                DBG("cmd:");
                for(uint8_t i=0;i<partLen;i++) DBG("%x ", _streamPtr->data[partDataPos+i]);
            }
            break;
        case OTA_STREAMTYPE_STREAM1:
            DBG("str1:");
            for(uint8_t i=0;i<partLen;i++) DBG("%x ", _streamPtr->data[partDataPos+i]);
            break;
        case OTA_STREAMTYPE_STREAM2:
            DBG("str2:");
            for(uint8_t i=0;i<partLen;i++) DBG("%x ", _streamPtr->data[partDataPos+i]);
            break;                        
        }
    }
}

ICACHE_RAM_ATTR StreamTxRx::CmdType StreamReceiver::ReceiveOtaPacket(OTA_Packet_s * const otaPktPtr)
{
    StreamTxRx::CmdType cmd = StreamTxRx::CmdType::NOP;

    _PartBegin(otaPktPtr);

    ack = (_streamPtr->hdrFirst.ack == 1 ? ackState::ACK : ackState::NACK);
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
        case OTA_STREAMTYPE_STREAM1:
            stream1Fifo.pushBytes(&_streamPtr->data[partDataPos], partLen);
            break;
        case OTA_STREAMTYPE_STREAM2:
            stream2Fifo.pushBytes(&_streamPtr->data[partDataPos], partLen);
            break;                        
        }
    }

    return cmd;  
}

bool StreamReceiver::PopCrsfPacket(uint8_t * CRSFBuffer) {
    while (stream1Fifo.size() >= 4) //minimum package length is 4: {sync len type crc}
    {
        if (stream1Fifo.peekPos(0) != crsfSync) 
        {
            stream1Fifo.pop();
            continue;
        }
        uint8_t len = stream1Fifo.peekPos(CRSF_TELEMETRY_LENGTH_INDEX);
        if (len < 2 || len > 63) 
        {
            stream1Fifo.pop();
            continue;
        }
        if (stream1Fifo.size() < len + CRSF_FRAME_NOT_COUNTED_BYTES)
        {
            break;
        }
        //TODO: check crc without popping data first -> improved recovery from missed OTA package
        stream1Fifo.popBytes(CRSFBuffer, len + CRSF_FRAME_NOT_COUNTED_BYTES);
        uint8_t crc = crsf_crc.calc(CRSFBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, len - CRSF_TELEMETRY_CRC_LENGTH);
        if (crc == CRSFBuffer[len + CRSF_FRAME_NOT_COUNTED_BYTES - CRSF_TELEMETRY_CRC_LENGTH]) 
        {
            return true;
        }
    }
    return false;
}
