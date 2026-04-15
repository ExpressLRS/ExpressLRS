#pragma once

#include <targets.h>
#include "FEC.h"

typedef uint8_t SX12XX_Radio_Number_t;
enum
{
    SX12XX_Radio_NONE = 0b00000000,     // Bit mask for no radio
    SX12XX_Radio_1    = 0b00000001,     // Bit mask for radio 1
    SX12XX_Radio_2    = 0b00000010,     // Bit mask for radio 2
    SX12XX_Radio_All  = 0b00000011      // bit mask for both radios
};

namespace RadioBandMod {
    static constexpr uint8_t MOD_SHIFT = 4;
    static constexpr uint8_t BAND_MASK = (1 << MOD_SHIFT) - 1; // 0x0F
    static constexpr uint8_t MOD_MASK  = ~BAND_MASK;           // 0xF0

    enum Band : uint8_t { B900 = 0, B2G4 = 1, BDUAL = 2 };
    enum Modulation : uint8_t { LORA = 0, FLRC = 1, GFSK = 2 };

    static constexpr uint8_t pack(Modulation m, Band b) {
        return (static_cast<uint8_t>(m) << MOD_SHIFT) | static_cast<uint8_t>(b);
    }

    static constexpr Modulation getModulation(uint8_t t)  { return static_cast<Modulation>((t & MOD_MASK) >> MOD_SHIFT); }
    static constexpr Band getBand(uint8_t t) { return static_cast<Band>(t & BAND_MASK); }

    enum Combined : uint8_t {
        LORA_900  = pack(LORA, B900),
        LORA_2G4  = pack(LORA, B2G4),
        LORA_DUAL = pack(LORA, BDUAL),
        FLRC_900  = pack(FLRC, B900),
        FLRC_2G4  = pack(FLRC, B2G4),
        GFSK_900  = pack(GFSK, B900),
        GFSK_2G4  = pack(GFSK, B2G4),
    };

    // uint8_t used here as common type to allow Band, Modulation, or Combined to be passed
    static constexpr bool isLoRa(uint8_t t) { return getModulation(t) == LORA; }
    static constexpr bool isFLRC(uint8_t t) { return getModulation(t) == FLRC; }
    static constexpr bool isGFSK(uint8_t t) { return getModulation(t) == GFSK; }

    static constexpr bool isB900(uint8_t t) { return getBand(t) == B900; }
    static constexpr bool isB2G4(uint8_t t) { return getBand(t) == B2G4; }
    static constexpr bool isBDUAL(uint8_t t) { return getBand(t) == BDUAL; }
    static constexpr bool isSameBand(uint8_t a, uint8_t b) { return getBand(a) == getBand(b); }
}

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
    WORD_ALIGNED_ATTR uint8_t RXdataBufferSecond[RXBuffSize];

    ///////////Radio Variables////////
    uint32_t currFreq;  // This actually the reg value! TODO fix the naming!
    uint8_t PayloadLength;
    bool IQinverted;

    SX12XX_Radio_Number_t processingPacketRadio;
    SX12XX_Radio_Number_t transmittingRadio;
    SX12XX_Radio_Number_t strongestReceivingRadio;
    SX12XX_Radio_Number_t GetProcessingPacketRadio() { return processingPacketRadio; }
    SX12XX_Radio_Number_t GetStrongestReceivingRadio() { return strongestReceivingRadio; }
    SX12XX_Radio_Number_t GetLastTransmitRadio() {return transmittingRadio; }

    /////////////Packet Stats//////////
    int8_t LastPacketRSSI;
    int8_t LastPacketRSSI2;
    int8_t LastPacketSNRRaw; // in RADIO_SNR_SCALE units
    int8_t FuzzySNRThreshold;
    bool gotRadio[2] = {false, false};
    bool hasSecondRadioGotData = false;

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
            return (int8_t)(lower_value >> 4);
        }
        else if (diff > threshold_scaled * 2)
        {
            return (int8_t)(average_value >> 4);
        }
        else
        {
            int16_t transition_value = (diff - threshold_scaled) / threshold_scaled;
            return (int8_t)((lower_value * (16 - transition_value) + average_value * transition_value) >> 8);
        }
    }
};