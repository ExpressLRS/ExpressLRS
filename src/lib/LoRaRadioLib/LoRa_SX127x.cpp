#include "LoRa_lowlevel.h"
#include "LoRa_SX127x.h"
#include "LoRa_SX1276.h"
#include "LoRa_SX1278.h"

#include "debug.h"

#include <stdio.h>

#define DEBUG

void (*SX127xDriver::RXdoneCallback1)(uint8_t *) = SX127xDriver::rx_nullCallback;
//void (*SX127xDriver::RXdoneCallback2)(uint8_t *) = SX127xDriver::rx_nullCallback;

void (*SX127xDriver::TXdoneCallback1)() = SX127xDriver::tx_nullCallback;
void (*SX127xDriver::TXdoneCallback2)() = SX127xDriver::tx_nullCallback;
void (*SX127xDriver::TXdoneCallback3)() = SX127xDriver::tx_nullCallback;
void (*SX127xDriver::TXdoneCallback4)() = SX127xDriver::tx_nullCallback;

/////////////////////////////////////////////////////////////////

enum isr_states
{
    NONE,
    RX_DONE,
    TX_DONE,
    ISR_RCVD,
};

static volatile enum isr_states p_state_dio0_isr = NONE;
static void ICACHE_RAM_ATTR _rxtx_isr_handler_dio0(void)
{
    if (p_state_dio0_isr == RX_DONE)
        Radio.RXnbISR();
    else if (p_state_dio0_isr == TX_DONE)
        Radio.TXnbISR();
    else
        p_state_dio0_isr = ISR_RCVD;
}

static volatile enum isr_states p_state_dio1_isr = NONE;
static void ICACHE_RAM_ATTR _rxtx_isr_handler_dio1(void)
{
    p_state_dio1_isr = ISR_RCVD;
}

//////////////////////////////////////////////

