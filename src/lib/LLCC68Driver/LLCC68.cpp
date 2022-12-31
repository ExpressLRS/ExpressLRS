#include "LLCC68_Regs.h"
#include "LLCC68_hal.h"
#include "LLCC68.h"
#include "logging.h"

LLCC68Hal hal;
LLCC68Driver *LLCC68Driver::instance = NULL;

//DEBUG_LLCC68_OTA_TIMING

#if defined(DEBUG_LLCC68_OTA_TIMING)
static uint32_t beginTX;
static uint32_t endTX;
#endif

#define RX_TIMEOUT_PERIOD_BASE_NANOS 15625

#ifdef USE_LLCC68_DCDC
    #ifndef OPT_USE_LLCC68_DCDC
        #define OPT_USE_LLCC68_DCDC true
    #endif
#else
    #define OPT_USE_LLCC68_DCDC false
#endif

LLCC68Driver::LLCC68Driver(): SX12xxDriverCommon()
{
    instance = this;
    timeout = 0xFFFFFF;
    currOpmode = LLCC68_MODE_SLEEP;
    lastSuccessfulPacketRadio = SX12XX_Radio_1;
}

void LLCC68Driver::End()
{
    if (currOpmode != LLCC68_MODE_SLEEP)
    {
        SetMode(LLCC68_MODE_SLEEP, SX12XX_Radio_All);
    }
    hal.end();
    RemoveCallbacks();
}

bool LLCC68Driver::Begin()
{
    hal.init();
    hal.IsrCallback_1 = &LLCC68Driver::IsrCallback_1;
    hal.IsrCallback_2 = &LLCC68Driver::IsrCallback_2;

    hal.reset();
    DBGLN("LLCC68 Begin");

    SetMode(LLCC68_MODE_STDBY_RC, SX12XX_Radio_All); // Put in STDBY_RC mode.  Must be LLCC68_MODE_STDBY_RC for LLCC68_RADIO_SET_REGULATORMODE to be set.

    // Validate that the LLCC68 is working.
    // Read the lora syncword register, should be 0x1424 at power on
    uint8_t syncWordMSB = hal.ReadRegister(0x0740, SX12XX_Radio_1);
    uint8_t syncWordLSB = hal.ReadRegister(0x0741, SX12XX_Radio_1);
    uint16_t loraSyncword = ((uint16_t)syncWordMSB << 8) | syncWordLSB;
    DBGLN("Read Vers LLCC68 #1 loraSyncword (5156): %d", loraSyncword);
    if (loraSyncword != 0x1424) {
        return false;
    }

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        syncWordMSB = hal.ReadRegister(0x0740, SX12XX_Radio_2);
        syncWordLSB = hal.ReadRegister(0x0741, SX12XX_Radio_2);
        loraSyncword = ((uint16_t)syncWordMSB << 8) | syncWordLSB;
        DBGLN("Read Vers LLCC68 #2 loraSyncword (5156): %d", loraSyncword);
        if (loraSyncword != 0x1424) {
            return false;
        }
    }

    hal.WriteCommand(LLCC68_RADIO_SET_RXTXFALLBACKMODE, LLCC68_RX_RXTXFALLBACKMODE_FS, SX12XX_Radio_All);
    hal.WriteRegister(LLCC68_RADIO_RX_GAIN, LLCC68_RX_BOOSTED_GAIN, SX12XX_Radio_All);   //default is low power mode, switch to high sensitivity instead
    hal.WriteCommand(LLCC68_RADIO_SET_DIO2ASSWITCHCTRL, LLCC68_DIO2ASSWITCHCTRL_ON, SX12XX_Radio_All);

    // Force the next power update, and the lowest power
    pwrCurrent = PWRPENDING_NONE;
    SetOutputPower(LLCC68_POWER_MIN);
    CommitOutputPower();
#if defined(USE_LLCC68_DCDC)
    if (OPT_USE_LLCC68_DCDC)
    {
        hal.WriteCommand(LLCC68_RADIO_SET_REGULATORMODE, LLCC68_USE_DCDC, SX12XX_Radio_All);        // Enable DCDC converter instead of LDO
    }
#endif

    return true;
}

void LLCC68Driver::startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t buffer;         // we just need a buffer for the write command
    SetFrequencyHz(freq, radioNumber);
    CommitOutputPower();
    hal.TXenable(radioNumber);
    hal.WriteCommand(LLCC68_RADIO_SET_TXCONTINUOUSWAVE, &buffer, 0, radioNumber);
}

