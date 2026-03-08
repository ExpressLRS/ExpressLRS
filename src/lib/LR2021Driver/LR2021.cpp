#include "LR2021.h"
#include "LR2021_hal.h"
#include "FEC.h"
#include "logging.h"

#include <LittleFS.h>

LR2021Hal hal;
LR2021Driver *LR2021Driver::instance = nullptr;

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

#if defined(DEBUG_LOG)
#define CHECK(msg, x)                        \
    do                                       \
    {                                        \
        uint16_t err = x;                    \
        if ((err & 0x400) != 0x400)          \
        {                                    \
            DBGLN("FAILED %s %x", msg, err); \
        }                                    \
    } while (false)
#else
#define CHECK(msg, x) x
#endif

LR2021Driver::LR2021Driver()
{
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
    uint8_t buffer[4] {};
    CHECK("LR2021_SYSTEM_GET_VERSION_OC", hal.WriteCommand(LR2021_SYSTEM_GET_VERSION_OC, radioNumber));
    CHECK("LR2021_SYSTEM_GET_VERSION_OC(resp)", hal.ReadCommand(buffer, sizeof(buffer), radioNumber));
    hal.WaitOnBusy(radioNumber);

    const uint16_t version = static_cast<uint16_t>(buffer[2] << 8 | buffer[3]);
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
    if (GPIO_PIN_NSS_2 != UNDEF_PIN && !CheckVersion(SX12XX_Radio_2))
        return false;

    hal.IsrCallback_1 = &LR2021Driver::IsrCallback_1;
    hal.IsrCallback_2 = &LR2021Driver::IsrCallback_2;

    // Clear Errors
    CHECK("LR2021_SYSTEM_CLEAR_ERRORS_OC", hal.WriteCommand(LR2021_SYSTEM_CLEAR_ERRORS_OC, SX12XX_Radio_All)); // Remove later?  Might not be required???

    // 6.3.7 SetRxTxFallbackMode
    constexpr uint8_t FBbuf = LR2021_RADIO_FALLBACK_FS;
    fallBackMode = LR2021_MODE_FS;
    CHECK("LR2021_RADIO_SET_RX_TX_FALLBACK_MODE_OC", hal.WriteCommand(LR2021_RADIO_SET_RX_TX_FALLBACK_MODE_OC, &FBbuf, 1, SX12XX_Radio_All));

    // 6.3.17 SetDefaultRxTxTimeout
    constexpr uint8_t timeouts[] {
        0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00,
    };
    CHECK("LR2021_RADIO_SET_DEFAULT_RX_TX_TIMEOUT_OC", hal.WriteCommand(LR2021_RADIO_SET_DEFAULT_RX_TX_TIMEOUT_OC, timeouts, sizeof(timeouts), SX12XX_Radio_All));

    SetDioAsRfSwitch();

    // 6.3.18 SetRegMode
    const uint8_t RegMode = OPT_USE_HARDWARE_DCDC ? 0x02 : 0x00;
    CHECK("LR2021_SYSTEM_SET_REGMODE_OC", hal.WriteCommand(LR2021_SYSTEM_SET_REGMODE_OC, &RegMode, 1, SX12XX_Radio_All)); // Enable DCDC converter instead of LDO

    // 6.4.2 Calibrate
    constexpr uint8_t calibrate = 0x7F;
    CHECK("LR2021_SYSTEM_CALIBRATE_OC", hal.WriteCommand(LR2021_SYSTEM_CALIBRATE_OC, &calibrate, 1, SX12XX_Radio_All));

    // 6.4.2 CalibFE
    const uint8_t calibrateFE[]{static_cast<uint8_t>((lowBandFreq / 4000000) >> 8), static_cast<uint8_t>(lowBandFreq / 4000000), static_cast<uint8_t>(((highBandFreq / 4000000) >> 8) | 0x80), static_cast<uint8_t>(highBandFreq / 4000000)};
    CHECK("LR2021_SYSTEM_CALIBRATE_FRONTEND_OC", hal.WriteCommand(LR2021_SYSTEM_CALIBRATE_FRONTEND_OC, calibrateFE, sizeof(calibrateFE), SX12XX_Radio_All));

    return true;
}

