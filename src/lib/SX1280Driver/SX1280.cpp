#include "SX1280_Regs.h"
#include "SX1280_hal.h"
#include "SX1280.h"
#include "logging.h"
#include "RFAMP_hal.h"

SX1280Hal hal;
SX1280Driver *SX1280Driver::instance = NULL;

RFAMP_hal RFAMP;

//DEBUG_SX1280_OTA_TIMING

/* Steps for startup

1. If not in STDBY_RC mode, then go to this mode by sending the command:
SetStandby(STDBY_RC)

2. Define the LoRa® packet type by sending the command:
SetPacketType(PACKET_TYPE_LORA)

3. Define the RF frequency by sending the command:
SetRfFrequency(rfFrequency)
The LSB of rfFrequency is equal to the PLL step i.e. 52e6/2^18 Hz. SetRfFrequency() defines the Tx frequency.

4. Indicate the addresses where the packet handler will read (txBaseAddress in Tx) or write (rxBaseAddress in Rx) the first
byte of the data payload by sending the command:
SetBufferBaseAddress(txBaseAddress, rxBaseAddress)
Note:
txBaseAddress and rxBaseAddress are offset relative to the beginning of the data memory map.

5. Define the modulation parameter signal BW SF CR
*/

#if defined(DEBUG_SX1280_OTA_TIMING)
static uint32_t beginTX;
static uint32_t endTX;
#endif

/*
 * Period Base from table 11-24, page 79 datasheet rev 3.2
 * SX1280_RADIO_TICK_SIZE_0015_US = 15625 nanos
 * SX1280_RADIO_TICK_SIZE_0062_US = 62500 nanos
 * SX1280_RADIO_TICK_SIZE_1000_US = 1000000 nanos
 * SX1280_RADIO_TICK_SIZE_4000_US = 4000000 nanos
 */
#define RX_TIMEOUT_PERIOD_BASE SX1280_RADIO_TICK_SIZE_0015_US
#define RX_TIMEOUT_PERIOD_BASE_NANOS 15625

#ifdef USE_SX1280_DCDC
    #ifndef OPT_USE_SX1280_DCDC
        #define OPT_USE_SX1280_DCDC true
    #endif
#else
    #define OPT_USE_SX1280_DCDC false
#endif

SX1280Driver::SX1280Driver(): SX12xxDriverCommon()
{
    instance = this;
    timeout = 0xffff;
    currOpmode = SX1280_MODE_SLEEP;
    lastSuccessfulPacketRadio = SX12XX_Radio_1;
}

void SX1280Driver::End()
{
    if (currOpmode != SX1280_MODE_SLEEP)
    {
        SetMode(SX1280_MODE_SLEEP, SX12XX_Radio_All);
    }
    hal.end();
    RFAMP.TXRXdisable();
    RemoveCallbacks();
    currFreq = (uint32_t)((double)2400000000 / (double)FREQ_STEP);
    PayloadLength = 8; // Dummy default value which is overwritten during setup.
}

bool SX1280Driver::Begin()
{
    hal.init();
    hal.IsrCallback_1 = &SX1280Driver::IsrCallback_1;
    hal.IsrCallback_2 = &SX1280Driver::IsrCallback_2;

    hal.reset();
    DBGLN("SX1280 Begin");
  
    RFAMP.init();

    SetMode(SX1280_MODE_STDBY_RC, SX12XX_Radio_All); // Put in STDBY_RC mode.  Must be SX1280_MODE_STDBY_RC for SX1280_RADIO_SET_REGULATORMODE to be set.

    uint16_t firmwareRev = (((hal.ReadRegister(REG_LR_FIRMWARE_VERSION_MSB, SX12XX_Radio_1)) << 8) | (hal.ReadRegister(REG_LR_FIRMWARE_VERSION_MSB + 1, SX12XX_Radio_1)));
    DBGLN("Read Vers sx1280 #1: %d", firmwareRev);
    if ((firmwareRev == 0) || (firmwareRev == 65535))
    {
        // SPI communication failed, just return without configuration
        return false;
    }

    hal.WriteRegister(0x0891, (hal.ReadRegister(0x0891, SX12XX_Radio_1) | 0xC0), SX12XX_Radio_1);   //default is low power mode, switch to high sensitivity instead

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        firmwareRev = (((hal.ReadRegister(REG_LR_FIRMWARE_VERSION_MSB, SX12XX_Radio_2)) << 8) | (hal.ReadRegister(REG_LR_FIRMWARE_VERSION_MSB + 1, SX12XX_Radio_2)));
        DBGLN("Read Vers sx1280 #2: %d", firmwareRev);
        if ((firmwareRev == 0) || (firmwareRev == 65535))
        {
            // SPI communication failed, just return without configuration
            return false;
        }

        hal.WriteRegister(0x0891, (hal.ReadRegister(0x0891, SX12XX_Radio_2) | 0xC0), SX12XX_Radio_2);   //default is low power mode, switch to high sensitivity instead
    }

