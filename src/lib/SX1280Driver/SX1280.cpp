#include "SX1280_Regs.h"
#include "SX1280_hal.h"
#include "SX1280.h"
#include "logging.h"

SX1280Hal hal;
SX1280Driver *SX1280Driver::instance = NULL;

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
}

void SX1280Driver::End()
{
    SetMode(SX1280_MODE_SLEEP);
    hal.end();
    RemoveCallbacks();
    currFreq = 2400000000;
    PayloadLength = 8; // Dummy default value which is overwritten during setup.
}

bool SX1280Driver::Begin()
{
    hal.init();
    hal.IsrCallback = &SX1280Driver::IsrCallback;

    hal.reset();
    DBGLN("SX1280 Begin");
    delay(100);
    uint16_t firmwareRev = (((hal.ReadRegister(REG_LR_FIRMWARE_VERSION_MSB)) << 8) | (hal.ReadRegister(REG_LR_FIRMWARE_VERSION_MSB + 1)));
    DBGLN("Read Vers: %d", firmwareRev);
    if ((firmwareRev == 0) || (firmwareRev == 65535))
    {
        // SPI communication failed, just return without configuration
        return false;
    }

    SetMode(SX1280_MODE_STDBY_RC);                                                                                                //Put in STDBY_RC mode
    hal.WriteCommand(SX1280_RADIO_SET_PACKETTYPE, SX1280_PACKET_TYPE_LORA);                                                       //Set packet type to LoRa
    ConfigModParamsLoRa(SX1280_LORA_BW_0800, SX1280_LORA_SF6, SX1280_LORA_CR_4_7);                                                //Configure Modulation Params
    hal.WriteCommand(SX1280_RADIO_SET_AUTOFS, 0x01);                                                                              //Enable auto FS
    hal.WriteRegister(0x0891, (hal.ReadRegister(0x0891) | 0xC0));                                                                 //default is low power mode, switch to high sensitivity instead
    SetPacketParamsLoRa(12, SX1280_LORA_PACKET_IMPLICIT, 8, SX1280_LORA_CRC_OFF, SX1280_LORA_IQ_NORMAL);                          //default params
    SetFrequencyReg(currFreq);                                                                                                    //Set Freq
    SetFIFOaddr(0x00, 0x00);                                                                                                      //Config FIFO addr
    SetDioIrqParams(SX1280_IRQ_RADIO_ALL, SX1280_IRQ_TX_DONE | SX1280_IRQ_RX_DONE);                                               //set IRQ to both RXdone/TXdone on DIO1
    if (OPT_USE_SX1280_DCDC)
    {
        hal.WriteCommand(SX1280_RADIO_SET_REGULATORMODE, SX1280_USE_DCDC);     // Enable DCDC converter instead of LDO
    }
    return true;
}

