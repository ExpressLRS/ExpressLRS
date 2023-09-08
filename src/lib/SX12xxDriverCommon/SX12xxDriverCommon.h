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

    SX12XX_Radio_Number_t processingPacketRadio;
    SX12XX_Radio_Number_t lastSuccessfulPacketRadio;
    SX12XX_Radio_Number_t transmittingRadio;
    SX12XX_Radio_Number_t GetProcessingPacketRadio() { return processingPacketRadio; }
    SX12XX_Radio_Number_t GetLastSuccessfulPacketRadio() { return lastSuccessfulPacketRadio; }
    SX12XX_Radio_Number_t GetLastTransmitRadio() {return transmittingRadio; }

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

    /**
     * @brief Calculates the reported SNR value using a fuzzy logic approach
     *
     * This function computes the reported SNR based on two input SNR values (snr1 and snr2) and a threshold value.
     * The reported SNR is determined by a smooth transition between the lower value of the two input SNRs and their average.
     * The transition is controlled by the difference between the input SNRs and the threshold value.
     *
     * A polynomial approximation is used to create a smooth S-shaped curve that maps the difference between the input SNRs
     * to a value between 0 and 1. This approximation enables faster computation while maintaining a similar transition effect.
     * This value is then used to interpolate between the lower value and the average value, providing a smooth transition between the two conditions.
     *
     * @param snr1 The first SNR value
     * @param snr2 The second SNR value
     * @param threshold The threshold value to control the transition between the lower value and the average value reporting strategy
     * @return The reported SNR value, which is either the lower value of the two input SNRs, their average, or a value in between, depending on the difference between the input SNRs and the threshold value
     */
    int8_t fuzzy_snr(int16_t snr1, int16_t snr2, int16_t threshold)
    {
        if (threshold == 0)
        {
            return (int8_t)(snr1 < snr2 ? snr1 : snr2);
        }

        int16_t diff = (int16_t)abs(snr1 - snr2) << 4;
        int16_t lower_value = (int16_t)(snr1 < snr2 ? snr1 : snr2) << 4;
        int16_t average_value = ((int16_t)snr1 + snr2) << 3;

        int16_t threshold_scaled = (int16_t)threshold << 4;

        if (diff < threshold_scaled)
        {
            return (int8_t)lower_value >> 4;
        }
        else if (diff > threshold_scaled * 2)
        {
            return (int8_t)average_value >> 4;
        }
        else
        {
            int16_t transition_value = (diff - threshold_scaled) / threshold_scaled;
            return (int8_t)((lower_value * (16 - transition_value) + average_value * transition_value) >> 8);
        }
    }
};