#pragma once

#include "platform.h"
#include "RadioHalSpi.h"
#include <stdint.h>

typedef enum
{
    BW_7_80_KHZ = 0,
    BW_10_40_KHZ = 1,
    BW_15_60_KHZ = 2,
    BW_20_80_KHZ = 3,
    BW_31_25_KHZ = 4,
    BW_41_70_KHZ = 5,
    BW_62_50_KHZ = 6,
    BW_125_00_KHZ = 7,
    BW_250_00_KHZ = 8,
    BW_500_00_KHZ = 9
} Bandwidth;

typedef enum
{
    SF_6,
    SF_7,
    SF_8,
    SF_9,
    SF_10,
    SF_11,
    SF_12
} SpreadingFactor;

typedef enum
{
    CR_4_5,
    CR_4_6,
    CR_4_7,
    CR_4_8
} CodingRate;

typedef enum
{
    RFMOD_SX1278,
    RFMOD_SX1276
} RFmodule_;

#define RX_BUFFER_LEN (8)

#define SX127X_SYNC_WORD          0xC8  //  200 - default ExpressLRS sync word - 200Hz
#define SX127X_SYNC_WORD_LORAWAN  0x34  //  52  - sync word reserved for LoRaWAN networks

class SX127xDriver: public RadioHalSpi
{
public:
    SX127xDriver(HwSpi &spi, int rst = -1, int dio0 = -1, int dio1 = -1, int txpin = -1, int rxpin = -1);

    ///////Callback Function Pointers/////
    static void rx_nullCallback(uint8_t *){};
    static void tx_nullCallback(){};
    static void (*RXdoneCallback1)(uint8_t *buff); //function pointer for callback
    //static void (*RXdoneCallback2)(uint8_t *buff); //function pointer for callback
    static void (*TXdoneCallback1)(); //function pointer for callback
    static void (*TXdoneCallback2)(); //function pointer for callback
    static void (*TXdoneCallback3)(); //function pointer for callback
    static void (*TXdoneCallback4)(); //function pointer for callback

    ////////Hardware/////////////
    volatile int8_t _RXenablePin;
    volatile int8_t _TXenablePin;

    volatile uint8_t SX127x_dio0;
    volatile uint8_t SX127x_dio1;
    volatile int8_t SX127x_RST;

    /////////////////////////////

    ///////////Radio Variables////////
    volatile bool headerExplMode;

    volatile RFmodule_ RFmodule;
    volatile Bandwidth currBW;
    volatile SpreadingFactor currSF;
    volatile CodingRate currCR;
    volatile uint8_t _syncWord;
    volatile uint32_t currFreq;
    volatile uint8_t currPWR;
    ///////////////////////////////////

    /////////////Packet Stats//////////
    volatile uint32_t LastPacketIsrMicros = 0;
    volatile int16_t LastPacketRSSI;
    volatile uint8_t LastPacketRssiRaw;
    volatile int8_t LastPacketSNR;
    volatile uint8_t NonceTX;
    volatile uint8_t NonceRX;
    /////////////////////////////////

    ////////////////Configuration Functions/////////////
    void SetPins(int rst, int dio0, int dio1);
    uint8_t Begin(int txpin = -1, int rxpin = -1);
    uint8_t Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr,
                   uint32_t freq = 0, uint8_t syncWord = 0, uint8_t crc = 0);

    uint32_t getCurrBandwidth() const;
    uint8_t SetSyncWord(uint8_t syncWord);
    void SetOutputPower(uint8_t Power);
    void SetPreambleLength(uint16_t PreambleLen);
    void ICACHE_RAM_ATTR SetFrequency(uint32_t freq, uint8_t mode);
    int32_t ICACHE_RAM_ATTR GetFrequencyError();
    void ICACHE_RAM_ATTR setPPMoffsetReg(int32_t error_hz, uint32_t frf = 0);

    /////////////////Utility Funcitons//////////////////
    int16_t MeasureNoiseFloor(uint32_t num_meas, uint32_t freq);

    //////////////TX related Functions/////////////////
    uint8_t ICACHE_RAM_ATTR TX(uint8_t *data, uint8_t length);

    //////////////RX related Functions/////////////////
    uint8_t RunCAD(uint32_t timeout = 500);
    uint8_t ICACHE_RAM_ATTR RX(uint32_t freq, uint8_t *data, uint8_t length, uint32_t timeout = UINT32_MAX);

    uint8_t ICACHE_RAM_ATTR GetLastPacketRSSIUnsigned();
    int16_t ICACHE_RAM_ATTR GetLastPacketRSSI();
    int8_t ICACHE_RAM_ATTR GetLastPacketSNR();
    void ICACHE_RAM_ATTR GetLastRssiSnr();
    int16_t ICACHE_RAM_ATTR GetCurrRSSI() const;

    ////////////Non-blocking TX related Functions/////////////////
    void ICACHE_RAM_ATTR TXnb(const uint8_t *data, uint8_t length, uint32_t freq = 0);
    void ICACHE_RAM_ATTR TXnbISR(uint8_t irqs); //ISR for non-blocking TX routine

    /////////////Non-blocking RX related Functions///////////////
    void ICACHE_RAM_ATTR StopContRX();
    void ICACHE_RAM_ATTR RXnb(uint32_t freq = 0);
    void ICACHE_RAM_ATTR RXnbISR(uint8_t irqs); //ISR for non-blocking RC routine

    uint8_t ICACHE_RAM_ATTR GetIRQFlags();
    void ICACHE_RAM_ATTR ClearIRQFlags();

private:
    volatile uint32_t p_freqOffset = 0;
    volatile uint8_t p_ppm_off = 0;
    //volatile uint8_t p_isr_mask = 0;
    volatile uint8_t p_last_payload_len = 0;

    uint8_t CheckChipVersion();
    void SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord, uint8_t crc);
    void ICACHE_RAM_ATTR SetMode(uint8_t mode);

    void ICACHE_RAM_ATTR RxConfig(uint32_t freq);

    inline __attribute__((always_inline)) void ICACHE_RAM_ATTR _change_mode_val(uint8_t mode);
    void ICACHE_RAM_ATTR reg_op_mode_mode_lora(void);
    void ICACHE_RAM_ATTR reg_dio1_rx_done(void);
    void ICACHE_RAM_ATTR reg_dio1_tx_done(void);
    void ICACHE_RAM_ATTR reg_dio1_isr_mask_write(uint8_t mask);
};

extern SX127xDriver Radio;
