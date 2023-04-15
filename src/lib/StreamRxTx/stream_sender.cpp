#include <cstdint>
#include <algorithm>
#include <cstring>
#include "stream_sender.h"

uint32_t DEBUG_txdatacnt = 0;

void StreamSender::GetOtaPacket(OTA_Packet_s * const otaPktPtr)
{
    otaPktPtr->std.type = packetType;

    _PartBegin(otaPktPtr);
                
    //clear packet
    memset(_streamPtr->data, StreamTxRx::CmdType::NOP, _dataSize);

    //setup first header
    _streamPtr->hdrFirst.packetType = packetType;
    _streamPtr->hdrFirst.ack = (ack == ackState::ACK ? 1 : 0);
    _streamPtr->hdrFirst.seq = seq;    
    _streamPtr->hdrFirst.partType = OTA_STREAMTYPE_CMD; 
    _streamPtr->hdrFirst.partMore = 0;

    //push parts into ota packet
    _PartPush(OTA_STREAMTYPE_CMD, cmdFifo, false);
    _PartPush(OTA_STREAMTYPE_DATA, data, true);
    _PartPush(OTA_STREAMTYPE_DATA2, data2, true);
        
    seq = (seq + 1) & 0x01;
}

void StreamSender::PushCrsfPackage(uint8_t* dataToTransmit, uint8_t lengthToTransmit)
{
    if(data.free() < lengthToTransmit) return;
    dataToTransmit[0] = crsfSync;
    data.pushBytes(dataToTransmit, lengthToTransmit);
}