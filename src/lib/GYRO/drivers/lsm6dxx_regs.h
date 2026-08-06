/*
 * This file is part of INAV.
 *
 * INAV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * INAV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See tLSM6DXXhe
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with INAV.  If not, see <http://www.gnu.org/licenses/>.
 * moved from atbetaflight https://github.com/flightng/atbetaflight by tcdddd
 */

#pragma once

#include "targets.h"

#define GET_BIT(value, bit) ((value >> bit) & 1)

// LSM6DXX registers (not the complete list)
typedef enum {
    LSM6DXX_REG_COUNTER_BDR1 = 0x0B,// Counter batch data rate register LSM6DSL_REG_DRDY_PULSED_CFG_G
    LSM6DXX_REG_INT1_CTRL = 0x0D,  // int pin 1 control
    LSM6DXX_REG_INT2_CTRL = 0x0E,  // int pin 2 control
    LSM6DXX_REG_WHO_AM_I = 0x0F,   // chip ID
    LSM6DXX_REG_CTRL1_XL = 0x10,   // accelerometer control
    LSM6DXX_REG_CTRL2_G = 0x11,    // gyro control
    LSM6DXX_REG_CTRL3_C = 0x12,    // control register 3
    LSM6DXX_REG_CTRL4_C = 0x13,    // control register 4
    LSM6DXX_REG_CTRL5_C = 0x14,    // control register 5
    LSM6DXX_REG_CTRL6_C = 0x15,    // control register 6
    LSM6DXX_REG_CTRL7_G = 0x16,    // control register 7
    LSM6DXX_REG_CTRL8_XL = 0x17,   // control register 8
    LSM6DXX_REG_CTRL9_XL = 0x18,   // control register 9
    LSM6DXX_REG_CTRL10_C = 0x19,   // control register 10
    LSM6DXX_REG_STATUS = 0x1E,     // status register
    LSM6DXX_REG_OUT_TEMP_L = 0x20, // temperature LSB
    LSM6DXX_REG_OUT_TEMP_H = 0x21, // temperature MSB
    LSM6DXX_REG_OUTX_L_G = 0x22,   // gyro X axis LSB
    LSM6DXX_REG_OUTX_H_G = 0x23,   // gyro X axis MSB
    LSM6DXX_REG_OUTY_L_G = 0x24,   // gyro Y axis LSB
    LSM6DXX_REG_OUTY_H_G = 0x25,   // gyro Y axis MSB
    LSM6DXX_REG_OUTZ_L_G = 0x26,   // gyro Z axis LSB
    LSM6DXX_REG_OUTZ_H_G = 0x27,   // gyro Z axis MSB
    LSM6DXX_REG_OUTX_L_A = 0x28,   // acc X axis LSB
    LSM6DXX_REG_OUTX_H_A = 0x29,   // acc X axis MSB
    LSM6DXX_REG_OUTY_L_A = 0x2A,   // acc Y axis LSB
    LSM6DXX_REG_OUTY_H_A = 0x2B,   // acc Y axis MSB
    LSM6DXX_REG_OUTZ_L_A = 0x2C,   // acc Z axis LSB
    LSM6DXX_REG_OUTZ_H_A = 0x2D,   // acc Z axis MSB
} lsm6dxxRegister_e;
  
