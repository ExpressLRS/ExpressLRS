#include "LoRa_SX1280_Regs.h"
#include "LoRa_SX1280_hal.h"
#include "LoRa_SX1280.h"
#include "LoRa_lowlevel.h"

SX1280Hal hal;
/////////////////////////////////////////////////////////////////

SX1280_RadioLoRaBandwidths_t SX1280Driver::currBW = SX1280_LORA_BW_0800;
SX1280_RadioLoRaSpreadingFactors_t SX1280Driver::currSF = SX1280_LORA_SF10;
SX1280_RadioLoRaCodingRates_t SX1280Driver::currCR = SX1280_LORA_CR_4_5;
uint32_t SX1280Driver::currFreq = 2420000000; //should be mid 2.4 band, todo

SX1280_RadioOperatingModes_t SX1280Driver::currOpmode = SX1280_MODE_SLEEP;

SX1280Driver *SX1280Driver::instance = NULL;
//InterruptAssignment_ InterruptAssignment = NONE;

//uint8_t SX127xDriver::_syncWord = SX127X_SYNC_WORD;

//uint8_t SX127xDriver::currPWR = 0b0000;
//uint8_t SX127xDriver::maxPWR = 0b1111;

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

uint32_t beginTX;
uint32_t endTX;

void ICACHE_RAM_ATTR SX1280Driver::nullCallback(void){};

SX1280Driver::SX1280Driver()
{
    instance = this;
}

void SX1280Driver::Begin()
{
    hal.init();
    hal.TXdoneCallback = &SX1280Driver::TXnbISR;
    hal.RXdoneCallback = &SX1280Driver::RXnbISR;

    hal.reset();
    Serial.println("SX1280 Begin");
    delay(100);

    uint16_t firmwareRev = (((hal.ReadRegister(REG_LR_FIRMWARE_VERSION_MSB)) << 8) | (hal.ReadRegister(REG_LR_FIRMWARE_VERSION_MSB + 1)));
    Serial.print("Firmware Revision: ");
    Serial.println(firmwareRev);

    this->SetMode(SX1280_MODE_STDBY_RC);                                    //step 1 put in STDBY_RC mode
    hal.WriteCommand(SX1280_RADIO_SET_PACKETTYPE, SX1280_PACKET_TYPE_LORA); //Step 2: set packet type to LoRa
    this->ConfigModParams(currBW, currSF, currCR);                          //Step 5: Configure Modulation Params
    this->SetPacketParams(12, SX1280_LORA_PACKET_EXPLICIT, 8, SX1280_LORA_CRC_OFF, SX1280_LORA_IQ_NORMAL);
    this->SetFrequency(this->currFreq); //Step 3: Set Freq
    this->SetFIFOaddr(0x00, 0x00);      //Step 4: Config FIFO addr
    this->SetOutputPower(13);

    this->SetDioIrqParams(SX1280_IRQ_RADIO_ALL, SX1280_IRQ_TX_DONE | SX1280_IRQ_RX_DONE, SX1280_IRQ_RADIO_NONE, SX1280_IRQ_RADIO_NONE); // set DIO1 to trigger on TxDone and RxDone, disable other DIOs

    //hal.WriteCommand(SX1280_RADIO_SET_AUTOFS, 0x01); //enable auto FS

    //this->SetMode(SX1280_MODE_STDBY_XOSC); //step 1 put in STDBY_RC mode
}

void ICACHE_RAM_ATTR SX1280Driver::Config(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr, uint32_t freq, SX1280_RadioPreambleLengths_t PreambleLength)
{
    SetFrequency(freq);
    ConfigModParams(bw, currSF, currCR);
    SetPacketParams(PreambleLength, SX1280_LORA_PACKET_IMPLICIT, 8, SX1280_LORA_CRC_OFF, SX1280_LORA_IQ_NORMAL); // TODO don't make static etc.
}

