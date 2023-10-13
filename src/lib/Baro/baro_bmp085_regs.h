#pragma once

#define BMP085_I2C_ADDR                     0x77
#define BMP085_CHIPID                       0x55

// Registers
#define BMP085_CALIB_COEFFS_START           0xAA
#define BMP085_CALIB_COEFFS_END             0xBE
#define BMP085_REG_CHIPID                   0xD0
#define BMP085_REG_CONTROL                  0xF4
#define BMP085_REG_TEMPERATURE_DATA         0xF6 // uses same scratchpad as press
#define BMP085_REG_PRESSURE_DATA            0xF6 // uses same scratchpad as temp
#define BMP085_CMD_MEAS_TEMPERATURE         0x2E
#define BMP085_CMD_MEAS_PRESSURE            0x34

#define BMP085_CALIB_COEFFS_LEN                 (BMP085_CALIB_COEFFS_END - BMP085_CALIB_COEFFS_START + 1)
