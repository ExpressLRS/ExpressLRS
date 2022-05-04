#pragma once

#include <targets.h>


class SX12xxDriverCommon
{
public:
    typedef uint8_t rx_status;
    enum
    {
        SX12XX_RX_OK             = 0,
        SX12XX_RX_CRC_FAIL       = 1 << 0,
        SX12XX_RX_TIMEOUT        = 1 << 1,
        SX12XX_RX_SYNCWORD_ERROR = 1 << 2,
    };

    SX12xxDriverCommon():
        RXdoneCallback(nullCallbackRx),
        TXdoneCallback(nullCallbackTx) {}

    static bool ICACHE_RAM_ATTR nullCallbackRx(rx_status) {return false;}
    static void ICACHE_RAM_ATTR nullCallbackTx() {}

    ///////Callback Function Pointers/////
    bool (*RXdoneCallback)(rx_status crcFail); //function pointer for callback
    void (*TXdoneCallback)(); //function pointer for callback

    #define TXRXBuffSize 16
    volatile WORD_ALIGNED_ATTR uint8_t TXdataBuffer[TXRXBuffSize];
    volatile WORD_ALIGNED_ATTR uint8_t RXdataBuffer[TXRXBuffSize];

    ///////////Radio Variables////////
    uint32_t currFreq;  // This actually the reg value! TODO fix the naming!
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
