#pragma once

#include <targets.h>


class SX12xxDriverCommon
{
public:
    typedef uint8_t rx_status;
    enum
    {
        SX12XX_RX_OK        = 0,
        SX12XX_RX_CRC_FAIL  = 1 << 0,
        SX12XX_RX_TIMEOUT   = 1 << 1,
    };

    SX12xxDriverCommon():
        RXdoneCallback(nullCallbackRx),
        TXdoneCallback(nullCallbackTx) {}

    static void ICACHE_RAM_ATTR nullCallbackRx(rx_status) {}
    static void ICACHE_RAM_ATTR nullCallbackTx() {}

    ///////Callback Function Pointers/////
    void (*RXdoneCallback)(rx_status crcFail); //function pointer for callback
    void (*TXdoneCallback)(); //function pointer for callback

    #define RXBuffSize 16
    WORD_ALIGNED_ATTR uint8_t RXdataBuffer[RXBuffSize];

    ///////////Radio Variables////////
    uint32_t currFreq;
    uint8_t PayloadLength;
    bool IQinverted;

    /////////////Packet Stats//////////
    int8_t LastPacketRSSI;
    int8_t LastPacketSNR;

protected:
    void RemoveCallbacks(void)
    {
        RXdoneCallback = nullCallbackRx;
        TXdoneCallback = nullCallbackTx;
    }
};