void ICACHE_RAM_ATTR SX1280Driver::SetOutputPower(int8_t power)
{
    uint8_t buf[2];
    buf[0] = power + 18;
    buf[1] = (uint8_t)SX1280_RADIO_RAMP_04_US;
    hal.WriteCommand(SX1280_RADIO_SET_TXPARAMS, buf, 2);
    return; //TODO impliment function
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

void SX1280Driver::SetPreambleLength(SX1280_RadioPreambleLengths_t preambleLen)
{
}

void SX1280Driver::SetMode(SX1280_RadioOperatingModes_t OPmode)
{

    //if (OPmode == currOpmode)
    //{
    //   return;
    //}

    uint8_t buf3[3]; //TODO make word alignmed

    switch (OPmode)
    {

    case SX1280_MODE_SLEEP:
        hal.WriteCommand(SX1280_RADIO_SET_SLEEP, 0x01);
        break;

    case SX1280_MODE_CALIBRATION:
        break;

    case SX1280_MODE_STDBY_RC:
        hal.WriteCommand(SX1280_RADIO_SET_STANDBY, SX1280_STDBY_RC);
        break;
    case SX1280_MODE_STDBY_XOSC:
        hal.WriteCommand(SX1280_RADIO_SET_STANDBY, SX1280_STDBY_XOSC);
        break;

    case SX1280_MODE_FS:
        hal.WriteCommand(SX1280_RADIO_SET_FS, 0x00);
        break;

    case SX1280_MODE_RX:

        buf3[0] = 0x00; // periodBase = 1ms, page 71 datasheet, set to FF for cont RX
        buf3[1] = 0xFF;
        buf3[2] = 0xFF;
        hal.WriteCommand(SX1280_RADIO_SET_RX, buf3, sizeof(buf3));

        break;

    case SX1280_MODE_TX:

        //uses timeout Time-out duration = periodBase * periodBaseCount
        buf3[0] = 0x00; // periodBase = 1ms, page 71 datasheet
        buf3[1] = 0xFF; // no timeout set for now
        buf3[2] = 0xFF; // TODO dynamic timeout based on expected onairtime

        hal.WriteCommand(SX1280_RADIO_SET_TX, buf3, sizeof(buf3));

        break;

    case SX1280_MODE_CAD:
        break;

    default:
        break;
    }

    currOpmode = OPmode;
}

void SX1280Driver::ConfigModParams(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr)
{
    // Care must therefore be taken to ensure that modulation parameters are set using the command
    // SetModulationParam() only after defining the packet type SetPacketType() to be used

    uint8_t rfparams[3] = {0}; //TODO make word alignmed

    rfparams[0] = (uint8_t)sf;
    rfparams[1] = (uint8_t)bw;
    rfparams[2] = (uint8_t)cr;

    hal.WriteCommand(SX1280_RADIO_SET_MODULATIONPARAMS, rfparams, sizeof(rfparams));
}

void SX1280Driver::SetFrequency(uint32_t Reqfreq)
{
    uint8_t buf[3] = {0}; //TODO make word alignmed

    uint32_t freq = (uint32_t)((double)Reqfreq / (double)SX1280_FREQ_STEP);
    buf[0] = (uint8_t)((freq >> 16) & 0xFF);
    buf[1] = (uint8_t)((freq >> 8) & 0xFF);
    buf[2] = (uint8_t)(freq & 0xFF);

    hal.WriteCommand(SX1280_RADIO_SET_RFFREQUENCY, buf, sizeof(3));
}

int32_t SX1280Driver::GetFrequencyError()
{
    uint8_t efeRaw[3] = {0}; //TODO make word alignmed
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
    hal.WriteCommand(SX1280_RADIO_SET_BUFFERBASEADDRESS, buf, 2);
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

    hal.WriteCommand(SX1280_RADIO_SET_DIOIRQPARAMS, buf, 8);
}

void SX1280Driver::ClearIrqStatus(uint16_t irqMask)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(((uint16_t)irqMask >> 8) & 0x00FF);
    buf[1] = (uint8_t)((uint16_t)irqMask & 0x00FF);

    hal.WriteCommand(SX1280_RADIO_CLR_IRQSTATUS, buf, 2);
}

