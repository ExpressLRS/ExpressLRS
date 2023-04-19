//#include <cstdint>
//#include <algorithm>
//#include <cstring>
#include "StreamSender.h"

void StreamSender::GetOtaPacket(OTA_Packet_s * const otaPktPtr)
{
    otaPktPtr->std.type = packetType;

    _PartBegin(otaPktPtr);
                
    //clear packet, also ensures that partially filled packages are correctly terminated with CmdType::NOP 
    memset(_streamPtr->data, StreamBase::CmdType::NOP, _dataSize);

    //setup first header
    _streamPtr->hdrFirst.packetType = packetType;
    //NOTE: set ack=1 until ackState is fully implemented
    _streamPtr->hdrFirst.ack = 1; //_streamPtr->hdrFirst.ack = (ack == ackState::ACK ? 1 : 0);
    _streamPtr->hdrFirst.seq = seq;    
    _streamPtr->hdrFirst.partType = OTA_STREAMTYPE_CMD; 
    _streamPtr->hdrFirst.partMore = 0;

    //push parts into ota packet 
    //cmd has highest priority, if space left then send stream1, if still space left then send stream2
    _PartPush(OTA_STREAMTYPE_CMD, cmdFifo, false);
    _PartPush(OTA_STREAMTYPE_STREAM1, stream1Fifo, true);
    _PartPush(OTA_STREAMTYPE_STREAM2, stream2Fifo, true);
        
    seq = (seq + 1) & 0x01;
}

void StreamSender::PushCrsfPackage(uint8_t* dataToTransmit, uint8_t lengthToTransmit)
{
    if(stream1Fifo.free() < lengthToTransmit) return;
    dataToTransmit[0] = crsfSync;
    stream1Fifo.pushBytes(dataToTransmit, lengthToTransmit);
} 

void StreamSender::GetStreamStartPacket(OTA_Packet_s * const otaPktPtr)
{
    otaPktPtr->std.type = packetType;

    _PartBegin(otaPktPtr);
                
    //clear packet, also ensures that partially filled packages are correctly terminated with CmdType::NOP 
    memset(_streamPtr->data, StreamBase::CmdType::NOP, _dataSize);

    //setup first header
    _streamPtr->hdrFirst.packetType = packetType;
    //NOTE: set ack=1 until ackState is fully implemented
    _streamPtr->hdrFirst.ack = 1; //_streamPtr->hdrFirst.ack = (ack == ackState::ACK ? 1 : 0);
    _streamPtr->hdrFirst.seq = seq;
    _streamPtr->hdrFirst.partType = OTA_STREAMTYPE_CMD; 
    _streamPtr->hdrFirst.partMore = 0;

    //push parts into ota packet
    cmdFifo.flush();
    cmdFifo.push(CmdType::STREAM_START);
    cmdFifo.push('s');
    cmdFifo.push('t');
    cmdFifo.push('r');
    cmdFifo.push(STREAM_VERSION);
    _PartPush(OTA_STREAMTYPE_CMD, cmdFifo, false);
        
    seq = (seq + 1) & 0x01;
}
