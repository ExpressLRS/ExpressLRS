#include <cstdint>
#include <algorithm>
#include <cstring>
#include "stream_receiver.h"
#include "crsf_protocol.h"
#include "crsf.h"

ICACHE_RAM_ATTR StreamTxRx::CmdType StreamReceiver::ReceiveOtaPacket(OTA_Packet_s const * const otaPktPtr)
{
    if (OtaIsFullRes)
    {
        ack = (otaPktPtr->full.stream.ack == 1 ? ackState::ACK : ackState::NACK) ;
        //uint8_t seq = otaPktPtr->full.stream.seq; //TODO
        //uint8_t stream = otaPktPtr->full.stream.stream; //TODO
        if (otaPktPtr->full.stream.hasExt == 0) 
        {
            data.pushBytes(otaPktPtr->full.stream.dataOnly, ELRS8_TELEMETRY_BYTES_PER_CALL);
            cmd = StreamTxRx::CmdType::NOP;
            cmdLen = 0;            
        }
        else 
        {
            uint8_t dataLen = otaPktPtr->full.stream.ext.dataLen;
            if (dataLen > ELRS8_TELEMETRY_BYTES_PER_CALL - 1) dataLen = ELRS8_TELEMETRY_BYTES_PER_CALL - 1;
            data.pushBytes(otaPktPtr->full.stream.ext.dataAndCmd, dataLen);

            cmd = (StreamTxRx::CmdType) otaPktPtr->full.stream.ext.cmd;
            cmdLen = ELRS8_TELEMETRY_BYTES_PER_CALL - 1 - dataLen;    
            memcpy(cmdData, otaPktPtr->full.stream.ext.dataAndCmd + dataLen, cmdLen);
        }
    }
    else //!OtaIsFullRes
    {
        ack = (otaPktPtr->std.stream.ack == 1 ? ackState::ACK : ackState::NACK) ;
        //uint8_t seq = otaPktPtr->std.stream.seq; //TODO
        //uint8_t stream = otaPktPtr->std.stream.stream; //TODO
        if (otaPktPtr->std.stream.isCmd == 0) 
        {
            data.pushBytes(otaPktPtr->std.stream.dataOrCmd, otaPktPtr->std.stream.lenOrCmd);
            cmd = StreamTxRx::CmdType::NOP;
            cmdLen = 0;            
        }
        else 
        {
            cmd = (StreamTxRx::CmdType) otaPktPtr->std.stream.lenOrCmd;
            cmdLen = 4; //TODO (?) variable cmdLen 
            memcpy(cmdData, otaPktPtr->std.stream.dataOrCmd, cmdLen);
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
