#pragma once

#define BMP280_I2C_ADDR                     0x76
#define BMP280_I2C_ADDR_ALT                 0x77
#define BMP280_CHIPID                       0x58 // Pressure,Temp
#define BME280_CHIPID                       0x60 // Pressure,Temp,Humid

// Registers
#define BMP280_REG_CALIB_COEFFS_START       0x88
#define BMP280_REG_CHIPID                   0xD0
#define BMP280_REG_STATUS                   0xF3
#define BMP280_REG_CTRL_MEAS                0xF4
#define BMP280_REG_CONFIG                   0xF5
#define BMP280_REG_PRESSURE_MSB             0xF7
#define BMP280_REG_PRESSURE_LSB             0xF8
#define BMP280_REG_PRESSURE_XLSB            0xF9
#define BMP280_REG_TEMPERATURE_MSB          0xFA
#define BMP280_REG_TEMPERATURE_LSB          0xFB
#define BMP280_REG_TEMPERATURE_XLSB         0xFC


#define BMP280_LEN_TEMP_PRESS_DATA          6
#define BMP280_MODE_SLEEP                   0x00
#define BMP280_MODE_FORCED                  0x01
#define BMP280_MODE_NORMAL                  0x03 // Freerunning mode

#define BMP280_OVERSAMP_SKIPPED             0x00
#define BMP280_OVERSAMP_1X                  0x01
#define BMP280_OVERSAMP_2X                  0x02
#define BMP280_OVERSAMP_4X                  0x03
#define BMP280_OVERSAMP_8X                  0x04
#define BMP280_OVERSAMP_16X                 0x05

#define BMP280_FILTER_COEFF_OFF             0x00
#define BMP280_FILTER_COEFF_2               0x01
#define BMP280_FILTER_COEFF_4               0x02
#define BMP280_FILTER_COEFF_8               0x03
#define BMP280_FILTER_COEFF_16              0x04