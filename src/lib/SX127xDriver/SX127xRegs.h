#pragma once

typedef enum
{
    SX127x_OPMODE_FSK_OOK = 0b00000000,
    SX127x_OPMODE_LORA = 0b10000000,
    SX127X_ACCESS_SHARED_REG_OFF = 0b00000000,
    SX127X_ACCESS_SHARED_REG_ON = 0b01000000
} SX127x_ModulationModes;

typedef enum
{
    SX127x_OPMODE_SLEEP = 0b00000000,
    SX127x_OPMODE_STANDBY = 0b00000001,
    SX127x_OPMODE_FSTX = 0b00000010,
    SX127x_OPMODE_TX = 0b00000011,
    SX127x_OPMODE_FSRX = 0b00000100,
    SX127x_OPMODE_RXCONTINUOUS = 0b00000101,
    SX127x_OPMODE_RXSINGLE = 0b00000110,
    SX127x_OPMODE_CAD = 0b00000111,
} SX127x_RadioOPmodes;

#if defined(RADIO_SX1272)
typedef enum
{
    // Only 125 to 500 KHZ supported
    SX127x_BW_7_80_KHZ = 0b00000000,
    SX127x_BW_10_40_KHZ = 0b00000000,
    SX127x_BW_15_60_KHZ = 0b00000000,
    SX127x_BW_20_80_KHZ = 0b00000000,
    SX127x_BW_31_25_KHZ = 0b00000000,
    SX127x_BW_41_70_KHZ = 0b00000000,
    SX127x_BW_62_50_KHZ = 0b00000000,
    SX127x_BW_125_00_KHZ = 0b00000000,
    SX127x_BW_250_00_KHZ = 0b01000000,
    SX127x_BW_500_00_KHZ = 0b10000000
} SX127x_Bandwidth;
#else
typedef enum
{
    SX127x_BW_7_80_KHZ = 0b00000000,
    SX127x_BW_10_40_KHZ = 0b00010000,
    SX127x_BW_15_60_KHZ = 0b00100000,
    SX127x_BW_20_80_KHZ = 0b00110000,
    SX127x_BW_31_25_KHZ = 0b01000000,
    SX127x_BW_41_70_KHZ = 0b01010000,
    SX127x_BW_62_50_KHZ = 0b01100000,
    SX127x_BW_125_00_KHZ = 0b01110000,
    SX127x_BW_250_00_KHZ = 0b10000000,
    SX127x_BW_500_00_KHZ = 0b10010000
} SX127x_Bandwidth;
#endif

typedef enum
{
    SX127x_SF_6 = 0b01100000,
    SX127x_SF_7 = 0b01110000,
    SX127x_SF_8 = 0b10000000,
    SX127x_SF_9 = 0b10010000,
    SX127x_SF_10 = 0b10100000,
    SX127x_SF_11 = 0b10110000,
    SX127x_SF_12 = 0b11000000
} SX127x_SpreadingFactor;
#define SX127X_SPREADING_FACTOR_MASK 0b11110000

#if defined(RADIO_SX1272)
typedef enum
{
    SX127x_CR_4_5 = 0b00001000,
    SX127x_CR_4_6 = 0b00010000,
    SX127x_CR_4_7 = 0b00011000,
    SX127x_CR_4_8 = 0b00100000,
} SX127x_CodingRate;
#else
typedef enum
{
    SX127x_CR_4_5 = 0b00000010,
    SX127x_CR_4_6 = 0b00000100,
    SX127x_CR_4_7 = 0b00000110,
    SX127x_CR_4_8 = 0b00001000,
} SX127x_CodingRate;
#endif

