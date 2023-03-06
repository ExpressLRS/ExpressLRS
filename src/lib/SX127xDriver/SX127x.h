#pragma once

#include "SX127xRegs.h"
#include "SX127xHal.h"
#include "SX12xxDriverCommon.h"

#ifdef PLATFORM_ESP8266
#include <cstdint>
#endif

#define RADIO_SNR_SCALE 4

class SX127xDriver: public SX12xxDriverCommon
{

public:
    static SX127xDriver *instance;

    ///////////Radio Variables////////
    bool headerExplMode;
    bool crcEnabled;

    //// Parameters ////
    uint16_t timeoutSymbols;
    ///////////////////////////////////

    ////////////////Configuration Functions/////////////
    SX127xDriver();
    bool Begin();
    void End();
    bool DetectChip(SX12XX_Radio_Number_t radioNumber);
    void Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t preambleLen, uint8_t syncWord, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval);
    void Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t preambleLen, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval);
    void SetMode(SX127x_RadioOPmodes mode, SX12XX_Radio_Number_t radioNumber);
    void SetTxIdleMode() { SetMode(SX127x_OPMODE_STANDBY, SX12XX_Radio_All); } // set Idle mode used when switching from RX to TX
    void ConfigLoraDefaults();

    void SetBandwidthCodingRate(SX127x_Bandwidth bw, SX127x_CodingRate cr);
    void SetCRCMode(bool on); //false for off
    void SetSyncWord(uint8_t syncWord);
    void SetOutputPower(uint8_t Power);
    void SetPreambleLength(uint8_t PreambleLen);
    void SetSpreadingFactor(SX127x_SpreadingFactor sf);
    void SetRxTimeoutUs(uint32_t interval);

    uint32_t GetCurrBandwidth();
    uint32_t GetCurrBandwidthNormalisedShifted();

    #define FREQ_STEP 61.03515625
    void SetFrequencyHz(uint32_t freq, SX12XX_Radio_Number_t radioNumber);
    void SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber = SX12XX_Radio_All);
    bool FrequencyErrorAvailable() const { return true; }
    int32_t GetFrequencyError();
    bool GetFrequencyErrorbool();
    void SetPPMoffsetReg(int32_t offset);

    ////////////////////////////////////////////////////

    /////////////////Utility Funcitons//////////////////
    uint8_t GetIrqFlags(SX12XX_Radio_Number_t radioNumber);
    void ClearIrqFlags(SX12XX_Radio_Number_t radioNumber);

    //////////////RX related Functions/////////////////

    //uint8_t RunCAD();

    uint8_t UnsignedGetLastPacketRSSI(SX12XX_Radio_Number_t radioNumber);
    int8_t GetLastPacketRSSI();
    int8_t GetLastPacketSNRRaw();
    int8_t GetCurrRSSI(SX12XX_Radio_Number_t radioNumber);
    void GetLastPacketStats();
    SX12XX_Radio_Number_t GetProcessingPacketRadio(){return processingPacketRadio;}
    SX12XX_Radio_Number_t GetLastSuccessfulPacketRadio() { return lastSuccessfulPacketRadio; }

    ////////////Non-blocking TX related Functions/////////////////
    void TXnb(uint8_t * data, uint8_t size, SX12XX_Radio_Number_t radioNumber);
    /////////////Non-blocking RX related Functions///////////////
    void RXnb();

private:
    // constant used for no power change pending
    // must not be a valid power register value
    static const uint8_t PWRPENDING_NONE = SX127X_MAX_OUTPUT_POWER_INVALID;

    SX127x_Bandwidth currBW;
    SX127x_SpreadingFactor currSF;
    SX127x_CodingRate currCR;
    SX127x_RadioOPmodes currOpmode;
    SX127x_ModulationModes ModFSKorLoRa;
    uint8_t currSyncWord;
    uint8_t currPreambleLen;
    SX12XX_Radio_Number_t processingPacketRadio;
    SX12XX_Radio_Number_t lastSuccessfulPacketRadio;
    uint8_t pwrCurrent;
    uint8_t pwrPending;
    uint8_t lowFrequencyMode;

    static void IsrCallback_1();
    static void IsrCallback_2();
    static void IsrCallback(SX12XX_Radio_Number_t radioNumber);
    bool RXnbISR(SX12XX_Radio_Number_t radioNumber); // ISR for non-blocking RX routine
    void TXnbISR(); // ISR for non-blocking TX routine
    void CommitOutputPower();
};