#if defined(TARGET_RX)
    hal.WriteCommand(SX1280_RADIO_SET_AUTOFS, 0x01, SX12XX_Radio_All); //Enable auto FS
#else
/*
Do not enable for dual radio TX.
When SX1280_RADIO_SET_AUTOFS is set and tlm received by only 1 of the 2 radios,  that radio will go into FS mode and the other
into Standby mode.  After the following SPI command for tx mode, busy will go high for differing periods of time because 1 is
transitioning from FS mode and the other from Standby mode. This causes the tx done dio of the 2 radios to occur at very different times.
*/
    if (GPIO_PIN_NSS_2 == UNDEF_PIN)
    {
        hal.WriteCommand(SX1280_RADIO_SET_AUTOFS, 0x01, SX12XX_Radio_All); //Enable auto FS
    }
#endif

    // Force the next power update, and the lowest power
    pwrCurrent = PWRPENDING_NONE;
    SetOutputPower(SX1280_POWER_MIN);
    CommitOutputPower();
#if defined(USE_SX1280_DCDC)
    if (OPT_USE_SX1280_DCDC)
    {
        hal.WriteCommand(SX1280_RADIO_SET_REGULATORMODE, SX1280_USE_DCDC, SX12XX_Radio_All);        // Enable DCDC converter instead of LDO
    }
#endif

    return true;
}

void SX1280Driver::startCWTest(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t buffer;         // we just need a buffer for the write command
    SetFrequencyHz(freq, radioNumber);
    CommitOutputPower();
    RFAMP.TXenable(radioNumber);
    hal.WriteCommand(SX1280_RADIO_SET_TXCONTINUOUSWAVE, &buffer, 0, radioNumber);
}

void SX1280Driver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t regfreq,
                          uint8_t PreambleLength, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval,
                          uint32_t flrcSyncWord, uint16_t flrcCrcSeed, uint8_t flrc)
{
    uint8_t const mode = (flrc) ? SX1280_PACKET_TYPE_FLRC : SX1280_PACKET_TYPE_LORA;

    PayloadLength = _PayloadLength;
    IQinverted = InvertIQ;
    packet_mode = mode;
    SetMode(SX1280_MODE_STDBY_RC, SX12XX_Radio_All);
    hal.WriteCommand(SX1280_RADIO_SET_PACKETTYPE, mode, SX12XX_Radio_All, 20);
    if (mode == SX1280_PACKET_TYPE_FLRC)
    {
        DBG("Config FLRC ");
        ConfigModParamsFLRC(bw, cr, sf);
        SetPacketParamsFLRC(SX1280_FLRC_PACKET_FIXED_LENGTH, PreambleLength, _PayloadLength, flrcSyncWord, flrcCrcSeed, cr);
    }
    else
    {
        DBG("Config LoRa ");
        ConfigModParamsLoRa(bw, sf, cr);
#if defined(DEBUG_FREQ_CORRECTION)
        SX1280_RadioLoRaPacketLengthsModes_t packetLengthType = SX1280_LORA_PACKET_VARIABLE_LENGTH;
#else
        SX1280_RadioLoRaPacketLengthsModes_t packetLengthType = SX1280_LORA_PACKET_FIXED_LENGTH;
#endif
        SetPacketParamsLoRa(PreambleLength, packetLengthType, _PayloadLength, InvertIQ);
    }
    SetFrequencyReg(regfreq);
    SetRxTimeoutUs(interval);

    uint8_t dio1Mask = SX1280_IRQ_TX_DONE | SX1280_IRQ_RX_DONE;
    uint8_t irqMask  = SX1280_IRQ_TX_DONE | SX1280_IRQ_RX_DONE | SX1280_IRQ_SYNCWORD_VALID | SX1280_IRQ_SYNCWORD_ERROR | SX1280_IRQ_CRC_ERROR;
    SetDioIrqParams(irqMask, dio1Mask);
}