void LLCC68Driver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t regfreq,
                          uint8_t PreambleLength, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval,
                          uint32_t flrcSyncWord, uint16_t flrcCrcSeed, uint8_t flrc)
{
    PayloadLength = _PayloadLength;
    IQinverted = InvertIQ;
    SetMode(LLCC68_MODE_STDBY_RC, SX12XX_Radio_All);
    hal.WriteCommand(LLCC68_RADIO_SET_PACKETTYPE, LLCC68_PACKET_TYPE_LORA, SX12XX_Radio_All, 20);
    DBG("Config LoRa ");
    SetFrequencyReg(regfreq);
    ConfigModParamsLoRa(bw, sf, cr);
#if defined(DEBUG_FREQ_CORRECTION) // TODO Chekc if this available with the LLCC68?
    LLCC68_RadioLoRaPacketLengthsModes_t packetLengthType = LLCC68_LORA_PACKET_VARIABLE_LENGTH;
#else
    LLCC68_RadioLoRaPacketLengthsModes_t packetLengthType = LLCC68_LORA_PACKET_FIXED_LENGTH;
#endif
    SetPacketParamsLoRa(PreambleLength, packetLengthType, _PayloadLength, InvertIQ);
    SetRxTimeoutUs(interval);

	uint8_t calFreq[2];
    uint32_t freq = regfreq * FREQ_STEP;
	if (freq > 900000000)
	{
		calFreq[0] = 0xE1;
		calFreq[1] = 0xE9;
	}
	else if (freq > 850000000)
	{
		calFreq[0] = 0xD7;
		calFreq[1] = 0xDB;
	}
	else if (freq > 770000000)
	{
		calFreq[0] = 0xC1;
		calFreq[1] = 0xC5;
	}
	else if (freq > 460000000)
	{
		calFreq[0] = 0x75;
		calFreq[1] = 0x81;
	}
	else if (freq > 425000000)
	{
		calFreq[0] = 0x6B;
		calFreq[1] = 0x6F;
	}
	hal.WriteCommand(LLCC68_RADIO_CALIBRATEIMAGE, calFreq, sizeof(calFreq), SX12XX_Radio_All);

    uint16_t dio1Mask = LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE | LLCC68_IRQ_RX_TX_TIMEOUT;
    uint16_t irqMask  = LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE | LLCC68_IRQ_RX_TX_TIMEOUT;
    SetDioIrqParams(irqMask, dio1Mask);
}

void LLCC68Driver::SetRxTimeoutUs(uint32_t interval)
{
    timeout = 0xFFFFFF; // no timeout, continuous mode
    if (interval)
    {
        timeout = interval * 1000 / RX_TIMEOUT_PERIOD_BASE_NANOS; // number of periods for the LLCC68 to timeout
    }
}

/***
 * @brief: 15.3 Implicit Header Mode Timeout Behavior (Data Sheet Rev. 1.0)
 ***/
void LLCC68Driver::clearTimeout(SX12XX_Radio_Number_t radioNumber)
{
    if (timeout == 0xFFFFFF) 
        return;

    hal.WriteRegister(0x0902, 0x00, radioNumber);

    uint8_t tempValue;
    if (radioNumber & SX12XX_Radio_1)
    {
        tempValue = hal.ReadRegister(0x0944, SX12XX_Radio_1);
        hal.WriteRegister(0x0944, tempValue | 0x02, SX12XX_Radio_1);
    }
    
if (GPIO_PIN_NSS_2 != UNDEF_PIN)
{
    if (radioNumber & SX12XX_Radio_2)
    {
        tempValue = hal.ReadRegister(0x0944, SX12XX_Radio_2);
        hal.WriteRegister(0x0944, tempValue | 0x02, SX12XX_Radio_2);
    }
}
}

/***
 * @brief: Schedule an output power change after the next transmit
 ***/
void LLCC68Driver::SetOutputPower(int8_t power)
{
    uint8_t pwrNew = constrain(power, LLCC68_POWER_MIN, LLCC68_POWER_MAX);

    if ((pwrPending == PWRPENDING_NONE && pwrCurrent != pwrNew) || pwrPending != pwrNew)
    {
        pwrPending = pwrNew;
        DBGLN("SetPower: %u", pwrPending);
    }
}