// SX127x series common registers
#define SX127X_REG_FIFO 0x00
#define SX127X_REG_OP_MODE 0x01
#define SX127X_REG_FRF_MSB 0x06
#define SX127X_REG_FRF_MID 0x07
#define SX127X_REG_FRF_LSB 0x08
#define SX127X_REG_PA_CONFIG 0x09
#define SX127X_REG_PA_RAMP 0x0A
#define SX127X_REG_OCP 0x0B
#define SX127X_REG_LNA 0x0C
#define SX127X_REG_FIFO_ADDR_PTR 0x0D
#define SX127X_REG_FIFO_TX_BASE_ADDR 0x0E
#define SX127X_REG_FIFO_RX_BASE_ADDR 0x0F
#define SX127X_REG_FIFO_RX_CURRENT_ADDR 0x10
#define SX127X_REG_IRQ_FLAGS_MASK 0x11
#define SX127X_REG_IRQ_FLAGS 0x12
#define SX127X_REG_RX_NB_BYTES 0x13
#define SX127X_REG_RX_HEADER_CNT_VALUE_MSB 0x14
#define SX127X_REG_RX_HEADER_CNT_VALUE_LSB 0x15
#define SX127X_REG_RX_PACKET_CNT_VALUE_MSB 0x16
#define SX127X_REG_RX_PACKET_CNT_VALUE_LSB 0x17
#define SX127X_REG_MODEM_STAT 0x18
#define SX127X_REG_PKT_SNR_VALUE 0x19
#define SX127X_REG_PKT_RSSI_VALUE 0x1A
#define SX127X_REG_RSSI_VALUE 0x1B
#define SX127X_REG_HOP_CHANNEL 0x1C
#define SX127X_REG_MODEM_CONFIG_1 0x1D
#define SX127X_REG_MODEM_CONFIG_2 0x1E
#define SX127X_REG_SYMB_TIMEOUT_MSB 0x1E
#define SX127X_REG_SYMB_TIMEOUT_MSB_MASK 0b00000011
#define SX127X_REG_SYMB_TIMEOUT_LSB 0x1F
#define SX127X_REG_PREAMBLE_MSB 0x20
#define SX127X_REG_PREAMBLE_LSB 0x21
#define SX127X_REG_PAYLOAD_LENGTH 0x22
#define SX127X_REG_MAX_PAYLOAD_LENGTH 0x23
#define SX127X_REG_HOP_PERIOD 0x24
#define SX127X_REG_FIFO_RX_BYTE_ADDR 0x25
#define SX127X_REG_FEI_MSB 0x28
#define SX127X_REG_FEI_MID 0x29
#define SX127X_REG_FEI_LSB 0x2A
#define SX127X_REG_RSSI_WIDEBAND 0x2C
#define SX127X_REG_DETECT_OPTIMIZE 0x31
#define SX127X_REG_INVERT_IQ 0b01000001
#define SX127X_REG_INVERT_IQ_MASK 0b01000001
#define SX127X_REG_DETECTION_THRESHOLD 0x37
#define SX127X_REG_SYNC_WORD 0x39
#define SX127X_REG_DIO_MAPPING_1 0x40
#define SX127X_REG_DIO_MAPPING_2 0x41
#define SX127X_REG_VERSION 0x42

#if defined(RADIO_SX1272)
    #define SX127X_VERSION 0x22
#else
    #define SX127X_VERSION 0x12
#endif

// SX127X_REG_PA_CONFIG
#define SX127X_PA_SELECT_RFO 0b00000000    //  7     7     RFO pin output, power limited to +14 dBm
#define SX127X_PA_SELECT_BOOST 0b10000000  //  7     7     PA_BOOST pin output, power limited to +20 dBm
#define SX127X_OUTPUT_POWER 0b00001111     //  3     0     output power: P_out = 17 - (15 - OUTPUT_POWER) [dBm] for PA_SELECT_BOOST
#define SX127X_MAX_OUTPUT_POWER_RFO_HF 0b00000000 //       Max output power when using RFO_HF
#define SX127X_MAX_OUTPUT_POWER 0b01110000 //              Enable max output power
#define SX127X_MAX_OUTPUT_POWER_INVALID 0b00010000 //      A value for the MaxPower field that is not used by ExpressLRS
// SX127X_REG_OCP
#define SX127X_OCP_OFF 0b00000000   //  5     5     PA overload current protection disabled
#define SX127X_OCP_ON 0b00100000    //  5     5     PA overload current protection enabled
#define SX127X_OCP_TRIM 0b00001011  //  4     0     OCP current: I_max(OCP_TRIM = 0b1011) = 100 mA
#define SX127X_OCP_150MA 0b00010010 //  4     0     OCP current: I_max(OCP_TRIM = 10010) = 150 mA
#define SX127X_OCP_MASK 0b00111111

