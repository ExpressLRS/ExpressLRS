#pragma once

#include "platform.h"
#include "LoRa_SX127x_Regs.h"
#include <stdint.h>

typedef enum
{
    CURR_OPMODE_FSK_OOK = 0b00000000,
    CURR_OPMODE_LORA = 0b10000000, //removed CURR_OPMODE_ACCESS_SHARED_REG_OFF and CURR_OPMODE_ACCESS_SHARED_REG_ON for now
    CURR_OPMODE_SLEEP = 0b00000000,
    CURR_OPMODE_STANDBY = 0b00000001,
    CURR_OPMODE_FSTX = 0b00000010,
    CURR_OPMODE_TX = 0b00000011,
    CURR_OPMODE_FSRX = 0b00000100,
    CURR_OPMODE_RXCONTINUOUS = 0b00000101,
    CURR_OPMODE_RXSINGLE = 0b00000110,
    CURR_OPMODE_CAD = 0b00000111
} RadioOPmodes;

typedef enum
{
    CH_SX1272,
    CH_SX1273,
    CH_SX1276,
    CH_SX1277,
    CH_SX1278,
    CH_SX1279
} Chip;
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

/*typedef enum
{
    RADIO_IDLE,
    RADIO_BUSY
} RadioState_;
*/

class SX127xDriver
{

public:
    SX127xDriver();

    ///////Callback Function Pointers/////
    static void rx_nullCallback(volatile uint8_t *){};
    static void tx_nullCallback(){};
    static void (*RXdoneCallback1)(volatile uint8_t *buff); //function pointer for callback
    //static void (*RXdoneCallback2)(); //function pointer for callback
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
    //volatile uint8_t TXdataBuffer[16];
    volatile uint32_t __RXdataBuffer[16 / 4]; // ESP requires aligned buffer
    //volatile uint8_t RXdataBuffer[16];
    volatile uint8_t *RXdataBuffer = (uint8_t *)&__RXdataBuffer;

    volatile uint8_t TXbuffLen;
    volatile uint8_t RXbuffLen;

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
    //volatile uint32_t LastPacketIsrMicros;
    volatile uint8_t LastPacketRssiRaw;
    volatile int8_t LastPacketRSSI;
    volatile int8_t LastPacketSNR;
    volatile uint8_t NonceTX;
    volatile uint8_t NonceRX;
    /////////////////////////////////

    ////////////////Configuration Functions/////////////
    void SetPins(int rst, int dio0, int dio1);
    uint8_t Begin(int txpin = -1, int rxpin = -1);
    uint8_t Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq = 0, uint8_t syncWord = 0);
    // Don't call SX127xConfig directly from app! use Config instead
    uint8_t SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord);

    uint8_t SetBandwidth(Bandwidth bw);
    uint32_t getCurrBandwidth();
    uint32_t getCurrBandwidthNormalisedShifted();
    uint8_t SetSyncWord(uint8_t syncWord);
    uint8_t SetOutputPower(uint8_t Power);
    uint8_t SetPreambleLength(uint16_t PreambleLen);
    uint8_t SetSpreadingFactor(SpreadingFactor sf);
    uint8_t SetCodingRate(CodingRate cr);
    uint8_t ICACHE_RAM_ATTR SetFrequency(uint32_t freq, uint8_t mode = SX127X_STANDBY);
    int32_t ICACHE_RAM_ATTR GetFrequencyError();
    void ICACHE_RAM_ATTR setPPMoffsetReg(int32_t offset);
    void ICACHE_RAM_ATTR setPPMoffsetReg(int32_t error_hz, uint32_t frf);

    uint8_t SX127xBegin();
    uint8_t SetMode(uint8_t mode);
    uint8_t GetMode(void)
    {
        return (p_RegOpMode & SX127X_CAD);
    }

    /////////////////Utility Funcitons//////////////////
    void ICACHE_RAM_ATTR ClearIRQFlags();

    //////////////TX related Functions/////////////////
    uint8_t ICACHE_RAM_ATTR TX(uint8_t *data, uint8_t length);

    //////////////RX related Functions/////////////////
    uint8_t RunCAD();
    uint8_t ICACHE_RAM_ATTR RXsingle(uint8_t *data, uint8_t length);
    uint8_t ICACHE_RAM_ATTR RXsingle(uint8_t *data, uint8_t length, uint32_t timeout);

    uint8_t ICACHE_RAM_ATTR GetLastPacketRSSIUnsigned();
    int8_t ICACHE_RAM_ATTR GetLastPacketRSSI();
    int8_t ICACHE_RAM_ATTR GetLastPacketSNR();
    int8_t ICACHE_RAM_ATTR GetCurrRSSI() const;

    ////////////Non-blocking TX related Functions/////////////////
    uint8_t ICACHE_RAM_ATTR TXnb(const uint8_t *data, uint8_t length, uint32_t freq = 0);
    void ICACHE_RAM_ATTR TXnbISR(); //ISR for non-blocking TX routine

    /////////////Non-blocking RX related Functions///////////////
    void ICACHE_RAM_ATTR StopContRX();
    void ICACHE_RAM_ATTR RXnb(uint32_t freq = 0);
    void ICACHE_RAM_ATTR RXnbISR(); //ISR for non-blocking RC routine

private:
    volatile uint8_t p_RegOpMode = 0;
    //volatile uint8_t p_RegDioMapping1 = 0;
    //volatile uint8_t p_RegDioMapping2 = 0;
    volatile uint8_t p_ppm_off = 0;

    void ICACHE_RAM_ATTR reg_op_mode_mode_lora(void);
    void ICACHE_RAM_ATTR reg_dio1_rx_done(void);
    void ICACHE_RAM_ATTR reg_dio1_tx_done(void);
};

extern SX127xDriver Radio;
