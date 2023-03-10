#pragma once

#include <cstdint>
#include "OTA.h"
#include "FIFO_GENERIC.h"

enum ackState{ACK,NACK,UNKNOWN};

class StreamTxRx
{
public:
    enum CmdType{NOP,LINKSTAT,BIND,SET_RX_WIFI_MODE,SET_RX_LOAN_MODE,CMD_LAST};
};

class StreamSender
{
public:
    FIFO_GENERIC<96> data;
    ackState ack;
    uint8_t stream;
    StreamSender(uint8_t crsfSync, uint8_t packetType) : ack(ackState::UNKNOWN), stream(0)
    {
        this->crsfSync = crsfSync;
        this->packetType = packetType;
    };
    void GetOtaPacket(OTA_Packet_s * const otaPktPtr);
    void PushCrsfPackage(uint8_t* dataToTransmit, uint8_t lengthToTransmit);
    bool IsDataBufferLow() 
    { 
        return data.size() < 20; 
    } 
    bool IsOtaPacketReady() 
    { 
        return data.size() > 0 || cmd != StreamTxRx::CmdType::NOP; 
    }
    void SetCmd(StreamTxRx::CmdType a_cmd, uint8_t * a_cmdData)
    {
        if (a_cmd >= StreamTxRx::CmdType::CMD_LAST) return;
        memcpy(cmdData, a_cmdData, 4);
        cmd = a_cmd;
    }
private:
    uint8_t crsfSync; 
    uint8_t packetType;
    uint8_t seq;
    StreamTxRx::CmdType cmd;
    uint8_t cmdData[ELRS8_TELEMETRY_BYTES_PER_CALL - 1]; 
};