// void ICACHE_RAM_ATTR LLCC68Driver::CommitOutputPower()
// {
//     if (pwrPending == PWRPENDING_NONE)
//         return;

//     pwrCurrent = pwrPending;
//     pwrPending = PWRPENDING_NONE;
//     uint8_t buf[2] = { pwrCurrent, (uint8_t)LLCC68_RADIO_RAMP_10_US };
//     hal.WriteCommand(LLCC68_RADIO_SET_TXPARAMS, buf, sizeof(buf), SX12XX_Radio_All);
// }


/***
 * @brief: For discussion.  Should the PA Config be changed, or fixed?
 * 
 * "Changing the paDutyCycle will affect the distribution of the power in the harmonics and should be
 * selected to work in conjunction with a given matching network."
 ***/

void ICACHE_RAM_ATTR LLCC68Driver::CommitOutputPower()
{
    if (pwrPending == PWRPENDING_NONE)
        return;

    pwrCurrent = pwrPending;
    pwrPending = PWRPENDING_NONE;

    uint8_t pwrOffset = 0;
    uint8_t paDutyCycle = 0x04;
    uint8_t hpMax = 0x07;

    if (pwrCurrent > 30) // power range -9 to -1.
    {
        pwrOffset = 8;
        paDutyCycle = 0x02;
        hpMax = 0x02;
    }
    else if (pwrCurrent > 20)
    {
        paDutyCycle = 0x04;
        hpMax = 0x07;
    }
    else if (pwrCurrent > 17)
    {
        pwrOffset = 2;
        paDutyCycle = 0x03;
        hpMax = 0x05;
    }
    else if (pwrCurrent > 14)
    {
        pwrOffset = 5;
        paDutyCycle = 0x02;
        hpMax = 0x3;
    }
    else
    {
        pwrOffset = 8;
        paDutyCycle = 0x02;
        hpMax = 0x02;
    }

    // PA Operating Modes with Optimal Settings
    uint8_t paparams[4] = {paDutyCycle, hpMax, 0x00, 0x01};
    hal.WriteCommand(LLCC68_RADIO_SET_PACONFIG, paparams, sizeof(paparams), SX12XX_Radio_All, 25);

    uint8_t buf[2] = {pwrCurrent + pwrOffset, (uint8_t)LLCC68_RADIO_RAMP_10_US};
    hal.WriteCommand(LLCC68_RADIO_SET_TXPARAMS, buf, sizeof(buf), SX12XX_Radio_All);
}

void LLCC68Driver::SetMode(LLCC68_RadioOperatingModes_t OPmode, SX12XX_Radio_Number_t radioNumber)
{
    /*
    Comment out since it is difficult to keep track of dual radios.
    When checking SPI it is also useful to see every possible SPI transaction to make sure it fits when required.
    */
    // if (OPmode == currOpmode)
    // {
    //    return;
    // }

    WORD_ALIGNED_ATTR uint8_t buf[3];
    switch (OPmode)
    {

    case LLCC68_MODE_SLEEP:
        hal.WriteCommand(LLCC68_RADIO_SET_SLEEP, (uint8_t)0x01, radioNumber);
        break;

    case LLCC68_MODE_STDBY_RC:
        hal.WriteCommand(LLCC68_RADIO_SET_STANDBY, LLCC68_STDBY_RC, radioNumber, 1500);
        break;

    // The DC-DC supply regulation is automatically powered in STDBY_XOSC mode.
    case LLCC68_MODE_STDBY_XOSC:
        hal.WriteCommand(LLCC68_RADIO_SET_STANDBY, LLCC68_STDBY_XOSC, radioNumber, 50);
        break;

    case LLCC68_MODE_FS:
        hal.WriteCommand(LLCC68_RADIO_SET_FS, buf, 0, radioNumber, 70);
        break;

    case LLCC68_MODE_RX:
        buf[0] = timeout >> 16;
        buf[1] = timeout >> 8;
        buf[2] = timeout & 0xFF;
        hal.WriteCommand(LLCC68_RADIO_SET_RX, buf, sizeof(buf), radioNumber, 100);
        break;

    case LLCC68_MODE_RX_CONT:
        buf[0] = 0xFF;
        buf[1] = 0xFF;
        buf[2] = 0xFF;
        hal.WriteCommand(LLCC68_RADIO_SET_RX, buf, sizeof(buf), radioNumber, 100);
        break;

    case LLCC68_MODE_TX:
        //uses timeout Time-out duration = periodBase * periodBaseCount
        buf[0] = 0x00; // no timeout set for now
        buf[1] = 0x00; // TODO dynamic timeout based on expected onairtime
        buf[2] = 0x00;
        hal.WriteCommand(LLCC68_RADIO_SET_TX, buf, sizeof(buf), radioNumber, 100);
        break;

    case LLCC68_MODE_CAD:
        break;

    default:
        break;
    }

    currOpmode = OPmode;
}

