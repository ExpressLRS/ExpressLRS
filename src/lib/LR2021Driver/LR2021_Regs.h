/*!
 * @file      lr20xx_radio_types.h
 *
 * @brief     Radio driver types for LR20XX
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

#define LR2021_XTAL_FREQ 32000000
#define FREQ_STEP 61.03515625 // TODO check and fix... this is a cut and paste from sx1276

#define LR2021_POWER_MIN_LF_PA (-19) // Low Frequency PA 0.5dBm steps
#define LR2021_POWER_MAX_LF_PA (44)
#define LR2021_POWER_MIN_HF_PA (-39) // High Frequency PA 0.5dBm steps
#define LR2021_POWER_MAX_HF_PA (24)

#define LR2021_IRQ_TX_DONE bit(19)
#define LR2021_IRQ_RX_DONE bit(18)

typedef enum
{
    LR2021_LORA_PACKET_VARIABLE_LENGTH = 0x00, //!< The packet length is variable size, header included in the packet
    LR2021_LORA_PACKET_FIXED_LENGTH = 0x01,    //!< The packet length is known on both sides, no header included in the packet
    LR2021_LORA_PACKET_EXPLICIT = LR2021_LORA_PACKET_VARIABLE_LENGTH,
    LR2021_LORA_PACKET_IMPLICIT = LR2021_LORA_PACKET_FIXED_LENGTH,
} lr20xx_RadioLoRaPacketLengthsModes_t;

typedef enum
{
    LR2021_MODE_SLEEP = 0x00, //! The radio is in sleep mode
    LR2021_MODE_STDBY_RC,     //! The radio is in standby mode with RC oscillator
    LR2021_MODE_STDBY_XOSC,   //! The radio is in standby mode with XOSC oscillator
    LR2021_MODE_FS,           //! The radio is in frequency synthesis mode
    LR2021_MODE_RX_CONT,      //! The radio is in continuous receive mode
    LR2021_MODE_TX,           //! The radio is in transmit mode
    LR2021_MODE_CAD           //! The radio is in channel activity detection mode
} lr20xx_RadioOperatingModes_t;

enum
{
    LR2021_RADIO_READ_RX_FIFO = 0x0001,
    LR2021_RADIO_WRITE_TX_FIFO = 0x0002,
    LR2021_SYSTEM_GET_STATUS_OC = 0x0100,
    LR2021_SYSTEM_GET_VERSION_OC = 0x0101,
    LR2021_REGMEM_WRITE_REGMEM32_MASK_OC = 0x0105,
    LR2021_SYSTEM_GET_ERRORS_OC = 0x0110,
    LR2021_SYSTEM_CLEAR_ERRORS_OC = 0x0111,
    LR2021_SYSTEM_SET_DIO_FUNCTION_OC = 0x0112,
    LR2021_SYSTEM_SET_DIO_RF_SWITCH_CFG_OC = 0x0113,
    LR2021_SYSTEM_SET_DIOIRQPARAMS_OC = 0x0115,
    LR2021_SYSTEM_CLEAR_IRQ_OC = 0x0116,
    LR2021_SYSTEM_CLEAR_RX_FIFO_OC = 0x011E,
    LR2021_SYSTEM_SET_REGMODE_OC = 0x0121,
    LR2021_SYSTEM_CALIBRATE_OC = 0x0122,
    LR2021_SYSTEM_CALIBRATE_FRONTEND_OC = 0x0123,
    LR2021_SYSTEM_SET_SLEEP_OC = 0x0127,
    LR2021_SYSTEM_SET_STANDBY_OC = 0x0128,
    LR2021_SYSTEM_SET_FS_OC = 0x0129,
    LR2021_RADIO_SET_RF_FREQUENCY_OC = 0x0200,
    LR2021_RADIO_SET_RX_PATH_OC = 0x0201,
    LR2021_RADIO_SET_PA_CFG_OC = 0x0202,
    LR2021_RADIO_SET_TX_PARAMS_OC = 0x0203,
    LR2021_RADIO_SET_RX_TX_FALLBACK_MODE_OC = 0x0206,
    LR2021_RADIO_SET_PKT_TYPE_OC = 0x0207,
    LR2021_RADIO_GET_RSSI_INST_OC = 0x020B,
    LR2021_RADIO_SET_RX_OC = 0x020C,
    LR2021_RADIO_SET_TX_OC = 0x020D,
    LR2021_RADIO_SET_TX_TEST_MODE_OC = 0x020E,
    LR2021_RADIO_SET_DEFAULT_RX_TX_TIMEOUT_OC = 0x0215,
    LR2021_RADIO_SET_LORA_MODULATION_PARAM_OC = 0x0220,
    LR2021_RADIO_SET_LORA_PACKET_PARAMS_OC = 0x0221,
    LR2021_RADIO_GET_LORA_PACKET_STATUS_OC = 0x022A,
    LR2021_RADIO_SET_FSK_MODULATION_PARAMS_OC = 0x0240,
    LR2021_RADIO_SET_FSK_PACKET_PARAMS_OC = 0x0241,
    LR2021_RADIO_SET_FSK_WHITENING_PARAMS_OC = 0x0242,
    LR2021_RADIO_SET_FSK_SYNC_WORD_OC = 0x0244,
    LR2021_RADIO_GET_FSK_PACKET_STATUS_OC = 0x0247
};

typedef enum
{
    LR2021_RADIO_PA_SEL_LF = 0x00, //!< Low-power Power Amplifier
    LR2021_RADIO_PA_SEL_HF = 0x80, //!< High-frequency Power Amplifier
} lr20xx_radio_pa_selection_t;

typedef enum // USED
{
    LR2021_RADIO_FALLBACK_FS = 0x03 //!< FS
} lr2021_radio_fallback_modes_t;

typedef enum
{
    LR2021_RADIO_RAMP_48_US = 0x05, //!< 48 us Ramp Time (Default)
} lr20xx_radio_ramp_time_t;

typedef enum
{
    LR2021_RADIO_LORA_SF5 = 0x05,  //!< Spreading Factor 5
    LR2021_RADIO_LORA_SF6 = 0x06,  //!< Spreading Factor 6
    LR2021_RADIO_LORA_SF7 = 0x07,  //!< Spreading Factor 7
    LR2021_RADIO_LORA_SF8 = 0x08,  //!< Spreading Factor 8
    LR2021_RADIO_LORA_SF9 = 0x09,  //!< Spreading Factor 9
    LR2021_RADIO_LORA_SF10 = 0x0A, //!< Spreading Factor 10
    LR2021_RADIO_LORA_SF11 = 0x0B, //!< Spreading Factor 11
    LR2021_RADIO_LORA_SF12 = 0x0C, //!< Spreading Factor 12
} lr20xx_radio_lora_sf_t;

typedef enum
{
    LR2021_RADIO_LORA_BW_62 = 0x03,  //!< Bandwidth 62.50 kHz
    LR2021_RADIO_LORA_BW_500 = 0x06, //!< Bandwidth 500.00 kHz
    LR2021_RADIO_LORA_BW_800 = 0x0F, //!< Bandwidth 812.00 kHz, 2G4 and compatible with LR112x chips only
} lr20xx_radio_lora_bw_t;

typedef enum
{
    LR2021_RADIO_LORA_CR_4_5 = 0x01,    //!< Coding Rate 4/5 Short Interleaver
    LR2021_RADIO_LORA_CR_4_6 = 0x02,    //!< Coding Rate 4/6 Short Interleaver
    LR2021_RADIO_LORA_CR_4_7 = 0x03,    //!< Coding Rate 4/7 Short Interleaver
    LR2021_RADIO_LORA_CR_4_8 = 0x04,    //!< Coding Rate 4/8 Short Interleaver
    LR2021_RADIO_LORA_CR_LI_4_5 = 0x05, //!< Coding Rate 4/5 Long Interleaver
    LR2021_RADIO_LORA_CR_LI_4_6 = 0x06, //!< Coding Rate 4/6 Long Interleaver
    LR2021_RADIO_LORA_CR_LI_4_8 = 0x07, //!< Coding Rate 4/8 Long Interleaver
    LR2021_RADIO_LORA_CC_LI_4_6 = 0x08, //!< Convolutional Code 4/6 Long Interleaver
    LR2021_RADIO_LORA_CC_LI_4_8 = 0x09, //!< Convolutional Code 4/8 Long Interleaver
} lr20xx_radio_lora_cr_t;

typedef enum
{
    LR2021_RADIO_LORA_IQ_STANDARD = 0x00, //!< IQ standard
    LR2021_RADIO_LORA_IQ_INVERTED = 0x01, //!< IQ inverted
} lr20xx_radio_lora_iq_t;

typedef enum
{
    LR2021_RADIO_PKT_TYPE_LORA = 0x00, //!< LoRa modulation
    LR2021_RADIO_PKT_TYPE_FSK = 0x02,  //!< FSK modulation
    LR2021_RADIO_PKT_TYPE_FLRC = 0x05, //!< FLRC modulation
} lr20xx_radio_pkt_type_t;

typedef enum
{
    LR2021_RADIO_GFSK_CRC_OFF = 0x00, //!< CRC check deactivated
} lr20xx_radio_gfsk_crc_type_t;

typedef enum
{
    LR2021_RADIO_GFSK_DC_FREE_WHITENING = 0x01, //!< Whitening enabled
} lr20xx_radio_gfsk_dc_free_t;

typedef enum
{
    LR2021_RADIO_GFSK_PREAMBLE_DETECTOR_OFF = 0x00,
    LR2021_RADIO_GFSK_PREAMBLE_DETECTOR_MIN_8BITS = 0x08,
    LR2021_RADIO_GFSK_PREAMBLE_DETECTOR_MIN_16BITS = 0x10,
    LR2021_RADIO_GFSK_PREAMBLE_DETECTOR_MIN_24BITS = 0x18,
    LR2021_RADIO_GFSK_PREAMBLE_DETECTOR_MIN_32BITS = 0x20
} lr20xx_radio_gfsk_preamble_detector_t;

typedef enum
{
    LR2021_RADIO_GFSK_BW_476000 = 81    //!< Bandwidth 476.2 kHz
} lr20xx_radio_gfsk_bw_t;

typedef enum
{
    LR2021_RADIO_GFSK_PULSE_SHAPE_OFF = 0x00,   //!< No filter applied
    LR2021_RADIO_GFSK_PULSE_SHAPE_BT_03 = 0x08, //!< Gaussian BT 0.3
    LR2021_RADIO_GFSK_PULSE_SHAPE_BT_05 = 0x09, //!< Gaussian BT 0.5
    LR2021_RADIO_GFSK_PULSE_SHAPE_BT_07 = 0x0A, //!< Gaussian BT 0.7
    LR2021_RADIO_GFSK_PULSE_SHAPE_BT_1 = 0x0B   //!< Gaussian BT 1.0
} lr20xx_radio_gfsk_pulse_shape_t;

typedef enum
{
    LR2021_RADIO_GFSK_BITRATE_300k = 30
} lr20xx_radio_gfsk_bitrate_t;

typedef enum
{
    LR2021_RADIO_GFSK_FDEV_100k = 100
} lr20xx_radio_gfsk_fdev_t;

#define LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_ADDRESS (0x00F30A14)
#define LR20XX_WORKAROUND_LORA_SX1276_COMPATIBILITY_REGISTER_MASK (3 << 18)
