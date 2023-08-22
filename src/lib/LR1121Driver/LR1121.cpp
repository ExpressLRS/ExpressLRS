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

#ifdef USE_LR1121_DCDC
    #ifndef OPT_USE_LR1121_DCDC
        #define OPT_USE_LR1121_DCDC true
    #endif
#else
    #define OPT_USE_LR1121_DCDC false
#endif

LR1121Driver::LR1121Driver(): SX12xxDriverCommon()
{
    instance = this;
    timeout = 0xFFFFFF;
    lastSuccessfulPacketRadio = SX12XX_Radio_1;
    subGRF = false;
    fallBackMode = LR1121_MODE_STDBY_XOSC;
}

void LR1121Driver::End()
{
    SetMode(LR1121_MODE_SLEEP, SX12XX_Radio_All);
    hal.end();
    RemoveCallbacks();
}

bool LR1121Driver::Begin()
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
#if defined(TARGET_TX)
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        FBbuf[0] = {LR11XX_RADIO_FALLBACK_STDBY_XOSC};
        fallBackMode = LR1121_MODE_STDBY_XOSC;
    }
#endif
    // 7.2.5 SetRxTxFallbackMode
    hal.WriteCommand(LR11XX_RADIO_SET_RX_TX_FALLBACK_MODE_OC, FBbuf, sizeof(FBbuf), SX12XX_Radio_All);

    // 7.2.12 SetRxBoosted
    uint8_t abuf[1] = {1};
    hal.WriteCommand(LR11XX_RADIO_SET_RX_BOOSTED_OC, abuf, sizeof(abuf), SX12XX_Radio_All);

    // 4.2.1 SetDioAsRfSwitch
    // SKY13588-460LF
    uint8_t buf[8];
    buf[0] = 0b00000011; // RfswEnable
    buf[1] = 0; // RfSwStbyCfg
    buf[2] = 0b00000001; // RfSwRxCfg - DIO5 HIGH, DIO6 LOW
    buf[3] = 0b00000011; // RfSwTxCfg
    buf[4] = 0b00000010; // RfSwTxHPCfg
    buf[5] = 0; // RfSwTxHfCfg
    buf[6] = 0; // 
    buf[7] = 0; // 
    hal.WriteCommand(LR11XX_SYSTEM_SET_DIO_AS_RF_SWITCH_OC, buf, sizeof(buf), SX12XX_Radio_All);

    SetDioIrqParams();

    // Force the next power update, and the lowest power
    pwrCurrent = PWRPENDING_NONE;
    SetOutputPower(LR1121_POWER_MIN);
    CommitOutputPower();

#if defined(USE_LR1121_DCDC)
    if (OPT_USE_LR1121_DCDC)
    {
        // hal.WriteCommand(LLCC68_RADIO_SET_REGULATORMODE, LLCC68_USE_DCDC, SX12XX_Radio_All);        // Enable DCDC converter instead of LDO
    }
#endif

    return true;
}

void LR1121Driver::startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    // uint8_t buffer;         // we just need a buffer for the write command
    // SetFrequencyHz(freq, radioNumber);
    // CommitOutputPower();
    // hal.TXenable(radioNumber);
    // hal.WriteCommand(LLCC68_RADIO_SET_TXCONTINUOUSWAVE, &buffer, 0, radioNumber);
}

