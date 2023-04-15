#pragma once

#include <cstdint>
#include "OTA.h"
#include "FIFO_GENERIC.h"

//TODO: replace FIFO_GENERIC with FIFO_unsafe

// Non-thread safe FIFO class
// Notes:
// 1) Can simultanously write from one thread, and read from another thread. 
// 2) Can NOT simultanously write from more than one thread
// 3) Can NOT simultanously read from more than one thread.
// 4) Has no flush() method, as this would violate Note 1
// 5) Actual number of elements the buffer can hold is fifo_size-1
class FIFO_unsafe
{
private:
    uint16_t head;
    uint16_t tail;
    uint8_t *buffer;
    uint16_t fifo_size;

public:
    //use external buffer. 
    FIFO_unsafe(uint8_t *buf, uint16_t len)
    {
        head = 0;
        tail = 0;       
        buffer = buf;
        fifo_size = len;
    }

    ICACHE_RAM_ATTR uint16_t size()
    {
        return (tail - head) % fifo_size;
    }

    ICACHE_RAM_ATTR uint16_t free()
    {
        return fifo_size - 1 - size();
    }

    ICACHE_RAM_ATTR bool push(const uint8_t data)
    {
        if (free() < 1) return false;

        buffer[tail] = data;
        tail = (tail + 1) % fifo_size;
    }

    ICACHE_RAM_ATTR int16_t pop()
    {
        if (size() < 1) return -1;

        uint8_t data = buffer[head];
        head = (head + 1) % fifo_size;
        return data;
    }

    ICACHE_RAM_ATTR bool pushBytes(const uint8_t *data, uint16_t len)
    {
        if (free() < len) return false;

        for (uint8_t i = 0; i < len; i++) poke(i, data[i]);
        tail = (tail + len) % fifo_size;
        return true;
    }

    ICACHE_RAM_ATTR bool popBytes(uint8_t *data, uint8_t len)
    {
        if (size() < len) return false;

        for (uint8_t i = 0; i < len; i++) data[i] = peek(i);
        head = (head + len) % fifo_size;
        return true;
    }

    // push message as "sync len data" to buffer 
    ICACHE_RAM_ATTR bool pushMessage(const uint8_t *data, uint8_t len)
    {
        if (free() < len + 2 || len == 0) return false;

        poke(0, 'S');
        poke(1, len);       
        for (uint8_t i = 0; i < len; i++) poke(i+2, data[i]);
        tail = (tail + len + 2) % fifo_size;
        return true;
    } 

    ICACHE_RAM_ATTR uint8_t popMessage(uint8_t *data, uint8_t maxlen)
    {
        while (size() >= 3)
        {
            uint8_t len = peek(1);
            if (peek(0) == 'S' && len <= maxlen && len >= 1)
            {
                if (size() < len + 2) 
                {
                    //found header, but not complete message - should not happen as pushMessage is atomic ...
                    return 0;
                }
                else
                {
                    for (uint8_t i = 0; i < len; i++) data[i] = peek(i + 2);
                    head = (head + len + 2) % fifo_size;
                    return len;             
                }
            }
            //something is wrong, buffer does not start with message - pop a byte and try again
            pop();
        }
        return 0;
    }

    //low level peek(head), no checks
    ICACHE_RAM_ATTR uint8_t peek(int16_t pos)
    {
        return buffer[(head + pos) % fifo_size];
    }

    //low level poke(tail), no checks
    ICACHE_RAM_ATTR void poke(int16_t pos, uint8_t value)
    {
        buffer[(tail + pos) % fifo_size] = value;
    } 
};

enum ackState{ACK,NACK,UNKNOWN};

class StreamTxRx
{
public:
    enum CmdType{NOP,LINKSTAT,BIND,SET_RX_WIFI_MODE,SET_RX_LOAN_MODE,CMD_LAST};
//command          - number of argument bytes
//LINKSTAT         - 4 (OTA_LinkStats_s)
//BIND             - 4
//SET_RX_WIFI_MODE - 0
//SET_RX_LOAN_MODE - 0

protected:
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
    void _PartPush(uint8_t type, FIFO_GENERIC<96>& fifo, bool partialAllowed)
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

class StreamSender : StreamTxRx
{
public:
    FIFO_GENERIC<96> data;
    FIFO_GENERIC<96> data2;    
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
        return cmdFifo.size() > 0 || data.size() > 0 || data2.size() > 0; 
    }
    void SetCmd(StreamTxRx::CmdType cmd, uint8_t * cmdArgs, uint8_t cmdArgsLen)
    {
        if (cmd == StreamTxRx::CmdType::NOP || cmd >= StreamTxRx::CmdType::CMD_LAST) return;
        cmdFifo.flush();
        cmdFifo.push(cmd);
        cmdFifo.pushBytes(cmdArgs, cmdArgsLen);
    }
private:
    uint8_t crsfSync; 
    uint8_t packetType;
    uint8_t seq;
    FIFO_GENERIC<96> cmdFifo;  
};
