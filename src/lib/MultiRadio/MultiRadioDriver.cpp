#if defined(RADIO_MULTI)

#include "MultiRadioDriver.h"
#include "logging.h"

radio_chip_e radioChip = RADIO_CHIP_SX128X; // replaced at boot, see ChipFromLayout()

// The state every driver shares (see SX12xxDriverCommon.h)
bool (*SX12xxDriverCommon::RXdoneCallback)(SX12xxDriverCommon::rx_status) = SX12xxDriverCommon::nullCallbackRx;
void (*SX12xxDriverCommon::TXdoneCallback)() = SX12xxDriverCommon::nullCallbackTx;
WORD_ALIGNED_ATTR uint8_t SX12xxDriverCommon::RXdataBuffer[RXBuffSize];
WORD_ALIGNED_ATTR uint8_t SX12xxDriverCommon::RXdataBufferSecond[RXBuffSize];
uint32_t SX12xxDriverCommon::currFreq;
uint8_t SX12xxDriverCommon::PayloadLength;
bool SX12xxDriverCommon::IQinverted;
SX12XX_Radio_Number_t SX12xxDriverCommon::processingPacketRadio;
SX12XX_Radio_Number_t SX12xxDriverCommon::transmittingRadio;
SX12XX_Radio_Number_t SX12xxDriverCommon::strongestReceivingRadio;
int8_t SX12xxDriverCommon::LastPacketRSSI;
int8_t SX12xxDriverCommon::LastPacketRSSI2;
int8_t SX12xxDriverCommon::LastPacketSNRRaw;
int8_t SX12xxDriverCommon::FuzzySNRThreshold;
bool SX12xxDriverCommon::gotRadio[2];
bool SX12xxDriverCommon::hasSecondRadioGotData;
#if defined(DEBUG_RCVR_SIGNAL_STATS)
SX12xxDriverCommon::rxSignalStats_t SX12xxDriverCommon::rxSignalStats[2];
uint16_t SX12xxDriverCommon::irq_count_or;
uint16_t SX12xxDriverCommon::irq_count_both;
#endif

radio_chip_e MultiRadioDriver::ChipFromLayout()
{
    const int chip = hardware_int(HARDWARE_radio_chip);
    if (chip == RADIO_CHIP_SX127X || chip == RADIO_CHIP_SX128X)
    {
        DBGLN("Radio chip %d from layout", chip);
        return (radio_chip_e)chip;
    }
    if (chip != -1)
    {
        DBGLN("Radio chip %d not in this build, using the pins", chip);
    }
    return (GPIO_PIN_DIO0 != UNDEF_PIN) ? RADIO_CHIP_SX127X : RADIO_CHIP_SX128X;
}

bool MultiRadioDriver::Begin()
{
    DBGLN("Radio %s", radioChip == RADIO_CHIP_SX127X ? "SX127x" : "SX1280");
    return radioChip == RADIO_CHIP_SX127X ? sx127x.Begin() : sx1280.Begin();
}

void MultiRadioDriver::End()
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.End();
    else
        sx1280.End();
}

void MultiRadioDriver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq,
                              uint8_t PreambleLength, bool InvertIQ, uint8_t PayloadLength,
                              uint32_t flrcSyncWord, uint16_t flrcCrcSeed, RadioBandMod::Combined modulation)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.Config(bw, sf, cr, freq, PreambleLength, InvertIQ, PayloadLength);
    else
        sx1280.Config(bw, sf, cr, freq, PreambleLength, InvertIQ, PayloadLength, flrcSyncWord, flrcCrcSeed, modulation);
}

void MultiRadioDriver::SetOutputPower(int16_t power)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.SetOutputPower((uint8_t)power);
    else
        sx1280.SetOutputPower((int8_t)power);
}

void MultiRadioDriver::SetTxIdleMode()
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.SetTxIdleMode();
    else
        sx1280.SetTxIdleMode();
}

void MultiRadioDriver::startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.startCWTest(freq, radioNumber);
    else
        sx1280.startCWTest(freq, radioNumber);
}

void MultiRadioDriver::cwRepeat(SX12XX_Radio_Number_t radioNumber)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.cwRepeat(radioNumber);
}

void ICACHE_RAM_ATTR MultiRadioDriver::SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber, bool doRx)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.SetFrequencyReg(freq, radioNumber, doRx);
    else
        sx1280.SetFrequencyReg(freq, radioNumber, doRx);
}

bool ICACHE_RAM_ATTR MultiRadioDriver::GetFrequencyErrorbool(SX12XX_Radio_Number_t radioNumber)
{
    return radioChip == RADIO_CHIP_SX127X ? sx127x.GetFrequencyErrorbool(radioNumber) : sx1280.GetFrequencyErrorbool(radioNumber);
}

void ICACHE_RAM_ATTR MultiRadioDriver::SetPPMoffsetReg(int32_t offset, SX12XX_Radio_Number_t radioNumber)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.SetPPMoffsetReg(offset, radioNumber);
}

int8_t ICACHE_RAM_ATTR MultiRadioDriver::GetRssiInst(SX12XX_Radio_Number_t radioNumber)
{
    return radioChip == RADIO_CHIP_SX127X ? sx127x.GetCurrRSSI(radioNumber) : sx1280.GetRssiInst(radioNumber);
}

void ICACHE_RAM_ATTR MultiRadioDriver::TXnb(uint8_t * data, bool sendGeminiBuffer, uint8_t * dataGemini, SX12XX_Radio_Number_t radioNumber)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.TXnb(data, sendGeminiBuffer, dataGemini, radioNumber);
    else
        sx1280.TXnb(data, sendGeminiBuffer, dataGemini, radioNumber);
}

void ICACHE_RAM_ATTR MultiRadioDriver::RXnb()
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.RXnb();
    else
        sx1280.RXnb();
}

void ICACHE_RAM_ATTR MultiRadioDriver::GetLastPacketStats()
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.GetLastPacketStats();
    else
        sx1280.GetLastPacketStats();
}

void ICACHE_RAM_ATTR MultiRadioDriver::CheckForSecondPacket()
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.CheckForSecondPacket();
    else
        sx1280.CheckForSecondPacket();
}

#endif
