#pragma once

#include <Arduino.h>
#include "SX127xRegs.h"
#include "SX127xHal.h"

#ifdef PLATFORM_ESP8266
#include <cstdint>
#endif

class SX127xDriver
{

public:
    static SX127xDriver *instance;
    SX127xDriver();
    ///////Callback Function Pointers/////
    static void inline nullCallback(void);
    
    static void (*RXdoneCallback)(); //function pointer for callback
    static void (*TXdoneCallback)(); //function pointer for callback

    static void (*TXtimeout)(); //function pointer for callback
    static void (*RXtimeout)(); //function pointer for callback

///////////Radio Variables////////
    #define TXRXBuffSize 8
    const uint8_t TXbuffLen = TXRXBuffSize; //TODO might not always be const
    const uint8_t RXbuffLen = TXRXBuffSize;

    static volatile WORD_ALIGNED_ATTR uint8_t TXdataBuffer[TXRXBuffSize];
    static volatile WORD_ALIGNED_ATTR uint8_t RXdataBuffer[TXRXBuffSize];

    bool headerExplMode = false;
    bool crcEnabled = false;

    //// Parameters ////
    uint32_t currFreq = 0; // leave as 0 to ensure that it gets set
    uint8_t currSyncWord = SX127X_SYNC_WORD;
    uint8_t currPreambleLen = 0;
    SX127x_Bandwidth currBW = SX127x_BW_125_00_KHZ; //default values from datasheet
    SX127x_SpreadingFactor currSF = SX127x_SF_7;
    SX127x_CodingRate currCR = SX127x_CR_4_5;
    SX127x_RadioOPmodes currOpmode = SX127x_OPMODE_SLEEP;
    uint8_t currPWR = 0b0000;
    SX127x_ModulationModes ModFSKorLoRa = SX127x_OPMODE_LORA;

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
    void Begin();
    void End();
    void DetectChip();
    void Config(SX127x_Bandwidth bw, SX127x_SpreadingFactor sf, SX127x_CodingRate cr, uint32_t freq, uint8_t preambleLen, uint8_t syncWord);
    void Config(SX127x_Bandwidth bw, SX127x_SpreadingFactor sf, SX127x_CodingRate cr, uint8_t preambleLen);
    void SetMode(SX127x_RadioOPmodes mode);
    void ConfigLoraDefaults();

    void SetBandwidthCodingRate(SX127x_Bandwidth bw, SX127x_CodingRate cr);

    void SetSyncWord(uint8_t syncWord);
    void SetOutputPower(uint8_t Power);
    void SetPreambleLength(uint8_t PreambleLen);
    void SetSpreadingFactor(SX127x_SpreadingFactor sf);

    uint32_t GetCurrBandwidth();
    uint32_t GetCurrBandwidthNormalisedShifted();

    void SetFrequency(uint32_t freq);
    int32_t GetFrequencyError();
    bool GetFrequencyErrorbool();
    void SetPPMoffsetReg(int32_t offset);

    ////////////////////////////////////////////////////

    /////////////////Utility Funcitons//////////////////
    void ICACHE_RAM_ATTR ClearIRQFlags();

    //////////////RX related Functions/////////////////

    //uint8_t RunCAD();

    uint8_t ICACHE_RAM_ATTR UnsignedGetLastPacketRSSI();
    int8_t ICACHE_RAM_ATTR GetLastPacketRSSI();
    int8_t ICACHE_RAM_ATTR GetLastPacketSNR();
    int8_t ICACHE_RAM_ATTR GetCurrRSSI();

    ////////////Non-blocking TX related Functions/////////////////
    static void ICACHE_RAM_ATTR TXnb(uint8_t volatile *data, uint8_t length);
    static void ICACHE_RAM_ATTR TXnbISR(); //ISR for non-blocking TX routine
    /////////////Non-blocking RX related Functions///////////////
    static void ICACHE_RAM_ATTR RXnb();
    static void ICACHE_RAM_ATTR RXnbISR(); //ISR for non-blocking RC routin

private:
};
