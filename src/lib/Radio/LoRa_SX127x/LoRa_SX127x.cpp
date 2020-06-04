#include "LoRa_SX127x.h"
#include "LoRa_SX127x_Regs.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>

#define DEBUG

void (*SX127xDriver::RXdoneCallback1)(uint8_t *) = SX127xDriver::rx_nullCallback;
//void (*SX127xDriver::RXdoneCallback2)(uint8_t *) = SX127xDriver::rx_nullCallback;

void (*SX127xDriver::TXdoneCallback1)() = SX127xDriver::tx_nullCallback;
void (*SX127xDriver::TXdoneCallback2)() = SX127xDriver::tx_nullCallback;
void (*SX127xDriver::TXdoneCallback3)() = SX127xDriver::tx_nullCallback;
void (*SX127xDriver::TXdoneCallback4)() = SX127xDriver::tx_nullCallback;

static volatile uint8_t DMA_ATTR p_RegOpMode = 0;

/////////////////////////////////////////////////////////////////

enum isr_states
{
    NONE,
    RX_DONE,
    RX_TIMEOUT,
    TX_DONE,
    CRC_ERROR,
    CAD_DETECTED,
    ISR_RCVD,
};

static volatile enum isr_states DRAM_ATTR p_state_dio0_isr = NONE;
static void ICACHE_RAM_ATTR _rxtx_isr_handler_dio0(void)
{
    Radio.LastPacketIsrMicros = micros();
    uint8_t irqs = Radio.GetIRQFlags();
    if (p_state_dio0_isr == RX_DONE) {
        Radio.RXnbISR(irqs);
    } else if (p_state_dio0_isr == TX_DONE) {
        Radio.TXnbISR(irqs);
    } else {
        if (irqs & SX127X_CLEAR_IRQ_FLAG_TX_DONE)
            p_state_dio0_isr = TX_DONE;
        else if (irqs & SX127X_CLEAR_IRQ_FLAG_RX_TIMEOUT)
            p_state_dio0_isr = RX_TIMEOUT;
        else if (irqs & SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR)
            p_state_dio0_isr = CRC_ERROR;
        else if (irqs & SX127X_CLEAR_IRQ_FLAG_RX_DONE)
            p_state_dio0_isr = RX_DONE;
        else if (irqs & SX127X_CLEAR_IRQ_FLAG_CAD_DETECTED)
            p_state_dio0_isr = CAD_DETECTED;
    }
    Radio.ClearIRQFlags();
}

static volatile enum isr_states DRAM_ATTR p_state_dio1_isr = NONE;
static void ICACHE_RAM_ATTR _rxtx_isr_handler_dio1(void)
{
    p_state_dio1_isr = ISR_RCVD;
}

//////////////////////////////////////////////

SX127xDriver::SX127xDriver(HwSpi &spi, int rst, int dio0, int dio1, int txpin, int rxpin):
    RadioHalSpi(spi, SX127X_SPI_READ, SX127X_SPI_WRITE)
{
    _RXenablePin = rxpin;
    _TXenablePin = txpin;
    SX127x_dio0 = dio0;
    SX127x_dio1 = dio1;
    SX127x_RST = rst;

    headerExplMode = false;
    RFmodule = RFMOD_SX1276;

    currBW = BW_500_00_KHZ;
    currSF = SF_6;
    currCR = CR_4_5;
    _syncWord = SX127X_SYNC_WORD;
    currFreq = 0;
    currPWR = (SX127X_PA_SELECT_BOOST + SX127X_MAX_OUTPUT_POWER); // define to max for forced init.

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
    RadioHalSpi::Begin();

    status = CheckChipVersion();
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
    reg_dio1_isr_mask_write(0);

    return (status);
}

uint8_t SX127xDriver::SetSyncWord(uint8_t syncWord)
{
    //writeRegister(SX127X_REG_SYNC_WORD, syncWord);
    _syncWord = syncWord;
    return (ERR_NONE);
}

void SX127xDriver::SetOutputPower(uint8_t Power)
{
    Power &= 0xF; // 4bits
    if (currPWR == Power)
        return;

    writeRegister(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_MAX_OUTPUT_POWER | Power);
    writeRegister(SX127X_REG_PA_DAC, (0x80 | ((Power == 0xf) ? SX127X_PA_BOOST_ON : SX127X_PA_BOOST_OFF)));

    currPWR = Power;
}

