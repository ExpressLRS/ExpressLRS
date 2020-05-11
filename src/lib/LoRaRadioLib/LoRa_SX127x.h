#pragma once

#include <Arduino.h>
#include "../../src/targets.h"

#include "LoRa_SX127x_Regs.h"

#ifdef PLATFORM_ESP32
#include "FreeRTOS.h"
#include "esp32-hal-timer.h"
#endif

#ifdef PLATFORM_8266
#include <cstdint>
#endif

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
typedef enum
{
    CONT_OFF,
    CONT_TX,
    CONT_RX
} ContinousMode;
typedef enum
{
    RADIO_IDLE,
    RADIO_BUSY
} RadioState_;

class SX127xDriver
{

public:
    ///////Callback Function Pointers/////
    static void (*RXdoneCallback1)(); //function pointer for callback
    static void (*RXdoneCallback2)(); //function pointer for callback
    static void (*TXdoneCallback1)(); //function pointer for callback
    static void (*TXdoneCallback2)(); //function pointer for callback
    static void (*TXdoneCallback3)(); //function pointer for callback
    static void (*TXdoneCallback4)(); //function pointer for callback

    static void (*TXtimeout)(); //function pointer for callback
    static void (*RXtimeout)(); //function pointer for callback

    static void (*TimerDoneCallback)(); //function pointer for callback

#ifdef PLATFORM_ESP32
    static TaskHandle_t Timertask_handle; //Task Handle for ContTX mode
#endif

    //static void (*TXcallback)();

    ////////Hardware/////////////
    static uint8_t _RXenablePin;
    static uint8_t _TXenablePin;

    static uint8_t SX127x_nss;
    static uint8_t SX127x_dio0;
    static uint8_t SX127x_dio1;

    static uint8_t SX127x_MOSI;
    static uint8_t SX127x_MISO;
    static uint8_t SX127x_SCK;
    static uint8_t SX127x_RST;

    static bool HighPowerModule;

    /////////////////////////////

    ///////////Radio Variables////////
    static volatile uint8_t TXdataBuffer[256];
    static volatile uint8_t RXdataBuffer[256];

    static volatile uint8_t TXbuffLen;
    static volatile uint8_t RXbuffLen;

    static volatile uint32_t PacketCount;

    static volatile bool headerExplMode;

    static volatile uint32_t TimerInterval; //20ms default for now.

    static uint32_t currFreq;
    static uint8_t _syncWord;

    static ContinousMode ContMode;
    static RFmodule_ RFmodule;
    static Bandwidth currBW;
    static SpreadingFactor currSF;
    static CodingRate currCR;
    static uint8_t currPWR;
    static uint8_t maxPWR;
    static RadioOPmodes _opmode;
    static RadioState_ RadioState;
    ///////////////////////////////////

    /////////////Packet Stats//////////
    static int8_t LastPacketRSSI;
    static int8_t LastPacketSNR;
    static float PacketLossRate;
    static volatile uint8_t NonceTX;
    static volatile uint8_t NonceRX;
    static uint32_t TimeOnAir;
    static uint32_t TXstartMicros;
    static uint32_t TXspiTime;
    static uint32_t HeadRoom;
    static uint32_t LastTXdoneMicros;
    static uint32_t TXdoneMicros;
    /////////////////////////////////

    ////////////////Configuration Functions/////////////
    static uint8_t Begin();
    static uint8_t Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord);
    static uint8_t SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord);

    static uint8_t SetBandwidth(Bandwidth bw);
    static uint32_t getCurrBandwidth();
    static uint32_t getCurrBandwidthNormalisedShifted();
    static uint8_t SetSyncWord(uint8_t syncWord);
    static uint8_t SetOutputPower(uint8_t Power);
    static uint8_t SetPreambleLength(uint8_t PreambleLen);
    static uint8_t SetSpreadingFactor(SpreadingFactor sf);
    static uint8_t SetCodingRate(CodingRate cr);
    static uint8_t SetFrequency(uint32_t freq);
    static int32_t GetFrequencyError();
    static void setPPMoffsetReg(int32_t offset);

    static uint8_t SX127xBegin();
    static void ConfigLoraDefaults();
    static uint8_t SetMode(uint8_t mode);
    static uint8_t currOpmode;
    static uint8_t TX(uint8_t *data, uint8_t length);
    ////////////////////////////////////////////////////

    /////////////////Utility Funcitons//////////////////
    static void ClearIRQFlags();

    //////////////RX related Functions/////////////////

    static uint8_t RunCAD();

    static uint8_t ICACHE_RAM_ATTR UnsignedGetLastPacketRSSI();

    static int8_t ICACHE_RAM_ATTR GetLastPacketRSSI();
    static int8_t ICACHE_RAM_ATTR GetLastPacketSNR();
    static int8_t ICACHE_RAM_ATTR GetCurrRSSI();

    ////////////Non-blocking TX related Functions/////////////////
    static void nullCallback(void);

    static void ICACHE_RAM_ATTR StartTimerTask(); //Start Cont TX mode, sends data continuiously
    static void ICACHE_RAM_ATTR StopTimerTask();
    static void ICACHE_RAM_ATTR UpdateTimerInterval();
    static uint8_t ICACHE_RAM_ATTR TXnb(const volatile uint8_t *data, uint8_t length);

    static void ICACHE_RAM_ATTR TXnbISR(); //ISR for non-blocking TX routine
    static void ICACHE_RAM_ATTR TimerTask_ISRhandler();
    static void ICACHE_RAM_ATTR TimerTask(void *param);

    /////////////Non-blocking RX related Functions///////////////
    static void ICACHE_RAM_ATTR StopContRX();
    static void ICACHE_RAM_ATTR RXnb();

    static void ICACHE_RAM_ATTR RXnbISR(); //ISR for non-blocking RC routine

    static uint8_t ICACHE_RAM_ATTR RXsingle(uint8_t *data, uint8_t length);
    static uint8_t ICACHE_RAM_ATTR RXsingle(uint8_t *data, uint8_t length, uint32_t timeout);

private:
};