void SX1280Driver::SetRxTimeoutUs(uint32_t interval)
{
    if (interval)
    {
        timeout = interval * 1000 / RX_TIMEOUT_PERIOD_BASE_NANOS; // number of periods for the SX1280 to timeout
    }
    else
    {
        timeout = 0xFFFF;   // no timeout, continuous mode
    }
}

/***
 * @brief: Schedule an output power change after the next transmit
 ***/
void SX1280Driver::SetOutputPower(int8_t power)
{
    uint8_t pwrNew = constrain(power, SX1280_POWER_MIN, SX1280_POWER_MAX) + (-SX1280_POWER_MIN);

    if ((pwrPending == PWRPENDING_NONE && pwrCurrent != pwrNew) || pwrPending != pwrNew)
    {
        pwrPending = pwrNew;
        DBGLN("SetPower: %u", pwrPending);
    }
}

void ICACHE_RAM_ATTR SX1280Driver::CommitOutputPower()
{
    if (pwrPending == PWRPENDING_NONE)
        return;

    pwrCurrent = pwrPending;
    pwrPending = PWRPENDING_NONE;
    uint8_t buf[2] = { pwrCurrent, (uint8_t)SX1280_RADIO_RAMP_04_US };
    hal.WriteCommand(SX1280_RADIO_SET_TXPARAMS, buf, sizeof(buf), SX12XX_Radio_All);
}

void SX1280Driver::SetMode(SX1280_RadioOperatingModes_t OPmode, SX12XX_Radio_Number_t radioNumber)
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

    case SX1280_MODE_SLEEP:
        hal.WriteCommand(SX1280_RADIO_SET_SLEEP, (uint8_t)0x01, radioNumber);
        break;

    case SX1280_MODE_CALIBRATION:
        break;

    case SX1280_MODE_STDBY_RC:
        hal.WriteCommand(SX1280_RADIO_SET_STANDBY, SX1280_STDBY_RC, radioNumber, 1500);
        break;

    // The DC-DC supply regulation is automatically powered in STDBY_XOSC mode.
    case SX1280_MODE_STDBY_XOSC:
        hal.WriteCommand(SX1280_RADIO_SET_STANDBY, SX1280_STDBY_XOSC, radioNumber, 50);
        break;

    case SX1280_MODE_FS:
        hal.WriteCommand(SX1280_RADIO_SET_FS, (uint8_t)0x00, radioNumber, 70);
        break;

    case SX1280_MODE_RX:
        buf[0] = RX_TIMEOUT_PERIOD_BASE;
        buf[1] = timeout >> 8;
        buf[2] = timeout & 0xFF;
        hal.WriteCommand(SX1280_RADIO_SET_RX, buf, sizeof(buf), radioNumber, 100);
        break;

    case SX1280_MODE_RX_CONT:
        buf[0] = RX_TIMEOUT_PERIOD_BASE;
        buf[1] = 0xFFFF >> 8;
        buf[2] = 0xFFFF & 0xFF;
        hal.WriteCommand(SX1280_RADIO_SET_RX, buf, sizeof(buf), radioNumber, 100);
        break;        

    case SX1280_MODE_TX:
        //uses timeout Time-out duration = periodBase * periodBaseCount
        buf[0] = RX_TIMEOUT_PERIOD_BASE;
        buf[1] = 0xFF; // no timeout set for now
        buf[2] = 0xFF; // TODO dynamic timeout based on expected onairtime
        hal.WriteCommand(SX1280_RADIO_SET_TX, buf, sizeof(buf), radioNumber, 100);
        break;

    case SX1280_MODE_CAD:
        break;

    default:
        break;
    }

    currOpmode = OPmode;
}

