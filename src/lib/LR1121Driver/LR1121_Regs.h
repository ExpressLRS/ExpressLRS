/*!
 * @file      lr11xx_radio_types.h
 *
 * @brief     Radio driver types for LR11XX
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2021. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#define LR1121_XTAL_FREQ 32000000
#define FREQ_STEP 61.03515625 // TODO check and fix... this is a cut and past from sx1276

#define LR1121_POWER_MIN_LP_PA (-17) // Low Power PA
#define LR1121_POWER_MAX_LP_PA  (14)
#define LR1121_POWER_MIN_HP_PA  (-9) // High Power PA
#define LR1121_POWER_MAX_HP_PA  (22)
#define LR1121_POWER_MIN_HF_PA (-18) // High Frequency PA
#define LR1121_POWER_MAX_HF_PA  (13)

#define LR1121_IRQ_TX_DONE 0x00000004
#define LR1121_IRQ_RX_DONE 0x00000008
#define LR1121_IRQ_CRC_ERR 0x00000080
#define LR1121_IRQ_RADIO_NONE 0
// #define LR1121_IRQ_RADIO_ALL 0xFFFFFFFF

typedef enum
{
    LR1121_LORA_PACKET_VARIABLE_LENGTH = 0x00, //!< The packet is on variable size, header included
    LR1121_LORA_PACKET_FIXED_LENGTH = 0x01,    //!< The packet is known on both sides, no header included in the packet
    LR1121_LORA_PACKET_EXPLICIT = LR1121_LORA_PACKET_VARIABLE_LENGTH,
    LR1121_LORA_PACKET_IMPLICIT = LR1121_LORA_PACKET_FIXED_LENGTH,
} lr11xx_RadioLoRaPacketLengthsModes_t;

typedef enum
{
    LR1121_MODE_SLEEP = 0x00, //! The radio is in sleep mode
    LR1121_MODE_STDBY_RC,     //! The radio is in standby mode with RC oscillator
    LR1121_MODE_STDBY_XOSC,   //! The radio is in standby mode with XOSC oscillator
    LR1121_MODE_FS,           //! The radio is in frequency synthesis mode
    LR1121_MODE_RX,           //! The radio is in receive mode
    LR1121_MODE_RX_CONT,      //! The radio is in continuous receive mode
    LR1121_MODE_TX,           //! The radio is in transmit mode
    LR1121_MODE_CAD           //! The radio is in channel activity detection mode
} lr11xx_RadioOperatingModes_t;

enum
{
    LR11XX_RADIO_RESET_STATS_OC               = 0x0200,
    LR11XX_RADIO_GET_STATS_OC                 = 0x0201,
    LR11XX_RADIO_GET_PKT_TYPE_OC              = 0x0202,
    LR11XX_RADIO_GET_RXBUFFER_STATUS_OC       = 0x0203,
    LR11XX_RADIO_GET_PKT_STATUS_OC            = 0x0204,
    LR11XX_RADIO_GET_RSSI_INST_OC             = 0x0205,
    LR11XX_RADIO_SET_GFSK_SYNC_WORD_OC        = 0x0206,
    LR11XX_RADIO_SET_LORA_PUBLIC_NETWORK_OC   = 0x0208,
    LR11XX_RADIO_SET_RX_OC                    = 0x0209,
    LR11XX_RADIO_SET_TX_OC                    = 0x020A,
    LR11XX_RADIO_SET_RF_FREQUENCY_OC          = 0x020B,
    LR11XX_RADIO_AUTOTXRX_OC                  = 0x020C,
    LR11XX_RADIO_SET_CAD_PARAMS_OC            = 0x020D,
    LR11XX_RADIO_SET_PKT_TYPE_OC              = 0x020E,
    LR11XX_RADIO_SET_MODULATION_PARAM_OC      = 0x020F,
    LR11XX_RADIO_SET_PKT_PARAM_OC             = 0x0210,
    LR11XX_RADIO_SET_TX_PARAMS_OC             = 0x0211,
    LR11XX_RADIO_SET_PKT_ADRS_OC              = 0x0212,
    LR11XX_RADIO_SET_RX_TX_FALLBACK_MODE_OC   = 0x0213,
    LR11XX_RADIO_SET_RX_DUTY_CYCLE_OC         = 0x0214,
    LR11XX_RADIO_SET_PA_CFG_OC                = 0x0215,
    LR11XX_RADIO_STOP_TIMEOUT_ON_PREAMBLE_OC  = 0x0217,
    LR11XX_RADIO_SET_CAD_OC                   = 0x0218,
    LR11XX_RADIO_SET_TX_CW_OC                 = 0x0219,
    LR11XX_RADIO_SET_TX_INFINITE_PREAMBLE_OC  = 0x021A,
    LR11XX_RADIO_SET_LORA_SYNC_TIMEOUT_OC     = 0x021B,
    LR11XX_RADIO_SET_GFSK_CRC_PARAMS_OC       = 0x0224,
    LR11XX_RADIO_SET_GFSK_WHITENING_PARAMS_OC = 0x0225,
    LR11XX_RADIO_SET_RX_BOOSTED_OC            = 0x0227,
    LR11XX_RADIO_SET_RSSI_CALIBRATION_OC      = 0x0229,
    LR11XX_RADIO_SET_LORA_SYNC_WORD_OC        = 0x022B,
    LR11XX_RADIO_SET_LR_FHSS_SYNC_WORD_OC     = 0x022D,
    LR11XX_RADIO_CFG_BLE_BEACON_OC            = 0x022E,
    LR11XX_RADIO_GET_LORA_RX_INFO_OC          = 0x0230,
    LR11XX_RADIO_BLE_BEACON_SEND_OC           = 0x0231,
};

enum
{
    LR11XX_SYSTEM_GET_STATUS_OC              = 0x0100,
    LR11XX_SYSTEM_GET_VERSION_OC             = 0x0101,
    LR11XX_SYSTEM_GET_ERRORS_OC              = 0x010D,
    LR11XX_SYSTEM_CLEAR_ERRORS_OC            = 0x010E,
    LR11XX_SYSTEM_CALIBRATE_OC               = 0x010F,
    LR11XX_SYSTEM_SET_REGMODE_OC             = 0x0110,
    LR11XX_SYSTEM_CALIBRATE_IMAGE_OC         = 0x0111,
    LR11XX_SYSTEM_SET_DIO_AS_RF_SWITCH_OC    = 0x0112,
    LR11XX_SYSTEM_SET_DIOIRQPARAMS_OC        = 0x0113,
    LR11XX_SYSTEM_CLEAR_IRQ_OC               = 0x0114,
    LR11XX_SYSTEM_CFG_LFCLK_OC               = 0x0116,
    LR11XX_SYSTEM_SET_TCXO_MODE_OC           = 0x0117,
    LR11XX_SYSTEM_REBOOT_OC                  = 0x0118,
    LR11XX_SYSTEM_GET_VBAT_OC                = 0x0119,
    LR11XX_SYSTEM_GET_TEMP_OC                = 0x011A,
    LR11XX_SYSTEM_SET_SLEEP_OC               = 0x011B,
    LR11XX_SYSTEM_SET_STANDBY_OC             = 0x011C,
    LR11XX_SYSTEM_SET_FS_OC                  = 0x011D,
    LR11XX_SYSTEM_GET_RANDOM_OC              = 0x0120,
    LR11XX_SYSTEM_ERASE_INFOPAGE_OC          = 0x0121,
    LR11XX_SYSTEM_WRITE_INFOPAGE_OC          = 0x0122,
    LR11XX_SYSTEM_READ_INFOPAGE_OC           = 0x0123,
    LR11XX_SYSTEM_READ_UID_OC                = 0x0125,
    LR11XX_SYSTEM_READ_JOIN_EUI_OC           = 0x0126,
    LR11XX_SYSTEM_READ_PIN_OC                = 0x0127,
    LR11XX_SYSTEM_ENABLE_SPI_CRC_OC          = 0x0128,
    LR11XX_SYSTEM_DRIVE_DIO_IN_SLEEP_MODE_OC = 0x012A,
};

enum
{
    LR11XX_REGMEM_WRITE_REGMEM32_OC      = 0x0105,
    LR11XX_REGMEM_READ_REGMEM32_OC       = 0x0106,
    LR11XX_REGMEM_WRITE_MEM8_OC          = 0x0107,
    LR11XX_REGMEM_READ_MEM8_OC           = 0x0108,
    LR11XX_REGMEM_WRITE_BUFFER8_OC       = 0x0109,
    LR11XX_REGMEM_READ_BUFFER8_OC        = 0x010A,
    LR11XX_REGMEM_CLEAR_RXBUFFER_OC      = 0x010B,
    LR11XX_REGMEM_WRITE_REGMEM32_MASK_OC = 0x010C,
};

typedef enum
{
    LR11XX_RADIO_PA_SEL_LP = 0x00,  //!< Low-power Power Amplifier
    LR11XX_RADIO_PA_SEL_HP = 0x01,  //!< High-power Power Amplifier
    LR11XX_RADIO_PA_SEL_HF = 0x02,  //!< High-frequency Power Amplifier
} lr11xx_radio_pa_selection_t;

typedef enum
{
    LR11XX_RADIO_FALLBACK_STDBY_RC   = 0x01,  //!< Standby RC (Default)
    LR11XX_RADIO_FALLBACK_STDBY_XOSC = 0x02,  //!< Standby XOSC
    LR11XX_RADIO_FALLBACK_FS         = 0x03   //!< FS
} lr11xx_radio_fallback_modes_t;

typedef enum
{
    LR11XX_RADIO_RAMP_16_US  = 0x00,  //!< 16 us Ramp Time
    LR11XX_RADIO_RAMP_32_US  = 0x01,  //!< 32 us Ramp Time
    LR11XX_RADIO_RAMP_48_US  = 0x02,  //!< 48 us Ramp Time (Default)
    LR11XX_RADIO_RAMP_64_US  = 0x03,  //!< 64 us Ramp Time
    LR11XX_RADIO_RAMP_80_US  = 0x04,  //!< 80 us Ramp Time
    LR11XX_RADIO_RAMP_96_US  = 0x05,  //!< 96 us Ramp Time
    LR11XX_RADIO_RAMP_112_US = 0x06,  //!< 112 us Ramp Time
    LR11XX_RADIO_RAMP_128_US = 0x07,  //!< 128 us Ramp Time
    LR11XX_RADIO_RAMP_144_US = 0x08,  //!< 144 us Ramp Time
    LR11XX_RADIO_RAMP_160_US = 0x09,  //!< 160 us Ramp Time
    LR11XX_RADIO_RAMP_176_US = 0x0A,  //!< 176 us Ramp Time
    LR11XX_RADIO_RAMP_192_US = 0x0B,  //!< 192 us Ramp Time
    LR11XX_RADIO_RAMP_208_US = 0x0C,  //!< 208 us Ramp Time
    LR11XX_RADIO_RAMP_240_US = 0x0D,  //!< 240 us Ramp Time
    LR11XX_RADIO_RAMP_272_US = 0x0E,  //!< 272 us Ramp Time
    LR11XX_RADIO_RAMP_304_US = 0x0F,  //!< 304 us Ramp Time
} lr11xx_radio_ramp_time_t;

typedef enum
{
    LR11XX_RADIO_LORA_NETWORK_PRIVATE = 0x00,  //!< LoRa private network
    LR11XX_RADIO_LORA_NETWORK_PUBLIC  = 0x01,  //!< LoRa public network
} lr11xx_radio_lora_network_type_t;

typedef enum
{
    LR11XX_RADIO_LORA_SF5  = 0x05,  //!< Spreading Factor 5
    LR11XX_RADIO_LORA_SF6  = 0x06,  //!< Spreading Factor 6
    LR11XX_RADIO_LORA_SF7  = 0x07,  //!< Spreading Factor 7
    LR11XX_RADIO_LORA_SF8  = 0x08,  //!< Spreading Factor 8
    LR11XX_RADIO_LORA_SF9  = 0x09,  //!< Spreading Factor 9
    LR11XX_RADIO_LORA_SF10 = 0x0A,  //!< Spreading Factor 10
    LR11XX_RADIO_LORA_SF11 = 0x0B,  //!< Spreading Factor 11
    LR11XX_RADIO_LORA_SF12 = 0x0C,  //!< Spreading Factor 12
} lr11xx_radio_lora_sf_t;

typedef enum
{
    LR11XX_RADIO_LORA_BW_10  = 0x08,  //!< Bandwidth 10.42 kHz
    LR11XX_RADIO_LORA_BW_15  = 0x01,  //!< Bandwidth 15.63 kHz
    LR11XX_RADIO_LORA_BW_20  = 0x09,  //!< Bandwidth 20.83 kHz
    LR11XX_RADIO_LORA_BW_31  = 0x02,  //!< Bandwidth 31.25 kHz
    LR11XX_RADIO_LORA_BW_41  = 0x0A,  //!< Bandwidth 41.67 kHz
    LR11XX_RADIO_LORA_BW_62  = 0x03,  //!< Bandwidth 62.50 kHz
    LR11XX_RADIO_LORA_BW_125 = 0x04,  //!< Bandwidth 125.00 kHz
    LR11XX_RADIO_LORA_BW_250 = 0x05,  //!< Bandwidth 250.00 kHz
    LR11XX_RADIO_LORA_BW_500 = 0x06,  //!< Bandwidth 500.00 kHz
    LR11XX_RADIO_LORA_BW_200 = 0x0D,  //!< Bandwidth 203.00 kHz, 2G4 and compatible with LR112x chips only
    LR11XX_RADIO_LORA_BW_400 = 0x0E,  //!< Bandwidth 406.00 kHz, 2G4 and compatible with LR112x chips only
    LR11XX_RADIO_LORA_BW_800 = 0x0F,  //!< Bandwidth 812.00 kHz, 2G4 and compatible with LR112x chips only
} lr11xx_radio_lora_bw_t;

typedef enum
{
    LR11XX_RADIO_LORA_NO_CR     = 0x00,  //!< No Coding Rate
    LR11XX_RADIO_LORA_CR_4_5    = 0x01,  //!< Coding Rate 4/5 Short Interleaver
    LR11XX_RADIO_LORA_CR_4_6    = 0x02,  //!< Coding Rate 4/6 Short Interleaver
    LR11XX_RADIO_LORA_CR_4_7    = 0x03,  //!< Coding Rate 4/7 Short Interleaver
    LR11XX_RADIO_LORA_CR_4_8    = 0x04,  //!< Coding Rate 4/8 Short Interleaver
    LR11XX_RADIO_LORA_CR_LI_4_5 = 0x05,  //!< Coding Rate 4/5 Long Interleaver
    LR11XX_RADIO_LORA_CR_LI_4_6 = 0x06,  //!< Coding Rate 4/6 Long Interleaver
    LR11XX_RADIO_LORA_CR_LI_4_8 = 0x07,  //!< Coding Rate 4/8 Long Interleaver
} lr11xx_radio_lora_cr_t;

typedef enum
{
    LR11XX_RADIO_MODE_SLEEP = 0x00,  //!< Sleep / Not recommended with LR1110 FW from 0x0303 to 0x0307 and LR1120 FW
                                     //!< 0x0101 in case of transition from Rx to Tx in LoRa
    LR11XX_RADIO_MODE_STANDBY_RC   = 0x01,  //!< Standby RC
    LR11XX_RADIO_MODE_STANDBY_XOSC = 0x02,  //!< Standby XOSC
    LR11XX_RADIO_MODE_FS           = 0x03   //!< Frequency Synthesis
} lr11xx_radio_intermediary_mode_t;

typedef enum
{
    LR11XX_RADIO_LORA_CRC_OFF = 0x00,  //!< CRC deactivated
    LR11XX_RADIO_LORA_CRC_ON  = 0x01,  //!< CRC activated
} lr11xx_radio_lora_crc_t;

typedef enum
{
    LR11XX_RADIO_LORA_PKT_EXPLICIT = 0x00,  //!< Explicit header: transmitted over the air
    LR11XX_RADIO_LORA_PKT_IMPLICIT = 0x01,  //!< Implicit header: not transmitted over the air
} lr11xx_radio_lora_pkt_len_modes_t;

typedef enum
{
    LR11XX_RADIO_LORA_IQ_STANDARD = 0x00,  //!< IQ standard
    LR11XX_RADIO_LORA_IQ_INVERTED = 0x01,  //!< IQ inverted
} lr11xx_radio_lora_iq_t;

typedef enum
{
    LR11XX_RADIO_PKT_NONE         = 0x00,  //!< State after cold start, Wi-Fi or GNSS capture
    LR11XX_RADIO_PKT_TYPE_GFSK    = 0x01,  //!< GFSK modulation
    LR11XX_RADIO_PKT_TYPE_LORA    = 0x02,  //!< LoRa modulation
    LR11XX_RADIO_PKT_TYPE_BPSK    = 0x03,  //!< BPSK modulation
    LR11XX_RADIO_PKT_TYPE_LR_FHSS = 0x04,  //!< LR-FHSS modulation
    LR11XX_RADIO_PKT_TYPE_RANGING = 0x05,  //!< Ranging packet
} lr11xx_radio_pkt_type_t;

typedef enum
{
    LR11XX_RADIO_PA_REG_SUPPLY_VREG = 0x00,  //!< Power amplifier supplied by the main regulator
    LR11XX_RADIO_PA_REG_SUPPLY_VBAT = 0x01   //!< Power amplifier supplied by the battery
} lr11xx_radio_pa_reg_supply_t;

typedef enum
{
    LR11XX_RADIO_RX_DUTY_CYCLE_MODE_RX  = 0x00,  //!< LoRa/GFSK: Uses Rx for listening to packets
    LR11XX_RADIO_RX_DUTY_CYCLE_MODE_CAD = 0x01,  //!< Only in LoRa: Uses CAD to listen for over-the-air activity
} lr11xx_radio_rx_duty_cycle_mode_t;
