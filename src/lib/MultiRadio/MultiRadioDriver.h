#pragma once

#include "targets.h"
#include "radiochip.h"

#if !defined(RADIO_SX127X) || !defined(RADIO_SX128X)
#error "A multi-radio build has the SX127x and SX1280 drivers at least"
#endif

#include "SX127xDriver.h"
// Each driver header defines FREQ_STEP, its chip's frequency register unit in Hz. Keep
// the SX12xx ones under their own names: shared code has to pick one by radioChip.
// The LR chips take frequencies in Hz.
static constexpr double SX127X_FREQ_STEP = FREQ_STEP;
#undef FREQ_STEP
#include "SX1280Driver.h"
static constexpr double SX1280_FREQ_STEP = FREQ_STEP;
#undef FREQ_STEP
#if defined(RADIO_LR1121)
#include "LR1121Driver.h"
#undef FREQ_STEP
#endif
#if defined(RADIO_LR2021)
#include "LR2021Driver.h"
#undef FREQ_STEP
#endif

/**
 * Radio in a multi-radio build (RADIO_MULTI): every driver is linked in and the one for
 * radioChip, set once at boot, does the work. Calls are forwarded by testing radioChip,
 * a byte in RAM, and not through virtual functions: ESP8285 builds keep their vtables in
 * flash, which the radio ISRs can't rely on. The drivers share their SX12xxDriverCommon
 * state, so Radio.RXdataBuffer, Radio.LastPacketRSSI and the callbacks work as usual.
 *
 * The signatures are the superset of the drivers'; each driver gets the arguments it
 * takes and ignores the rest, as its single-radio build does.
 */
class MultiRadioDriver : public SX12xxDriverCommon
{
public:
    // Which chip the board has: the layout's "radio_chip" if set, else the SX127x's DIO0
    // pin, else an LR11xx/LR20xx if one answers GetVersion, else the SX1280
    static radio_chip_e ChipFromLayout();

    // The LR chips are started with the edges of their bands, taken from FHSSconfig
    bool Begin();
    void End();
    void Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq,
                uint8_t PreambleLength, bool InvertIQ, uint8_t PayloadLength,
                RadioBandMod::Combined modulation, uint8_t fskSyncWord1, uint8_t fskSyncWord2,
                uint32_t flrcSyncWord, uint16_t flrcCrcSeed,
                SX12XX_Radio_Number_t radioNumber = SX12XX_Radio_All);
    // int16_t carries any driver's value: uint8_t register value (SX127x), int8_t dBm (the others)
    void SetOutputPower(int16_t power, bool isSubGHz = true);
    void SetTxIdleMode();
    void startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber);
    void cwRepeat(SX12XX_Radio_Number_t radioNumber); // SX127x only

    void SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber, bool doRx = false, uint32_t rxTime = 0);
    bool FrequencyErrorAvailable() const
    {
        if (radioChip == RADIO_CHIP_SX127X) return sx127x.FrequencyErrorAvailable();
        if (radioChip == RADIO_CHIP_SX128X) return sx1280.FrequencyErrorAvailable();
        return false; // the LR drivers have none
    }
    bool GetFrequencyErrorbool(SX12XX_Radio_Number_t radioNumber);
    void SetPPMoffsetReg(int32_t offset, SX12XX_Radio_Number_t radioNumber); // SX127x only, a no-op on the others
    void StartRssiInst(SX12XX_Radio_Number_t radioNumber); // LR chips only, a no-op on the others
    int8_t GetRssiInst(SX12XX_Radio_Number_t radioNumber);

    void TXnb(uint8_t * data, bool sendGeminiBuffer, uint8_t * dataGemini, SX12XX_Radio_Number_t radioNumber);
    void RXnb();
    void GetLastPacketStats();
    void CheckForSecondPacket();

#if defined(RADIO_LR1121)
    // The LR1121 transceiver firmware updater (web page), for an LR1121 only
    firmware_version_t GetFirmwareVersion(SX12XX_Radio_Number_t radioNumber);
    int BeginUpdate(SX12XX_Radio_Number_t radioNumber, uint32_t expectedSize);
    int WriteUpdateBytes(const uint8_t *bytes, uint32_t size);
    int EndUpdate();
#endif

private:
    SX127xDriver sx127x;
    SX1280Driver sx1280;
#if defined(RADIO_LR1121)
    LR1121Driver lr1121;
#endif
#if defined(RADIO_LR2021)
    LR2021Driver lr2021;
#endif
};
