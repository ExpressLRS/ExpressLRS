#include "LR1121_Regs.h"
#include "LR1121_hal.h"
#include "LR1121.h"
#include "logging.h"
#include <math.h>

LR1121Hal hal;
LR1121Driver *LR1121Driver::instance = NULL;

//DEBUG_LR1121_OTA_TIMING

#if defined(DEBUG_LR1121_OTA_TIMING)
static uint32_t beginTX;
static uint32_t endTX;
#endif

// RxTimeout is expressed in periods of the 32.768kHz RTC
#define RX_TIMEOUT_PERIOD_BASE_NANOS 1000000000 / 32768 // TODO check for LR1121

#ifdef USE_HARDWARE_DCDC
    #ifndef OPT_USE_HARDWARE_DCDC
        #define OPT_USE_HARDWARE_DCDC true
    #endif
#else
    #define OPT_USE_HARDWARE_DCDC false
#endif

// This define refers to the High Frequency output on the SX1276.  But has been reused/repruposed for the LR1121.
// In ELRS V4 it should be changed to USE_RADIO_RFO_LP and refer to using the Low Power radio frequency output
// of both the SX1276 and LR1121.
#ifdef USE_SX1276_RFO_HF
  #ifndef OPT_USE_SX1276_RFO_HF
    #define OPT_USE_SX1276_RFO_HF true
  #endif
#else
  #define OPT_USE_SX1276_RFO_HF false
#endif

LR1121Driver::LR1121Driver(): SX12xxDriverCommon()
{
    useFSK = false;
    instance = this;
    timeout = 0xFFFFFF;
    lastSuccessfulPacketRadio = SX12XX_Radio_1;
    fallBackMode = LR1121_MODE_FS;
    useFEC = false;
}

void LR1121Driver::End()
{
    SetMode(LR1121_MODE_SLEEP, SX12XX_Radio_All);
    hal.end();
    RemoveCallbacks();
}

bool LR1121Driver::Begin(uint32_t minimumFrequency, uint32_t maximumFrequency)
{
    hal.init();
    hal.IsrCallback_1 = &LR1121Driver::IsrCallback_1;
    hal.IsrCallback_2 = &LR1121Driver::IsrCallback_2;

    hal.reset();

    // Validate that the LR1121 is working.
    uint8_t version[5] = {0};
    hal.WriteCommand(LR11XX_SYSTEM_GET_VERSION_OC, SX12XX_Radio_1);
    hal.ReadCommand(version, sizeof(version), SX12XX_Radio_1);

    DBGLN("Read LR1121 #1 Use Case (0x03 = LR1121): %d", version[2]);
    if (version[2] != 0x03)
    {
    DBGLN("LR1121 #1 failed to be detected.");
        return false;
    }
    DBGLN("LR1121 #1 Ready");

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        // Validate that the LR1121 #2 is working.
        memset(version, 0, sizeof(version));
        hal.WriteCommand(LR11XX_SYSTEM_GET_VERSION_OC, SX12XX_Radio_2);
        hal.ReadCommand(version, sizeof(version), SX12XX_Radio_2);

        DBGLN("Read LR1121 #2 Use Case (0x03 = LR1121): %d", version[2]);
        if (version[2] != 0x03)
        {
        DBGLN("LR1121 #2 failed to be detected.");
            return false;
        }
        DBGLN("LR1121 #2 Ready");
    }

    //Clear Errors
    hal.WriteCommand(LR11XX_SYSTEM_CLEAR_ERRORS_OC, SX12XX_Radio_All); // Remove later?  Might not be required???

/*
Do not enable for dual radio TX.
When AUTOFS is set and tlm received by only 1 of the 2 radios,  that radio will go into FS mode and the other
into Standby mode.  After the following SPI command for tx mode, busy will go high for differing periods of time because 1 is
transitioning from FS mode and the other from Standby mode. This causes the tx done dio of the 2 radios to occur at very different times.
*/
    // 7.2.5 SetRxTxFallbackMode
    uint8_t FBbuf[1] = {LR11XX_RADIO_FALLBACK_FS};
    fallBackMode = LR1121_MODE_FS;
    hal.WriteCommand(LR11XX_RADIO_SET_RX_TX_FALLBACK_MODE_OC, FBbuf, sizeof(FBbuf), SX12XX_Radio_All);

    // 7.2.12 SetRxBoosted
    uint8_t abuf[1] = {1};
    hal.WriteCommand(LR11XX_RADIO_SET_RX_BOOSTED_OC, abuf, sizeof(abuf), SX12XX_Radio_All);
    
    SetDioAsRfSwitch();
    SetDioIrqParams();