void LLCC68Driver::ConfigModParamsLoRa(uint8_t bw, uint8_t sf, uint8_t cr)
{
    // Care must therefore be taken to ensure that modulation parameters are set using the command
    // SetModulationParam() only after defining the packet type SetPacketType() to be used

    WORD_ALIGNED_ATTR uint8_t rfparams[4] = {sf, bw, cr, 0};
    hal.WriteCommand(LLCC68_RADIO_SET_MODULATIONPARAMS, rfparams, sizeof(rfparams), SX12XX_Radio_All, 25);

    // Set the PA config to 22dBm max output
    // WORD_ALIGNED_ATTR uint8_t paparams[4] = {0x04, 0x07, 0x00, 0x01};
    // hal.WriteCommand(LLCC68_RADIO_SET_PACONFIG, paparams, sizeof(paparams), SX12XX_Radio_All, 25);

    // as per section 15.1: Modulation Quality with 500 kHz
    uint8_t reg = hal.ReadRegister(0x889, SX12XX_Radio_1);
    if (bw == LLCC68_LORA_BW_500)
    {
        hal.WriteRegister(0x889, reg & 0xFB, SX12XX_Radio_1);
    }
    else
    {
        hal.WriteRegister(0x889, reg | 0x04, SX12XX_Radio_1);
    }
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        reg = hal.ReadRegister(0x889, SX12XX_Radio_2);
        if (bw == LLCC68_LORA_BW_500)
        {
            hal.WriteRegister(0x889, reg & 0xFB, SX12XX_Radio_2);
        }
        else
        {
            hal.WriteRegister(0x889, reg | 0x04, SX12XX_Radio_2);
        }
    }

    // as per section 15.2: Better Resistance of the LLCC68 Tx to Antenna Mismatch
    reg = hal.ReadRegister(0x8D8, SX12XX_Radio_1);
    hal.WriteRegister(0x8D8, reg | 0x1E, SX12XX_Radio_1);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        reg = hal.ReadRegister(0x8D8, SX12XX_Radio_2);
        hal.WriteRegister(0x8D8, reg | 0x1E, SX12XX_Radio_2);
    }

}

void LLCC68Driver::SetPacketParamsLoRa(uint8_t PreambleLength, LLCC68_RadioLoRaPacketLengthsModes_t HeaderType,
                                       uint8_t PayloadLength, uint8_t InvertIQ)
{
    uint8_t buf[6];

    buf[0] = 0x00; // Preamble MSB
    buf[1] = PreambleLength;
    buf[2] = HeaderType;
    buf[3] = PayloadLength;
    buf[4] = LLCC68_LORA_CRC_OFF;
    // buf[5] = InvertIQ ? LLCC68_LORA_IQ_INVERTED : LLCC68_LORA_IQ_NORMAL; // This is broken for the sx1276 and never gets set.
    buf[5] = LLCC68_LORA_IQ_NORMAL;

    hal.WriteCommand(LLCC68_RADIO_SET_PACKETPARAMS, buf, sizeof(buf), SX12XX_Radio_All, 20);

    // FEI only triggers in Lora mode when the header is present :(
    modeSupportsFei = HeaderType == LLCC68_LORA_PACKET_VARIABLE_LENGTH;
}

void ICACHE_RAM_ATTR LLCC68Driver::SetFrequencyHz(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    uint32_t regfreq = (uint32_t)((double)freq / (double)FREQ_STEP);

    SetFrequencyReg(regfreq, radioNumber);
}

void ICACHE_RAM_ATTR LLCC68Driver::SetFrequencyReg(uint32_t regfreq, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[4] = {0};

    buf[0] = (uint8_t)((regfreq >> 24) & 0xFF);
    buf[1] = (uint8_t)((regfreq >> 16) & 0xFF);
    buf[2] = (uint8_t)((regfreq >> 8) & 0xFF);
    buf[3] = (uint8_t)(regfreq & 0xFF);

    hal.WriteCommand(LLCC68_RADIO_SET_RFFREQUENCY, buf, sizeof(buf), radioNumber);

    currFreq = regfreq;
}

