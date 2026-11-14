#if defined(RADIO_MULTI)

#include "MultiRadioDriver.h"
#include "FHSS.h"
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

#if defined(RADIO_LR1121)
#define WITH_LR1121(...) __VA_ARGS__
#else
#define WITH_LR1121(...)
#endif
#if defined(RADIO_LR2021)
#define WITH_LR2021(...) __VA_ARGS__
#else
#define WITH_LR2021(...)
#endif

// Call the same method, with the same arguments, on the driver for radioChip
#define FORWARD(call) do { \
        if (radioChip == RADIO_CHIP_SX127X) return sx127x.call; \
        WITH_LR1121(if (radioChip == RADIO_CHIP_LR1121) return lr1121.call;) \
        WITH_LR2021(if (radioChip == RADIO_CHIP_LR2021) return lr2021.call;) \
        return sx1280.call; \
    } while (0)

#if defined(RADIO_LR2021)
namespace lr2021 { extern LR2021Hal hal; }

/**
 * Ask the LR11xx/LR20xx GetVersion command (0x0101, the same on both) which LR chip is
 * fitted. Only reads: the LR1121 driver would reflash a chip whose firmware it doesn't
 * recognise, so it must not be started on a guess.
 * @return the chip, or RADIO_CHIP_SX128X if no LR chip answered
 */
static radio_chip_e ProbeLRChip()
{
    if (GPIO_PIN_BUSY == UNDEF_PIN)
    {
        return RADIO_CHIP_SX128X; // every LR layout has BUSY
    }
    using lr2021::hal;
    uint8_t buffer[4] {};
    hal.init();
    hal.reset();
    hal.WriteCommand(LR2021_SYSTEM_GET_VERSION_OC, SX12XX_Radio_1);
    hal.ReadCommand(buffer, sizeof(buffer), SX12XX_Radio_1);
    hal.WaitOnBusy(SX12XX_Radio_1);
    hal.end();
    DBGLN("LR probe %x %x %x %x", buffer[0], buffer[1], buffer[2], buffer[3]);

    // LR20xx: 2 status bytes, then the version, 0x0118 on the LR2021 the driver supports.
    // LR11xx: 1 status byte, hardware, then the firmware type: 0x03 Semtech's LR1121
    // firmware, 0xF3 the ELRS one, 0xDF the bootloader.
    WITH_LR2021(if (buffer[2] == 0x01 && buffer[3] == 0x18) return RADIO_CHIP_LR2021;)
    WITH_LR1121(if (buffer[2] == 0x03 || buffer[2] == 0xF3 || buffer[2] == 0xDF) return RADIO_CHIP_LR1121;)
    return RADIO_CHIP_SX128X;
}
#endif

radio_chip_e MultiRadioDriver::ChipFromLayout()
{
    const int chip = hardware_int(HARDWARE_radio_chip);
    if (chip == RADIO_CHIP_SX127X || chip == RADIO_CHIP_SX128X
        WITH_LR1121(|| chip == RADIO_CHIP_LR1121) WITH_LR2021(|| chip == RADIO_CHIP_LR2021))
    {
        DBGLN("Radio chip %d from layout", chip);
        return (radio_chip_e)chip;
    }
    if (chip != -1)
    {
        DBGLN("Radio chip %d not in this build, using the pins", chip);
    }
    if (GPIO_PIN_DIO0 != UNDEF_PIN)
    {
        return RADIO_CHIP_SX127X;
    }
#if defined(RADIO_LR2021)
    return ProbeLRChip();
#else
    return RADIO_CHIP_SX128X;
#endif
}

bool MultiRadioDriver::Begin()
{
    DBGLN("Radio chip %d", radioChip);
    if (radioChip == RADIO_CHIP_SX127X) return sx127x.Begin();
    WITH_LR1121(if (radioChip == RADIO_CHIP_LR1121) return lr1121.Begin(FHSSgetMinimumFreq(), FHSSgetMaximumFreq());)
    WITH_LR2021(if (radioChip == RADIO_CHIP_LR2021) return lr2021.Begin(FHSSconfig->freq_center, FHSSconfigDualBand->freq_center);)
    return sx1280.Begin();
}

void MultiRadioDriver::End()
{
    FORWARD(End());
}

void MultiRadioDriver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq,
                              uint8_t PreambleLength, bool InvertIQ, uint8_t PayloadLength,
                              RadioBandMod::Combined modulation, uint8_t fskSyncWord1, uint8_t fskSyncWord2,
                              uint32_t flrcSyncWord, uint16_t flrcCrcSeed,
                              SX12XX_Radio_Number_t radioNumber)
{
    if (radioChip == RADIO_CHIP_SX127X)
        return sx127x.Config(bw, sf, cr, freq, PreambleLength, InvertIQ, PayloadLength);
    WITH_LR1121(if (radioChip == RADIO_CHIP_LR1121)
        return lr1121.Config(bw, sf, cr, freq, PreambleLength, InvertIQ, PayloadLength, modulation,
                             fskSyncWord1, fskSyncWord2, radioNumber);)
    WITH_LR2021(if (radioChip == RADIO_CHIP_LR2021)
        return lr2021.Config(bw, sf, cr, freq, PreambleLength, InvertIQ, PayloadLength, modulation,
                             fskSyncWord1, fskSyncWord2, flrcSyncWord, flrcCrcSeed, radioNumber);)
    return sx1280.Config(bw, sf, cr, freq, PreambleLength, InvertIQ, PayloadLength, flrcSyncWord, flrcCrcSeed, modulation);
}