#if defined(USE_HARDWARE_DCDC)
    if (OPT_USE_HARDWARE_DCDC)
    {
        // 5.1.1 SetRegMode
        uint8_t RegMode[1] = {1};    
        hal.WriteCommand(LR11XX_SYSTEM_SET_REGMODE_OC, RegMode, sizeof(RegMode), SX12XX_Radio_All); // Enable DCDC converter instead of LDO
    }
#endif

    // 2.1.3.1 CalibImage
    uint8_t CalImagebuf[2];
    CalImagebuf[0] = ((minimumFrequency / 1000000 ) - 1) / 4;       // Freq1 = floor( (fmin_mhz - 1)/4)
    CalImagebuf[1] = 1 + ((maximumFrequency / 1000000 ) + 1) / 4;   // Freq2 = ceil( (fmax_mhz + 1)/4)
    hal.WriteCommand(LR11XX_SYSTEM_CALIBRATE_IMAGE_OC, CalImagebuf, sizeof(CalImagebuf), SX12XX_Radio_All);

    return true;
}

// 12.2.1 SetTxCw
void LR1121Driver::startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    // Set a basic Config that can be used for both 2.4G and SubGHz bands.
    Config(LR11XX_RADIO_LORA_BW_62, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_4_8, freq, 12, false, 8, 0, false, 0, 0, radioNumber);
    CommitOutputPower();
    hal.WriteCommand(LR11XX_RADIO_SET_TX_CW_OC, radioNumber);
}

void LR1121Driver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t regfreq,
                          uint8_t PreambleLength, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval,
                          bool setFSKModulation, uint8_t fskSyncWord1, uint8_t fskSyncWord2,
                          SX12XX_Radio_Number_t radioNumber)
{
    PayloadLength = _PayloadLength;
    
    bool isSubGHz = regfreq < 1000000000;

    if (radioNumber & SX12XX_Radio_1)
        radio1isSubGHz = isSubGHz;
    
    if (radioNumber & SX12XX_Radio_2)
        radio2isSubGHz = isSubGHz;

    IQinverted = InvertIQ ? LR11XX_RADIO_LORA_IQ_INVERTED : LR11XX_RADIO_LORA_IQ_STANDARD;
    // IQinverted is always STANDARD for 900 and SX1276
    if (isSubGHz)
    {
        IQinverted = LR11XX_RADIO_LORA_IQ_STANDARD;
    }

    SetRxTimeoutUs(interval);

    SetMode(LR1121_MODE_STDBY_RC, radioNumber);

    useFSK = setFSKModulation;
    
    // 8.1.1 SetPacketType
    uint8_t buf[1] = {useFSK ? LR11XX_RADIO_PKT_TYPE_GFSK : LR11XX_RADIO_PKT_TYPE_LORA};
    hal.WriteCommand(LR11XX_RADIO_SET_PKT_TYPE_OC, buf, sizeof(buf), radioNumber);

    useFEC = false;
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
            useFEC = true;
            PayloadLength = 14;
        }

        SetPacketParamsFSK(PreambleLength, PayloadLength, radioNumber);
        SetFSKSyncWord(fskSyncWord1, fskSyncWord2, radioNumber);
    }
    else
    {
        DBGLN("Config LoRa");
        ConfigModParamsLoRa(bw, sf, cr, radioNumber);

    #if defined(DEBUG_FREQ_CORRECTION) // TODO Check if this available with the LR1121?
        lr11xx_RadioLoRaPacketLengthsModes_t packetLengthType = LR1121_LORA_PACKET_VARIABLE_LENGTH;
    #else
        lr11xx_RadioLoRaPacketLengthsModes_t packetLengthType = LR1121_LORA_PACKET_FIXED_LENGTH;
    #endif

        SetPacketParamsLoRa(PreambleLength, packetLengthType, PayloadLength, IQinverted, radioNumber);
    }

    SetFrequencyHz(regfreq, radioNumber);

    pwrForceUpdate = true; // Must be called after changing rf modes between subG and 2.4G.  This sets the correct rf amps, and txen pins to be used.
    
    ClearIrqStatus(radioNumber);
}

