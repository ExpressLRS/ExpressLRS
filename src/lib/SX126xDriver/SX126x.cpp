#include "SX126x_Regs.h"
#include "SX126x_hal.h"
#include "SX126x.h"
#include "logging.h"

SX126xHal hal;
SX126xDriver *SX126xDriver::instance = NULL;

//DEBUG_SX126x_OTA_TIMING

/* Steps for startup

1. If not in STDBY_RC mode, then go to this mode by sending the command:
SetStandby(STDBY_RC)

2. Define the LoRa® packet type by sending the command:
SetPacketType(PACKET_TYPE_LORA)

3. Define the RF frequency by sending the command:
SetRfFrequency(rfFrequency)
The LSB of rfFrequency is equal to the PLL step i.e. 32e6/2^25 Hz. SetRfFrequency() defines the Tx frequency.

4. Indicate the addresses where the packet handler will read (txBaseAddress in Tx) or write (rxBaseAddress in Rx) the first
byte of the data payload by sending the command:
SetBufferBaseAddress(txBaseAddress, rxBaseAddress)
Note:
txBaseAddress and rxBaseAddress are offset relative to the beginning of the data memory map.

5. Define the modulation parameter signal BW SF CR
*/

#if defined(DEBUG_SX126x_OTA_TIMING)
static uint32_t beginTX;
static uint32_t endTX;
#endif

#define RX_TIMEOUT_PERIOD_BASE_NANOS 15625

#ifdef USE_SX126x_DCDC
    #ifndef OPT_USE_SX126x_DCDC
        #define OPT_USE_SX126x_DCDC true
    #endif
#else
    #define OPT_USE_SX126x_DCDC false
#endif

SX126xDriver::SX126xDriver(): SX12xxDriverCommon()
{
    instance = this;
    timeout = 0xffff;
    currOpmode = SX126x_MODE_SLEEP;
    lastSuccessfulPacketRadio = SX12XX_Radio_1;
}

void SX126xDriver::End()
{
    if (currOpmode != SX126x_MODE_SLEEP)
    {
        SetMode(SX126x_MODE_SLEEP, SX12XX_Radio_All);
    }
    hal.end();
    RemoveCallbacks();
}

bool SX126xDriver::Begin()
{
    hal.init();
    hal.IsrCallback_1 = &SX126xDriver::IsrCallback_1;
    hal.IsrCallback_2 = &SX126xDriver::IsrCallback_2;

    hal.reset();
    DBGLN("SX126x Begin");

    SetMode(SX126x_MODE_STDBY_RC, SX12XX_Radio_All); // Put in STDBY_RC mode.  Must be SX126x_MODE_STDBY_RC for SX126x_RADIO_SET_REGULATORMODE to be set.


    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        hal.WriteRegister(0x8AC, 0x96, SX12XX_Radio_2);   //default is low power mode, switch to high sensitivity instead
    }

    hal.WriteRegister(0x8AC, 0x96, SX12XX_Radio_1);   //default is low power mode, switch to high sensitivity instead
    // Force the next power update, and the lowest power
    pwrCurrent = PWRPENDING_NONE;
    SetOutputPower(SX126x_POWER_MIN);
    CommitOutputPower();
#if defined(USE_SX126x_DCDC)
    if (OPT_USE_SX126x_DCDC)
    {
        hal.WriteCommand(SX126x_RADIO_SET_REGULATORMODE, SX126x_USE_DCDC, SX12XX_Radio_All);        // Enable DCDC converter instead of LDO
    }
#endif

    return true;
}

void SX126xDriver::startCWTest(uint32_t freq, SX12XX_Radio_Number_t radio)
{
    uint8_t buffer;         // we just need a buffer for the write command
    SetFrequencyHz(freq);
    CommitOutputPower();
    hal.TXenable(radio);
    hal.WriteCommand(SX126x_RADIO_SET_TXCONTINUOUSWAVE, &buffer, 0, radio);
}

void SX126xDriver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t regfreq,
                          uint8_t PreambleLength, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval,
                          uint32_t flrcSyncWord, uint16_t flrcCrcSeed, uint8_t flrc)
{
    PayloadLength = _PayloadLength;
    IQinverted = InvertIQ;
    SetMode(SX126x_MODE_STDBY_RC, SX12XX_Radio_All);
    hal.WriteCommand(SX126x_RADIO_SET_PACKETTYPE, SX126x_PACKET_TYPE_LORA, SX12XX_Radio_All, 20);
    DBG("Config LoRa ");
    SetFrequencyReg(regfreq);
    ConfigModParamsLoRa(bw, sf, cr);
#if defined(DEBUG_FREQ_CORRECTION)
    SX126x_RadioLoRaPacketLengthsModes_t packetLengthType = SX126x_LORA_PACKET_VARIABLE_LENGTH;
#else
    SX126x_RadioLoRaPacketLengthsModes_t packetLengthType = SX126x_LORA_PACKET_FIXED_LENGTH;
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
	hal.WriteCommand(SX126x_RADIO_CALIBRATEIMAGE, calFreq, sizeof(calFreq), SX12XX_Radio_All);

    uint8_t dio1Mask = SX126x_IRQ_TX_DONE | SX126x_IRQ_RX_DONE;
    uint8_t irqMask  = SX126x_IRQ_TX_DONE | SX126x_IRQ_RX_DONE | SX126x_IRQ_CRC_ERROR;
    SetDioIrqParams(irqMask, dio1Mask);
}