void SX1280Driver::ConfigModParamsLoRa(uint8_t bw, uint8_t sf, uint8_t cr)
{
    // Care must therefore be taken to ensure that modulation parameters are set using the command
    // SetModulationParam() only after defining the packet type SetPacketType() to be used

    WORD_ALIGNED_ATTR uint8_t rfparams[3] = {sf, bw, cr};

    hal.WriteCommand(SX1280_RADIO_SET_MODULATIONPARAMS, rfparams, sizeof(rfparams), SX12XX_Radio_All, 25);

    switch (sf)
    {
    case SX1280_LORA_SF5:
    case SX1280_LORA_SF6:
        hal.WriteRegister(SX1280_REG_SF_ADDITIONAL_CONFIG, 0x1E, SX12XX_Radio_All); // for SF5 or SF6
        break;
    case SX1280_LORA_SF7:
    case SX1280_LORA_SF8:
        hal.WriteRegister(SX1280_REG_SF_ADDITIONAL_CONFIG, 0x37, SX12XX_Radio_All); // for SF7 or SF8
        break;
    default:
        hal.WriteRegister(SX1280_REG_SF_ADDITIONAL_CONFIG, 0x32, SX12XX_Radio_All); // for SF9, SF10, SF11, SF12
    }
    // Datasheet in LoRa Operation says "After SetModulationParams command:
    // In all cases 0x1 must be written to the Frequency Error Compensation mode register 0x093C"
    // However, this causes CRC errors for SF9 when using a high deviation TX (145kHz) and not using Explicit Header mode.
    // The default register value (0x1b) seems most compatible, so don't mess with it
    // InvertIQ=0 0x00=No reception 0x01=Poor reception w/o Explicit Header 0x02=OK 0x03=OK
    // InvertIQ=1 0x00, 0x01, 0x02, and 0x03=Poor reception w/o Explicit Header
    // hal.WriteRegister(SX1280_REG_FREQ_ERR_CORRECTION, 0x03, SX12XX_Radio_All);
}

void SX1280Driver::SetPacketParamsLoRa(uint8_t PreambleLength, SX1280_RadioLoRaPacketLengthsModes_t HeaderType,
                                       uint8_t PayloadLength, uint8_t InvertIQ)
{
    uint8_t buf[7];

    buf[0] = PreambleLength;
    buf[1] = HeaderType;
    buf[2] = PayloadLength;
    buf[3] = SX1280_LORA_CRC_OFF;
    buf[4] = InvertIQ ? SX1280_LORA_IQ_INVERTED : SX1280_LORA_IQ_NORMAL;
    buf[5] = 0x00;
    buf[6] = 0x00;

    hal.WriteCommand(SX1280_RADIO_SET_PACKETPARAMS, buf, sizeof(buf), SX12XX_Radio_All, 20);

    // FEI only triggers in Lora mode when the header is present :(
    modeSupportsFei = HeaderType == SX1280_LORA_PACKET_VARIABLE_LENGTH;
}

void SX1280Driver::ConfigModParamsFLRC(uint8_t bw, uint8_t cr, uint8_t bt)
{
    WORD_ALIGNED_ATTR uint8_t rfparams[3] = {bw, cr, bt};
    hal.WriteCommand(SX1280_RADIO_SET_MODULATIONPARAMS, rfparams, sizeof(rfparams), SX12XX_Radio_All, 110);
}