void LR1121Driver::ConfigModParamsFSK(uint32_t Bitrate, uint8_t BWF, uint32_t Fdev, SX12XX_Radio_Number_t radioNumber)
{
    // 8.5.1 SetModulationParams
    uint8_t buf[10];
    buf[0] = Bitrate >> 24;
    buf[1] = Bitrate >> 16;
    buf[2] = Bitrate >> 8;
    buf[3] = Bitrate >> 0;
    buf[4] = LR11XX_RADIO_GFSK_PULSE_SHAPE_OFF;             // Pulse Shape - 0x00: No filter applied
    buf[5] = BWF;
    buf[6] = Fdev >> 24;
    buf[7] = Fdev >> 16;
    buf[8] = Fdev >> 8;
    buf[9] = Fdev >> 0;
    hal.WriteCommand(LR11XX_RADIO_SET_MODULATION_PARAM_OC, buf, sizeof(buf), radioNumber);    
}

void LR1121Driver::SetPacketParamsFSK(uint8_t PreambleLength, uint8_t PayloadLength, SX12XX_Radio_Number_t radioNumber)
{
    // 8.5.2 SetPacketParams
    uint8_t buf[9];
    buf[0] = 0;                                             // MSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[1] = PreambleLength;                                // LSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[2] = LR11XX_RADIO_GFSK_PREAMBLE_DETECTOR_MIN_8BITS; // Pbl Detect - 0x04: Preamble detector length 8 bits
    buf[3] = 16;                                            // SyncWordLen defines the length of the Syncword in bits. By default, the Syncword is set to 0x9723522556536564
    buf[4] = LR11XX_RADIO_GFSK_ADDRESS_FILTERING_DISABLE;   // Addr Comp - 0x00: Address Filtering Disabled
    buf[5] = LR11XX_RADIO_GFSK_PKT_FIX_LEN;                 // PacketType - 0x00: Packet length is known on both sides
    buf[6] = PayloadLength;                                 // PayloadLen
    buf[7] = LR11XX_RADIO_GFSK_CRC_OFF;                     // CrcType - 0x01: CRC_OFF (No CRC).
    buf[8] = LR11XX_RADIO_GFSK_DC_FREE_WHITENING;           // DcFree - 0x01: SX127x/SX126x/LR11xx compatible whitening enable. 0x03: SX128x compatible whitening enable
    hal.WriteCommand(LR11XX_RADIO_SET_PKT_PARAM_OC, buf, sizeof(buf), radioNumber);
}

