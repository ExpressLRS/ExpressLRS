#include "LoRa_lowlevel.h"
#include "LoRa_SX127x.h"
#include "LoRa_SX1276.h"
#include "LoRa_SX1278.h"

#include "../../src/debug.h"

#define DEBUG

static void nullCallback(void){};

void (*SX127xDriver::RXdoneCallback1)() = &nullCallback;
void (*SX127xDriver::RXdoneCallback2)() = &nullCallback;

void (*SX127xDriver::TXdoneCallback1)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback2)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback3)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback4)() = &nullCallback;

/////////////////////////////////////////////////////////////////

enum InterruptAssignment_
{
    NONE,
    RX_DONE,
    TX_DONE
};

static InterruptAssignment_ InterruptAssignment = NONE;
//////////////////////////////////////////////

SX127xDriver::SX127xDriver()
{
    _RXenablePin = -1;
    _TXenablePin = -1;
    SX127x_dio0 = 0xff;
    SX127x_dio1 = 0xff;
    SX127x_RST = 0xff;

    TXbuffLen = RXbuffLen = 8;

    headerExplMode = false;
    RFmodule = RFMOD_SX1276;

    currBW = BW_500_00_KHZ;
    currSF = SF_6;
    currCR = CR_4_7;
    _syncWord = SX127X_SYNC_WORD;
    currFreq = 123456789;
    currPWR = 0b0000;
    //maxPWR = 0b1111;

    LastPacketIsrMicros = 0;
    LastPacketRSSI = LastPacketRssiRaw = 0;
    LastPacketSNR = 0;
    NonceTX = 0;
    NonceRX = 0;
}

void SX127xDriver::SetPins(int rst, int dio0, int dio1)
{
    SX127x_RST = rst;
    SX127x_dio0 = dio0;
    SX127x_dio1 = dio1;
}

uint8_t SX127xDriver::Begin(bool HighPowerModule, int txpin, int rxpin)
{
    DEBUG_PRINTLN("Driver Begin");
    uint8_t status;

    pinMode(SX127x_dio0, INPUT);
    pinMode(SX127x_dio1, INPUT);

    pinMode(SX127x_RST, OUTPUT);
    digitalWrite(SX127x_RST, 0);
    delay(100);
    digitalWrite(SX127x_RST, 1);
    delay(100);

    _RXenablePin = _TXenablePin = -1;
    if (HighPowerModule)
    {
        _TXenablePin = txpin;
        _RXenablePin = rxpin;

        if (-1 != _TXenablePin)
        {
            pinMode(_TXenablePin, OUTPUT);
            digitalWrite(_RXenablePin, LOW);
        }
        if (-1 != _RXenablePin)
        {
            pinMode(_RXenablePin, OUTPUT);
            digitalWrite(_RXenablePin, LOW);
        }
    }

    // initialize low-level drivers
    initModule();

    status = SX127xBegin();
    if (status != ERR_NONE)
    {
        return (status);
    }

    // Store mode register locally
    p_RegOpMode = readRegister(SX127X_REG_OP_MODE);
    //p_RegDioMapping1 = readRegister(SX127X_REG_DIO_MAPPING_1);
    //p_RegDioMapping2 = readRegister(SX127X_REG_DIO_MAPPING_2);

#if 0
    if (RFmodule == RFMOD_SX1278)
    {
        DEBUG_PRINTLN("Init module SX1278");
        status = SX1278config(this, currBW, currSF, currCR, currFreq, _syncWord);
    }
    else if (RFmodule == RFMOD_SX1276)
    {
        DEBUG_PRINTLN("Init module SX1276");
        status = SX1276config(this, currBW, currSF, currCR, currFreq, _syncWord);
    }
    else
    {
        status = ERR_CHIP_NOT_FOUND;
    }
    DEBUG_PRINTLN("Done");
#endif

    return (status);
}

uint8_t SX127xDriver::SetBandwidth(Bandwidth bw)
{
    uint8_t state = SX127xConfig(bw, currSF, currCR, currFreq, _syncWord);
    if (state == ERR_NONE)
    {
        currBW = bw;
    }
    return (state);
}

uint8_t SX127xDriver::SetSyncWord(uint8_t syncWord)
{
    writeRegister(SX127X_REG_SYNC_WORD, syncWord);
    _syncWord = syncWord;
    return (ERR_NONE);
}

uint8_t SX127xDriver::SetOutputPower(uint8_t Power)
{
    //todo make function turn on PA_BOOST ect
    writeRegister(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_MAX_OUTPUT_POWER | Power);

    currPWR = Power;

    return (ERR_NONE);
}