void SX1280Driver::SetPacketParamsFLRC(uint8_t HeaderType,
                                       uint8_t PreambleLength,
                                       uint8_t PayloadLength,
                                       uint32_t syncWord,
                                       uint16_t crcSeed,
                                       uint8_t cr)
{
    if (PreambleLength < 8)
        PreambleLength = 8;
    PreambleLength = ((PreambleLength / 4) - 1) << 4;

    uint8_t buf[7];
    buf[0] = PreambleLength;                    // AGCPreambleLength
    buf[1] = SX1280_FLRC_SYNC_WORD_LEN_P32S;    // SyncWordLength
    buf[2] = SX1280_FLRC_RX_MATCH_SYNC_WORD_1;  // SyncWordMatch
    buf[3] = HeaderType;                        // PacketType
    buf[4] = PayloadLength;                     // PayloadLength
    buf[5] = SX1280_FLRC_CRC_3_BYTE;            // CrcLength
    buf[6] = 0x08;                              // Must be whitening disabled
    hal.WriteCommand(SX1280_RADIO_SET_PACKETPARAMS, buf, sizeof(buf), SX12XX_Radio_All, 30);

    // CRC seed (use dedicated cipher)
    buf[0] = (uint8_t)(crcSeed >> 8);
    buf[1] = (uint8_t)crcSeed;
    hal.WriteRegister(SX1280_REG_FLRC_CRC_SEED, buf, 2, SX12XX_Radio_All);

    // Set SyncWord1
    buf[0] = (uint8_t)(syncWord >> 24);
    buf[1] = (uint8_t)(syncWord >> 16);
    buf[2] = (uint8_t)(syncWord >> 8);
    buf[3] = (uint8_t)syncWord;

    // DS_SX1280-1_V3.2.pdf - 16.4 FLRC Modem: Increased PER in FLRC Packets with Synch Word
    if (((cr == SX1280_FLRC_CR_1_2) || (cr == SX1280_FLRC_CR_3_4)) &&
        ((buf[0] == 0x8C && buf[1] == 0x38) || (buf[0] == 0x63 && buf[1] == 0x0E)))
    {
        uint8_t temp = buf[0];
        buf[0] = buf[1];
        buf[1] = temp;
        // For SX1280_FLRC_CR_3_4 the datasheet also says
        // "In addition to this the two LSB values XX XX must not be in the range 0x0000 to 0x3EFF"
        if (cr == SX1280_FLRC_CR_3_4 && buf[3] <= 0x3e)
            buf[3] |= 0x80; // 0x80 or 0x40 would work
    }

    hal.WriteRegister(SX1280_REG_FLRC_SYNC_WORD, buf, 4, SX12XX_Radio_All);

    // FEI only works in Lora and Ranging mode
    modeSupportsFei = false;
}

void ICACHE_RAM_ATTR SX1280Driver::SetFrequencyHz(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{
    uint32_t regfreq = (uint32_t)((double)freq / (double)FREQ_STEP);

    SetFrequencyReg(regfreq, radioNumber);
}

void ICACHE_RAM_ATTR SX1280Driver::SetFrequencyReg(uint32_t regfreq, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[3] = {0};

    buf[0] = (uint8_t)((regfreq >> 16) & 0xFF);
    buf[1] = (uint8_t)((regfreq >> 8) & 0xFF);
    buf[2] = (uint8_t)(regfreq & 0xFF);

    hal.WriteCommand(SX1280_RADIO_SET_RFFREQUENCY, buf, sizeof(buf), radioNumber);

    currFreq = regfreq;
}

void SX1280Driver::SetFIFOaddr(uint8_t txBaseAddr, uint8_t rxBaseAddr)
{
    uint8_t buf[2];

    buf[0] = txBaseAddr;
    buf[1] = rxBaseAddr;
    hal.WriteCommand(SX1280_RADIO_SET_BUFFERBASEADDRESS, buf, sizeof(buf), SX12XX_Radio_All);
}

void SX1280Driver::SetDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask)
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

    hal.WriteCommand(SX1280_RADIO_SET_DIOIRQPARAMS, buf, sizeof(buf), SX12XX_Radio_All);
}