void LR1121Driver::SetFSKSyncWord(uint8_t fskSyncWord1, uint8_t fskSyncWord2, SX12XX_Radio_Number_t radioNumber)
{
    // 8.5.3 SetGfskSyncWord
    // SyncWordLen is 16 bits as set in SetPacketParamsFSK().  Fill the rest with preamble bytes.
    uint8_t synbuf[8] = {fskSyncWord1, fskSyncWord2, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
    hal.WriteCommand(LR11XX_RADIO_SET_GFSK_SYNC_WORD_OC, synbuf, sizeof(synbuf), radioNumber);
}

void LR1121Driver::SetDioAsRfSwitch()
{
    // 4.2.1 SetDioAsRfSwitch
    uint8_t switchbuf[8];
    if (LR1121_RFSW_CTRL_COUNT == 8)
    {
        switchbuf[0] = LR1121_RFSW_CTRL[0]; // RfswEnable
        switchbuf[1] = LR1121_RFSW_CTRL[1]; // RfSwStbyCfg
        switchbuf[2] = LR1121_RFSW_CTRL[2]; // RfSwRxCfg
        switchbuf[3] = LR1121_RFSW_CTRL[3]; // RfSwTxCfg
        switchbuf[4] = LR1121_RFSW_CTRL[4]; // RfSwTxHPCfg
        switchbuf[5] = LR1121_RFSW_CTRL[5]; // RfSwTxHfCfg
        switchbuf[6] = LR1121_RFSW_CTRL[6]; // Unused
        switchbuf[7] = LR1121_RFSW_CTRL[7]; // RfSwWifiCfg - Each bit indicates the state of the relevant RFSW DIO when in Wi-Fi scanning mode or high frequency RX mode (LR1110_H1_UM_V1-7-1.pdf)
    }
    else
    {
        switchbuf[0] = 0b00001111; // RfswEnable
        switchbuf[1] = 0b00000000; // RfSwStbyCfg
        switchbuf[2] = 0b00000100; // RfSwRxCfg
        switchbuf[3] = 0b00001000; // RfSwTxCfg
        switchbuf[4] = 0b00001000; // RfSwTxHPCfg
        switchbuf[5] = 0b00000010; // RfSwTxHfCfg
        switchbuf[6] = 0;          // Unused
        switchbuf[7] = 0b00000001; // RfSwWifiCfg - Each bit indicates the state of the relevant RFSW DIO when in Wi-Fi scanning mode or high frequency RX mode (LR1110_H1_UM_V1-7-1.pdf)
    }
    hal.WriteCommand(LR11XX_SYSTEM_SET_DIO_AS_RF_SWITCH_OC, switchbuf, sizeof(switchbuf), SX12XX_Radio_All);
}

void LR1121Driver::SetRxTimeoutUs(uint32_t interval)
{
    timeout = 0xFFFFFF; // no timeout, continuous mode
    if (interval)
    {
        timeout = interval * 1000 / RX_TIMEOUT_PERIOD_BASE_NANOS;
    }
}

void LR1121Driver::CorrectRegisterForSF6(uint8_t sf, SX12XX_Radio_Number_t radioNumber)
{
    // 8.3.1 SetModulationParams
    // - SF6 can be made compatible with the SX127x family in implicit mode via a register setting1.
    // - Set bit 18 of register at address 0xf20414 to 1
    // - Set bit 23 of register at address 0xf20414 to 0.  This information is from Semecth in an email.
    // 3.7.3 WriteRegMemMask32

    if ((lr11xx_radio_lora_sf_t)sf == LR11XX_RADIO_LORA_SF6)
    {
        uint8_t wrbuf[12];
        // Address
        wrbuf[0] = 0x00; // MSB
        wrbuf[1] = 0xf2;
        wrbuf[2] = 0x04;
        wrbuf[3] = 0x14;
        // Mask
        wrbuf[4] = 0x00; // MSB
        wrbuf[5] = 0b10000100; // bit18=1 and bit23=0
        wrbuf[6] = 0x00;
        wrbuf[7] = 0x00; 
        // Data
        wrbuf[8] = 0x00; // MSB
        wrbuf[9] = 0b00000100; // bit18=1 and bit23=0
        wrbuf[10] = 0x00;
        wrbuf[11] = 0x00;
        hal.WriteCommand(LR11XX_REGMEM_WRITE_REGMEM32_MASK_OC, wrbuf, sizeof(wrbuf), radioNumber);
    }
}

/***
 * @brief: Schedule an output power change after the next transmit
 ***/
void LR1121Driver::SetOutputPower(int8_t power, bool isSubGHz)
{
    uint8_t pwrNew;

    if (isSubGHz)
    {
        if (OPT_USE_SX1276_RFO_HF)
        {
            pwrNew = constrain(power, LR1121_POWER_MIN_LP_PA, LR1121_POWER_MAX_LP_PA);
        }
        else
        {
            pwrNew = constrain(power, LR1121_POWER_MIN_HP_PA, LR1121_POWER_MAX_HP_PA);
        }

        if ((pwrPendingLF == PWRPENDING_NONE && pwrCurrentLF != pwrNew) || pwrPendingLF != pwrNew)
        {
            pwrPendingLF = pwrNew;
        }
    }
    else
    {
        pwrNew = constrain(power, LR1121_POWER_MIN_HF_PA, LR1121_POWER_MAX_HF_PA);

        if ((pwrPendingHF == PWRPENDING_NONE && pwrCurrentHF != pwrNew) || pwrPendingHF != pwrNew)
        {
            pwrPendingHF = pwrNew;
        }
    }
}

void ICACHE_RAM_ATTR LR1121Driver::CommitOutputPower()
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

void ICACHE_RAM_ATTR LR1121Driver::WriteOutputPower(uint8_t power, bool isSubGHz, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t Pabuf[4] = {0};
    uint8_t Txbuf[2] = {power, LR11XX_RADIO_RAMP_48_US};

    // 9.5.1 SetPaConfig
    // 9.5.2 SetTxParams
    if (isSubGHz)
    {
        // 900M low power RF Amp
        // Table 9-1: Optimized Settings for LP PA with Same Matching Network
        // -17dBm (0xEF) to +14dBm (0x0E) by steps of 1dB if the low power PA is selected
        if (OPT_USE_SX1276_RFO_HF)
        {
            Pabuf[0] = LR11XX_RADIO_PA_SEL_LP; // PaSel - 0x01: Selects the high power PA
            Pabuf[1] = LR11XX_RADIO_PA_REG_SUPPLY_VREG; // RegPaSupply - 0x01: Powers the PA from VBAT. The user must use RegPaSupply = 0x01 whenever TxPower > 14
            Pabuf[2] = 0x07; // PaDutyCycle
        }
        // 900M high power RF Amp
        // Table 9-2: Optimized Settings for HP PA with Same Matching Network
        // -9dBm (0xF7) to +22dBm (0x16) by steps of 1dB if the high power PA is selected
        else
        {
            Pabuf[0] = LR11XX_RADIO_PA_SEL_HP; // PaSel - 0x01: Selects the high power PA
            Pabuf[1] = LR11XX_RADIO_PA_REG_SUPPLY_VBAT; // RegPaSupply - 0x01: Powers the PA from VBAT. The user must use RegPaSupply = 0x01 whenever TxPower > 14
            Pabuf[2] = 0x04; // PaDutyCycle
            Pabuf[3] = 0x07; // PaHPSel - In order to reach +22dBm output power, PaHPSel must be set to 7. PaHPSel has no impact on either the low power PA or the high frequency PA.
        }
    }
    // 2.4G RF Amp
    // Table 9-3: Optimized Settings for HF PA with the Same Matching Network
    // -18dBm (0xEE) to +13dBm (0x0D) by steps of 1dB if the high frequency PA is selected
    else
    {
        Pabuf[0] = LR11XX_RADIO_PA_SEL_HF; // PaSel - 0x02: Selects the high frequency PA
        Pabuf[1] = LR11XX_RADIO_PA_REG_SUPPLY_VREG; // RegPaSupply - 0x01: Powers the PA from VBAT. The user must use RegPaSupply = 0x01 whenever TxPower > 14
        Pabuf[2] = 0x00; // PaDutyCycle
        Pabuf[3] = 0x00; // PaHPSel - In order to reach +22dBm output power, PaHPSel must be set to 7. PaHPSel has no impact on either the low power PA or the high frequency PA.
    }

    hal.WriteCommand(LR11XX_RADIO_SET_PA_CFG_OC, Pabuf, sizeof(Pabuf), radioNumber);
    hal.WriteCommand(LR11XX_RADIO_SET_TX_PARAMS_OC, Txbuf, sizeof(Txbuf), radioNumber);
}

void LR1121Driver::SetMode(lr11xx_RadioOperatingModes_t OPmode, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[5] = {0};
    switch (OPmode)
    {
    case LR1121_MODE_SLEEP:
        // 2.1.5.1 SetSleep
        hal.WriteCommand(LR11XX_SYSTEM_SET_SLEEP_OC, buf, 5, radioNumber);
        break;

    case LR1121_MODE_STDBY_RC:
        //2.1.2.1 SetStandby
        buf[0] = 0x00;
        hal.WriteCommand(LR11XX_SYSTEM_SET_STANDBY_OC, buf, 1, radioNumber);
        break;

    case LR1121_MODE_STDBY_XOSC:
        //2.1.2.1 SetStandby
        buf[0] = 0x01;
        hal.WriteCommand(LR11XX_SYSTEM_SET_STANDBY_OC, buf, 1, radioNumber);
        break;

    case LR1121_MODE_FS:
        // 2.1.9.1 SetFs
        hal.WriteCommand(LR11XX_SYSTEM_SET_FS_OC, radioNumber);
        break;

    case LR1121_MODE_RX:
        // 7.2.2 SetRx
        buf[0] = timeout >> 16;
        buf[1] = timeout >> 8;
        buf[2] = timeout & 0xFF;
        hal.WriteCommand(LR11XX_RADIO_SET_RX_OC, buf, 3, radioNumber);
        break;

    case LR1121_MODE_RX_CONT:
        // 7.2.2 SetRx
        buf[0] = 0xFF;
        buf[1] = 0xFF;
        buf[2] = 0xFF;
        hal.WriteCommand(LR11XX_RADIO_SET_RX_OC, buf, 3, radioNumber);
        break;

    case LR1121_MODE_TX:
        // Table 7-3: SetTx Command
        hal.WriteCommand(LR11XX_RADIO_SET_TX_OC, buf, 3, radioNumber);
        break;

    case LR1121_MODE_CAD:
        break;

    default:
        break;
    }
}

void LR1121Driver::ConfigModParamsLoRa(uint8_t bw, uint8_t sf, uint8_t cr, SX12XX_Radio_Number_t radioNumber)
{
    // 8.3.1 SetModulationParams
    uint8_t buf[4];
    buf[0] = sf;
    buf[1] = bw;
    buf[2] = cr;
    buf[3] = 0x00; // 0x00: LowDataRateOptimize off
    hal.WriteCommand(LR11XX_RADIO_SET_MODULATION_PARAM_OC, buf, sizeof(buf), radioNumber);    

    if (radioNumber & SX12XX_Radio_1 && radio1isSubGHz)
        CorrectRegisterForSF6(sf, SX12XX_Radio_1);

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        if (radioNumber & SX12XX_Radio_2 && radio2isSubGHz)
            CorrectRegisterForSF6(sf, SX12XX_Radio_2);
    }
}