// 12.2.1 SetTxCw
void LR2021Driver::startCWTest(const uint32_t freq, const SX12XX_Radio_Number_t radioNumber)
{
    // Set a basic Config that can be used for both 2.4G and SubGHz bands.
    Config(LR2021_RADIO_LORA_BW_62, LR2021_RADIO_LORA_SF6, LR2021_RADIO_LORA_CR_4_8, freq, 12, false, 8, RADIO_MODULATION_LORA_2G4, 0, 0, 0, 0, radioNumber);
    constexpr uint8_t mode = 0x02;
    CHECK("LR2021_RADIO_SET_TX_TEST_MODE_OC", hal.WriteCommand(LR2021_RADIO_SET_TX_TEST_MODE_OC, &mode, 1, radioNumber));
}

void LR2021Driver::Config(const uint8_t bw, const uint8_t sf, const uint8_t cr, const uint32_t regfreq,
                          const uint8_t PreambleLength, const bool InvertIQ, const uint8_t _PayloadLength,
                          const radio_band_modulation_t _modulation, const uint8_t fskSyncWord1, const uint8_t fskSyncWord2,
                          const uint32_t flrcSyncWord, const uint16_t flrcCrcSeed,
                          const SX12XX_Radio_Number_t radioNumber)
{
    PayloadLength = _PayloadLength;
    modulation = _modulation;

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
    lr20xx_radio_lora_iq_t inverted = InvertIQ ? LR2021_RADIO_LORA_IQ_INVERTED : LR2021_RADIO_LORA_IQ_STANDARD;
    // IQinverted is always STANDARD for 900
    if (isSubGHz)
    {
        inverted = LR2021_RADIO_LORA_IQ_STANDARD;
    }

    SetMode(LR2021_MODE_STDBY_RC, radioNumber);

    codec = &copyCodec;
    if (isFLRCModulation(modulation)) {
        DBGLN("Config FLRC");
        ConfigModParamsFLRC(bw, cr, sf, radioNumber);
        SetPacketParamsFLRC(PreambleLength, flrcSyncWord, flrcCrcSeed, radioNumber);
    }
    else if (isGFSKModulation(modulation))
    {
        DBGLN("Config FSK");
        const uint32_t bitrate = static_cast<uint32_t>(bw) * 10000;
        const uint8_t bwf = sf;
        const uint32_t fdev = static_cast<uint32_t>(cr) * 1000;
        ConfigModParamsFSK(bitrate, bwf, fdev, radioNumber);

        // Increase packet length for FEC used only on 1000Hz 2.4GHz.
        if (!isSubGHz)
        {
            codec = &fecCodec;
            PayloadLength = 14;
        }

        SetPacketParamsFSK(PreambleLength, radioNumber);
        SetFSKSyncWord(fskSyncWord1, fskSyncWord2, radioNumber);
    }
    else
    {
        DBGLN("Config LoRa");
        ConfigModParamsLoRa(bw, sf, cr, radioNumber);
        SetPacketParamsLoRa(PreambleLength, inverted, radioNumber);
    }

    SetFrequencyReg(regfreq, radioNumber, false);

    // 7.2.2 SetRxPath
    if (isSubGHz)
    {
        constexpr uint8_t buf[] {0x00, 0x00};
        CHECK("LR2021_RADIO_SET_RX_PATH_OC", hal.WriteCommand(LR2021_RADIO_SET_RX_PATH_OC, buf, sizeof(buf), radioNumber));
    }
    else
    {
        const uint8_t buf[] {0x01, static_cast<uint8_t>(isFLRCModulation(modulation) ? 0x00 : 0x04)};
        CHECK("LR2021_RADIO_SET_RX_PATH_OC", hal.WriteCommand(LR2021_RADIO_SET_RX_PATH_OC, buf, sizeof(buf), radioNumber));
    }

    ClearIrqStatus(radioNumber);

    SetPaConfig(isSubGHz, radioNumber); // Must be called after changing rf modes between subG and 2.4G.  This sets the correct rf amps, and txen pins to be used.
    pwrForceUpdate = true;              // force an update of the output power because the band may have changed, and we need to configure the power for the band.
    CommitOutputPower();

    CHECK("LR2021_SYSTEM_CLEAR_RX_FIFO_OC", hal.WriteCommand(LR2021_SYSTEM_CLEAR_RX_FIFO_OC, radioNumber));
    CHECK("LR2021_SYSTEM_CLEAR_TX_FIFO_OC", hal.WriteCommand(LR2021_SYSTEM_CLEAR_TX_FIFO_OC, radioNumber));
}