void SX126xDriver::SetRxTimeoutUs(uint32_t interval)
{
    if (interval)
    {
        timeout = interval * 1000 / RX_TIMEOUT_PERIOD_BASE_NANOS; // number of periods for the SX126x to timeout
    }
    else
    {
        timeout = 0;   // no timeout, continuous mode
    }
}

/***
 * @brief: Schedule an output power change after the next transmit
 ***/
void SX126xDriver::SetOutputPower(int8_t power)
{
    uint8_t pwrNew = constrain(power, SX126x_POWER_MIN, SX126x_POWER_MAX);

    if ((pwrPending == PWRPENDING_NONE && pwrCurrent != pwrNew) || pwrPending != pwrNew)
    {
        pwrPending = pwrNew;
        DBGLN("SetPower: %u", pwrPending);
    }
}

void ICACHE_RAM_ATTR SX126xDriver::CommitOutputPower()
{
    if (pwrPending == PWRPENDING_NONE)
        return;

    pwrCurrent = pwrPending;
    pwrPending = PWRPENDING_NONE;
    uint8_t buf[2] = { pwrCurrent, (uint8_t)SX126x_RADIO_RAMP_10_US };
    hal.WriteCommand(SX126x_RADIO_SET_TXPARAMS, buf, sizeof(buf), SX12XX_Radio_All);
}

void SX126xDriver::SetMode(SX126x_RadioOperatingModes_t OPmode, SX12XX_Radio_Number_t radioNumber)
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

    case SX126x_MODE_SLEEP:
        hal.WriteCommand(SX126x_RADIO_SET_SLEEP, (uint8_t)0x01, radioNumber);
        break;

    case SX126x_MODE_STDBY_RC:
        hal.WriteCommand(SX126x_RADIO_SET_STANDBY, SX126x_STDBY_RC, radioNumber, 1500);
        break;

    // The DC-DC supply regulation is automatically powered in STDBY_XOSC mode.
    case SX126x_MODE_STDBY_XOSC:
        hal.WriteCommand(SX126x_RADIO_SET_STANDBY, SX126x_STDBY_XOSC, radioNumber, 50);
        break;

    case SX126x_MODE_FS:
        hal.WriteCommand(SX126x_RADIO_SET_FS, (uint8_t)0x00, radioNumber, 70);
        break;

    case SX126x_MODE_RX:
        buf[0] = timeout >> 16;
        buf[1] = timeout >> 8;
        buf[2] = timeout & 0xFF;
        hal.WriteCommand(SX126x_RADIO_SET_RX, buf, sizeof(buf), radioNumber, 100);
        break;

    case SX126x_MODE_RX_CONT:
        buf[0] = 0x00;
        buf[1] = 0x00;
        buf[2] = 0x00;
        hal.WriteCommand(SX126x_RADIO_SET_RX, buf, sizeof(buf), radioNumber, 100);
        break;

    case SX126x_MODE_TX:
        //uses timeout Time-out duration = periodBase * periodBaseCount
        buf[0] = 0x00; // no timeout set for now
        buf[1] = 0x00; // TODO dynamic timeout based on expected onairtime
        buf[2] = 0x00;
        hal.WriteCommand(SX126x_RADIO_SET_TX, buf, sizeof(buf), radioNumber, 100);
        break;

    case SX126x_MODE_CAD:
        break;

    default:
        break;
    }

    currOpmode = OPmode;
}

