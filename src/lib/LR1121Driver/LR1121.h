#pragma once

#include "targets.h"
#include "SX12xxDriverCommon.h"
#include "LR1121_Regs.h"

#ifdef PLATFORM_ESP8266
#include <cstdint>
#endif

#define RADIO_SNR_SCALE 4

typedef struct
{
    uint8_t hardware;
    uint8_t type;
    uint16_t version;
} __attribute__((packed)) firmware_version_t;

class LR1121Driver: public SX12xxDriverCommon
{
public:
    static LR1121Driver *instance;

    ///////////Radio Variables////////
    uint32_t timeout;

    ///////////////////////////////////

    ////////////////Configuration Functions/////////////
    LR1121Driver();
    bool Begin(uint32_t minimumFrequency, uint32_t maximumFrequency);
    void End();
    void SetTxIdleMode() { SetMode(LR1121_MODE_FS, SX12XX_Radio_All); }; // set Idle mode used when switching from RX to TX
    void Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq,
                uint8_t PreambleLength, bool InvertIQ, uint8_t PayloadLength, uint32_t interval, bool setFSKModulation,
                uint8_t fskSyncWord1, uint8_t fskSyncWord2, SX12XX_Radio_Number_t radioNumber = SX12XX_Radio_All);
    void SetFrequencyHz(uint32_t freq, SX12XX_Radio_Number_t radioNumber);
    void SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber = SX12XX_Radio_All);
    void SetRxTimeoutUs(uint32_t interval);
    void SetOutputPower(int8_t power, bool isSubGHz = true);
    void startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber);


    bool GetFrequencyErrorbool();
    // bool FrequencyErrorAvailable() const { return modeSupportsFei && (LastPacketSNRRaw > 0); }
    bool FrequencyErrorAvailable() const { return false; }

    void TXnb(uint8_t * data, SX12XX_Radio_Number_t radioNumber);
    void RXnb(lr11xx_RadioOperatingModes_t rxMode = LR1121_MODE_RX, uint32_t incomingTimeout = 0);

    uint32_t GetIrqStatus(SX12XX_Radio_Number_t radioNumber);
    void ClearIrqStatus(SX12XX_Radio_Number_t radioNumber);

    void StartRssiInst(SX12XX_Radio_Number_t radioNumber);
    int8_t GetRssiInst(SX12XX_Radio_Number_t radioNumber);
    void GetLastPacketStats();

    // Firmware update methods
    firmware_version_t GetFirmwareVersion(SX12XX_Radio_Number_t radioNumber, uint16_t command = LR11XX_SYSTEM_GET_VERSION_OC);
    int BeginUpdate(SX12XX_Radio_Number_t radioNumber, uint32_t expectedSize);
    int WriteUpdateBytes(const uint8_t *bytes, uint32_t size);
    int EndUpdate();

private:
    // constant used for no power change pending
    // must not be a valid power register value
    static const uint8_t PWRPENDING_NONE = 0x7f;

    // LR1121_RadioOperatingModes_t currOpmode;
    bool useFSK;
    bool modeSupportsFei;
    uint8_t pwrCurrentLF;
    uint8_t pwrPendingLF;
    uint8_t pwrCurrentHF; // HF = High Frequency
    uint8_t pwrPendingHF;
    bool pwrForceUpdate;
    bool radio1isSubGHz;
    bool radio2isSubGHz;
    lr11xx_RadioOperatingModes_t fallBackMode;
    bool useFEC;

    WORD_ALIGNED_ATTR uint8_t rx_buf[32] = {};

    bool CheckVersion(SX12XX_Radio_Number_t radioNumber);

    void SetMode(lr11xx_RadioOperatingModes_t OPmode, SX12XX_Radio_Number_t radioNumber, uint32_t incomingTimeout = 0);

    // LoRa functions
    void ConfigModParamsLoRa(uint8_t bw, uint8_t sf, uint8_t cr, SX12XX_Radio_Number_t radioNumber);
    void SetPacketParamsLoRa(uint8_t PreambleLength, lr11xx_RadioLoRaPacketLengthsModes_t HeaderType,
                             uint8_t PayloadLength, uint8_t InvertIQ, SX12XX_Radio_Number_t radioNumber);
    // FSK functions
    void ConfigModParamsFSK(uint32_t Bitrate, uint8_t BWF, uint32_t Fdev, SX12XX_Radio_Number_t radioNumber);
    void SetPacketParamsFSK(uint8_t PreambleLength, uint8_t PayloadLength, SX12XX_Radio_Number_t radioNumber);
    void SetFSKSyncWord(uint8_t fskSyncWord1, uint8_t fskSyncWord2, SX12XX_Radio_Number_t radioNumber);

    void SetDioIrqParams();
    void SetDioAsRfSwitch();
    void CorrectRegisterForSF6(uint8_t sf, SX12XX_Radio_Number_t radioNumber);

    static void IsrCallback_1();
    static void IsrCallback_2();
    static void IsrCallback(SX12XX_Radio_Number_t radioNumber);

    void DecodeRssiSnr(SX12XX_Radio_Number_t radioNumber, const uint8_t *buf);

    bool RXnbISR(SX12XX_Radio_Number_t radioNumber); // ISR for non-blocking RX routine
    void TXnbISR(); // ISR for non-blocking TX routine
    void CommitOutputPower();
    void WriteOutputPower(uint8_t pwr, bool isSubGHz, SX12XX_Radio_Number_t radioNumber);
    void SetPaConfig(bool isSubGHz, SX12XX_Radio_Number_t radioNumber);
};