void SX1280Driver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq,
                          uint8_t PreambleLength, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval,
                          uint32_t flrcSyncWord, uint16_t flrcCrcSeed, uint8_t flrc)
{
    uint8_t irqs = SX1280_IRQ_TX_DONE | SX1280_IRQ_RX_DONE;
    uint8_t const mode = (flrc) ? SX1280_PACKET_TYPE_FLRC : SX1280_PACKET_TYPE_LORA;

    PayloadLength = _PayloadLength;
    IQinverted = InvertIQ;
    packet_mode = mode;
    SetMode(SX1280_MODE_STDBY_XOSC);
    hal.WriteCommand(SX1280_RADIO_SET_PACKETTYPE, mode, 20);
    if (mode == SX1280_PACKET_TYPE_FLRC)
    {
        DBGLN("Config FLRC");
        ConfigModParamsFLRC(bw, cr, sf);
        SetPacketParamsFLRC(SX1280_FLRC_PACKET_FIXED_LENGTH, /*crc=*/1,
                            PreambleLength, _PayloadLength, flrcSyncWord, flrcCrcSeed);
        irqs |= SX1280_IRQ_CRC_ERROR;
    }
    else
    {
        DBGLN("Config LoRa");
        ConfigModParamsLoRa(bw, sf, cr);
#if defined(DEBUG_FREQ_CORRECTION)
        SX1280_RadioLoRaPacketLengthsModes_t packetLengthType = SX1280_LORA_PACKET_VARIABLE_LENGTH;
#else
        SX1280_RadioLoRaPacketLengthsModes_t packetLengthType = SX1280_LORA_PACKET_FIXED_LENGTH;
#endif
        SetPacketParamsLoRa(PreambleLength, packetLengthType,
                            _PayloadLength, SX1280_LORA_CRC_OFF, InvertIQ);
    }
    SetFrequencyReg(freq);
    SetDioIrqParams(SX1280_IRQ_RADIO_ALL, irqs);
    SetRxTimeoutUs(interval);
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

void SX1280Driver::SetOutputPower(int8_t power)
{
    if (power < -18) power = -18;
    else if (13 < power) power = 13;
    uint8_t buf[2] = {(uint8_t)(power + 18), (uint8_t)SX1280_RADIO_RAMP_04_US};
    hal.WriteCommand(SX1280_RADIO_SET_TXPARAMS, buf, sizeof(buf));
    DBGLN("SetPower: %d", buf[0]);
    return;
}

void SX1280Driver::SetMode(SX1280_RadioOperatingModes_t OPmode)
{
    if (OPmode == currOpmode)
    {
       return;
    }

    WORD_ALIGNED_ATTR uint8_t buf[3];
    switch (OPmode)
    {

    case SX1280_MODE_SLEEP:
        hal.WriteCommand(SX1280_RADIO_SET_SLEEP, (uint8_t)0x01);
        break;

    case SX1280_MODE_CALIBRATION:
        break;

    case SX1280_MODE_STDBY_RC:
        hal.WriteCommand(SX1280_RADIO_SET_STANDBY, SX1280_STDBY_RC, 1500);
        break;

    case SX1280_MODE_STDBY_XOSC:
        hal.WriteCommand(SX1280_RADIO_SET_STANDBY, SX1280_STDBY_XOSC, 50);
        break;

    case SX1280_MODE_FS:
        hal.WriteCommand(SX1280_RADIO_SET_FS, (uint8_t)0x00, 70);
        break;

    case SX1280_MODE_RX:
        buf[0] = RX_TIMEOUT_PERIOD_BASE;
        buf[1] = timeout >> 8;
        buf[2] = timeout & 0xFF;
        hal.WriteCommand(SX1280_RADIO_SET_RX, buf, sizeof(buf), 100);
        break;

    case SX1280_MODE_TX:
        //uses timeout Time-out duration = periodBase * periodBaseCount
        buf[0] = RX_TIMEOUT_PERIOD_BASE;
        buf[1] = 0xFF; // no timeout set for now
        buf[2] = 0xFF; // TODO dynamic timeout based on expected onairtime
        hal.WriteCommand(SX1280_RADIO_SET_TX, buf, sizeof(buf), 100);
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

    hal.WriteCommand(SX1280_RADIO_SET_MODULATIONPARAMS, rfparams, sizeof(rfparams), 25);

    switch (sf)
    {
    case SX1280_LORA_SF5:
    case SX1280_LORA_SF6:
        hal.WriteRegister(SX1280_REG_SF_ADDITIONAL_CONFIG, 0x1E); // for SF5 or SF6
        break;
    case SX1280_LORA_SF7:
    case SX1280_LORA_SF8:
        hal.WriteRegister(SX1280_REG_SF_ADDITIONAL_CONFIG, 0x37); // for SF7 or SF8
        break;
    default:
        hal.WriteRegister(SX1280_REG_SF_ADDITIONAL_CONFIG, 0x32); // for SF9, SF10, SF11, SF12
    }
    // Enable frequency compensation
    hal.WriteRegister(SX1280_REG_FREQ_ERR_CORRECTION, 0x1);
}

void SX1280Driver::SetPacketParamsLoRa(uint8_t PreambleLength, SX1280_RadioLoRaPacketLengthsModes_t HeaderType,
                                       uint8_t PayloadLength, SX1280_RadioLoRaCrcModes_t crc,
                                       uint8_t InvertIQ)
{
    uint8_t buf[7];

    buf[0] = PreambleLength;
    buf[1] = HeaderType;
    buf[2] = PayloadLength;
    buf[3] = crc;
    buf[4] = InvertIQ ? SX1280_LORA_IQ_INVERTED : SX1280_LORA_IQ_NORMAL;
    buf[5] = 0x00;
    buf[6] = 0x00;

    hal.WriteCommand(SX1280_RADIO_SET_PACKETPARAMS, buf, sizeof(buf), 20);

    // FEI only triggers in Lora mode when the header is present :(
    modeSupportsFei = HeaderType == SX1280_LORA_PACKET_VARIABLE_LENGTH;
}

void SX1280Driver::ConfigModParamsFLRC(uint8_t bw, uint8_t cr, uint8_t bt)
{
    WORD_ALIGNED_ATTR uint8_t rfparams[3] = {bw, cr, bt};
    hal.WriteCommand(SX1280_RADIO_SET_MODULATIONPARAMS, rfparams, sizeof(rfparams), 110);
}

void SX1280Driver::SetPacketParamsFLRC(uint8_t HeaderType,
                                       uint8_t crc,
                                       uint8_t PreambleLength,
                                       uint8_t PayloadLength,
                                       uint32_t syncWord,
                                       uint16_t crcSeed)
{
    if (PreambleLength < 8)
        PreambleLength = 8;
    PreambleLength = ((PreambleLength / 4) - 1) << 4;
    crc = (crc) ? SX1280_FLRC_CRC_2_BYTE : SX1280_FLRC_CRC_OFF;

    uint8_t buf[7];
    buf[0] = PreambleLength;                    // AGCPreambleLength
    buf[1] = SX1280_FLRC_SYNC_WORD_LEN_P32S;    // SyncWordLength
    buf[2] = SX1280_FLRC_RX_MATCH_SYNC_WORD_1;  // SyncWordMatch
    buf[3] = HeaderType;                        // PacketType
    buf[4] = PayloadLength;                     // PayloadLength
    buf[5] = (crc << 4);                        // CrcLength
    buf[6] = 0x08;                              // Must be whitening disabled
    hal.WriteCommand(SX1280_RADIO_SET_PACKETPARAMS, buf, sizeof(buf), 30);

    // CRC seed (use dedicated cipher)
    buf[0] = (uint8_t)(crcSeed >> 8);
    buf[1] = (uint8_t)crcSeed;
    hal.WriteRegister(SX1280_REG_FLRC_CRC_SEED, buf, 2);

    // CRC POLY 0x3D65
    buf[0] = 0x3D;
    buf[1] = 0x65;
    hal.WriteRegister(SX1280_REG_FLRC_CRC_POLY, buf, 2);

    // Set SyncWord1
    buf[0] = (uint8_t)(syncWord >> 24);
    buf[1] = (uint8_t)(syncWord >> 16);
    buf[2] = (uint8_t)(syncWord >> 8);
    buf[3] = (uint8_t)syncWord;
    hal.WriteRegister(SX1280_REG_FLRC_SYNC_WORD, buf, 4);

    // FEI only works in Lora and Ranging mode
    modeSupportsFei = false;
}

void ICACHE_RAM_ATTR SX1280Driver::SetFrequencyHz(uint32_t Reqfreq)
{
    WORD_ALIGNED_ATTR uint8_t buf[3] = {0};

    uint32_t freq = (uint32_t)((double)Reqfreq / (double)FREQ_STEP);
    buf[0] = (uint8_t)((freq >> 16) & 0xFF);
    buf[1] = (uint8_t)((freq >> 8) & 0xFF);
    buf[2] = (uint8_t)(freq & 0xFF);

    hal.WriteCommand(SX1280_RADIO_SET_RFFREQUENCY, buf, sizeof(buf));
    currFreq = Reqfreq;
}

void ICACHE_RAM_ATTR SX1280Driver::SetFrequencyReg(uint32_t freq)
{
    WORD_ALIGNED_ATTR uint8_t buf[3] = {0};

    buf[0] = (uint8_t)((freq >> 16) & 0xFF);
    buf[1] = (uint8_t)((freq >> 8) & 0xFF);
    buf[2] = (uint8_t)(freq & 0xFF);

    hal.WriteCommand(SX1280_RADIO_SET_RFFREQUENCY, buf, sizeof(buf));
    currFreq = freq;
}

void SX1280Driver::SetFIFOaddr(uint8_t txBaseAddr, uint8_t rxBaseAddr)
{
    uint8_t buf[2];

    buf[0] = txBaseAddr;
    buf[1] = rxBaseAddr;
    hal.WriteCommand(SX1280_RADIO_SET_BUFFERBASEADDRESS, buf, sizeof(buf));
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

    hal.WriteCommand(SX1280_RADIO_SET_DIOIRQPARAMS, buf, sizeof(buf));
}

uint16_t ICACHE_RAM_ATTR SX1280Driver::GetIrqStatus()
{
    uint8_t status[2];

    hal.ReadCommand(SX1280_RADIO_GET_IRQSTATUS, status, 2);
    return status[0] << 8 | status[1];
}

void ICACHE_RAM_ATTR SX1280Driver::ClearIrqStatus(uint16_t irqMask)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(((uint16_t)irqMask >> 8) & 0x00FF);
    buf[1] = (uint8_t)((uint16_t)irqMask & 0x00FF);

    hal.WriteCommand(SX1280_RADIO_CLR_IRQSTATUS, buf, sizeof(buf));
}

void ICACHE_RAM_ATTR SX1280Driver::TXnbISR()
{
    currOpmode = SX1280_MODE_FS; // radio goes to FS after TX
#ifdef DEBUG_SX1280_OTA_TIMING
    endTX = micros();
    DBGLN("TOA: %d", endTX - beginTX);
#endif
    TXdoneCallback();
}

uint8_t FIFOaddr = 0;

void ICACHE_RAM_ATTR SX1280Driver::TXnb()
{
    if (currOpmode == SX1280_MODE_TX) //catch TX timeout
    {
        //DBGLN("Timeout!");
        SetMode(SX1280_MODE_FS);
        TXnbISR();
        return;
    }
    hal.TXenable();                      // do first to allow PA stablise
    hal.WriteBuffer(0x00, TXdataBuffer, PayloadLength); //todo fix offset to equal fifo addr
    instance->SetMode(SX1280_MODE_TX);
#ifdef DEBUG_SX1280_OTA_TIMING
    beginTX = micros();
#endif
}

void ICACHE_RAM_ATTR SX1280Driver::RXnbISR(uint16_t const irqStatus)
{
    rx_status const fail =
        ((irqStatus & SX1280_IRQ_CRC_ERROR) ? SX12XX_RX_CRC_FAIL : SX12XX_RX_OK) +
        ((irqStatus & SX1280_IRQ_RX_TX_TIMEOUT) ? SX12XX_RX_TIMEOUT : SX12XX_RX_OK);
    // In continuous receive mode, the device stays in Rx mode
    if (timeout != 0xFFFF)
    {
        // From table 11-28, pg 81 datasheet rev 3.2
        // upon successsful receipt, when the timer is active or in single mode, it returns to STDBY_RC
        // but because we have AUTO_FS enabled we automatically transition to state SX1280_MODE_FS
        currOpmode = SX1280_MODE_FS;
    }
    if (fail == SX12XX_RX_OK)
    {
        uint8_t const FIFOaddr = GetRxBufferAddr();
        hal.ReadBuffer(FIFOaddr, RXdataBuffer, PayloadLength);
        GetLastPacketStats();
    }
    RXdoneCallback(fail);
}

void ICACHE_RAM_ATTR SX1280Driver::RXnb()
{
    hal.RXenable();
    SetMode(SX1280_MODE_RX);
}

uint8_t ICACHE_RAM_ATTR SX1280Driver::GetRxBufferAddr()
{
    WORD_ALIGNED_ATTR uint8_t status[2] = {0};
    hal.ReadCommand(SX1280_RADIO_GET_RXBUFFERSTATUS, status, 2);
    return status[1];
}

void ICACHE_RAM_ATTR SX1280Driver::GetStatus()
{
    uint8_t status = 0;
    hal.ReadCommand(SX1280_RADIO_GET_STATUS, (uint8_t *)&status, 1);
    DBGLN("Status: %x, %x, %x", (0b11100000 & status) >> 5, (0b00011100 & status) >> 2, 0b00000001 & status);
}

bool ICACHE_RAM_ATTR SX1280Driver::GetFrequencyErrorbool()
{
    // Only need the highest bit of the 20-bit FEI to determine the direction
    uint8_t feiMsb = hal.ReadRegister(SX1280_REG_LR_ESTIMATED_FREQUENCY_ERROR_MSB);
    // fei & (1 << 19) and flip sign if IQinverted
    if (feiMsb & 0x08)
        return IQinverted;
    else
        return !IQinverted;
}

int8_t ICACHE_RAM_ATTR SX1280Driver::GetRssiInst()
{
    uint8_t status = 0;

    hal.ReadCommand(SX1280_RADIO_GET_RSSIINST, (uint8_t *)&status, 1);
    return -(int8_t)(status / 2);
}

void ICACHE_RAM_ATTR SX1280Driver::GetLastPacketStats()
{
    uint8_t status[2];
    hal.ReadCommand(SX1280_RADIO_GET_PACKETSTATUS, status, 2);
    if (packet_mode == SX1280_PACKET_TYPE_FLRC) {
        // No SNR in FLRC mode
        LastPacketRSSI = -(int8_t)(status[1] / 2);
        LastPacketSNR = 0;
        return;
    }
    // LoRa mode has both RSSI and SNR
    LastPacketRSSI = -(int8_t)(status[0] / 2);
    LastPacketSNR = (int8_t)status[1] / 4;
    // https://www.mouser.com/datasheet/2/761/DS_SX1280-1_V2.2-1511144.pdf
    // need to subtract SNR from RSSI when SNR <= 0;
    int8_t negOffset = (LastPacketSNR < 0) ? LastPacketSNR : 0;
    LastPacketRSSI += negOffset;
}

void ICACHE_RAM_ATTR SX1280Driver::IsrCallback()
{
    uint16_t irqStatus = instance->GetIrqStatus();
    instance->ClearIrqStatus(SX1280_IRQ_RADIO_ALL);
    if (irqStatus & SX1280_IRQ_TX_DONE)
    {
        hal.TXRXdisable();
        instance->TXnbISR();
    }
    else if (irqStatus & (SX1280_IRQ_RX_DONE | SX1280_IRQ_CRC_ERROR | SX1280_IRQ_RX_TX_TIMEOUT))
        instance->RXnbISR(irqStatus);
}
