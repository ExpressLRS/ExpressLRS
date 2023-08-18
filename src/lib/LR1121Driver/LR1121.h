#pragma once

#include "targets.h"
#include "LR1121_Regs.h"
#include "LR1121_hal.h"
#include "SX12xxDriverCommon.h"

#ifdef PLATFORM_ESP8266
#include <cstdint>
#endif

#define RADIO_SNR_SCALE 4

class LR1121Driver: public SX12xxDriverCommon
{
public:
    static LR1121Driver *instance;

    ///////////Radio Variables////////
    uint32_t timeout;

    ///////////////////////////////////

    ////////////////Configuration Functions/////////////
    LR1121Driver();
    bool Begin();
    void End();
    void SetTxIdleMode() { SetMode(LR1121_MODE_FS, SX12XX_Radio_All); }; // set Idle mode used when switching from RX to TX
    void Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq,
                uint8_t PreambleLength, bool InvertIQ, uint8_t PayloadLength, uint32_t interval,
                uint32_t flrcSyncWord=0, uint16_t flrcCrcSeed=0, uint8_t flrc=0);
    void SetFrequencyHz(uint32_t freq, SX12XX_Radio_Number_t radioNumber);
    void SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber = SX12XX_Radio_All);
    void SetRxTimeoutUs(uint32_t interval);
    void SetOutputPower(int8_t power);
    void startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber);


    bool GetFrequencyErrorbool();
    // bool FrequencyErrorAvailable() const { return modeSupportsFei && (LastPacketSNRRaw > 0); }
    bool FrequencyErrorAvailable() const { return false; }

    void TXnb(uint8_t * data, uint8_t size, SX12XX_Radio_Number_t radioNumber);
    void RXnb(lr11xx_RadioOperatingModes_t rxMode = LR1121_MODE_RX);

    uint32_t GetIrqStatus(SX12XX_Radio_Number_t radioNumber);
    void ClearIrqStatus(SX12XX_Radio_Number_t radioNumber);

    int8_t GetRssiInst(SX12XX_Radio_Number_t radioNumber);
    void GetLastPacketStats();

private:
    // constant used for no power change pending
    // must not be a valid power register value
    static const uint8_t PWRPENDING_NONE = 0xff;

    // LR1121_RadioOperatingModes_t currOpmode;
    bool subGRF;
    bool modeSupportsFei;
    uint8_t pwrCurrent;
    uint8_t pwrPending;

    void SetMode(lr11xx_RadioOperatingModes_t OPmode, SX12XX_Radio_Number_t radioNumber);

    // LoRa functions
    void ConfigModParamsLoRa(uint8_t bw, uint8_t sf, uint8_t cr);
    void SetPacketParamsLoRa(uint8_t PreambleLength, lr11xx_RadioLoRaPacketLengthsModes_t HeaderType,
                             uint8_t PayloadLength, uint8_t InvertIQ);

    void SetDioIrqParams();
    void CorrectRegisterForSF6(uint8_t sf, SX12XX_Radio_Number_t radioNumber);

    static void IsrCallback_1();
    static void IsrCallback_2();
    static void IsrCallback(SX12XX_Radio_Number_t radioNumber);
    bool RXnbISR(SX12XX_Radio_Number_t radioNumber); // ISR for non-blocking RX routine
    void TXnbISR(); // ISR for non-blocking TX routine
    void CommitOutputPower();
};