// SX127X_REG_LNA
#define SX127X_LNA_GAIN_0 0b00000000    //  7     5     LNA gain setting:   not used
#define SX127X_LNA_GAIN_1 0b00100000    //  7     5                         max gain
#define SX127X_LNA_GAIN_2 0b01000000    //  7     5                         .
#define SX127X_LNA_GAIN_3 0b01100000    //  7     5                         .
#define SX127X_LNA_GAIN_4 0b10000000    //  7     5                         .
#define SX127X_LNA_GAIN_5 0b10100000    //  7     5                         .
#define SX127X_LNA_GAIN_6 0b11000000    //  7     5                         min gain
#define SX127X_LNA_GAIN_7 0b11100000    //  7     5                         not used
#define SX127X_LNA_BOOST_OFF 0b00000000 //  1     0     default LNA current
#define SX127X_LNA_BOOST_ON 0b00000011  //  1     0     150% LNA current

#define SX127X_TX_MODE_SINGLE 0b00000000 //  3     3     single TX
#define SX127X_TX_MODE_CONT 0b00001000   //  3     3     continuous TX
#define SX127X_TX_MODE_MASK 0b00001000   //  3     3
#define SX127X_RX_TIMEOUT_MSB 0b00000000 //  1     0

// SX127X_REG_SYMB_TIMEOUT_LSB
#define SX127X_RX_TIMEOUT_LSB 0b01100100 //  7     0     10 bit RX operation timeout

// SX127X_REG_PREAMBLE_MSB + REG_PREAMBLE_LSB
#define SX127X_PREAMBLE_LENGTH_MSB 0b00000000 //  7     0     2 byte preamble length setting: l_P = PREAMBLE_LENGTH + 4.25
#define SX127X_PREAMBLE_LENGTH_LSB 0b00001000 //  7     0         where l_p = preamble length
//#define SX127X_PREAMBLE_LENGTH_LSB                    0b00000100  //  7     0         where l_p = preamble length  //CHANGED

// SX127X_REG_DETECT_OPTIMIZE
#define SX127X_DETECT_OPTIMIZE_SF_6 0b00000101    //  2     0     SF6 detection optimization
#define SX127X_DETECT_OPTIMIZE_SF_7_12 0b00000011 //  2     0     SF7 to SF12 detection optimization
#define SX127X_DETECT_OPTIMIZE_SF_MASK 0b00000111 //  2     0

// SX127X_REG_DETECTION_THRESHOLD
#define SX127X_DETECTION_THRESHOLD_SF_6 0b00001100    //  7     0     SF6 detection threshold
#define SX127X_DETECTION_THRESHOLD_SF_7_12 0b00001010 //  7     0     SF7 to SF12 detection threshold

// SX127X_REG_PA_DAC
#define SX127X_PA_BOOST_OFF 0b00000100 //  2     0     PA_BOOST disabled
#define SX127X_PA_BOOST_ON 0b00000111  //  2     0     +20 dBm on PA_BOOST when OUTPUT_POWER = 0b1111

// SX127X_REG_HOP_PERIOD
#define SX127X_HOP_PERIOD_OFF 0b00000000 //  7     0     number of periods between frequency hops; 0 = disabled
#define SX127X_HOP_PERIOD_MAX 0b11111111 //  7     0

// SX127X_REG_DIO_MAPPING_1
#define SX127X_DIO0_RX_DONE 0b00000000             //  7     6
#define SX127X_DIO0_TX_DONE 0b01000000             //  7     6
#define SX127X_DIO0_RXTX_DONE 0b11000000           //  7     6
#define SX127X_DIO0_CAD_DONE 0b10000000            //  7     6
#define SX127X_DIO0_MASK 0b11000000                //  7     6
#define SX127X_DIO1_RX_TIMEOUT 0b00000000          //  5     4
#define SX127X_DIO1_FHSS_CHANGE_CHANNEL 0b00010000 //  5     4
#define SX127X_DIO1_CAD_DETECTED 0b00100000        //  5     4