uint8_t SX127xDriver::SetPreambleLength(uint8_t PreambleLen)
{
    writeRegister(SX127X_REG_PREAMBLE_LSB, PreambleLen);
    return (ERR_NONE);
}

uint8_t SX127xDriver::SetSpreadingFactor(SpreadingFactor sf)
{
    uint8_t status;
    if (sf == SX127X_SF_6)
    {
        status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_SINGLE, 7, 3);
        status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
        writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
    }
    else
    {
        status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
        status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
        writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
    }
    if (status == ERR_NONE)
    {
        currSF = sf;
    }
    return (status);
}

uint8_t SX127xDriver::SetCodingRate(CodingRate cr)
{
    uint8_t state = SX127xConfig(currBW, currSF, cr, currFreq, _syncWord);
    if (state == ERR_NONE)
    {
        currCR = cr;
    }
    return (state);
}

uint8_t SX127xDriver::SetFrequency(uint32_t freq)
{
    uint8_t status = SetMode(SX127X_SLEEP);
    if (status != ERR_NONE)
    {
        return (status);
    }

#define FREQ_STEP 61.03515625

    uint32_t FRQ = ((uint32_t)((double)freq / (double)FREQ_STEP));
    uint8_t buff[3] = {(uint8_t)((FRQ >> 16) & 0xFF), (uint8_t)((FRQ >> 8) & 0xFF), (uint8_t)(FRQ & 0xFF)};
    writeRegisterBurstStr(SX127X_REG_FRF_MSB, buff, sizeof(buff));
    currFreq = freq;

    return SetMode(SX127X_STANDBY);
}

uint8_t SX127xDriver::SX127xBegin()
{
    uint8_t i = 0;
    bool flagFound = false;
    while ((i < 10) && !flagFound)
    {
        uint8_t version = readRegister(SX127X_REG_VERSION);
        DEBUG_PRINTLN(version, HEX);
        if (version == 0x12)
        {
            flagFound = true;
        }
        else
        {
#ifdef DEBUG
            DEBUG_PRINT(" not found! (");
            DEBUG_PRINT(i + 1);
            DEBUG_PRINT(" of 10 tries) REG_VERSION == ");

            char buffHex[5];
            sprintf(buffHex, "0x%02X", version);
            DEBUG_PRINT(buffHex);
            DEBUG_PRINTLN();
#endif
            delay(200);
            i++;
        }
    }

    if (!flagFound)
    {
#ifdef DEBUG
        DEBUG_PRINTLN(" not found!");
#endif
        //SPI.end();
        return (ERR_CHIP_NOT_FOUND);
    }
#ifdef DEBUG
    else
    {
        DEBUG_PRINTLN(" found! (match by REG_VERSION == 0x12)");
    }
#endif
    return (ERR_NONE);
}

uint8_t SX127xDriver::TX(uint8_t *data, uint8_t length)
{
    detachInterrupt(SX127x_dio0);
    InterruptAssignment = NONE;

    SetMode(SX127X_STANDBY);

    reg_dio1_tx_done();

    ClearIRQFlags();

    if (length >= 256)
    {
        return (ERR_PACKET_TOO_LONG);
    }

    writeRegister(SX127X_REG_PAYLOAD_LENGTH, length);
    writeRegister(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);

    writeRegisterBurstStr((uint8_t)SX127X_REG_FIFO, data, (uint8_t)length);

    if (-1 != _RXenablePin)
        digitalWrite(_RXenablePin, LOW);
    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled

    DEBUG_PRINTLN("tx");

    SetMode(SX127X_TX);

    unsigned long start = millis();
    while (!digitalRead(SX127x_dio0))
    {
        //yield();
        //delay(1);
        //TODO: calculate timeout dynamically based on modem settings
        if (millis() - start > (length * 100))
        {
            DEBUG_PRINTLN("Send Timeout");
            ClearIRQFlags();
            return (ERR_TX_TIMEOUT);
        }
    }
    NonceTX++;
    ClearIRQFlags();

    return (ERR_NONE);

#if 0 /* Obsolete code, cannot be reached! */
  if (-1 != _RXenablePin)
    digitalWrite(_RXenablePin, LOW);
  if (-1 != _TXenablePin)
    digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
#endif
}

////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////TX functions/////////////////////////////////////////

static void ICACHE_RAM_ATTR tx_isr_handler(void)
{
    Radio.TXnbISR();
}

