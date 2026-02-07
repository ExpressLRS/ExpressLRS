#include "LR2021.h"
#include "LR2021_hal.h"
#include "FEC.h"
#include "logging.h"

#include <LittleFS.h>

LR2021Hal hal;
LR2021Driver *LR2021Driver::instance = NULL;

// DEBUG_LR2021_OTA_TIMING

#if defined(DEBUG_LR2021_OTA_TIMING)
static uint32_t beginTX;
static uint32_t endTX;
#endif

class FECCodec final : public BufferCodec
{
public:
    void encode(uint8_t *out, uint8_t *in, uint32_t len) override;
    void decode(uint8_t *out, uint8_t *in, uint32_t len) override;
} fecCodec;

class CopyCodec final : public BufferCodec
{
public:
    void encode(uint8_t *out, uint8_t *in, uint32_t len) override;
    void decode(uint8_t *out, uint8_t *in, uint32_t len) override;
} copyCodec;

void ICACHE_RAM_ATTR FECCodec::encode(uint8_t *out, uint8_t *in, const uint32_t len)
{
    memset(out, 0, len); // ensure that the buffer is zeroed to start
    FECEncode(in, out);
}

void ICACHE_RAM_ATTR FECCodec::decode(uint8_t *out, uint8_t *in, const uint32_t len)
{
    FECDecode(in, out);
}

void ICACHE_RAM_ATTR CopyCodec::encode(uint8_t *out, uint8_t *in, const uint32_t len)
{
    memcpy(out, in, len);
}
void ICACHE_RAM_ATTR CopyCodec::decode(uint8_t *out, uint8_t *in, const uint32_t len)
{
    memcpy(out, in, len);
}

#define CHECK(msg, x)                        \
    do                                       \
    {                                        \
        uint16_t err = x;                    \
        if ((err & 0x400) != 0x400)          \
        {                                    \
            DBGLN("FAILED %s %x", msg, err); \
        }                                    \
    } while (false)

LR2021Driver::LR2021Driver()
{
    useFSK = false;
    instance = this;
    strongestReceivingRadio = SX12XX_Radio_1;
    fallBackMode = LR2021_MODE_FS;
    codec = &copyCodec;
}

void LR2021Driver::End()
{
    SetMode(LR2021_MODE_SLEEP, SX12XX_Radio_All);
    hal.end();
    RemoveCallbacks();
}

bool LR2021Driver::CheckVersion(const SX12XX_Radio_Number_t radioNumber)
{
    uint8_t buffer[4] = {};
    hal.WriteCommand(LR2021_SYSTEM_GET_VERSION_OC, radioNumber);
    CHECK("GET_FIRMWARE", hal.ReadCommand(buffer, sizeof(buffer), radioNumber));
    hal.WaitOnBusy(radioNumber);

    uint16_t version = (uint16_t)(buffer[2] << 8 | buffer[3]);
    if (version != 0x0118)
    {
        DBGLN("LR2021 #%d failed to be detected %x.", radioNumber, version);
        return false;
    }
    DBGLN("LR2021 #%d Ready", radioNumber);
    return true;
}

bool LR2021Driver::Begin(const uint32_t lowBandFreq, const uint32_t highBandFreq)
{
    hal.init();
    hal.reset();

    // Validate that the LR2021(s) are working.
    if (!CheckVersion(SX12XX_Radio_1))
        return false;
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        if (!CheckVersion(SX12XX_Radio_2))
            return false;
    }

    hal.IsrCallback_1 = &LR2021Driver::IsrCallback_1;
    hal.IsrCallback_2 = &LR2021Driver::IsrCallback_2;

    // Clear Errors
    CHECK("LR2021_SYSTEM_CLEAR_ERRORS_OC", hal.WriteCommand(LR2021_SYSTEM_CLEAR_ERRORS_OC, SX12XX_Radio_All)); // Remove later?  Might not be required???

    // 6.3.7 SetRxTxFallbackMode
    constexpr uint8_t FBbuf = LR2021_RADIO_FALLBACK_FS;
    fallBackMode = LR2021_MODE_FS;
    CHECK("LR2021_RADIO_SET_RX_TX_FALLBACK_MODE_OC", hal.WriteCommand(LR2021_RADIO_SET_RX_TX_FALLBACK_MODE_OC, &FBbuf, 1, SX12XX_Radio_All));

    // 6.3.17 SetDefaultRxTxTimeout
    constexpr uint8_t timeouts[]{0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00};
    CHECK("LR2021_RADIO_SET_DEFAULT_RX_TX_TIMEOUT_OC", hal.WriteCommand(LR2021_RADIO_SET_DEFAULT_RX_TX_TIMEOUT_OC, timeouts, sizeof(timeouts), SX12XX_Radio_All));

    SetDioAsRfSwitch();
    SetDioIrqParams();

    // 6.3.18 SetRegMode
    const uint8_t RegMode = OPT_USE_HARDWARE_DCDC ? 0x02 : 0x00;
    CHECK("LR2021_SYSTEM_SET_REGMODE_OC", hal.WriteCommand(LR2021_SYSTEM_SET_REGMODE_OC, &RegMode, 1, SX12XX_Radio_All)); // Enable DCDC converter instead of LDO

    // 6.4.2 Calibrate
    constexpr uint8_t calibrate = 0x7F;
    CHECK("LR2021_SYSTEM_CALIBRATE_OC", hal.WriteCommand(LR2021_SYSTEM_CALIBRATE_OC, &calibrate, 1, SX12XX_Radio_All));

    // 6.4.2 CalibFE
    const uint8_t calibrateFE[]{(uint8_t)((lowBandFreq / 4000000) >> 8), (uint8_t)(lowBandFreq / 4000000), (uint8_t)(((highBandFreq / 4000000) >> 8) | 0x80), (uint8_t)(highBandFreq / 4000000)};
    CHECK("LR2021_SYSTEM_CALIBRATE_FRONTEND_OC", hal.WriteCommand(LR2021_SYSTEM_CALIBRATE_FRONTEND_OC, calibrateFE, sizeof(calibrateFE), SX12XX_Radio_All));

    return true;
}