void SX126xDriver::ConfigModParamsLoRa(uint8_t bw, uint8_t sf, uint8_t cr)
{
    // Care must therefore be taken to ensure that modulation parameters are set using the command
    // SetModulationParam() only after defining the packet type SetPacketType() to be used

    WORD_ALIGNED_ATTR uint8_t rfparams[4] = {sf, bw, cr, 0};
    hal.WriteCommand(SX126x_RADIO_SET_MODULATIONPARAMS, rfparams, sizeof(rfparams), SX12XX_Radio_All, 25);

    // Set the PA config to 22dBm max output
    WORD_ALIGNED_ATTR uint8_t paparams[4] = {0x04, 0x07, 0x00, 0x01};
    hal.WriteCommand(SX126x_RADIO_SET_PACONFIG, paparams, sizeof(paparams), SX12XX_Radio_All, 25);

    // as per section 15.1: Modulation Quality with 500 kHz
    uint8_t reg = hal.ReadRegister(0x889, SX12XX_Radio_1);
    if (bw == SX126x_LORA_BW_500)
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
        if (bw == SX126x_LORA_BW_500)
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

void SX126xDriver::SetPacketParamsLoRa(uint8_t PreambleLength, SX126x_RadioLoRaPacketLengthsModes_t HeaderType,
                                       uint8_t PayloadLength, uint8_t InvertIQ)
{
    uint8_t buf[6];

    buf[0] = 0x00;
    buf[1] = PreambleLength;
    buf[2] = HeaderType;
    buf[3] = PayloadLength;
    buf[4] = SX126x_LORA_CRC_OFF;
    buf[5] = InvertIQ ? SX126x_LORA_IQ_INVERTED : SX126x_LORA_IQ_NORMAL;

    hal.WriteCommand(SX126x_RADIO_SET_PACKETPARAMS, buf, sizeof(buf), SX12XX_Radio_All, 20);

    // FEI only triggers in Lora mode when the header is present :(
    modeSupportsFei = HeaderType == SX126x_LORA_PACKET_VARIABLE_LENGTH;
}

void ICACHE_RAM_ATTR SX126xDriver::SetFrequencyHz(uint32_t freq)
{
    uint32_t regfreq = (uint32_t)((double)freq / (double)FREQ_STEP);

    SetFrequencyReg(regfreq);
}

void ICACHE_RAM_ATTR SX126xDriver::SetFrequencyReg(uint32_t regfreq)
{
    WORD_ALIGNED_ATTR uint8_t buf[4] = {0};

    buf[0] = (uint8_t)((regfreq >> 24) & 0xFF);
    buf[1] = (uint8_t)((regfreq >> 16) & 0xFF);
    buf[2] = (uint8_t)((regfreq >> 8) & 0xFF);
    buf[3] = (uint8_t)(regfreq & 0xFF);

    hal.WriteCommand(SX126x_RADIO_SET_RFFREQUENCY, buf, sizeof(buf), SX12XX_Radio_All);

    currFreq = regfreq;
}

void SX126xDriver::SetFIFOaddr(uint8_t txBaseAddr, uint8_t rxBaseAddr)
{
    uint8_t buf[2];

    buf[0] = txBaseAddr;
    buf[1] = rxBaseAddr;
    hal.WriteCommand(SX126x_RADIO_SET_BUFFERBASEADDRESS, buf, sizeof(buf), SX12XX_Radio_All);
}

void SX126xDriver::SetDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask)
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

    hal.WriteCommand(SX126x_RADIO_SET_DIOIRQPARAMS, buf, sizeof(buf), SX12XX_Radio_All);
}

uint16_t ICACHE_RAM_ATTR SX126xDriver::GetIrqStatus(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status[2];

    hal.ReadCommand(SX126x_RADIO_GET_IRQSTATUS, status, 2, radioNumber);
    return status[0] << 8 | status[1];
}

void ICACHE_RAM_ATTR SX126xDriver::ClearIrqStatus(uint16_t irqMask, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(((uint16_t)irqMask >> 8) & 0x00FF);
    buf[1] = (uint8_t)((uint16_t)irqMask & 0x00FF);

    hal.WriteCommand(SX126x_RADIO_CLR_IRQSTATUS, buf, sizeof(buf), radioNumber);
}

void ICACHE_RAM_ATTR SX126xDriver::TXnbISR()
{
    currOpmode = SX126x_MODE_FS; // radio goes to FS after TX
#ifdef DEBUG_SX126x_OTA_TIMING
    endTX = micros();
    DBGLN("TOA: %d", endTX - beginTX);
#endif
    CommitOutputPower();
    TXdoneCallback();
}

void ICACHE_RAM_ATTR SX126xDriver::TXnb(uint8_t * data, uint8_t size)
{
    if (currOpmode == SX126x_MODE_TX) //catch TX timeout
    {
        DBGLN("Timeout!");
        SetMode(SX126x_MODE_FS, SX12XX_Radio_All);
        ClearIrqStatus(SX126x_IRQ_RADIO_ALL, SX12XX_Radio_All);
        TXnbISR();
        return;
    }

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        // Make sure the unused radio is in FS mode and will not receive the tx packet.
        if (lastSuccessfulPacketRadio == SX12XX_Radio_1)
        {
            instance->SetMode(SX126x_MODE_FS, SX12XX_Radio_2);
        }
        else
        {
            instance->SetMode(SX126x_MODE_FS, SX12XX_Radio_1);
        }
    }

    hal.TXenable(lastSuccessfulPacketRadio); // do first to allow PA stablise
    hal.WriteBuffer(0x00, data, size, lastSuccessfulPacketRadio); //todo fix offset to equal fifo addr
    instance->SetMode(SX126x_MODE_TX, lastSuccessfulPacketRadio);

