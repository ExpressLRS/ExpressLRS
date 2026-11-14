#pragma once

#include "targets.h"
#include "radiochip.h"

#include "SX127xDriver.h"
// Each driver header defines FREQ_STEP, its chip's frequency register unit in Hz. Keep
// both under their own names: shared code has to pick one by radioChip.
static constexpr double SX127X_FREQ_STEP = FREQ_STEP;
#undef FREQ_STEP
#include "SX1280Driver.h"
static constexpr double SX1280_FREQ_STEP = FREQ_STEP;
#undef FREQ_STEP

/**
 * Radio in a multi-radio build (RADIO_MULTI): every driver is linked in and the one for
 * radioChip, set once at boot, does the work. Calls are forwarded by testing radioChip,
 * a byte in RAM, and not through virtual functions: ESP8285 builds keep their vtables in
 * flash, which the radio ISRs can't rely on. The drivers share their SX12xxDriverCommon
 * state, so Radio.RXdataBuffer, Radio.LastPacketRSSI and the callbacks work as usual.
 *
 * The signatures are those of the SX1280 driver, the superset of the two; the SX127x
 * gets the arguments it takes and ignores the FLRC ones, as an SX127X build does.
 */
class MultiRadioDriver : public SX12xxDriverCommon
{
public:
    // Which chip the hardware layout is for: its "radio_chip" if set, else the SX127x's DIO0
    // pin, which SX1280 layouts don't have (they use DIO1 and BUSY)
    static radio_chip_e ChipFromLayout();

    bool Begin();
    void End();
    void Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq,
                uint8_t PreambleLength, bool InvertIQ, uint8_t PayloadLength,
                uint32_t flrcSyncWord, uint16_t flrcCrcSeed, RadioBandMod::Combined modulation);
    // int16_t carries either driver's value: uint8_t register value (SX127x), int8_t dBm (SX1280)
    void SetOutputPower(int16_t power);
    void SetTxIdleMode();
    void startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber);
    void cwRepeat(SX12XX_Radio_Number_t radioNumber); // SX127x only

    void SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber, bool doRx = false);
    bool FrequencyErrorAvailable() const { return radioChip == RADIO_CHIP_SX127X ? sx127x.FrequencyErrorAvailable() : sx1280.FrequencyErrorAvailable(); }
    bool GetFrequencyErrorbool(SX12XX_Radio_Number_t radioNumber);
    void SetPPMoffsetReg(int32_t offset, SX12XX_Radio_Number_t radioNumber); // SX127x only, no-op on the SX1280
    int8_t GetRssiInst(SX12XX_Radio_Number_t radioNumber);

    void TXnb(uint8_t * data, bool sendGeminiBuffer, uint8_t * dataGemini, SX12XX_Radio_Number_t radioNumber);
    void RXnb();
    void GetLastPacketStats();
    void CheckForSecondPacket();

private:
    SX127xDriver sx127x;
    SX1280Driver sx1280;
};