// 12.2.1 SetTxCw
void LR2021Driver::startCWTest(const uint32_t freq, const SX12XX_Radio_Number_t radioNumber)
{
    // Set a basic Config that can be used for both 2.4G and SubGHz bands.
    Config(LR2021_RADIO_LORA_BW_62, LR2021_RADIO_LORA_SF6, LR2021_RADIO_LORA_CR_4_8, freq, 12, false, 8, false, 0, 0, radioNumber);
    uint8_t mode = 0x02;
    CHECK("LR2021_RADIO_SET_TX_TEST_MODE_OC", hal.WriteCommand(LR2021_RADIO_SET_TX_TEST_MODE_OC, &mode, 1, radioNumber));
}

void LR2021Driver::Config(const uint8_t bw, const uint8_t sf, const uint8_t cr, const uint32_t regfreq,
                          const uint8_t PreambleLength, const bool InvertIQ, const uint8_t _PayloadLength,
                          const bool setFSKModulation, const uint8_t fskSyncWord1, const uint8_t fskSyncWord2,
                          const SX12XX_Radio_Number_t radioNumber)
{
    PayloadLength = _PayloadLength;

    const bool isSubGHz = regfreq < 1000000000;

    if (radioNumber & SX12XX_Radio_1)
    {
        radio1isSubGHz = isSubGHz;
    }

    if (radioNumber & SX12XX_Radio_2)
    {
        radio2isSubGHz = isSubGHz;
    }

    IQinverted = InvertIQ;
    lr11xx_radio_lora_iq_t inverted = InvertIQ ? LR2021_RADIO_LORA_IQ_INVERTED : LR2021_RADIO_LORA_IQ_STANDARD;
    // IQinverted is always STANDARD for 900
    if (isSubGHz)
    {
        inverted = LR2021_RADIO_LORA_IQ_STANDARD;
    }

    SetMode(LR2021_MODE_STDBY_RC, radioNumber);

    useFSK = setFSKModulation;
    codec = &copyCodec;
    if (useFSK)
    {
        DBGLN("Config FSK");
        uint32_t bitrate = (uint32_t)bw * 10000;
        uint8_t bwf = sf;
        uint32_t fdev = (uint32_t)cr * 1000;
        ConfigModParamsFSK(bitrate, bwf, fdev, radioNumber);

        // Increase packet length for FEC used only on 1000Hz 2.5GHz.
        if (!isSubGHz)
        {
            codec = &fecCodec;
            PayloadLength = 14;
        }

        SetPacketParamsFSK(PreambleLength, PayloadLength, radioNumber);
        SetFSKSyncWord(fskSyncWord1, fskSyncWord2, radioNumber);
    }
    else
    {
        DBGLN("Config LoRa");
        ConfigModParamsLoRa(bw, sf, cr, radioNumber);

#if defined(DEBUG_FREQ_CORRECTION) // TODO Check if this available with the LR2021?
        lr11xx_RadioLoRaPacketLengthsModes_t packetLengthType = LR2021_LORA_PACKET_EXPLICIT;
#else
        lr11xx_RadioLoRaPacketLengthsModes_t packetLengthType = LR2021_LORA_PACKET_IMPLICIT;
#endif

        SetPacketParamsLoRa(PreambleLength, packetLengthType, PayloadLength, inverted, radioNumber);
    }

    SetFrequencyReg(regfreq, radioNumber, false);
    // 7.2.2 SetRxPath
    if (isSubGHz)
    {
        constexpr uint8_t buf[] = {0x00, 0x00};
        CHECK("LR2021_RADIO_SET_RX_PATH_OC", hal.WriteCommand(LR2021_RADIO_SET_RX_PATH_OC, buf, 2, radioNumber));
    }
    else
    {
        constexpr uint8_t buf[] = {0x01, 0x04};
        CHECK("LR2021_RADIO_SET_RX_PATH_OC", hal.WriteCommand(LR2021_RADIO_SET_RX_PATH_OC, buf, 2, radioNumber));
    }

    ClearIrqStatus(radioNumber);

    SetPaConfig(isSubGHz, radioNumber); // Must be called after changing rf modes between subG and 2.4G.  This sets the correct rf amps, and txen pins to be used.
    pwrForceUpdate = true;              // force an update of the output power because the band may have changed, and we need to configure the power for the band.
    CommitOutputPower();
}

