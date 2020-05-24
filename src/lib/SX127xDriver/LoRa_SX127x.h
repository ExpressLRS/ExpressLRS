#pragma once

#include <Arduino.h>
#include "../../src/targets.h"
#include "SX127x_Regs.h"
#include "SX127xHal.h"

#ifdef PLATFORM_ESP8266
#include <cstdint>
#endif

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

    ///////////Radio Variables////////
    uint8_t TXbuffLen = 8;
    uint8_t RXbuffLen = 8;

    static volatile WORD_ALIGNED_ATTR uint8_t TXdataBuffer[256];
    static volatile WORD_ALIGNED_ATTR uint8_t RXdataBuffer[256];

    bool headerExplMode = false;

    //// Default Parameters ////
    uint32_t currFreq = 915000000;
    uint8_t currSyncWord = SX127X_SYNC_WORD;
    RFmodule_ RFmodule = RFMOD_SX1276;
    Bandwidth currBW = BW_500_00_KHZ;
    SpreadingFactor currSF = SF_6;
    CodingRate currCRCR_4_5;
    uint8_t currPWR = 0b0000;
    uint8_t maxPWR = 0b1111;
    RadioOPmodes _opmode = CURR_OPMODE_SLEEP;
    uint8_t currOpmode = (uint8_t)CURR_OPMODE_SLEEP;
    ///////////////////////////////////

    /////////////Packet Stats//////////
    int8_t LastPacketRSSI;
    int8_t LastPacketSNR;
    volatile uint8_t NonceTX;
    volatile uint8_t NonceRX;
    uint32_t TimeOnAir;
    uint32_t TXstartMicros;
    uint32_t TXspiTime;
    uint32_t HeadRoom;
    uint32_t LastTXdoneMicros;
    uint32_t TXdoneMicros;
    bool IRQneedsClear = true;
    /////////////////////////////////

    ////////////////Configuration Functions/////////////
    uint8_t Begin();
    uint8_t DetectChip();
    uint8_t Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord);
    uint8_t SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord);
    uint8_t SetMode(uint8_t mode);
    void ConfigLoraDefaults();

    uint8_t SetBandwidth(Bandwidth bw);
    uint32_t GetCurrBandwidth();
    uint32_t GetCurrBandwidthNormalisedShifted();
    uint8_t SetSyncWord(uint8_t syncWord);
    uint8_t SetOutputPower(uint8_t Power);
    uint8_t SetPreambleLength(uint8_t PreambleLen);
    uint8_t SetSpreadingFactor(SpreadingFactor sf);
    uint8_t SetCodingRate(CodingRate cr);
    uint8_t SetFrequency(uint32_t freq);
    int32_t GetFrequencyError();
    bool GetFrequencyErrorbool();
    void SetPPMoffsetReg(int32_t offset);

    ////////////////////////////////////////////////////

    /////////////////Utility Funcitons//////////////////
    void ClearIRQFlags();

    //////////////RX related Functions/////////////////

    static uint8_t RunCAD();

    uint8_t ICACHE_RAM_ATTR UnsignedGetLastPacketRSSI();
    int8_t ICACHE_RAM_ATTR GetLastPacketRSSI();
    int8_t ICACHE_RAM_ATTR GetLastPacketSNR();
    int8_t ICACHE_RAM_ATTR GetCurrRSSI();

    static void nullCallback(void);
    ////////////Non-blocking TX related Functions/////////////////
    static void ICACHE_RAM_ATTR TXnb(const volatile uint8_t *data, uint8_t length);
    static void ICACHE_RAM_ATTR TXnbISR(); //ISR for non-blocking TX routine
    /////////////Non-blocking RX related Functions///////////////
    static void ICACHE_RAM_ATTR RXnb();
    static void ICACHE_RAM_ATTR RXnbISR(); //ISR for non-blocking RC routin

private:
};