void LR1121Driver::SetPacketParamsLoRa(uint8_t PreambleLength, lr11xx_RadioLoRaPacketLengthsModes_t HeaderType,
                                       uint8_t PayloadLength, uint8_t InvertIQ, SX12XX_Radio_Number_t radioNumber)
{
    // 8.3.2 SetPacketParams
    uint8_t buf[6];
    buf[0] = 0; // MSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[1] = PreambleLength; // LSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[2] = HeaderType; // HeaderType defines if the header is explicit or implicit
    buf[3] = PayloadLength; // PayloadLen defines the size of the payload
    buf[4] = LR11XX_RADIO_LORA_CRC_OFF;
    buf[5] = InvertIQ;
    hal.WriteCommand(LR11XX_RADIO_SET_PKT_PARAM_OC, buf, sizeof(buf), radioNumber);
}

void ICACHE_RAM_ATTR LR1121Driver::SetFrequencyHz(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    // 7.2.1 SetRfFrequency
    uint8_t buf[4];
    buf[0] = freq >> 24;
    buf[1] = freq >> 16;
    buf[2] = freq >> 8;
    buf[3] = freq & 0xFF;
    hal.WriteCommand(LR11XX_RADIO_SET_RF_FREQUENCY_OC, buf, sizeof(buf), radioNumber);

    currFreq = freq;
}