void SX1280Driver::TXnbISR()
{
    endTX = micros();
    Serial.print("TOA: ");
    Serial.println(endTX - beginTX);
    instance->GetStatus();
    instance->NonceTX++;

    instance->currOpmode = SX1280_MODE_SLEEP; // radio goes to sleep after TX
    Serial.println("TXnbISR!");
    instance->GetStatus();
    instance->ClearIrqStatus(SX1280_IRQ_RADIO_ALL);
    instance->GetStatus();

    instance->TXdoneCallback1();
    instance->TXdoneCallback2();
    instance->TXdoneCallback3();
    instance->TXdoneCallback4();
}

void SX1280Driver::TXnb(volatile uint8_t *data, uint8_t length)
{
    Serial.println("TXnb");
    instance->ClearIrqStatus(SX1280_IRQ_RADIO_ALL);
    instance->SetOutputPower(13);
    instance->GetStatus();
    hal.setIRQassignment(SX1280_INTERRUPT_TX_DONE);
    instance->GetStatus();
    instance->SetFIFOaddr(0x00, 0x00); // not 100% sure if needed again
    instance->GetStatus();
    hal.WriteBuffer(0x00, (uint8_t *)data, length); //todo fix offset to equal fifo addr
    instance->GetStatus();
    instance->SetMode(SX1280_MODE_TX);
    beginTX = micros();
    instance->GetStatus();
}

void SX1280Driver::RXnbISR()
{
    instance->currOpmode = SX1280_MODE_SLEEP; // radio goes to sleep after TX
    Serial.println("RXnbISR!");

    instance->ClearIrqStatus(SX1280_IRQ_RADIO_ALL);

    hal.ReadBuffer(0x00, instance->RXdataBuffer, 8);

    instance->RXdoneCallback1();
    instance->RXdoneCallback2();
}

void SX1280Driver::RXnb()
{
    // Serial.println("RXnb");
    // instance->SetMode(SX1280_MODE_STDBY_XOSC);
    // instance->GetStatus();
    // //instance->SetDioIrqParams(0x0003, 0x0003, 0x0000, 0x0000); // set DIO1 to trigger on TxDone and RxDone, disable other DIOs
    // //instance->ClearIrqStatus(SX1280_IRQ_RADIO_ALL);
    // instance->GetStatus();
    // hal.setIRQassignment(SX1280_INTERRUPT_RX_DONE);
    // instance->GetStatus();
    // //instance->SetFIFOaddr(0x00, 0x00);
    // instance->GetStatus();
     instance->SetMode(SX1280_MODE_RX);
    // instance->GetStatus();
}

void ICACHE_RAM_ATTR SX1280Driver::GetStatus()
{
    uint8_t status = 0;

    uint8_t stat1;
    uint8_t stat2;
    bool busy;

    hal.ReadCommand(SX1280_RADIO_GET_STATUS, (uint8_t *)&status, 1);
    stat1 = (0b11100000 & status) >> 5;
    stat2 = (0b00011100 & status) >> 2;
    busy = 0b00000001 & status;
    Serial.print("Status: ");
    Serial.print(stat1, HEX);
    Serial.print(", ");
    Serial.print(stat2, HEX);
    Serial.print(", ");
    Serial.println(busy, HEX);
}

bool ICACHE_RAM_ATTR SX1280Driver::GetFrequencyErrorbool()
{
    uint8_t val = hal.ReadRegister(SX1280_REG_LR_ESTIMATED_FREQUENCY_ERROR_MSB);

    if (((0b1000 & val) >> 3) == 1)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void ICACHE_RAM_ATTR SX1280Driver::setPPMoffsetReg(int32_t offset) { return; }

int8_t ICACHE_RAM_ATTR SX1280Driver::GetLastPacketRSSI() { return 0; };
int8_t ICACHE_RAM_ATTR SX1280Driver::GetLastPacketSNR() { return 0; };