void SX127xDriver::SetPreambleLength(uint16_t PreambleLen)
{
    if (PreambleLen < 6)
        PreambleLen = 6;
    SetMode(SX127X_SLEEP);
    uint8_t len[2] = {(uint8_t)(PreambleLen >> 16), (uint8_t)(PreambleLen & 0xff)};
    writeRegisterBurst(SX127X_REG_PREAMBLE_MSB, len, sizeof(len));
    SetMode(SX127X_STANDBY);
}

void ICACHE_RAM_ATTR SX127xDriver::SetFrequency(uint32_t freq, uint8_t mode)
{
    // TODO: Take this into use if ok!!
    if (freq == currFreq)
        return;

    if (mode != 0xff)
        SetMode(SX127X_SLEEP);

#if FREQ_USE_DOUBLE
#define FREQ_STEP 61.03515625

    uint32_t FRQ = ((uint32_t)((double)freq / (double)FREQ_STEP));
    uint8_t buff[3] = {(uint8_t)((FRQ >> 16) & 0xFF), (uint8_t)((FRQ >> 8) & 0xFF), (uint8_t)(FRQ & 0xFF)};
#else
    uint64_t frf = ((uint64_t)(freq + p_freqOffset) << 19) / 32000000;
    uint8_t buff[3] = {
        (uint8_t)(frf >> 16),
        (uint8_t)(frf >> 8),
        (uint8_t)(frf >> 0),
    };
#endif
    writeRegisterBurst(SX127X_REG_FRF_MSB, buff, sizeof(buff));
    currFreq = freq;

    if (mode != 0xff && mode != SX127X_SLEEP)
        SetMode(mode);
}

uint8_t SX127xDriver::CheckChipVersion()
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
    delay(3);

    int noise = 0;
    for (uint32_t iter = 0; iter < num_meas; iter++)
    {
        delayMicroseconds(100);
        noise += GetCurrRSSI();
    }
    noise /= (int)num_meas; // Compiler bug! must cast to int to make this div working!
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
    writeRegisterBurst((uint8_t)SX127X_REG_FIFO, data, (uint8_t)length);

    //reg_dio1_isr_mask_write(SX127X_MASK_IRQ_FLAG_TX_DONE);

    SetMode(SX127X_TX);

    unsigned long start = millis();
    while (p_state_dio0_isr == NONE)
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

void ICACHE_RAM_ATTR SX127xDriver::TXnbISR(uint8_t irqs)
{
    // Ignore if not a TX DONE ISR
    if (!(irqs & SX127X_CLEAR_IRQ_FLAG_TX_DONE))
        return;

    _change_mode_val(SX127X_STANDBY); // Standby mode is automatically set by IC, set it also locally

    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled

    NonceTX++; // keep this before callbacks!
    TXdoneCallback1();
    TXdoneCallback2();
    TXdoneCallback3();
    TXdoneCallback4();
}

void ICACHE_RAM_ATTR SX127xDriver::TXnb(const uint8_t *data, uint8_t length, uint32_t freq)
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
    writeRegisterBurst(SX127X_REG_FIFO_ADDR_PTR, cfg, sizeof(cfg));
    writeRegisterBurst(SX127X_REG_FIFO, (uint8_t *)data, length);

    //reg_dio1_isr_mask_write(SX127X_MASK_IRQ_FLAG_TX_DONE);

    SetMode(SX127X_TX);
}

////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// RX functions ///////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