void ICACHE_RAM_ATTR LR1121Driver::SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    SetFrequencyHz(freq, radioNumber);
}

// 4.1.1 SetDioIrqParams
void LR1121Driver::SetDioIrqParams()
{
    uint8_t buf[8] = {0};
    buf[3] = LR1121_IRQ_TX_DONE | LR1121_IRQ_RX_DONE;
    hal.WriteCommand(LR11XX_SYSTEM_SET_DIOIRQPARAMS_OC, buf, sizeof(buf), SX12XX_Radio_All);
}

// 3.4.1 GetStatus
uint32_t ICACHE_RAM_ATTR LR1121Driver::GetIrqStatus(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status[6] = {0};
    status[0] = LR11XX_SYSTEM_CLEAR_IRQ_OC >> 8;
    status[1] = LR11XX_SYSTEM_CLEAR_IRQ_OC & 0xFF;
    status[2] = 0xFF;
    status[3] = 0xFF;
    status[4] = 0xFF;
    status[5] = 0xFF;
    hal.ReadCommand(status, sizeof(status), radioNumber);
    return status[2] << 24 | status[3] << 16 | status[4] << 8 | status[5];
}

void ICACHE_RAM_ATTR LR1121Driver::ClearIrqStatus(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t buf[4];
    buf[0] = 0xFF;
    buf[1] = 0xFF;
    buf[2] = 0xFF;
    buf[3] = 0xFF;
    hal.WriteCommand(LR11XX_SYSTEM_CLEAR_IRQ_OC, buf, sizeof(buf), radioNumber);
}

void ICACHE_RAM_ATTR LR1121Driver::TXnbISR()
{
#ifdef DEBUG_LR1121_OTA_TIMING
    endTX = micros();
    DBGLN("TOA: %d", endTX - beginTX);
#endif
    CommitOutputPower();
    TXdoneCallback();
}

void ICACHE_RAM_ATTR LR1121Driver::TXnb(uint8_t * data, uint8_t size, SX12XX_Radio_Number_t radioNumber)
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
        instance->rxSignalStats[0].telem_count++;
    }
    if (radioNumber == SX12XX_Radio_All || radioNumber == SX12XX_Radio_2)
    {
        instance->rxSignalStats[1].telem_count++;
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

    if (useFEC)
    {
        uint8_t FECBuffer[PayloadLength] = {0};
        FECEncode(data, FECBuffer);

        // 3.7.4 WriteBuffer8
        hal.WriteCommand(LR11XX_REGMEM_WRITE_BUFFER8_OC, FECBuffer, PayloadLength, radioNumber);
    }
    else
    {
        // 3.7.4 WriteBuffer8
        hal.WriteCommand(LR11XX_REGMEM_WRITE_BUFFER8_OC, data, size, radioNumber);
    }

    SetMode(LR1121_MODE_TX, radioNumber);

#ifdef DEBUG_LLCC68_OTA_TIMING
    beginTX = micros();
#endif
}