#ifdef DEBUG_SX126x_OTA_TIMING
    beginTX = micros();
#endif
}

bool ICACHE_RAM_ATTR SX126xDriver::RXnbISR(uint16_t irqStatus, SX12XX_Radio_Number_t radioNumber)
{
    // In continuous receive mode, the device stays in Rx mode
    if (timeout != 0xFFFF)
    {
        // From table 11-28, pg 81 datasheet rev 3.2
        // upon successsful receipt, when the timer is active or in single mode, it returns to STDBY_RC
        // but because we have AUTO_FS enabled we automatically transition to state SX126x_MODE_FS
        currOpmode = SX126x_MODE_FS;
    }

    rx_status fail = SX12XX_RX_OK;
    if (fail == SX12XX_RX_OK)
    {
        uint8_t const FIFOaddr = GetRxBufferAddr(radioNumber);
        hal.ReadBuffer(FIFOaddr, RXdataBuffer, PayloadLength, radioNumber);
    }

    return RXdoneCallback(fail);
}

void ICACHE_RAM_ATTR SX126xDriver::RXnb(SX126x_RadioOperatingModes_t rxMode)
{
    hal.RXenable();
    SetMode(rxMode, SX12XX_Radio_All);
}

uint8_t ICACHE_RAM_ATTR SX126xDriver::GetRxBufferAddr(SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t status[2] = {0};
    hal.ReadCommand(SX126x_RADIO_GET_RXBUFFERSTATUS, status, 2, radioNumber);
    return status[1];
}

void ICACHE_RAM_ATTR SX126xDriver::GetStatus(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status = 0;
    hal.ReadCommand(SX126x_RADIO_GET_STATUS, (uint8_t *)&status, 1, radioNumber);
    DBGLN("Status: %x, %x, %x", (0b11100000 & status) >> 5, (0b00011100 & status) >> 2, 0b00000001 & status);
}

bool ICACHE_RAM_ATTR SX126xDriver::GetFrequencyErrorbool()
{
    return false;
}

int8_t ICACHE_RAM_ATTR SX126xDriver::GetRssiInst()
{
    uint8_t status = 0;

    hal.ReadCommand(SX126x_RADIO_GET_RSSIINST, (uint8_t *)&status, 1, lastSuccessfulPacketRadio);
    return -(int8_t)(status / 2);
}

void ICACHE_RAM_ATTR SX126xDriver::GetLastPacketStats()
{
    uint8_t status[2];
    hal.ReadCommand(SX126x_RADIO_GET_PACKETSTATUS, status, 2, processingPacketRadio);
    // LoRa mode has both RSSI and SNR
    LastPacketRSSI = -(int8_t)(status[0] / 2);
    LastPacketSNRRaw = (int8_t)status[1];
    // https://www.mouser.com/datasheet/2/761/DS_SX126x-1_V2.2-1511144.pdf p84
    // need to subtract SNR from RSSI when SNR <= 0;
    int8_t negOffset = (LastPacketSNRRaw < 0) ? (LastPacketSNRRaw / RADIO_SNR_SCALE) : 0;
    LastPacketRSSI += negOffset;
}

void ICACHE_RAM_ATTR SX126xDriver::IsrCallback_1()
{
    instance->IsrCallback(SX12XX_Radio_1);
}

void ICACHE_RAM_ATTR SX126xDriver::IsrCallback_2()
{
    instance->IsrCallback(SX12XX_Radio_2);
}

void ICACHE_RAM_ATTR SX126xDriver::IsrCallback(SX12XX_Radio_Number_t radioNumber)
{
    instance->processingPacketRadio = radioNumber;
    SX12XX_Radio_Number_t irqClearRadio = radioNumber;
DBGLN("cb");
    uint16_t irqStatus = instance->GetIrqStatus(radioNumber);
    if (irqStatus & SX126x_IRQ_TX_DONE)
    {
        hal.TXRXdisable();
        instance->TXnbISR();
        irqClearRadio = SX12XX_Radio_All;
    }
    else if (irqStatus & SX126x_IRQ_RX_DONE)
    {
        if (instance->RXnbISR(irqStatus, radioNumber))
        {
            instance->lastSuccessfulPacketRadio = radioNumber;
            irqClearRadio = SX12XX_Radio_All; // Packet received so clear all radios and dont spend extra time retrieving data.
        }
    }
    else if (irqStatus == SX126x_IRQ_RADIO_NONE)
    {
        return;
    }
    instance->ClearIrqStatus(SX126x_IRQ_RADIO_ALL, irqClearRadio);
}
