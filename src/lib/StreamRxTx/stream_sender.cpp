#include <cstdint>
#include <algorithm>
#include <cstring>
#include "stream_sender.h"

void StreamSender::GetOtaPacket(OTA_Packet_s * const otaPktPtr)
{
    uint8_t cmdLen = 4; //TODO (?) variable cmdLen
    if (OtaIsFullRes)
    {
        otaPktPtr->full.stream.packetType = packetType;
        otaPktPtr->full.stream.ack = (ack == ackState::ACK ? 1 : 0);
        otaPktPtr->full.stream.seq = seq;
        otaPktPtr->full.stream.stream = stream;
        if (cmd == StreamTxRx::CmdType::NOP && data.size() >= ELRS8_TELEMETRY_BYTES_PER_CALL) 
        {
            otaPktPtr->full.stream.hasExt = 0;
            data.popBytes(otaPktPtr->full.stream.dataOnly, ELRS8_TELEMETRY_BYTES_PER_CALL);
        }
        else 
        {
            if (cmd >= StreamTxRx::CmdType::CMD_LAST) cmd = StreamTxRx::CmdType::NOP;         
            if (cmdLen > ELRS8_TELEMETRY_BYTES_PER_CALL - 1) cmdLen = ELRS8_TELEMETRY_BYTES_PER_CALL - 1;
            uint8_t dataLen = ELRS8_TELEMETRY_BYTES_PER_CALL - 1 - cmdLen;
            if (dataLen > data.size()) dataLen = data.size();
            otaPktPtr->full.stream.hasExt = 1;
            otaPktPtr->full.stream.ext.dataLen = dataLen;
            otaPktPtr->full.stream.ext.cmd = cmd;
            data.popBytes(otaPktPtr->full.stream.ext.dataAndCmd, dataLen);
            memcpy(otaPktPtr->full.stream.ext.dataAndCmd + dataLen, cmdData, cmdLen);
        }
    }
    else //!OtaIsFullRes
    {
        otaPktPtr->std.type = packetType;
        otaPktPtr->std.stream.ack = (ack == ackState::ACK ? 1 : 0);
        otaPktPtr->std.stream.seq = seq;
        otaPktPtr->std.stream.stream = stream;
        if (cmd == StreamTxRx::CmdType::NOP) 
        {
            otaPktPtr->std.stream.isCmd = 0;
            uint8_t dataLen = (data.size() < ELRS4_TELEMETRY_BYTES_PER_CALL ? data.size() : ELRS4_TELEMETRY_BYTES_PER_CALL);
            otaPktPtr->std.stream.lenOrCmd = dataLen;
            data.popBytes(otaPktPtr->std.stream.dataOrCmd, dataLen);
        }
        else 
        {
            otaPktPtr->std.stream.isCmd = 1;
            otaPktPtr->std.stream.lenOrCmd = cmd;
            if (cmdLen > ELRS4_TELEMETRY_BYTES_PER_CALL) cmdLen = ELRS4_TELEMETRY_BYTES_PER_CALL;
            memcpy(otaPktPtr->std.stream.dataOrCmd, cmdData, cmdLen);
        }
    }
    cmd = StreamTxRx::CmdType::NOP;
    seq = (seq + 1) & 0x01;
}

void StreamSender::PushCrsfPackage(uint8_t* dataToTransmit, uint8_t lengthToTransmit)
{
    if(data.free() < lengthToTransmit) return;
    dataToTransmit[0] = crsfSync;
    data.pushBytes(dataToTransmit, lengthToTransmit);
}