void LR2021Driver::ConfigModParamsFSK(const uint32_t Bitrate, const uint8_t BWF, const uint32_t Fdev, const SX12XX_Radio_Number_t radioNumber)
{
    // 8.1.1 SetPacketType
    constexpr uint8_t packetType = LR2021_RADIO_PKT_TYPE_FSK;
    CHECK("LR2021_RADIO_SET_PKT_TYPE_OC", hal.WriteCommand(LR2021_RADIO_SET_PKT_TYPE_OC, &packetType, 1, radioNumber));

    // 11.3.1 SetFskModulationParams
    uint8_t buf[9];
    buf[0] = Bitrate >> 24;
    buf[1] = Bitrate >> 16;
    buf[2] = Bitrate >> 8;
    buf[3] = Bitrate >> 0;
    buf[4] = LR2021_RADIO_GFSK_PULSE_SHAPE_OFF; // Pulse Shape - 0x00: No filter applied
    buf[5] = BWF;
    buf[6] = Fdev >> 16;
    buf[7] = Fdev >> 8;
    buf[8] = Fdev >> 0;
    CHECK("LR2021_RADIO_SET_FSK_MODULATION_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_MODULATION_PARAMS_OC, buf, sizeof(buf), radioNumber));
}

void LR2021Driver::SetPacketParamsFSK(const uint8_t PreambleLength, const uint8_t PayloadLength, const SX12XX_Radio_Number_t radioNumber)
{
    // 11.3.2 SetFskPacketParams
    uint8_t buf[8];
    buf[0] = 0;                                             // MSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[1] = PreambleLength;                                // LSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[2] = LR2021_RADIO_GFSK_PREAMBLE_DETECTOR_MIN_8BITS; // Pbl Detect
    buf[3] = 0x00;                                          //
    buf[4] = 0;                                             // MSB PayloadLen
    buf[5] = PayloadLength;                                 // LSB PayloadLen
    buf[6] = LR2021_RADIO_GFSK_CRC_OFF << 4 | LR2021_RADIO_GFSK_DC_FREE_WHITENING;
    CHECK("LR2021_RADIO_SET_FSK_PACKET_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_PACKET_PARAMS_OC, buf, 8, radioNumber));

    // 11.3.3 SetFskPacketParams
    buf[0] = 0x01; // SX127x/SX126x/LR11xx compatible whitening enable 0x0100 seed
    buf[1] = 0x00;
    CHECK("LR2021_RADIO_SET_FSK_WHITENING_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_WHITENING_PARAMS_OC, buf, 2, radioNumber));
}

void LR2021Driver::SetFSKSyncWord(const uint8_t fskSyncWord1, const uint8_t fskSyncWord2, const SX12XX_Radio_Number_t radioNumber)
{
    // 11.3.5 SetFskSyncWord
    // SyncWordLen is 16 bits as set in SetPacketParamsFSK().  Fill the rest with preamble bytes.
    const uint8_t syncBuf[] {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, fskSyncWord1, fskSyncWord2, 0x90};
    CHECK("LR2021_RADIO_SET_FSK_SYNC_WORD_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_SYNC_WORD_OC, syncBuf, sizeof(syncBuf), radioNumber));
}

void LR2021Driver::SetDioAsRfSwitch()
{
    /*
     * Here the radio_rfsw_ctrl has 7 values which set the pin (DIO5-11) state for each rf mode
     * So if DIO5 is to be high for 2.4 RX and low otherwise; then it should be set to 0x08
     * BIT | Mode
     * ----------
     *  7  | 0
     *  6  | 0
     *  5  | 0
     *  4  | 2.4 TX
     *  3  | 2.4 RX
     *  2  | SubG TX
     *  1  | SubG RX
     *  0  | Standby
     */
    constexpr uint8_t default_rfsw_ctrl[]{0x10, 0x08, 0x02, 0x4, 0, 0, 0};
    uint8_t switchbuf[2];
    // set all DIOs as RF output, and set the RF state for the DIO
    for (int i = 5; i <= 11; i++)
    {
        switchbuf[0] = i;
        switchbuf[1] = i == 5 ? 0x22 : 0x23; // RF switch on DIO, SLEEP PullAuto (except DIO5 which *must* be PullUp)
        CHECK("LR2021_SYSTEM_SET_DIO_FUNCTION_OC", hal.WriteCommand(LR2021_SYSTEM_SET_DIO_FUNCTION_OC, switchbuf, sizeof(switchbuf), SX12XX_Radio_All));
        switchbuf[1] = LR1121_RFSW_CTRL ? LR1121_RFSW_CTRL[i - 5] : default_rfsw_ctrl[i - 5];
        CHECK("LR2021_SYSTEM_SET_DIO_RF_SWITCH_CFG_OC", hal.WriteCommand(LR2021_SYSTEM_SET_DIO_RF_SWITCH_CFG_OC, switchbuf, sizeof(switchbuf), SX12XX_Radio_All));
    }
    switchbuf[0] = 9;    // DIO9
    switchbuf[1] = 0x13; // IRQ, SLEEP PullAuto
    CHECK("LR2021_SYSTEM_SET_DIO_FUNCTION_OC", hal.WriteCommand(LR2021_SYSTEM_SET_DIO_FUNCTION_OC, switchbuf, sizeof(switchbuf), SX12XX_Radio_All));
}

void LR2021Driver::CorrectRegisterForSF6(const uint8_t sf, const SX12XX_Radio_Number_t radioNumber)
{
    // 8.3.1 SetModulationParams
    // - SF6 can be made compatible with the SX127x family in implicit mode via a register setting.
    // 3.7.3 WriteRegMemMask32

    if ((lr11xx_radio_lora_sf_t)sf == LR2021_RADIO_LORA_SF6)
    {
        uint8_t wrbuf[11];
        // Address
        wrbuf[0] = LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_ADDRESS >> 16; // MSB
        wrbuf[1] = LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_ADDRESS >> 8;  // MSB
        wrbuf[2] = LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_ADDRESS >> 0;  // MSB
        // Mask
        wrbuf[3] = LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK >> 24; // MSB
        wrbuf[4] = LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK >> 16; // bit18=1 and bit23=0
        wrbuf[5] = LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK >> 8;
        wrbuf[6] = LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK >> 0;
        // Data
        wrbuf[7] = 0x00;       // MSB
        wrbuf[8] = 0b00001000; // bit19=1
        wrbuf[9] = 0x00;
        wrbuf[10] = 0x00;
        CHECK("LR2021_REGMEM_WRITE_REGMEM32_MASK_OC", hal.WriteCommand(LR2021_REGMEM_WRITE_REGMEM32_MASK_OC, wrbuf, sizeof(wrbuf), radioNumber));
    }
}

/***
 * @brief: Schedule an output power change after the next transmit
 ***/
void LR2021Driver::SetOutputPower(const int8_t power, const bool isSubGHz)
{
    uint8_t pwrNew;

    if (isSubGHz)
    {
        pwrNew = constrain(power, LR2021_POWER_MIN_LF_PA, LR2021_POWER_MAX_LF_PA);

        if ((pwrPendingLF == PWRPENDING_NONE && pwrCurrentLF != pwrNew) || pwrPendingLF != pwrNew)
        {
            pwrPendingLF = pwrNew;
        }
    }
    else
    {
        pwrNew = constrain(power, LR2021_POWER_MIN_HF_PA, LR2021_POWER_MAX_HF_PA);

        if ((pwrPendingHF == PWRPENDING_NONE && pwrCurrentHF != pwrNew) || pwrPendingHF != pwrNew)
        {
            pwrPendingHF = pwrNew;
        }
    }
}

void ICACHE_RAM_ATTR LR2021Driver::CommitOutputPower()
{
    if (pwrPendingLF != PWRPENDING_NONE)
    {
        pwrCurrentLF = pwrPendingLF;
        pwrPendingLF = PWRPENDING_NONE;
        pwrForceUpdate = true;
    }

    if (pwrPendingHF != PWRPENDING_NONE)
    {
        pwrCurrentHF = pwrPendingHF;
        pwrPendingHF = PWRPENDING_NONE;
        pwrForceUpdate = true;
    }

    if (pwrForceUpdate)
    {
        WriteOutputPower(radio1isSubGHz ? pwrCurrentLF : pwrCurrentHF, radio1isSubGHz, SX12XX_Radio_1);
        if (GPIO_PIN_NSS_2 != UNDEF_PIN)
        {
            WriteOutputPower(radio2isSubGHz ? pwrCurrentLF : pwrCurrentHF, radio2isSubGHz, SX12XX_Radio_2);
        }
        pwrForceUpdate = false;
    }
}

void ICACHE_RAM_ATTR LR2021Driver::WriteOutputPower(const uint8_t power, const bool isSubGHz, const SX12XX_Radio_Number_t radioNumber)
{
    uint8_t Txbuf[2] = {power, LR2021_RADIO_RAMP_48_US};

    // 9.5.2 SetTxParams
    CHECK("LR2021_RADIO_SET_TX_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_TX_PARAMS_OC, Txbuf, sizeof(Txbuf), radioNumber));
}

void ICACHE_RAM_ATTR LR2021Driver::SetPaConfig(const bool isSubGHz, const SX12XX_Radio_Number_t radioNumber)
{
    uint8_t Pabuf[3] = {0};

    // 7.3.1 SetPaConfig
    if (isSubGHz)
    {
        Pabuf[0] = LR2021_RADIO_PA_SEL_LF;
        Pabuf[1] = 7 << 4 | 6; // PaDutyCycle
        Pabuf[2] = 16;         // PaHFDutyCycle
        CHECK("LR2021_RADIO_SET_PA_CFG_OC", hal.WriteCommand(LR2021_RADIO_SET_PA_CFG_OC, Pabuf, 3, radioNumber));
    }
    else
    {
        Pabuf[0] = LR2021_RADIO_PA_SEL_HF;
        Pabuf[1] = 6 << 4 | 0; // PaDutyCycle | PaSlices
        Pabuf[2] = 30;         // PaHFDutyCycle
        CHECK("LR2021_RADIO_SET_PA_CFG_OC", hal.WriteCommand(LR2021_RADIO_SET_PA_CFG_OC, Pabuf, 3, radioNumber));
    }
}

void LR2021Driver::SetMode(const lr11xx_RadioOperatingModes_t OPmode, const SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[5] = {0};

    switch (OPmode)
    {
    case LR2021_MODE_SLEEP:
        // 2.1.5.1 SetSleep
        CHECK("LR2021_SYSTEM_SET_SLEEP_OC", hal.WriteCommand(LR2021_SYSTEM_SET_SLEEP_OC, buf, 5, radioNumber));
        break;

    case LR2021_MODE_STDBY_RC:
        // 2.1.2.1 SetStandby
        buf[0] = 0x00;
        CHECK("LR2021_SYSTEM_SET_STANDBY_OC(RC)", hal.WriteCommand(LR2021_SYSTEM_SET_STANDBY_OC, buf, 1, radioNumber));
        break;

    case LR2021_MODE_STDBY_XOSC:
        // 2.1.2.1 SetStandby
        buf[0] = 0x01;
        CHECK("LR2021_SYSTEM_SET_STANDBY_OC(XOSC)", hal.WriteCommand(LR2021_SYSTEM_SET_STANDBY_OC, buf, 1, radioNumber));
        break;

    case LR2021_MODE_FS:
        // 2.1.9.1 SetFs
        CHECK("LR2021_SYSTEM_SET_FS_OC", hal.WriteCommand(LR2021_SYSTEM_SET_FS_OC, radioNumber));
        break;

    case LR2021_MODE_RX_CONT:
        // 6.3.5 SetRx
        CHECK("LR2021_RADIO_SET_RX_OC", hal.WriteCommand(LR2021_RADIO_SET_RX_OC, radioNumber));
        break;

    case LR2021_MODE_TX:
        // 6.3.6: SetTx
        CHECK("LR2021_RADIO_SET_TX_OC", hal.WriteCommand(LR2021_RADIO_SET_TX_OC, radioNumber));
        break;

    case LR2021_MODE_CAD:
        break;

    default:
        break;
    }
}

void LR2021Driver::ConfigModParamsLoRa(const uint8_t bw, const uint8_t sf, const uint8_t cr, const SX12XX_Radio_Number_t radioNumber)
{
    // 8.1.1 SetPacketType
    constexpr uint8_t packetType = LR2021_RADIO_PKT_TYPE_LORA;
    CHECK("LR2021_RADIO_SET_PKT_TYPE_OC", hal.WriteCommand(LR2021_RADIO_SET_PKT_TYPE_OC, &packetType, 1, radioNumber));

    // 8.3.1 SetModulationParams
    uint8_t buf[2];
    buf[0] = sf << 4 | bw;
    buf[1] = cr << 4;
    CHECK("LR2021_RADIO_SET_LORA_MODULATION_PARAM_OC", hal.WriteCommand(LR2021_RADIO_SET_LORA_MODULATION_PARAM_OC, buf, sizeof(buf), radioNumber));

    if (radioNumber & SX12XX_Radio_1 && radio1isSubGHz)
    {
        CorrectRegisterForSF6(sf, SX12XX_Radio_1);
    }

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        if (radioNumber & SX12XX_Radio_2 && radio2isSubGHz)
        {
            CorrectRegisterForSF6(sf, SX12XX_Radio_2);
        }
    }
}

void LR2021Driver::SetPacketParamsLoRa(const uint8_t PreambleLength, const lr11xx_RadioLoRaPacketLengthsModes_t HeaderType,
                                       const uint8_t PayloadLength, const uint8_t InvertIQ, const SX12XX_Radio_Number_t radioNumber)
{
    // 8.3.2 SetPacketParams
    uint8_t buf[4];
    buf[0] = PreambleLength >> 8; // MSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[1] = PreambleLength;      // LSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[2] = PayloadLength;       // PayloadLen defines the size of the payload
    buf[3] = HeaderType << 2 | InvertIQ;
    CHECK("LR2021_RADIO_SET_PKT_PARAM_OC", hal.WriteCommand(LR2021_RADIO_SET_LORA_PACKET_PARAMS_OC, buf, sizeof(buf), radioNumber));
}

void ICACHE_RAM_ATTR LR2021Driver::SetFrequencyReg(const uint32_t freq, const SX12XX_Radio_Number_t radioNumber, const bool doRx, const uint32_t rxTime)
{
    uint8_t buf[4] = {
        (uint8_t)(freq >> 24),
        (uint8_t)(freq >> 16),
        (uint8_t)(freq >> 8),
        (uint8_t)(freq),
    };
    // 7.2.1 SetRfFrequency
    CHECK("LR2021_RADIO_SET_RF_FREQUENCY_OC", hal.WriteCommand(LR2021_RADIO_SET_RF_FREQUENCY_OC, buf, 4, radioNumber));
    if (doRx)
    {
        SetMode(LR2021_MODE_RX_CONT, radioNumber);
    }
    currFreq = freq;
}

// 4.1.1 SetDioIrqParams
void LR2021Driver::SetDioIrqParams()
{
    constexpr uint32_t enable = LR2021_IRQ_TX_DONE | LR2021_IRQ_RX_DONE;
    uint8_t buf[]{9, (uint8_t)(enable >> 24), (uint8_t)(enable >> 16), (uint8_t)(enable >> 8), (uint8_t)(enable)};
    CHECK("LR2021_SYSTEM_SET_DIOIRQPARAMS_OC", hal.WriteCommand(LR2021_SYSTEM_SET_DIOIRQPARAMS_OC, buf, sizeof(buf), SX12XX_Radio_All));
}

// 3.4.1 GetStatus
uint32_t ICACHE_RAM_ATTR LR2021Driver::GetIrqStatus(const SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status[6] = {0};
    status[0] = LR2021_SYSTEM_CLEAR_IRQ_OC >> 8;
    status[1] = LR2021_SYSTEM_CLEAR_IRQ_OC & 0xFF;
    status[2] = 0xFF;
    status[3] = 0xFF;
    status[4] = 0xFF;
    status[5] = 0xFF;
    CHECK("LR2021_SYSTEM_CLEAR_IRQ_OC(Get)", hal.ReadCommand(status, sizeof(status), radioNumber));
    return status[2] << 24 | status[3] << 16 | status[4] << 8 | status[5];
}

void ICACHE_RAM_ATTR LR2021Driver::ClearIrqStatus(const SX12XX_Radio_Number_t radioNumber)
{
    GetIrqStatus(radioNumber);
}

void ICACHE_RAM_ATTR LR2021Driver::TXnbISR()
{
#ifdef DEBUG_LR2021_OTA_TIMING
    endTX = micros();
    DBGLN("TOA: %d", endTX - beginTX);
#endif
    CommitOutputPower();
    TXdoneCallback();
}

void ICACHE_RAM_ATTR LR2021Driver::TXnb(uint8_t *data, const bool sendGeminiBuffer, uint8_t *dataGemini, const SX12XX_Radio_Number_t radioNumber)
{
    transmittingRadio = radioNumber;

    // //catch TX timeout
    // if (currOpmode == SX1280_MODE_TX)
    // {
    //     DBGLN("Timeout!");
    //     SetMode(SX1280_MODE_FS, SX12XX_Radio_All);
    //     ClearIrqStatus(SX1280_IRQ_RADIO_ALL, SX12XX_Radio_All);
    //     TXnbISR();
    //     return;
    // }

    if (radioNumber == SX12XX_Radio_NONE)
    {
        SetMode(fallBackMode, SX12XX_Radio_All);
        return;
    }

#if defined(DEBUG_RCVR_SIGNAL_STATS)
    if (radioNumber == SX12XX_Radio_All || radioNumber == SX12XX_Radio_1)
    {
        rxSignalStats[0].telem_count++;
    }
    if (radioNumber == SX12XX_Radio_All || radioNumber == SX12XX_Radio_2)
    {
        rxSignalStats[1].telem_count++;
    }
#endif

    // Normal diversity mode
    if (GPIO_PIN_NSS_2 != UNDEF_PIN && radioNumber != SX12XX_Radio_All)
    {
        // Make sure the unused radio is in FS mode and will not receive the tx packet.
        if (radioNumber == SX12XX_Radio_1)
        {
            SetMode(fallBackMode, SX12XX_Radio_2);
        }
        else
        {
            SetMode(fallBackMode, SX12XX_Radio_1);
        }
    }

    WORD_ALIGNED_ATTR uint8_t outBuffer[32] = {0};
    codec->encode(outBuffer, data, PayloadLength);
    if (sendGeminiBuffer)
    {
        hal.WriteCommand(LR2021_RADIO_WRITE_TX_FIFO, outBuffer, PayloadLength, SX12XX_Radio_1);
        codec->encode(outBuffer, dataGemini, PayloadLength);
        hal.WriteCommand(LR2021_RADIO_WRITE_TX_FIFO, outBuffer, PayloadLength, SX12XX_Radio_2);
    }
    else
    {
        hal.WriteCommand(LR2021_RADIO_WRITE_TX_FIFO, outBuffer, PayloadLength, radioNumber);
    }
    hal.WriteCommand(LR2021_RADIO_SET_TX_OC, radioNumber);
#ifdef DEBUG_LLCC68_OTA_TIMING
    beginTX = micros();
#endif
}

inline void ICACHE_RAM_ATTR LR2021Driver::DecodeRssiSnr(const SX12XX_Radio_Number_t radioNumber, uint8_t *buf)
{
    if (useFSK) {
        CHECK("LR2021_RADIO_GET_FSK_PACKET_STATUS_OC", hal.WriteCommand(LR2021_RADIO_GET_FSK_PACKET_STATUS_OC, radioNumber));
        CHECK("LR2021_RADIO_GET_FSK_PACKET_STATUS_OC", hal.ReadCommand(buf, 8, radioNumber));
    } else {
        CHECK("LR2021_RADIO_GET_LORA_PACKET_STATUS_OC", hal.WriteCommand(LR2021_RADIO_GET_LORA_PACKET_STATUS_OC, radioNumber));
        CHECK("LR2021_RADIO_GET_LORA_PACKET_STATUS_OC", hal.ReadCommand(buf, 8, radioNumber));
    }

    // RssiPkt defines the average RSSI over the last packet received. RSSI value in dBm is –RssiPkt
    const int8_t rssi = -(int8_t)buf[useFSK ? 4 : 5];

    // If radio # is 0, update LastPacketRSSI, otherwise LastPacketRSSI2
    radioNumber == SX12XX_Radio_1 ? LastPacketRSSI = rssi : LastPacketRSSI2 = rssi;

    // Update whatever SNRs we have
    LastPacketSNRRaw = useFSK ? 0 : (int8_t)buf[4];

#if defined(DEBUG_RCVR_SIGNAL_STATS)
    // stat updates
    int i = radioNumber == SX12XX_Radio_1 ? 0 : 1;
    rxSignalStats[i].irq_count++;
    rxSignalStats[i].rssi_sum += rssi;
    rxSignalStats[i].snr_sum += LastPacketSNRRaw;
    if (LastPacketSNRRaw > rxSignalStats[i].snr_max)
    {
        rxSignalStats[i].snr_max = LastPacketSNRRaw;
    }
#endif
}

bool ICACHE_RAM_ATTR LR2021Driver::RXnbISR(const SX12XX_Radio_Number_t radioNumber)
{
    // GetPacket
    memset(rx_buf, 0, sizeof(rx_buf));
    rx_buf[0] = LR2021_RADIO_READ_RX_FIFO >> 8;
    rx_buf[1] = LR2021_RADIO_READ_RX_FIFO;
    CHECK("LR2021_RADIO_READ_RX_FIFO", hal.ReadCommand(rx_buf, PayloadLength + 2, radioNumber));
    codec->decode(RXdataBuffer, rx_buf + 2, PayloadLength);
    CHECK("CLEAR", hal.WriteCommand(LR2021_SYSTEM_CLEAR_RX_FIFO_OC, radioNumber));
    if (!RXdoneCallback(SX12XX_RX_OK))
    {
#if defined(DEBUG_RCVR_SIGNAL_STATS)
        rxSignalStats[radioNumber == SX12XX_Radio_1 ? 0 : 1].fail_count++;
#endif
        return false;
    }
    return true;
}

void ICACHE_RAM_ATTR LR2021Driver::RXnb()
{
    SetMode(LR2021_MODE_RX_CONT, SX12XX_Radio_All);
}

bool ICACHE_RAM_ATTR LR2021Driver::GetFrequencyErrorbool(SX12XX_Radio_Number_t radioNumber)
{
    return false;
}

// 7.2.8 GetRssiInst
void ICACHE_RAM_ATTR LR2021Driver::StartRssiInst(const SX12XX_Radio_Number_t radioNumber)
{
    CHECK("LR2021_RADIO_GET_RSSI_INST_OC", hal.WriteCommand(LR2021_RADIO_GET_RSSI_INST_OC, radioNumber));
}

int8_t ICACHE_RAM_ATTR LR2021Driver::GetRssiInst(const SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status[4] = {0};
    hal.ReadCommand(status, sizeof(status), radioNumber);
    return -(int8_t)(status[2]); // status[3] contains the bottom bit of if 0.5dB so we ignore it
}

void ICACHE_RAM_ATTR LR2021Driver::CheckForSecondPacket()
{
    hasSecondRadioGotData = false;
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        constexpr SX12XX_Radio_Number_t radio[2] = {SX12XX_Radio_1, SX12XX_Radio_2};
        const uint8_t processingRadioIdx = (instance->processingPacketRadio == SX12XX_Radio_1) ? 0 : 1;
        const uint8_t secondRadioIdx = !processingRadioIdx;
        const uint32_t secondIrqStatus = instance->GetIrqStatus(radio[secondRadioIdx]);
        if (secondIrqStatus & LR2021_IRQ_RX_DONE)
        {
            memset(rx2_buf, 0, sizeof(rx2_buf));
            rx2_buf[0] = LR2021_RADIO_READ_RX_FIFO >> 8;
            rx2_buf[1] = LR2021_RADIO_READ_RX_FIFO;
            CHECK("LR2021_RADIO_READ_RX_FIFO", hal.ReadCommand(rx2_buf, PayloadLength + 2, radio[secondRadioIdx]));
            codec->decode(RXdataBufferSecond, rx2_buf + 2, PayloadLength);
            hasSecondRadioGotData = true;
        }
    }
}

void ICACHE_RAM_ATTR LR2021Driver::GetLastPacketStats()
{
    const SX12XX_Radio_Number_t radioNumber = processingPacketRadio == SX12XX_Radio_1 ? SX12XX_Radio_2 : SX12XX_Radio_1;

    // by default, set the strongest receiving radio to be the current processing radio (which got a successful packet)
    strongestReceivingRadio = processingPacketRadio;
    DecodeRssiSnr(processingPacketRadio, rx_buf);
#if defined(DEBUG_RCVR_SIGNAL_STATS)
    irq_count_or++;
#endif

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        // when both radio got the packet, use the better RSSI one
        if (hasSecondRadioGotData)
        {
            const int8_t firstSNR = LastPacketSNRRaw;
            DecodeRssiSnr(radioNumber, rx2_buf);
            LastPacketSNRRaw = fuzzy_snr(LastPacketSNRRaw, firstSNR, FuzzySNRThreshold);
            // Update the strongest receiving radio to be the one with better signal strength
            strongestReceivingRadio = LastPacketRSSI > LastPacketRSSI2 ? SX12XX_Radio_1 : SX12XX_Radio_2;
#if defined(DEBUG_RCVR_SIGNAL_STATS)
            irq_count_both++;
        }
        else
        {
            rxSignalStats[radioNumber == SX12XX_Radio_1 ? 0 : 1].fail_count++;
#endif
        }
    }
}

void ICACHE_RAM_ATTR LR2021Driver::IsrCallback_1()
{
    IsrCallback(SX12XX_Radio_1);
}

void ICACHE_RAM_ATTR LR2021Driver::IsrCallback_2()
{
    IsrCallback(SX12XX_Radio_2);
}

void ICACHE_RAM_ATTR LR2021Driver::IsrCallback(const SX12XX_Radio_Number_t radioNumber)
{
    instance->processingPacketRadio = radioNumber;
    const SX12XX_Radio_Number_t otherRadioNumber = radioNumber == SX12XX_Radio_1 ? SX12XX_Radio_2 : SX12XX_Radio_1;

    const uint32_t irqStatus = instance->GetIrqStatus(radioNumber);
    if (irqStatus & LR2021_IRQ_TX_DONE)
    {
        instance->TXnbISR();
        if (GPIO_PIN_NSS_2 != UNDEF_PIN)
        {
            instance->ClearIrqStatus(otherRadioNumber);
        }
    }
    else if (irqStatus & LR2021_IRQ_RX_DONE)
    {
        instance->RXnbISR(radioNumber);
    }
}