bool ICACHE_RAM_ATTR LR1121Driver::RXnbISR(SX12XX_Radio_Number_t radioNumber)
{
    // 7.2.11 GetRxBufferStatus
    uint8_t buf[3] = {0};
    hal.WriteCommand(LR11XX_RADIO_GET_RXBUFFER_STATUS_OC, radioNumber);
    hal.ReadCommand(buf, sizeof(buf), radioNumber);

    uint8_t const PayloadLengthRX = buf[1];
    uint8_t const RxStartBufferPointer = buf[2];

    // 3.7.5 ReadBuffer8
    uint8_t inbuf[2];
    inbuf[0] = RxStartBufferPointer;
    inbuf[1] = PayloadLengthRX;

    uint8_t payloadbuf[PayloadLengthRX + 1] = {0};
    hal.WriteCommand(LR11XX_REGMEM_READ_BUFFER8_OC, inbuf, sizeof(inbuf), radioNumber);
    hal.ReadCommand(payloadbuf, sizeof(payloadbuf), radioNumber);

    if (useFEC)
    {
        FECDecode(payloadbuf + 1, RXdataBuffer);
    }
    else
    {
        memcpy(RXdataBuffer, payloadbuf + 1, PayloadLengthRX);
    }

    return RXdoneCallback(SX12XX_RX_OK);
}

void ICACHE_RAM_ATTR LR1121Driver::RXnb(lr11xx_RadioOperatingModes_t rxMode)
{
    SetMode(LR1121_MODE_RX, SX12XX_Radio_All);
}

bool ICACHE_RAM_ATTR LR1121Driver::GetFrequencyErrorbool()
{
    return false;
}

// 7.2.8 GetRssiInst
int8_t ICACHE_RAM_ATTR LR1121Driver::GetRssiInst(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status[2] = {0};
    hal.WriteCommand(LR11XX_RADIO_GET_RSSI_INST_OC, radioNumber);
    hal.ReadCommand(status, sizeof(status), radioNumber);
    return -(int8_t)(status[1] / 2);
}

void ICACHE_RAM_ATTR LR1121Driver::GetLastPacketStats()
{
    SX12XX_Radio_Number_t radio[2] = {SX12XX_Radio_1, SX12XX_Radio_2};
    bool gotRadio[2] = {false, false}; // one-radio default.
    uint8_t processingRadioIdx = (instance->processingPacketRadio == SX12XX_Radio_1) ? 0 : 1;
    uint8_t secondRadioIdx = !processingRadioIdx;

    // processingRadio always passed the sanity check here
    gotRadio[processingRadioIdx] = true;

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        bool isSecondRadioGotData = false;

        uint32_t secondIrqStatus = instance->GetIrqStatus(radio[secondRadioIdx]);
        if(secondIrqStatus & LR1121_IRQ_RX_DONE)
        {
            // 7.2.11 GetRxBufferStatus
            uint8_t buf[3] = {0};
            hal.WriteCommand(LR11XX_RADIO_GET_RXBUFFER_STATUS_OC, radio[secondRadioIdx]);
            hal.ReadCommand(buf, sizeof(buf), radio[secondRadioIdx]);

            uint8_t const PayloadLengthRX = buf[1];
            uint8_t const RxStartBufferPointer = buf[2];

            // 3.7.5 ReadBuffer8
            uint8_t inbuf[2];
            inbuf[0] = RxStartBufferPointer;
            inbuf[1] = PayloadLengthRX;

            WORD_ALIGNED_ATTR uint8_t RXdataBuffer_second[PayloadLengthRX + 1] = {0};

            hal.WriteCommand(LR11XX_REGMEM_READ_BUFFER8_OC, inbuf, sizeof(inbuf), radio[secondRadioIdx]);
            hal.ReadCommand(RXdataBuffer_second, sizeof(RXdataBuffer_second), radio[secondRadioIdx]);

            if (useFEC)
            {
                uint8_t decodedRXdataBuffer_second[8];
                FECDecode(RXdataBuffer_second + 1, decodedRXdataBuffer_second);
                // if the second packet is same to the first, it's valid
                if(memcmp(RXdataBuffer, decodedRXdataBuffer_second, 8) == 0)
                {
                    isSecondRadioGotData = true;
                }
            }
            else
            {
                // if the second packet is same to the first, it's valid
                if(memcmp(RXdataBuffer, RXdataBuffer_second + 1, PayloadLength) == 0)
                {
                    isSecondRadioGotData = true;
                }
            }
        }

        // second radio received the same packet to the processing radio
        gotRadio[secondRadioIdx] = isSecondRadioGotData;
        #if defined(DEBUG_RCVR_SIGNAL_STATS)
        if(!isSecondRadioGotData)
        {
            instance->rxSignalStats[secondRadioIdx].fail_count++;
        }
        #endif
    }

    uint8_t status[3];
    int8_t rssi[2];
    int8_t snr[2];

    // Get both radios ready at the same time to return packet stats
    hal.WriteCommand(LR11XX_RADIO_GET_PKT_STATUS_OC, instance->processingPacketRadio | (gotRadio[secondRadioIdx] ? radio[secondRadioIdx] : 0));

    for(uint8_t i=0; i<2; i++)
    {
        if (gotRadio[i])
        {
            // 8.3.7 GetPacketStatus (LoRa)
            // 8.5.7 GetPacketStatus (FSK)
            memset(status, 0, sizeof(status));
            hal.ReadCommand(status, sizeof(status), radio[i]);
            
            // RssiPkt defines the average RSSI over the last packet received. RSSI value in dBm is –RssiPkt/2.
            rssi[i] = -(int8_t)(status[useFSK ? 2 : 1] / 2);

            // SignalRssiPkt is an estimation of RSSI of the LoRa signal (after despreading) on last packet received, in two’s
            // complement format [negated, dBm, fixdt(0,8,1)]. Actual RSSI in dB is -SignalRssiPkt/2.
            // rssi[i = -(int8_t)(status[3] / 2); // SignalRssiPkt

            snr[i] = useFSK ? 0 : (int8_t)status[2];

            // If radio # is 0, update LastPacketRSSI, otherwise LastPacketRSSI2
            (i == 0) ? LastPacketRSSI = rssi[i] : LastPacketRSSI2 = rssi[i];
            // Update whatever SNRs we have
            LastPacketSNRRaw = snr[i];
        }
    }

    // by default, set the last successful packet radio to be the current processing radio (which got a successful packet)
    instance->lastSuccessfulPacketRadio = instance->processingPacketRadio;

    // when both radio got the packet, use the better RSSI one
    if(gotRadio[0] && gotRadio[1])
    {
        LastPacketSNRRaw = instance->fuzzy_snr(snr[0], snr[1], instance->FuzzySNRThreshold);
        // Update the last successful packet radio to be the one with better signal strength
        instance->lastSuccessfulPacketRadio = (rssi[0]>rssi[1])? radio[0]: radio[1];
    }