// LSM6DXX register configuration values
typedef enum {
    LSM6DXX_VAL_COUNTER_BDR1_DDRY_PM = BIT(7),// (bit 7) enable data ready pulsed mode
    LSM6DXX_VAL_INT1_CTRL_ENABLE  = 0x02,     // enable gyro data ready interrupt pin 1
    LSM6DXX_VAL_INT2_CTRL_DISABLE = 0x00,     // disable gyro data ready interrupt pin 2
    
    LSM6DXX_VAL_CTRL1_XL_ODR833 = 0x07,       // accelerometer 833hz output data rate (gyro/8)
    LSM6DXX_VAL_CTRL1_XL_ODR1667 = 0x08,      // accelerometer 1666hz output data rate (gyro/4)
    LSM6DXX_VAL_CTRL1_XL_ODR3332 = 0x09,      // accelerometer 3332hz output data rate (gyro/2)
    LSM6DXX_VAL_CTRL1_XL_ODR6664 = 0x0A,      // accelerometer 6664hz output data rate (gyro/1)

    LSM6DXX_VAL_CTRL1_XL_2G = 0x00,           // accelerometer 2G scale
    LSM6DXX_VAL_CTRL1_XL_16G = 0x01,          // accelerometer 16G scale
    LSM6DXX_VAL_CTRL1_XL_8G = 0x03,           // accelerometer 8G scale

    LSM6DXX_VAL_CTRL1_XL_LPF1 = 0x00,         // accelerometer output from LPF1
    LSM6DXX_VAL_CTRL1_XL_LPF2 = 0x01,         // accelerometer output from LPF2

    LSM6DXX_VAL_CTRL2_G_ODR833 = 0x07,        // gyro 833hz output data rate
    LSM6DXX_VAL_CTRL2_G_ODR1667 = 0x08,       // gyro 1666hz output data rate
    LSM6DXX_VAL_CTRL2_G_ODR3332 = 0x09,       // gyro 3332hz output data rate
    LSM6DXX_VAL_CTRL2_G_ODR6664 = 0x0A,       // gyro 6664hz output data rate

    LSM6DXX_VAL_CTRL2_G_2000DPS = 0x03,       // gyro 2000dps scale

    LSM6DXX_VAL_CTRL3_C_BDU = BIT(6),         // (bit 6) output registers are not updated until MSB and LSB have been read (prevents MSB from being updated while burst reading LSB/MSB)
    LSM6DXX_VAL_CTRL3_C_H_LACTIVE = 0,        // (bit 5) interrupt pins active high
    LSM6DXX_VAL_CTRL3_C_PP_OD = 0,            // (bit 4) interrupt pins push/pull
    LSM6DXX_VAL_CTRL3_C_SIM = 0,              // (bit 3) SPI 4-wire interface mode
    LSM6DXX_VAL_CTRL3_C_IF_INC = BIT(2),      // (bit 2) auto-increment address for burst reads
    LSM6DXX_VAL_CTRL4_C_DRDY_MASK = BIT(3),   // (bit 3) data ready interrupt mask
    LSM6DXX_VAL_CTRL4_C_DRDY_ENABLED = BIT(3),   // (bit 3) data ready interrupt
    LSM6DXX_VAL_CTRL4_C_I2C_DISABLE = BIT(2), // (bit 2) disable I2C interface
    LSM6DXX_VAL_CTRL4_C_SPI_DISABLE = BIT(3), // (bit 3) disable SPI interface
    LSM6DXX_VAL_CTRL4_C_LPF1_SEL_G = BIT(1),  // (bit 1) enable gyro LPF1
    LSM6DXX_VAL_CTRL6_C_XL_HM_MODE = 0,       // (bit 4) enable accelerometer high performance mode
    LSM6DXX_VAL_CTRL6_C_FTYPE_335HZ = 0x00,   // (bits 2:0) gyro LPF1 cutoff 304.2Hz on ODR=1.6kh
    LSM6DXX_VAL_CTRL6_C_FTYPE_232HZ = 0x01,   // (bits 2:0) gyro LPF1 cutoff 220.7Hz on ODR=1.6kh
    LSM6DXX_VAL_CTRL6_C_FTYPE_171HZ = 0x02,   // (bits 2:0) gyro LPF1 cutoff 166.6Hz on ODR=1.6kh
    LSM6DXX_VAL_CTRL6_C_FTYPE_609HZ = 0x03,   // (bits 2:0) gyro LPF1 cutoff 453.2Hz on ODR=1.6kh
    LSM6DXX_VAL_CTRL6_C_FTYPE_99HZ  = 0x04,   // (bits 2:0) gyro LPF1 cutoff 99.6Hz  on ODR=1.6kh
    LSM6DXX_VAL_CTRL6_C_FTYPE_49HZ  = 0x05,   // (bits 2:0) gyro LPF1 cutoff 49.8Hz  on ODR=1.6kh
    LSM6DXX_VAL_CTRL7_G_HP_EN_G = BIT(6),   // (bit 6) enable gyro high-pass filter
    LSM6DXX_VAL_CTRL7_G_HPM_G_16 = 0x00,      // (bits 5:4) gyro HPF cutoff 16mHz
    LSM6DXX_VAL_CTRL7_G_HPM_G_65 = 0x01,      // (bits 5:4) gyro HPF cutoff 65mHz
    LSM6DXX_VAL_CTRL7_G_HPM_G_260 = 0x02,     // (bits 5:4) gyro HPF cutoff 260mHz
    LSM6DXX_VAL_CTRL7_G_HPM_G_1040 = 0x03,    // (bits 5:4) gyro HPF cutoff 1.04Hz
    LSM6DXX_VAL_CTRL9_XL_I3C_DISABLE = BIT(1),// (bit 1) disable I3C interface
    LSM6DXX_VAL_STATUS_GDA  = BIT(0),        // (bit 0) gyro data Available
    LSM6DXX_VAL_STATUS_XLDA = BIT(2),        // (bit 2) acceleromenter Data Available
} lsm6dxxConfigValues_e;

// LSM6DXX register configuration bit masks
typedef enum {
    LSM6DXX_MASK_COUNTER_BDR1 = 0x80,    // 0b10000000
    LSM6DXX_MASK_CTRL3_C = 0x3C,         // 0b00111100
    LSM6DXX_MASK_CTRL3_C_RESET = BIT(0), // 0b00000001
    LSM6DXX_MASK_CTRL4_C = 0x0E,         // 0b00001110
    LSM6DXX_MASK_CTRL6_C = 0x17,         // 0b00010111
    LSM6DXX_MASK_CTRL7_G = 0x70,         // 0b01110000
    LSM6DXX_MASK_CTRL9_XL = 0x02,        // 0b00000010
    LSM6DSL_MASK_CTRL6_C = 0x13,         // 0b00010011

} lsm6dxxConfigMasks_e;


#define LSM6DSO_CHIP_ID 0x6C
#define LSM6DSL_CHIP_ID 0x6A
#define LSM6DS3_CHIP_ID 0x69