void LR2021Driver::ConfigModParamsLoRa(const uint8_t bw, const uint8_t sf, const uint8_t cr, const SX12XX_Radio_Number_t radioNumber)
{
    // 8.1.1 SetPacketType
    constexpr uint8_t packetType = LR2021_RADIO_PKT_TYPE_LORA;
    CHECK("LR2021_RADIO_SET_PKT_TYPE_OC", hal.WriteCommand(LR2021_RADIO_SET_PKT_TYPE_OC, &packetType, 1, radioNumber));

    // 8.3.1 SetModulationParams
    const uint8_t buf[] {
        static_cast<uint8_t>(sf << 4 | bw),
        static_cast<uint8_t>(cr << 4),
    };
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

void LR2021Driver::SetPacketParamsLoRa(const uint8_t PreambleLength, const uint8_t InvertIQ, const SX12XX_Radio_Number_t radioNumber)
{
#if defined(DEBUG_FREQ_CORRECTION) // TODO Check if this available with the LR2021?
    constexpr lr20xx_RadioLoRaPacketLengthsModes_t packetLengthType = LR2021_LORA_PACKET_EXPLICIT;
#else
    constexpr lr20xx_RadioLoRaPacketLengthsModes_t packetLengthType = LR2021_LORA_PACKET_IMPLICIT;
#endif

    // 8.3.2 SetPacketParams
    const uint8_t buf[] {
        static_cast<uint8_t>(PreambleLength >> 8),   // MSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised
        PreambleLength,                 // LSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised
        PayloadLength,                  // PayloadLen defines the size of the payload
        static_cast<uint8_t>(packetLengthType << 2 | InvertIQ),
    };
    CHECK("LR2021_RADIO_SET_LORA_PACKET_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_LORA_PACKET_PARAMS_OC, buf, sizeof(buf), radioNumber));
}

void LR2021Driver::ConfigModParamsFSK(const uint32_t Bitrate, const uint8_t BWF, const uint32_t Fdev, const SX12XX_Radio_Number_t radioNumber)
{
    // 8.1.1 SetPacketType
    constexpr uint8_t packetType = LR2021_RADIO_PKT_TYPE_FSK;
    CHECK("LR2021_RADIO_SET_PKT_TYPE_OC", hal.WriteCommand(LR2021_RADIO_SET_PKT_TYPE_OC, &packetType, 1, radioNumber));

    // 11.3.1 SetFskModulationParams
    const uint8_t buf[] {
        static_cast<uint8_t>(Bitrate >> 24),
        static_cast<uint8_t>(Bitrate >> 16),
        static_cast<uint8_t>(Bitrate >> 8),
        static_cast<uint8_t>(Bitrate),
        LR2021_RADIO_GFSK_PULSE_SHAPE_OFF, // Pulse Shape - 0x00: No filter applied
        BWF,
        static_cast<uint8_t>(Fdev >> 16),
        static_cast<uint8_t>(Fdev >> 8),
        static_cast<uint8_t>(Fdev >> 0),
    };
    CHECK("LR2021_RADIO_SET_FSK_MODULATION_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_MODULATION_PARAMS_OC, buf, sizeof(buf), radioNumber));
}

void LR2021Driver::SetPacketParamsFSK(const uint8_t PreambleLength, const SX12XX_Radio_Number_t radioNumber)
{
    // 11.3.2 SetFskPacketParams
    const uint8_t packetParams[] {
        0,                                             // MSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised
        PreambleLength,                                // LSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised
        LR2021_RADIO_GFSK_PREAMBLE_DETECTOR_MIN_8BITS, // Pbl Detect
        0x00,                                          // Payload len in bytes / address filtering disabled / fixed length packet
        0,                                             // MSB PayloadLen
        PayloadLength,                                 // LSB PayloadLen
        LR2021_RADIO_GFSK_CRC_OFF | LR2021_RADIO_GFSK_DC_FREE_WHITENING,
    };
    CHECK("LR2021_RADIO_SET_FSK_PACKET_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_PACKET_PARAMS_OC, packetParams, sizeof(packetParams), radioNumber));

    // 11.3.3 SetFskWhiteningParams
    constexpr uint16_t lr1121DefaultWhitening = 0x0100;
    constexpr uint8_t whiteningParams[] {
        LR2021_GFSK_WHITENING_TYPE_SX126X_LR11XX | lr1121DefaultWhitening >> 8,
        lr1121DefaultWhitening & 0xFF,
    };
    CHECK("LR2021_RADIO_SET_FSK_WHITENING_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_WHITENING_PARAMS_OC, whiteningParams, sizeof(whiteningParams), radioNumber));
}

void LR2021Driver::SetFSKSyncWord(const uint8_t fskSyncWord1, const uint8_t fskSyncWord2, const SX12XX_Radio_Number_t radioNumber)
{
    // 11.3.5 SetFskSyncWord
    // SyncWordLen is 16 bits.  Fill the rest with preamble bytes.
    const uint8_t syncBuf[] {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, fskSyncWord1, fskSyncWord2, LR2021_GFSK_SYNCWORD_BIT_ORDER_MSB | 16};
    CHECK("LR2021_RADIO_SET_FSK_SYNC_WORD_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_SYNC_WORD_OC, syncBuf, sizeof(syncBuf), radioNumber));
}

void LR2021Driver::ConfigModParamsFLRC(const uint8_t bw, const uint8_t cr, const uint32_t bt, const SX12XX_Radio_Number_t radioNumber)
{
    // 8.1.1 SetPacketType
    constexpr uint8_t packetType = LR2021_RADIO_PKT_TYPE_FLRC;
    CHECK("LR2021_RADIO_SET_PKT_TYPE_OC", hal.WriteCommand(LR2021_RADIO_SET_PKT_TYPE_OC, &packetType, 1, radioNumber));

    // 18.4.1 SetFlrcModulationParams
    const uint8_t buf[] {
        bw,
        static_cast<uint8_t>(cr << 4 | bt),
    };
    CHECK("LR2021_RADIO_SET_FLRC_MODULATION_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FLRC_MODULATION_PARAMS_OC, buf, sizeof(buf), radioNumber));
}

void LR2021Driver::SetPacketParamsFLRC(const uint8_t PreambleLength, const uint32_t syncWord, const uint16_t crcSeed, const SX12XX_Radio_Number_t radioNumber)
{
    // 18.4.2 SetFlrcPacketParams
    const uint8_t preambleBits = (PreambleLength / 4) - 1;
    constexpr uint8_t syncWordIndex = 1;
    const uint8_t buf[] {
        static_cast<uint8_t>(preambleBits << 2 | LR2021_RADIO_FLRC_SYNC_32BITS),                                                     // 32bit preamble, 32bit sync_len
        syncWordIndex << 6 | syncWordIndex << 3 | LR2021_RADIO_FLRC_PAYLOAD_FIXED_LEN | LR2021_RADIO_FLRC_CRC_24BITS,   // syncword 1, match 1, fixed len, 24bit/3-byte crc
        0,                                                                                                              // MSB PayloadLen
        PayloadLength,                                                                                                  // LSB PayloadLen
    };
    CHECK("LR2021_RADIO_SET_FLRC_PACKET_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FLRC_PACKET_PARAMS_OC, buf, sizeof(buf), radioNumber));

    // 11.3.4 SetFskCrcParams (This is shared across _all_ the FSK based modulations.)
    /*
     * From DS_SX1280-1_V3.2.pdf table 14-40
     * P24(x) = x^24 + x^22 + x^20 + x^19 + x^18 + x^16 + x^14 + x^13 + x^11 + x^10 + x^8 + x^7+ x^6 + x^3 + x + 1
     *
     * NB: the documentation for the SX1280 CRC Table 14-40 shows only 2 registers (i.e. 16 bit) for the initialization value,
     * however, for 3-byte CRC (24-bits) there would eed to be a 3rd top 8-bits byte, it appears that the default value on the SX1280
     * for this is 0xFF, hence that value in the parameters below.
     */
    constexpr uint32_t polynomial = 0x5D6DCB;
    const uint8_t crcParams[] {
        (polynomial >> 24), (polynomial >> 16), static_cast<uint8_t>(polynomial >> 8), static_cast<uint8_t>(polynomial),
        0, 0xFF, static_cast<uint8_t>(crcSeed >> 8), static_cast<uint8_t>(crcSeed)
    };
    CHECK("LR2021_RADIO_SET_FSK_CRC_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_FSK_CRC_PARAMS_OC, crcParams, sizeof(crcParams), radioNumber));

    // 18.4.5 SetFlrcSyncword
    uint8_t syncWordBuf[] {1, static_cast<uint8_t>(syncWord >> 24), static_cast<uint8_t>(syncWord >> 16), static_cast<uint8_t>(syncWord >> 8), static_cast<uint8_t>(syncWord)};

    /*
     * DS_SX1280-1_V3.2.pdf - 16.4 FLRC Modem: Increased PER in FLRC Packets with Synch Word
     * Simplified from the SX1280 as we only use CR 1/2
     */
    if ((syncWordBuf[1] == 0x8C && syncWordBuf[2] == 0x38) || (syncWordBuf[1] == 0x63 && syncWordBuf[2] == 0x0E))
    {
        const uint8_t temp = syncWordBuf[1];
        syncWordBuf[1] = syncWordBuf[2];
        syncWordBuf[2] = temp;
    }

    CHECK("LR2021_RADIO_SET_FLRC_SYNCWORD_OC", hal.WriteCommand(LR2021_RADIO_SET_FLRC_SYNCWORD_OC, syncWordBuf, sizeof(syncWordBuf), radioNumber));
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
    constexpr uint8_t default_rfsw_ctrl[] {
        0x10,   // DIO5 = 2.4 TX
        0x08,   // DIO6 = 2.4 RX
        0x02,   // DIO7 = subGHz TX
        0x04,   // DIO8 = subGHz RX
        0xFF,   // DIO9 = IRQ pin
        0x00,   // DIO10 = nothing
        0x00,   // DIO11 = nothing
    };

    for (int i = 5; i <= 11; i++)
    {
        const uint8_t pinConfig = LR1121_RFSW_CTRL ? LR1121_RFSW_CTRL[i - 5] : default_rfsw_ctrl[i - 5];
        uint8_t switchbuf[2];
        switchbuf[0] = i;
        if (pinConfig == 0xFF) {
            // Set DIO as Interrupt pin
            switchbuf[1] = 0x10; // IRQ, SLEEP PullNone
            CHECK("LR2021_SYSTEM_SET_DIO_FUNCTION_OC", hal.WriteCommand(LR2021_SYSTEM_SET_DIO_FUNCTION_OC, switchbuf, sizeof(switchbuf), SX12XX_Radio_All));
            SetDioIrqParams(i);
        } else {
            // Set DIO as RF output, and set the RF state(s) for the DIO
            switchbuf[1] = i == 5 ? 0x22 : 0x23; // RF switch on DIO, SLEEP PullAuto (except DIO5 which *must* be PullUp)
            CHECK("LR2021_SYSTEM_SET_DIO_FUNCTION_OC", hal.WriteCommand(LR2021_SYSTEM_SET_DIO_FUNCTION_OC, switchbuf, sizeof(switchbuf), SX12XX_Radio_All));
            switchbuf[1] = pinConfig;
            CHECK("LR2021_SYSTEM_SET_DIO_RF_SWITCH_CFG_OC", hal.WriteCommand(LR2021_SYSTEM_SET_DIO_RF_SWITCH_CFG_OC, switchbuf, sizeof(switchbuf), SX12XX_Radio_All));
        }
    }
}

void LR2021Driver::CorrectRegisterForSF6(const uint8_t sf, const SX12XX_Radio_Number_t radioNumber)
{
    // 8.3.1 SetModulationParams
    // - SF6 can be made compatible with the SX127x family in implicit mode via a register setting.
    // 3.7.3 WriteRegMemMask32

    if (sf == LR2021_RADIO_LORA_SF6)
    {
        constexpr uint8_t wrbuf[] {
            // Address
            static_cast<uint8_t>(LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_ADDRESS >> 16), // MSB
            static_cast<uint8_t>(LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_ADDRESS >> 8),  // MSB
            static_cast<uint8_t>(LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_ADDRESS >> 0),  // MSB
            // Mask
            static_cast<uint8_t>(LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK >> 24),    // MSB
            static_cast<uint8_t>(LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK >> 16),    // bit18=1 and bit23=0
            static_cast<uint8_t>(LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK >> 8),
            static_cast<uint8_t>(LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK >> 0),
            // Data (bit 19 set)
            0x00, 0x08, 0x00, 0x00,
        };
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
        WriteOutputPower(radio1isSubGHz ? pwrCurrentLF : pwrCurrentHF, SX12XX_Radio_1);
        if (GPIO_PIN_NSS_2 != UNDEF_PIN)
        {
            WriteOutputPower(radio2isSubGHz ? pwrCurrentLF : pwrCurrentHF, SX12XX_Radio_2);
        }
        pwrForceUpdate = false;
    }
}

void ICACHE_RAM_ATTR LR2021Driver::WriteOutputPower(const uint8_t power, const SX12XX_Radio_Number_t radioNumber)
{
    const uint8_t Txbuf[] {
        power,
        LR2021_RADIO_RAMP_48_US,
    };

    // 9.5.2 SetTxParams
    CHECK("LR2021_RADIO_SET_TX_PARAMS_OC", hal.WriteCommand(LR2021_RADIO_SET_TX_PARAMS_OC, Txbuf, sizeof(Txbuf), radioNumber));
}

void ICACHE_RAM_ATTR LR2021Driver::SetPaConfig(const bool isSubGHz, const SX12XX_Radio_Number_t radioNumber)
{
    // 7.3.1 SetPaConfig
    if (isSubGHz)
    {
        constexpr uint8_t Pabuf[] {
            LR2021_RADIO_PA_SEL_LF,
            7 << 4 | 6, // PaDutyCycle
            16,         // PaHFDutyCycle (default when not using HF)
        };
        CHECK("LR2021_RADIO_SET_PA_CFG_OC", hal.WriteCommand(LR2021_RADIO_SET_PA_CFG_OC, Pabuf, 3, radioNumber));
    }
    else
    {
        constexpr uint8_t Pabuf[] {
            LR2021_RADIO_PA_SEL_HF,
            6 << 4 | 7, // PaDutyCycle | PaSlices (default when not using LF)
            30,         // PaHFDutyCycle
        };
        CHECK("LR2021_RADIO_SET_PA_CFG_OC", hal.WriteCommand(LR2021_RADIO_SET_PA_CFG_OC, Pabuf, 3, radioNumber));
    }
}

void LR2021Driver::SetMode(const lr20xx_RadioOperatingModes_t OPmode, const SX12XX_Radio_Number_t radioNumber)
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

void ICACHE_RAM_ATTR LR2021Driver::SetFrequencyReg(const uint32_t freq, const SX12XX_Radio_Number_t radioNumber, const bool doRx, const uint32_t rxTime)
{
    const uint8_t buf[] {
        static_cast<uint8_t>(freq >> 24),
        static_cast<uint8_t>(freq >> 16),
        static_cast<uint8_t>(freq >> 8),
        static_cast<uint8_t>(freq),
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
void LR2021Driver::SetDioIrqParams(const uint8_t dio)
{
    constexpr uint32_t enable = LR2021_IRQ_TX_DONE | LR2021_IRQ_RX_DONE;
    const uint8_t buf[] {
        dio,
        static_cast<uint8_t>(enable >> 24), static_cast<uint8_t>(enable >> 16), static_cast<uint8_t>(enable >> 8), static_cast<uint8_t>(enable),
    };
    CHECK("LR2021_SYSTEM_SET_DIOIRQPARAMS_OC", hal.WriteCommand(LR2021_SYSTEM_SET_DIOIRQPARAMS_OC, buf, sizeof(buf), SX12XX_Radio_All));
}

// 3.4.1 GetStatus
uint32_t ICACHE_RAM_ATTR LR2021Driver::GetIrqStatus(const SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status[] {
        LR2021_SYSTEM_CLEAR_IRQ_OC >> 8,
        LR2021_SYSTEM_CLEAR_IRQ_OC & 0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
    };
    CHECK("LR2021_SYSTEM_CLEAR_IRQ_OC(Get)", hal.ReadCommand(status, sizeof(status), radioNumber));
    return status[2] << 24 | status[3] << 16 | status[4] << 8 | status[5];
}

void ICACHE_RAM_ATTR LR2021Driver::ClearIrqStatus(const SX12XX_Radio_Number_t radioNumber)
{
    constexpr uint8_t status[] {
        0xFF,
        0xFF,
        0xFF,
        0xFF,
    };
    CHECK("LR2021_SYSTEM_CLEAR_IRQ_OC(Clr)", hal.WriteCommand(LR2021_SYSTEM_CLEAR_IRQ_OC, status, sizeof(status), radioNumber));
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
        CHECK("LR2021_RADIO_WRITE_TX_FIFO", hal.WriteCommand(LR2021_RADIO_WRITE_TX_FIFO, outBuffer, PayloadLength, SX12XX_Radio_1));
        codec->encode(outBuffer, dataGemini, PayloadLength);
        CHECK("LR2021_RADIO_WRITE_TX_FIFO", hal.WriteCommand(LR2021_RADIO_WRITE_TX_FIFO, outBuffer, PayloadLength, SX12XX_Radio_2));
    }
    else
    {
        CHECK("LR2021_RADIO_WRITE_TX_FIFO", hal.WriteCommand(LR2021_RADIO_WRITE_TX_FIFO, outBuffer, PayloadLength, radioNumber));
    }
    SetMode(LR2021_MODE_TX, radioNumber);
#ifdef DEBUG_LLCC68_OTA_TIMING
    beginTX = micros();
#endif
}

void ICACHE_RAM_ATTR LR2021Driver::DecodeRssiSnr(const SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[8] {};
    if (isGFSKModulation(modulation))
    {
        CHECK("LR2021_RADIO_GET_FSK_PACKET_STATUS_OC", hal.WriteCommand(LR2021_RADIO_GET_FSK_PACKET_STATUS_OC, radioNumber));
        CHECK("LR2021_RADIO_GET_FSK_PACKET_STATUS_OC", hal.ReadCommand(buf, 8, radioNumber));
    }
    else if (isFLRCModulation(modulation))
    {
        CHECK("LR2021_RADIO_GET_FLRC_PACKET_STATUS_OC", hal.WriteCommand(LR2021_RADIO_GET_FLRC_PACKET_STATUS_OC, radioNumber));
        CHECK("LR2021_RADIO_GET_FLRC_PACKET_STATUS_OC", hal.ReadCommand(buf, 7, radioNumber));
    }
    else
    {
        CHECK("LR2021_RADIO_GET_LORA_PACKET_STATUS_OC", hal.WriteCommand(LR2021_RADIO_GET_LORA_PACKET_STATUS_OC, radioNumber));
        CHECK("LR2021_RADIO_GET_LORA_PACKET_STATUS_OC", hal.ReadCommand(buf, 8, radioNumber));
    }

    // RssiPkt defines the average RSSI over the last packet received. RSSI value in dBm is –RssiPkt
    const int8_t rssi = -static_cast<int8_t>(buf[isLoRaModulation(modulation) ? 6 : 4]);

    // If radio # is 0, update LastPacketRSSI, otherwise LastPacketRSSI2
    radioNumber == SX12XX_Radio_1 ? LastPacketRSSI = rssi : LastPacketRSSI2 = rssi;

    // Update whatever SNRs we have
    LastPacketSNRRaw = isLoRaModulation(modulation) ? static_cast<int8_t>(buf[4]) : 0;

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
    return -static_cast<int8_t>(status[2]); // status[3] contains the bottom bit of if 0.5dB so we ignore it
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
    DecodeRssiSnr(processingPacketRadio);
#if defined(DEBUG_RCVR_SIGNAL_STATS)
    irq_count_or++;
#endif

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        // when both radio got the packet, use the better RSSI one
        if (hasSecondRadioGotData)
        {
            const int8_t firstSNR = LastPacketSNRRaw;
            DecodeRssiSnr(radioNumber);
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