// SX127X_REG_IRQ_FLAGS
#define SX127X_CLEAR_IRQ_FLAG_ALL 0b11111111
#define SX127X_CLEAR_IRQ_FLAG_RX_TIMEOUT 0b10000000          //  7     7     timeout
#define SX127X_CLEAR_IRQ_FLAG_RX_DONE 0b01000000             //  6     6     packet reception complete
#define SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR 0b00100000   //  5     5     payload CRC error
#define SX127X_CLEAR_IRQ_FLAG_VALID_HEADER 0b00010000        //  4     4     valid header received
#define SX127X_CLEAR_IRQ_FLAG_TX_DONE 0b00001000             //  3     3     payload transmission complete
#define SX127X_CLEAR_IRQ_FLAG_CAD_DONE 0b00000100            //  2     2     CAD complete
#define SX127X_CLEAR_IRQ_FLAG_FHSS_CHANGE_CHANNEL 0b00000010 //  1     1     FHSS change channel
#define SX127X_CLEAR_IRQ_FLAG_CAD_DETECTED 0b00000001        //  0     0     valid LoRa signal detected during CAD operation
#define SX127X_CLEAR_IRQ_FLAG_NONE 0b00000000

// SX127X_REG_IRQ_FLAGS_MASK
#define SX127X_MASK_IRQ_FLAG_RX_TIMEOUT 0b01111111          //  7     7     timeout
#define SX127X_MASK_IRQ_FLAG_RX_DONE 0b10111111             //  6     6     packet reception complete
#define SX127X_MASK_IRQ_FLAG_PAYLOAD_CRC_ERROR 0b11011111   //  5     5     payload CRC error
#define SX127X_MASK_IRQ_FLAG_VALID_HEADER 0b11101111        //  4     4     valid header received
#define SX127X_MASK_IRQ_FLAG_TX_DONE 0b11110111             //  3     3     payload transmission complete
#define SX127X_MASK_IRQ_FLAG_CAD_DONE 0b11111011            //  2     2     CAD complete
#define SX127X_MASK_IRQ_FLAG_FHSS_CHANGE_CHANNEL 0b11111101 //  1     1     FHSS change channel
#define SX127X_MASK_IRQ_FLAG_CAD_DETECTED 0b11111110        //  0     0     valid LoRa signal detected during CAD operation

// SX127X_REG_FIFO_TX_BASE_ADDR
#define SX127X_FIFO_TX_BASE_ADDR_MAX 0b00000000 //  7     0     allocate the entire FIFO buffer for TX only

// SX127X_REG_FIFO_RX_BASE_ADDR
#define SX127X_FIFO_RX_BASE_ADDR_MAX 0b00000000 //  7     0     allocate the entire FIFO buffer for RX only

// SX127X_REG_SYNC_WORD
//#define SX127X_SYNC_WORD 0xC8 //  200   0     default ExpressLRS sync word - 200Hz
#define SX127X_SYNC_WORD                              0x12        //  18    0     default LoRa sync word
#define SX127X_SYNC_WORD_LORAWAN 0x34 //  52    0     sync word reserved for LoRaWAN networks

#define IRQpin 26

///Added by Sandro
#define SX127x_TXCONTINUOUSMODE_MASK 0xF7
#define SX127x_TXCONTINUOUSMODE_ON 0x08
#define SX127x_TXCONTINUOUSMODE_OFF 0x00
#define SX127x_PPMOFFSET 0x27

///// SX1278 Regs /////
//SX1278 specific register map
#define SX1278_REG_MODEM_CONFIG_3 0x26
#define SX1278_REG_TCXO 0x4B
#define SX1278_REG_PA_DAC 0x4D
#define SX1278_REG_FORMER_TEMP 0x5D
#define SX1278_REG_AGC_REF 0x61
#define SX1278_REG_AGC_THRESH_1 0x62
#define SX1278_REG_AGC_THRESH_2 0x63
#define SX1278_REG_AGC_THRESH_3 0x64
#define SX1278_REG_PLL 0x70

