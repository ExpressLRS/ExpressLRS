#pragma once

#include <targets.h>

typedef uint8_t SX12XX_Radio_Number_t;
enum
{
    SX12XX_Radio_NONE = 0b00000000,     // Bit mask for no radio
    SX12XX_Radio_1    = 0b00000001,     // Bit mask for radio 1
    SX12XX_Radio_2    = 0b00000010,     // Bit mask for radio 2
    SX12XX_Radio_All  = 0b00000011      // bit mask for both radios
};

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

    #define RXBuffSize 16
    WORD_ALIGNED_ATTR uint8_t RXdataBuffer[RXBuffSize];

    ///////////Radio Variables////////
    uint32_t currFreq;  // This actually the reg value! TODO fix the naming!
    uint8_t PayloadLength;
    bool IQinverted;

    /////////////Packet Stats//////////
    int8_t LastPacketRSSI;
    int8_t LastPacketRSSI2;
    int8_t LastPacketSNRRaw; // in RADIO_SNR_SCALE units
    int8_t FuzzySNRThreshold;

    bool isFirstRxIrq = true;

#if defined(DEBUG_RCVR_SIGNAL_STATS)
    typedef struct rxSignalStats_s
    {
        uint16_t irq_count;
        uint16_t telem_count;
        int32_t rssi_sum;
        int32_t snr_sum;
        int8_t snr_max;
        uint16_t fail_count;
    } rxSignalStats_t;

    rxSignalStats_t rxSignalStats[2];
    uint16_t irq_count_or;
    uint16_t irq_count_both;
#endif

protected:
    void RemoveCallbacks(void)
    {
        RXdoneCallback = nullCallbackRx;
        TXdoneCallback = nullCallbackTx;
    }
};
