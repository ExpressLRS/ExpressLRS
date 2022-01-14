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

void ICACHE_RAM_ATTR SX1280Driver::nullCallback(void) {}

SX1280Driver::SX1280Driver()
{
    instance = this;
}

void SX1280Driver::End()
{
    SetMode(SX1280_MODE_SLEEP);
    hal.end();
    TXdoneCallback = &nullCallback; // remove callbacks
    RXdoneCallback = &nullCallback;
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
    ConfigLoRaModParams(currBW, currSF, currCR);                                                                                  //Configure Modulation Params
    hal.WriteCommand(SX1280_RADIO_SET_AUTOFS, 0x01);                                                                              //Enable auto FS
    hal.WriteRegister(0x0891, (hal.ReadRegister(0x0891) | 0xC0));                                                                 //default is low power mode, switch to high sensitivity instead
    SetPacketParams(12, SX1280_LORA_PACKET_IMPLICIT, 8, SX1280_LORA_CRC_OFF, SX1280_LORA_IQ_NORMAL);                              //default params
    SetFrequencyReg(currFreq);                                                                                                    //Set Freq
    SetFIFOaddr(0x00, 0x00);                                                                                                      //Config FIFO addr
    SetDioIrqParams(SX1280_IRQ_RADIO_ALL, SX1280_IRQ_TX_DONE | SX1280_IRQ_RX_DONE, SX1280_IRQ_RADIO_NONE, SX1280_IRQ_RADIO_NONE); //set IRQ to both RXdone/TXdone on DIO1
#if defined(USE_SX1280_DCDC)
    hal.WriteCommand(SX1280_RADIO_SET_REGULATORMODE, SX1280_USE_DCDC);     // Enable DCDC converter instead of LDO
#endif
    return true;
}