uint16_t ICACHE_RAM_ATTR SX1280Driver::GetIrqStatus(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status[2];

    hal.ReadCommand(SX1280_RADIO_GET_IRQSTATUS, status, 2, radioNumber);
    return status[0] << 8 | status[1];
}

void ICACHE_RAM_ATTR SX1280Driver::ClearIrqStatus(uint16_t irqMask, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(((uint16_t)irqMask >> 8) & 0x00FF);
    buf[1] = (uint8_t)((uint16_t)irqMask & 0x00FF);

    hal.WriteCommand(SX1280_RADIO_CLR_IRQSTATUS, buf, sizeof(buf), radioNumber);
}

void ICACHE_RAM_ATTR SX1280Driver::TXnbISR()
{
    currOpmode = SX1280_MODE_FS; // radio goes to FS after TX
#ifdef DEBUG_SX1280_OTA_TIMING
    endTX = micros();
    DBGLN("TOA: %d", endTX - beginTX);
#endif
    CommitOutputPower();
    TXdoneCallback();
}

void ICACHE_RAM_ATTR SX1280Driver::TXnb(uint8_t * data, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    if (currOpmode == SX1280_MODE_TX) //catch TX timeout
    {
        DBGLN("Timeout!");
        SetMode(SX1280_MODE_FS, SX12XX_Radio_All);
        ClearIrqStatus(SX1280_IRQ_RADIO_ALL, SX12XX_Radio_All);
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
            instance->SetMode(SX1280_MODE_FS, SX12XX_Radio_2);
        }
        else
        {
            instance->SetMode(SX1280_MODE_FS, SX12XX_Radio_1);
        }
    }

    RFAMP.TXenable(radioNumber); // do first to allow PA stablise
    hal.WriteBuffer(0x00, data, size, radioNumber); //todo fix offset to equal fifo addr
    instance->SetMode(SX1280_MODE_TX, radioNumber);

#ifdef DEBUG_SX1280_OTA_TIMING
    beginTX = micros();
#endif
}

bool ICACHE_RAM_ATTR SX1280Driver::RXnbISR(uint16_t irqStatus, SX12XX_Radio_Number_t radioNumber)
{
    // In continuous receive mode, the device stays in Rx mode
    if (timeout != 0xFFFF)
    {
        // From table 11-28, pg 81 datasheet rev 3.2
        // upon successsful receipt, when the timer is active or in single mode, it returns to STDBY_RC
        // but because we have AUTO_FS enabled we automatically transition to state SX1280_MODE_FS
        currOpmode = SX1280_MODE_FS;
    }

    rx_status fail = SX12XX_RX_OK;
    // The SYNCWORD_VALID bit isn't set on LoRa, it has no synch (sic) word, and CRC is only on for FLRC
    if (packet_mode == SX1280_PACKET_TYPE_FLRC)
    {
        fail = ((irqStatus & SX1280_IRQ_CRC_ERROR) ? SX12XX_RX_CRC_FAIL : SX12XX_RX_OK) |
               ((irqStatus & SX1280_IRQ_SYNCWORD_VALID) ? SX12XX_RX_OK : SX12XX_RX_SYNCWORD_ERROR) |
               ((irqStatus & SX1280_IRQ_SYNCWORD_ERROR) ? SX12XX_RX_SYNCWORD_ERROR : SX12XX_RX_OK);
    }
    if (fail == SX12XX_RX_OK)
    {
        uint8_t const FIFOaddr = GetRxBufferAddr(radioNumber);
        hal.ReadBuffer(FIFOaddr, RXdataBuffer, PayloadLength, radioNumber);
    }

    return RXdoneCallback(fail);
}

void ICACHE_RAM_ATTR SX1280Driver::RXnb(SX1280_RadioOperatingModes_t rxMode)
{
    RFAMP.RXenable();
    SetMode(rxMode, SX12XX_Radio_All);
}