void ICACHE_RAM_ATTR SX127xDriver::RxConfig(uint32_t freq)
{
    SetMode(SX127X_STANDBY);
    p_state_dio0_isr = p_state_dio1_isr = NONE;

    if (-1 != _TXenablePin)
        digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
    if (-1 != _RXenablePin)
        digitalWrite(_RXenablePin, HIGH);

    // Set freq if defined
    if (freq)
        SetFrequency(freq, 0xff);

    if (headerExplMode == false && p_last_payload_len != RX_BUFFER_LEN)
    {
        writeRegister(SX127X_REG_PAYLOAD_LENGTH, RX_BUFFER_LEN);
        p_last_payload_len = RX_BUFFER_LEN;
    }

    /* ESP requires aligned buffer when -Os is not set! */
#if SX127X_FIFO_RX_BASE_ADDR_MAX == 0 && SX127X_FIFO_TX_BASE_ADDR_MAX == 0
    uint32_t cfg = 0;
#else
    uint32_t cfg = SX127X_FIFO_RX_BASE_ADDR_MAX;
    cfg = (cfg << 8) + SX127X_FIFO_TX_BASE_ADDR_MAX;
    cfg = (cfg << 8) + SX127X_FIFO_RX_BASE_ADDR_MAX;
#endif
    //uint8_t WORD_ALIGNED_ATTR cfg[3] = {SX127X_FIFO_RX_BASE_ADDR_MAX,
    //                                    SX127X_FIFO_TX_BASE_ADDR_MAX,
    //                                    SX127X_FIFO_RX_BASE_ADDR_MAX};
    writeRegisterBurst(SX127X_REG_FIFO_ADDR_PTR, (uint8_t *)&cfg, 3);

    //reg_dio1_isr_mask_write(SX127X_MASK_IRQ_FLAG_RX_DONE & SX127X_MASK_IRQ_FLAG_PAYLOAD_CRC_ERROR);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

//uint32_t DRAM_ATTR __RXdataBuffer[16 / 4]; // ESP requires aligned buffer
//uint8_t * DRAM_ATTR RXdataBuffer = (uint8_t *)&__RXdataBuffer;
static uint8_t DMA_ATTR RXdataBuffer[RX_BUFFER_LEN];

void ICACHE_RAM_ATTR SX127xDriver::RXnbISR(uint8_t irqs)
{
    // Ignore if CRC is invalid
    if ((!(irqs & SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR)) &&
        (irqs & SX127X_CLEAR_IRQ_FLAG_RX_DONE))
    {
        // make sure the buffer is clean
        memset(RXdataBuffer, 0, RX_BUFFER_LEN);
        // fetch data from modem
        readRegisterBurst((uint8_t)SX127X_REG_FIFO, RX_BUFFER_LEN, (uint8_t *)RXdataBuffer);
        // fetch RSSI and SNR
        GetLastRssiSnr();
        NonceRX++;
        // Push to application if callback is set
        RXdoneCallback1(RXdataBuffer);
        //RXdoneCallback2();
    }
}

void ICACHE_RAM_ATTR SX127xDriver::StopContRX()
{
    SetMode(SX127X_STANDBY);
    p_state_dio0_isr = NONE;
}

void ICACHE_RAM_ATTR SX127xDriver::RXnb(uint32_t freq)
{
    RxConfig(freq);
    p_state_dio0_isr = RX_DONE;
    SetMode(SX127X_RXCONTINUOUS);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t ICACHE_RAM_ATTR SX127xDriver::RX(uint32_t freq, uint8_t *data, uint8_t length, uint32_t timeout)
{
    RxConfig(freq);
    SetMode(SX127X_RXSINGLE);

    uint32_t startTime = millis();

    while (p_state_dio0_isr == NONE)
    {
        if ((p_state_dio0_isr == RX_TIMEOUT) ||
            (timeout <= (millis() - startTime)))
        {
            return (ERR_RX_TIMEOUT);
        }
    }

    if (p_state_dio0_isr == CRC_ERROR)
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

uint8_t SX127xDriver::RunCAD(uint32_t timeout)
{
    p_state_dio0_isr = p_state_dio1_isr = NONE;

    SetMode(SX127X_STANDBY);

    //reg_dio1_isr_mask_write((SX127X_MASK_IRQ_FLAG_CAD_DONE & SX127X_MASK_IRQ_FLAG_CAD_DETECTED));

    SetMode(SX127X_CAD);

    uint32_t startTime = millis();

    while (p_state_dio0_isr != CAD_DETECTED)
    {
        if (timeout <= (millis() - startTime))
        {
            return (CHANNEL_FREE);
        }
    }
    return (PREAMBLE_DETECTED);
}

////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// config functions //////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

inline __attribute__((always_inline)) void ICACHE_RAM_ATTR SX127xDriver::_change_mode_val(uint8_t mode)
{
    p_RegOpMode &= (~SX127X_CAD);
    p_RegOpMode |= (mode & SX127X_CAD);
}

void ICACHE_RAM_ATTR SX127xDriver::SetMode(uint8_t mode)
{ //if radio is not already in the required mode set it to the requested mode
    mode &= SX127X_CAD;
    if (mode != (p_RegOpMode & SX127X_CAD))
    {
        //p_RegOpMode &= (~SX127X_CAD);
        //p_RegOpMode |= (mode & SX127X_CAD);
        _change_mode_val(mode);
        writeRegister(SX127X_REG_OP_MODE, p_RegOpMode);
    }
}

uint8_t SX127xDriver::Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord, uint8_t crc)
{
    uint8_t newBandwidth, newSpreadingFactor, newCodingRate;

    if (freq == 0)
        freq = currFreq;
    if (syncWord == 0)
        syncWord = _syncWord;

    if ((freq < 137000000) ||
        ((RFmodule == RFMOD_SX1276) && (freq > 1020000000)) ||
        ((RFmodule == RFMOD_SX1278) && (freq > 525000000)))
    {
        DEBUG_PRINT("Invalid Frequnecy!: ");
        DEBUG_PRINTLN(freq);
        return (ERR_INVALID_FREQUENCY);
    }

    // check the supplied BW, CR and SF values
    switch (bw)
    {
        case BW_7_80_KHZ:
            newBandwidth = SX127X_BW_7_80_KHZ;
            break;
        case BW_10_40_KHZ:
            newBandwidth = SX127X_BW_10_40_KHZ;
            break;
        case BW_15_60_KHZ:
            newBandwidth = SX127X_BW_15_60_KHZ;
            break;
        case BW_20_80_KHZ:
            newBandwidth = SX127X_BW_20_80_KHZ;
            break;
        case BW_31_25_KHZ:
            newBandwidth = SX127X_BW_31_25_KHZ;
            break;
        case BW_41_70_KHZ:
            newBandwidth = SX127X_BW_41_70_KHZ;
            break;
        case BW_62_50_KHZ:
            newBandwidth = SX127X_BW_62_50_KHZ;
            break;
        case BW_125_00_KHZ:
            newBandwidth = SX127X_BW_125_00_KHZ;
            break;
        case BW_250_00_KHZ:
            newBandwidth = SX127X_BW_250_00_KHZ;
            break;
        case BW_500_00_KHZ:
            newBandwidth = SX127X_BW_500_00_KHZ;
            break;
        default:
            return (ERR_INVALID_BANDWIDTH);
    }

    switch (sf)
    {
        case SF_6:
            newSpreadingFactor = SX127X_SF_6;
            break;
        case SF_7:
            newSpreadingFactor = SX127X_SF_7;
            break;
        case SF_8:
            newSpreadingFactor = SX127X_SF_8;
            break;
        case SF_9:
            newSpreadingFactor = SX127X_SF_9;
            break;
        case SF_10:
            newSpreadingFactor = SX127X_SF_10;
            break;
        case SF_11:
            newSpreadingFactor = SX127X_SF_11;
            break;
        case SF_12:
            newSpreadingFactor = SX127X_SF_12;
            break;
        default:
            return (ERR_INVALID_SPREADING_FACTOR);
    }

    switch (cr)
    {
        case CR_4_5:
            newCodingRate = SX127X_CR_4_5;
            break;
        case CR_4_6:
            newCodingRate = SX127X_CR_4_6;
            break;
        case CR_4_7:
            newCodingRate = SX127X_CR_4_7;
            break;
        case CR_4_8:
            newCodingRate = SX127X_CR_4_8;
            break;
        default:
            return (ERR_INVALID_CODING_RATE);
    }

    // configure common registers
    SX127xConfig(newBandwidth, newSpreadingFactor, newCodingRate, freq, syncWord, crc);

    // save the new settings
    currBW = bw;
    currSF = sf;
    currCR = cr;
    currFreq = freq;

    return ERR_NONE;
}

void SX127xDriver::SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord, uint8_t crc)
{
    uint8_t reg;

    // set mode to SLEEP
    SetMode(SX127X_SLEEP);

    // set LoRa mode
    reg_op_mode_mode_lora();

    SetFrequency(freq, 0xff); // 0xff = skip mode set calls

    // output power configuration
    SetOutputPower(currPWR);
    //if (RFmodule == RFMOD_SX1276)
    //    // Increase max current limit
    //    writeRegister(SX127X_REG_OCP, SX127X_OCP_ON | 18); // 15 (120mA) -> 18 (150mA)
    //else
        writeRegister(SX127X_REG_OCP, SX127X_OCP_ON | 23); //200ma
    // output power configuration
    //writeRegister(SX127X_REG_PA_DAC, (0x80 | SX127X_PA_BOOST_OFF));

    //writeRegister(SX127X_REG_LNA, SX127X_LNA_GAIN_1 | SX127X_LNA_BOOST_ON);

    // turn off frequency hopping
    writeRegister(SX127X_REG_HOP_PERIOD, SX127X_HOP_PERIOD_OFF);

    // basic setting (bw, cr, sf, header mode and CRC)
    reg = (sf | SX127X_TX_MODE_SINGLE); // RX timeout MSB = 0b00
    reg |= (crc) ? SX127X_RX_CRC_MODE_ON : SX127X_RX_CRC_MODE_OFF;
    writeRegister(SX127X_REG_MODEM_CONFIG_2, reg);

    if (sf == SX127X_SF_6)
    {
        reg = SX127X_DETECT_OPTIMIZE_SF_6;
        writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
    }
    else
    {
        reg = SX127X_DETECT_OPTIMIZE_SF_7_12;
        writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
    }
    if (bw == SX127X_BW_500_00_KHZ)
        reg |= (1u << 7); // Errata: bit 7 to 1
    writeRegister(SX127X_REG_DETECT_OPTIMIZE, reg);

    //  Errata fix
    switch (bw)
    {
        case SX127X_BW_7_80_KHZ:
            p_freqOffset = 7810;
            reg = 0x48;
            break;
        case SX127X_BW_10_40_KHZ:
            p_freqOffset = 10420;
            reg = 0x44;
            break;
        case SX127X_BW_15_60_KHZ:
            p_freqOffset = 15620;
            reg = 0x44;
            break;
        case SX127X_BW_20_80_KHZ:
            p_freqOffset = 20830;
            reg = 0x44;
            break;
        case SX127X_BW_31_25_KHZ:
            p_freqOffset = 31250;
            reg = 0x44;
            break;
        case SX127X_BW_41_70_KHZ:
            p_freqOffset = 41670;
            reg = 0x44;
            break;
        case SX127X_BW_62_50_KHZ:
        case SX127X_BW_125_00_KHZ:
        case SX127X_BW_250_00_KHZ:
            p_freqOffset = 0;
            reg = 0x40;
            break;
        case SX127X_BW_500_00_KHZ:
        default:
            p_freqOffset = 0;
            reg = 0;
            break;
    }

    if (reg)
        writeRegister(0x2F, reg);

    if (bw != SX127X_BW_500_00_KHZ)
        writeRegister(0x30, 0x00);

    // set the sync word
    writeRegister(SX127X_REG_SYNC_WORD, syncWord);

    reg = SX127X_AGC_AUTO_ON;
    if ((bw == SX127X_BW_125_00_KHZ) && ((sf == SX127X_SF_11) || (sf == SX127X_SF_12)))
    {
        reg |= SX127X_LOW_DATA_RATE_OPT_ON;
    }
    else
    {
        reg |= SX127X_LOW_DATA_RATE_OPT_OFF;
    }
    writeRegister(SX127X_REG_MODEM_CONFIG_3, reg);

    reg = bw | cr;
    if (headerExplMode == false)
        reg |= SX127X_HEADER_IMPL_MODE;
    writeRegister(SX127X_REG_MODEM_CONFIG_1, reg);

    if (bw == SX127X_BW_500_00_KHZ)
    {
        //datasheet errata reconmendation http://caxapa.ru/thumbs/972894/SX1276_77_8_ErrataNote_1.1_STD.pdf
        writeRegister(0x36, 0x02);
        writeRegister(0x3a, 0x64);
    }
    else
    {
        writeRegister(0x36, 0x03);
    }

    // set mode to STANDBY
    SetMode(SX127X_STANDBY);
}

uint32_t ICACHE_RAM_ATTR SX127xDriver::getCurrBandwidth() const
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

void ICACHE_RAM_ATTR SX127xDriver::setPPMoffsetReg(int32_t error_hz, uint32_t frf)
{
    if (!frf) // use locally stored value if not defined
        frf = currFreq;
    if (!frf)
        return;
    // Calc new PPM
    error_hz *= 95;
    error_hz /= (frf / 1E4);

    uint8_t regValue = (uint8_t)error_hz;
    if (regValue == p_ppm_off)
        return;
    p_ppm_off = regValue;
    writeRegister(SX127x_REG_PPMOFFSET, regValue);
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

    // Calculate Hz error where XTAL is 32MHz
    int64_t tmp_f = intFreqError;
    tmp_f <<= 11;
    tmp_f *= getCurrBandwidth();
    tmp_f /= 1953125000;
    intFreqError = tmp_f;
    //intFreqError = ((tmp_f << 11) * getCurrBandwidth()) / 1953125000;

    return intFreqError;
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketRSSIUnsigned()
{
    LastPacketRssiRaw = readRegister(SX127X_REG_PKT_RSSI_VALUE);
    return (LastPacketRssiRaw);
}

int16_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketRSSI()
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

int16_t ICACHE_RAM_ATTR SX127xDriver::GetCurrRSSI() const
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

uint8_t ICACHE_RAM_ATTR SX127xDriver::GetIRQFlags()
{
    return readRegister(SX127X_REG_IRQ_FLAGS);
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
    writeRegisterBurst(SX127X_REG_IRQ_FLAGS_MASK, cfg, sizeof(cfg));

    /*if (p_isr_mask != mask)
    {
        writeRegister(SX127X_REG_IRQ_FLAGS_MASK, mask);
        p_isr_mask = mask;
    }*/
}