void SX1280Driver::Config(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr, uint32_t freq, uint8_t PreambleLength, bool InvertIQ, uint8_t PayloadLength)
{
    PayloadLength = PayloadLength;
    IQinverted = InvertIQ;
    SetMode(SX1280_MODE_STDBY_XOSC);
    ConfigLoRaModParams(bw, sf, cr);
    SetPacketParams(PreambleLength, SX1280_LORA_PACKET_IMPLICIT, PayloadLength, SX1280_LORA_CRC_OFF, (SX1280_RadioLoRaIQModes_t)((uint8_t)!IQinverted << 6)); // TODO don't make static etc. LORA_IQ_STD = 0x40, LORA_IQ_INVERTED = 0x00
    SetFrequencyReg(freq);
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

void SX1280Driver::SetPacketParams(uint8_t PreambleLength, SX1280_RadioLoRaPacketLengthsModes_t HeaderType, uint8_t PayloadLength, SX1280_RadioLoRaCrcModes_t crc, SX1280_RadioLoRaIQModes_t InvertIQ)
{
    uint8_t buf[7];

    buf[0] = PreambleLength;
    buf[1] = HeaderType;
    buf[2] = PayloadLength;
    buf[3] = crc;
    buf[4] = InvertIQ;
    buf[5] = 0x00;
    buf[6] = 0x00;

    hal.WriteCommand(SX1280_RADIO_SET_PACKETPARAMS, buf, sizeof(buf));
}

void SX1280Driver::SetMode(SX1280_RadioOperatingModes_t OPmode)
{

    if (OPmode == currOpmode)
    {
       return;
    }

    WORD_ALIGNED_ATTR uint8_t buf[3];
    uint32_t switchDelay = 0;

    switch (OPmode)
    {

    case SX1280_MODE_SLEEP:
        hal.WriteCommand(SX1280_RADIO_SET_SLEEP, 0x01);
        break;

    case SX1280_MODE_CALIBRATION:
        break;

    case SX1280_MODE_STDBY_RC:
        hal.WriteCommand(SX1280_RADIO_SET_STANDBY, SX1280_STDBY_RC);
        switchDelay = 1500;
        break;

    case SX1280_MODE_STDBY_XOSC:
        hal.WriteCommand(SX1280_RADIO_SET_STANDBY, SX1280_STDBY_XOSC);
        switchDelay = 50;
        break;

    case SX1280_MODE_FS:
        hal.WriteCommand(SX1280_RADIO_SET_FS, 0x00);
        switchDelay = 70;
        break;

    case SX1280_MODE_RX:
        buf[0] = 0x00; // periodBase = 1ms, page 71 datasheet, set to FF for cont RX
        buf[1] = 0xFF;
        buf[2] = 0xFF;
        hal.WriteCommand(SX1280_RADIO_SET_RX, buf, sizeof(buf));
        switchDelay = 100;
        break;

    case SX1280_MODE_TX:
        //uses timeout Time-out duration = periodBase * periodBaseCount
        buf[0] = 0x00; // periodBase = 1ms, page 71 datasheet
        buf[1] = 0xFF; // no timeout set for now
        buf[2] = 0xFF; // TODO dynamic timeout based on expected onairtime
        hal.WriteCommand(SX1280_RADIO_SET_TX, buf, sizeof(buf));
        switchDelay = 100;
        break;

    case SX1280_MODE_CAD:
        break;

    default:
        break;
    }
    hal.BusyDelay(switchDelay);

    currOpmode = OPmode;
}

void SX1280Driver::ConfigLoRaModParams(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr)
{
    // Care must therefore be taken to ensure that modulation parameters are set using the command
    // SetModulationParam() only after defining the packet type SetPacketType() to be used

    WORD_ALIGNED_ATTR uint8_t rfparams[3] = {0};

    rfparams[0] = (uint8_t)sf;
    rfparams[1] = (uint8_t)bw;
    rfparams[2] = (uint8_t)cr;

    hal.WriteCommand(SX1280_RADIO_SET_MODULATIONPARAMS, rfparams, sizeof(rfparams));

    switch (sf)
    {
    case SX1280_LORA_SF5:
    case SX1280_LORA_SF6:
        hal.WriteRegister(0x925, 0x1E); // for SF5 or SF6
        break;
    case SX1280_LORA_SF7:
    case SX1280_LORA_SF8:
        hal.WriteRegister(0x925, 0x37); // for SF7 or SF8
        break;
    default:
        hal.WriteRegister(0x925, 0x32); // for SF9, SF10, SF11, SF12
    }
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

int32_t ICACHE_RAM_ATTR SX1280Driver::GetFrequencyError()
{
    WORD_ALIGNED_ATTR uint8_t efeRaw[3] = {0};
    uint32_t efe = 0;
    double efeHz = 0.0;

    efeRaw[0] = hal.ReadRegister(SX1280_REG_LR_ESTIMATED_FREQUENCY_ERROR_MSB);
    efeRaw[1] = hal.ReadRegister(SX1280_REG_LR_ESTIMATED_FREQUENCY_ERROR_MSB + 1);
    efeRaw[2] = hal.ReadRegister(SX1280_REG_LR_ESTIMATED_FREQUENCY_ERROR_MSB + 2);
    efe = (efeRaw[0] << 16) | (efeRaw[1] << 8) | efeRaw[2];

    efe &= SX1280_REG_LR_ESTIMATED_FREQUENCY_ERROR_MASK;

    //efeHz = 1.55 * (double)complement2(efe, 20) / (1600.0 / (double)GetLoRaBandwidth() * 1000.0);
    return efeHz;
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

void ICACHE_RAM_ATTR SX1280Driver::RXnbISR()
{
    // In continuous receive mode, the device stays in Rx mode
    //currOpmode = SX1280_MODE_FS;
    uint8_t FIFOaddr = GetRxBufferAddr();
    hal.ReadBuffer(FIFOaddr, RXdataBuffer, PayloadLength);
    GetLastPacketStats();
    RXdoneCallback();
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
    uint8_t regEFI[3];

    hal.ReadRegister(SX1280_REG_LR_ESTIMATED_FREQUENCY_ERROR_MSB, regEFI, sizeof(regEFI));

    DBGLN("%d %d %d", regEFI[0], regEFI[1], regEFI[2]);

    //bool result = (val & 0b00001000) >> 3;
    //return result; // returns true if pos freq error, neg if false
    return 0;
}

void ICACHE_RAM_ATTR SX1280Driver::GetLastPacketStats()
{
    uint8_t status[2];

    hal.ReadCommand(SX1280_RADIO_GET_PACKETSTATUS, status, 2);
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
    if ((irqStatus & SX1280_IRQ_TX_DONE))
    {
        hal.TXRXdisable();
        instance->TXnbISR();
    }
    else if ((irqStatus & SX1280_IRQ_RX_DONE))
        instance->RXnbISR();
}
