#pragma once

#include <cstdint>
#include "OTA.h"
#include "FIFO_GENERIC.h"

#define STREAM_VERSION 1

enum ackState{ACK,NACK,UNKNOWN};

class StreamBase
{
public:
    enum CmdType{NOP,LINKSTAT,BIND,SET_RX_WIFI_MODE,SET_RX_LOAN_MODE,STREAM_START,CMD_LAST};
//command          - number of argument bytes
//LINKSTAT         - 4 (OTA_LinkStats_s)
//BIND             - 4
//SET_RX_WIFI_MODE - 0
//SET_RX_LOAN_MODE - 0

protected:
    uint8_t crsfSync;

    // Example packet with packet len=13 (Tn:partType, Ln:partLen, M:partMore, Dn:data, where n=partNbr)
    // encodes 4 parts each with type and len. (last part len is implicit, uses remaining bytes of packet)
    // 0:hdr   1:hdr        2   3   4   5:hdr        6   7   8:hdr        9   10  11  12
    // T1,M=1  L1=3,T2,M=1  D1  D1  D1  L2=2,T3,M=1  D2  D2  L3=1,T4,M=0  D3  D4  D4  D4
    void _PartBegin(OTA_Packet_s * const otaPktPtr)
    {
        if (OtaIsFullRes)
        {
            _dataSize = ELRS8_TELEMETRY_BYTES_PER_CALL + 1; // =11 includes first header byte
            _streamPtr = &(otaPktPtr->full.stream);
        }
        else
        {
            _dataSize = ELRS4_TELEMETRY_BYTES_PER_CALL + 1; // =6 includes first header byte
            _streamPtr = &(otaPktPtr->std.stream);
        }    
        _hdrPos = 0;
        _dataPos = 1; 
    }

    //push part into OTA packet
    void _PartPush(uint8_t type, FIFO_GENERIC_Base& fifo, bool partialAllowed)
    {
        uint8_t len = fifo.size();
        if (len == 0) return;
        if (len > _dataSize - _dataPos) 
        {
            if (!partialAllowed) return;
            len = _dataSize - _dataPos;
        }
        _streamPtr->hdr[_hdrPos].partType = type;
        if (len < _dataSize - _dataPos)
        {
            _streamPtr->hdr[_hdrPos].partMore = 1;
            _hdrPos = _dataPos;
            _dataPos++;
            _streamPtr->hdr[_hdrPos].partLen = len - 1;
            _streamPtr->hdr[_hdrPos].partType = OTA_STREAMTYPE_CMD; 
            _streamPtr->hdr[_hdrPos].partMore = 0;
        }
        else
        {
            _streamPtr->hdr[_hdrPos].partMore = 0;
        }
        fifo.popBytes(&_streamPtr->data[_dataPos], len);
        _dataPos += len;     
    }

    //pop next part from OTA packet, returns true if part was pulled
    bool _PartPop(uint8_t *type, uint8_t *dataPos, uint8_t *len) 
    {
        if (_dataPos >= _dataSize) return false;

        *type = _streamPtr->hdr[_hdrPos].partType;

        if (1 == _streamPtr->hdr[_hdrPos].partMore)
        {
            //more parts exist, get lenght from next header
            _hdrPos = _dataPos;
            _dataPos++;
            *len = _streamPtr->hdr[_hdrPos].partLen + 1;
        }
        else
        {
            //this is the last part, its length is the remainder of the package
            *len = _dataSize - _dataPos; //len is 0 when filler header was used
        }
        *dataPos = _dataPos;
        _dataPos += *len;
        return (*len > 0);
    }

    OTA_Stream_s * _streamPtr;
    uint8_t _dataSize;
    uint8_t _hdrPos;
    uint8_t _dataPos;
};