void LR1121Driver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t regfreq,
                          uint8_t PreambleLength, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval,
                          uint32_t flrcSyncWord, uint16_t flrcCrcSeed, uint8_t flrc)
{
    DBGLN("Config LoRa ");
    PayloadLength = _PayloadLength;
    subGRF = (bw == LR11XX_RADIO_LORA_BW_500);

    IQinverted = InvertIQ ? LR11XX_RADIO_LORA_IQ_INVERTED : LR11XX_RADIO_LORA_IQ_STANDARD;
    // IQinverted is always STANDARD for 900 and SX1276
    if (subGRF)
    {
        IQinverted = LR11XX_RADIO_LORA_IQ_STANDARD;
    }

    SetRxTimeoutUs(interval);

    SetMode(LR1121_MODE_STDBY_RC, SX12XX_Radio_All);

    // 8.1.1 SetPacketType
    uint8_t abuf[1] = {LR11XX_RADIO_PKT_TYPE_LORA};
    hal.WriteCommand(LR11XX_RADIO_SET_PKT_TYPE_OC, abuf, sizeof(abuf), SX12XX_Radio_All);

    ConfigModParamsLoRa(bw, sf, cr);

#if defined(DEBUG_FREQ_CORRECTION) // TODO Check if this available with the LR1121?
    lr11xx_RadioLoRaPacketLengthsModes_t packetLengthType = LR1121_LORA_PACKET_VARIABLE_LENGTH;
#else
    lr11xx_RadioLoRaPacketLengthsModes_t packetLengthType = LR1121_LORA_PACKET_FIXED_LENGTH;
#endif

    SetPacketParamsLoRa(PreambleLength, packetLengthType, _PayloadLength, IQinverted);

    SetFrequencyReg(regfreq, SX12XX_Radio_All);

    // Force power update to set the subG or 2G4 RF amp.
    pwrPending = pwrCurrent;
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

    if ((lr11xx_radio_lora_sf_t)sf == LR11XX_RADIO_LORA_SF6 && subGRF)
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
void LR1121Driver::SetOutputPower(int8_t power)
{
    uint8_t pwrNew = constrain(power, LR1121_POWER_MIN, LR1121_POWER_MAX);

    if ((pwrPending == PWRPENDING_NONE && pwrCurrent != pwrNew) || pwrPending != pwrNew)
    {
        pwrPending = pwrNew;
        DBGLN("SetPower: %u", pwrPending);
    }
}

void ICACHE_RAM_ATTR LR1121Driver::CommitOutputPower()
{
    if (pwrPending == PWRPENDING_NONE)
        return;

    pwrCurrent = pwrPending;
    pwrPending = PWRPENDING_NONE;    

    if (subGRF)
    {
        // 9.5.1 SetPaConfig
        // Table 9-2: Optimized Settings for HP PA with Same Matching Network
        uint8_t Pabuf[4];
        Pabuf[0] = LR11XX_RADIO_PA_SEL_HP; // PaSel - 0x01: Selects the high power PA
        Pabuf[1] = LR11XX_RADIO_PA_REG_SUPPLY_VBAT; // RegPaSupply - 0x01: Powers the PA from VBAT. The user must use RegPaSupply = 0x01 whenever TxPower > 14
        Pabuf[2] = 0x04; // PaDutyCycle
        Pabuf[3] = 0x07; // PaHPSel - In order to reach +22dBm output power, PaHPSel must be set to 7. PaHPSel has no impact on either the low power PA or the high frequency PA.
        hal.WriteCommand(LR11XX_RADIO_SET_PA_CFG_OC, Pabuf, sizeof(Pabuf), SX12XX_Radio_All);

        // 9.5.2 SetTxParams
        uint8_t buf[2] = { 0x16, LR11XX_RADIO_RAMP_48_US };
        hal.WriteCommand(LR11XX_RADIO_SET_TX_PARAMS_OC, buf, sizeof(buf), SX12XX_Radio_All);

        // // 9.5.1 SetPaConfig
        // // Table 9-2: Optimized Settings for HP PA with Same Matching Network
        // uint8_t Pabuf[4];
        // Pabuf[0] = LR11XX_RADIO_PA_SEL_HP; // PaSel - 0x01: Selects the high power PA
        // Pabuf[1] = LR11XX_RADIO_PA_REG_SUPPLY_VBAT; // RegPaSupply - 0x01: Powers the PA from VBAT. The user must use RegPaSupply = 0x01 whenever TxPower > 14
        // Pabuf[2] = 0x01; // PaDutyCycle
        // Pabuf[3] = 0x03; // PaHPSel - In order to reach +22dBm output power, PaHPSel must be set to 7. PaHPSel has no impact on either the low power PA or the high frequency PA.
        // hal.WriteCommand(LR11XX_RADIO_SET_PA_CFG_OC, Pabuf, sizeof(Pabuf), SX12XX_Radio_All);

        // // 9.5.2 SetTxParams
        // uint8_t buf[2] = { pwrCurrent, LR11XX_RADIO_RAMP_48_US };
        // hal.WriteCommand(LR11XX_RADIO_SET_TX_PARAMS_OC, buf, sizeof(buf), SX12XX_Radio_All);
    }
    else
    {
        // 9.5.1 SetPaConfig
        // Table 9-2: Optimized Settings for HP PA with Same Matching Network
        uint8_t Pabuf[4];
        Pabuf[0] = LR11XX_RADIO_PA_SEL_HF; // PaSel - 0x02: Selects the high frequency PA
        Pabuf[1] = LR11XX_RADIO_PA_REG_SUPPLY_VREG; // RegPaSupply - 0x01: Powers the PA from VBAT. The user must use RegPaSupply = 0x01 whenever TxPower > 14
        Pabuf[2] = 0x00; // PaDutyCycle
        Pabuf[3] = 0x00; // PaHPSel - In order to reach +22dBm output power, PaHPSel must be set to 7. PaHPSel has no impact on either the low power PA or the high frequency PA.
        hal.WriteCommand(LR11XX_RADIO_SET_PA_CFG_OC, Pabuf, sizeof(Pabuf), SX12XX_Radio_All);

        // 9.5.2 SetTxParams
        uint8_t buf[2] = { 13, LR11XX_RADIO_RAMP_48_US };
        hal.WriteCommand(LR11XX_RADIO_SET_TX_PARAMS_OC, buf, sizeof(buf), SX12XX_Radio_All);
    }
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

void LR1121Driver::ConfigModParamsLoRa(uint8_t bw, uint8_t sf, uint8_t cr)
{
    // 8.3.1 SetModulationParams
    uint8_t buf[4];
    buf[0] = sf;
    buf[1] = bw;
    buf[2] = cr;
    buf[3] = 0x00; // 0x00: LowDataRateOptimize off
    hal.WriteCommand(LR11XX_RADIO_SET_MODULATION_PARAM_OC, buf, sizeof(buf), SX12XX_Radio_All);    

    CorrectRegisterForSF6(sf, SX12XX_Radio_All);
}

void LR1121Driver::SetPacketParamsLoRa(uint8_t PreambleLength, lr11xx_RadioLoRaPacketLengthsModes_t HeaderType,
                                       uint8_t PayloadLength, uint8_t InvertIQ)
{
    // 8.3.2 SetPacketParams
    uint8_t buf[6];
    buf[0] = 0; // MSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[1] = PreambleLength; // LSB PbLengthTX defines the length of the LoRa packet preamble. Minimum of 12 with SF5 and SF6, and of 8 for other SF advised;
    buf[2] = HeaderType; // HeaderType defines if the header is explicit or implicit
    buf[3] = PayloadLength; // PayloadLen defines the size of the payload
    buf[4] = LR11XX_RADIO_LORA_CRC_OFF;
    buf[5] = InvertIQ;
    hal.WriteCommand(LR11XX_RADIO_SET_PKT_PARAM_OC, buf, sizeof(buf), SX12XX_Radio_All);

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

void LR1121Driver::SetDioIrqParams()
{
    uint8_t buf[8] = {0};
    buf[3] = 0b00001100;
    hal.WriteCommand(LR11XX_SYSTEM_SET_DIOIRQPARAMS_OC, buf, sizeof(buf), SX12XX_Radio_All);
}

// IRQ is clear while reading.  2 for 1 SPI... WINNING!
uint32_t ICACHE_RAM_ATTR LR1121Driver::GetAndClearIrqStatus(SX12XX_Radio_Number_t radioNumber)
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
        // instance->rxSignalStats[0].telem_count++;
    }
    if (radioNumber == SX12XX_Radio_All || radioNumber == SX12XX_Radio_2)
    {
        // instance->rxSignalStats[1].telem_count++;
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

    // 3.7.4 WriteBuffer8
    hal.WriteCommand(LR11XX_REGMEM_WRITE_BUFFER8_OC, data, size, radioNumber);

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

    memcpy(RXdataBuffer, payloadbuf + 1, PayloadLengthRX);

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

int8_t ICACHE_RAM_ATTR LR1121Driver::GetRssiInst(SX12XX_Radio_Number_t radioNumber)
{
    // uint8_t status = 0;

    // hal.WriteCommand(LLCC68_RADIO_GET_RSSIINST, radioNumber);
    // hal.ReadCommand((uint8_t *)&status, 1, radioNumber);
    // return -(int8_t)(status / 2);
    return 0;
}

void ICACHE_RAM_ATTR LR1121Driver::GetLastPacketStats()
{
    // 8.3.7 GetPacketStatus
    uint8_t status[4] = {0};
    hal.WriteCommand(LR11XX_RADIO_GET_PKT_STATUS_OC, instance->processingPacketRadio);
    hal.ReadCommand(status, sizeof(status), instance->processingPacketRadio);
    
    // RssiPkt defines the average RSSI over the last packet received. RSSI value in dBm is –RssiPkt/2.
    LastPacketRSSI = -(int8_t)(status[1] / 2);

    // SignalRssiPkt is an estimation of RSSI of the LoRa signal (after despreading) on last packet received, in two’s
    // complement format [negated, dBm, fixdt(0,8,1)]. Actual RSSI in dB is -SignalRssiPkt/2.
    // LastPacketRSSI = -(int8_t)(status[3] / 2); // SignalRssiPkt

    LastPacketSNRRaw = (int8_t)status[2];
}

void ICACHE_RAM_ATTR LR1121Driver::IsrCallback_1()
{
    instance->IsrCallback(SX12XX_Radio_1);  
}

void ICACHE_RAM_ATTR LR1121Driver::IsrCallback_2()
{
    instance->IsrCallback(SX12XX_Radio_2);
}

void ICACHE_RAM_ATTR LR1121Driver::IsrCallback(SX12XX_Radio_Number_t radioNumber)
{
    instance->processingPacketRadio = radioNumber;
    // SX12XX_Radio_Number_t irqClearRadio = radioNumber;

    uint32_t irqStatus = instance->GetAndClearIrqStatus(radioNumber);
    if (irqStatus & LR1121_IRQ_TX_DONE)
    {
        instance->TXnbISR();
        // irqClearRadio = SX12XX_Radio_All;
    }
    else if (irqStatus & LR1121_IRQ_RX_DONE)
    {
        if (instance->RXnbISR(radioNumber))
        {
            // irqClearRadio = SX12XX_Radio_All;
        }
#if defined(DEBUG_RCVR_SIGNAL_STATS)
        else
        {
            // instance->rxSignalStats[(radioNumber == SX12XX_Radio_1) ? 0 : 1].fail_count++;
        }
#endif
        instance->isFirstRxIrq = false;   // RX isr is already fired in this period. (reset to true in tock)
    }
    else if (irqStatus == LR1121_IRQ_RADIO_NONE)
    {
        return;
    }
    // instance->ClearIrqStatus(irqClearRadio);
}