//SX1278 LoRa modem settings
//SX1278_REG_OP_MODE                                                  MSB   LSB   DESCRIPTION
#define SX1278_HIGH_FREQ 0b00000000 //  3     3     access HF test registers
#define SX1278_LOW_FREQ 0b00001000  //  3     3     access LF test registers

//SX1278_REG_FRF_MSB + REG_FRF_MID + REG_FRF_LSB
#define SX1278_FRF_MSB 0x6C //  7     0     carrier frequency setting: f_RF = (F(XOSC) * FRF)/2^19
#define SX1278_FRF_MID 0x80 //  7     0         where F(XOSC) = 32 MHz
#define SX1278_FRF_LSB 0x00 //  7     0               FRF = 3 byte value of FRF registers

//SX1278_REG_PA_CONFIG
#define SX1278_MAX_POWER 0b01110000 //  6     4     max power: P_max = 10.8 + 0.6*MAX_POWER [dBm]; P_max(MAX_POWER = 0b111) = 15 dBm
//#define SX1278_MAX_POWER                              0b00010000  //  6     4     changed

//SX1278_REG_LNA
#define SX1278_LNA_BOOST_LF_OFF 0b00000000 //  4     3     default LNA current

//SX127X_REG_MODEM_CONFIG_1
#if defined(RADIO_SX1272)
    #define SX127x_HEADER_EXPL_MODE 0b00000000 //  2     2     explicit header mode
    #define SX127x_HEADER_IMPL_MODE 0b00000100 //  2     2     implicit header mode
#else
    #define SX127x_HEADER_EXPL_MODE 0b00000000 //  0     0     explicit header mode
    #define SX127x_HEADER_IMPL_MODE 0b00000001 //  0     0     implicit header mode
#endif

//SX1278_REG_MODEM_CONFIG_2
#define SX1278_RX_CRC_MODE_OFF 0b00000000 //  2     2     CRC disabled
#define SX1278_RX_CRC_MODE_ON 0b00000100  //  2     2     CRC enabled
#define SX1278_RX_CRC_MODE_MASK 0b00000100

//SX1272_REG_MODEM_CONFIG_1
#define SX1272_RX_CRC_MODE_OFF 0b00000000 //  1     1     CRC disabled
#define SX1272_RX_CRC_MODE_ON 0b00000010  //  1     1     CRC enabled
#define SX1272_RX_CRC_MODE_MASK 0b00000010

//SX1278_REG_MODEM_CONFIG_3
#define SX1278_LOW_DATA_RATE_OPT_OFF 0b00000000 //  3     3     low data rate optimization disabled
#define SX1278_LOW_DATA_RATE_OPT_ON 0b00001000  //  3     3     low data rate optimization enabled
#define SX1278_AGC_AUTO_OFF 0b00000000          //  2     2     LNA gain set by REG_LNA
#define SX1278_AGC_AUTO_ON 0b00000100           //  2     2     LNA gain set by internal AGC loop


#define ERR_NONE 0x00
#define ERR_CHIP_NOT_FOUND 0x01
#define ERR_EEPROM_NOT_INITIALIZED 0x02

#define ERR_PACKET_TOO_LONG 0x10
#define ERR_TX_TIMEOUT 0x11

#define ERR_RX_TIMEOUT 0x20
#define ERR_CRC_MISMATCH 0x21

#define ERR_INVALID_BANDWIDTH 0x30
#define ERR_INVALID_SPREADING_FACTOR 0x31
#define ERR_INVALID_CODING_RATE 0x32
#define ERR_INVALID_FREQUENCY 0x33

#define ERR_INVALID_BIT_RANGE 0x40

#define CHANNEL_FREE 0x50
#define PREAMBLE_DETECTED 0x51

#define SPI_READ 0b00000000
#define SPI_WRITE 0b10000000