void LLCC68Driver::SetFIFOaddr(uint8_t txBaseAddr, uint8_t rxBaseAddr)
{
    uint8_t buf[2];

    buf[0] = txBaseAddr;
    buf[1] = rxBaseAddr;
    hal.WriteCommand(LLCC68_RADIO_SET_BUFFERBASEADDRESS, buf, sizeof(buf), SX12XX_Radio_All);
}

void LLCC68Driver::SetDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask)
{
    uint8_t buf[8];

    buf[0] = (uint8_t)((irqMask >> 8) & 0x00FF);
    buf[1] = (uint8_t)(irqMask & 0x00FF);
    buf[2] = (uint8_t)((dio1Mask >> 8) & 0x00FF);
    buf[3] = (uint8_t)(dio1Mask & 0x00FF);
    buf[4] = (uint8_t)((dio2Mask >> 8) & 0x00FF);
    buf[5] = (uint8_t)(dio2Mask & 0x00FF);
    buf[6] = (uint8_t)((dio3Mask >> 8) & 0x00FF);
    buf[7] = (uint8_t)(dio3Mask & 0x00FF);

    hal.WriteCommand(LLCC68_RADIO_SET_DIOIRQPARAMS, buf, sizeof(buf), SX12XX_Radio_All);
}

uint16_t ICACHE_RAM_ATTR LLCC68Driver::GetIrqStatus(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status[2];

    hal.ReadCommand(LLCC68_RADIO_GET_IRQSTATUS, status, 2, radioNumber);
    return status[0] << 8 | status[1];
}

void ICACHE_RAM_ATTR LLCC68Driver::ClearIrqStatus(uint16_t irqMask, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(((uint16_t)irqMask >> 8) & 0x00FF);
    buf[1] = (uint8_t)((uint16_t)irqMask & 0x00FF);

    hal.WriteCommand(LLCC68_RADIO_CLR_IRQSTATUS, buf, sizeof(buf), radioNumber);
}

void ICACHE_RAM_ATTR LLCC68Driver::TXnbISR()
{
    currOpmode = LLCC68_MODE_FS; // radio goes to FS after TX
#ifdef DEBUG_LLCC68_OTA_TIMING
    endTX = micros();
    DBGLN("TOA: %d", endTX - beginTX);
#endif
    CommitOutputPower();
    TXdoneCallback();
}

void ICACHE_RAM_ATTR LLCC68Driver::TXnb(uint8_t * data, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    if (currOpmode == LLCC68_MODE_TX) //catch TX timeout
    {
        DBGLN("Timeout!");
        SetMode(LLCC68_MODE_FS, SX12XX_Radio_All);
        ClearIrqStatus(LLCC68_IRQ_RADIO_ALL, SX12XX_Radio_All);
        TXnbISR();
        return;
    }

    if (radioNumber == SX12XX_Radio_Default)
    {
        radioNumber = lastSuccessfulPacketRadio;
    }
        
    // Normal diversity mode
    if (GPIO_PIN_NSS_2 != UNDEF_PIN && radioNumber != SX12XX_Radio_All)
    {
        // Make sure the unused radio is in FS mode and will not receive the tx packet.
        if (radioNumber == SX12XX_Radio_1)
        {
            instance->SetMode(LLCC68_MODE_FS, SX12XX_Radio_2);
        }
        else
        {
            instance->SetMode(LLCC68_MODE_FS, SX12XX_Radio_1);
        }
    }

    hal.TXenable(radioNumber); // do first to allow PA stablise
    hal.WriteBuffer(0x00, data, size, radioNumber); //todo fix offset to equal fifo addr
    instance->SetMode(LLCC68_MODE_TX, radioNumber);

#ifdef DEBUG_LLCC68_OTA_TIMING
    beginTX = micros();
#endif
}

