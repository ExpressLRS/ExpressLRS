#pragma once

#define SPA06_I2C_ADDR                          0x76
#define SPA06_I2C_ADDR_ALT                      0x77
#define SPA06_DEFAULT_CHIP_ID                   0x11

#define SPA06_PRESSURE_START_REG                0x00
#define SPA06_TEMPERATURE_START_REG             0x03
#define SPA06_PRESSURE_CFG_REG                  0x06
#define SPA06_TEMPERATURE_CFG_REG               0x07
#define SPA06_MODE_AND_STATUS_REG               0x08
#define SPA06_INT_AND_FIFO_CFG_REG              0x09
#define SPA06_RESET_REG                         0x0C
#define SPA06_CHIP_ID_REG                       0x0D
#define SPA06_CALIB_COEFFS_START                0x10
#define SPA06_CALIB_COEFFS_LEN                  21

// MODE_AND_STATUS_REG
#define SPA06_MEAS_PRESSURE                     (1 << 0)
#define SPA06_MEAS_TEMPERATURE                  (1 << 1)
#define SPA06_MEAS_MODE_MASK                    0x07
#define SPA06_MEAS_CFG_PRESSURE_RDY             (1 << 4)
#define SPA06_MEAS_CFG_TEMPERATURE_RDY          (1 << 5)
#define SPA06_MEAS_CFG_SENSOR_RDY               (1 << 6)
#define SPA06_MEAS_CFG_COEFFS_RDY               (1 << 7)

// INT_AND_FIFO_CFG_REG
#define SPA06_PRESSURE_RESULT_BIT_SHIFT         (1 << 2)  // Required for pressure oversampling > 8

// RESET_REG
#define SPA06_SOFT_RESET                        0x09
