#include "StreamBase.h"

class StreamSender : StreamBase
{
public:
    FIFO_GENERIC<96> stream1Fifo;
    FIFO_GENERIC<96> stream2Fifo;
    ackState ack;

private:
    uint8_t packetType;
    uint8_t seq;
    FIFO_GENERIC<6> cmdFifo;  //actually has only a single cmd in buffer

public:
    StreamSender(uint8_t crsfSync, uint8_t packetType) : ack(ackState::UNKNOWN) 
    {
        this->crsfSync = crsfSync;
        this->packetType = packetType;
    };

    void GetOtaPacket(OTA_Packet_s * const otaPktPtr);

    void PushCrsfPackage(uint8_t* dataToTransmit, uint8_t lengthToTransmit);

    // used to only send new CRSF data to stream1Fifo when the fifo is low on data,
    // prevents sending stale CRSF data OTA
    bool IsStream1FifoLow()
    { 
        return stream1Fifo.size() < 20; //20 bytes are 2 OTA8 payloads
    } 

    bool IsOtaPacketReady()
    { 
        return cmdFifo.size() > 0 || stream1Fifo.size() > 0 || stream2Fifo.size() > 0;
    }

    bool IsCmdSet()
    {
        return cmdFifo.size() > 0;
    }

    void SetCmd(StreamBase::CmdType cmd, uint8_t * cmdArgs, uint8_t cmdArgsLen)
    {
        if (cmd == StreamBase::CmdType::NOP || cmd >= StreamBase::CmdType::CMD_LAST) return;
        cmdFifo.flush();
        cmdFifo.push(cmd);
        cmdFifo.pushBytes(cmdArgs, cmdArgsLen);
    }

    void GetStreamStartPacket(OTA_Packet_s * const otaPktPtr);
};