void ICACHE_RAM_ATTR SX127xDriver::TXnbISR()
{
    LastPacketIsrMicros = micros();

    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled

    ClearIRQFlags();

    NonceTX++;
    TXdoneCallback1();
    TXdoneCallback2();
    TXdoneCallback3();
    TXdoneCallback4();
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::TXnb(const volatile uint8_t *data, uint8_t length)
{
    SetMode(SX127X_STANDBY);

    if (-1 != _RXenablePin)
        digitalWrite(_RXenablePin, LOW);
    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled

    if (InterruptAssignment != TX_DONE)
    {
        attachInterrupt(digitalPinToInterrupt(SX127x_dio0), tx_isr_handler, RISING);
        InterruptAssignment = TX_DONE;
    }

    writeRegister(SX127X_REG_PAYLOAD_LENGTH, length);
    writeRegister(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);

    writeRegisterBurstStr((uint8_t)SX127X_REG_FIFO, (uint8_t *)data, length);

    reg_dio1_tx_done();
    ClearIRQFlags();

    SetMode(SX127X_TX);

    return (ERR_NONE);
}

///////////////////////////////////RX Functions Non-Blocking///////////////////////////////////////////

static void ICACHE_RAM_ATTR rx_isr_handler(void)
{
    Radio.RXnbISR();
}

void ICACHE_RAM_ATTR SX127xDriver::RXnbISR()
{
    LastPacketIsrMicros = micros();
    readRegisterBurst((uint8_t)SX127X_REG_FIFO, (uint8_t)RXbuffLen, (uint8_t *)RXdataBuffer);
    GetLastPacketRSSI();
    GetLastPacketSNR();
    NonceRX++;
    ClearIRQFlags();
    RXdoneCallback1();
    RXdoneCallback2();
}

void ICACHE_RAM_ATTR SX127xDriver::StopContRX()
{
    detachInterrupt(SX127x_dio0);
    SetMode(SX127X_SLEEP);
    ClearIRQFlags();
    InterruptAssignment = NONE;
}

void ICACHE_RAM_ATTR SX127xDriver::RXnb()
{
    SetMode(SX127X_STANDBY);

    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
    if (-1 != _RXenablePin)
        digitalWrite(_RXenablePin, HIGH);

    //attach interrupt to DIO0, RX continuous mode
    if (InterruptAssignment != RX_DONE)
    {
        attachInterrupt(digitalPinToInterrupt(SX127x_dio0), rx_isr_handler, RISING);
        InterruptAssignment = RX_DONE;
    }

    if (headerExplMode == false)
    {
        writeRegister(SX127X_REG_PAYLOAD_LENGTH, RXbuffLen);
    }

    writeRegister(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);

    reg_dio1_rx_done();
    ClearIRQFlags();

    SetMode(SX127X_RXCONTINUOUS);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t ICACHE_RAM_ATTR SX127xDriver::RXsingle(uint8_t *data, uint8_t length, uint32_t timeout)
{
    detachInterrupt(SX127x_dio0); //disable int callback
    InterruptAssignment = NONE;

    SetMode(SX127X_STANDBY);

    reg_dio1_rx_done();
    ClearIRQFlags();

    writeRegister(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);

    SetMode(SX127X_RXSINGLE);

    uint32_t StartTime = millis();

    while (!digitalRead(SX127x_dio0))
    {
        if (millis() > (StartTime + timeout))
        {
            ClearIRQFlags();
            return (ERR_RX_TIMEOUT);
        }
        // if (digitalRead(SX127x_dio1) || millis() > (StartTime + timeout))
        //{

        //SetMode(SX127X_STANDBY);
        //}
    }

    readRegisterBurst((uint8_t)SX127X_REG_FIFO, length, data);
    GetLastPacketRSSI();
    GetLastPacketSNR();

    ClearIRQFlags();

    NonceRX++;

    return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::RXsingle(uint8_t *data, uint8_t length)
{

    SetMode(SX127X_STANDBY);

    reg_dio1_rx_done();
    ClearIRQFlags();

    writeRegister(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);

    SetMode(SX127X_RXSINGLE);

    while (!digitalRead(SX127x_dio0))
    {
        if (digitalRead(SX127x_dio1))
        {
            ClearIRQFlags();
            return (ERR_RX_TIMEOUT);
        }
    }

    if (getRegValue(SX127X_REG_IRQ_FLAGS, 5, 5) == SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR)
    {
        return (ERR_CRC_MISMATCH);
    }

    if (headerExplMode)
    {
        length = getRegValue(SX127X_REG_RX_NB_BYTES);
    }

    readRegisterBurst((uint8_t)SX127X_REG_FIFO, length, data);
    GetLastPacketRSSI();
    GetLastPacketSNR();

    ClearIRQFlags();
    NonceRX++;

    return (ERR_NONE);
}

uint8_t SX127xDriver::RunCAD()
{
    SetMode(SX127X_STANDBY);

    writeRegister(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_CAD_DONE | SX127X_DIO1_CAD_DETECTED);
    writeRegister(SX127X_REG_IRQ_FLAGS_MASK, (SX127X_CLEAR_IRQ_FLAG_CAD_DONE | SX127X_CLEAR_IRQ_FLAG_CAD_DETECTED));

    SetMode(SX127X_CAD);
    ClearIRQFlags();

    uint32_t startTime = millis();

    while (!digitalRead(SX127x_dio0))
    {
        if (millis() > (startTime + 500))
        {
            return (CHANNEL_FREE);
        }
        else
        {
            if (digitalRead(SX127x_dio1))
            {
                ClearIRQFlags();
                return (PREAMBLE_DETECTED);
            }
        }
    }

    ClearIRQFlags();
    return (CHANNEL_FREE);
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::SetMode(uint8_t mode)
{ //if radio is not already in the required mode set it to the requested mode

    p_RegOpMode &= (~SX127X_CAD);
    p_RegOpMode |= (mode & SX127X_CAD);
    writeRegister(SX127X_REG_OP_MODE, p_RegOpMode);

    return (ERR_NONE);
}

uint8_t SX127xDriver::Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord)
{
    if (freq == 0)
        freq = currFreq;
    if (syncWord == 0)
        syncWord = _syncWord;

    if (RFmodule == RFMOD_SX1276)
    {
        return SX1276config(this, bw, sf, cr, freq, syncWord);
    }
    else if (RFmodule == RFMOD_SX1278)
    {
        return SX1278config(this, bw, sf, cr, freq, syncWord);
    }
    return 1;
}

uint8_t SX127xDriver::SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord)
{

    uint8_t status = ERR_NONE;

    // set mode to SLEEP
    status = SetMode(SX127X_SLEEP);
    if (status != ERR_NONE)
    {
        return (status);
    }

    // set LoRa mode
    reg_op_mode_mode_lora();

    SetFrequency(freq);

    // output power configuration

    writeRegister(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_MAX_OUTPUT_POWER | currPWR);
    //writeRegister(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_OUTPUT_POWER);
    status = setRegValue(SX127X_REG_OCP, SX127X_OCP_ON | 23, 5, 0); //200ma
    //writeRegister(SX127X_REG_LNA, SX127X_LNA_GAIN_1 | SX127X_LNA_BOOST_ON);

    if (status != ERR_NONE)
    {
        return (status);
    }

    // turn off frequency hopping
    writeRegister(SX127X_REG_HOP_PERIOD, SX127X_HOP_PERIOD_OFF);

    // basic setting (bw, cr, sf, header mode and CRC)
    if (sf == SX127X_SF_6)
    {
        status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_SINGLE, 7, 3);
        //status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_CONT, 7, 3);
        status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
        writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
    }
    else
    {
        status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
        //status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_CONT, 7, 3);
        status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
        writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
    }

    if (status != ERR_NONE)
    {
        return (status);
    }

    // set the sync word
    writeRegister(SX127X_REG_SYNC_WORD, syncWord);

    // set default preamble length
    //status = setRegValue(SX127X_REG_PREAMBLE_MSB, SX127X_PREAMBLE_LENGTH_MSB);
    //status = setRegValue(SX127X_REG_PREAMBLE_LSB, SX127X_PREAMBLE_LENGTH_LSB);

    //status = setRegValue(SX127X_REG_PREAMBLE_MSB, 0);
    //status = setRegValue(SX127X_REG_PREAMBLE_LSB, 6);

    if (status != ERR_NONE)
    {
        return (status);
    }

    // set mode to STANDBY
    status = SetMode(SX127X_STANDBY);
    return (status);
}

uint32_t ICACHE_RAM_ATTR SX127xDriver::getCurrBandwidth()
{

    switch (currBW)
    {
    case BW_7_80_KHZ:
        return 7.8E3;
    case BW_10_40_KHZ:
        return 10.4E3;
    case BW_15_60_KHZ:
        return 15.6E3;
    case BW_20_80_KHZ:
        return 20.8E3;
    case BW_31_25_KHZ:
        return 31.25E3;
    case BW_41_70_KHZ:
        return 41.7E3;
    case BW_62_50_KHZ:
        return 62.5E3;
    case BW_125_00_KHZ:
        return 125E3;
    case BW_250_00_KHZ:
        return 250E3;
    case BW_500_00_KHZ:
        return 500E3;
    }

    return -1;
}

uint32_t ICACHE_RAM_ATTR SX127xDriver::getCurrBandwidthNormalisedShifted() // this is basically just used for speedier calc of the freq offset, pre compiled for 32mhz xtal
{

    switch (currBW)
    {
    case BW_7_80_KHZ:
        return 1026;
    case BW_10_40_KHZ:
        return 769;
    case BW_15_60_KHZ:
        return 513;
    case BW_20_80_KHZ:
        return 385;
    case BW_31_25_KHZ:
        return 256;
    case BW_41_70_KHZ:
        return 192;
    case BW_62_50_KHZ:
        return 128;
    case BW_125_00_KHZ:
        return 64;
    case BW_250_00_KHZ:
        return 32;
    case BW_500_00_KHZ:
        return 16;
    }

    return -1;
}

void ICACHE_RAM_ATTR SX127xDriver::setPPMoffsetReg(int32_t offset)
{
    //int32_t offsetValue = ((int32_t)243) * (offset << 8) / ((((int32_t)currFreq / 1000000)) << 8);
    offset <<= 8;
    offset *= 243;
    offset /= ((currFreq / 1000000) << 8);
    offset >>= 8;

    uint8_t regValue = offset & 0b01111111;

    if (offset < 0)
    {
        regValue |= 0b10000000; //set neg bit for 2s complement
    }

    writeRegister(SX127x_PPMOFFSET, regValue);
}

int32_t SX127xDriver::GetFrequencyError()
{
    int32_t intFreqError;
    uint8_t fei_reg[3] = {0x0, 0x0, 0x0};
    readRegisterBurst(SX127X_REG_FEI_MSB, sizeof(fei_reg), fei_reg);

    //memcpy(&intFreqError, fei_reg, sizeof(fei_reg));
    //intFreqError = (__builtin_bswap32(intFreqError) >> 8); // 24bit

    intFreqError = fei_reg[0] & 0b0111;
    intFreqError <<= 8;
    intFreqError += fei_reg[1];
    intFreqError <<= 8;
    intFreqError += fei_reg[2];

    if (fei_reg[0] & 0b1000) // Sign bit is on
    {
        // convert to negative
        intFreqError -= 524288;
    }

    // bit shift hackery so we don't have to use floaty bois; the >> 3 is intentional
    // and is a simplification of the formula on page 114 of sx1276 datasheet
    intFreqError = (intFreqError >> 3) * (getCurrBandwidthNormalisedShifted());
    intFreqError >>= 4;
    return intFreqError;
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketRSSIUnsigned()
{
    LastPacketRssiRaw = readRegister(SX127X_REG_PKT_RSSI_VALUE);
    return (LastPacketRssiRaw);
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketRSSI()
{
    LastPacketRSSI = (-157 + GetLastPacketRSSIUnsigned());
    return LastPacketRSSI;
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketSNR()
{
    LastPacketSNR = (int8_t)readRegister(SX127X_REG_PKT_SNR_VALUE);
    LastPacketSNR /= 4;
    return LastPacketSNR;
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetCurrRSSI() const
{
    return (-157 + readRegister(SX127X_REG_RSSI_VALUE));
}

void ICACHE_RAM_ATTR SX127xDriver::ClearIRQFlags()
{
    writeRegister(SX127X_REG_IRQ_FLAGS, 0xff);
}

void ICACHE_RAM_ATTR SX127xDriver::reg_op_mode_mode_lora(void)
{
    p_RegOpMode |= SX127X_LORA;
    writeRegister(SX127X_REG_OP_MODE, p_RegOpMode);
}

void ICACHE_RAM_ATTR SX127xDriver::reg_dio1_rx_done(void)
{
    // 0b00 == DIO0 RxDone
    //p_RegDioMapping1 &= 0b00111111;
    //p_RegDioMapping1 |= (SX127X_DIO0_RX_DONE);
    //writeRegister(SX127X_REG_DIO_MAPPING_1, p_RegDioMapping1);
    writeRegister(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RX_DONE);
    writeRegister(SX127X_REG_IRQ_FLAGS_MASK, SX127X_MASK_IRQ_FLAG_RX_DONE);
}

void ICACHE_RAM_ATTR SX127xDriver::reg_dio1_tx_done(void)
{
    // 0b00 == DIO0 TxDone
    //p_RegDioMapping1 &= 0b00111111;
    //p_RegDioMapping1 |= SX127X_DIO0_TX_DONE;
    //writeRegister(SX127X_REG_DIO_MAPPING_1, p_RegDioMapping1);
    writeRegister(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_TX_DONE);
    writeRegister(SX127X_REG_IRQ_FLAGS_MASK, SX127X_MASK_IRQ_FLAG_TX_DONE);
}

SX127xDriver Radio;