#if defined(DEBUG_RCVR_SIGNAL_STATS)
    // stat updates
    for (uint8_t i = 0; i < 2; i++)
    {
        if (gotRadio[i])
        {
            instance->rxSignalStats[i].irq_count++;
            instance->rxSignalStats[i].rssi_sum += rssi[i];
            instance->rxSignalStats[i].snr_sum += snr[i];
            if (snr[i] > instance->rxSignalStats[i].snr_max)
            {
                instance->rxSignalStats[i].snr_max = snr[i];
            }
            LastPacketSNRRaw = snr[i];
        }
    }
    if(gotRadio[0] || gotRadio[1])
    {
        instance->irq_count_or++;
    }
    if(gotRadio[0] && gotRadio[1])
    {
        instance->irq_count_both++;
    }
#endif
}

void ICACHE_RAM_ATTR LR1121Driver::IsrCallback_1()
{
    if (digitalRead(GPIO_PIN_DIO1))
    {
        instance->IsrCallback(SX12XX_Radio_1);
    }
}

void ICACHE_RAM_ATTR LR1121Driver::IsrCallback_2()
{
    if (digitalRead(GPIO_PIN_DIO1_2))
    {
        instance->IsrCallback(SX12XX_Radio_2);
    }
}

void ICACHE_RAM_ATTR LR1121Driver::IsrCallback(SX12XX_Radio_Number_t radioNumber)
{
    instance->processingPacketRadio = radioNumber;

    uint32_t irqStatus = instance->GetIrqStatus(radioNumber);
    if (irqStatus & LR1121_IRQ_TX_DONE)
    {
        instance->TXnbISR();
        instance->ClearIrqStatus(SX12XX_Radio_All);
    }
    else if (irqStatus & LR1121_IRQ_RX_DONE)
    {
        if (instance->RXnbISR(radioNumber))
        {
        }
#if defined(DEBUG_RCVR_SIGNAL_STATS)
        else
        {
            instance->rxSignalStats[(radioNumber == SX12XX_Radio_1) ? 0 : 1].fail_count++;
        }
#endif
    }
}