bool ICACHE_RAM_ATTR LLCC68Driver::RXnbISR(uint16_t irqStatus, SX12XX_Radio_Number_t radioNumber)
{
    // In continuous receive mode, the device stays in Rx mode
    if (timeout != 0xFFFF)
    {
        // From table 11-28, pg 81 datasheet rev 3.2
        // upon successsful receipt, when the timer is active or in single mode, it returns to STDBY_RC
        // but because we have AUTO_FS enabled we automatically transition to state LLCC68_MODE_FS
        currOpmode = LLCC68_MODE_FS;
    }

    rx_status fail = SX12XX_RX_OK;
    if (fail == SX12XX_RX_OK)
    {
        uint8_t const FIFOaddr = GetRxBufferAddr(radioNumber);
        hal.ReadBuffer(FIFOaddr, RXdataBuffer, PayloadLength, radioNumber);
    }

    return RXdoneCallback(fail);
}

void ICACHE_RAM_ATTR LLCC68Driver::RXnb(LLCC68_RadioOperatingModes_t rxMode)
{
    hal.RXenable();
    SetMode(rxMode, SX12XX_Radio_All);
}

uint8_t ICACHE_RAM_ATTR LLCC68Driver::GetRxBufferAddr(SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t status[2] = {0};
    hal.ReadCommand(LLCC68_RADIO_GET_RXBUFFERSTATUS, status, 2, radioNumber);
    return status[1];
}

void ICACHE_RAM_ATTR LLCC68Driver::GetStatus(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status = 0;
    hal.ReadCommand(LLCC68_RADIO_GET_STATUS, (uint8_t *)&status, 1, radioNumber);
    DBGLN("Status: %x, %x, %x", (0b11100000 & status) >> 5, (0b00011100 & status) >> 2, 0b00000001 & status);
}

bool ICACHE_RAM_ATTR LLCC68Driver::GetFrequencyErrorbool()
{
    return false;
}

int8_t ICACHE_RAM_ATTR LLCC68Driver::GetRssiInst(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status = 0;

    hal.ReadCommand(LLCC68_RADIO_GET_RSSIINST, (uint8_t *)&status, 1, radioNumber);
    return -(int8_t)(status / 2);
}

void ICACHE_RAM_ATTR LLCC68Driver::GetLastPacketStats()
{
    uint8_t status[2];
    hal.ReadCommand(LLCC68_RADIO_GET_PACKETSTATUS, status, 2, processingPacketRadio);
    // LoRa mode has both RSSI and SNR
    LastPacketRSSI = -(int8_t)(status[0] / 2);
    LastPacketSNRRaw = (int8_t)status[1];
}

void ICACHE_RAM_ATTR LLCC68Driver::IsrCallback_1()
{
    instance->IsrCallback(SX12XX_Radio_1);
}

void ICACHE_RAM_ATTR LLCC68Driver::IsrCallback_2()
{
    instance->IsrCallback(SX12XX_Radio_2);
}

void ICACHE_RAM_ATTR LLCC68Driver::IsrCallback(SX12XX_Radio_Number_t radioNumber)
{
    instance->processingPacketRadio = radioNumber;
    SX12XX_Radio_Number_t irqClearRadio = radioNumber;

    uint16_t irqStatus = instance->GetIrqStatus(radioNumber);
    if (irqStatus & LLCC68_IRQ_TX_DONE)
    {
        hal.TXRXdisable();
        instance->ClearIrqStatus(LLCC68_IRQ_RADIO_ALL, SX12XX_Radio_All); // Calling ClearIrqStatus first allows more time for processing instead of waiting for busy to clear.
        instance->TXnbISR();
        return;
    }
    else if (irqStatus & LLCC68_IRQ_RX_DONE)
    {
        if (instance->RXnbISR(irqStatus, radioNumber))
        {
            instance->lastSuccessfulPacketRadio = radioNumber;
        }
        instance->clearTimeout(irqClearRadio);
    }
    else if (irqStatus == LLCC68_IRQ_RX_TX_TIMEOUT)
    {
        instance->ClearIrqStatus(LLCC68_IRQ_RADIO_ALL, irqClearRadio); // Calling ClearIrqStatus first allows more time for processing e.g. 2nd radio IRQ.
        instance->SetMode(LLCC68_MODE_FS, radioNumber); // Returns automatically to STBY_RC mode on timer end-of-count.  Setting FS will be needed for Gemini Tx mode, and to minimise jitter.
        return;
    }
    else if (irqStatus == LLCC68_IRQ_RADIO_NONE)
    {
        return;
    }
    instance->ClearIrqStatus(LLCC68_IRQ_RADIO_ALL, irqClearRadio);
}