uint8_t ICACHE_RAM_ATTR SX1280Driver::GetRxBufferAddr(SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t status[2] = {0};
    hal.ReadCommand(SX1280_RADIO_GET_RXBUFFERSTATUS, status, 2, radioNumber);
    return status[1];
}

void ICACHE_RAM_ATTR SX1280Driver::GetStatus(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status = 0;
    hal.ReadCommand(SX1280_RADIO_GET_STATUS, (uint8_t *)&status, 1, radioNumber);
    DBGLN("Status: %x, %x, %x", (0b11100000 & status) >> 5, (0b00011100 & status) >> 2, 0b00000001 & status);
}

bool ICACHE_RAM_ATTR SX1280Driver::GetFrequencyErrorbool()
{
    // Only need the highest bit of the 20-bit FEI to determine the direction
    uint8_t feiMsb = hal.ReadRegister(SX1280_REG_LR_ESTIMATED_FREQUENCY_ERROR_MSB, lastSuccessfulPacketRadio);
    // fei & (1 << 19) and flip sign if IQinverted
    if (feiMsb & 0x08)
        return IQinverted;
    else
        return !IQinverted;
}

int8_t ICACHE_RAM_ATTR SX1280Driver::GetRssiInst(SX12XX_Radio_Number_t radioNumber)
{
    uint8_t status = 0;

    hal.ReadCommand(SX1280_RADIO_GET_RSSIINST, (uint8_t *)&status, 1, radioNumber);
    return -(int8_t)(status / 2);
}

void ICACHE_RAM_ATTR SX1280Driver::GetLastPacketStats()
{
    uint8_t status[2];
    hal.ReadCommand(SX1280_RADIO_GET_PACKETSTATUS, status, 2, processingPacketRadio);
    if (packet_mode == SX1280_PACKET_TYPE_FLRC) {
        // No SNR in FLRC mode
        LastPacketRSSI = -(int8_t)(status[1] / 2);
        LastPacketSNRRaw = 0;
        return;
    }
    // LoRa mode has both RSSI and SNR
    LastPacketRSSI = -(int8_t)(status[0] / 2);
    LastPacketSNRRaw = (int8_t)status[1];
    // https://www.mouser.com/datasheet/2/761/DS_SX1280-1_V2.2-1511144.pdf p84
    // need to subtract SNR from RSSI when SNR <= 0;
    int8_t negOffset = (LastPacketSNRRaw < 0) ? (LastPacketSNRRaw / RADIO_SNR_SCALE) : 0;
    LastPacketRSSI += negOffset;
}

void ICACHE_RAM_ATTR SX1280Driver::IsrCallback_1()
{
    instance->IsrCallback(SX12XX_Radio_1);
}

void ICACHE_RAM_ATTR SX1280Driver::IsrCallback_2()
{
    instance->IsrCallback(SX12XX_Radio_2);
}

void ICACHE_RAM_ATTR SX1280Driver::IsrCallback(SX12XX_Radio_Number_t radioNumber)
{
    instance->processingPacketRadio = radioNumber;
    SX12XX_Radio_Number_t irqClearRadio = radioNumber;

    uint16_t irqStatus = instance->GetIrqStatus(radioNumber);
    if (irqStatus & SX1280_IRQ_TX_DONE)
    {
        RFAMP.TXRXdisable();
        instance->TXnbISR();
        irqClearRadio = SX12XX_Radio_All;
    }
    else if (irqStatus & SX1280_IRQ_RX_DONE)
    {
        if (instance->RXnbISR(irqStatus, radioNumber))
        {
            instance->lastSuccessfulPacketRadio = radioNumber;
            irqClearRadio = SX12XX_Radio_All; // Packet received so clear all radios and dont spend extra time retrieving data.
        }
    }
    else if (irqStatus == SX1280_IRQ_RADIO_NONE)
    {
        return;
    }
    instance->ClearIrqStatus(SX1280_IRQ_RADIO_ALL, irqClearRadio);
}