SX127xDriver::SX127xDriver(int rst, int dio0, int dio1, int txpin, int rxpin)
{
    _RXenablePin = rxpin;
    _TXenablePin = txpin;
    SX127x_dio0 = dio0;
    SX127x_dio1 = dio1;
    SX127x_RST = rst;

    RXbuffLen = 8;

    headerExplMode = false;
    RFmodule = RFMOD_SX1276;

    currBW = BW_500_00_KHZ;
    currSF = SF_6;
    currCR = CR_4_7;
    _syncWord = SX127X_SYNC_WORD;
    currFreq = 123456789;
    currPWR = 0b0000;
    //maxPWR = 0b1111;

    //LastPacketIsrMicros = 0;
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

uint8_t SX127xDriver::Begin(int txpin, int rxpin)
{
    uint8_t status;

    pinMode(SX127x_dio0, INPUT);
    pinMode(SX127x_dio1, INPUT);

    /* Attach interrupts to pins */
    if (0xff != SX127x_dio0)
        attachInterrupt(digitalPinToInterrupt(SX127x_dio0), _rxtx_isr_handler_dio0, RISING);
    if (0xff != SX127x_dio1)
        attachInterrupt(digitalPinToInterrupt(SX127x_dio1), _rxtx_isr_handler_dio1, RISING);

    if (-1 < SX127x_RST)
    {
        pinMode(SX127x_RST, OUTPUT);
        digitalWrite(SX127x_RST, 0);
        delay(100);
        digitalWrite(SX127x_RST, 1);
        delay(100);
    }

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

    // initialize low-level drivers
    initModule();

    status = SX127xBegin();
    if (status != ERR_NONE)
    {
        return (status);
    }

    // Store mode register locally
    p_RegOpMode = readRegister(SX127X_REG_OP_MODE);
    // Write enable CAD, RX and TX ISRs
    writeRegister(SX127X_REG_DIO_MAPPING_1,
                  (SX127X_DIO0_CAD_DONE | SX127X_DIO1_CAD_DETECTED |
                   SX127X_DIO0_TX_DONE | SX127X_DIO0_RX_DONE));

    return (status);
}

uint8_t SX127xDriver::SetBandwidth(Bandwidth bw)
{
    return Config(bw, currSF, currCR, currFreq, _syncWord);
}

uint8_t SX127xDriver::SetSyncWord(uint8_t syncWord)
{
    //writeRegister(SX127X_REG_SYNC_WORD, syncWord);
    _syncWord = syncWord;
    return (ERR_NONE);
}

uint8_t SX127xDriver::SetOutputPower(uint8_t Power)
{
    Power &= 0xF; // 4bits
    if (currPWR == Power)
        return ERR_NONE;

    //todo make function turn on PA_BOOST ect
    writeRegister(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_MAX_OUTPUT_POWER | Power);

    currPWR = Power;

    return (ERR_NONE);
}

uint8_t SX127xDriver::SetPreambleLength(uint16_t PreambleLen)
{
    if (PreambleLen < 6)
        PreambleLen = 6;
    //SetMode(SX127X_SLEEP); // needed??
    uint8_t len[2] = {(uint8_t)(PreambleLen >> 16), (uint8_t)(PreambleLen & 0xff)};
    writeRegisterBurstStr(SX127X_REG_PREAMBLE_MSB, len, sizeof(len));
    //SetMode(SX127X_STANDBY);
    //writeRegister(SX127X_REG_PREAMBLE_LSB, (uint8_t)PreambleLen);
    return (ERR_NONE);
}

uint8_t SX127xDriver::SetSpreadingFactor(SpreadingFactor sf)
{
    return Config(currBW, sf, currCR, currFreq, _syncWord);
}

uint8_t SX127xDriver::SetCodingRate(CodingRate cr)
{
    return Config(currBW, currSF, cr, currFreq, _syncWord);
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::SetFrequency(uint32_t freq, uint8_t mode)
{
    //if (freq == currFreq)
    //    return ERR_NONE;

    if (mode != 0xff)
        SetMode(SX127X_SLEEP);

#if FREQ_USE_DOUBLE
#define FREQ_STEP 61.03515625

    uint32_t FRQ = ((uint32_t)((double)freq / (double)FREQ_STEP));
    uint8_t buff[3] = {(uint8_t)((FRQ >> 16) & 0xFF), (uint8_t)((FRQ >> 8) & 0xFF), (uint8_t)(FRQ & 0xFF)};
#else
    uint64_t frf = ((uint64_t)freq << 19) / 32000000;
    uint8_t buff[3] = {
        (uint8_t)(frf >> 16),
        (uint8_t)(frf >> 8),
        (uint8_t)(frf >> 0),
    };
#endif
    writeRegisterBurstStr(SX127X_REG_FRF_MSB, buff, sizeof(buff));
    currFreq = freq;

    if (mode != 0xff && mode != SX127X_SLEEP)
        SetMode(mode);

    return ERR_NONE;
}

uint8_t SX127xDriver::SX127xBegin()
{
    uint8_t i = 0;
    bool flagFound = false;
    while ((i < 10) && !flagFound)
    {
        uint8_t version = readRegister(SX127X_REG_VERSION);
        if (version == 0x12)
        {
            flagFound = true;
#ifdef DEBUG
            DEBUG_PRINTLN("SX127x found! (match by REG_VERSION == 0x12)");
#endif
        }
        else
        {
#ifdef DEBUG
            DEBUG_PRINT("SX127x not found! (");
            DEBUG_PRINT(i + 1);
            DEBUG_PRINT(" of 10 tries) REG_VERSION == 0x");
            DEBUG_PRINTLN(version, HEX);
#endif
            delay(200);
            i++;
        }
    }

    if (!flagFound)
    {
        return (ERR_CHIP_NOT_FOUND);
    }
    return (ERR_NONE);
}

int16_t SX127xDriver::MeasureNoiseFloor(uint32_t num_meas, uint32_t freq)
{
    SetFrequency(freq, SX127X_CAD);

    int32_t noise = 0;
    for (uint32_t iter = 0; iter < num_meas; iter++)
    {
        noise += GetCurrRSSI();
        delay(5);
    }
    noise /= num_meas;
    return noise;
}

////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// TX functions ///////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

uint8_t ICACHE_RAM_ATTR SX127xDriver::TX(uint8_t *data, uint8_t length)
{
    p_state_dio0_isr = p_state_dio1_isr = NONE;

    SetMode(SX127X_STANDBY);

    if (length >= 256)
    {
        return (ERR_PACKET_TOO_LONG);
    }

    if (-1 != _RXenablePin)
        digitalWrite(_RXenablePin, LOW);
    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled

    writeRegister(SX127X_REG_PAYLOAD_LENGTH, length);
    writeRegister(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
    writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);
    writeRegisterBurstStr((uint8_t)SX127X_REG_FIFO, data, (uint8_t)length);

    reg_dio1_isr_mask_write(SX127X_DIO0_TX_DONE);

    SetMode(SX127X_TX);

    unsigned long start = millis();
    while (p_state_dio0_isr != ISR_RCVD)
    {
        //yield();
        //delay(1);
        //TODO: calculate timeout dynamically based on modem settings
        if (millis() - start > (length * 100))
        {
            DEBUG_PRINTLN("Send Timeout");
            return (ERR_TX_TIMEOUT);
        }
    }
    NonceTX++;

    return (ERR_NONE);
}

void ICACHE_RAM_ATTR SX127xDriver::TXnbISR()
{
    LastPacketIsrMicros = micros();

    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled

    NonceTX++; // keep this before callbacks!
    TXdoneCallback1();
    TXdoneCallback2();
    TXdoneCallback3();
    TXdoneCallback4();
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::TXnb(const uint8_t *data, uint8_t length, uint32_t freq)
{
    SetMode(SX127X_STANDBY);
    p_state_dio0_isr = TX_DONE;

    if (-1 != _RXenablePin)
        digitalWrite(_RXenablePin, LOW);
    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled

    // Set freq if defined
    if (freq)
        SetFrequency(freq, 0xff);

    if (p_last_payload_len != length)
    {
        writeRegister(SX127X_REG_PAYLOAD_LENGTH, length);
        p_last_payload_len = length;
    }
    uint8_t cfg[2] = {SX127X_FIFO_TX_BASE_ADDR_MAX, SX127X_FIFO_TX_BASE_ADDR_MAX};
    writeRegisterBurstStr(SX127X_REG_FIFO_ADDR_PTR, cfg, sizeof(cfg));
    writeRegisterBurstStr(SX127X_REG_FIFO, (uint8_t *)data, length);

    reg_dio1_isr_mask_write(SX127X_DIO0_TX_DONE);
    //reg_dio1_tx_done();
    //ClearIRQFlags();

    SetMode(SX127X_TX);

    return (ERR_NONE);
}

////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// RX functions ///////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

void ICACHE_RAM_ATTR SX127xDriver::RXnbISR()
{
    LastPacketIsrMicros = micros();
    readRegisterBurst((uint8_t)SX127X_REG_FIFO, (uint8_t)RXbuffLen, (uint8_t *)RXdataBuffer);
    //GetLastPacketRSSI();
    //GetLastPacketSNR();
    GetLastRssiSnr();
    NonceRX++;
    RXdoneCallback1(RXdataBuffer);
    //RXdoneCallback2();
    ClearIRQFlags();
}

void ICACHE_RAM_ATTR SX127xDriver::StopContRX()
{
    SetMode(SX127X_SLEEP);
    p_state_dio0_isr = NONE;
}

void ICACHE_RAM_ATTR SX127xDriver::RXnb(uint32_t freq)
{
    SetMode(SX127X_STANDBY);
    p_state_dio0_isr = RX_DONE;
    p_state_dio1_isr = NONE;

    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
    if (-1 != _RXenablePin)
        digitalWrite(_RXenablePin, HIGH);

    // Set freq if defined
    if (freq)
        SetFrequency(freq, 0xff);

    if (headerExplMode == false && p_last_payload_len != RXbuffLen)
    {
        writeRegister(SX127X_REG_PAYLOAD_LENGTH, RXbuffLen);
        p_last_payload_len = RXbuffLen;
    }

    uint8_t cfg[3] = {SX127X_FIFO_RX_BASE_ADDR_MAX, SX127X_FIFO_TX_BASE_ADDR_MAX, SX127X_FIFO_RX_BASE_ADDR_MAX};
    writeRegisterBurstStr(SX127X_REG_FIFO_ADDR_PTR, cfg, sizeof(cfg));

    reg_dio1_isr_mask_write(SX127X_DIO0_RX_DONE);

    SetMode(SX127X_RXCONTINUOUS);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t ICACHE_RAM_ATTR SX127xDriver::RXsingle(uint8_t *data, uint8_t length, uint32_t timeout)
{
    p_state_dio0_isr = p_state_dio1_isr = NONE;

    SetMode(SX127X_STANDBY);

    reg_dio1_isr_mask_write(SX127X_DIO0_RX_DONE);

    uint8_t cfg[3] = {SX127X_FIFO_RX_BASE_ADDR_MAX, SX127X_FIFO_TX_BASE_ADDR_MAX, SX127X_FIFO_RX_BASE_ADDR_MAX};
    writeRegisterBurstStr(SX127X_REG_FIFO_ADDR_PTR, cfg, sizeof(cfg));

    SetMode(SX127X_RXSINGLE);

    uint32_t StartTime = millis();

    while (p_state_dio0_isr != ISR_RCVD)
    {
        if (millis() > (StartTime + timeout))
        {
            return (ERR_RX_TIMEOUT);
        }
    }

    readRegisterBurst((uint8_t)SX127X_REG_FIFO, length, data);
    GetLastRssiSnr();

    NonceRX++;

    return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::RXsingle(uint8_t *data, uint8_t length)
{
    p_state_dio0_isr = p_state_dio1_isr = NONE;

    SetMode(SX127X_STANDBY);

    uint8_t cfg[3] = {SX127X_FIFO_RX_BASE_ADDR_MAX, SX127X_FIFO_TX_BASE_ADDR_MAX, SX127X_FIFO_RX_BASE_ADDR_MAX};
    writeRegisterBurstStr(SX127X_REG_FIFO_ADDR_PTR, cfg, sizeof(cfg));

    reg_dio1_isr_mask_write(SX127X_DIO0_RX_DONE);

    SetMode(SX127X_RXSINGLE);

    while (p_state_dio0_isr != ISR_RCVD)
    {
        if (p_state_dio1_isr != NONE)
        {
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
    GetLastRssiSnr();

    NonceRX++;

    return (ERR_NONE);
}

uint8_t SX127xDriver::RunCAD()
{
    p_state_dio0_isr = p_state_dio1_isr = NONE;

    SetMode(SX127X_STANDBY);

    reg_dio1_isr_mask_write((SX127X_CLEAR_IRQ_FLAG_CAD_DONE | SX127X_CLEAR_IRQ_FLAG_CAD_DETECTED));

    SetMode(SX127X_CAD);

    uint32_t startTime = millis();

    while (p_state_dio0_isr != ISR_RCVD)
    {
        if (millis() > (startTime + 500))
        {
            return (CHANNEL_FREE);
        }
        else
        {
            if (p_state_dio1_isr != NONE)
            {
                return (PREAMBLE_DETECTED);
            }
        }
    }

    return (CHANNEL_FREE);
}

////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// config functions //////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

uint8_t ICACHE_RAM_ATTR SX127xDriver::SetMode(uint8_t mode)
{ //if radio is not already in the required mode set it to the requested mode
    mode &= SX127X_CAD;

    if (mode != (p_RegOpMode & SX127X_CAD))
    {
        p_RegOpMode &= (~SX127X_CAD);
        p_RegOpMode |= (mode & SX127X_CAD);
        writeRegister(SX127X_REG_OP_MODE, p_RegOpMode);
    }
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

    SetFrequency(freq, 0xff); // 0xff = skip mode set calls

    // output power configuration
    SetOutputPower(currPWR);
    status = setRegValue(SX127X_REG_OCP, SX127X_OCP_ON | 23, 5, 0); //200ma
    //writeRegister(SX127X_REG_LNA, SX127X_LNA_GAIN_1 | SX127X_LNA_BOOST_ON);

    if (status != ERR_NONE)
    {
        return (status);
    }

    // turn off frequency hopping
    writeRegister(SX127X_REG_HOP_PERIOD, SX127X_HOP_PERIOD_OFF);

    // basic setting (bw, cr, sf, header mode and CRC)
    uint8_t cfg2 = (sf | SX127X_TX_MODE_SINGLE | SX127X_RX_CRC_MODE_OFF);
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, cfg2, 7, 2);
    if (sf == SX127X_SF_6)
    {
        status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
        writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
    }
    else
    {
        status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
        writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
    }

    if (status != ERR_NONE)
    {
        return (status);
    }

    // set the sync word
    writeRegister(SX127X_REG_SYNC_WORD, syncWord);

    // set mode to STANDBY
    SetMode(SX127X_STANDBY);

    return ERR_NONE;
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
            return 41.667E3;
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
    if (regValue == p_ppm_off)
        return;
    p_ppm_off = regValue;
    writeRegister(SX127x_PPMOFFSET, regValue);
}

void ICACHE_RAM_ATTR SX127xDriver::setPPMoffsetReg(int32_t error_hz, uint32_t frf)
{
    if (!frf) // use locally stored value if not defined
        frf = currFreq;
    if (!frf)
        return;
    // Calc new PPM
    error_hz /= (frf / 1E6);
    error_hz *= 95;
    error_hz /= 100;

    uint8_t regValue = (uint8_t)error_hz;
    if (regValue == p_ppm_off)
        return;
    p_ppm_off = regValue;
    writeRegister(SX127x_PPMOFFSET, regValue);
}

int32_t ICACHE_RAM_ATTR SX127xDriver::GetFrequencyError()
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

#if !NEW_FREQ_CORR
    // bit shift hackery so we don't have to use floaty bois; the >> 3 is intentional
    // and is a simplification of the formula on page 114 of sx1276 datasheet
    intFreqError = (intFreqError >> 3) * (getCurrBandwidthNormalisedShifted());
    intFreqError >>= 4;
#else
    // Calculate Hz error where XTAL is 32MHz
    int64_t tmp_f = intFreqError;
    tmp_f <<= 11;
    tmp_f *= getCurrBandwidth();
    tmp_f /= 1953125000;
    intFreqError = tmp_f;
    //intFreqError = ((tmp_f << 11) * getCurrBandwidth()) / 1953125000;
#endif
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

void ICACHE_RAM_ATTR SX127xDriver::GetLastRssiSnr()
{
    // Packet SNR + RSSI and current RSSI
    uint8_t resp[] = {0x0, 0x0 /*, 0x0*/};
    readRegisterBurst(SX127X_REG_PKT_SNR_VALUE, sizeof(resp), resp);
    LastPacketSNR = (int8_t)resp[0] / 4;
    LastPacketRssiRaw = resp[1];
    LastPacketRSSI = -157 + resp[1];
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
    writeRegister(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RX_DONE);
    writeRegister(SX127X_REG_IRQ_FLAGS_MASK, SX127X_MASK_IRQ_FLAG_RX_DONE);
    //p_isr_mask = SX127X_MASK_IRQ_FLAG_RX_DONE;
}

void ICACHE_RAM_ATTR SX127xDriver::reg_dio1_tx_done(void)
{
    // 0b00 == DIO0 TxDone
    writeRegister(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_TX_DONE);
    writeRegister(SX127X_REG_IRQ_FLAGS_MASK, SX127X_MASK_IRQ_FLAG_TX_DONE);
    //p_isr_mask = SX127X_MASK_IRQ_FLAG_TX_DONE;
}

void ICACHE_RAM_ATTR SX127xDriver::reg_dio1_isr_mask_write(uint8_t mask)
{
    // write mask and clear irqs
    uint8_t cfg[2] = {mask, 0xff};
    writeRegisterBurstStr(SX127X_REG_IRQ_FLAGS_MASK, cfg, sizeof(cfg));

    /*if (p_isr_mask != mask)
    {
        writeRegister(SX127X_REG_IRQ_FLAGS_MASK, mask);
        p_isr_mask = mask;
    }*/
}

SX127xDriver Radio;