void MultiRadioDriver::SetOutputPower(int16_t power, bool isSubGHz)
{
    if (radioChip == RADIO_CHIP_SX127X) return sx127x.SetOutputPower((uint8_t)power);
    WITH_LR1121(if (radioChip == RADIO_CHIP_LR1121) return lr1121.SetOutputPower((int8_t)power, isSubGHz);)
    WITH_LR2021(if (radioChip == RADIO_CHIP_LR2021) return lr2021.SetOutputPower((int8_t)power, isSubGHz);)
    return sx1280.SetOutputPower((int8_t)power);
}

void MultiRadioDriver::SetTxIdleMode()
{
    FORWARD(SetTxIdleMode());
}

void MultiRadioDriver::startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    FORWARD(startCWTest(freq, radioNumber));
}

void MultiRadioDriver::cwRepeat(SX12XX_Radio_Number_t radioNumber)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.cwRepeat(radioNumber);
}

void ICACHE_RAM_ATTR MultiRadioDriver::SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber, bool doRx, uint32_t rxTime)
{
    if (radioChip == RADIO_CHIP_SX127X) return sx127x.SetFrequencyReg(freq, radioNumber, doRx);
    WITH_LR1121(if (radioChip == RADIO_CHIP_LR1121) return lr1121.SetFrequencyReg(freq, radioNumber, doRx, rxTime);)
    WITH_LR2021(if (radioChip == RADIO_CHIP_LR2021) return lr2021.SetFrequencyReg(freq, radioNumber, doRx, rxTime);)
    return sx1280.SetFrequencyReg(freq, radioNumber, doRx);
}

bool ICACHE_RAM_ATTR MultiRadioDriver::GetFrequencyErrorbool(SX12XX_Radio_Number_t radioNumber)
{
    FORWARD(GetFrequencyErrorbool(radioNumber));
}

void ICACHE_RAM_ATTR MultiRadioDriver::SetPPMoffsetReg(int32_t offset, SX12XX_Radio_Number_t radioNumber)
{
    if (radioChip == RADIO_CHIP_SX127X)
        sx127x.SetPPMoffsetReg(offset, radioNumber);
}

void ICACHE_RAM_ATTR MultiRadioDriver::StartRssiInst(SX12XX_Radio_Number_t radioNumber)
{
    WITH_LR1121(if (radioChip == RADIO_CHIP_LR1121) lr1121.StartRssiInst(radioNumber);)
    WITH_LR2021(if (radioChip == RADIO_CHIP_LR2021) lr2021.StartRssiInst(radioNumber);)
}

int8_t ICACHE_RAM_ATTR MultiRadioDriver::GetRssiInst(SX12XX_Radio_Number_t radioNumber)
{
    if (radioChip == RADIO_CHIP_SX127X) return sx127x.GetCurrRSSI(radioNumber);
    WITH_LR1121(if (radioChip == RADIO_CHIP_LR1121) return lr1121.GetRssiInst(radioNumber);)
    WITH_LR2021(if (radioChip == RADIO_CHIP_LR2021) return lr2021.GetRssiInst(radioNumber);)
    return sx1280.GetRssiInst(radioNumber);
}

void ICACHE_RAM_ATTR MultiRadioDriver::TXnb(uint8_t * data, bool sendGeminiBuffer, uint8_t * dataGemini, SX12XX_Radio_Number_t radioNumber)
{
    FORWARD(TXnb(data, sendGeminiBuffer, dataGemini, radioNumber));
}

void ICACHE_RAM_ATTR MultiRadioDriver::RXnb()
{
    FORWARD(RXnb());
}

void ICACHE_RAM_ATTR MultiRadioDriver::GetLastPacketStats()
{
    FORWARD(GetLastPacketStats());
}

void ICACHE_RAM_ATTR MultiRadioDriver::CheckForSecondPacket()
{
    FORWARD(CheckForSecondPacket());
}

#if defined(RADIO_LR1121)
firmware_version_t MultiRadioDriver::GetFirmwareVersion(SX12XX_Radio_Number_t radioNumber)
{
    return radioChip == RADIO_CHIP_LR1121 ? lr1121.GetFirmwareVersion(radioNumber) : firmware_version_t {};
}

int MultiRadioDriver::BeginUpdate(SX12XX_Radio_Number_t radioNumber, uint32_t expectedSize)
{
    return radioChip == RADIO_CHIP_LR1121 ? lr1121.BeginUpdate(radioNumber, expectedSize) : -1;
}

int MultiRadioDriver::WriteUpdateBytes(const uint8_t *bytes, uint32_t size)
{
    return radioChip == RADIO_CHIP_LR1121 ? lr1121.WriteUpdateBytes(bytes, size) : -1;
}

int MultiRadioDriver::EndUpdate()
{
    return radioChip == RADIO_CHIP_LR1121 ? lr1121.EndUpdate() : -1;
}
#endif

#endif
